/*
 * Copyright (c) 1997, 1998 Justin T. Gibbs.
 * Copyright (c) 1997, 1998, 1999 Kenneth D. Merry.
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/bio.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/devicestat.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_extend.h>
#include <cam/cam_periph.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
#include <cam/scsi/scsi_da.h>
#include <cam/scsi/scsi_pass.h>

typedef enum {
	PASS_FLAG_OPEN			= 0x01,
	PASS_FLAG_LOCKED		= 0x02,
	PASS_FLAG_INVALID		= 0x04
} pass_flags;

typedef enum {
	PASS_STATE_NORMAL
} pass_state;

typedef enum {
	PASS_CCB_BUFFER_IO,
	PASS_CCB_WAITING
} pass_ccb_types;

#define ccb_type	ppriv_field0
#define ccb_bp		ppriv_ptr1

struct pass_softc {
	pass_state	state;
	pass_flags	flags;
	u_int8_t	pd_type;
	struct		bio_queue_head bio_queue;
	union ccb	saved_ccb;
	struct devstat	device_stats;
	dev_t		dev;
};

#ifndef MIN
#define MIN(x,y) ((x<y) ? x : y)
#endif

#define PASS_CDEV_MAJOR 31

static	d_open_t	passopen;
static	d_close_t	passclose;
static	d_ioctl_t	passioctl;
static	d_strategy_t	passstrategy;

static	periph_init_t	passinit;
static	periph_ctor_t	passregister;
static	periph_oninv_t	passoninvalidate;
static	periph_dtor_t	passcleanup;
static	periph_start_t	passstart;
static	void		passasync(void *callback_arg, u_int32_t code,
				  struct cam_path *path, void *arg);
static	void		passdone(struct cam_periph *periph, 
				 union ccb *done_ccb);
static	int		passerror(union ccb *ccb, u_int32_t cam_flags, 
				  u_int32_t sense_flags);
static 	int		passsendccb(struct cam_periph *periph, union ccb *ccb,
				    union ccb *inccb);

static struct periph_driver passdriver =
{
	passinit, "pass",
	TAILQ_HEAD_INITIALIZER(passdriver.units), /* generation */ 0
};

DATA_SET(periphdriver_set, passdriver);

static struct cdevsw pass_cdevsw = {
	/* open */	passopen,
	/* close */	passclose,
	/* read */	physread,
	/* write */	physwrite,
	/* ioctl */	passioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	passstrategy,
	/* name */	"pass",
	/* maj */	PASS_CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
	/* bmaj */	-1
};

static struct extend_array *passperiphs;

static void
passinit(void)
{
	cam_status status;
	struct cam_path *path;

	/*
	 * Create our extend array for storing the devices we attach to.
	 */
	passperiphs = cam_extend_new();
	if (passperiphs == NULL) {
		printf("passm: Failed to alloc extend array!\n");
		return;
	}

	/*
	 * Install a global async callback.  This callback will
	 * receive async callbacks like "new device found".
	 */
	status = xpt_create_path(&path, /*periph*/NULL, CAM_XPT_PATH_ID,
				 CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD);

	if (status == CAM_REQ_CMP) {
		struct ccb_setasync csa;

                xpt_setup_ccb(&csa.ccb_h, path, /*priority*/5);
                csa.ccb_h.func_code = XPT_SASYNC_CB;
                csa.event_enable = AC_FOUND_DEVICE;
                csa.callback = passasync;
                csa.callback_arg = NULL;
                xpt_action((union ccb *)&csa);
		status = csa.ccb_h.status;
                xpt_free_path(path);
        }

	if (status != CAM_REQ_CMP) {
		printf("pass: Failed to attach master async callback "
		       "due to status 0x%x!\n", status);
	}
	
}

