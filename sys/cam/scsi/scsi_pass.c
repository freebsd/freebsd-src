/*-
 * Copyright (c) 1997, 1998, 2000 Justin T. Gibbs.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/bio.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/devicestat.h>
#include <sys/proc.h>
#include <sys/taskqueue.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_periph.h>
#include <cam/cam_queue.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_debug.h>
#include <cam/cam_sim.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_pass.h>

typedef enum {
	PASS_FLAG_OPEN			= 0x01,
	PASS_FLAG_LOCKED		= 0x02,
	PASS_FLAG_INVALID		= 0x04,
	PASS_FLAG_INITIAL_PHYSPATH	= 0x08
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
	pass_state	 state;
	pass_flags	 flags;
	u_int8_t	 pd_type;
	union ccb	 saved_ccb;
	int		 open_count;
	struct devstat	*device_stats;
	struct cdev	*dev;
	struct cdev	*alias_dev;
	struct task	 add_physpath_task;
};


static	d_open_t	passopen;
static	d_close_t	passclose;
static	d_ioctl_t	passioctl;

static	periph_init_t	passinit;
static	periph_ctor_t	passregister;
static	periph_oninv_t	passoninvalidate;
static	periph_dtor_t	passcleanup;
static	periph_start_t	passstart;
static void		pass_add_physpath(void *context, int pending);
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

PERIPHDRIVER_DECLARE(pass, passdriver);

static struct cdevsw pass_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_TRACKCLOSE,
	.d_open =	passopen,
	.d_close =	passclose,
	.d_ioctl =	passioctl,
	.d_name =	"pass",
};

static void
passinit(void)
{
	cam_status status;

	/*
	 * Install a global async callback.  This callback will
	 * receive async callbacks like "new device found".
	 */
	status = xpt_register_async(AC_FOUND_DEVICE, passasync, NULL, NULL);

	if (status != CAM_REQ_CMP) {
		printf("pass: Failed to attach master async callback "
		       "due to status 0x%x!\n", status);
	}

}

static void
passdevgonecb(void *arg)
{
	struct cam_sim    *sim;
	struct cam_periph *periph;
	struct pass_softc *softc;
	int i;

	periph = (struct cam_periph *)arg;
	sim = periph->sim;
	softc = (struct pass_softc *)periph->softc;

	KASSERT(softc->open_count >= 0, ("Negative open count %d",
		softc->open_count));

	mtx_lock(sim->mtx);

	/*
	 * When we get this callback, we will get no more close calls from
	 * devfs.  So if we have any dangling opens, we need to release the
	 * reference held for that particular context.
	 */
	for (i = 0; i < softc->open_count; i++)
		cam_periph_release_locked(periph);

	softc->open_count = 0;

	/*
	 * Release the reference held for the device node, it is gone now.
	 */
	cam_periph_release_locked(periph);

	/*
	 * We reference the SIM lock directly here, instead of using
	 * cam_periph_unlock().  The reason is that the final call to
	 * cam_periph_release_locked() above could result in the periph
	 * getting freed.  If that is the case, dereferencing the periph
	 * with a cam_periph_unlock() call would cause a page fault.
	 */
	mtx_unlock(sim->mtx);
}

static void
passoninvalidate(struct cam_periph *periph)
{
	struct pass_softc *softc;

	softc = (struct pass_softc *)periph->softc;

	/*
	 * De-register any async callbacks.
	 */
	xpt_register_async(0, passasync, periph, periph->path);

	softc->flags |= PASS_FLAG_INVALID;

	/*
	 * Tell devfs this device has gone away, and ask for a callback
	 * when it has cleaned up its state.
	 */
	destroy_dev_sched_cb(softc->dev, passdevgonecb, periph);

	/*
	 * XXX Return all queued I/O with ENXIO.
	 * XXX Handle any transactions queued to the card
	 *     with XPT_ABORT_CCB.
	 */

	if (bootverbose) {
		xpt_print(periph->path, "lost device\n");
	}

}

static void
passcleanup(struct cam_periph *periph)
{
	struct pass_softc *softc;

	softc = (struct pass_softc *)periph->softc;

	if (bootverbose)
		xpt_print(periph->path, "removing device entry\n");
	devstat_remove_entry(softc->device_stats);

	cam_periph_unlock(periph);
	taskqueue_drain(taskqueue_thread, &softc->add_physpath_task);

	cam_periph_lock(periph);

	free(softc, M_DEVBUF);
}

