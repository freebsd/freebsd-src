/*
 * Implementation of the Target Mode 'Black Hole device' for CAM.
 *
 * Copyright (c) 1999 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/cam/scsi/scsi_targ_bh.c,v 1.4.2.3 2000/07/14 21:10:26 mjacob Exp $
 */
#include <stddef.h>	/* For offsetof */
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/malloc.h>
#include <sys/uio.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_extend.h>
#include <cam/cam_periph.h>
#include <cam/cam_queue.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

typedef enum {
	TARGBH_STATE_NORMAL,
	TARGBH_STATE_EXCEPTION,
	TARGBH_STATE_TEARDOWN
} targbh_state;

typedef enum {
	TARGBH_FLAG_NONE	 = 0x00,
	TARGBH_FLAG_LUN_ENABLED	 = 0x01
} targbh_flags;

typedef enum {
	TARGBH_CCB_WORKQ,
	TARGBH_CCB_WAITING
} targbh_ccb_types;

#define MAX_ACCEPT	8
#define MAX_IMMEDIATE	16
#define MAX_BUF_SIZE	256	/* Max inquiry/sense/mode page transfer */

#define MIN(a, b) ((a > b) ? b : a)

/* Offsets into our private CCB area for storing accept information */
#define ccb_type	ppriv_field0
#define ccb_descr	ppriv_ptr1

/* We stick a pointer to the originating accept TIO in each continue I/O CCB */
#define ccb_atio	ppriv_ptr1

TAILQ_HEAD(ccb_queue, ccb_hdr);

struct targbh_softc {
	struct		ccb_queue pending_queue;
	struct		ccb_queue work_queue;
	struct		ccb_queue unknown_atio_queue;
	struct		devstat device_stats;
	targbh_state	state;
	targbh_flags	flags;	
	u_int		init_level;
	u_int		inq_data_len;
	struct		ccb_accept_tio *accept_tio_list;
	struct		ccb_hdr_slist immed_notify_slist;
};

struct targbh_cmd_desc {
	struct	  ccb_accept_tio* atio_link;
	u_int	  data_resid;	/* How much left to transfer */
	u_int	  data_increment;/* Amount to send before next disconnect */
	void*	  data;		/* The data. Can be from backing_store or not */
	void*	  backing_store;/* Backing store allocated for this descriptor*/
	u_int	  max_size;	/* Size of backing_store */
	u_int32_t timeout;	
	u_int8_t  status;	/* Status to return to initiator */
};

static struct scsi_inquiry_data no_lun_inq_data =
{
	T_NODEVICE | (SID_QUAL_BAD_LU << 5), 0,
	/* version */2, /* format version */2
};

static struct scsi_sense_data no_lun_sense_data =
{
	SSD_CURRENT_ERROR|SSD_ERRCODE_VALID,
	0,
	SSD_KEY_NOT_READY, 
	{ 0, 0, 0, 0 },
	/*extra_len*/offsetof(struct scsi_sense_data, fru)
                   - offsetof(struct scsi_sense_data, extra_len),
	{ 0, 0, 0, 0 },
	/* Logical Unit Not Supported */
	/*ASC*/0x25, /*ASCQ*/0
};

static const int request_sense_size = offsetof(struct scsi_sense_data, fru);

static periph_init_t	targbhinit;
static void		targbhasync(void *callback_arg, u_int32_t code,
				    struct cam_path *path, void *arg);
static cam_status	targbhenlun(struct cam_periph *periph);
static cam_status	targbhdislun(struct cam_periph *periph);
static periph_ctor_t	targbhctor;
static periph_dtor_t	targbhdtor;
static periph_start_t	targbhstart;
static void		targbhdone(struct cam_periph *periph,
				   union ccb *done_ccb);
#ifdef NOTYET
static  int		targbherror(union ccb *ccb, u_int32_t cam_flags,
				    u_int32_t sense_flags);
#endif
static struct targbh_cmd_desc*	targbhallocdescr(void);
static void		targbhfreedescr(struct targbh_cmd_desc *buf);
					
static struct periph_driver targbhdriver =
{
	targbhinit, "targbh",
	TAILQ_HEAD_INITIALIZER(targbhdriver.units), /* generation */ 0
};

