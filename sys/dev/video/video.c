/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 * Copyright (c) 2008 Robert Nagy <robert@openbsd.org>
 * Copyright (c) 2008 Marcus Glocker <mglocker@openbsd.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause AND ISC
 *
 * The cdev, kqfilter and buffer-pool handling are derived from uvideo(4).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/selinfo.h>
#include <sys/sx.h>
#include <sys/uio.h>
#include <sys/videoio.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/uma.h>

#include <dev/video/video_internal.h>

#include "video_if.h"

MALLOC_DEFINE(M_VIDEO, "video", "video(4) framework");

#define	VIDEO_UNIT_RETRIES	32

static struct unrhdr *video_unrhdr;

static d_open_t		video_open;
static d_close_t	video_close;
static d_read_t		video_read;
static d_ioctl_t	video_ioctl;
static d_poll_t		video_poll;
static d_kqfilter_t	video_kqfilter;
static d_mmap_single_t	video_mmap_single;

static struct cdevsw video_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	video_open,
	.d_close =	video_close,
	.d_read =	video_read,
	.d_ioctl =	video_ioctl,
	.d_poll =	video_poll,
	.d_kqfilter =	video_kqfilter,
	.d_mmap_single = video_mmap_single,
	.d_name =	"video",
};

static struct video_buf_pool *
video_pool_alloc(u_int nbufs, size_t buf_size)
{
	struct video_buf_pool *pool;
	struct video_buf *vb;
	size_t map_size;
	vm_object_t obj;
	vm_offset_t kva;
	u_int i;
	int error;

	buf_size = round_page(buf_size);
	if (buf_size == 0 || nbufs == 0 || nbufs > VIDEO_MAX_BUFFERS)
		return (NULL);
	if (SIZE_MAX / nbufs < buf_size)
		return (NULL);
	map_size = (size_t)nbufs * buf_size;

	/*
	 * Use phys_pager_allocate() rather than a bare vm_object_allocate():
	 * the latter leaves un_pager.phys.ops NULL, which faults when the VM
	 * system calls phys_pager_getpages() from vm_map_wire() or from a
	 * userspace fault on the mapping.
	 */
	obj = phys_pager_allocate(NULL, &default_phys_pg_ops, NULL, map_size,
	    VM_PROT_ALL, 0, curthread->td_ucred);
	if (obj == NULL)
		return (NULL);

	kva = vm_map_min(kernel_map);
	error = vm_map_find(kernel_map, obj, 0, &kva, map_size, 0,
	    VMFS_OPTIMAL_SPACE, VM_PROT_READ | VM_PROT_WRITE,
	    VM_PROT_READ | VM_PROT_WRITE, 0);
	if (error != KERN_SUCCESS) {
		vm_object_deallocate(obj);
		return (NULL);
	}
	error = vm_map_wire(kernel_map, kva, kva + map_size,
	    VM_MAP_WIRE_SYSTEM | VM_MAP_WIRE_NOHOLES);
	if (error != KERN_SUCCESS) {
		vm_map_remove(kernel_map, kva, kva + map_size);
		return (NULL);
	}

	pool = malloc(sizeof(*pool), M_VIDEO, M_WAITOK | M_ZERO);
	pool->obj = obj;
	pool->kva = kva;
	pool->nbufs = nbufs;
	pool->buf_size = buf_size;
	pool->map_size = map_size;

	for (i = 0; i < nbufs; i++) {
		vb = &pool->bufs[i];
		vb->pool = pool;
		vb->index = i;
		vb->state = VB_IDLE;
		vb->length = buf_size;
		vb->buf = (uint8_t *)kva + (size_t)i * buf_size;
	}
	return (pool);
}

static void
video_pool_free(struct video_buf_pool *pool)
{

	/*
	 * Removing the kernel mapping drops the map's reference on the
	 * object.  Any surviving userspace mapping holds its own reference,
	 * so the frames stay alive until the last one goes away.
	 */
	vm_map_remove(kernel_map, pool->kva, pool->kva + pool->map_size);
	free(pool, M_VIDEO);
}

static void
video_pool_detach(struct video_device *vd)
{
	struct video_buf_pool *pool;

	mtx_lock(&vd->mtx);
	if (vd->owner != NULL) {
		vd->owner->read_buf = NULL;
		vd->owner->read_offset = 0;
	}
	while (vd->readers > 0)
		msleep(&vd->readers, &vd->mtx, 0, "vdrain", 0);
	pool = vd->pool;
	vd->pool = NULL;
	STAILQ_INIT(&vd->queued);
	STAILQ_INIT(&vd->done);
	mtx_unlock(&vd->mtx);

	if (pool != NULL)
		video_pool_free(pool);
}

static void
video_file_dtor(void *arg)
{
	struct video_file *vf = arg;
	struct video_device *vd = vf->vd;

	sx_xlock(&vd->cfg_sx);

	if (vf->is_owner) {
		if (vd->streaming) {
			mtx_lock(&vd->mtx);
			vd->stopping = true;
			mtx_unlock(&vd->mtx);

			VIDEO_STOP_STREAM(vd->dev);

			mtx_lock(&vd->mtx);
			vd->streaming = false;
			vd->stopping = false;
			STAILQ_INIT(&vd->queued);
			STAILQ_INIT(&vd->done);
			mtx_unlock(&vd->mtx);
		}

		video_pool_detach(vd);

		mtx_lock(&vd->mtx);
		vd->owner = NULL;
		mtx_unlock(&vd->mtx);
		vd->mode = VMODE_NONE;

		VIDEO_CLOSE(vd->dev);
	}

	sx_xunlock(&vd->cfg_sx);

	free(vf, M_VIDEO);
}

