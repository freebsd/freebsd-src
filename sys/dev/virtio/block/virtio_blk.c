/*-
 * Copyright (c) 2011, Bryan Venteicher <bryanv@daemoninthecloset.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Driver for VirtIO block devices. */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bio.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sglist.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>

#include <geom/geom_disk.h>
#include <vm/uma.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/virtio/virtio.h>
#include <dev/virtio/virtqueue.h>
#include <dev/virtio/block/virtio_blk.h>

#include "virtio_if.h"

struct vtblk_request {
	struct virtio_blk_outhdr	 vbr_hdr;
	struct bio			*vbr_bp;
	uint8_t				 vbr_ack;

	TAILQ_ENTRY(vtblk_request)	 vbr_link;
};

struct vtblk_softc {
	device_t		 vtblk_dev;
	struct mtx		 vtblk_mtx;
	uint64_t		 vtblk_features;
	uint32_t		 vtblk_flags;
#define VTBLK_FLAG_INDIRECT	0x0001
#define VTBLK_FLAG_READONLY	0x0002
#define VTBLK_FLAG_DETACHING	0x0004
#define VTBLK_FLAG_SUSPENDED	0x0008
#define VTBLK_FLAG_DUMPING	0x0010

	struct virtqueue	*vtblk_vq;
	struct sglist		*vtblk_sglist;
	struct disk		*vtblk_disk;

	struct bio_queue_head	 vtblk_bioq;
	TAILQ_HEAD(, vtblk_request)
				 vtblk_req_free;
	TAILQ_HEAD(, vtblk_request)
				 vtblk_req_ready;

	struct taskqueue	*vtblk_tq;
	struct task		 vtblk_intr_task;

	int			 vtblk_sector_size;
	int			 vtblk_max_nsegs;
	int			 vtblk_unit;
	int			 vtblk_request_count;

	struct vtblk_request	 vtblk_dump_request;
};

static struct virtio_feature_desc vtblk_feature_desc[] = {
	{ VIRTIO_BLK_F_BARRIER,		"HostBarrier"	},
	{ VIRTIO_BLK_F_SIZE_MAX,	"MaxSegSize"	},
	{ VIRTIO_BLK_F_SEG_MAX,		"MaxNumSegs"	},
	{ VIRTIO_BLK_F_GEOMETRY,	"DiskGeometry"	},
	{ VIRTIO_BLK_F_RO,		"ReadOnly"	},
	{ VIRTIO_BLK_F_BLK_SIZE,	"BlockSize"	},
	{ VIRTIO_BLK_F_SCSI,		"SCSICmds"	},
	{ VIRTIO_BLK_F_FLUSH,		"FlushCmd"	},
	{ VIRTIO_BLK_F_TOPOLOGY,	"Topology"	},

	{ 0, NULL }
};

static int	vtblk_modevent(module_t, int, void *);

static int	vtblk_probe(device_t);
static int	vtblk_attach(device_t);
static int	vtblk_detach(device_t);
static int	vtblk_suspend(device_t);
static int	vtblk_resume(device_t);
static int	vtblk_shutdown(device_t);

static void	vtblk_negotiate_features(struct vtblk_softc *);
static int	vtblk_maximum_segments(struct vtblk_softc *,
		    struct virtio_blk_config *);
static int	vtblk_alloc_virtqueue(struct vtblk_softc *);
static void	vtblk_alloc_disk(struct vtblk_softc *,
		    struct virtio_blk_config *);
static void	vtblk_create_disk(struct vtblk_softc *);

static int	vtblk_open(struct disk *);
static int	vtblk_close(struct disk *);
static int	vtblk_ioctl(struct disk *, u_long, void *, int,
		    struct thread *);
static int	vtblk_dump(void *, void *, vm_offset_t, off_t, size_t);
static void	vtblk_strategy(struct bio *);

static void	vtblk_startio(struct vtblk_softc *);
static struct vtblk_request * vtblk_bio_request(struct vtblk_softc *);
static int	vtblk_execute_request(struct vtblk_softc *,
		    struct vtblk_request *);

static int	vtblk_vq_intr(void *);
static void	vtblk_intr_task(void *, int);

static void	vtblk_stop(struct vtblk_softc *);

static void	vtblk_get_ident(struct vtblk_softc *);
static void	vtblk_prepare_dump(struct vtblk_softc *);
static int	vtblk_write_dump(struct vtblk_softc *, void *, off_t, size_t);
static int	vtblk_flush_dump(struct vtblk_softc *);
static int	vtblk_poll_request(struct vtblk_softc *,
		    struct vtblk_request *);