DATA_SET(periphdriver_set, targbhdriver);

static void
targbhinit(void)
{
	cam_status status;
	struct cam_path *path;

	/*
	 * Install a global async callback.  This callback will
	 * receive async callbacks like "new path registered".
	 */
	status = xpt_create_path(&path, /*periph*/NULL, CAM_XPT_PATH_ID,
				 CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD);

	if (status == CAM_REQ_CMP) {
		struct ccb_setasync csa;

		xpt_setup_ccb(&csa.ccb_h, path, /*priority*/5);
		csa.ccb_h.func_code = XPT_SASYNC_CB;
		csa.event_enable = AC_PATH_REGISTERED;
		csa.callback = targbhasync;
		csa.callback_arg = NULL;
		xpt_action((union ccb *)&csa);
		status = csa.ccb_h.status;
		xpt_free_path(path);
        }

	if (status != CAM_REQ_CMP) {
		printf("targbh: Failed to attach master async callback "
		       "due to status 0x%x!\n", status);
	}
}

static void
targbhasync(void *callback_arg, u_int32_t code,
	    struct cam_path *path, void *arg)
{
	struct cam_periph *periph;

	periph = (struct cam_periph *)callback_arg;
	switch (code) {
	case AC_PATH_REGISTERED:
	{
		struct ccb_pathinq *cpi;
		struct cam_path *new_path;
		cam_status status;
 
		cpi = (struct ccb_pathinq *)arg;

		/* Only attach to controllers that support target mode */
		if ((cpi->target_sprt & PIT_PROCESSOR) == 0)
			break;

		/*
		 * Allocate a peripheral instance for
		 * this target instance.
		 */
		status = xpt_create_path(&new_path, NULL,
					 xpt_path_path_id(path),
					 CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD);
		if (status != CAM_REQ_CMP) {
			printf("targbhasync: Unable to create path "
				"due to status 0x%x\n", status);
			break;
		}
		status = cam_periph_alloc(targbhctor, NULL, targbhdtor,
					  targbhstart,
					  "targbh", CAM_PERIPH_BIO,
					  new_path, targbhasync,
					  AC_PATH_REGISTERED,
					  cpi);
		xpt_free_path(new_path);
		if (status != CAM_REQ_CMP
		 && status != CAM_REQ_INPROG)
			printf("targbhasync: Unable to allocate new periph "
			       "due to status 0x%x\n", status);
		break;
	}
	case AC_PATH_DEREGISTERED:
	{
		targbhdislun(periph);
		break;
	}
	default:
		break;
	}
}

