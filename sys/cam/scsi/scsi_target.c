/*
 * Implementation of a simple Target Mode SCSI Proccessor Target driver for CAM.
 *
 * Copyright (c) 1998, 1999 Justin T. Gibbs.
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
 *      $Id: scsi_target.c,v 1.11 1999/05/07 07:03:03 phk Exp $
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
#include <sys/poll.h>
#include <sys/select.h>	/* For struct selinfo. */
#include <sys/uio.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_extend.h>
#include <cam/cam_periph.h>
#include <cam/cam_queue.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_pt.h>
#include <cam/scsi/scsi_targetio.h>
#include <cam/scsi/scsi_message.h>

typedef enum {
	TARG_STATE_NORMAL,
	TARG_STATE_EXCEPTION,
	TARG_STATE_TEARDOWN
} targ_state;

typedef enum {
	TARG_FLAG_NONE		 = 0x00,
	TARG_FLAG_SEND_EOF	 = 0x01,
	TARG_FLAG_RECEIVE_EOF	 = 0x02,
	TARG_FLAG_LUN_ENABLED	 = 0x04
} targ_flags;

typedef enum {
	TARG_CCB_WORKQ,
	TARG_CCB_WAITING
} targ_ccb_types;

#define MAX_ACCEPT	16
#define MAX_IMMEDIATE	16
#define MAX_BUF_SIZE	256	/* Max inquiry/sense/mode page transfer */
#define MAX_INITIATORS	16	/* XXX More for Fibre-Channel */

#define MIN(a, b) ((a > b) ? b : a)

#define TARG_CONTROL_UNIT 0xffff00ff
#define TARG_IS_CONTROL_DEV(unit) ((unit) == TARG_CONTROL_UNIT)

/* Offsets into our private CCB area for storing accept information */
#define ccb_type	ppriv_field0
#define ccb_descr	ppriv_ptr1

/* We stick a pointer to the originating accept TIO in each continue I/O CCB */
#define ccb_atio	ppriv_ptr1

TAILQ_HEAD(ccb_queue, ccb_hdr);

struct targ_softc {
	struct		ccb_queue pending_queue;
	struct		ccb_queue work_queue;
	struct		ccb_queue snd_ccb_queue;
	struct		ccb_queue rcv_ccb_queue;
	struct		ccb_queue unknown_atio_queue;
	struct		buf_queue_head snd_buf_queue;
	struct		buf_queue_head rcv_buf_queue;
	struct		devstat device_stats;
	struct		selinfo snd_select;
	struct		selinfo rcv_select;
	targ_state	state;
	targ_flags	flags;	
	targ_exception	exceptions;	
	u_int		init_level;
	u_int		inq_data_len;
	struct		scsi_inquiry_data *inq_data;
	struct		ccb_accept_tio *accept_tio_list;
	struct		ccb_hdr_slist immed_notify_slist;
	struct		initiator_state istate[MAX_INITIATORS];
};

struct targ_cmd_desc {
	struct	  ccb_accept_tio* atio_link;
	u_int	  data_resid;	/* How much left to transfer */
	u_int	  data_increment;/* Amount to send before next disconnect */
	void*	  data;		/* The data. Can be from backing_store or not */
	void*	  backing_store;/* Backing store allocated for this descriptor*/
	struct	  buf *bp;	/* Buffer for this transfer */
	u_int	  max_size;	/* Size of backing_store */
	u_int32_t timeout;	
	u_int8_t  status;	/* Status to return to initiator */
};

static	d_open_t	targopen;
static	d_close_t	targclose;
static	d_read_t	targread;
static	d_write_t	targwrite;
static	d_ioctl_t	targioctl;
static	d_poll_t	targpoll;
static	d_strategy_t	targstrategy;

#define TARG_CDEV_MAJOR	65
static struct cdevsw targ_cdevsw = {
	/*d_open*/	targopen,
	/*d_close*/	targclose,
	/*d_read*/	targread,
	/*d_write*/	targwrite,
	/*d_ioctl*/	targioctl,
	/*d_stop*/	nostop,
	/*d_reset*/	noreset,
	/*d_devtotty*/	nodevtotty,
	/*d_poll*/	targpoll,
	/*d_mmap*/	nommap,
	/*d_strategy*/	targstrategy,
	/*d_name*/	"targ",
	/*d_spare*/	NULL,
	/*d_maj*/	-1,
	/*d_dump*/	nodump,
	/*d_psize*/	nopsize,
	/*d_flags*/	0,
	/*d_maxio*/	0,
	/*b_maj*/	-1
};

static int		targsendccb(struct cam_periph *periph, union ccb *ccb,
				    union ccb *inccb);
static periph_init_t	targinit;
static void		targasync(void *callback_arg, u_int32_t code,
				struct cam_path *path, void *arg);
static int		targallocinstance(struct ioc_alloc_unit *alloc_unit);
static int		targfreeinstance(struct ioc_alloc_unit *alloc_unit);
static cam_status	targenlun(struct cam_periph *periph);
static cam_status	targdislun(struct cam_periph *periph);
static periph_ctor_t	targctor;
static periph_dtor_t	targdtor;
static void		targrunqueue(struct cam_periph *periph,
				     struct targ_softc *softc);
static periph_start_t	targstart;
static void		targdone(struct cam_periph *periph,
				 union ccb *done_ccb);
static void		targfireexception(struct cam_periph *periph,
					  struct targ_softc *softc);
static  int		targerror(union ccb *ccb, u_int32_t cam_flags,
				  u_int32_t sense_flags);
static struct targ_cmd_desc*	allocdescr(void);
static void		freedescr(struct targ_cmd_desc *buf);
static void		fill_sense(struct scsi_sense_data *sense,
				   u_int error_code, u_int sense_key,
				   u_int asc, u_int ascq);
					
static struct periph_driver targdriver =
{
	targinit, "targ",
	TAILQ_HEAD_INITIALIZER(targdriver.units), /* generation */ 0
};

DATA_SET(periphdriver_set, targdriver);

static struct extend_array *targperiphs;

static void
targinit(void)
{
	dev_t dev;

	/*
	 * Create our extend array for storing the devices we attach to.
	 */
	targperiphs = cam_extend_new();
	if (targperiphs == NULL) {
		printf("targ: Failed to alloc extend array!\n");
		return;
	}

	/* If we were successfull, register our devsw */
	dev = makedev(TARG_CDEV_MAJOR, 0);
	cdevsw_add(&dev,&targ_cdevsw, NULL);
}