static void	vtblk_drain_vq(struct vtblk_softc *, int);
static void	vtblk_drain(struct vtblk_softc *);

static int	vtblk_alloc_requests(struct vtblk_softc *);
static void	vtblk_free_requests(struct vtblk_softc *);
static struct vtblk_request * vtblk_dequeue_request(struct vtblk_softc *);
static void	vtblk_enqueue_request(struct vtblk_softc *,
		    struct vtblk_request *);

static struct vtblk_request * vtblk_dequeue_ready(struct vtblk_softc *);
static void	vtblk_enqueue_ready(struct vtblk_softc *,
		    struct vtblk_request *);

static void	vtblk_bio_error(struct bio *, int);

/* Tunables. */
static int vtblk_no_ident = 0;
TUNABLE_INT("hw.vtblk.no_ident", &vtblk_no_ident);

/* Features desired/implemented by this driver. */
#define VTBLK_FEATURES \
    (VIRTIO_BLK_F_BARRIER		| \
     VIRTIO_BLK_F_SIZE_MAX		| \
     VIRTIO_BLK_F_SEG_MAX		| \
     VIRTIO_BLK_F_GEOMETRY		| \
     VIRTIO_BLK_F_RO			| \
     VIRTIO_BLK_F_BLK_SIZE		| \
     VIRTIO_BLK_F_FLUSH			| \
     VIRTIO_RING_F_INDIRECT_DESC)

#define VTBLK_MTX(_sc)		&(_sc)->vtblk_mtx
#define VTBLK_LOCK_INIT(_sc, _name) \
				mtx_init(VTBLK_MTX((_sc)), (_name), \
				    "VTBLK Lock", MTX_DEF)
#define VTBLK_LOCK(_sc)		mtx_lock(VTBLK_MTX((_sc)))
#define VTBLK_TRYLOCK(_sc)	mtx_trylock(VTBLK_MTX((_sc)))
#define VTBLK_UNLOCK(_sc)	mtx_unlock(VTBLK_MTX((_sc)))
#define VTBLK_LOCK_DESTROY(_sc)	mtx_destroy(VTBLK_MTX((_sc)))
#define VTBLK_LOCK_ASSERT(_sc)	mtx_assert(VTBLK_MTX((_sc)), MA_OWNED)
#define VTBLK_LOCK_ASSERT_NOTOWNED(_sc) \
				mtx_assert(VTBLK_MTX((_sc)), MA_NOTOWNED)

#define VTBLK_BIO_SEGMENTS(_bp)	sglist_count((_bp)->bio_data, (_bp)->bio_bcount)

#define VTBLK_DISK_NAME		"vtbd"

/*
 * Each block request uses at least two segments - one for the header
 * and one for the status.
 */
#define VTBLK_MIN_SEGMENTS	2

static uma_zone_t vtblk_req_zone;

static device_method_t vtblk_methods[] = {
	/* Device methods. */
	DEVMETHOD(device_probe,		vtblk_probe),
	DEVMETHOD(device_attach,	vtblk_attach),
	DEVMETHOD(device_detach,	vtblk_detach),
	DEVMETHOD(device_suspend,	vtblk_suspend),
	DEVMETHOD(device_resume,	vtblk_resume),
	DEVMETHOD(device_shutdown,	vtblk_shutdown),

	{ 0, 0 }
};

static driver_t vtblk_driver = {
	"vtblk",
	vtblk_methods,
	sizeof(struct vtblk_softc)
};
static devclass_t vtblk_devclass;

DRIVER_MODULE(virtio_blk, virtio_pci, vtblk_driver, vtblk_devclass,
    vtblk_modevent, 0);
MODULE_VERSION(virtio_blk, 1);
MODULE_DEPEND(virtio_blk, virtio, 1, 1, 1);