static void
pass_add_physpath(void *context, int pending)
{
	struct cam_periph *periph;
	struct pass_softc *softc;
	char *physpath;

	/*
	 * If we have one, create a devfs alias for our
	 * physical path.
	 */
	periph = context;
	softc = periph->softc;
	physpath = malloc(MAXPATHLEN, M_DEVBUF, M_WAITOK);
	cam_periph_lock(periph);
	if (periph->flags & CAM_PERIPH_INVALID) {
		cam_periph_unlock(periph);
		goto out;
	}
	if (xpt_getattr(physpath, MAXPATHLEN,
			"GEOM::physpath", periph->path) == 0
	 && strlen(physpath) != 0) {

		cam_periph_unlock(periph);
		make_dev_physpath_alias(MAKEDEV_WAITOK, &softc->alias_dev,
					softc->dev, softc->alias_dev, physpath);
		cam_periph_lock(periph);
	}

	/*
	 * Now that we've made our alias, we no longer have to have a
	 * reference to the device.
	 */
	if ((softc->flags & PASS_FLAG_INITIAL_PHYSPATH) == 0) {
		softc->flags |= PASS_FLAG_INITIAL_PHYSPATH;
		cam_periph_unlock(periph);
		dev_rel(softc->dev);
	}
	else
		cam_periph_unlock(periph);

out:
	free(physpath, M_DEVBUF);
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
		if (cgd == NULL)
			break;

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
		 && status != CAM_REQ_INPROG) {
			const struct cam_status_entry *entry;

			entry = cam_fetch_status_entry(status);

			printf("passasync: Unable to attach new device "
			       "due to status %#x: %s\n", status, entry ?
			       entry->status_text : "Unknown");
		}

		break;
	}
	case AC_ADVINFO_CHANGED:
	{
		uintptr_t buftype;

		buftype = (uintptr_t)arg;
		if (buftype == CDAI_TYPE_PHYS_PATH) {
			struct pass_softc *softc;

			softc = (struct pass_softc *)periph->softc;
			taskqueue_enqueue(taskqueue_thread,
					  &softc->add_physpath_task);
		}
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
	struct ccb_getdev *cgd;
	struct ccb_pathinq cpi;
	int    no_tags;

	cgd = (struct ccb_getdev *)arg;
	if (cgd == NULL) {
		printf("%s: no getdev CCB, can't register device\n", __func__);
		return(CAM_REQ_CMP_ERR);
	}

	softc = (struct pass_softc *)malloc(sizeof(*softc),
					    M_DEVBUF, M_NOWAIT);

	if (softc == NULL) {
		printf("%s: Unable to probe new device. "
		       "Unable to allocate softc\n", __func__);
		return(CAM_REQ_CMP_ERR);
	}

	bzero(softc, sizeof(*softc));
	softc->state = PASS_STATE_NORMAL;
	if (cgd->protocol == PROTO_SCSI || cgd->protocol == PROTO_ATAPI)
		softc->pd_type = SID_TYPE(&cgd->inq_data);
	else if (cgd->protocol == PROTO_SATAPM)
		softc->pd_type = T_ENCLOSURE;
	else
		softc->pd_type = T_DIRECT;

	periph->softc = softc;

	bzero(&cpi, sizeof(cpi));
	xpt_setup_ccb(&cpi.ccb_h, periph->path, CAM_PRIORITY_NORMAL);
	cpi.ccb_h.func_code = XPT_PATH_INQ;
	xpt_action((union ccb *)&cpi);

	/*
	 * We pass in 0 for a blocksize, since we don't 
	 * know what the blocksize of this device is, if 
	 * it even has a blocksize.
	 */
	cam_periph_unlock(periph);
	no_tags = (cgd->inq_data.flags & SID_CmdQue) == 0;
	softc->device_stats = devstat_new_entry("pass",
			  periph->unit_number, 0,
			  DEVSTAT_NO_BLOCKSIZE
			  | (no_tags ? DEVSTAT_NO_ORDERED_TAGS : 0),
			  softc->pd_type |
			  XPORT_DEVSTAT_TYPE(cpi.transport) |
			  DEVSTAT_TYPE_PASS,
			  DEVSTAT_PRIORITY_PASS);

	/*
	 * Acquire a reference to the periph before we create the devfs
	 * instance for it.  We'll release this reference once the devfs
	 * instance has been freed.
	 */
	if (cam_periph_acquire(periph) != CAM_REQ_CMP) {
		xpt_print(periph->path, "%s: lost periph during "
			  "registration!\n", __func__);
		cam_periph_lock(periph);
		return (CAM_REQ_CMP_ERR);
	}

	/* Register the device */
	softc->dev = make_dev(&pass_cdevsw, periph->unit_number,
			      UID_ROOT, GID_OPERATOR, 0600, "%s%d",
			      periph->periph_name, periph->unit_number);

	/*
	 * Now that we have made the devfs instance, hold a reference to it
	 * until the task queue has run to setup the physical path alias.
	 * That way devfs won't get rid of the device before we add our
	 * alias.
	 */
	dev_ref(softc->dev);

	cam_periph_lock(periph);
	softc->dev->si_drv1 = periph;

	TASK_INIT(&softc->add_physpath_task, /*priority*/0,
		  pass_add_physpath, periph);

	/*
	 * See if physical path information is already available.
	 */
	taskqueue_enqueue(taskqueue_thread, &softc->add_physpath_task);

	/*
	 * Add an async callback so that we get notified if
	 * this device goes away or its physical path
	 * (stored in the advanced info data of the EDT) has
	 * changed.
	 */
	xpt_register_async(AC_LOST_DEVICE | AC_ADVINFO_CHANGED,
			   passasync, periph, periph->path);

	if (bootverbose)
		xpt_announce_periph(periph, NULL);

	return(CAM_REQ_CMP);
}