static int
video_open(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct video_device *vd = dev->si_drv1;
	struct video_file *vf;

	if (vd == NULL || vd->dying)
		return (ENXIO);

	vf = malloc(sizeof(*vf), M_VIDEO, M_WAITOK | M_ZERO);
	vf->vd = vd;

	return (devfs_set_cdevpriv(vf, video_file_dtor));
}

static int
video_close(struct cdev *dev, int flags, int fmt, struct thread *td)
{

	return (0);
}

static int
video_claim_owner(struct video_device *vd, struct video_file *vf)
{
	int error;

	sx_assert(&vd->cfg_sx, SA_XLOCKED);

	if (vd->dying)
		return (ENXIO);
	if (vd->owner == vf)
		return (0);
	if (vd->owner != NULL)
		return (EBUSY);

	error = VIDEO_OPEN(vd->dev);
	if (error != 0)
		return (error);

	mtx_lock(&vd->mtx);
	vd->owner = vf;
	mtx_unlock(&vd->mtx);
	vf->is_owner = true;
	return (0);
}

struct video_buf *
video_buf_acquire(struct video_device *vd)
{
	struct video_buf *vb;

	mtx_lock(&vd->mtx);
	vb = STAILQ_FIRST(&vd->queued);
	if (vb != NULL) {
		STAILQ_REMOVE_HEAD(&vd->queued, entry);
		vb->state = VB_ACTIVE;
		vb->bytesused = 0;
	}
	mtx_unlock(&vd->mtx);
	return (vb);
}

int
video_buf_write(struct video_buf *vb, size_t offset, const void *src,
    size_t len)
{

	if (offset > vb->length || len > vb->length - offset)
		return (ENOSPC);

	memcpy((uint8_t *)vb->buf + offset, src, len);
	return (0);
}

void
video_buf_done(struct video_buf *vb, size_t bytesused, uint32_t sequence)
{
	struct video_device *vd;
	bool overrun;

	overrun = bytesused > vb->length;
	if (overrun)
		bytesused = 0;

	vd = vb->vd;
	mtx_lock(&vd->mtx);
	if (vb->state != VB_ACTIVE) {
		mtx_unlock(&vd->mtx);
		return;
	}
	vb->bytesused = bytesused;
	vb->sequence = sequence;
	getmicrouptime(&vb->timestamp);
	vb->flags = V4L2_BUF_FLAG_DONE | V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	if (overrun)
		vb->flags |= V4L2_BUF_FLAG_ERROR;
	vb->state = VB_DONE;
	STAILQ_INSERT_TAIL(&vd->done, vb, entry);
	selwakeup(&vd->sel);
	wakeup(&vd->done);
	mtx_unlock(&vd->mtx);
}

void
video_buf_error(struct video_buf *vb)
{
	struct video_device *vd = vb->vd;

	mtx_lock(&vd->mtx);
	if (vb->state != VB_ACTIVE) {
		mtx_unlock(&vd->mtx);
		return;
	}
	vb->bytesused = 0;
	vb->flags = V4L2_BUF_FLAG_ERROR;
	getmicrouptime(&vb->timestamp);
	vb->state = VB_ERROR;
	STAILQ_INSERT_TAIL(&vd->done, vb, entry);
	selwakeup(&vd->sel);
	wakeup(&vd->done);
	mtx_unlock(&vd->mtx);
}

static int
video_querycap(struct video_device *vd, struct v4l2_capability *cap)
{
	struct video_caps vc;
	int error;

	memset(&vc, 0, sizeof(vc));
	error = VIDEO_QUERYCAP(vd->dev, &vc);
	if (error != 0)
		return (error);

	memset(cap, 0, sizeof(*cap));
	strlcpy((char *)cap->driver, vc.driver, sizeof(cap->driver));
	strlcpy((char *)cap->card, vc.card, sizeof(cap->card));
	strlcpy((char *)cap->bus_info, vc.bus_info, sizeof(cap->bus_info));
	cap->version = vc.version;
	cap->device_caps = vc.capabilities;
	cap->capabilities = vc.capabilities | V4L2_CAP_DEVICE_CAPS;
	return (0);
}