static void
targasync(void *callback_arg, u_int32_t code,
	  struct cam_path *path, void *arg)
{
	struct cam_periph *periph;

	periph = (struct cam_periph *)callback_arg;
	switch (code) {
	case AC_PATH_DEREGISTERED:
	{
		/* XXX Implement */
		break;
	}
	case AC_BUS_RESET:
	{
		/* Flush transaction queue */
	}
	default:
		break;
	}
}

/* Attempt to enable our lun */
static cam_status
targenlun(struct cam_periph *periph)
{
	union ccb immed_ccb;
	struct targ_softc *softc;
	cam_status status;
	int i;

	softc = (struct targ_softc *)periph->softc;

	if ((softc->flags & TARG_FLAG_LUN_ENABLED) != 0)
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
		printf("targenlun - Enable Lun Rejected for status 0x%x\n",
		       status);
		return (status);
	}
	
	softc->flags |= TARG_FLAG_LUN_ENABLED;

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

		atio->ccb_h.ccb_descr = allocdescr();

		if (atio->ccb_h.ccb_descr == NULL) {
			free(atio, M_DEVBUF);
			status = CAM_RESRC_UNAVAIL;
			break;
		}

		xpt_setup_ccb(&atio->ccb_h, periph->path, /*priority*/1);
		atio->ccb_h.func_code = XPT_ACCEPT_TARGET_IO;
		atio->ccb_h.cbfcnp = targdone;
		xpt_action((union ccb *)atio);
		status = atio->ccb_h.status;
		if (status != CAM_REQ_INPROG) {
			xpt_print_path(periph->path);
			printf("Queue of atio failed\n");
			freedescr(atio->ccb_h.ccb_descr);
			free(atio, M_DEVBUF);
			break;
		}
		((struct targ_cmd_desc*)atio->ccb_h.ccb_descr)->atio_link =
		    softc->accept_tio_list;
		softc->accept_tio_list = atio;
	}

	if (i == 0) {
		xpt_print_path(periph->path);
		printf("targenlun - Could not allocate accept tio CCBs: "
		       "status = 0x%x\n", status);
		targdislun(periph);
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
		inot->ccb_h.cbfcnp = targdone;
		xpt_action((union ccb *)inot);
		status = inot->ccb_h.status;
		if (status != CAM_REQ_INPROG) {
			printf("Queue of inot failed\n");
			free(inot, M_DEVBUF);
			break;
		}
		SLIST_INSERT_HEAD(&softc->immed_notify_slist, &inot->ccb_h,
				  periph_links.sle);
	}

	if (i == 0) {
		xpt_print_path(periph->path);
		printf("targenlun - Could not allocate immediate notify CCBs: "
		       "status = 0x%x\n", status);
		targdislun(periph);
		return (CAM_REQ_CMP_ERR);
	}

	return (CAM_REQ_CMP);
}

static cam_status
targdislun(struct cam_periph *periph)
{
	union ccb ccb;
	struct targ_softc *softc;
	struct ccb_accept_tio* atio;
	struct ccb_hdr *ccb_h;

	softc = (struct targ_softc *)periph->softc;
	if ((softc->flags & TARG_FLAG_LUN_ENABLED) == 0)
		return CAM_REQ_CMP;

	/* XXX Block for Continue I/O completion */

	/* Kill off all ACCECPT and IMMEDIATE CCBs */
	while ((atio = softc->accept_tio_list) != NULL) {

		softc->accept_tio_list =
		    ((struct targ_cmd_desc*)atio->ccb_h.ccb_descr)->atio_link;
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
		printf("targdislun - Disabling lun on controller failed "
		       "with status 0x%x\n", ccb.cel.ccb_h.status);
	else 
		softc->flags &= ~TARG_FLAG_LUN_ENABLED;
	return (ccb.cel.ccb_h.status);
}

static cam_status
targctor(struct cam_periph *periph, void *arg)
{
	struct ccb_pathinq *cpi;
	struct targ_softc *softc;
	int i;

	cpi = (struct ccb_pathinq *)arg;

	/* Allocate our per-instance private storage */
	softc = (struct targ_softc *)malloc(sizeof(*softc), M_DEVBUF, M_NOWAIT);
	if (softc == NULL) {
		printf("targctor: unable to malloc softc\n");
		return (CAM_REQ_CMP_ERR);
	}

	bzero(softc, sizeof(softc));
	TAILQ_INIT(&softc->pending_queue);
	TAILQ_INIT(&softc->work_queue);
	TAILQ_INIT(&softc->snd_ccb_queue);
	TAILQ_INIT(&softc->rcv_ccb_queue);
	TAILQ_INIT(&softc->unknown_atio_queue);
	bufq_init(&softc->snd_buf_queue);
	bufq_init(&softc->rcv_buf_queue);
	softc->accept_tio_list = NULL;
	SLIST_INIT(&softc->immed_notify_slist);
	softc->state = TARG_STATE_NORMAL;
	periph->softc = softc;
	softc->init_level++;

	cam_extend_set(targperiphs, periph->unit_number, periph);

	/*
	 * We start out life with a UA to indicate power-on/reset.
	 */
	for (i = 0; i < MAX_INITIATORS; i++)
		softc->istate[i].pending_ua = UA_POWER_ON;
	
	/*
	 * Allocate an initial inquiry data buffer.  We might allow the
	 * user to override this later via an ioctl.
	 */
	softc->inq_data_len = sizeof(*softc->inq_data);
	softc->inq_data = malloc(softc->inq_data_len, M_DEVBUF, M_NOWAIT);
	if (softc->inq_data == NULL) {
		printf("targctor - Unable to malloc inquiry data\n");
		targdtor(periph);
		return (CAM_RESRC_UNAVAIL);
	}
	bzero(softc->inq_data, softc->inq_data_len);
	softc->inq_data->device = T_PROCESSOR | (SID_QUAL_LU_CONNECTED << 5);
	softc->inq_data->version = 2;
	softc->inq_data->response_format = 2; /* SCSI2 Inquiry Format */
	softc->inq_data->flags =
	    cpi->hba_inquiry & (PI_SDTR_ABLE|PI_WIDE_16|PI_WIDE_32);
	softc->inq_data->additional_length = softc->inq_data_len - 4;
	strncpy(softc->inq_data->vendor, "FreeBSD ", SID_VENDOR_SIZE);
	strncpy(softc->inq_data->product, "TM-PT           ", SID_PRODUCT_SIZE);
	strncpy(softc->inq_data->revision, "0.0 ", SID_REVISION_SIZE);
	softc->init_level++;

	return (CAM_REQ_CMP);
}

static void
targdtor(struct cam_periph *periph)
{
	struct targ_softc *softc;

	softc = (struct targ_softc *)periph->softc;

	softc->state = TARG_STATE_TEARDOWN;

	targdislun(periph);

	cam_extend_release(targperiphs, periph->unit_number);

	switch (softc->init_level) {
	default:
		/* FALLTHROUGH */
	case 2:
		free(softc->inq_data, M_DEVBUF);
		/* FALLTHROUGH */
	case 1:
		free(softc, M_DEVBUF);
		break;
	case 0:
		panic("targdtor - impossible init level");;
	}
}

static int
targopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct cam_periph *periph;
	struct	targ_softc *softc;
	u_int unit;
	cam_status status;
	int error;
	int s;

	unit = minor(dev);

	/* An open of the control device always succeeds */
	if (TARG_IS_CONTROL_DEV(unit))
		return 0;

	s = splsoftcam();
	periph = cam_extend_get(targperiphs, unit);
	if (periph == NULL) {
		return (ENXIO);
        	splx(s);
	}
	if ((error = cam_periph_lock(periph, PRIBIO | PCATCH)) != 0) {
		splx(s);
		return (error);
	}

	softc = (struct targ_softc *)periph->softc;
	if ((softc->flags & TARG_FLAG_LUN_ENABLED) == 0) {
		if (cam_periph_acquire(periph) != CAM_REQ_CMP) {
			splx(s);
			cam_periph_unlock(periph);
			return(ENXIO);
		}
	}
        splx(s);
	
	status = targenlun(periph);
	switch (status) {
	case CAM_REQ_CMP:
		error = 0;
		break;
	case CAM_RESRC_UNAVAIL:
		error = ENOMEM;
		break;
	case CAM_LUN_ALRDY_ENA:
		error = EADDRINUSE;
		break;
	default:
		error = ENXIO;
		break;
	}
        cam_periph_unlock(periph);
	return (error);
}

