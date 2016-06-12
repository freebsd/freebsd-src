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

#include <cam/ctl/ctl_io.h>

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
	PASS_STATE_NORMAL
} pass_state;

typedef enum {
	PASS_CCB_BUFFER_IO,
	PASS_CCB_QUEUED_IO
} pass_ccb_types;

#define ccb_type	ppriv_field0
#define ccb_ioreq	ppriv_ptr1

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

struct passthrough_io_req {
	union ccb			 ccb;
	union ccb			*alloced_ccb;
	union ccb			*user_ccb_ptr;
	camq_entry			 user_periph_links;
	ccb_ppriv_area			 user_periph_priv;
	struct cam_periph_map_info	 mapinfo;
	pass_io_flags			 flags;
	ccb_flags			 data_flags;
	int				 num_user_segs;
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
	TAILQ_ENTRY(passthrough_io_req)	 links;
};

struct passthrough_softc {
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
	struct selinfo		  read_select;
	TAILQ_HEAD(, passthrough_io_req) incoming_queue;
	TAILQ_HEAD(, passthrough_io_req) active_queue;
	TAILQ_HEAD(, passthrough_io_req) abandoned_queue;
	TAILQ_HEAD(, passthrough_io_req) done_queue;
	struct cam_periph	 *periph;
	char			  zone_name[12];
	char			  io_zone_name[12];
	uma_zone_t		  pass_zone;
	uma_zone_t		  pass_io_zone;
	size_t			  io_zone_size;
};

static	d_open_t	passthroughopen;
static	d_close_t	passthroughclose;
static	periph_init_t	passthroughinit;
static	periph_ctor_t	passthroughregister;
static	periph_oninv_t	passthroughoninvalidate;
static	periph_dtor_t	passthroughcleanup;
static	periph_start_t	passthroughstart;

static	void		passthrough_add_physpath(void *context, int pending);
static	void		passthroughasync(void *callback_arg, u_int32_t code,
				  struct cam_path *path, void *arg);
static	void		passthroughdone(struct cam_periph *periph, 
				 union ccb *done_ccb);
static	void		passthroughiocleanup(struct passthrough_softc *softc, 
				      struct passthrough_io_req *io_req);

static	int		passthrougherror(union ccb *ccb, u_int32_t cam_flags, 
				  u_int32_t sense_flags);
static 	int		ctlccb(union ctl_io *io); 

static struct periph_driver passthroughdriver =
{
	passthroughinit, "passthrough",
	TAILQ_HEAD_INITIALIZER(passthroughdriver.units), /* generation */ 0
};

PERIPHDRIVER_DECLARE(passthrough, passthroughdriver);

static struct cdevsw passthrough_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_TRACKCLOSE,
	.d_open =	passthroughopen,
	.d_close =	passthroughclose,
	.d_name =	"passthrough",
};


static MALLOC_DEFINE(M_SCSIPASSTHROUGH, "scsi_pass", "scsi passthrough buffers");

static void
passthroughinit(void)
{
	cam_status status;

	/*
	 * Install a global async callback.  This callback will
	 * receive async callbacks like "new device found".
	 */
	status = xpt_register_async(AC_FOUND_DEVICE, passthroughasync, NULL, NULL);

	if (status != CAM_REQ_CMP) {
		printf("pass: Failed to attach master async callback "
		       "due to status 0x%x!\n", status);
	}

}

static int ctlccb(union ctl_io *io)
{

	return 1;

}




