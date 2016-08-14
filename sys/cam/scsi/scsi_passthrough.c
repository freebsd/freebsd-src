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
#ifndef CTL_PASS
#define CTL_PASS
 
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_kdtrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/types.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/devicestat.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/sdt.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <machine/bus.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_periph.h>
#include <cam/cam_queue.h>
#include <cam/cam_xpt.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_debug.h>
#include <cam/cam_compat.h>
#include <cam/cam_xpt_periph.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_passthrough.h>
#include <cam/scsi/scsi_message.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl_backend_passthrough.h>
#include <cam/ctl/ctl.h>
//#include <cam/ctl/ctl_error.c>
#endif
typedef enum {
	PASS_FLAG_OPEN			= 0x01,
	PASS_FLAG_LOCKED		= 0x02,
	PASS_FLAG_INVALID		= 0x04,
	PASS_FLAG_INITIAL_PHYSPATH	= 0x08,
	PASS_FLAG_ZONE_INPROG		= 0x10,
	PASS_FLAG_ZONE_VALID		= 0x20,
	PASS_FLAG_UNMAPPED_CAPABLE	= 0x40,
	PASS_FLAG_ABANDONED_REF_SET	= 0x80
} pass_flags;

typedef enum {
	CTLPASS_STATE_NORMAL
} pass_state;

typedef enum {
	PASS_CCB_BUFFER_IO,
	PASS_CCB_QUEUED_IO
} pass_ccb_types;

#define ccb_type	ppriv_field0
#define ccb_ioreq	ppriv_ptr1
#define ctl_ioreq          ppriv_ptr0
/*
 * The maximum number of memory segments we preallocate.
 */
#define	PASS_MAX_SEGS	16

typedef enum {
	PASS_IO_NONE		= 0x00,
	PASS_IO_USER_SEG_MALLOC	= 0x01,
	PASS_IO_KERN_SEG_MALLOC	= 0x02,
	PASS_IO_ABANDONED	= 0x04
} pass_io_flags; 

struct ctlpass_io_req {
	union ccb			 ccb;
	union ccb			 *alloced_ccb;
	camq_entry			 user_periph_links;
	ccb_ppriv_area			 user_periph_priv;
	struct cam_periph_map_info	 mapinfo;
	pass_io_flags			 flags;
	ccb_flags			 data_flags;
	bus_dma_segment_t		 user_segs[PASS_MAX_SEGS];
	int				 num_kern_segs;
	bus_dma_segment_t		 kern_segs[PASS_MAX_SEGS];
	bus_dma_segment_t		*user_segptr;
	bus_dma_segment_t		*kern_segptr;
	int				 num_bufs;
	uint32_t			 dirs[CAM_PERIPH_MAXMAPS];
	uint32_t			 lengths[CAM_PERIPH_MAXMAPS];
	uint8_t				*user_bufs[CAM_PERIPH_MAXMAPS];
	uint8_t				*kern_bufs[CAM_PERIPH_MAXMAPS];
	struct bintime			 start_time;
	TAILQ_ENTRY(ctlpass_io_req)	 links;
};

struct ctlpass_softc {
	pass_state		  state;
	pass_flags		  flags;
	u_int8_t		  pd_type;
	union ccb		  saved_ccb;
	int			  open_count;
	u_int		 	  maxio;
	struct devstat		 *device_stats;
	struct cdev		 *dev;
	struct cdev		 *alias_dev;
	struct task		  add_physpath_task;
	struct task		  shutdown_kqueue_task;
	struct selinfo		  read_select;
	TAILQ_HEAD(, ctlpass_io_req) incoming_queue;
	TAILQ_HEAD(, ctlpass_io_req) active_queue;
	TAILQ_HEAD(, ctlpass_io_req) abandoned_queue;
	TAILQ_HEAD(, ctlpass_io_req) done_queue;
	struct cam_periph	 *periph;
	char			  zone_name[12];
	char			  io_zone_name[12];
	uma_zone_t		  pass_zone;
	uma_zone_t		  pass_io_zone;
	size_t			  io_zone_size;
};
static	d_open_t	ctlpassopen;
static	d_close_t	ctlpassclose;

static	periph_init_t	ctlpassinit;
static	periph_ctor_t	ctlpassregister;
static	periph_oninv_t	ctlpassoninvalidate;
static	periph_dtor_t	ctlpasscleanup;
static	periph_start_t	ctlpassstart;
static	void		pass_shutdown_kqueue(void *context, int pending);
static	void		ctlpass_add_physpath(void *context, int pending);
static	void		ctlpassasync(void *callback_arg, u_int32_t code,
				  struct cam_path *path, void *arg);
static	void		ctlpassdone(struct cam_periph *periph, 
				 union ccb *done_ccb);
static	int		ctlpasscreatezone(struct cam_periph *periph);
static	void		ctlpassiocleanup(struct ctlpass_softc *softc, 
				      struct ctlpass_io_req *io_req);
static	int		ctlpasserror(union ccb *ccb, u_int32_t cam_flags, 
				  u_int32_t sense_flags);

static int ctlccb(struct cam_periph *periph,union ctl_io *ccb);
static int ctlstrcmp(const char *a,const char *b);

static struct periph_driver ctlpassdriver =
{
	ctlpassinit, "ctlpass",
	TAILQ_HEAD_INITIALIZER(ctlpassdriver.units), /* generation */ 0
};

PERIPHDRIVER_DECLARE(ctlpass, ctlpassdriver);

static struct cdevsw ctlpass_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_TRACKCLOSE,
	.d_open =	ctlpassopen,
	.d_close =	ctlpassclose,
        .d_name =	"ctlpass",
};


static MALLOC_DEFINE(M_SCSIPASSTHROUGH, "ctl_pass", "ctl passthrough buffers");

static void
ctlpassinit(void)
{
	cam_status status;

	/*
	 * Install a global async callback.  This callback will
	 * receive async callbacks like "new device found".
	 */
	status = xpt_register_async(AC_FOUND_DEVICE, ctlpassasync, NULL, NULL);

	if (status != CAM_REQ_CMP) {
		printf("pass: Failed to attach master async callback "
		       "due to status 0x%x!\n", status);
	}

}

static void
ctlpassrejectios(struct cam_periph *periph)
{
	struct ctlpass_io_req *io_req, *io_req2;
	struct ctlpass_softc *softc;

	softc = (struct ctlpass_softc *)periph->softc;

	/*
	 * The user can no longer get status for I/O on the done queue, so
	 * clean up all outstanding I/O on the done queue.
	 */
	TAILQ_FOREACH_SAFE(io_req, &softc->done_queue, links, io_req2) {
		TAILQ_REMOVE(&softc->done_queue, io_req, links);
		ctlpassiocleanup(softc, io_req);
		uma_zfree(softc->pass_zone, io_req);
	}

	/*
	 * The underlying device is gone, so we can't issue these I/Os.
	 * The devfs node has been shut down, so we can't return status to
	 * the user.  Free any I/O left on the incoming queue.
	 */
	TAILQ_FOREACH_SAFE(io_req, &softc->incoming_queue, links, io_req2) {
		TAILQ_REMOVE(&softc->incoming_queue, io_req, links);
		ctlpassiocleanup(softc, io_req);
		uma_zfree(softc->pass_zone, io_req);
	}

	/*
	 * Normally we would put I/Os on the abandoned queue and acquire a
	 * reference when we saw the final close.  But, the device went
	 * away and devfs may have moved everything off to deadfs by the
	 * time the I/O done callback is called; as a result, we won't see
	 * any more closes.  So, if we have any active I/Os, we need to put
	 * them on the abandoned queue.  When the abandoned queue is empty,
	 * we'll release the remaining reference (see below) to the peripheral.
	 */
	TAILQ_FOREACH_SAFE(io_req, &softc->active_queue, links, io_req2) {
		TAILQ_REMOVE(&softc->active_queue, io_req, links);
		io_req->flags |= PASS_IO_ABANDONED;
		TAILQ_INSERT_TAIL(&softc->abandoned_queue, io_req, links);
	}

	/*
	 * If we put any I/O on the abandoned queue, acquire a reference.
	 */
	if ((!TAILQ_EMPTY(&softc->abandoned_queue))
	 && ((softc->flags & PASS_FLAG_ABANDONED_REF_SET) == 0)) {
		cam_periph_doacquire(periph);
		softc->flags |= PASS_FLAG_ABANDONED_REF_SET;
	}
}