/* Attempt to enable our lun */
static cam_status
targbhenlun(struct cam_periph *periph)
{
	union ccb immed_ccb;
	struct targbh_softc *softc;
	cam_status status;
	int i;

	softc = (struct targbh_softc *)periph->softc;

	if ((softc->flags & TARGBH_FLAG_LUN_ENABLED) != 0)
		return (CAM_REQ_CMP);

	xpt_setup_ccb(&immed_ccb.ccb_h, periph->path, /*priority*/1);
	immed_ccb.ccb_h.func_code = XPT_EN_LUN;

	/* Don't need support for any vendor specific commands */
	immed_ccb.cel.grp6_len = 0;
	immed_ccb.cel.grp7_len = 0;
	immed_ccb.cel.enable = 1;
	xpt_action(&immed_ccb);
	status = immed_ccb.ccb_h.status;
	if (status != CAM_REQ_CMP) {
		xpt_print_path(periph->path);
		printf("targbhenlun - Enable Lun Rejected for status 0x%x\n",
		       status);
		return (status);
	}
	
	softc->flags |= TARGBH_FLAG_LUN_ENABLED;

	/*
	 * Build up a buffer of accept target I/O
	 * operations for incoming selections.
	 */
	for (i = 0; i < MAX_ACCEPT; i++) {
		struct ccb_accept_tio *atio;

		atio = (struct ccb_accept_tio*)malloc(sizeof(*atio), M_DEVBUF,
						      M_NOWAIT);
		if (atio == NULL) {
			status = CAM_RESRC_UNAVAIL;
			break;
		}

		atio->ccb_h.ccb_descr = targbhallocdescr();

		if (atio->ccb_h.ccb_descr == NULL) {
			free(atio, M_DEVBUF);
			status = CAM_RESRC_UNAVAIL;
			break;
		}

		xpt_setup_ccb(&atio->ccb_h, periph->path, /*priority*/1);
		atio->ccb_h.func_code = XPT_ACCEPT_TARGET_IO;
		atio->ccb_h.cbfcnp = targbhdone;
		xpt_action((union ccb *)atio);
		status = atio->ccb_h.status;
		if (status != CAM_REQ_INPROG) {
			targbhfreedescr(atio->ccb_h.ccb_descr);
			free(atio, M_DEVBUF);
			break;
		}
		((struct targbh_cmd_desc*)atio->ccb_h.ccb_descr)->atio_link =
		    softc->accept_tio_list;
		softc->accept_tio_list = atio;
	}

	if (i == 0) {
		xpt_print_path(periph->path);
		printf("targbhenlun - Could not allocate accept tio CCBs: "
		       "status = 0x%x\n", status);
		targbhdislun(periph);
		return (CAM_REQ_CMP_ERR);
	}

	/*
	 * Build up a buffer of immediate notify CCBs
	 * so the SIM can tell us of asynchronous target mode events.
	 */
	for (i = 0; i < MAX_ACCEPT; i++) {
		struct ccb_immed_notify *inot;

		inot = (struct ccb_immed_notify*)malloc(sizeof(*inot), M_DEVBUF,
						        M_NOWAIT);

		if (inot == NULL) {
			status = CAM_RESRC_UNAVAIL;
			break;
		}

		xpt_setup_ccb(&inot->ccb_h, periph->path, /*priority*/1);
		inot->ccb_h.func_code = XPT_IMMED_NOTIFY;
		inot->ccb_h.cbfcnp = targbhdone;
		xpt_action((union ccb *)inot);
		status = inot->ccb_h.status;
		if (status != CAM_REQ_INPROG) {
			free(inot, M_DEVBUF);
			break;
		}
		SLIST_INSERT_HEAD(&softc->immed_notify_slist, &inot->ccb_h,
				  periph_links.sle);
	}

	if (i == 0) {
		xpt_print_path(periph->path);
		printf("targbhenlun - Could not allocate immediate notify "
		       "CCBs: status = 0x%x\n", status);
		targbhdislun(periph);
		return (CAM_REQ_CMP_ERR);
	}

	return (CAM_REQ_CMP);
}

static cam_status
targbhdislun(struct cam_periph *periph)
{
	union ccb ccb;
	struct targbh_softc *softc;
	struct ccb_accept_tio* atio;
	struct ccb_hdr *ccb_h;

	softc = (struct targbh_softc *)periph->softc;
	if ((softc->flags & TARGBH_FLAG_LUN_ENABLED) == 0)
		return CAM_REQ_CMP;

	/* XXX Block for Continue I/O completion */

	/* Kill off all ACCECPT and IMMEDIATE CCBs */
	while ((atio = softc->accept_tio_list) != NULL) {
		
		softc->accept_tio_list =
		    ((struct targbh_cmd_desc*)atio->ccb_h.ccb_descr)->atio_link;
		xpt_setup_ccb(&ccb.cab.ccb_h, periph->path, /*priority*/1);
		ccb.cab.ccb_h.func_code = XPT_ABORT;
		ccb.cab.abort_ccb = (union ccb *)atio;
		xpt_action(&ccb);
	}

	while ((ccb_h = SLIST_FIRST(&softc->immed_notify_slist)) != NULL) {
		SLIST_REMOVE_HEAD(&softc->immed_notify_slist, periph_links.sle);
		xpt_setup_ccb(&ccb.cab.ccb_h, periph->path, /*priority*/1);
		ccb.cab.ccb_h.func_code = XPT_ABORT;
		ccb.cab.abort_ccb = (union ccb *)ccb_h;
		xpt_action(&ccb);
	}

	/*
	 * Dissable this lun.
	 */
	xpt_setup_ccb(&ccb.cel.ccb_h, periph->path, /*priority*/1);
	ccb.cel.ccb_h.func_code = XPT_EN_LUN;
	ccb.cel.enable = 0;
	xpt_action(&ccb);

	if (ccb.cel.ccb_h.status != CAM_REQ_CMP)
		printf("targbhdislun - Disabling lun on controller failed "
		       "with status 0x%x\n", ccb.cel.ccb_h.status);
	else 
		softc->flags &= ~TARGBH_FLAG_LUN_ENABLED;
	return (ccb.cel.ccb_h.status);
}