static int
passopen(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct cam_periph *periph;
	struct pass_softc *softc;
	int error;

	periph = (struct cam_periph *)dev->si_drv1;
	if (cam_periph_acquire(periph) != CAM_REQ_CMP)
		return (ENXIO);

	cam_periph_lock(periph);

	softc = (struct pass_softc *)periph->softc;

	if (softc->flags & PASS_FLAG_INVALID) {
		cam_periph_release_locked(periph);
		cam_periph_unlock(periph);
		return(ENXIO);
	}

	/*
	 * Don't allow access when we're running at a high securelevel.
	 */
	error = securelevel_gt(td->td_ucred, 1);
	if (error) {
		cam_periph_release_locked(periph);
		cam_periph_unlock(periph);
		return(error);
	}

	/*
	 * Only allow read-write access.
	 */
	if (((flags & FWRITE) == 0) || ((flags & FREAD) == 0)) {
		cam_periph_release_locked(periph);
		cam_periph_unlock(periph);
		return(EPERM);
	}

	/*
	 * We don't allow nonblocking access.
	 */
	if ((flags & O_NONBLOCK) != 0) {
		xpt_print(periph->path, "can't do nonblocking access\n");
		cam_periph_release_locked(periph);
		cam_periph_unlock(periph);
		return(EINVAL);
	}

	softc->open_count++;

	cam_periph_unlock(periph);

	return (error);
}

static int
passclose(struct cdev *dev, int flag, int fmt, struct thread *td)
{
	struct  cam_sim    *sim;
	struct 	cam_periph *periph;
	struct  pass_softc *softc;

	periph = (struct cam_periph *)dev->si_drv1;
	if (periph == NULL)
		return (ENXIO);	

	sim = periph->sim;
	softc = periph->softc;

	mtx_lock(sim->mtx);

	softc->open_count--;

	cam_periph_release_locked(periph);

	/*
	 * We reference the SIM lock directly here, instead of using
	 * cam_periph_unlock().  The reason is that the call to
	 * cam_periph_release_locked() above could result in the periph
	 * getting freed.  If that is the case, dereferencing the periph
	 * with a cam_periph_unlock() call would cause a page fault.
	 *
	 * cam_periph_release() avoids this problem using the same method,
	 * but we're manually acquiring and dropping the lock here to
	 * protect the open count and avoid another lock acquisition and
	 * release.
	 */
	mtx_unlock(sim->mtx);

	return (0);
}