static void
passrejectios(struct cam_periph *periph)
{
	struct passthrough_io_req *io_req, *io_req2;
	struct passthrough_softc *softc;

	softc = (struct passthrough_softc *)periph->softc;

	/*
	 * The user can no longer get status for I/O on the done queue, so
	 * clean up all outstanding I/O on the done queue.
	 */
	TAILQ_FOREACH_SAFE(io_req, &softc->done_queue, links, io_req2) {
		TAILQ_REMOVE(&softc->done_queue, io_req, links);
		passthroughiocleanup(softc, io_req);
		uma_zfree(softc->pass_zone, io_req);
	}

	/*
	 * The underlying device is gone, so we can't issue these I/Os.
	 * The devfs node has been shut down, so we can't return status to
	 * the user.  Free any I/O left on the incoming queue.
	 */
	TAILQ_FOREACH_SAFE(io_req, &softc->incoming_queue, links, io_req2) {
		TAILQ_REMOVE(&softc->incoming_queue, io_req, links);
		passthroughiocleanup(softc, io_req);
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
passthroughdevgonecb(void *arg)
{
	struct cam_periph *periph;
	struct mtx *mtx;
	struct passthrough_softc *softc;
	int i;

	periph = (struct cam_periph *)arg;
	mtx = cam_periph_mtx(periph);
	mtx_lock(mtx);

	softc = (struct passthrough_softc *)periph->softc;
	KASSERT(softc->open_count >= 0, ("Negative open count %d",
		softc->open_count));

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
	 * Accordingly, inform all queued I/Os of their fate.
	 */
	cam_periph_release_locked(periph);
	passrejectios(periph);

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
passthroughoninvalidate(struct cam_periph *periph)
{
	struct passthrough_softc *softc;

	softc = (struct passthrough_softc *)periph->softc;

	/*
	 * De-register any async callbacks.
	 */
	xpt_register_async(0, passthroughasync, periph, periph->path);

	softc->flags |= PASS_FLAG_INVALID;

	/*
	 * Tell devfs this device has gone away, and ask for a callback
	 * when it has cleaned up its state.
	 */
	destroy_dev_sched_cb(softc->dev, passthroughdevgonecb, periph);
}

static void
passthroughcleanup(struct cam_periph *periph)
{
	struct passthrough_softc *softc;

	softc = (struct passthrough_softc *)periph->softc;

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
pass_add_physpath(void *context, int pending)
{
	struct cam_periph *periph;
	struct passthrough_softc *softc;
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
passthroughasync(void *callback_arg, u_int32_t code,
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
		status = cam_periph_alloc(passthroughregister, passthroughoninvalidate,
					  passthroughcleanup, passthroughstart, "passthrough",
					  CAM_PERIPH_BIO, path,
					  passthroughasync, AC_FOUND_DEVICE, cgd);

		if (status != CAM_REQ_CMP
		 && status != CAM_REQ_INPROG) {
			const struct cam_status_entry *entry;

			entry = cam_fetch_status_entry(status);

			printf("passthroughasync: Unable to attach new device "
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
			struct passthrough_softc *softc;
			cam_status status;

			softc = (struct passthrough_softc *)periph->softc;
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
passthroughregister(struct cam_periph *periph, void *arg)
{
	struct passthrough_softc *softc;
	struct ccb_getdev *cgd;
	struct ccb_pathinq cpi;
	struct make_dev_args args;
	int error, no_tags;

	cgd = (struct ccb_getdev *)arg;
	if (cgd == NULL) {
		printf("%s: no getdev CCB, can't register device\n", __func__);
		return(CAM_REQ_CMP_ERR);
	}

	softc = (struct passthrough_softc *)malloc(sizeof(*softc),
					    M_DEVBUF, M_NOWAIT);

	if (softc == NULL) {
		printf("%s: Unable to probe new device. "
		       "Unable to allocate softc\n", __func__);
		return(CAM_REQ_CMP_ERR);
	}

	bzero(softc, sizeof(*softc));
	softc->state = PASS_STATE_NORMAL;
	if (cgd->protocol == PROTO_SCSI)
		softc->pd_type = SID_TYPE(&cgd->inq_data);
	else
		return(CAM_REQ_CMP_ERR);

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

	if (cpi.maxio == 0)
		softc->maxio = DFLTPHYS;	/* traditional default */
	else if (cpi.maxio > MAXPHYS)
		softc->maxio = MAXPHYS;		/* for safety */
	else
		softc->maxio = cpi.maxio;	/* real value */

	if (cpi.hba_misc & PIM_UNMAPPED)
		softc->flags |= PASS_FLAG_UNMAPPED_CAPABLE;

	/*
	 * We pass in 0 for a blocksize, since we don't 
	 * know what the blocksize of this device is, if 
	 * it even has a blocksize.
	 */
	cam_periph_unlock(periph);
	no_tags = (cgd->inq_data.flags & SID_CmdQue) == 0;
	softc->device_stats = devstat_new_entry("passthrough",
			  periph->unit_number, 0,
			  DEVSTAT_NO_BLOCKSIZE
			  | (no_tags ? DEVSTAT_NO_ORDERED_TAGS : 0),
			  softc->pd_type |
			  XPORT_DEVSTAT_TYPE(cpi.transport) |
			  DEVSTAT_TYPE_PASS,
			  DEVSTAT_PRIORITY_PASS);

	/*
	 * Acquire a reference to the periph that we can release once we've
	 * cleaned up the kqueue.
	 */
	if (cam_periph_acquire(periph) != CAM_REQ_CMP) {
		xpt_print(periph->path, "%s: lost periph during "
			  "registration!\n", __func__);
		cam_periph_lock(periph);
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
		cam_periph_lock(periph);
		return (CAM_REQ_CMP_ERR);
	}

	/* Register the device */
	make_dev_args_init(&args);
	args.mda_devsw = &passthrough_cdevsw;
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

	cam_periph_lock(periph);

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
			   passthroughasync, periph, periph->path);

	if (bootverbose)
		xpt_announce_periph(periph, NULL);

	return(CAM_REQ_CMP);
}

static int
passthroughopen(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct cam_periph *periph;
	struct passthrough_softc *softc;
	int error;

	periph = (struct cam_periph *)dev->si_drv1;
	if (cam_periph_acquire(periph) != CAM_REQ_CMP)
		return (ENXIO);

	cam_periph_lock(periph);

	softc = (struct passthrough_softc *)periph->softc;

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
passthroughclose(struct cdev *dev, int flag, int fmt, struct thread *td)
{
	struct 	cam_periph *periph;
	struct  passthrough_softc *softc;
	struct mtx *mtx;

	periph = (struct cam_periph *)dev->si_drv1;
	mtx = cam_periph_mtx(periph);
	mtx_lock(mtx);

	softc = periph->softc;
	softc->open_count--;

	if (softc->open_count == 0) {
		struct passthrough_io_req *io_req, *io_req2;

		TAILQ_FOREACH_SAFE(io_req, &softc->done_queue, links, io_req2) {
			TAILQ_REMOVE(&softc->done_queue, io_req, links);
			passthroughiocleanup(softc, io_req);
			uma_zfree(softc->pass_zone, io_req);
		}

		TAILQ_FOREACH_SAFE(io_req, &softc->incoming_queue, links,
				   io_req2) {
			TAILQ_REMOVE(&softc->incoming_queue, io_req, links);
			passthroughiocleanup(softc, io_req);
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
passthroughstart(struct cam_periph *periph, union ccb *start_ccb)
{
	struct passthrough_softc *softc;

	softc = (struct passthrough_softc *)periph->softc;

	switch (softc->state) {
	case PASS_STATE_NORMAL: {
		struct passthrough_io_req *io_req;

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
		start_ccb->ccb_h.cbfcnp = passthroughdone;
		io_req->alloced_ccb = start_ccb;
		binuptime(&io_req->start_time);
		devstat_start_transaction(softc->device_stats,
					  &io_req->start_time);

		xpt_action(start_ccb);

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
passthroughdone(struct cam_periph *periph, union ccb *done_ccb)
{ 
	struct passthrough_softc *softc;
	struct ccb_scsiio *csio;

	softc = (struct passthrough_softc *)periph->softc;

	cam_periph_assert(periph, MA_OWNED);

	csio = &done_ccb->csio;
	switch (csio->ccb_h.ccb_type) {
	case PASS_CCB_QUEUED_IO: {
		struct passthrough_io_req *io_req;

		io_req = done_ccb->ccb_h.ccb_ioreq;
#if 0
		xpt_print(periph->path, "%s: called for user CCB %p\n",
			  __func__, io_req->user_ccb_ptr);
#endif
		if (((done_ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP)
		 && (done_ccb->ccb_h.flags & CAM_PASS_ERR_RECOVER)
		 && ((io_req->flags & PASS_IO_ABANDONED) == 0)) {
			int error;

			error = passthrougherror(done_ccb, CAM_RETRY_SELTO,
					  SF_RETRY_UA | SF_NO_PRINT);

			if (error == ERESTART) {
				/*
				 * A retry was scheduled, so
 				 * just return.
				 */
				return;
			}
		}

		/*
		 * Copy the allocated CCB contents back to the malloced CCB
		 * so we can give status back to the user when he requests it.
		 */
		bcopy(done_ccb, &io_req->ccb, sizeof(*done_ccb));

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
			break;
		case XPT_SMP_IO:
			/*
			 * XXX KDM this isn't quite right, but there isn't
			 * currently an easy way to represent a bidirectional 
			 * transfer in devstat.  The only way to do it
			 * and have the byte counts come out right would
			 * mean that we would have to record two
			 * transactions, one for the request and one for the
			 * response.  For now, so that we report something,
			 * just treat the entire thing as a read.
			 */
			devstat_end_transaction(softc->device_stats,
			    done_ccb->smpio.smp_request_len +
			    done_ccb->smpio.smp_response_len,
			    DEVSTAT_TAG_SIMPLE, DEVSTAT_READ, NULL,
			    &io_req->start_time);
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
			TAILQ_INSERT_TAIL(&softc->done_queue, io_req, links);
			selwakeuppri(&softc->read_select, PRIBIO);
			KNOTE_LOCKED(&softc->read_select.si_note, 0);
		} else {
			/*
			 * In the case of an abandoned I/O (final close
			 * without fetching the I/O), take it off of the
			 * abandoned queue and free it.
			 */
			TAILQ_REMOVE(&softc->abandoned_queue, io_req, links);
			passthroughiocleanup(softc, io_req);
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

static void
passthroughiocleanup(struct passthrough_softc *softc, struct passthrough_io_req *io_req)
{
	union ccb *ccb;
	u_int8_t **data_ptrs[CAM_PERIPH_MAXMAPS];
	int i, numbufs;

	ccb = &io_req->ccb;

	switch (ccb->ccb_h.func_code) {
	case XPT_DEV_MATCH:
		numbufs = min(io_req->num_bufs, 2);

		if (numbufs == 1) {
			data_ptrs[0] = (u_int8_t **)&ccb->cdm.matches;
		} else {
			data_ptrs[0] = (u_int8_t **)&ccb->cdm.patterns;
			data_ptrs[1] = (u_int8_t **)&ccb->cdm.matches;
		}
		break;
	case XPT_SCSI_IO:
	case XPT_CONT_TARGET_IO:
		data_ptrs[0] = &ccb->csio.data_ptr;
		numbufs = min(io_req->num_bufs, 1);
		break;
	case XPT_DEV_ADVINFO:
		numbufs = min(io_req->num_bufs, 1);
		data_ptrs[0] = (uint8_t **)&ccb->cdai.buf;
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
	 * We only want to free memory we malloced.
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
static int
passthrougherror(union ccb *ccb, u_int32_t cam_flags, u_int32_t sense_flags)
{
	struct cam_periph *periph;
	struct passthrough_softc *softc;

	periph = xpt_path_periph(ccb->ccb_h.path);
	softc = (struct passthrough_softc *)periph->softc;
	
	return(cam_periph_error(ccb, cam_flags, sense_flags, 
				 &softc->saved_ccb));
}