static int
video_enum_fmt(struct video_device *vd, struct v4l2_fmtdesc *fd)
{
	struct video_format vf;
	int error;

	if (fd->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return (EINVAL);

	memset(&vf, 0, sizeof(vf));
	error = VIDEO_ENUM_FORMAT(vd->dev, fd->index, &vf);
	if (error != 0)
		return (error);

	fd->pixelformat = vf.pixelformat;
	fd->flags = vf.flags;
	strlcpy((char *)fd->description, vf.description,
	    sizeof(fd->description));
	return (0);
}

static int
video_g_fmt(struct video_device *vd, struct v4l2_format *f)
{
	struct video_format vf;
	int error;

	if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return (EINVAL);

	memset(&vf, 0, sizeof(vf));
	error = VIDEO_GET_FORMAT(vd->dev, &vf);
	if (error != 0)
		return (error);

	memset(&f->fmt.pix, 0, sizeof(f->fmt.pix));
	f->fmt.pix.width = vf.width;
	f->fmt.pix.height = vf.height;
	f->fmt.pix.pixelformat = vf.pixelformat;
	f->fmt.pix.field = vf.field;
	f->fmt.pix.bytesperline = vf.bytesperline;
	f->fmt.pix.sizeimage = vf.sizeimage;
	f->fmt.pix.colorspace = vf.colorspace;
	f->fmt.pix.xfer_func = vf.xfer_func;
	f->fmt.pix.ycbcr_enc = vf.ycbcr_enc;
	f->fmt.pix.flags = vf.flags;
	return (0);
}

static int
video_s_fmt(struct video_device *vd, struct video_file *vf,
    struct v4l2_format *f)
{
	struct video_format fmt;
	int error;

	if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return (EINVAL);

	sx_xlock(&vd->cfg_sx);
	error = video_claim_owner(vd, vf);
	if (error != 0) {
		sx_xunlock(&vd->cfg_sx);
		return (error);
	}
	if (vd->streaming) {
		sx_xunlock(&vd->cfg_sx);
		return (EBUSY);
	}

	memset(&fmt, 0, sizeof(fmt));
	fmt.width = f->fmt.pix.width;
	fmt.height = f->fmt.pix.height;
	fmt.pixelformat = f->fmt.pix.pixelformat;
	fmt.field = f->fmt.pix.field;

	error = VIDEO_SET_FORMAT(vd->dev, &fmt);
	if (error == 0) {
		VIDEO_GET_FORMAT(vd->dev, &vd->format);

		f->fmt.pix.width = vd->format.width;
		f->fmt.pix.height = vd->format.height;
		f->fmt.pix.pixelformat = vd->format.pixelformat;
		f->fmt.pix.field = vd->format.field;
		f->fmt.pix.bytesperline = vd->format.bytesperline;
		f->fmt.pix.sizeimage = vd->format.sizeimage;
		f->fmt.pix.colorspace = vd->format.colorspace;
	}
	sx_xunlock(&vd->cfg_sx);
	return (error);
}

static int
video_try_fmt(struct video_device *vd, struct v4l2_format *f)
{
	struct video_format fmt;
	int error;

	if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return (EINVAL);
	memset(&fmt, 0, sizeof(fmt));
	fmt.width = f->fmt.pix.width;
	fmt.height = f->fmt.pix.height;
	fmt.pixelformat = f->fmt.pix.pixelformat;
	fmt.field = f->fmt.pix.field;

	error = VIDEO_TRY_FORMAT(vd->dev, &fmt);
	if (error == 0) {
		f->fmt.pix.width = fmt.width;
		f->fmt.pix.height = fmt.height;
		f->fmt.pix.pixelformat = fmt.pixelformat;
		f->fmt.pix.field = fmt.field;
		f->fmt.pix.bytesperline = fmt.bytesperline;
		f->fmt.pix.sizeimage = fmt.sizeimage;
		f->fmt.pix.colorspace = fmt.colorspace;
	}
	return (error);
}

static int
video_reqbufs(struct video_device *vd, struct video_file *vf,
    struct v4l2_requestbuffers *rb)
{
	struct video_buf_pool *pool;
	int error;

	if (rb->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return (EINVAL);
	if (rb->memory != V4L2_MEMORY_MMAP)
		return (EINVAL);

	sx_xlock(&vd->cfg_sx);
	error = video_claim_owner(vd, vf);
	if (error != 0) {
		sx_xunlock(&vd->cfg_sx);
		return (error);
	}
	if (vd->streaming) {
		sx_xunlock(&vd->cfg_sx);
		return (EBUSY);
	}

	video_pool_detach(vd);

	if (rb->count == 0) {
		vd->mode = VMODE_NONE;
		sx_xunlock(&vd->cfg_sx);
		rb->capabilities = V4L2_BUF_CAP_SUPPORTS_MMAP;
		return (0);
	}

	if (rb->count > VIDEO_MAX_BUFFERS)
		rb->count = VIDEO_MAX_BUFFERS;
	if (rb->count < 2)
		rb->count = 2;

	if (vd->format.sizeimage == 0) {
		error = VIDEO_GET_FORMAT(vd->dev, &vd->format);
		if (error != 0 || vd->format.sizeimage == 0) {
			sx_xunlock(&vd->cfg_sx);
			return (error != 0 ? error : EINVAL);
		}
	}

	pool = video_pool_alloc(rb->count, vd->format.sizeimage);
	if (pool == NULL) {
		sx_xunlock(&vd->cfg_sx);
		return (ENOMEM);
	}
	mtx_lock(&vd->mtx);
	vd->pool = pool;
	mtx_unlock(&vd->mtx);

	vd->mode = VMODE_MMAP;
	rb->capabilities = V4L2_BUF_CAP_SUPPORTS_MMAP;
	sx_xunlock(&vd->cfg_sx);
	return (0);
}

static int
video_querybuf(struct video_device *vd, struct video_file *vf,
    struct v4l2_buffer *b)
{
	struct video_buf *vb;

	if (b->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return (EINVAL);

	mtx_lock(&vd->mtx);
	if (vd->owner != vf) {
		mtx_unlock(&vd->mtx);
		return (EPERM);
	}
	if (vd->pool == NULL || b->index >= vd->pool->nbufs) {
		mtx_unlock(&vd->mtx);
		return (EINVAL);
	}

	vb = &vd->pool->bufs[b->index];
	b->memory = V4L2_MEMORY_MMAP;
	b->length = vb->length;
	b->m.offset = vb->index * vd->pool->buf_size;
	b->bytesused = vb->bytesused;
	b->field = V4L2_FIELD_NONE;
	b->timestamp = vb->timestamp;
	b->sequence = vb->sequence;
	b->flags = 0;

	switch (vb->state) {
	case VB_QUEUED:
	case VB_ACTIVE:
		b->flags |= V4L2_BUF_FLAG_QUEUED;
		break;
	case VB_DONE:
		b->flags |= V4L2_BUF_FLAG_DONE;
		break;
	case VB_ERROR:
		b->flags |= V4L2_BUF_FLAG_DONE | V4L2_BUF_FLAG_ERROR;
		break;
	default:
		break;
	}
	b->flags |= V4L2_BUF_FLAG_MAPPED | V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	mtx_unlock(&vd->mtx);
	return (0);
}

static int
video_qbuf(struct video_device *vd, struct video_file *vf,
    struct v4l2_buffer *b)
{
	struct video_buf *vb;

	if (b->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return (EINVAL);
	if (b->memory != V4L2_MEMORY_MMAP)
		return (EINVAL);

	mtx_lock(&vd->mtx);
	if (vd->owner != vf) {
		mtx_unlock(&vd->mtx);
		return (EPERM);
	}
	if (vd->pool == NULL || vd->mode != VMODE_MMAP ||
	    b->index >= vd->pool->nbufs) {
		mtx_unlock(&vd->mtx);
		return (EINVAL);
	}

	vb = &vd->pool->bufs[b->index];
	if (vb->state != VB_IDLE) {
		mtx_unlock(&vd->mtx);
		return (EINVAL);
	}

	vb->state = VB_QUEUED;
	vb->bytesused = 0;
	vb->vd = vd;
	STAILQ_INSERT_TAIL(&vd->queued, vb, entry);
	mtx_unlock(&vd->mtx);

	b->flags = V4L2_BUF_FLAG_QUEUED | V4L2_BUF_FLAG_MAPPED;
	return (0);
}

static int
video_dqbuf(struct video_device *vd, struct video_file *vf,
    struct v4l2_buffer *b, int flags)
{
	struct video_buf *vb;
	int error;

	if (b->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return (EINVAL);
	if (b->memory != V4L2_MEMORY_MMAP)
		return (EINVAL);

	mtx_lock(&vd->mtx);
	if (vd->owner != vf) {
		mtx_unlock(&vd->mtx);
		return (EPERM);
	}
	while (STAILQ_EMPTY(&vd->done)) {
		if (!vd->streaming || vd->stopping) {
			mtx_unlock(&vd->mtx);
			return (EINVAL);
		}
		if (flags & O_NONBLOCK) {
			mtx_unlock(&vd->mtx);
			return (EAGAIN);
		}
		error = msleep(&vd->done, &vd->mtx, PCATCH, "vdqbuf", 0);
		if (error != 0) {
			mtx_unlock(&vd->mtx);
			return (error);
		}
	}

	vb = STAILQ_FIRST(&vd->done);
	STAILQ_REMOVE_HEAD(&vd->done, entry);

	b->index = vb->index;
	b->bytesused = vb->bytesused;
	b->field = V4L2_FIELD_NONE;
	b->timestamp = vb->timestamp;
	b->sequence = vb->sequence;
	b->memory = V4L2_MEMORY_MMAP;
	b->m.offset = vb->index * vd->pool->buf_size;
	b->length = vb->length;
	b->flags = V4L2_BUF_FLAG_MAPPED | V4L2_BUF_FLAG_DONE |
	    V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	if (vb->flags & V4L2_BUF_FLAG_ERROR)
		b->flags |= V4L2_BUF_FLAG_ERROR;

	vb->state = VB_IDLE;
	mtx_unlock(&vd->mtx);

	return (0);
}

static int
video_streamon(struct video_device *vd, struct video_file *vf, int *type)
{
	int error;

	if (*type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return (EINVAL);

	sx_xlock(&vd->cfg_sx);
	error = video_claim_owner(vd, vf);
	if (error != 0) {
		sx_xunlock(&vd->cfg_sx);
		return (error);
	}
	if (vd->streaming) {
		sx_xunlock(&vd->cfg_sx);
		return (0);
	}
	if (vd->pool == NULL || vd->mode != VMODE_MMAP) {
		sx_xunlock(&vd->cfg_sx);
		return (EINVAL);
	}

	error = VIDEO_START_STREAM(vd->dev);
	if (error != 0) {
		sx_xunlock(&vd->cfg_sx);
		return (error);
	}

	mtx_lock(&vd->mtx);
	vd->streaming = true;
	mtx_unlock(&vd->mtx);

	sx_xunlock(&vd->cfg_sx);
	return (0);
}

static int
video_streamoff(struct video_device *vd, struct video_file *vf, int *type)
{
	struct video_buf *vb;
	u_int i;

	if (*type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return (EINVAL);

	sx_xlock(&vd->cfg_sx);
	if (vd->owner != vf) {
		sx_xunlock(&vd->cfg_sx);
		return (EBUSY);
	}
	if (!vd->streaming) {
		sx_xunlock(&vd->cfg_sx);
		return (0);
	}

	mtx_lock(&vd->mtx);
	vd->stopping = true;
	wakeup(&vd->done);
	mtx_unlock(&vd->mtx);

	VIDEO_STOP_STREAM(vd->dev);

	mtx_lock(&vd->mtx);
	vd->streaming = false;
	vd->stopping = false;
	STAILQ_INIT(&vd->queued);
	STAILQ_INIT(&vd->done);
	for (i = 0; i < vd->pool->nbufs; i++) {
		vb = &vd->pool->bufs[i];
		vb->state = VB_IDLE;
		vb->bytesused = 0;
		vb->vd = NULL;
	}
	vf->read_buf = NULL;
	vf->read_offset = 0;
	mtx_unlock(&vd->mtx);

	sx_xunlock(&vd->cfg_sx);
	return (0);
}

static int
video_g_parm(struct video_device *vd, struct v4l2_streamparm *sp)
{
	struct video_fract fract;
	int error;

	if (sp->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return (EINVAL);
	memset(&fract, 0, sizeof(fract));
	error = VIDEO_GET_PARM(vd->dev, &fract);
	if (error != 0)
		return (error);

	memset(&sp->parm.capture, 0, sizeof(sp->parm.capture));
	sp->parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
	sp->parm.capture.timeperframe.numerator = fract.numerator;
	sp->parm.capture.timeperframe.denominator = fract.denominator;
	return (0);
}

static int
video_s_parm(struct video_device *vd, struct v4l2_streamparm *sp)
{
	struct video_fract fract;
	int error;

	if (sp->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return (EINVAL);
	fract.numerator = sp->parm.capture.timeperframe.numerator;
	fract.denominator = sp->parm.capture.timeperframe.denominator;

	error = VIDEO_SET_PARM(vd->dev, &fract);
	if (error != 0)
		return (error);

	sp->parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
	sp->parm.capture.timeperframe.numerator = fract.numerator;
	sp->parm.capture.timeperframe.denominator = fract.denominator;
	return (0);
}

static int
video_queryctrl(struct video_device *vd, struct v4l2_queryctrl *qc)
{
	struct video_control_desc desc;
	int error;

	memset(&desc, 0, sizeof(desc));
	desc.id = qc->id;
	error = VIDEO_QUERY_CONTROL(vd->dev, &desc);
	if (error != 0)
		return (error);

	qc->id = desc.id;
	qc->type = desc.type;
	strlcpy((char *)qc->name, desc.name, sizeof(qc->name));
	qc->minimum = desc.minimum;
	qc->maximum = desc.maximum;
	qc->step = desc.step;
	qc->default_value = desc.default_value;
	qc->flags = desc.flags;
	return (0);
}

static int
video_g_ctrl(struct video_device *vd, struct v4l2_control *c)
{
	struct video_control vc;
	int error;

	vc.id = c->id;
	vc.value = 0;
	error = VIDEO_GET_CONTROL(vd->dev, &vc);
	if (error == 0)
		c->value = vc.value;
	return (error);
}

static int
video_s_ctrl(struct video_device *vd, struct v4l2_control *c)
{
	struct video_control vc;

	vc.id = c->id;
	vc.value = c->value;
	return (VIDEO_SET_CONTROL(vd->dev, &vc));
}

static int
video_enum_framesizes(struct video_device *vd, struct v4l2_frmsizeenum *fs)
{
	struct video_frmsizeenum vfs;
	int error;

	memset(&vfs, 0, sizeof(vfs));
	vfs.index = fs->index;
	vfs.pixelformat = fs->pixel_format;
	error = VIDEO_ENUM_FRAMESIZES(vd->dev, &vfs);
	if (error != 0)
		return (error);

	fs->type = vfs.type;
	if (vfs.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
		fs->discrete.width = vfs.discrete.width;
		fs->discrete.height = vfs.discrete.height;
	} else {
		fs->stepwise.min_width = vfs.stepwise.min_width;
		fs->stepwise.max_width = vfs.stepwise.max_width;
		fs->stepwise.step_width = vfs.stepwise.step_width;
		fs->stepwise.min_height = vfs.stepwise.min_height;
		fs->stepwise.max_height = vfs.stepwise.max_height;
		fs->stepwise.step_height = vfs.stepwise.step_height;
	}
	return (0);
}

static int
video_enum_frameintervals(struct video_device *vd,
    struct v4l2_frmivalenum *fi)
{
	struct video_frmivalenum vfi;
	int error;

	memset(&vfi, 0, sizeof(vfi));
	vfi.index = fi->index;
	vfi.pixelformat = fi->pixel_format;
	vfi.width = fi->width;
	vfi.height = fi->height;
	error = VIDEO_ENUM_FRAMEINTERVALS(vd->dev, &vfi);
	if (error != 0)
		return (error);

	fi->type = vfi.type;
	if (vfi.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
		fi->discrete.numerator = vfi.discrete.numerator;
		fi->discrete.denominator = vfi.discrete.denominator;
	} else {
		fi->stepwise.min.numerator = vfi.stepwise.min.numerator;
		fi->stepwise.min.denominator = vfi.stepwise.min.denominator;
		fi->stepwise.max.numerator = vfi.stepwise.max.numerator;
		fi->stepwise.max.denominator = vfi.stepwise.max.denominator;
		fi->stepwise.step.numerator = vfi.stepwise.step.numerator;
		fi->stepwise.step.denominator = vfi.stepwise.step.denominator;
	}
	return (0);
}

static int
video_enum_input(struct video_device *vd, struct v4l2_input *inp)
{
	struct video_input vi;
	int error;

	memset(&vi, 0, sizeof(vi));
	error = VIDEO_ENUM_INPUT(vd->dev, inp->index, &vi);
	if (error != 0)
		return (error);

	strlcpy((char *)inp->name, vi.name, sizeof(inp->name));
	inp->type = vi.type;
	return (0);
}

static int
video_g_input(struct video_device *vd, int *index)
{
	uint32_t idx;
	int error;

	error = VIDEO_GET_INPUT(vd->dev, &idx);
	if (error == 0)
		*index = idx;
	return (error);
}

static int
video_s_input(struct video_device *vd, int *index)
{

	return (VIDEO_SET_INPUT(vd->dev, *index));
}

static int
video_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct video_device *vd = dev->si_drv1;
	struct video_file *vf;
	int error;

	if (vd == NULL || vd->dying)
		return (ENXIO);

	error = devfs_get_cdevpriv((void **)&vf);
	if (error != 0)
		return (error);

	switch (cmd) {
	case VIDIOC_QUERYCAP:
		return (video_querycap(vd, (struct v4l2_capability *)data));
	case VIDIOC_ENUM_FMT:
		return (video_enum_fmt(vd, (struct v4l2_fmtdesc *)data));
	case VIDIOC_G_FMT:
		return (video_g_fmt(vd, (struct v4l2_format *)data));
	case VIDIOC_S_FMT:
		return (video_s_fmt(vd, vf, (struct v4l2_format *)data));
	case VIDIOC_TRY_FMT:
		return (video_try_fmt(vd, (struct v4l2_format *)data));
	case VIDIOC_REQBUFS:
		return (video_reqbufs(vd, vf,
		    (struct v4l2_requestbuffers *)data));
	case VIDIOC_QUERYBUF:
		return (video_querybuf(vd, vf, (struct v4l2_buffer *)data));
	case VIDIOC_QBUF:
		return (video_qbuf(vd, vf, (struct v4l2_buffer *)data));
	case VIDIOC_DQBUF:
		return (video_dqbuf(vd, vf, (struct v4l2_buffer *)data,
		    fflag));
	case VIDIOC_STREAMON:
		return (video_streamon(vd, vf, (int *)data));
	case VIDIOC_STREAMOFF:
		return (video_streamoff(vd, vf, (int *)data));
	case VIDIOC_G_PARM:
		return (video_g_parm(vd, (struct v4l2_streamparm *)data));
	case VIDIOC_S_PARM:
		return (video_s_parm(vd, (struct v4l2_streamparm *)data));
	case VIDIOC_QUERYCTRL:
		return (video_queryctrl(vd, (struct v4l2_queryctrl *)data));
	case VIDIOC_G_CTRL:
		return (video_g_ctrl(vd, (struct v4l2_control *)data));
	case VIDIOC_S_CTRL:
		return (video_s_ctrl(vd, (struct v4l2_control *)data));
	case VIDIOC_ENUMINPUT:
		return (video_enum_input(vd, (struct v4l2_input *)data));
	case VIDIOC_G_INPUT:
		return (video_g_input(vd, (int *)data));
	case VIDIOC_S_INPUT:
		return (video_s_input(vd, (int *)data));
	case VIDIOC_ENUM_FRAMESIZES:
		return (video_enum_framesizes(vd,
		    (struct v4l2_frmsizeenum *)data));
	case VIDIOC_ENUM_FRAMEINTERVALS:
		return (video_enum_frameintervals(vd,
		    (struct v4l2_frmivalenum *)data));
	default:
		return (ENOTTY);
	}
}

static int
video_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct video_device *vd = dev->si_drv1;
	struct video_buf_pool *pool;
	struct video_file *vf;
	struct video_buf *vb;
	uint8_t *src;
	size_t bytesused, chunk, offset;
	int error;

	if (vd == NULL || vd->dying)
		return (ENXIO);

	error = devfs_get_cdevpriv((void **)&vf);
	if (error != 0)
		return (error);

	mtx_lock(&vd->mtx);
	while (vf->reading) {
		error = msleep(&vf->reading, &vd->mtx, PCATCH, "vrdser", 0);
		if (error != 0) {
			mtx_unlock(&vd->mtx);
			return (error);
		}
	}
	vf->reading = true;
	mtx_unlock(&vd->mtx);

	sx_xlock(&vd->cfg_sx);
	error = video_claim_owner(vd, vf);
	if (error != 0) {
		sx_xunlock(&vd->cfg_sx);
		goto out;
	}

	if (!vd->streaming) {
		if (vd->mode == VMODE_MMAP) {
			sx_xunlock(&vd->cfg_sx);
			error = EBUSY;
			goto out;
		}

		if (vd->format.sizeimage == 0) {
			error = VIDEO_GET_FORMAT(vd->dev, &vd->format);
			if (error != 0 || vd->format.sizeimage == 0) {
				sx_xunlock(&vd->cfg_sx);
				if (error == 0)
					error = EIO;
				goto out;
			}
		}

		if (vd->pool == NULL) {
			pool = video_pool_alloc(VIDEO_READ_BUFFERS,
			    vd->format.sizeimage);
			if (pool == NULL) {
				sx_xunlock(&vd->cfg_sx);
				error = ENOMEM;
				goto out;
			}
			mtx_lock(&vd->mtx);
			vd->pool = pool;
			mtx_unlock(&vd->mtx);
		}
		vd->mode = VMODE_READ;

		mtx_lock(&vd->mtx);
		for (u_int i = 0; i < vd->pool->nbufs; i++) {
			vb = &vd->pool->bufs[i];
			vb->state = VB_QUEUED;
			vb->vd = vd;
			STAILQ_INSERT_TAIL(&vd->queued, vb, entry);
		}
		mtx_unlock(&vd->mtx);

		error = VIDEO_START_STREAM(vd->dev);
		if (error != 0) {
			video_pool_detach(vd);
			vd->mode = VMODE_NONE;
			sx_xunlock(&vd->cfg_sx);
			goto out;
		}

		mtx_lock(&vd->mtx);
		vd->streaming = true;
		mtx_unlock(&vd->mtx);
	}
	sx_xunlock(&vd->cfg_sx);

	mtx_lock(&vd->mtx);
	while (vf->read_buf == NULL) {
		if (!STAILQ_EMPTY(&vd->done)) {
			vf->read_buf = STAILQ_FIRST(&vd->done);
			STAILQ_REMOVE_HEAD(&vd->done, entry);
			vf->read_offset = 0;
			break;
		}
		if (!vd->streaming || vd->stopping) {
			mtx_unlock(&vd->mtx);
			error = EIO;
			goto out;
		}
		if (ioflag & O_NONBLOCK) {
			mtx_unlock(&vd->mtx);
			error = EAGAIN;
			goto out;
		}
		error = msleep(&vd->done, &vd->mtx, PCATCH, "vread", 0);
		if (error != 0) {
			mtx_unlock(&vd->mtx);
			goto out;
		}
	}

	vb = vf->read_buf;
	bytesused = vb->bytesused;
	offset = vf->read_offset;
	chunk = 0;
	if (uio->uio_resid > 0 && offset < bytesused) {
		chunk = MIN((size_t)uio->uio_resid, bytesused - offset);
		src = (uint8_t *)vb->buf + offset;
		vd->readers++;
		mtx_unlock(&vd->mtx);

		error = uiomove(src, chunk, uio);

		mtx_lock(&vd->mtx);
		if (--vd->readers == 0)
			wakeup(&vd->readers);
		if (error != 0) {
			mtx_unlock(&vd->mtx);
			goto out;
		}
	}

	if (vf->read_buf == vb) {
		vf->read_offset = offset + chunk;
		if (vf->read_offset >= bytesused) {
			vf->read_buf = NULL;
			vf->read_offset = 0;
			vb->state = VB_QUEUED;
			vb->bytesused = 0;
			STAILQ_INSERT_TAIL(&vd->queued, vb, entry);
		}
	}
	mtx_unlock(&vd->mtx);

	error = 0;
out:
	mtx_lock(&vd->mtx);
	vf->reading = false;
	wakeup(&vf->reading);
	mtx_unlock(&vd->mtx);
	return (error);
}

static int
video_poll(struct cdev *dev, int events, struct thread *td)
{
	struct video_device *vd = dev->si_drv1;
	int revents = 0;

	if (vd == NULL || vd->dying)
		return (POLLHUP);

	mtx_lock(&vd->mtx);
	if (events & (POLLIN | POLLRDNORM)) {
		if (!STAILQ_EMPTY(&vd->done))
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(td, &vd->sel);
	}
	mtx_unlock(&vd->mtx);
	return (revents);
}

static void video_kqfilter_detach(struct knote *kn);
static int video_kqfilter_event(struct knote *kn, long hint);

static const struct filterops video_read_filterops = {
	.f_isfd =	1,
	.f_detach =	video_kqfilter_detach,
	.f_event =	video_kqfilter_event,
};

static void
video_kqfilter_detach(struct knote *kn)
{
	struct video_device *vd = kn->kn_hook;

	knlist_remove(&vd->sel.si_note, kn, 0);
}

static int
video_kqfilter_event(struct knote *kn, long hint)
{
	struct video_device *vd = kn->kn_hook;

	if (!STAILQ_EMPTY(&vd->done)) {
		kn->kn_data = 1;
		return (1);
	}
	return (0);
}

static int
video_kqfilter(struct cdev *dev, struct knote *kn)
{
	struct video_device *vd = dev->si_drv1;

	if (vd == NULL || vd->dying)
		return (ENXIO);

	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &video_read_filterops;
		kn->kn_hook = vd;
		knlist_add(&vd->sel.si_note, kn, 0);
		return (0);
	default:
		return (EINVAL);
	}
}

static int
video_mmap_single(struct cdev *dev, vm_ooffset_t *offset, vm_size_t size,
    vm_object_t *objp, int nprot)
{
	struct video_device *vd = dev->si_drv1;
	struct video_file *vf;
	int error;

	if (vd == NULL || vd->dying)
		return (ENXIO);

	error = devfs_get_cdevpriv((void **)&vf);
	if (error != 0)
		return (error);

	sx_slock(&vd->cfg_sx);
	if (vd->owner != vf || vd->pool == NULL || vd->mode != VMODE_MMAP) {
		sx_sunlock(&vd->cfg_sx);
		return (EINVAL);
	}

	if (*offset >= vd->pool->map_size ||
	    size > vd->pool->map_size - *offset) {
		sx_sunlock(&vd->cfg_sx);
		return (EINVAL);
	}

	/*
	 * Hand out a reference to the object backing the whole pool; the
	 * offset selects which buffer within it gets mapped.  The VM system
	 * tracks the mapping lifetime through the object reference count,
	 * so no per-mapping bookkeeping is needed here.
	 */
	vm_object_reference(vd->pool->obj);
	*objp = vd->pool->obj;
	sx_sunlock(&vd->cfg_sx);

	return (0);
}

int
video_register(device_t dev, struct video_device **vdp)
{
	struct make_dev_args args;
	struct video_device *vd;
	int taken[VIDEO_UNIT_RETRIES];
	int collisions, error, unit;

	vd = malloc(sizeof(*vd), M_VIDEO, M_WAITOK | M_ZERO);
	vd->dev = dev;

	sx_init(&vd->cfg_sx, "video_cfg");
	mtx_init(&vd->mtx, "video_mtx", NULL, MTX_DEF);
	STAILQ_INIT(&vd->queued);
	STAILQ_INIT(&vd->done);
	knlist_init_mtx(&vd->sel.si_note, &vd->mtx);

	error = EEXIST;
	for (collisions = 0; collisions < VIDEO_UNIT_RETRIES; collisions++) {
		taken[collisions] = unit = alloc_unr(video_unrhdr);

		make_dev_args_init(&args);
		args.mda_flags = MAKEDEV_CHECKNAME | MAKEDEV_WAITOK;
		args.mda_devsw = &video_cdevsw;
		args.mda_uid = UID_ROOT;
		args.mda_gid = GID_VIDEO;
		args.mda_mode = 0660;
		args.mda_unit = unit;
		args.mda_si_drv1 = vd;

		error = make_dev_s(&args, &vd->cdev, "video%d", unit);
		if (error != EEXIST)
			break;
	}

	while (collisions > 0)
		free_unr(video_unrhdr, taken[--collisions]);

	if (error != 0) {
		free_unr(video_unrhdr, unit);
		device_printf(dev, "failed to create a /dev/video node: %d\n",
		    error);
		knlist_destroy(&vd->sel.si_note);
		mtx_destroy(&vd->mtx);
		sx_destroy(&vd->cfg_sx);
		free(vd, M_VIDEO);
		return (error);
	}
	vd->unit = unit;

	*vdp = vd;
	device_printf(dev, "registered as /dev/video%d\n", unit);
	return (0);
}

void
video_unregister(struct video_device *vd)
{

	sx_xlock(&vd->cfg_sx);
	vd->dying = true;

	if (vd->streaming) {
		mtx_lock(&vd->mtx);
		vd->stopping = true;
		wakeup(&vd->done);
		mtx_unlock(&vd->mtx);

		VIDEO_STOP_STREAM(vd->dev);

		mtx_lock(&vd->mtx);
		vd->streaming = false;
		vd->stopping = false;
		STAILQ_INIT(&vd->queued);
		STAILQ_INIT(&vd->done);
		mtx_unlock(&vd->mtx);
	}

	sx_xunlock(&vd->cfg_sx);

	destroy_dev(vd->cdev);
	free_unr(video_unrhdr, vd->unit);

	video_pool_detach(vd);

	knlist_destroy(&vd->sel.si_note);
	mtx_destroy(&vd->mtx);
	sx_destroy(&vd->cfg_sx);

	free(vd, M_VIDEO);
}

static int
video_modevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		video_unrhdr = new_unrhdr(0, INT_MAX, NULL);
		return (0);
	case MOD_UNLOAD:
		delete_unrhdr(video_unrhdr);
		return (0);
	default:
		return (EOPNOTSUPP);
	}
}

static moduledata_t video_mod = {
	"video",
	video_modevent,
	NULL,
};

DECLARE_MODULE(video, video_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_VERSION(video, 1);