static void
passstart(struct cam_periph *periph, union ccb *start_ccb)
{
	struct pass_softc *softc;

	softc = (struct pass_softc *)periph->softc;

	switch (softc->state) {
	case PASS_STATE_NORMAL:
		start_ccb->ccb_h.ccb_type = PASS_CCB_WAITING;			
		SLIST_INSERT_HEAD(&periph->ccb_list, &start_ccb->ccb_h,
				  periph_links.sle);
		periph->immediate_priority = CAM_PRIORITY_NONE;
		wakeup(&periph->ccb_list);
		break;
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
	case PASS_CCB_WAITING:
		/* Caller will release the CCB */
		wakeup(&done_ccb->ccb_h.cbfcnp);
		return;
	}
	xpt_release_ccb(done_ccb);
}

static int
passioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag, struct thread *td)
{
	struct	cam_periph *periph;
	struct	pass_softc *softc;
	int	error;
	uint32_t priority;

	periph = (struct cam_periph *)dev->si_drv1;
	if (periph == NULL)
		return(ENXIO);

	cam_periph_lock(periph);
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
			xpt_print(periph->path, "CCB function code %#x is "
			    "restricted to the XPT device\n",
			    inccb->ccb_h.func_code);
			error = ENODEV;
			break;
		}

		/* Compatibility for RL/priority-unaware code. */
		priority = inccb->ccb_h.pinfo.priority;
		if (priority < CAM_RL_TO_PRIORITY(CAM_RL_NORMAL))
		    priority += CAM_RL_TO_PRIORITY(CAM_RL_NORMAL);

		/*
		 * Non-immediate CCBs need a CCB from the per-device pool
		 * of CCBs, which is scheduled by the transport layer.
		 * Immediate CCBs and user-supplied CCBs should just be
		 * malloced.
		 */
		if ((inccb->ccb_h.func_code & XPT_FC_QUEUED)
		 && ((inccb->ccb_h.func_code & XPT_FC_USER_CCB) == 0)) {
			ccb = cam_periph_getccb(periph, priority);
			ccb_malloced = 0;
		} else {
			ccb = xpt_alloc_ccb_nowait();

			if (ccb != NULL)
				xpt_setup_ccb(&ccb->ccb_h, periph->path,
					      priority);
			ccb_malloced = 1;
		}

		if (ccb == NULL) {
			xpt_print(periph->path, "unable to allocate CCB\n");
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

	cam_periph_unlock(periph);
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
	 * cam_periph_mapmem() supports SCSI, ATA, SMP, ADVINFO and device
	 * match CCBs.  For the SCSI, ATA and ADVINFO CCBs, we only pass the
	 * CCB in if there's actually data to map.  cam_periph_mapmem() will
	 * do the right thing, even if there isn't data to map, but since CCBs
	 * without data are a reasonably common occurance (e.g. test unit
	 * ready), it will save a few cycles if we check for it here.
	 *
	 * XXX What happens if a sg list is supplied?  We don't filter that
	 * out.
	 */
	if (((ccb->ccb_h.flags & CAM_DATA_MASK) == CAM_DATA_VADDR)
	 && (((ccb->ccb_h.func_code == XPT_SCSI_IO ||
	       ccb->ccb_h.func_code == XPT_ATA_IO)
	    && ((ccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE))
	  || (ccb->ccb_h.func_code == XPT_DEV_MATCH)
	  || (ccb->ccb_h.func_code == XPT_SMP_IO)
	  || ((ccb->ccb_h.func_code == XPT_DEV_ADVINFO)
	   && (ccb->cdai.bufsiz > 0)))) {

		bzero(&mapinfo, sizeof(mapinfo));

		/*
		 * cam_periph_mapmem calls into proc and vm functions that can
		 * sleep as well as trigger I/O, so we can't hold the lock.
		 * Dropping it here is reasonably safe.
		 */
		cam_periph_unlock(periph);
		error = cam_periph_mapmem(ccb, &mapinfo); 
		cam_periph_lock(periph);

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
	cam_periph_runccb(ccb, passerror, /* cam_flags */ CAM_RETRY_SELTO,
	    /* sense_flags */ ((ccb->ccb_h.flags & CAM_PASS_ERR_RECOVER) ?
	     SF_RETRY_UA : SF_NO_RECOVERY) | SF_NO_PRINT,
	    softc->device_stats);

	if (need_unmap != 0)
		cam_periph_unmapmem(ccb, &mapinfo);

	ccb->ccb_h.cbfcnp = NULL;
	ccb->ccb_h.periph_priv = inccb->ccb_h.periph_priv;
	bcopy(ccb, inccb, sizeof(union ccb));

	return(0);
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