static int
targclose(dev_t dev, int flag, int fmt, struct proc *p)
{
	struct	cam_periph *periph;
	struct	targ_softc *softc;
	u_int	unit;
	int	s;
	int	error;

	unit = minor(dev);

	/* A close of the control device always succeeds */
	if (TARG_IS_CONTROL_DEV(unit))
		return 0;

	s = splsoftcam();
	periph = cam_extend_get(targperiphs, unit);
	if (periph == NULL) {
		splx(s);
		return (ENXIO);
	}
	if ((error = cam_periph_lock(periph, PRIBIO)) != 0)
		return (error);
	softc = (struct targ_softc *)periph->softc;
	splx(s);

	targdislun(periph);

	cam_periph_unlock(periph);
	cam_periph_release(periph);

	return (0);
}

static int
targallocinstance(struct ioc_alloc_unit *alloc_unit)
{
	struct ccb_pathinq cpi;
	struct cam_path *path;
	struct cam_periph *periph;
	cam_status status;
	int free_path_on_return;
	int error;
 
	free_path_on_return = 0;
	status = xpt_create_path(&path, /*periph*/NULL,
				 alloc_unit->path_id,
				 alloc_unit->target_id,
				 alloc_unit->lun_id);
	free_path_on_return++;

	if (status != CAM_REQ_CMP) {
		printf("Couldn't Allocate Path %x\n", status);
		goto fail;
	}

	xpt_setup_ccb(&cpi.ccb_h, path, /*priority*/1);
	cpi.ccb_h.func_code = XPT_PATH_INQ;
	xpt_action((union ccb *)&cpi);
	status = cpi.ccb_h.status;

	if (status != CAM_REQ_CMP) {
		printf("Couldn't CPI %x\n", status);
		goto fail;
	}

	/* Can only alloc units on controllers that support target mode */
	if ((cpi.target_sprt & PIT_PROCESSOR) == 0) {
		printf("Controller does not support target mode%x\n", status);
		status = CAM_PATH_INVALID;
		goto fail;
	}

	/* Ensure that we don't already have an instance for this unit. */
	if ((periph = cam_periph_find(path, "targ")) != NULL) { 
		printf("Lun already enabled%x\n", status);
		status = CAM_LUN_ALRDY_ENA;
		goto fail;
	}

	/*
	 * Allocate a peripheral instance for
	 * this target instance.
	 */
	status = cam_periph_alloc(targctor, NULL, targdtor, targstart,
				  "targ", CAM_PERIPH_BIO, path, targasync,
				  0, &cpi);

fail:
	switch (status) {
	case CAM_REQ_CMP:
	{
		struct cam_periph *periph;

		if ((periph = cam_periph_find(path, "targ")) == NULL)
			panic("targallocinstance: Succeeded but no periph?");
		error = 0;
		alloc_unit->unit = periph->unit_number;
		break;
	}
	case CAM_RESRC_UNAVAIL:
		error = ENOMEM;
		break;
	case CAM_LUN_ALRDY_ENA:
		error = EADDRINUSE;
		break;
	default:
		printf("targallocinstance: Unexpected CAM status %x\n", status);
		/* FALLTHROUGH */
	case CAM_PATH_INVALID:
		error = ENXIO;
		break;
	case CAM_PROVIDE_FAIL:
		error = ENODEV;
		break;
	}

	if (free_path_on_return != 0)
		xpt_free_path(path);

	return (error);
}

static int
targfreeinstance(struct ioc_alloc_unit *alloc_unit)
{
	struct cam_path *path;
	struct cam_periph *periph;
	struct targ_softc *softc;
	cam_status status;
	int free_path_on_return;
	int error;
 
	periph = NULL;
	free_path_on_return = 0;
	status = xpt_create_path(&path, /*periph*/NULL,
				 alloc_unit->path_id,
				 alloc_unit->target_id,
				 alloc_unit->lun_id);
	free_path_on_return++;

	if (status != CAM_REQ_CMP)
		goto fail;

	/* Find our instance. */
	if ((periph = cam_periph_find(path, "targ")) == NULL) {
		xpt_print_path(path);
		status = CAM_PATH_INVALID;
		goto fail;
	}

        softc = (struct targ_softc *)periph->softc;
                                 
        if ((softc->flags & TARG_FLAG_LUN_ENABLED) != 0) {
		status = CAM_BUSY;
		goto fail;
	}

fail:
	if (free_path_on_return != 0)
		xpt_free_path(path);

	switch (status) {
	case CAM_REQ_CMP:
		if (periph != NULL)
			cam_periph_invalidate(periph);
		error = 0;
		break;
	case CAM_RESRC_UNAVAIL:
		error = ENOMEM;
		break;
	case CAM_LUN_ALRDY_ENA:
		error = EADDRINUSE;
		break;
	default:
		printf("targfreeinstance: Unexpected CAM status %x\n", status);
		/* FALLTHROUGH */
	case CAM_PATH_INVALID:
		error = ENODEV;
		break;
	}
	return (error);
}