static void
ctlpassdevgonecb(void *arg)
{
	struct cam_periph *periph;
	struct mtx *mtx;
	struct ctlpass_softc *softc;
	//int i;

	periph = (struct cam_periph *)arg;
	mtx = cam_periph_mtx(periph);
	mtx_lock(mtx);

	softc = (struct ctlpass_softc *)periph->softc;
	KASSERT(softc->open_count >= 0, ("Negative open count %d",
		softc->open_count));

	/*
	 * When we get this callback, we will get no more close calls from
	 * devfs.  So if we have any dangling opens, we need to release the
	 * reference held for that particular context.
	 */
	//for (i = 0; i < softc->open_count; i++)
		cam_periph_release_locked(periph);

	softc->open_count = 0;

	/*
	 * Release the reference held for the device node, it is gone now.
	 * Accordingly, inform all queued I/Os of their fate.
	 */
	cam_periph_release_locked(periph);
	ctlpassrejectios(periph);

	/*
	 * We reference the SIM lock directly here, instead of using
	 * cam_periph_unlock().  The reason is that the final call to
	 * cam_periph_release_locked() above could result in the periph
	 * getting freed.  If that is the case, dereferencing the periph
	 * with a cam_periph_unlock() call would cause a page fault.
	 */
	mtx_unlock(mtx);

}

static void
ctlpassoninvalidate(struct cam_periph *periph)
{
	struct ctlpass_softc *softc;

	softc = (struct ctlpass_softc *)periph->softc;

	/*
	 * De-register any async callbacks.
	 */
	xpt_register_async(0, ctlpassasync, periph, periph->path);

	softc->flags |= PASS_FLAG_INVALID;

	/*
	 * Tell devfs this device has gone away, and ask for a callback
	 * when it has cleaned up its state.
	 */
	destroy_dev_sched_cb(softc->dev, ctlpassdevgonecb, periph);
}

static void
ctlpasscleanup(struct cam_periph *periph)
{
	struct ctlpass_softc *softc;

	softc = (struct ctlpass_softc *)periph->softc;

	cam_periph_assert(periph, MA_OWNED);
	KASSERT(TAILQ_EMPTY(&softc->active_queue),
		("%s called when there are commands on the active queue!\n",
		__func__));
	KASSERT(TAILQ_EMPTY(&softc->abandoned_queue),
		("%s called when there are commands on the abandoned queue!\n",
		__func__));
	KASSERT(TAILQ_EMPTY(&softc->incoming_queue),
		("%s called when there are commands on the incoming queue!\n",
		__func__));
	KASSERT(TAILQ_EMPTY(&softc->done_queue),
		("%s called when there are commands on the done queue!\n",
		__func__));

	devstat_remove_entry(softc->device_stats);

	cam_periph_unlock(periph);

	/*
	 * We call taskqueue_drain() for the physpath task to make sure it
	 * is complete.  We drop the lock because this can potentially
	 * sleep.  XXX KDM that is bad.  Need a way to get a callback when
	 * a taskqueue is drained.
	 *
 	 * Note that we don't drain the kqueue shutdown task queue.  This
	 * is because we hold a reference on the periph for kqueue, and
	 * release that reference from the kqueue shutdown task queue.  So
	 * we cannot come into this routine unless we've released that
	 * reference.  Also, because that could be the last reference, we
	 * could be called from the cam_periph_release() call in
	 * pass_shutdown_kqueue().  In that case, the taskqueue_drain()
	 * would deadlock.  It would be preferable if we had a way to
	 * get a callback when a taskqueue is done.
	 */
	taskqueue_drain(taskqueue_thread, &softc->add_physpath_task);

	cam_periph_lock(periph);

	free(softc, M_DEVBUF);
}

static void
pass_shutdown_kqueue(void *context, int pending)
{
	struct cam_periph *periph;
	struct ctlpass_softc *softc;

	periph = context;
	softc = periph->softc;

	knlist_clear(&softc->read_select.si_note, /*is_locked*/ 0);
	knlist_destroy(&softc->read_select.si_note);

	/*
	 * Release the reference we held for kqueue.
	 */
	cam_periph_release(periph);
}

static void
ctlpass_add_physpath(void *context, int pending)
{
	struct cam_periph *periph;
	struct ctlpass_softc *softc;
	struct mtx *mtx;
	char *physpath;

	/*
	 * If we have one, create a devfs alias for our
	 * physical path.
	 */
	periph = context;
	softc = periph->softc;
	physpath = malloc(MAXPATHLEN, M_DEVBUF, M_WAITOK);
	mtx = cam_periph_mtx(periph);
	mtx_lock(mtx);

	if (periph->flags & CAM_PERIPH_INVALID)
		goto out;

	if (xpt_getattr(physpath, MAXPATHLEN,
			"GEOM::physpath", periph->path) == 0
	 && strlen(physpath) != 0) {

		mtx_unlock(mtx);
		make_dev_physpath_alias(MAKEDEV_WAITOK, &softc->alias_dev,
					softc->dev, softc->alias_dev, physpath);
		mtx_lock(mtx);
	}

out:
	/*
	 * Now that we've made our alias, we no longer have to have a
	 * reference to the device.
	 */
	if ((softc->flags & PASS_FLAG_INITIAL_PHYSPATH) == 0)
		softc->flags |= PASS_FLAG_INITIAL_PHYSPATH;

	/*
	 * We always acquire a reference to the periph before queueing this
	 * task queue function, so it won't go away before we run.
	 */
	while (pending-- > 0)
		cam_periph_release_locked(periph);
	mtx_unlock(mtx);

	free(physpath, M_DEVBUF);
}