static void
passoninvalidate(struct cam_periph *periph)
{
	int s;
	struct pass_softc *softc;
	struct bio *q_bp;
	struct ccb_setasync csa;

	softc = (struct pass_softc *)periph->softc;

	/*
	 * De-register any async callbacks.
	 */
	xpt_setup_ccb(&csa.ccb_h, periph->path,
		      /* priority */ 5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = 0;
	csa.callback = passasync;
	csa.callback_arg = periph;
	xpt_action((union ccb *)&csa);

	softc->flags |= PASS_FLAG_INVALID;

	/*
	 * Although the oninvalidate() routines are always called at
	 * splsoftcam, we need to be at splbio() here to keep the buffer
	 * queue from being modified while we traverse it.
	 */
	s = splbio();

	/*
	 * Return all queued I/O with ENXIO.
	 * XXX Handle any transactions queued to the card
	 *     with XPT_ABORT_CCB.
	 */
	while ((q_bp = bioq_first(&softc->bio_queue)) != NULL){
		bioq_remove(&softc->bio_queue, q_bp);
		q_bp->bio_resid = q_bp->bio_bcount;
		q_bp->bio_error = ENXIO;
		q_bp->bio_flags |= BIO_ERROR;
		biodone(q_bp);
	}
	splx(s);

	if (bootverbose) {
		xpt_print_path(periph->path);
		printf("lost device\n");
	}

}

static void
passcleanup(struct cam_periph *periph)
{
	struct pass_softc *softc;

	softc = (struct pass_softc *)periph->softc;

	devstat_remove_entry(&softc->device_stats);

	destroy_dev(softc->dev);

	cam_extend_release(passperiphs, periph->unit_number);

	if (bootverbose) {
		xpt_print_path(periph->path);
		printf("removing device entry\n");
	}
	free(softc, M_DEVBUF);
}

static void
passasync(void *callback_arg, u_int32_t code,
	  struct cam_path *path, void *arg)
{
	struct cam_periph *periph;

	periph = (struct cam_periph *)callback_arg;

	switch (code) {
	case AC_FOUND_DEVICE:
	{
		struct ccb_getdev *cgd;
		cam_status status;
 
		cgd = (struct ccb_getdev *)arg;

		/*
		 * Allocate a peripheral instance for
		 * this device and start the probe
		 * process.
		 */
		status = cam_periph_alloc(passregister, passoninvalidate,
					  passcleanup, passstart, "pass",
					  CAM_PERIPH_BIO, cgd->ccb_h.path,
					  passasync, AC_FOUND_DEVICE, cgd);

		if (status != CAM_REQ_CMP
		 && status != CAM_REQ_INPROG)
			printf("passasync: Unable to attach new device "
				"due to status 0x%x\n", status);

		break;
	}
	default:
		cam_periph_async(periph, code, path, arg);
		break;
	}
}

static cam_status
passregister(struct cam_periph *periph, void *arg)
{
	struct pass_softc *softc;
	struct ccb_setasync csa;
	struct ccb_getdev *cgd;

	cgd = (struct ccb_getdev *)arg;
	if (periph == NULL) {
		printf("passregister: periph was NULL!!\n");
		return(CAM_REQ_CMP_ERR);
	}

	if (cgd == NULL) {
		printf("passregister: no getdev CCB, can't register device\n");
		return(CAM_REQ_CMP_ERR);
	}

	softc = (struct pass_softc *)malloc(sizeof(*softc),
					    M_DEVBUF, M_NOWAIT);

	if (softc == NULL) {
		printf("passregister: Unable to probe new device. "
		       "Unable to allocate softc\n");				
		return(CAM_REQ_CMP_ERR);
	}

	bzero(softc, sizeof(*softc));
	softc->state = PASS_STATE_NORMAL;
	softc->pd_type = SID_TYPE(&cgd->inq_data);
	bioq_init(&softc->bio_queue);

	periph->softc = softc;

	cam_extend_set(passperiphs, periph->unit_number, periph);
	/*
	 * We pass in 0 for a blocksize, since we don't 
	 * know what the blocksize of this device is, if 
	 * it even has a blocksize.
	 */
	devstat_add_entry(&softc->device_stats, "pass", periph->unit_number,
			  0, DEVSTAT_NO_BLOCKSIZE | DEVSTAT_NO_ORDERED_TAGS,
			  softc->pd_type |
			  DEVSTAT_TYPE_IF_SCSI |
			  DEVSTAT_TYPE_PASS,
			  DEVSTAT_PRIORITY_PASS);

	/* Register the device */
	softc->dev = make_dev(&pass_cdevsw, periph->unit_number, UID_ROOT,
			      GID_OPERATOR, 0600, "%s%d", periph->periph_name,
			      periph->unit_number);

	/*
	 * Add an async callback so that we get
	 * notified if this device goes away.
	 */
	xpt_setup_ccb(&csa.ccb_h, periph->path, /* priority */ 5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = AC_LOST_DEVICE;
	csa.callback = passasync;
	csa.callback_arg = periph;
	xpt_action((union ccb *)&csa);

	if (bootverbose)
		xpt_announce_periph(periph, NULL);

	return(CAM_REQ_CMP);
}

static int
passopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct cam_periph *periph;
	struct pass_softc *softc;
	int unit, error;
	int s;

	error = 0; /* default to no error */

	/* unit = dkunit(dev); */
	/* XXX KDM fix this */
	unit = minor(dev) & 0xff;

	periph = cam_extend_get(passperiphs, unit);

	if (periph == NULL)
		return (ENXIO);

	softc = (struct pass_softc *)periph->softc;

	s = splsoftcam();
	if (softc->flags & PASS_FLAG_INVALID) {
		splx(s);
		return(ENXIO);
	}

	/*
	 * Don't allow access when we're running at a high securelvel.
	 */
	if (securelevel > 1) {
		splx(s);
		return(EPERM);
	}

	/*
	 * Only allow read-write access.
	 */
	if (((flags & FWRITE) == 0) || ((flags & FREAD) == 0)) {
		splx(s);
		return(EPERM);
	}

	/*
	 * We don't allow nonblocking access.
	 */
	if ((flags & O_NONBLOCK) != 0) {
		xpt_print_path(periph->path);
		printf("can't do nonblocking accesss\n");
		splx(s);
		return(EINVAL);
	}

	if ((error = cam_periph_lock(periph, PRIBIO | PCATCH)) != 0) {
		splx(s);
		return (error);
	}

	splx(s);

	if ((softc->flags & PASS_FLAG_OPEN) == 0) {
		if (cam_periph_acquire(periph) != CAM_REQ_CMP)
			return(ENXIO);
		softc->flags |= PASS_FLAG_OPEN;
	}

	cam_periph_unlock(periph);

	return (error);
}

static int
passclose(dev_t dev, int flag, int fmt, struct proc *p)
{
	struct 	cam_periph *periph;
	struct	pass_softc *softc;
	int	unit, error;

	/* unit = dkunit(dev); */
	/* XXX KDM fix this */
	unit = minor(dev) & 0xff;

	periph = cam_extend_get(passperiphs, unit);
	if (periph == NULL)
		return (ENXIO);	

	softc = (struct pass_softc *)periph->softc;

	if ((error = cam_periph_lock(periph, PRIBIO)) != 0)
		return (error);

	softc->flags &= ~PASS_FLAG_OPEN;

	cam_periph_unlock(periph);
	cam_periph_release(periph);

	return (0);
}

/*
 * Actually translate the requested transfer into one the physical driver
 * can understand.  The transfer is described by a buf and will include
 * only one physical transfer.
 */
static void
passstrategy(struct bio *bp)
{
	struct cam_periph *periph;
	struct pass_softc *softc;
	u_int  unit;
	int    s;

	/*
	 * The read/write interface for the passthrough driver doesn't
	 * really work right now.  So, we just pass back EINVAL to tell the
	 * user to go away.
	 */
	bp->bio_error = EINVAL;
	goto bad;

	/* unit = dkunit(bp->bio_dev); */
	/* XXX KDM fix this */
	unit = minor(bp->bio_dev) & 0xff;

	periph = cam_extend_get(passperiphs, unit);
	if (periph == NULL) {
		bp->bio_error = ENXIO;
		goto bad;
	}
	softc = (struct pass_softc *)periph->softc;

	/*
	 * Odd number of bytes or negative offset
	 */
	/* valid request?  */
	if (bp->bio_blkno < 0) {
		bp->bio_error = EINVAL;
		goto bad;
        }
	
	/*
	 * Mask interrupts so that the pack cannot be invalidated until
	 * after we are in the queue.  Otherwise, we might not properly
	 * clean up one of the buffers.
	 */
	s = splbio();
	
	bioq_insert_tail(&softc->bio_queue, bp);

	splx(s);
	
	/*
	 * Schedule ourselves for performing the work.
	 */
	xpt_schedule(periph, /* XXX priority */1);

	return;
bad:
	bp->bio_flags |= BIO_ERROR;

	/*
	 * Correctly set the buf to indicate a completed xfer
	 */
	bp->bio_resid = bp->bio_bcount;
	biodone(bp);
	return;
}

static void
passstart(struct cam_periph *periph, union ccb *start_ccb)
{
	struct pass_softc *softc;
	int s;

	softc = (struct pass_softc *)periph->softc;

	switch (softc->state) {
	case PASS_STATE_NORMAL:
	{
		struct bio *bp;

		s = splbio();
		bp = bioq_first(&softc->bio_queue);
		if (periph->immediate_priority <= periph->pinfo.priority) {
			start_ccb->ccb_h.ccb_type = PASS_CCB_WAITING;			
			SLIST_INSERT_HEAD(&periph->ccb_list, &start_ccb->ccb_h,
					  periph_links.sle);
			periph->immediate_priority = CAM_PRIORITY_NONE;
			splx(s);
			wakeup(&periph->ccb_list);
		} else if (bp == NULL) {
			splx(s);
			xpt_release_ccb(start_ccb);
		} else {

			bioq_remove(&softc->bio_queue, bp);

			devstat_start_transaction(&softc->device_stats);

			/*
			 * XXX JGibbs -
			 * Interpret the contents of the bp as a CCB
			 * and pass it to a routine shared by our ioctl
			 * code and passtart.
			 * For now, just biodone it with EIO so we don't
			 * hang.
			 */
			bp->bio_error = EIO;
			bp->bio_flags |= BIO_ERROR;
			bp->bio_resid = bp->bio_bcount;
			biodone(bp);
			bp = bioq_first(&softc->bio_queue);
			splx(s);
  
			xpt_action(start_ccb);

		}
		if (bp != NULL) {
			/* Have more work to do, so ensure we stay scheduled */
			xpt_schedule(periph, /* XXX priority */1);
		}
		break;
	}
	}
}
static void
passdone(struct cam_periph *periph, union ccb *done_ccb)
{ 
	struct pass_softc *softc;
	struct ccb_scsiio *csio;

	softc = (struct pass_softc *)periph->softc;
	csio = &done_ccb->csio;
	switch (csio->ccb_h.ccb_type) {
	case PASS_CCB_BUFFER_IO:
	{
		struct bio		*bp;
		cam_status		status;
		u_int8_t		scsi_status;
		devstat_trans_flags	ds_flags;

		status = done_ccb->ccb_h.status;
		scsi_status = done_ccb->csio.scsi_status;
		bp = (struct bio *)done_ccb->ccb_h.ccb_bp;
		/* XXX handle errors */
		if (!(((status & CAM_STATUS_MASK) == CAM_REQ_CMP)
		  && (scsi_status == SCSI_STATUS_OK))) {
			int error;
			
			if ((error = passerror(done_ccb, 0, 0)) == ERESTART) {
				/*
				 * A retry was scheuled, so
				 * just return.
				 */
				return;
			}

			/*
			 * XXX unfreeze the queue after we complete
			 * the abort process
			 */
			bp->bio_error = error;
			bp->bio_flags |= BIO_ERROR;
		}

		if ((done_ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
			ds_flags = DEVSTAT_READ;
		else if ((done_ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_OUT)
			ds_flags = DEVSTAT_WRITE;
		else
			ds_flags = DEVSTAT_NO_DATA;

		devstat_end_transaction_bio(&softc->device_stats, bp);
		biodone(bp);
		break;
	}
	case PASS_CCB_WAITING:
	{
		/* Caller will release the CCB */
		wakeup(&done_ccb->ccb_h.cbfcnp);
		return;
	}
	}
	xpt_release_ccb(done_ccb);
}

static int
passioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	struct 	cam_periph *periph;
	struct	pass_softc *softc;
	u_int8_t unit;
	int      error;


	/* unit = dkunit(dev); */
	/* XXX KDM fix this */
	unit = minor(dev) & 0xff;

	periph = cam_extend_get(passperiphs, unit);

	if (periph == NULL)
		return(ENXIO);

	softc = (struct pass_softc *)periph->softc;

	error = 0;

	switch (cmd) {

	case CAMIOCOMMAND:
	{
		union ccb *inccb;
		union ccb *ccb;
		int ccb_malloced;

		inccb = (union ccb *)addr;

		/*
		 * Some CCB types, like scan bus and scan lun can only go
		 * through the transport layer device.
		 */
		if (inccb->ccb_h.func_code & XPT_FC_XPT_ONLY) {
			xpt_print_path(periph->path);
			printf("CCB function code %#x is restricted to the "
			       "XPT device\n", inccb->ccb_h.func_code);
			error = ENODEV;
			break;
		}

		/*
		 * Non-immediate CCBs need a CCB from the per-device pool
		 * of CCBs, which is scheduled by the transport layer.
		 * Immediate CCBs and user-supplied CCBs should just be
		 * malloced.
		 */
		if ((inccb->ccb_h.func_code & XPT_FC_QUEUED)
		 && ((inccb->ccb_h.func_code & XPT_FC_USER_CCB) == 0)) {
			ccb = cam_periph_getccb(periph,
						inccb->ccb_h.pinfo.priority);
			ccb_malloced = 0;
		} else {
			ccb = xpt_alloc_ccb();

			if (ccb != NULL)
				xpt_setup_ccb(&ccb->ccb_h, periph->path,
					      inccb->ccb_h.pinfo.priority);
			ccb_malloced = 1;
		}

		if (ccb == NULL) {
			xpt_print_path(periph->path);
			printf("unable to allocate CCB\n");
			error = ENOMEM;
			break;
		}

		error = passsendccb(periph, ccb, inccb);

		if (ccb_malloced)
			xpt_free_ccb(ccb);
		else
			xpt_release_ccb(ccb);

		break;
	}
	default:
		error = cam_periph_ioctl(periph, cmd, addr, passerror);
		break;
	}

	return(error);
}

/*
 * Generally, "ccb" should be the CCB supplied by the kernel.  "inccb"
 * should be the CCB that is copied in from the user.
 */
static int
passsendccb(struct cam_periph *periph, union ccb *ccb, union ccb *inccb)
{
	struct pass_softc *softc;
	struct cam_periph_map_info mapinfo;
	int error, need_unmap;

	softc = (struct pass_softc *)periph->softc;

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
	ccb->ccb_h.cbfcnp = passdone;

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
	 && (((ccb->ccb_h.func_code == XPT_SCSI_IO)
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
				  passerror : NULL,
				  /* cam_flags */ 0,
				  /* sense_flags */SF_RETRY_UA | SF_RETRY_SELTO,
				  &softc->device_stats);

	if (need_unmap != 0)
		cam_periph_unmapmem(ccb, &mapinfo);

	ccb->ccb_h.cbfcnp = NULL;
	ccb->ccb_h.periph_priv = inccb->ccb_h.periph_priv;
	bcopy(ccb, inccb, sizeof(union ccb));

	return(error);
}

static int
passerror(union ccb *ccb, u_int32_t cam_flags, u_int32_t sense_flags)
{
	struct cam_periph *periph;
	struct pass_softc *softc;

	periph = xpt_path_periph(ccb->ccb_h.path);
	softc = (struct pass_softc *)periph->softc;
	
	return(cam_periph_error(ccb, cam_flags, sense_flags, 
				 &softc->saved_ccb));
}