static int
targioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	struct cam_periph *periph;
	struct targ_softc *softc;
	u_int  unit;
	int    error;

	unit = minor(dev);
	error = 0;
	if (TARG_IS_CONTROL_DEV(unit)) {
		switch (cmd) {
		case TARGCTLIOALLOCUNIT:
			error = targallocinstance((struct ioc_alloc_unit*)addr);
			break;
		case TARGCTLIOFREEUNIT:
			error = targfreeinstance((struct ioc_alloc_unit*)addr);
			break;
		default:
			error = EINVAL;
			break;
		}
		return (error);
	}

	periph = cam_extend_get(targperiphs, unit);
	if (periph == NULL)
		return (ENXIO);
	softc = (struct targ_softc *)periph->softc;
	switch (cmd) {
	case TARGIOCFETCHEXCEPTION:
		*((targ_exception *)addr) = softc->exceptions;
		break;
	case TARGIOCCLEAREXCEPTION:
	{
		targ_exception clear_mask;

		clear_mask = *((targ_exception *)addr);
		if ((clear_mask & TARG_EXCEPT_UNKNOWN_ATIO) != 0) {
			struct ccb_hdr *ccbh;

			ccbh = TAILQ_FIRST(&softc->unknown_atio_queue);
			if (ccbh != NULL) {
				TAILQ_REMOVE(&softc->unknown_atio_queue,
					     ccbh, periph_links.tqe);
				/* Requeue the ATIO back to the controller */
				xpt_action((union ccb *)ccbh);
				ccbh = TAILQ_FIRST(&softc->unknown_atio_queue);
			}
			if (ccbh != NULL)
				clear_mask &= ~TARG_EXCEPT_UNKNOWN_ATIO;
		}
		softc->exceptions &= ~clear_mask;
		if (softc->exceptions == TARG_EXCEPT_NONE
		 && softc->state == TARG_STATE_EXCEPTION) {
			softc->state = TARG_STATE_NORMAL;
			targrunqueue(periph, softc);
		}
		break;
	}
	case TARGIOCFETCHATIO:
	{
		struct ccb_hdr *ccbh;

		ccbh = TAILQ_FIRST(&softc->unknown_atio_queue);
		if (ccbh != NULL) {
			bcopy(ccbh, addr, sizeof(struct ccb_accept_tio));
		} else {
			error = ENOENT;
		}
		break;
	}
	case TARGIOCCOMMAND:
	{
		union ccb *inccb;
		union ccb *ccb;

		/*
		 * XXX JGibbs
		 * This code is lifted directly from the pass-thru driver.
		 * Perhaps this should be moved to a library????
		 */
		inccb = (union ccb *)addr;
		ccb = cam_periph_getccb(periph, inccb->ccb_h.pinfo.priority);

		error = targsendccb(periph, ccb, inccb);

		xpt_release_ccb(ccb);

		break;
	}
	case TARGIOCGETISTATE:
	case TARGIOCSETISTATE:
	{
		struct ioc_initiator_state *ioc_istate;

		ioc_istate = (struct ioc_initiator_state *)addr;
		if (ioc_istate->initiator_id > MAX_INITIATORS) {
			error = EINVAL;
			break;
		}
		CAM_DEBUG(periph->path, CAM_DEBUG_SUBTRACE,
			  ("GET/SETISTATE for %d\n", ioc_istate->initiator_id));
		if (cmd == TARGIOCGETISTATE) {
			bcopy(&softc->istate[ioc_istate->initiator_id],
			      &ioc_istate->istate, sizeof(ioc_istate->istate));
		} else {
			bcopy(&ioc_istate->istate,
			      &softc->istate[ioc_istate->initiator_id],
			      sizeof(ioc_istate->istate));
		CAM_DEBUG(periph->path, CAM_DEBUG_SUBTRACE,
			  ("pending_ca now %x\n",
			   softc->istate[ioc_istate->initiator_id].pending_ca));
		}
		break;
	}
	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

/*
 * XXX JGibbs lifted from pass-thru driver.
 * Generally, "ccb" should be the CCB supplied by the kernel.  "inccb"
 * should be the CCB that is copied in from the user.
 */
static int
targsendccb(struct cam_periph *periph, union ccb *ccb, union ccb *inccb)
{
	struct targ_softc *softc;
	struct cam_periph_map_info mapinfo;
	int error, need_unmap;

	softc = (struct targ_softc *)periph->softc;

	need_unmap = 0;

	/*
	 * There are some fields in the CCB header that need to be
	 * preserved, the rest we get from the user.
	 */
	xpt_merge_ccb(ccb, inccb);

	/*
	 * There's no way for the user to have a completion
	 * function, so we put our own completion function in here.
	 */
	ccb->ccb_h.cbfcnp = targdone;

	/*
	 * We only attempt to map the user memory into kernel space
	 * if they haven't passed in a physical memory pointer,
	 * and if there is actually an I/O operation to perform.
	 * Right now cam_periph_mapmem() only supports SCSI and device
	 * match CCBs.  For the SCSI CCBs, we only pass the CCB in if
	 * there's actually data to map.  cam_periph_mapmem() will do the
	 * right thing, even if there isn't data to map, but since CCBs
	 * without data are a reasonably common occurance (e.g. test unit
	 * ready), it will save a few cycles if we check for it here.
	 */
	if (((ccb->ccb_h.flags & CAM_DATA_PHYS) == 0)
	 && (((ccb->ccb_h.func_code == XPT_CONT_TARGET_IO)
	    && ((ccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE))
	  || (ccb->ccb_h.func_code == XPT_DEV_MATCH))) {

		bzero(&mapinfo, sizeof(mapinfo));

		error = cam_periph_mapmem(ccb, &mapinfo); 

		/*
		 * cam_periph_mapmem returned an error, we can't continue.
		 * Return the error to the user.
		 */
		if (error)
			return(error);

		/*
		 * We successfully mapped the memory in, so we need to
		 * unmap it when the transaction is done.
		 */
		need_unmap = 1;
	}

	/*
	 * If the user wants us to perform any error recovery, then honor
	 * that request.  Otherwise, it's up to the user to perform any
	 * error recovery.
	 */
	error = cam_periph_runccb(ccb,
				  (ccb->ccb_h.flags & CAM_PASS_ERR_RECOVER) ?
				  targerror : NULL,
				  /* cam_flags */ 0,
				  /* sense_flags */SF_RETRY_UA,
				  &softc->device_stats);

	if (need_unmap != 0)
		cam_periph_unmapmem(ccb, &mapinfo);

	ccb->ccb_h.cbfcnp = NULL;
	ccb->ccb_h.periph_priv = inccb->ccb_h.periph_priv;
	bcopy(ccb, inccb, sizeof(union ccb));

	return(error);
}


static int
targpoll(dev_t dev, int poll_events, struct proc *p)
{
	struct cam_periph *periph;
	struct targ_softc *softc;
	u_int  unit;
	int    revents;
	int    s;

	unit = minor(dev);

	/* ioctl is the only supported operation of the control device */
	if (TARG_IS_CONTROL_DEV(unit))
		return EINVAL;

	periph = cam_extend_get(targperiphs, unit);
	if (periph == NULL)
		return (ENXIO);
	softc = (struct targ_softc *)periph->softc;

	revents = 0;
	s = splcam();
	if ((poll_events & (POLLOUT | POLLWRNORM)) != 0) {
		if (TAILQ_FIRST(&softc->rcv_ccb_queue) != NULL
		 && bufq_first(&softc->rcv_buf_queue) == NULL)
			revents |= poll_events & (POLLOUT | POLLWRNORM);
	}
	if ((poll_events & (POLLIN | POLLRDNORM)) != 0) {
		if (TAILQ_FIRST(&softc->snd_ccb_queue) != NULL
		 && bufq_first(&softc->snd_buf_queue) == NULL)
			revents |= poll_events & (POLLIN | POLLRDNORM);
	}

	if (softc->state != TARG_STATE_NORMAL)
		revents |= POLLERR;

	if (revents == 0) {
		if (poll_events & (POLLOUT | POLLWRNORM))
			selrecord(p, &softc->rcv_select);
		if (poll_events & (POLLIN | POLLRDNORM))
			selrecord(p, &softc->snd_select);
	}
	splx(s);
	return (revents);
}

static int
targread(dev_t dev, struct uio *uio, int ioflag)
{
	u_int  unit;

	unit = minor(dev);
	/* ioctl is the only supported operation of the control device */
	if (TARG_IS_CONTROL_DEV(unit))
		return EINVAL;

	if (uio->uio_iovcnt == 0
	 || uio->uio_iov->iov_len == 0) {
		/* EOF */
		struct cam_periph *periph;
		struct targ_softc *softc;
		int    s;
	
		s = splcam();
		periph = cam_extend_get(targperiphs, unit);
		if (periph == NULL)
			return (ENXIO);
		softc = (struct targ_softc *)periph->softc;
		softc->flags |= TARG_FLAG_SEND_EOF;
		splx(s);
		targrunqueue(periph, softc);
		return (0);
	}
	return(physread(dev, uio, ioflag));
}

static int
targwrite(dev_t dev, struct uio *uio, int ioflag)
{
	u_int  unit;

	unit = minor(dev);
	/* ioctl is the only supported operation of the control device */
	if (TARG_IS_CONTROL_DEV(unit))
		return EINVAL;

	if (uio->uio_iovcnt == 0
	 || uio->uio_iov->iov_len == 0) {
		/* EOF */
		struct cam_periph *periph;
		struct targ_softc *softc;
		int    s;
	
		s = splcam();
		periph = cam_extend_get(targperiphs, unit);
		if (periph == NULL)
			return (ENXIO);
		softc = (struct targ_softc *)periph->softc;
		softc->flags |= TARG_FLAG_RECEIVE_EOF;
		splx(s);
		targrunqueue(periph, softc);
		return (0);
	}
	return(physwrite(dev, uio, ioflag));
}

/*
 * Actually translate the requested transfer into one the physical driver
 * can understand.  The transfer is described by a buf and will include
 * only one physical transfer.
 */
static void
targstrategy(struct buf *bp)
{
	struct cam_periph *periph;
	struct targ_softc *softc;
	u_int  unit;
	int    s;
	
	unit = minor(bp->b_dev);

	/* ioctl is the only supported operation of the control device */
	if (TARG_IS_CONTROL_DEV(unit)) {
		bp->b_error = EINVAL;
		goto bad;
	}

	periph = cam_extend_get(targperiphs, unit);
	if (periph == NULL) {
		bp->b_error = ENXIO;
		goto bad;
	}
	softc = (struct targ_softc *)periph->softc;

	/*
	 * Mask interrupts so that the device cannot be invalidated until
	 * after we are in the queue.  Otherwise, we might not properly
	 * clean up one of the buffers.
	 */
	s = splbio();
	
	/*
	 * If there is an exception pending, error out
	 */
	if (softc->state != TARG_STATE_NORMAL) {
		splx(s);
		if (softc->state == TARG_STATE_EXCEPTION
		 && (softc->exceptions & TARG_EXCEPT_DEVICE_INVALID) == 0)
			bp->b_error = EBUSY;
		else
			bp->b_error = ENXIO;
		goto bad;
	}
	
	/*
	 * Place it in the queue of buffers available for either
	 * SEND or RECEIVE commands.
	 * 
	 */
	bp->b_resid = bp->b_bcount;
	if ((bp->b_flags & B_READ) != 0) {
		CAM_DEBUG(periph->path, CAM_DEBUG_SUBTRACE,
			  ("Queued a SEND buffer\n"));
		bufq_insert_tail(&softc->snd_buf_queue, bp);
	} else {
		CAM_DEBUG(periph->path, CAM_DEBUG_SUBTRACE,
			  ("Queued a RECEIVE buffer\n"));
		bufq_insert_tail(&softc->rcv_buf_queue, bp);
	}

	splx(s);
	
	/*
	 * Attempt to use the new buffer to service any pending
	 * target commands.
	 */
	targrunqueue(periph, softc);

	return;
bad:
	bp->b_flags |= B_ERROR;

	/*
	 * Correctly set the buf to indicate a completed xfer
	 */
	bp->b_resid = bp->b_bcount;
	biodone(bp);
}

static void
targrunqueue(struct cam_periph *periph, struct targ_softc *softc)
{
	struct  ccb_queue *pending_queue;
	struct	ccb_accept_tio *atio;
	struct	buf_queue_head *bufq;
	struct	buf *bp;
	struct	targ_cmd_desc *desc;
	struct	ccb_hdr *ccbh;
	int	s;

	s = splbio();
	pending_queue = NULL;
	bufq = NULL;
	ccbh = NULL;
	/* Only run one request at a time to maintain data ordering. */
	if (softc->state != TARG_STATE_NORMAL
	 || TAILQ_FIRST(&softc->work_queue) != NULL
	 || TAILQ_FIRST(&softc->pending_queue) != NULL) {
		splx(s);
		return;
	}

	if (((bp = bufq_first(&softc->snd_buf_queue)) != NULL
	  || (softc->flags & TARG_FLAG_SEND_EOF) != 0)
	 && (ccbh = TAILQ_FIRST(&softc->snd_ccb_queue)) != NULL) {

		if (bp == NULL)
			softc->flags &= ~TARG_FLAG_SEND_EOF;
		else {
			CAM_DEBUG(periph->path, CAM_DEBUG_SUBTRACE,
				  ("De-Queued a SEND buffer %ld\n",
				   bp->b_bcount));
		}
		bufq = &softc->snd_buf_queue;
		pending_queue = &softc->snd_ccb_queue;
	} else if (((bp = bufq_first(&softc->rcv_buf_queue)) != NULL
	  	 || (softc->flags & TARG_FLAG_RECEIVE_EOF) != 0)
		&& (ccbh = TAILQ_FIRST(&softc->rcv_ccb_queue)) != NULL) {
		
		if (bp == NULL)
			softc->flags &= ~TARG_FLAG_RECEIVE_EOF;
		else {
			CAM_DEBUG(periph->path, CAM_DEBUG_SUBTRACE,
				  ("De-Queued a RECEIVE buffer %ld\n",
				   bp->b_bcount));
		}
		bufq = &softc->rcv_buf_queue;
		pending_queue = &softc->rcv_ccb_queue;
	}

	if (pending_queue != NULL) {
		/* Process a request */
		atio = (struct ccb_accept_tio *)ccbh;
		TAILQ_REMOVE(pending_queue, ccbh, periph_links.tqe);
		desc = (struct targ_cmd_desc *)atio->ccb_h.ccb_descr;
		desc->bp = bp;
		if (bp == NULL) {
			/* EOF */
			desc->data = NULL;
			desc->data_increment = 0;
			desc->data_resid = 0;
			atio->ccb_h.flags &= ~CAM_DIR_MASK;
			atio->ccb_h.flags |= CAM_DIR_NONE;
		} else {
			bufq_remove(bufq, bp);
			desc->data = &bp->b_data[bp->b_bcount - bp->b_resid];
			desc->data_increment =
			    MIN(desc->data_resid, bp->b_resid);
		}
		CAM_DEBUG(periph->path, CAM_DEBUG_SUBTRACE,
			  ("Buffer command: data %x: datacnt %d\n",
			   (intptr_t)desc->data, desc->data_increment));
		TAILQ_INSERT_TAIL(&softc->work_queue, &atio->ccb_h,
				  periph_links.tqe);
	}
	if (TAILQ_FIRST(&softc->work_queue) != NULL) {
		splx(s);
		xpt_schedule(periph, /*XXX priority*/1);
	} else 
		splx(s);
}

static void
targstart(struct cam_periph *periph, union ccb *start_ccb)
{
	struct targ_softc *softc;
	struct ccb_hdr *ccbh;
	struct ccb_accept_tio *atio;
	struct targ_cmd_desc *desc;
	struct ccb_scsiio *csio;
	ccb_flags flags;
	int    s;

	softc = (struct targ_softc *)periph->softc;
	
	s = splbio();
	ccbh = TAILQ_FIRST(&softc->work_queue);
	if (periph->immediate_priority <= periph->pinfo.priority) {
		start_ccb->ccb_h.ccb_type = TARG_CCB_WAITING;			
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
		desc = (struct targ_cmd_desc *)atio->ccb_h.ccb_descr;

		/* Is this a tagged request? */
		flags = atio->ccb_h.flags & (CAM_TAG_ACTION_VALID|CAM_DIR_MASK);

		/*
		 * If we are done with the transaction, tell the
		 * controller to send status and perform a CMD_CMPLT.
		 */
		if (desc->data_resid == desc->data_increment)
			flags |= CAM_SEND_STATUS;

		csio = &start_ccb->csio;
		cam_fill_ctio(csio,
			      /*retries*/2,
			      targdone,
			      flags,
			      /*tag_action*/MSG_SIMPLE_Q_TAG,
			      atio->tag_id,
			      atio->init_id,
			      desc->status,
			      /*data_ptr*/desc->data_increment == 0
					  ? NULL : desc->data,
			      /*dxfer_len*/desc->data_increment,
			      /*timeout*/desc->timeout);

		start_ccb->ccb_h.ccb_type = TARG_CCB_WORKQ;
		start_ccb->ccb_h.ccb_atio = atio;
		CAM_DEBUG(periph->path, CAM_DEBUG_SUBTRACE,
			  ("Sending a CTIO\n"));
		xpt_action(start_ccb);
		s = splbio();
		ccbh = TAILQ_FIRST(&softc->work_queue);
		splx(s);
	}
	if (ccbh != NULL)
		targrunqueue(periph, softc);
}

static void
targdone(struct cam_periph *periph, union ccb *done_ccb)
{
	struct targ_softc *softc;

	softc = (struct targ_softc *)periph->softc;

	if (done_ccb->ccb_h.ccb_type == TARG_CCB_WAITING) {
		/* Caller will release the CCB */
		wakeup(&done_ccb->ccb_h.cbfcnp);
		return;
	}

	switch (done_ccb->ccb_h.func_code) {
	case XPT_ACCEPT_TARGET_IO:
	{
		struct ccb_accept_tio *atio;
		struct targ_cmd_desc *descr;
		struct initiator_state *istate;
		u_int8_t *cdb;

		atio = &done_ccb->atio;
		descr = (struct targ_cmd_desc*)atio->ccb_h.ccb_descr;
		istate = &softc->istate[atio->init_id];
		cdb = atio->cdb_io.cdb_bytes;
		if (softc->state == TARG_STATE_TEARDOWN
		 || atio->ccb_h.status == CAM_REQ_ABORTED) {
			freedescr(descr);
			free(done_ccb, M_DEVBUF);
			return;
		}

		if (istate->pending_ca == 0
		 && istate->pending_ua != 0
		 && cdb[0] != INQUIRY) {
			/* Pending UA, tell initiator */
			/* Direction is always relative to the initator */
			istate->pending_ca = CA_UNIT_ATTN;
			atio->ccb_h.flags &= ~CAM_DIR_MASK;
			atio->ccb_h.flags |= CAM_DIR_NONE;
			descr->data_resid = 0;
			descr->data_increment = 0;
			descr->timeout = 5 * 1000;
			descr->status = SCSI_STATUS_CHECK_COND;
		} else { 
			/*
			 * Save the current CA and UA status so
			 * they can be used by this command.
			 */
			ua_types pending_ua;
			ca_types pending_ca;

			pending_ua = istate->pending_ua;
			pending_ca = istate->pending_ca;

			/*
			 * As per the SCSI2 spec, any command that occurs
			 * after a CA is reported, clears the CA.  If the
			 * command is not an inquiry, we are also supposed
			 * to clear the UA condition, if any, that caused
			 * the CA to occur assuming the UA is not a
			 * persistant state.
			 */
			istate->pending_ca = CA_NONE;
			if ((pending_ca
			   & (CA_CMD_SENSE|CA_UNIT_ATTN)) == CA_UNIT_ATTN
			 && cdb[0] != INQUIRY)
				istate->pending_ua = UA_NONE;

			/*
			 * Determine the type of incoming command and
			 * setup our buffer for a response.
			 */
			switch (cdb[0]) {
			case INQUIRY:
			{
				struct scsi_inquiry *inq;
				struct scsi_sense_data *sense;

				inq = (struct scsi_inquiry *)cdb;
				sense = &istate->sense_data;
				CAM_DEBUG(periph->path, CAM_DEBUG_SUBTRACE,
					  ("Saw an inquiry!\n"));
				/*
				 * Validate the command.  We don't
				 * support any VPD pages, so complain
				 * if EVPD is set.
				 */
				if ((inq->byte2 & SI_EVPD) != 0
				 || inq->page_code != 0) {
					istate->pending_ca = CA_CMD_SENSE;
					atio->ccb_h.flags &= ~CAM_DIR_MASK;
					atio->ccb_h.flags |= CAM_DIR_NONE;
					descr->data_resid = 0;
					descr->data_increment = 0;
					descr->status = SCSI_STATUS_CHECK_COND;
					fill_sense(sense,
						   SSD_CURRENT_ERROR,
						   SSD_KEY_ILLEGAL_REQUEST,
						   /*asc*/0x24, /*ascq*/0x00);
					sense->extra_len =
						offsetof(struct scsi_sense_data,
							 extra_bytes)
					      - offsetof(struct scsi_sense_data,
							 extra_len);
				}

				if ((inq->byte2 & SI_EVPD) != 0) {
					sense->sense_key_spec[0] = 
					    SSD_SCS_VALID|SSD_FIELDPTR_CMD
					   |SSD_BITPTR_VALID| /*bit value*/1;
					sense->sense_key_spec[1] = 0;
					sense->sense_key_spec[2] = 
					    offsetof(struct scsi_inquiry,
						     byte2);
					break;
				} else if (inq->page_code != 0) {
					sense->sense_key_spec[0] = 
					    SSD_SCS_VALID|SSD_FIELDPTR_CMD;
					sense->sense_key_spec[1] = 0;
					sense->sense_key_spec[2] = 
					    offsetof(struct scsi_inquiry,
						     page_code);
					break;
				}
				/*
				 * Direction is always relative
				 * to the initator.
				 */
				atio->ccb_h.flags &= ~CAM_DIR_MASK;
				atio->ccb_h.flags |= CAM_DIR_IN;
				descr->data = softc->inq_data;
				descr->data_resid = MIN(softc->inq_data_len,
						       inq->length);
				descr->data_increment = descr->data_resid;
				descr->timeout = 5 * 1000;
				descr->status = SCSI_STATUS_OK;
				break;
			}
			case TEST_UNIT_READY:
				atio->ccb_h.flags &= ~CAM_DIR_MASK;
				atio->ccb_h.flags |= CAM_DIR_NONE;
				descr->data_resid = 0;
				descr->data_increment = 0;
				descr->timeout = 5 * 1000;
				descr->status = SCSI_STATUS_OK;
				break;
			case REQUEST_SENSE:
			{
				struct scsi_request_sense *rsense;
				struct scsi_sense_data *sense;

				rsense = (struct scsi_request_sense *)cdb;
				sense = &istate->sense_data;
				if (pending_ca == 0) {
					fill_sense(sense, SSD_CURRENT_ERROR,
						   SSD_KEY_NO_SENSE, 0x00,
						   0x00);
					CAM_DEBUG(periph->path,
						  CAM_DEBUG_SUBTRACE,
						  ("No pending CA!\n"));
				} else if (pending_ca == CA_UNIT_ATTN) {
					u_int ascq;

					if (pending_ua == UA_POWER_ON)
						ascq = 0x1;
					else
						ascq = 0x2;
					fill_sense(sense, SSD_CURRENT_ERROR,
						   SSD_KEY_UNIT_ATTENTION,
						   0x29, ascq);
					CAM_DEBUG(periph->path,
						  CAM_DEBUG_SUBTRACE,
						  ("Pending UA!\n"));
				}
				/*
				 * Direction is always relative
				 * to the initator.
				 */
				atio->ccb_h.flags &= ~CAM_DIR_MASK;
				atio->ccb_h.flags |= CAM_DIR_IN;
				descr->data = sense;
				descr->data_resid =
			 		offsetof(struct scsi_sense_data,
						 extra_len)
				      + sense->extra_len;
				descr->data_resid = MIN(descr->data_resid,
						       rsense->length);
				descr->data_increment = descr->data_resid;
				descr->timeout = 5 * 1000;
				descr->status = SCSI_STATUS_OK;
				break;
			}
			case RECEIVE:
			case SEND:
			{
				struct scsi_send_receive *sr;

				sr = (struct scsi_send_receive *)cdb;

				/*
				 * Direction is always relative
				 * to the initator.
				 */
				atio->ccb_h.flags &= ~CAM_DIR_MASK;
				descr->data_resid = scsi_3btoul(sr->xfer_len);
				descr->timeout = 5 * 1000;
				descr->status = SCSI_STATUS_OK;
				if (cdb[0] == SEND) {
					atio->ccb_h.flags |= CAM_DIR_OUT;
					CAM_DEBUG(periph->path,
						  CAM_DEBUG_SUBTRACE,
						  ("Saw a SEND!\n"));
					atio->ccb_h.flags |= CAM_DIR_OUT;
					TAILQ_INSERT_TAIL(&softc->snd_ccb_queue,
							  &atio->ccb_h,
							  periph_links.tqe);
					selwakeup(&softc->snd_select);
				} else {
					atio->ccb_h.flags |= CAM_DIR_IN;
					CAM_DEBUG(periph->path,
						  CAM_DEBUG_SUBTRACE,
						  ("Saw a RECEIVE!\n"));
					TAILQ_INSERT_TAIL(&softc->rcv_ccb_queue,
							  &atio->ccb_h,
							  periph_links.tqe);
					selwakeup(&softc->rcv_select);
				}
				/*
				 * Attempt to satisfy this request with
				 * a user buffer.
				 */
				targrunqueue(periph, softc);
				return;
			}
			default:
				/*
				 * Queue for consumption by our userland
				 * counterpart and  transition to the exception
				 * state.
				 */
				TAILQ_INSERT_TAIL(&softc->unknown_atio_queue,
						  &atio->ccb_h,
						  periph_links.tqe);
				softc->exceptions |= TARG_EXCEPT_UNKNOWN_ATIO;
				targfireexception(periph, softc);
				return;
			}
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
		struct targ_cmd_desc *desc;
		struct buf *bp;

		CAM_DEBUG(periph->path, CAM_DEBUG_SUBTRACE,
			  ("Received completed CTIO\n"));
		atio = (struct ccb_accept_tio*)done_ccb->ccb_h.ccb_atio;
		desc = (struct targ_cmd_desc *)atio->ccb_h.ccb_descr;

		TAILQ_REMOVE(&softc->pending_queue, &atio->ccb_h,
			     periph_links.tqe);

		/* XXX Check for errors */
		if ((done_ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			
		}
		desc->data_resid -= desc->data_increment;
		if ((bp = desc->bp) != NULL) {

			bp->b_resid -= desc->data_increment;
			bp->b_error = 0;

			CAM_DEBUG(periph->path, CAM_DEBUG_SUBTRACE,
				  ("Buffer I/O Completed - Resid %ld:%d\n",
				   bp->b_resid, desc->data_resid));
			/*
			 * Send the buffer back to the client if
			 * either the command has completed or all
			 * buffer space has been consumed.
			 */
			if (desc->data_resid == 0
			 || bp->b_resid == 0) {
				if (bp->b_resid != 0)
					/* Short transfer */
					bp->b_flags |= B_ERROR;
				
				CAM_DEBUG(periph->path, CAM_DEBUG_SUBTRACE,
					  ("Completing a buffer\n"));
				biodone(bp);
				desc->bp = NULL;
			}
		}

		xpt_release_ccb(done_ccb);
		if (softc->state != TARG_STATE_TEARDOWN) {

			if (desc->data_resid == 0) {
				/*
				 * Send the original accept TIO back to the
				 * controller to handle more work.
				 */
				CAM_DEBUG(periph->path, CAM_DEBUG_SUBTRACE,
					  ("Returning ATIO to target\n"));
				xpt_action((union ccb *)atio);
				break;
			}

			/* Queue us up for another buffer */
			if (atio->cdb_io.cdb_bytes[0] == SEND) {
				if (desc->bp != NULL)
					TAILQ_INSERT_HEAD(
						&softc->snd_buf_queue.queue,
						bp, b_act);
				TAILQ_INSERT_HEAD(&softc->snd_ccb_queue,
						  &atio->ccb_h,
						  periph_links.tqe);
			} else {
				if (desc->bp != NULL)
					TAILQ_INSERT_HEAD(
						&softc->rcv_buf_queue.queue,
						bp, b_act);
				TAILQ_INSERT_HEAD(&softc->rcv_ccb_queue,
						  &atio->ccb_h,
						  periph_links.tqe);
			}
			desc->bp = NULL;
			targrunqueue(periph, softc);
		} else {
			if (desc->bp != NULL) {
				bp->b_flags |= B_ERROR;
				bp->b_error = ENXIO;
				biodone(bp);
			}
			freedescr(desc);
			free(atio, M_DEVBUF);
		}
		break;
	}
	case XPT_IMMED_NOTIFY:
	{
		if (softc->state == TARG_STATE_TEARDOWN
		 || done_ccb->ccb_h.status == CAM_REQ_ABORTED)
			free(done_ccb, M_DEVBUF);
		break;
	}
	default:
		panic("targdone: Impossible xpt opcode %x encountered.",
		      done_ccb->ccb_h.func_code);
		/* NOTREACHED */
		break;
	}
}

/*
 * Transition to the exception state and notify our symbiotic
 * userland process of the change.
 */
static void
targfireexception(struct cam_periph *periph, struct targ_softc *softc)
{
	/*
	 * return all pending buffers with short read/write status so our
	 * process unblocks, and do a selwakeup on any process queued
	 * waiting for reads or writes.  When the selwakeup is performed,
	 * the waking process will wakeup, call our poll routine again,
	 * and pick up the exception.
	 */
	struct buf *bp;

	if (softc->state != TARG_STATE_NORMAL)
		/* Already either tearing down or in exception state */
		return;

	softc->state = TARG_STATE_EXCEPTION;

	while ((bp = bufq_first(&softc->snd_buf_queue)) != NULL) {
		bufq_remove(&softc->snd_buf_queue, bp);
		bp->b_flags |= B_ERROR;
		biodone(bp);
	}

	while ((bp = bufq_first(&softc->rcv_buf_queue)) != NULL) {
		bufq_remove(&softc->snd_buf_queue, bp);
		bp->b_flags |= B_ERROR;
		biodone(bp);
	}

	selwakeup(&softc->snd_select);
	selwakeup(&softc->rcv_select);
}

static int
targerror(union ccb *ccb, u_int32_t cam_flags, u_int32_t sense_flags)
{
	return 0;
}

static struct targ_cmd_desc*
allocdescr()
{
	struct targ_cmd_desc* descr;

	/* Allocate the targ_descr structure */
	descr = (struct targ_cmd_desc *)malloc(sizeof(*descr),
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
freedescr(struct targ_cmd_desc *descr)
{
	free(descr->backing_store, M_DEVBUF);
	free(descr, M_DEVBUF);
}

static void
fill_sense(struct scsi_sense_data *sense, u_int error_code, u_int sense_key,
	   u_int asc, u_int ascq)
{
	bzero(sense, sizeof(*sense));
	sense->error_code = error_code;
	sense->flags = sense_key;
	sense->add_sense_code = asc;
	sense->add_sense_code_qual = ascq;

	sense->extra_len = offsetof(struct scsi_sense_data, fru)
			 - offsetof(struct scsi_sense_data, extra_len);
}