static void
ctlpassasync(void *callback_arg, u_int32_t code,
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
		if(cgd->protocol != PROTO_SCSI)
			break;
		/*
		 * Allocate a peripheral instance for
		 * this device and start the probe
		 * process.
		 */
		status = cam_periph_alloc(ctlpassregister, ctlpassoninvalidate,
					  ctlpasscleanup, ctlpassstart, "ctlpass",
					  CAM_PERIPH_BIO, path,
					  ctlpassasync, AC_FOUND_DEVICE, cgd);

		if (status != CAM_REQ_CMP
		 && status != CAM_REQ_INPROG) {
			const struct cam_status_entry *entry;

			entry = cam_fetch_status_entry(status);

			printf("ctlpassasync: Unable to attach new device "
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
			struct ctlpass_softc *softc;
			cam_status status;

			softc = (struct ctlpass_softc *)periph->softc;
			/*
			 * Acquire a reference to the periph before we
			 * start the taskqueue, so that we don't run into
			 * a situation where the periph goes away before
			 * the task queue has a chance to run.
			 */
			status = cam_periph_acquire(periph);
			if (status != CAM_REQ_CMP)
				break;

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
ctlpassregister(struct cam_periph *periph, void *arg)
{
	struct ctlpass_softc *softc;
	struct ccb_getdev *cgd;
	struct ccb_pathinq cpi;
	struct make_dev_args args;
	int error;// no_tags;
	const char *str ="camsim";
	const char *str1; 
	struct cam_sim *sim;

	/*Not registering devices from camsim */
	sim = periph->sim;
	str1= cam_sim_name(sim);
	if(ctlstrcmp(str1, str)==0)
	{
		return(CAM_REQ_CMP_ERR);
		
	}
	

	cgd = (struct ccb_getdev *)arg;
	if (cgd == NULL) {
		printf("%s: no getdev CCB, can't register device\n", __func__);
		return(CAM_REQ_CMP_ERR);
	}

	softc = (struct ctlpass_softc *)malloc(sizeof(*softc),
					    M_DEVBUF, M_NOWAIT);

	if (softc == NULL) {
		printf("%s: Unable to probe new device. "
		       "Unable to allocate softc\n", __func__);
		return(CAM_REQ_CMP_ERR);
	}

	bzero(softc, sizeof(*softc));
	softc->state = CTLPASS_STATE_NORMAL;
	if (cgd->protocol == PROTO_SCSI)
		softc->pd_type = SID_TYPE(&cgd->inq_data);


	periph->softc = softc;
	softc->periph = periph;
	TAILQ_INIT(&softc->incoming_queue);
	TAILQ_INIT(&softc->active_queue);
	TAILQ_INIT(&softc->abandoned_queue);
	TAILQ_INIT(&softc->done_queue);
	snprintf(softc->zone_name, sizeof(softc->zone_name), "%s%d",
		 periph->periph_name, periph->unit_number);
	snprintf(softc->io_zone_name, sizeof(softc->io_zone_name), "%s%dIO",
		 periph->periph_name, periph->unit_number);
	softc->io_zone_size = MAXPHYS;
	knlist_init_mtx(&softc->read_select.si_note, cam_periph_mtx(periph));

	bzero(&cpi, sizeof(cpi));
	xpt_setup_ccb(&cpi.ccb_h, periph->path, CAM_PRIORITY_NORMAL);
	cpi.ccb_h.func_code = XPT_PATH_INQ;
	xpt_action((union ccb *)&cpi);

//	if (cpi.maxio == 0)
//		softc->maxio = DFLTPHYS;	/* traditional default */
//	else if (cpi.maxio > MAXPHYS)
//		softc->maxio = MAXPHYS;		/* for safety */
//	else
//		softc->maxio = cpi.maxio;	/* real value */
/*
	if (cpi.hba_misc & PIM_UNMAPPED)
		softc->flags |= PASS_FLAG_UNMAPPED_CAPABLE;
*/
	/*
	 * We pass in 0 for a blocksize, since we don't 
	 * know what the blocksize of this device is, if 
	 * it even has a blocksize.
	 */
	cam_periph_unlock(periph);
	/*no_tags = (cgd->inq_data.flags & SID_CmdQue) == 0;
	softc->device_stats = devstat_new_entry("ctlpass",
			  periph->unit_number, 0,
			  DEVSTAT_NO_BLOCKSIZE
			  | (no_tags ? DEVSTAT_NO_ORDERED_TAGS : 0),
			  softc->pd_type |
			  XPORT_DEVSTAT_TYPE(cpi.transport) |
			  DEVSTAT_TYPE_PASS,
			  DEVSTAT_PRIORITY_PASS);
*/
	if((cpi.ccb_h.status & CAM_DEV_QFRZN) != 0)
		cam_release_devq(cpi.ccb_h.path, 0 ,0 ,0, 0);
	/*
	 * Acquire a reference to the periph that we can release once we've
	 * cleaned up the kqueue.
	 */
	if (cam_periph_acquire(periph) != CAM_REQ_CMP) {
		xpt_print(periph->path, "%s: lost periph during "
			  "registration!\n", __func__);
		//cam_periph_lock(periph);
		return (CAM_REQ_CMP_ERR);
	}

	/*
	 * Acquire a reference to the periph before we create the devfs
	 * instance for it.  We'll release this reference once the devfs
	 * instance has been freed.
	 */
	if (cam_periph_acquire(periph) != CAM_REQ_CMP) {
		xpt_print(periph->path, "%s: lost periph during "
			  "registration!\n", __func__);
	//	cam_periph_lock(periph);
		return (CAM_REQ_CMP_ERR);
	}

	/* Register the device */
	make_dev_args_init(&args);
	args.mda_devsw = &ctlpass_cdevsw;
	args.mda_unit = periph->unit_number;
	args.mda_uid = UID_ROOT;
	args.mda_gid = GID_OPERATOR;
	args.mda_mode = 0600;
	args.mda_si_drv1 = periph;
	error = make_dev_s(&args, &softc->dev, "%s%d", periph->periph_name,
	    periph->unit_number);
	if (error != 0) {
		cam_periph_lock(periph);
		cam_periph_release_locked(periph);
		return (CAM_REQ_CMP_ERR);
	}

	/*
	 * Hold a reference to the periph before we create the physical
	 * path alias so it can't go away.
	 */
	if (cam_periph_acquire(periph) != CAM_REQ_CMP) {
		xpt_print(periph->path, "%s: lost periph during "
			  "registration!\n", __func__);
		cam_periph_lock(periph);
		return (CAM_REQ_CMP_ERR);
	}

///	error = ctl_backend_passthrough_create(periph);

	if(error!=0)
	{
		printf("Failed to create lun for this ctlpass device");
		return (CAM_REQ_CMP_ERR);

	}
	cam_periph_lock(periph);

	TASK_INIT(&softc->add_physpath_task, /*priority*/0,
		  ctlpass_add_physpath, periph);

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
			  ctlpassasync, periph, periph->path);

	if (bootverbose)
		xpt_announce_periph(periph, NULL);

	return(CAM_REQ_CMP);
}

static int
ctlpassopen(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct cam_periph *periph;
	struct ctlpass_softc *softc;
	int error;

	periph = (struct cam_periph *)dev->si_drv1;
	if (cam_periph_acquire(periph) != CAM_REQ_CMP)
		return (ENXIO);

	cam_periph_lock(periph);

	softc = (struct ctlpass_softc *)periph->softc;

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
ctlpassclose(struct cdev *dev, int flag, int fmt, struct thread *td)
{
	struct 	cam_periph *periph;
	struct  ctlpass_softc *softc;
	struct mtx *mtx;

	periph = (struct cam_periph *)dev->si_drv1;
	mtx = cam_periph_mtx(periph);
	mtx_lock(mtx);

	softc = periph->softc;
	softc->open_count--;

	if (softc->open_count == 0) {
		struct ctlpass_io_req *io_req, *io_req2;

		TAILQ_FOREACH_SAFE(io_req, &softc->done_queue, links, io_req2) {
			TAILQ_REMOVE(&softc->done_queue, io_req, links);
			ctlpassiocleanup(softc, io_req);
			uma_zfree(softc->pass_zone, io_req);
		}

		TAILQ_FOREACH_SAFE(io_req, &softc->incoming_queue, links,
				   io_req2) {
			TAILQ_REMOVE(&softc->incoming_queue, io_req, links);
			ctlpassiocleanup(softc, io_req);
			uma_zfree(softc->pass_zone, io_req);
		}

		/*
		 * If there are any active I/Os, we need to forcibly acquire a
		 * reference to the peripheral so that we don't go away
		 * before they complete.  We'll release the reference when
		 * the abandoned queue is empty.
		 */
		io_req = TAILQ_FIRST(&softc->active_queue);
		if ((io_req != NULL)
		 && (softc->flags & PASS_FLAG_ABANDONED_REF_SET) == 0) {
			cam_periph_doacquire(periph);
			softc->flags |= PASS_FLAG_ABANDONED_REF_SET;
		}

		/*
		 * Since the I/O in the active queue is not under our
		 * control, just set a flag so that we can clean it up when
		 * it completes and put it on the abandoned queue.  This
		 * will prevent our sending spurious completions in the
		 * event that the device is opened again before these I/Os
		 * complete.
		 */
		TAILQ_FOREACH_SAFE(io_req, &softc->active_queue, links,
				   io_req2) {
			TAILQ_REMOVE(&softc->active_queue, io_req, links);
			io_req->flags |= PASS_IO_ABANDONED;
			TAILQ_INSERT_TAIL(&softc->abandoned_queue, io_req,
					  links);
		}
	}

	cam_periph_release_locked(periph);

	/*
	 * We reference the lock directly here, instead of using
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
	mtx_unlock(mtx);

	return (0);
}


static void
ctlpassstart(struct cam_periph *periph, union ccb *start_ccb)
{
	struct ctlpass_softc *softc;

	softc = (struct ctlpass_softc *)periph->softc;
	
	switch (softc->state) {
	case CTLPASS_STATE_NORMAL: {
		struct ctlpass_io_req *io_req;
	
		printf("\nctl passstart\n");	
		/*
		 * Check for any queued I/O requests that require an
		 * allocated slot.
		 */
		io_req = TAILQ_FIRST(&softc->incoming_queue);
		if (io_req == NULL) {
			xpt_release_ccb(start_ccb);
			break;
		}
		TAILQ_REMOVE(&softc->incoming_queue, io_req, links);
		TAILQ_INSERT_TAIL(&softc->active_queue, io_req, links);
		/*
		 * Merge the user's CCB into the allocated CCB.
		 */
		xpt_merge_ccb(start_ccb, &io_req->ccb);
		start_ccb->ccb_h.ccb_type = PASS_CCB_QUEUED_IO;
		start_ccb->ccb_h.ccb_ioreq = io_req;
		start_ccb->ccb_h.cbfcnp = ctlpassdone;
		io_req->alloced_ccb = start_ccb;
		binuptime(&io_req->start_time);
		devstat_start_transaction(softc->device_stats,
							  &io_req->start_time);
		
		cam_periph_unlock(periph);
		xpt_action(start_ccb);
		cam_periph_lock(periph);

		/*
		 * If we have any more I/O waiting, schedule ourselves again.
		 */
		if (!TAILQ_EMPTY(&softc->incoming_queue))
			xpt_schedule(periph, CAM_PRIORITY_NORMAL);
		break;
	}
	default:
		break;
	}
}

static void
ctlpassdone(struct cam_periph *periph, union ccb *done_ccb)
{ 
	struct ctlpass_softc *softc;
	struct ccb_scsiio *csio;
	struct ctlpass_io_req *io_req;
	union ctl_io *ctlio;
	

	printf("ctlpassdone\n");	
	softc = (struct ctlpass_softc *)periph->softc;

	cam_periph_assert(periph, MA_OWNED);

	
	csio = &done_ccb->csio;
	
	
	switch(csio->ccb_h.ccb_type){
	case PASS_CCB_QUEUED_IO:{
		io_req = done_ccb->ccb_h.ccb_ioreq;
		ctlio=io_req->ccb.ccb_h.ctl_ioreq;
	
#if 0
		xpt_print(periph->path, "%s: called for user CCB %p\n",
			  __func__, io_req->user_ccb_ptr);
#endif
		if (done_ccb->ccb_h.status != CAM_REQ_CMP) {
			int error;
			error = ctlpasserror(done_ccb, CAM_RETRY_SELTO,
					  SF_RETRY_UA | SF_NO_PRINT);
			scsi_sense_print(&done_ccb->csio);
		ctlio->scsiio.scsi_status = done_ccb->csio.scsi_status;
				ctlio->scsiio.sense_len = 0;
				ctlio->scsiio.io_hdr.status = CTL_SUCCESS;


			if (error == ERESTART) {
				/*
				 * A retry was scheduled, so
 				 * just return.
				 */
				return;
			}
				ctl_done((union ctl_io *)&ctlio->scsiio);

		}
	if((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
		cam_release_devq(done_ccb->ccb_h.path, 0 ,0 ,0, 0);

		/*
		 * Copy the allocated CCB contents back to the malloced CCB
		 * so we can give status back to the user when he requests it.
		 */
//	memcpy(&io_req->ccb,done_ccb,io->scsiio.cdb_len);

		/*
		 * Log data/transaction completion with devstat(9).
		 */
		switch (done_ccb->ccb_h.func_code) {
			
		case XPT_SCSI_IO:
			
			devstat_end_transaction(softc->device_stats,
			    done_ccb->csio.dxfer_len - done_ccb->csio.resid,
			    done_ccb->csio.tag_action & 0x3,
			    ((done_ccb->ccb_h.flags & CAM_DIR_MASK) ==
			    CAM_DIR_NONE) ? DEVSTAT_NO_DATA :
			    (done_ccb->ccb_h.flags & CAM_DIR_OUT) ?
			    DEVSTAT_WRITE : DEVSTAT_READ, NULL,
			    &io_req->start_time);
	
			if(done_ccb->ccb_h.status==CAM_REQ_CMP)
			{
				ctlio->scsiio.scsi_status = SCSI_STATUS_OK;
				ctlio->scsiio.sense_len = 0;
				ctlio->scsiio.io_hdr.status = CTL_SUCCESS;

				//ctl_set_success(&ctlio->scsiio);
				switch(csio->cdb_io.cdb_bytes[0])
				{
		
					case INQUIRY:
					case SERVICE_ACTION_IN:
					case READ_CAPACITY:
					case MODE_SENSE_6:
					case MODE_SENSE_10:
					case READ_6:
					case READ_10:
					case READ_12:
					case READ_16:
					case REQUEST_SENSE:
					case REPORT_LUNS:{

						if(csio->data_ptr == NULL)
						{
							printf("hello I am null");
						}
						memcpy(ctlio->scsiio.kern_data_ptr,csio->data_ptr,csio->dxfer_len);
						//scsi_print_inquiry((struct scsi_inquiry_data *)ctlio->scsiio.kern_data_ptr);
						ctlio->scsiio.kern_sg_entries = 0;
						ctlio->scsiio.kern_data_resid = 0;
						ctlio->scsiio.kern_rel_offset=0;
						ctlio->scsiio.kern_data_len = csio->dxfer_len;
						ctlio->scsiio.kern_total_len = csio->dxfer_len;
						ctlio->scsiio.io_hdr.flags |= CTL_FLAG_ALLOCATED;
						ctlio->scsiio.be_move_done = ctl_config_move_done;
	
					//	free(csio->data_ptr, M_SCSIPASSTHROUGH);
					//	csio->data_ptr= NULL;			
					ctl_datamove((union ctl_io *)ctlio);
						break;
					}
				
					case TEST_UNIT_READY:
					case START_STOP_UNIT:
					case WRITE_6:
					case WRITE_10:
					case WRITE_12:
					case WRITE_16:
					case SYNCHRONIZE_CACHE:
					case SYNCHRONIZE_CACHE_16:{

						ctl_done((union ctl_io *)&ctlio->scsiio);
						break;
					}
				}			
			}
			break;
		default:
			devstat_end_transaction(softc->device_stats, 0,
			    DEVSTAT_TAG_NONE, DEVSTAT_NO_DATA, NULL,
			    &io_req->start_time);
			break;
		}
			
		
		/*
		 * In the normal case, take the completed I/O off of the
		 * active queue and put it on the done queue.  Notitfy the
		 * user that we have a completed I/O.
		 */
		if ((io_req->flags & PASS_IO_ABANDONED) == 0) {
			TAILQ_REMOVE(&softc->active_queue, io_req, links);
			ctlpassiocleanup(softc, io_req);
			uma_zfree(softc->pass_zone, io_req);
		//	TAILQ_INSERT_TAIL(&softc->done_queue, io_req, links);	
		} else {
			/*
			 * In the case of an abandoned I/O (final close
			 * without fetching the I/O), take it off of the
			 * abandoned queue and free it.
			 */
			TAILQ_REMOVE(&softc->abandoned_queue, io_req, links);
			ctlpassiocleanup(softc, io_req);
			uma_zfree(softc->pass_zone, io_req);
	

			/*
			 * Release the done_ccb here, since we may wind up
			 * freeing the peripheral when we decrement the
			 * reference count below.
			 */
			xpt_release_ccb(done_ccb);

			/*
			 * If the abandoned queue is empty, we can release
			 * our reference to the periph since we won't have
			 * any more completions coming.
			 */
			if ((TAILQ_EMPTY(&softc->abandoned_queue))
			 && (softc->flags & PASS_FLAG_ABANDONED_REF_SET)) {
				softc->flags &= ~PASS_FLAG_ABANDONED_REF_SET;
				cam_periph_release_locked(periph);
			}

			/*
			 * We have already released the CCB, so we can
			 * return.
			 */
			return;
		}
		break;
	}
	}
	
	xpt_release_ccb(done_ccb);
}

static int
ctlpasscreatezone(struct cam_periph *periph)
{
	struct ctlpass_softc *softc;
	int error;

	error = 0;
	softc = (struct ctlpass_softc *)periph->softc;

	cam_periph_assert(periph, MA_OWNED);
	KASSERT(((softc->flags & PASS_FLAG_ZONE_VALID) == 0), 
		("%s called when the passthrough  zone is valid!\n", __func__));
	KASSERT((softc->pass_zone == NULL), 
		("%s called when the passthrough  zone is allocated!\n", __func__));

	if ((softc->flags & PASS_FLAG_ZONE_INPROG) == 0) {

		/*
		 * We're the first context through, so we need to create
		 * the passthrough UMA zone for I/O requests.
		 */
		softc->flags |= PASS_FLAG_ZONE_INPROG;

		/*
		 * uma_zcreate() does a blocking (M_WAITOK) allocation,
		 * so we cannot hold a mutex while we call it.
		 */
		cam_periph_unlock(periph);

		softc->pass_zone = uma_zcreate(softc->zone_name,
		    sizeof(struct ctlpass_io_req), NULL, NULL, NULL, NULL,
		    /*align*/ 0, /*flags*/ 0);

//		softc->pass_io_zone = uma_zcreate(softc->io_zone_name,
//		    softc->io_zone_size, NULL, NULL, NULL, NULL,
//		    /*align*/ 0, /*flags*/ 0);

		cam_periph_lock(periph);

		if (softc->pass_zone == NULL) {
			if (softc->pass_zone == NULL)
				xpt_print(periph->path, "unable to allocate "
				    "IO Req UMA zone\n");
			else
				xpt_print(periph->path, "unable to allocate "
				    "IO UMA zone\n");
			softc->flags &= ~PASS_FLAG_ZONE_INPROG;
			goto bailout;
		}

		/*
		 * Set the flags appropriately and notify any other waiters.
		 */
		softc->flags &= PASS_FLAG_ZONE_INPROG;
		softc->flags |= PASS_FLAG_ZONE_VALID;
		wakeup(&softc->pass_zone);
	} else {
		/*
		 * In this case, the UMA zone has not yet been created, but
		 * another context is in the process of creating it.  We
		 * need to sleep until the creation is either done or has
		 * failed.
		 */
		while ((softc->flags & PASS_FLAG_ZONE_INPROG)
		    && ((softc->flags & PASS_FLAG_ZONE_VALID) == 0)) {
			error = msleep(&softc->pass_zone,
				       cam_periph_mtx(periph), PRIBIO,
				       "paszon", 0);
			if (error != 0)
				goto bailout;
		}
		/*
		 * If the zone creation failed, no luck for the user.
		 */
		if ((softc->flags & PASS_FLAG_ZONE_VALID) == 0){
			error = ENOMEM;
			goto bailout;
		}
	}
bailout:
	return (error);
}

/*
* Reset the user's pointers to their original values and free
* allocated memory.
*/

static void
ctlpassiocleanup(struct ctlpass_softc *softc, struct ctlpass_io_req *io_req)
{
	union ccb *ccb;
	u_int8_t **data_ptrs[CAM_PERIPH_MAXMAPS];
	int i, numbufs;

	ccb = &io_req->ccb;

	switch (ccb->ccb_h.func_code) {

	case XPT_SCSI_IO:
	case XPT_CONT_TARGET_IO:
		data_ptrs[0] = &ccb->csio.data_ptr;
		numbufs = min(io_req->num_bufs, 1);
		break;
	default:
		/* allow ourselves to be swapped once again */
		return;
		break; /* NOTREACHED */ 
	}

	if (io_req->flags & PASS_IO_USER_SEG_MALLOC) {
		free(io_req->user_segptr, M_SCSIPASSTHROUGH);
		io_req->user_segptr = NULL;
	}

	/*
	 * We only want to free memory we MALLOC_DECLARE(M_CTLPASS);
MALLOC_DEFINE(M_CTLPASS, "CTL PASS BUFFER","BUFFER FOR CTLPASS DRIVER");malloced.
	 */
	if (io_req->data_flags == CAM_DATA_VADDR) {
		for (i = 0; i < io_req->num_bufs; i++) {
			if (io_req->kern_bufs[i] == NULL)
				continue;

			free(io_req->kern_bufs[i], M_SCSIPASSTHROUGH);
			io_req->kern_bufs[i] = NULL;
		}
	} else if (io_req->data_flags == CAM_DATA_SG) {
		for (i = 0; i < io_req->num_kern_segs; i++) {
			if ((uint8_t *)(uintptr_t)
			    io_req->kern_segptr[i].ds_addr == NULL)
				continue;

			uma_zfree(softc->pass_io_zone, (uint8_t *)(uintptr_t)
			    io_req->kern_segptr[i].ds_addr);
			io_req->kern_segptr[i].ds_addr = 0;
		}
	}

	if (io_req->flags & PASS_IO_KERN_SEG_MALLOC) {
		free(io_req->kern_segptr, M_SCSIPASSTHROUGH);
		io_req->kern_segptr = NULL;
	}

	if (io_req->data_flags != CAM_DATA_PADDR) {
		for (i = 0; i < numbufs; i++) {
			/*
			 * Restore the user's buffer pointers to their
			 * previous values.
			 */
			if (io_req->user_bufs[i] != NULL)
				*data_ptrs[i] = io_req->user_bufs[i];
		}
	}

}



static int ctlccb(struct cam_periph *periph,union ctl_io *io)
{
	struct	ctlpass_softc *softc;
	union ccb *ccb;
	
	int	error;
	uint32_t priority;
	struct ctlpass_io_req *io_req;
	struct ccb_scsiio *csio;
	
	cam_periph_lock(periph);
	softc = (struct ctlpass_softc *)periph->softc;


	printf("I am in ctlccb");
		
	error =0;
	

	if ((softc->flags & PASS_FLAG_ZONE_VALID) == 0)
	{
		error = ctlpasscreatezone(periph);
		if (error != 0)
		{
				
			goto bailout;
		}
	}

	
	cam_periph_unlock(periph);
	io_req = uma_zalloc(softc->pass_zone, M_WAITOK | M_ZERO);
	ccb = &io_req->ccb;
	csio = &ccb->csio;
	
	
		switch(io->scsiio.cdb[0])
		{
		
			case INQUIRY:{
					struct scsi_inquiry *cdb;

					cdb = (struct scsi_inquiry *)io->scsiio.cdb;

					if(cdb->byte2 & SI_EVPD)
					{
						switch(cdb->page_code){
						case SVPD_SUPPORTED_PAGES:
						//	int sup_page_size;
						//	struct scsi_vpd_supported_pages *pages;
							
							//sup_page_size = sizeof(struct scsi_vpd_supported_pages)*10;
							printf("ctlpass suported pages");
							io->scsiio.kern_data_ptr = malloc(sizeof(struct scsi_vpd_supported_pages)*10/*SCSI_EVPD_NUM_SUPPORTED_PAGES*/ ,M_CTL, M_WAITOK); 
							io->scsiio.kern_data_len=sizeof(struct scsi_vpd_supported_pages)*10/*SCSI_EVPD_NUM_SUPPORTED_PAGES*/;
							csio->data_ptr =malloc(sizeof(struct scsi_vpd_supported_pages)*10/*SCSI_EVPD_NUM_SUPPORTED_PAGES*/,M_SCSIPASSTHROUGH, M_WAITOK);
							csio->dxfer_len =sizeof(struct scsi_vpd_supported_pages)*10/*SCSI_EVPD_NUM_SUPPORTED_PAGES*/;

							break;
						case SVPD_UNIT_SERIAL_NUMBER:
							printf("ctlpass svpd unit serial number");
							io->scsiio.kern_data_ptr = malloc(sizeof(struct scsi_vpd_unit_serial_number),M_CTL, M_WAITOK); 
							io->scsiio.kern_data_len= sizeof(struct scsi_vpd_unit_serial_number);
							csio->data_ptr =malloc(sizeof(struct scsi_vpd_unit_serial_number),M_SCSIPASSTHROUGH, M_WAITOK);
							csio->dxfer_len = sizeof(struct scsi_vpd_unit_serial_number);
							 break;
						case SVPD_DEVICE_ID:
							printf("ctlpass svpd device id");
							io->scsiio.kern_data_ptr = malloc(SVPD_DEVICE_ID_MAX_SIZE,M_CTL, M_WAITOK); 
							io->scsiio.kern_data_len= SVPD_DEVICE_ID_MAX_SIZE;
							csio->data_ptr =malloc(SVPD_DEVICE_ID_MAX_SIZE,M_SCSIPASSTHROUGH, M_WAITOK);
							csio->dxfer_len = SVPD_DEVICE_ID_MAX_SIZE;
							break;
						case SVPD_EXTENDED_INQUIRY_DATA:
							printf("ctl pass inquiry data");
							io->scsiio.kern_data_ptr = malloc(sizeof(struct scsi_vpd_extended_inquiry_data),M_CTL, M_WAITOK); 
							io->scsiio.kern_data_len= sizeof(struct scsi_vpd_extended_inquiry_data);
							csio->data_ptr =malloc(sizeof(struct scsi_vpd_extended_inquiry_data),M_SCSIPASSTHROUGH, M_WAITOK);
							csio->dxfer_len = sizeof(struct scsi_vpd_extended_inquiry_data);
							break;
				}
			}
			else
			{
					struct scsi_inquiry *scsi_cmd;
					printf("hello inquiry");
					scsi_cmd = (struct scsi_inquiry *)&csio->cdb_io.cdb_bytes;
					bzero(scsi_cmd, sizeof(*scsi_cmd));
	
					io->scsiio.kern_data_ptr =malloc(io->scsiio.ext_data_len,M_CTL, M_WAITOK);
	 	
					io->scsiio.kern_data_len = io->scsiio.ext_data_len;
					csio->data_ptr =malloc(io->scsiio.kern_data_len,M_SCSIPASSTHROUGH, M_WAITOK);
					csio->dxfer_len = io->scsiio.kern_data_len;
			}
			printf("after switch case");
			csio->cdb_len = io->scsiio.cdb_len;
			csio->ccb_h.flags = CAM_DIR_IN;
			csio->sense_len = SSD_FULL_SIZE;

			break;
		}
			case REQUEST_SENSE:{
					struct scsi_request_sense *scsi_cmd;
					scsi_cmd = (struct scsi_request_sense *)&csio->cdb_io.cdb_bytes;
			//		bzero(scsi_cmd, sizeof(*scsi_cmd));


					io->scsiio.kern_data_ptr =malloc(sizeof(struct scsi_sense_data),M_CTL, M_WAITOK);
	 	
					io->scsiio.kern_data_len = sizeof(struct scsi_sense_data);

					csio->cdb_len = sizeof(*scsi_cmd);
					csio->ccb_h.flags = CAM_DIR_IN;
					csio->sense_len = SSD_FULL_SIZE;

	
					csio->data_ptr =malloc(sizeof(struct scsi_sense_data),M_SCSIPASSTHROUGH, M_WAITOK);
					csio->dxfer_len = sizeof(struct scsi_sense_data);
					break;
					}
			case REPORT_LUNS:{
/*

						struct scsi_report_luns_data *lun_data;
						struct ctl_port *port;
						int num_luns;
						uint32_t lun_datalen,targ_lun_id;
					
						port = ctl_io_port(&io->scsiio.io_hdr);

						
						num_luns=0;			
						for(targ_lun_id = 0 ;targ_lun_id <CTL_MAX_LUNS;targ_lun_id++){
							if(ctl_lun_map_from_port(port, targ_lun_id)<CTL_MAX_LUNS)
								num_luns++;
}

						lun_datalen  = sizeof(*lun_data) + (num_luns * sizeof(struct scsi_report_luns_lundata));
					io->scsiio.kern_data_ptr =malloc(lun_datalen,M_CTL, M_WAITOK);
	 	
					io->scsiio.kern_data_len = lun_datalen;
					csio->data_ptr =malloc(lun_datalen,M_CTLPASS, M_WAITOK);
					csio->dxfer_len = lun_datalen;*/
					error=1;
					return (error);
					break;
					}
			case START_STOP_UNIT:
				{
				struct scsi_start_stop_unit *scsi_cmd;
				int extra_flags=0;
				scsi_cmd = (struct scsi_start_stop_unit *)&csio->cdb_io.cdb_bytes;
			//	bzero(scsi_cmd , sizeof(*scsi_cmd));

				csio->cdb_len = sizeof(*scsi_cmd);	
				
				extra_flags |= CAM_HIGH_POWER;

				csio->ccb_h.flags = CAM_DIR_NONE|extra_flags;
				csio->sense_len = SSD_FULL_SIZE;
				csio->data_ptr = NULL;
				csio->dxfer_len = 0;
			
					break;
				}
			case READ_CAPACITY:{
					struct scsi_read_capacity *scsi_cmd;
					scsi_cmd = (struct scsi_read_capacity *)&csio->cdb_io.cdb_bytes;
			//	bzero(scsi_cmd, sizeof(*scsi_cmd));
			io->scsiio.kern_data_ptr =(uint8_t *)malloc(sizeof(struct scsi_read_capacity_data_long),M_CTL, M_WAITOK);
	 	
			io->scsiio.kern_data_len = sizeof(struct scsi_read_capacity_data_long);

			csio->cdb_len = sizeof(*scsi_cmd);	
	

			csio->ccb_h.flags = CAM_DIR_IN;
			csio->sense_len = SSD_FULL_SIZE;
				
			csio->data_ptr =(uint8_t *)malloc(sizeof(struct scsi_read_capacity_data_long),M_SCSIPASSTHROUGH, M_WAITOK);
			csio->dxfer_len = sizeof(struct scsi_read_capacity_data_long);
					break;
			
			}
			case SERVICE_ACTION_IN:{
				struct scsi_read_capacity_16 *scsi_cmd;
				scsi_cmd = (struct scsi_read_capacity_16 *)&csio->cdb_io.cdb_bytes;
		//	bzero(scsi_cmd, sizeof(*scsi_cmd));
				io->scsiio.kern_data_ptr =(uint8_t *)malloc(sizeof(struct scsi_read_capacity_data_long),M_CTL, M_WAITOK);
	 	
			io->scsiio.kern_data_len = sizeof(struct scsi_read_capacity_data_long);

			csio->cdb_len = sizeof(*scsi_cmd);	
	

			csio->ccb_h.flags = CAM_DIR_IN;
			csio->sense_len = SSD_FULL_SIZE;
				
			csio->data_ptr =(uint8_t *)malloc(sizeof(struct scsi_read_capacity_data_long),M_SCSIPASSTHROUGH, M_WAITOK);
			csio->dxfer_len = sizeof(struct scsi_read_capacity_data_long);
					break;
	}
			case MODE_SENSE_6:{
			/*	struct scsi_mode_sense_6 *scsi_cmd;
				scsi_cmd = (struct scsi_mode_sense_6 *)&csio->cdb_io.cdb_bytes;
			bzero(scsi_cmd, sizeof(*scsi_cmd));
				io->scsiio.kern_data_len =  sizeof(struct scsi_mode_header_6)+sizeof(struct scsi_mode_blk_desc);
				io->scsiio.kern_data_ptr =malloc(io->scsiio.kern_data_len, M_CTL, M_WAITOK);
	 	
				
	//		scsi_cmd->opcode = MODE_SENSE_6;
			csio->cdb_len = sizeof(*scsi_cmd);	
	

			csio->ccb_h.flags = CAM_DIR_IN;
			csio->sense_len = SSD_FULL_SIZE;
		
			csio->dxfer_len = sizeof(struct scsi_mode_header_6)+sizeof(struct scsi_mode_blk_desc);		
			csio->data_ptr =malloc(csio->dxfer_len,M_CTLPASS, M_WAITOK);
	*/
					error=1;
					return (error);
					break;
			}
			case MODE_SENSE_10:{
				struct scsi_mode_sense_10 *scsi_cmd;
				scsi_cmd = (struct scsi_mode_sense_10 *)&csio->cdb_io.cdb_bytes;
		//		bzero(scsi_cmd, sizeof(*scsi_cmd));
				io->scsiio.kern_data_len = sizeof(struct scsi_mode_header_10)+sizeof(struct scsi_mode_blk_desc);
				io->scsiio.kern_data_ptr =malloc(io->scsiio.kern_data_len,M_CTL, M_WAITOK);
	 		
				csio->cdb_len = sizeof(*scsi_cmd);	
	

				csio->ccb_h.flags = CAM_DIR_IN;
				csio->sense_len = SSD_FULL_SIZE;
				
			
				csio->dxfer_len =   sizeof(struct scsi_mode_header_6)+sizeof(struct scsi_mode_blk_desc);	
				csio->data_ptr =malloc(csio->dxfer_len,M_SCSIPASSTHROUGH, M_WAITOK);

				break;

		}
			case TEST_UNIT_READY:{
					struct scsi_test_unit_ready *scsi_cmd;
					scsi_cmd = (struct scsi_test_unit_ready *)&csio->cdb_io.cdb_bytes;
		//			bzero(scsi_cmd , sizeof(*scsi_cmd));

				csio->cdb_len = sizeof(*scsi_cmd);	
	

				csio->ccb_h.flags = CAM_DIR_NONE;
				csio->sense_len = SSD_FULL_SIZE;
				csio->data_ptr = NULL;
				csio->dxfer_len = 0;
					break;
			}
			case WRITE_6:{

				struct scsi_rw_6 *scsi_cmd;
				scsi_cmd = (struct scsi_rw_6 *)&csio->cdb_io.cdb_bytes;
				csio->cdb_len = sizeof(*scsi_cmd);
				io->scsiio.kern_rel_offset =0 ;
		//		bzero(scsi_cmd, sizeof(*scsi_cmd));
			//	printf("%.*s",io->scsiio.kern_data_len,io->scsiio.kern_data_ptr);

				csio->ccb_h.flags = CAM_DIR_OUT;
				csio->sense_len = SSD_FULL_SIZE;
				csio->data_ptr = (uint8_t *)malloc(io->scsiio.kern_data_len,M_SCSIPASSTHROUGH, M_WAITOK);
				csio->dxfer_len = io->scsiio.kern_data_len;

				memcpy(csio->data_ptr,io->scsiio.kern_data_ptr,io->scsiio.kern_data_len);
				//printf("%.*s",csio->dxfer_len,csio->data_ptr);
				break;
			}
			case WRITE_10:{
					struct scsi_rw_10 *scsi_cmd;
				scsi_cmd = (struct scsi_rw_10 *)&csio->cdb_io.cdb_bytes;
		//			bzero(scsi_cmd, sizeof(*scsi_cmd));
				csio->cdb_len = sizeof(*scsi_cmd);
				io->scsiio.kern_rel_offset =0 ;

			//	printf("%.*s",io->scsiio.kern_data_len,io->scsiio.kern_data_ptr);

				csio->ccb_h.flags = CAM_DIR_OUT;
				csio->sense_len = SSD_FULL_SIZE;
				csio->data_ptr = (uint8_t *)malloc(io->scsiio.kern_data_len,M_SCSIPASSTHROUGH, M_WAITOK);
				csio->dxfer_len = io->scsiio.kern_data_len;

				memcpy(csio->data_ptr,io->scsiio.kern_data_ptr,io->scsiio.kern_data_len);
				//printf("%.*s",csio->dxfer_len,csio->data_ptr);
				break;

			}
			case WRITE_12:{
						struct scsi_rw_12 *scsi_cmd;
				scsi_cmd = (struct scsi_rw_12 *)&csio->cdb_io.cdb_bytes;
				bzero(scsi_cmd, sizeof(*scsi_cmd));
				csio->cdb_len = sizeof(*scsi_cmd);
				io->scsiio.kern_rel_offset =0 ;

			//	printf("%.*s",io->scsiio.kern_data_len,io->scsiio.kern_data_ptr);

				csio->ccb_h.flags = CAM_DIR_OUT;
				csio->sense_len = SSD_FULL_SIZE;
				csio->data_ptr = (uint8_t *)malloc(io->scsiio.kern_data_len,M_SCSIPASSTHROUGH, M_WAITOK);
				csio->dxfer_len = io->scsiio.kern_data_len;

				memcpy(csio->data_ptr,io->scsiio.kern_data_ptr,io->scsiio.kern_data_len);
			//	printf("%.*s",csio->dxfer_len,csio->data_ptr);
				break;

			}

			case WRITE_16:{
				struct scsi_rw_16 *scsi_cmd;
				scsi_cmd = (struct scsi_rw_16 *)&csio->cdb_io.cdb_bytes;
				bzero(scsi_cmd, sizeof(*scsi_cmd));
				csio->cdb_len = sizeof(*scsi_cmd);
				io->scsiio.kern_rel_offset =0 ;

			//	printf("%.*s",io->scsiio.kern_data_len,io->scsiio.kern_data_ptr);

				csio->ccb_h.flags = CAM_DIR_OUT;
				csio->sense_len = SSD_FULL_SIZE;
				csio->data_ptr = (uint8_t *)malloc(io->scsiio.kern_data_len,M_SCSIPASSTHROUGH, M_WAITOK);
				csio->dxfer_len = io->scsiio.kern_data_len;

				memcpy(csio->data_ptr,io->scsiio.kern_data_ptr,io->scsiio.kern_data_len);
			//	printf("%.*s",csio->dxfer_len,csio->data_ptr);
				break;



			}			
			case READ_6:{
				struct scsi_rw_6 *scsi_cmd;
				scsi_cmd = (struct scsi_rw_6 *)&csio->cdb_io.cdb_bytes;
				bzero(scsi_cmd, sizeof(*scsi_cmd));
				csio->cdb_len = sizeof(*scsi_cmd);
				io->scsiio.kern_rel_offset =0 ;

				csio->ccb_h.flags = CAM_DIR_IN;
				csio->sense_len = SSD_FULL_SIZE;
					
	
				io->scsiio.kern_data_ptr =malloc(io->scsiio.ext_data_len,M_CTL, M_WAITOK);
	 	
				io->scsiio.kern_data_len = io->scsiio.ext_data_len;
				
				csio->data_ptr = (uint8_t *)malloc(io->scsiio.kern_data_len,M_SCSIPASSTHROUGH, M_WAITOK);
				csio->dxfer_len = io->scsiio.kern_data_len;

				break;
			}
			case READ_10:{
				struct scsi_rw_10 *scsi_cmd;
				scsi_cmd = (struct scsi_rw_10 *)&csio->cdb_io.cdb_bytes;
				bzero(scsi_cmd, sizeof(*scsi_cmd));
				csio->cdb_len = sizeof(*scsi_cmd);
				io->scsiio.kern_rel_offset =0 ;

				csio->ccb_h.flags = CAM_DIR_IN;
				csio->sense_len = SSD_FULL_SIZE;
					
	
				io->scsiio.kern_data_ptr =malloc(io->scsiio.ext_data_len,M_CTL, M_WAITOK);
	 	
				io->scsiio.kern_data_len = io->scsiio.ext_data_len;
				
				csio->data_ptr = (uint8_t *)malloc(io->scsiio.kern_data_len,M_SCSIPASSTHROUGH, M_WAITOK);
				csio->dxfer_len = io->scsiio.kern_data_len;

				break;
			}
			case READ_12:{
				struct scsi_rw_12 *scsi_cmd;
				scsi_cmd = (struct scsi_rw_12 *)&csio->cdb_io.cdb_bytes;
				bzero(scsi_cmd, sizeof(*scsi_cmd));
				csio->cdb_len = sizeof(*scsi_cmd);
				io->scsiio.kern_rel_offset =0 ;

				csio->ccb_h.flags = CAM_DIR_IN;
				csio->sense_len = SSD_FULL_SIZE;
					
	
				io->scsiio.kern_data_ptr =malloc(io->scsiio.ext_data_len,M_CTL, M_WAITOK);
	 	
				io->scsiio.kern_data_len = io->scsiio.ext_data_len;
				
				csio->data_ptr = (uint8_t *)malloc(io->scsiio.kern_data_len,M_SCSIPASSTHROUGH, M_WAITOK);
				csio->dxfer_len = io->scsiio.kern_data_len;

				break;
			}

			case READ_16:{
				struct scsi_rw_16 *scsi_cmd;
				scsi_cmd = (struct scsi_rw_16 *)&csio->cdb_io.cdb_bytes;
				bzero(scsi_cmd, sizeof(*scsi_cmd));
				csio->cdb_len = sizeof(*scsi_cmd);
				io->scsiio.kern_rel_offset =0 ;

				csio->ccb_h.flags = CAM_DIR_IN;
				csio->sense_len = SSD_FULL_SIZE;
					
	
				io->scsiio.kern_data_ptr =malloc(io->scsiio.ext_data_len,M_CTL, M_WAITOK);
	 	
				io->scsiio.kern_data_len = io->scsiio.ext_data_len;
				
				csio->data_ptr = (uint8_t *)malloc(io->scsiio.kern_data_len,M_SCSIPASSTHROUGH, M_WAITOK);
				csio->dxfer_len = io->scsiio.kern_data_len;

				break;
			}
			case SYNCHRONIZE_CACHE:{
				
				struct scsi_sync_cache *scsi_cmd;
					scsi_cmd = (struct scsi_sync_cache *)&csio->cdb_io.cdb_bytes;
					bzero(scsi_cmd , sizeof(*scsi_cmd));

				csio->cdb_len = sizeof(*scsi_cmd);	
	

				csio->ccb_h.flags = CAM_DIR_NONE;
				csio->sense_len = SSD_FULL_SIZE;
				csio->data_ptr = NULL;
				csio->dxfer_len = 0;
					break;

			}
			case SYNCHRONIZE_CACHE_16: {
				struct scsi_sync_cache_16 *scsi_cmd;
				scsi_cmd = (struct scsi_sync_cache_16 *)&csio->cdb_io.cdb_bytes;
				bzero(scsi_cmd , sizeof(*scsi_cmd));

				csio->cdb_len = sizeof(*scsi_cmd);	
	

				csio->ccb_h.flags = CAM_DIR_NONE;
				csio->sense_len = SSD_FULL_SIZE;
				csio->data_ptr = NULL;
				csio->dxfer_len = 0;
					break;

			}
			default:
					return 1;
					break;
		}
	
	memcpy(csio->cdb_io.cdb_bytes,io->scsiio.cdb, io->scsiio.cdb_len);
		
		ccb->ccb_h.func_code = XPT_SCSI_IO;

		switch(io->scsiio.tag_type)
		{
			case CTL_TAG_SIMPLE:
				csio->tag_action = MSG_SIMPLE_Q_TAG;
				break;
			case CTL_TAG_HEAD_OF_QUEUE:
				csio->tag_action = MSG_HEAD_OF_QUEUE_TASK;
				break;
			case CTL_TAG_ORDERED:
				csio->tag_action = MSG_ORDERED_Q_TAG;
				break;
			case CTL_TAG_ACA:
				csio->tag_action = MSG_ACA_TASK;
				break;
			default:
				csio->tag_action = CAM_TAG_ACTION_NONE;
				break;

		}
	
		ccb->ccb_h.ctl_ioreq= io;
		ccb->ccb_h.ccb_ioreq = io_req;
		
		/* Compatibility for RL/priority-unaware code. */
		priority = ccb->ccb_h.pinfo.priority;
		if (priority <= CAM_PRIORITY_OOB)
		    priority += CAM_PRIORITY_OOB + 1;
	


		ccb->ccb_h.cbfcnp = ctlpassdone;
		
		xpt_setup_ccb(&ccb->ccb_h, periph->path,priority);		

	
		
	
		
	
		io_req->mapinfo.num_bufs_used = 0;

        	cam_periph_lock(periph);	

		/*
		 * Everything goes on the incoming queue initially.
		 */
		TAILQ_INSERT_TAIL(&softc->incoming_queue, io_req, links);
	
		xpt_schedule(periph,priority);
		
bailout:
	cam_periph_unlock(periph);

	return(error);
}



static int
ctlpasserror(union ccb *ccb, u_int32_t cam_flags, u_int32_t sense_flags)
{
	struct cam_periph *periph;
	struct ctlpass_softc *softc;

	periph = xpt_path_periph(ccb->ccb_h.path);
	softc = (struct ctlpass_softc *)periph->softc;
	return(cam_periph_error(ccb, cam_flags, sense_flags, 
				 &softc->saved_ccb));
}

static int ctlstrcmp(const char *a,const char *b)
{
	while(*a && *b && *a == *b){ ++a; ++b; }
	return *a - *b;
}