static cam_status
targbhctor(struct cam_periph *periph, void *arg)
{
	struct ccb_pathinq *cpi;
	struct targbh_softc *softc;

	cpi = (struct ccb_pathinq *)arg;

	/* Allocate our per-instance private storage */
	softc = (struct targbh_softc *)malloc(sizeof(*softc),
					      M_DEVBUF, M_NOWAIT);
	if (softc == NULL) {
		printf("targctor: unable to malloc softc\n");
		return (CAM_REQ_CMP_ERR);
	}

	bzero(softc, sizeof(*softc));
	TAILQ_INIT(&softc->pending_queue);
	TAILQ_INIT(&softc->work_queue);
	softc->accept_tio_list = NULL;
	SLIST_INIT(&softc->immed_notify_slist);
	softc->state = TARGBH_STATE_NORMAL;
	periph->softc = softc;
	softc->init_level++;

	return (targbhenlun(periph));
}

static void
targbhdtor(struct cam_periph *periph)
{
	struct targbh_softc *softc;

	softc = (struct targbh_softc *)periph->softc;

	softc->state = TARGBH_STATE_TEARDOWN;

	targbhdislun(periph);

	switch (softc->init_level) {
	default:
		/* FALLTHROUGH */
	case 1:
		free(softc, M_DEVBUF);
		break;
	case 0:
		panic("targdtor - impossible init level");;
	}
}

static void
targbhstart(struct cam_periph *periph, union ccb *start_ccb)
{
	struct targbh_softc *softc;
	struct ccb_hdr *ccbh;
	struct ccb_accept_tio *atio;
	struct targbh_cmd_desc *desc;
	struct ccb_scsiio *csio;
	ccb_flags flags;
	int    s;

	softc = (struct targbh_softc *)periph->softc;
	
	s = splbio();
	ccbh = TAILQ_FIRST(&softc->work_queue);
	if (periph->immediate_priority <= periph->pinfo.priority) {
		start_ccb->ccb_h.ccb_type = TARGBH_CCB_WAITING;			
		SLIST_INSERT_HEAD(&periph->ccb_list, &start_ccb->ccb_h,
				  periph_links.sle);
		periph->immediate_priority = CAM_PRIORITY_NONE;
		splx(s);
		wakeup(&periph->ccb_list);
	} else if (ccbh == NULL) {
		splx(s);
		xpt_release_ccb(start_ccb);	
	} else {
		TAILQ_REMOVE(&softc->work_queue, ccbh, periph_links.tqe);
		TAILQ_INSERT_HEAD(&softc->pending_queue, ccbh,
				  periph_links.tqe);
		splx(s);	
		atio = (struct ccb_accept_tio*)ccbh;
		desc = (struct targbh_cmd_desc *)atio->ccb_h.ccb_descr;

		/* Is this a tagged request? */
		flags = atio->ccb_h.flags & (CAM_TAG_ACTION_VALID|CAM_DIR_MASK);

		csio = &start_ccb->csio;
		/*
		 * If we are done with the transaction, tell the
		 * controller to send status and perform a CMD_CMPLT.
		 * If we have associated sense data, see if we can
		 * send that too.
		 */
		if (desc->data_resid == desc->data_increment) {
			flags |= CAM_SEND_STATUS;
			if (atio->sense_len) {
				csio->sense_len = atio->sense_len;
				csio->sense_data = atio->sense_data;
				flags |= CAM_SEND_SENSE;
			}

		}

		cam_fill_ctio(csio,
			      /*retries*/2,
			      targbhdone,
			      flags,
			      (flags & CAM_TAG_ACTION_VALID)?
				MSG_SIMPLE_Q_TAG : 0,
			      atio->tag_id,
			      atio->init_id,
			      desc->status,
			      /*data_ptr*/desc->data_increment == 0
					  ? NULL : desc->data,
			      /*dxfer_len*/desc->data_increment,
			      /*timeout*/desc->timeout);

		/* Override our wildcard attachment */
		start_ccb->ccb_h.target_id = atio->ccb_h.target_id;
		start_ccb->ccb_h.target_lun = atio->ccb_h.target_lun;

		start_ccb->ccb_h.ccb_type = TARGBH_CCB_WORKQ;
		start_ccb->ccb_h.ccb_atio = atio;
		CAM_DEBUG(periph->path, CAM_DEBUG_SUBTRACE,
			  ("Sending a CTIO\n"));
		xpt_action(start_ccb);
		s = splbio();
		ccbh = TAILQ_FIRST(&softc->work_queue);
		splx(s);
	}
	if (ccbh != NULL)
		xpt_schedule(periph, /*priority*/1);
}