static int
vtblk_modevent(module_t mod, int type, void *unused)
{
	int error;

	error = 0;

	switch (type) {
	case MOD_LOAD:
		vtblk_req_zone = uma_zcreate("vtblk_request",
		    sizeof(struct vtblk_request),
		    NULL, NULL, NULL, NULL, 0, 0);
		break;
	case MOD_QUIESCE:
	case MOD_UNLOAD:
		if (uma_zone_get_cur(vtblk_req_zone) > 0)
			error = EBUSY;
		else if (type == MOD_UNLOAD) {
			uma_zdestroy(vtblk_req_zone);
			vtblk_req_zone = NULL;
		}
		break;
	case MOD_SHUTDOWN:
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

static int
vtblk_probe(device_t dev)
{

	if (virtio_get_device_type(dev) != VIRTIO_ID_BLOCK)
		return (ENXIO);

	device_set_desc(dev, "VirtIO Block Adapter");

	return (BUS_PROBE_DEFAULT);
}

static int
vtblk_attach(device_t dev)
{
	struct vtblk_softc *sc;
	struct virtio_blk_config blkcfg;
	int error;

	sc = device_get_softc(dev);
	sc->vtblk_dev = dev;
	sc->vtblk_unit = device_get_unit(dev);

	VTBLK_LOCK_INIT(sc, device_get_nameunit(dev));

	bioq_init(&sc->vtblk_bioq);
	TAILQ_INIT(&sc->vtblk_req_free);
	TAILQ_INIT(&sc->vtblk_req_ready);

	virtio_set_feature_desc(dev, vtblk_feature_desc);
	vtblk_negotiate_features(sc);

	if (virtio_with_feature(dev, VIRTIO_RING_F_INDIRECT_DESC))
		sc->vtblk_flags |= VTBLK_FLAG_INDIRECT;

	if (virtio_with_feature(dev, VIRTIO_BLK_F_RO))
		sc->vtblk_flags |= VTBLK_FLAG_READONLY;

	/* Get local copy of config. */
	if (virtio_with_feature(dev, VIRTIO_BLK_F_TOPOLOGY) == 0) {
		bzero(&blkcfg, sizeof(struct virtio_blk_config));
		virtio_read_device_config(dev, 0, &blkcfg,
		    offsetof(struct virtio_blk_config, physical_block_exp));
	} else
		virtio_read_device_config(dev, 0, &blkcfg,
		    sizeof(struct virtio_blk_config));

	/*
	 * With the current sglist(9) implementation, it is not easy
	 * for us to support a maximum segment size as adjacent
	 * segments are coalesced. For now, just make sure it's larger
	 * than the maximum supported transfer size.
	 */
	if (virtio_with_feature(dev, VIRTIO_BLK_F_SIZE_MAX)) {
		if (blkcfg.size_max < MAXPHYS) {
			error = ENOTSUP;
			device_printf(dev, "host requires unsupported "
			    "maximum segment size feature\n");
			goto fail;
		}
	}

	sc->vtblk_max_nsegs = vtblk_maximum_segments(sc, &blkcfg);

	/*
	 * Allocate working sglist. The number of segments may be too
	 * large to safely store on the stack.
	 */
	sc->vtblk_sglist = sglist_alloc(sc->vtblk_max_nsegs, M_NOWAIT);
	if (sc->vtblk_sglist == NULL) {
		error = ENOMEM;
		device_printf(dev, "cannot allocate sglist\n");
		goto fail;
	}

	error = vtblk_alloc_virtqueue(sc);
	if (error) {
		device_printf(dev, "cannot allocate virtqueue\n");
		goto fail;
	}

	error = vtblk_alloc_requests(sc);
	if (error) {
		device_printf(dev, "cannot preallocate requests\n");
		goto fail;
	}

	vtblk_alloc_disk(sc, &blkcfg);

	TASK_INIT(&sc->vtblk_intr_task, 0, vtblk_intr_task, sc);
	sc->vtblk_tq = taskqueue_create_fast("vtblk_taskq", M_NOWAIT,
	    taskqueue_thread_enqueue, &sc->vtblk_tq);
	if (sc->vtblk_tq == NULL) {
		error = ENOMEM;
		device_printf(dev, "cannot allocate taskqueue\n");
		goto fail;
	}
	taskqueue_start_threads(&sc->vtblk_tq, 1, PI_DISK, "%s taskq",
	    device_get_nameunit(dev));

	error = virtio_setup_intr(dev, INTR_TYPE_BIO | INTR_ENTROPY);
	if (error) {
		device_printf(dev, "cannot setup virtqueue interrupt\n");
		goto fail;
	}

	vtblk_create_disk(sc);

	virtqueue_enable_intr(sc->vtblk_vq);

fail:
	if (error)
		vtblk_detach(dev);

	return (error);
}

static int
vtblk_detach(device_t dev)
{
	struct vtblk_softc *sc;

	sc = device_get_softc(dev);

	VTBLK_LOCK(sc);
	sc->vtblk_flags |= VTBLK_FLAG_DETACHING;
	if (device_is_attached(dev))
		vtblk_stop(sc);
	VTBLK_UNLOCK(sc);

	if (sc->vtblk_tq != NULL) {
		taskqueue_drain(sc->vtblk_tq, &sc->vtblk_intr_task);
		taskqueue_free(sc->vtblk_tq);
		sc->vtblk_tq = NULL;
	}

	vtblk_drain(sc);

	if (sc->vtblk_disk != NULL) {
		disk_destroy(sc->vtblk_disk);
		sc->vtblk_disk = NULL;
	}

	if (sc->vtblk_sglist != NULL) {
		sglist_free(sc->vtblk_sglist);
		sc->vtblk_sglist = NULL;
	}

	VTBLK_LOCK_DESTROY(sc);

	return (0);
}

static int
vtblk_suspend(device_t dev)
{
	struct vtblk_softc *sc;

	sc = device_get_softc(dev);

	VTBLK_LOCK(sc);
	sc->vtblk_flags |= VTBLK_FLAG_SUSPENDED;
	/* TODO Wait for any inflight IO to complete? */
	VTBLK_UNLOCK(sc);

	return (0);
}

static int
vtblk_resume(device_t dev)
{
	struct vtblk_softc *sc;

	sc = device_get_softc(dev);

	VTBLK_LOCK(sc);
	sc->vtblk_flags &= ~VTBLK_FLAG_SUSPENDED;
	/* TODO Resume IO? */
	VTBLK_UNLOCK(sc);

	return (0);
}

static int
vtblk_shutdown(device_t dev)
{

	return (0);
}

static int
vtblk_open(struct disk *dp)
{
	struct vtblk_softc *sc;

	if ((sc = dp->d_drv1) == NULL)
		return (ENXIO);

	return (sc->vtblk_flags & VTBLK_FLAG_DETACHING ? ENXIO : 0);
}

static int
vtblk_close(struct disk *dp)
{
	struct vtblk_softc *sc;

	if ((sc = dp->d_drv1) == NULL)
		return (ENXIO);

	return (0);
}

static int
vtblk_ioctl(struct disk *dp, u_long cmd, void *addr, int flag,
    struct thread *td)
{
	struct vtblk_softc *sc;

	if ((sc = dp->d_drv1) == NULL)
		return (ENXIO);

	return (ENOTTY);
}

static int
vtblk_dump(void *arg, void *virtual, vm_offset_t physical, off_t offset,
    size_t length)
{
	struct disk *dp;
	struct vtblk_softc *sc;
	int error;

	dp = arg;
	error = 0;

	if ((sc = dp->d_drv1) == NULL)
		return (ENXIO);

	if (VTBLK_TRYLOCK(sc) == 0) {
		device_printf(sc->vtblk_dev,
		    "softc already locked, cannot dump...\n");
		return (EBUSY);
	}

	if ((sc->vtblk_flags & VTBLK_FLAG_DUMPING) == 0) {
		vtblk_prepare_dump(sc);
		sc->vtblk_flags |= VTBLK_FLAG_DUMPING;
	}

	if (length > 0)
		error = vtblk_write_dump(sc, virtual, offset, length);
	else if (virtual == NULL && offset == 0)
		error = vtblk_flush_dump(sc);

	VTBLK_UNLOCK(sc);

	return (error);
}

static void
vtblk_strategy(struct bio *bp)
{
	struct vtblk_softc *sc;

	if ((sc = bp->bio_disk->d_drv1) == NULL) {
		vtblk_bio_error(bp, EINVAL);
		return;
	}

	/*
	 * Fail any write if RO. Unfortunately, there does not seem to
	 * be a better way to report our readonly'ness to GEOM above.
	 */
	if (sc->vtblk_flags & VTBLK_FLAG_READONLY &&
	    (bp->bio_cmd == BIO_WRITE || bp->bio_cmd == BIO_FLUSH)) {
		vtblk_bio_error(bp, EROFS);
		return;
	}

	/*
	 * Prevent read/write buffers spanning too many segments from
	 * getting into the queue. This should only trip if d_maxsize
	 * was incorrectly set.
	 */
	if (bp->bio_cmd == BIO_READ || bp->bio_cmd == BIO_WRITE) {
		KASSERT(VTBLK_BIO_SEGMENTS(bp) <= sc->vtblk_max_nsegs -
		    VTBLK_MIN_SEGMENTS,
		    ("bio spanned too many segments: %d, max: %d",
		    VTBLK_BIO_SEGMENTS(bp),
		    sc->vtblk_max_nsegs - VTBLK_MIN_SEGMENTS));
	}

	VTBLK_LOCK(sc);
	if ((sc->vtblk_flags & VTBLK_FLAG_DETACHING) == 0) {
		bioq_disksort(&sc->vtblk_bioq, bp);
		vtblk_startio(sc);
	} else
		vtblk_bio_error(bp, ENXIO);
	VTBLK_UNLOCK(sc);
}

static void
vtblk_negotiate_features(struct vtblk_softc *sc)
{
	device_t dev;
	uint64_t features;

	dev = sc->vtblk_dev;
	features = VTBLK_FEATURES;

	sc->vtblk_features = virtio_negotiate_features(dev, features);
}

static int
vtblk_maximum_segments(struct vtblk_softc *sc,
    struct virtio_blk_config *blkcfg)
{
	device_t dev;
	int nsegs;

	dev = sc->vtblk_dev;
	nsegs = VTBLK_MIN_SEGMENTS;

	if (virtio_with_feature(dev, VIRTIO_BLK_F_SEG_MAX)) {
		nsegs += MIN(blkcfg->seg_max, MAXPHYS / PAGE_SIZE + 1);
		if (sc->vtblk_flags & VTBLK_FLAG_INDIRECT)
			nsegs = MIN(nsegs, VIRTIO_MAX_INDIRECT);
	} else
		nsegs += 1;

	return (nsegs);
}

static int
vtblk_alloc_virtqueue(struct vtblk_softc *sc)
{
	device_t dev;
	struct vq_alloc_info vq_info;

	dev = sc->vtblk_dev;

	VQ_ALLOC_INFO_INIT(&vq_info, sc->vtblk_max_nsegs,
	    vtblk_vq_intr, sc, &sc->vtblk_vq,
	    "%s request", device_get_nameunit(dev));

	return (virtio_alloc_virtqueues(dev, 0, 1, &vq_info));
}

static void
vtblk_alloc_disk(struct vtblk_softc *sc, struct virtio_blk_config *blkcfg)
{
	device_t dev;
	struct disk *dp;

	dev = sc->vtblk_dev;

	sc->vtblk_disk = dp = disk_alloc();
	dp->d_open = vtblk_open;
	dp->d_close = vtblk_close;
	dp->d_ioctl = vtblk_ioctl;
	dp->d_strategy = vtblk_strategy;
	dp->d_name = VTBLK_DISK_NAME;
	dp->d_unit = sc->vtblk_unit;
	dp->d_drv1 = sc;

	if ((sc->vtblk_flags & VTBLK_FLAG_READONLY) == 0)
		dp->d_dump = vtblk_dump;

	/* Capacity is always in 512-byte units. */
	dp->d_mediasize = blkcfg->capacity * 512;

	if (virtio_with_feature(dev, VIRTIO_BLK_F_BLK_SIZE))
		sc->vtblk_sector_size = blkcfg->blk_size;
	else
		sc->vtblk_sector_size = 512;
	dp->d_sectorsize = sc->vtblk_sector_size;

	/*
	 * The VirtIO maximum I/O size is given in terms of segments.
	 * However, FreeBSD limits I/O size by logical buffer size, not
	 * by physically contiguous pages. Therefore, we have to assume
	 * no pages are contiguous. This may impose an artificially low
	 * maximum I/O size. But in practice, since QEMU advertises 128
	 * segments, this gives us a maximum IO size of 125 * PAGE_SIZE,
	 * which is typically greater than MAXPHYS. Eventually we should
	 * just advertise MAXPHYS and split buffers that are too big.
	 *
	 * Note we must subtract one additional segment in case of non
	 * page aligned buffers.
	 */
	dp->d_maxsize = (sc->vtblk_max_nsegs - VTBLK_MIN_SEGMENTS - 1) *
	    PAGE_SIZE;
	if (dp->d_maxsize < PAGE_SIZE)
		dp->d_maxsize = PAGE_SIZE; /* XXX */

	if (virtio_with_feature(dev, VIRTIO_BLK_F_GEOMETRY)) {
		dp->d_fwsectors = blkcfg->geometry.sectors;
		dp->d_fwheads = blkcfg->geometry.heads;
	}

	if (virtio_with_feature(dev, VIRTIO_BLK_F_FLUSH))
		dp->d_flags |= DISKFLAG_CANFLUSHCACHE;
}

static void
vtblk_create_disk(struct vtblk_softc *sc)
{
	struct disk *dp;

	dp = sc->vtblk_disk;

	/*
	 * Retrieving the identification string must be done after
	 * the virtqueue interrupt is setup otherwise it will hang.
	 */
	vtblk_get_ident(sc);

	device_printf(sc->vtblk_dev, "%juMB (%ju %u byte sectors)\n",
	    (uintmax_t) dp->d_mediasize >> 20,
	    (uintmax_t) dp->d_mediasize / dp->d_sectorsize,
	    dp->d_sectorsize);

	disk_create(dp, DISK_VERSION);
}

static void
vtblk_startio(struct vtblk_softc *sc)
{
	struct virtqueue *vq;
	struct vtblk_request *req;
	int enq;

	vq = sc->vtblk_vq;
	enq = 0;

	VTBLK_LOCK_ASSERT(sc);

	if (sc->vtblk_flags & VTBLK_FLAG_SUSPENDED)
		return;

	while (!virtqueue_full(vq)) {
		if ((req = vtblk_dequeue_ready(sc)) == NULL)
			req = vtblk_bio_request(sc);
		if (req == NULL)
			break;

		if (vtblk_execute_request(sc, req) != 0) {
			vtblk_enqueue_ready(sc, req);
			break;
		}

		enq++;
	}

	if (enq > 0)
		virtqueue_notify(vq);
}

static struct vtblk_request *
vtblk_bio_request(struct vtblk_softc *sc)
{
	struct bio_queue_head *bioq;
	struct vtblk_request *req;
	struct bio *bp;

	bioq = &sc->vtblk_bioq;

	if (bioq_first(bioq) == NULL)
		return (NULL);

	req = vtblk_dequeue_request(sc);
	if (req == NULL)
		return (NULL);

	bp = bioq_takefirst(bioq);
	req->vbr_bp = bp;
	req->vbr_ack = -1;
	req->vbr_hdr.ioprio = 1;

	switch (bp->bio_cmd) {
	case BIO_FLUSH:
		req->vbr_hdr.type = VIRTIO_BLK_T_FLUSH;
		break;
	case BIO_READ:
		req->vbr_hdr.type = VIRTIO_BLK_T_IN;
		req->vbr_hdr.sector = bp->bio_offset / 512;
		break;
	case BIO_WRITE:
		req->vbr_hdr.type = VIRTIO_BLK_T_OUT;
		req->vbr_hdr.sector = bp->bio_offset / 512;
		break;
	default:
		KASSERT(0, ("bio with unhandled cmd: %d", bp->bio_cmd));
		req->vbr_hdr.type = -1;
		break;
	}

	if (bp->bio_flags & BIO_ORDERED)
		req->vbr_hdr.type |= VIRTIO_BLK_T_BARRIER;

	return (req);
}

static int
vtblk_execute_request(struct vtblk_softc *sc, struct vtblk_request *req)
{
	struct sglist *sg;
	struct bio *bp;
	int writable, error;

	sg = sc->vtblk_sglist;
	bp = req->vbr_bp;
	writable = 0;

	VTBLK_LOCK_ASSERT(sc);

	sglist_reset(sg);
	error = sglist_append(sg, &req->vbr_hdr,
	    sizeof(struct virtio_blk_outhdr));
	KASSERT(error == 0, ("error adding header to sglist"));
	KASSERT(sg->sg_nseg == 1,
	    ("header spanned multiple segments: %d", sg->sg_nseg));

	if (bp->bio_cmd == BIO_READ || bp->bio_cmd == BIO_WRITE) {
		error = sglist_append(sg, bp->bio_data, bp->bio_bcount);
		KASSERT(error == 0, ("error adding buffer to sglist"));

		/* BIO_READ means the host writes into our buffer. */
		if (bp->bio_cmd == BIO_READ)
			writable += sg->sg_nseg - 1;
	}

	error = sglist_append(sg, &req->vbr_ack, sizeof(uint8_t));
	KASSERT(error == 0, ("error adding ack to sglist"));
	writable++;

	KASSERT(sg->sg_nseg >= VTBLK_MIN_SEGMENTS,
	    ("fewer than min segments: %d", sg->sg_nseg));

	error = virtqueue_enqueue(sc->vtblk_vq, req, sg,
	    sg->sg_nseg - writable, writable);

	return (error);
}

static int
vtblk_vq_intr(void *xsc)
{
	struct vtblk_softc *sc;

	sc = xsc;

	virtqueue_disable_intr(sc->vtblk_vq);
	taskqueue_enqueue_fast(sc->vtblk_tq, &sc->vtblk_intr_task);

	return (1);
}

static void
vtblk_intr_task(void *arg, int pending)
{
	struct vtblk_softc *sc;
	struct vtblk_request *req;
	struct virtqueue *vq;
	struct bio *bp;

	sc = arg;
	vq = sc->vtblk_vq;

	VTBLK_LOCK(sc);
	if (sc->vtblk_flags & VTBLK_FLAG_DETACHING) {
		VTBLK_UNLOCK(sc);
		return;
	}

	while ((req = virtqueue_dequeue(vq, NULL)) != NULL) {
		bp = req->vbr_bp;

		if (req->vbr_ack == VIRTIO_BLK_S_OK)
			bp->bio_resid = 0;
		else {
			bp->bio_flags |= BIO_ERROR;
			if (req->vbr_ack == VIRTIO_BLK_S_UNSUPP)
				bp->bio_error = ENOTSUP;
			else
				bp->bio_error = EIO;
		}

		biodone(bp);
		vtblk_enqueue_request(sc, req);
	}

	vtblk_startio(sc);

	if (virtqueue_enable_intr(vq) != 0) {
		virtqueue_disable_intr(vq);
		VTBLK_UNLOCK(sc);
		taskqueue_enqueue_fast(sc->vtblk_tq,
		    &sc->vtblk_intr_task);
		return;
	}

	VTBLK_UNLOCK(sc);
}

static void
vtblk_stop(struct vtblk_softc *sc)
{

	virtqueue_disable_intr(sc->vtblk_vq);
	virtio_stop(sc->vtblk_dev);
}

static void
vtblk_get_ident(struct vtblk_softc *sc)
{
	struct bio buf;
	struct disk *dp;
	struct vtblk_request *req;
	int len, error;

	dp = sc->vtblk_disk;
	len = MIN(VIRTIO_BLK_ID_BYTES, DISK_IDENT_SIZE);

	if (vtblk_no_ident != 0)
		return;

	req = vtblk_dequeue_request(sc);
	if (req == NULL)
		return;

	req->vbr_ack = -1;
	req->vbr_hdr.type = VIRTIO_BLK_T_GET_ID;
	req->vbr_hdr.ioprio = 1;
	req->vbr_hdr.sector = 0;

	req->vbr_bp = &buf;
	bzero(&buf, sizeof(struct bio));

	buf.bio_cmd = BIO_READ;
	buf.bio_data = dp->d_ident;
	buf.bio_bcount = len;

	VTBLK_LOCK(sc);
	error = vtblk_poll_request(sc, req);
	vtblk_enqueue_request(sc, req);
	VTBLK_UNLOCK(sc);

	if (error) {
		device_printf(sc->vtblk_dev,
		    "error getting device identifier: %d\n", error);
	}
}

static void
vtblk_prepare_dump(struct vtblk_softc *sc)
{
	device_t dev;
	struct virtqueue *vq;

	dev = sc->vtblk_dev;
	vq = sc->vtblk_vq;

	vtblk_stop(sc);

	/*
	 * Drain all requests caught in-flight in the virtqueue,
	 * skipping biodone(). When dumping, only one request is
	 * outstanding at a time, and we just poll the virtqueue
	 * for the response.
	 */
	vtblk_drain_vq(sc, 1);

	if (virtio_reinit(dev, sc->vtblk_features) != 0)
		panic("cannot reinit VirtIO block device during dump");

	virtqueue_disable_intr(vq);
	virtio_reinit_complete(dev);
}

static int
vtblk_write_dump(struct vtblk_softc *sc, void *virtual, off_t offset,
    size_t length)
{
	struct bio buf;
	struct vtblk_request *req;

	req = &sc->vtblk_dump_request;
	req->vbr_ack = -1;
	req->vbr_hdr.type = VIRTIO_BLK_T_OUT;
	req->vbr_hdr.ioprio = 1;
	req->vbr_hdr.sector = offset / 512;

	req->vbr_bp = &buf;
	bzero(&buf, sizeof(struct bio));

	buf.bio_cmd = BIO_WRITE;
	buf.bio_data = virtual;
	buf.bio_bcount = length;

	return (vtblk_poll_request(sc, req));
}

static int
vtblk_flush_dump(struct vtblk_softc *sc)
{
	struct bio buf;
	struct vtblk_request *req;

	req = &sc->vtblk_dump_request;
	req->vbr_ack = -1;
	req->vbr_hdr.type = VIRTIO_BLK_T_FLUSH;
	req->vbr_hdr.ioprio = 1;
	req->vbr_hdr.sector = 0;

	req->vbr_bp = &buf;
	bzero(&buf, sizeof(struct bio));

	buf.bio_cmd = BIO_FLUSH;

	return (vtblk_poll_request(sc, req));
}

static int
vtblk_poll_request(struct vtblk_softc *sc, struct vtblk_request *req)
{
	device_t dev;
	struct virtqueue *vq;
	struct vtblk_request *r;
	int error;

	dev = sc->vtblk_dev;
	vq = sc->vtblk_vq;

	if (!virtqueue_empty(vq))
		return (EBUSY);

	error = vtblk_execute_request(sc, req);
	if (error)
		return (error);

	virtqueue_notify(vq);

	r = virtqueue_poll(vq, NULL);
	KASSERT(r == req, ("unexpected request response"));

	if (req->vbr_ack != VIRTIO_BLK_S_OK) {
		error = req->vbr_ack == VIRTIO_BLK_S_UNSUPP ? ENOTSUP : EIO;
		if (bootverbose)
			device_printf(dev,
			    "vtblk_poll_request: IO error: %d\n", error);
	}

	return (error);
}

static void
vtblk_drain_vq(struct vtblk_softc *sc, int skip_done)
{
	struct virtqueue *vq;
	struct vtblk_request *req;
	int last;

	vq = sc->vtblk_vq;
	last = 0;

	while ((req = virtqueue_drain(vq, &last)) != NULL) {
		if (!skip_done)
			vtblk_bio_error(req->vbr_bp, ENXIO);

		vtblk_enqueue_request(sc, req);
	}

	KASSERT(virtqueue_empty(vq), ("virtqueue not empty"));
}

static void
vtblk_drain(struct vtblk_softc *sc)
{
	struct bio_queue_head *bioq;
	struct vtblk_request *req;
	struct bio *bp;

	bioq = &sc->vtblk_bioq;

	if (sc->vtblk_vq != NULL)
		vtblk_drain_vq(sc, 0);

	while ((req = vtblk_dequeue_ready(sc)) != NULL) {
		vtblk_bio_error(req->vbr_bp, ENXIO);
		vtblk_enqueue_request(sc, req);
	}

	while (bioq_first(bioq) != NULL) {
		bp = bioq_takefirst(bioq);
		vtblk_bio_error(bp, ENXIO);
	}

	vtblk_free_requests(sc);
}

static int
vtblk_alloc_requests(struct vtblk_softc *sc)
{
	struct vtblk_request *req;
	int i, size;

	size = virtqueue_size(sc->vtblk_vq);

	/*
	 * Preallocate sufficient requests to keep the virtqueue full. Each
	 * request consumes VTBLK_MIN_SEGMENTS or more descriptors so reduce
	 * the number allocated when indirect descriptors are not available.
	 */
	if ((sc->vtblk_flags & VTBLK_FLAG_INDIRECT) == 0)
		size /= VTBLK_MIN_SEGMENTS;

	for (i = 0; i < size; i++) {
		req = uma_zalloc(vtblk_req_zone, M_NOWAIT);
		if (req == NULL)
			return (ENOMEM);

		sc->vtblk_request_count++;
		vtblk_enqueue_request(sc, req);
	}

	return (0);
}

static void
vtblk_free_requests(struct vtblk_softc *sc)
{
	struct vtblk_request *req;

	while ((req = vtblk_dequeue_request(sc)) != NULL) {
		sc->vtblk_request_count--;
		uma_zfree(vtblk_req_zone, req);
	}

	KASSERT(sc->vtblk_request_count == 0, ("leaked requests"));
}

static struct vtblk_request *
vtblk_dequeue_request(struct vtblk_softc *sc)
{
	struct vtblk_request *req;

	req = TAILQ_FIRST(&sc->vtblk_req_free);
	if (req != NULL)
		TAILQ_REMOVE(&sc->vtblk_req_free, req, vbr_link);

	return (req);
}

static void
vtblk_enqueue_request(struct vtblk_softc *sc, struct vtblk_request *req)
{

	bzero(req, sizeof(struct vtblk_request));
	TAILQ_INSERT_HEAD(&sc->vtblk_req_free, req, vbr_link);
}

static struct vtblk_request *
vtblk_dequeue_ready(struct vtblk_softc *sc)
{
	struct vtblk_request *req;

	req = TAILQ_FIRST(&sc->vtblk_req_ready);
	if (req != NULL)
		TAILQ_REMOVE(&sc->vtblk_req_ready, req, vbr_link);

	return (req);
}

static void
vtblk_enqueue_ready(struct vtblk_softc *sc, struct vtblk_request *req)
{

	TAILQ_INSERT_HEAD(&sc->vtblk_req_ready, req, vbr_link);
}

static void
vtblk_bio_error(struct bio *bp, int error)
{

	biofinish(bp, NULL, error);
}