static void
targbhdone(struct cam_periph *periph, union ccb *done_ccb)
{
	struct targbh_softc *softc;

	softc = (struct targbh_softc *)periph->softc;

	if (done_ccb->ccb_h.ccb_type == TARGBH_CCB_WAITING) {
		/* Caller will release the CCB */
		wakeup(&done_ccb->ccb_h.cbfcnp);
		return;
	}

	switch (done_ccb->ccb_h.func_code) {
	case XPT_ACCEPT_TARGET_IO:
	{
		struct ccb_accept_tio *atio;
		struct targbh_cmd_desc *descr;
		u_int8_t *cdb;

		atio = &done_ccb->atio;
		descr = (struct targbh_cmd_desc*)atio->ccb_h.ccb_descr;
		cdb = atio->cdb_io.cdb_bytes;
		if (softc->state == TARGBH_STATE_TEARDOWN
		 || atio->ccb_h.status == CAM_REQ_ABORTED) {
			targbhfreedescr(descr);
			free(done_ccb, M_DEVBUF);
			return;
		}

		/*
		 * Determine the type of incoming command and
		 * setup our buffer for a response.
		 */
		switch (cdb[0]) {
		case INQUIRY:
		{
			struct scsi_inquiry *inq;

			inq = (struct scsi_inquiry *)cdb;
			CAM_DEBUG(periph->path, CAM_DEBUG_SUBTRACE,
				  ("Saw an inquiry!\n"));
			/*
			 * Validate the command.  We don't
			 * support any VPD pages, so complain
			 * if EVPD is set.
			 */
			if ((inq->byte2 & SI_EVPD) != 0
			 || inq->page_code != 0) {
				atio->ccb_h.flags &= ~CAM_DIR_MASK;
				atio->ccb_h.flags |= CAM_DIR_NONE;
				/*
				 * This needs to have other than a
				 * no_lun_sense_data response.
				 */
				atio->sense_data = no_lun_sense_data;
				atio->sense_len = sizeof(no_lun_sense_data);
				descr->data_resid = 0;
				descr->data_increment = 0;
				descr->status = SCSI_STATUS_CHECK_COND;
				break;
			}
			/*
			 * Direction is always relative
			 * to the initator.
			 */
			atio->ccb_h.flags &= ~CAM_DIR_MASK;
			atio->ccb_h.flags |= CAM_DIR_IN;
			descr->data = &no_lun_inq_data;
			descr->data_resid = MIN(sizeof(no_lun_inq_data),
						SCSI_CDB6_LEN(inq->length));
			descr->data_increment = descr->data_resid;
			descr->timeout = 5 * 1000;
			descr->status = SCSI_STATUS_OK;
			break;
		}
		case REQUEST_SENSE:
		{
			struct scsi_request_sense *rsense;

			rsense = (struct scsi_request_sense *)cdb;
			/* Refer to static sense data */
			atio->ccb_h.flags &= ~CAM_DIR_MASK;
			atio->ccb_h.flags |= CAM_DIR_IN;
			descr->data = &no_lun_sense_data;
			descr->data_resid = request_sense_size;
			descr->data_resid = MIN(descr->data_resid,
						SCSI_CDB6_LEN(rsense->length));
			descr->data_increment = descr->data_resid;
			descr->timeout = 5 * 1000;
			descr->status = SCSI_STATUS_OK;
			break;
		}
		default:
			/* Constant CA, tell initiator */
			/* Direction is always relative to the initator */
			atio->ccb_h.flags &= ~CAM_DIR_MASK;
			atio->ccb_h.flags |= CAM_DIR_NONE;
			atio->sense_data = no_lun_sense_data;
			atio->sense_len = sizeof (no_lun_sense_data);
			descr->data_resid = 0;
			descr->data_increment = 0;
			descr->timeout = 5 * 1000;
			descr->status = SCSI_STATUS_CHECK_COND;
			break;
		}

		/* Queue us up to receive a Continue Target I/O ccb. */
		TAILQ_INSERT_TAIL(&softc->work_queue, &atio->ccb_h,
				  periph_links.tqe);
		xpt_schedule(periph, /*priority*/1);
		break;
	}
	case XPT_CONT_TARGET_IO:
	{
		struct ccb_accept_tio *atio;
		struct targbh_cmd_desc *desc;

		CAM_DEBUG(periph->path, CAM_DEBUG_SUBTRACE,
			  ("Received completed CTIO\n"));
		atio = (struct ccb_accept_tio*)done_ccb->ccb_h.ccb_atio;
		desc = (struct targbh_cmd_desc *)atio->ccb_h.ccb_descr;

		TAILQ_REMOVE(&softc->pending_queue, &atio->ccb_h,
			     periph_links.tqe);

		/*
		 * We could check for CAM_SENT_SENSE bein set here,
		 * but since we're not maintaining any CA/UA state,
		 * there's no point.
		 */
		atio->sense_len = 0;
		done_ccb->ccb_h.flags &= ~CAM_SEND_SENSE;
		done_ccb->ccb_h.status &= ~CAM_SENT_SENSE;

		/* XXX Check for errors */
		desc->data_resid -= desc->data_increment;
		xpt_release_ccb(done_ccb);
		if (softc->state != TARGBH_STATE_TEARDOWN) {

			/*
			 * Send the original accept TIO back to the
			 * controller to handle more work.
			 */
			CAM_DEBUG(periph->path, CAM_DEBUG_SUBTRACE,
				  ("Returning ATIO to target\n"));
			/* Restore wildcards */
			atio->ccb_h.target_id = CAM_TARGET_WILDCARD;
			atio->ccb_h.target_lun = CAM_LUN_WILDCARD;
			xpt_action((union ccb *)atio);
			break;
		} else {
			targbhfreedescr(desc);
			free(atio, M_DEVBUF);
		}
		break;
	}
	case XPT_IMMED_NOTIFY:
	{
		if (softc->state == TARGBH_STATE_TEARDOWN
		 || done_ccb->ccb_h.status == CAM_REQ_ABORTED) {
			printf("Freed an immediate notify\n");
			free(done_ccb, M_DEVBUF);
		}
		break;
	}
	default:
		panic("targbhdone: Unexpected ccb opcode");
		break;
	}
}

#ifdef NOTYET
static int
targbherror(union ccb *ccb, u_int32_t cam_flags, u_int32_t sense_flags)
{
	return 0;
}
#endif

static struct targbh_cmd_desc*
targbhallocdescr()
{
	struct targbh_cmd_desc* descr;

	/* Allocate the targbh_descr structure */
	descr = (struct targbh_cmd_desc *)malloc(sizeof(*descr),
					       M_DEVBUF, M_NOWAIT);
	if (descr == NULL)
		return (NULL);

	bzero(descr, sizeof(*descr));

	/* Allocate buffer backing store */
	descr->backing_store = malloc(MAX_BUF_SIZE, M_DEVBUF, M_NOWAIT);
	if (descr->backing_store == NULL) {
		free(descr, M_DEVBUF);
		return (NULL);
	}
	descr->max_size = MAX_BUF_SIZE;
	return (descr);
}

static void
targbhfreedescr(struct targbh_cmd_desc *descr)
{
	free(descr->backing_store, M_DEVBUF);
	free(descr, M_DEVBUF);
}
