/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright (c) 2006 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * All rights reserved.
 */

#include <sys/zfs_context.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bio.h>
#include <sys/spa.h>
#include <sys/vdev_impl.h>
#include <sys/fs/zfs.h>
#include <sys/zio.h>
#include <geom/geom.h>

/*
 * Virtual device vector for GEOM.
 */

struct g_class zfs_vdev_class = {
	.name = "ZFS::VDEV",
	.version = G_VERSION,
};

DECLARE_GEOM_CLASS(zfs_vdev_class, zfs_vdev);

typedef struct vdev_geom_ctx {
	struct g_consumer *gc_consumer;
	int gc_state;
	struct bio_queue_head gc_queue;
	struct mtx gc_queue_mtx;
} vdev_geom_ctx_t;

static void
vdev_geom_release(vdev_t *vd)
{
	vdev_geom_ctx_t *ctx;

	ctx = vd->vdev_tsd;
	vd->vdev_tsd = NULL;

	mtx_lock(&ctx->gc_queue_mtx);
	ctx->gc_state = 1;
	wakeup_one(&ctx->gc_queue);
	while (ctx->gc_state != 2)
		msleep(&ctx->gc_state, &ctx->gc_queue_mtx, 0, "vgeom:w", 0);
	mtx_unlock(&ctx->gc_queue_mtx);
	mtx_destroy(&ctx->gc_queue_mtx);
	kmem_free(ctx, sizeof(*ctx));
}

static void
vdev_geom_orphan(struct g_consumer *cp)
{
	struct g_geom *gp;
	vdev_t *vd;
	int error;

	g_topology_assert();

	vd = cp->private;
	gp = cp->geom;
	error = cp->provider->error;

	ZFS_LOG(1, "Closing access to %s.", cp->provider->name);
	g_access(cp, -cp->acr, -cp->acw, -cp->ace);
	g_detach(cp);
	ZFS_LOG(1, "Destroyed consumer to %s.", cp->provider->name);
	g_destroy_consumer(cp);
	/* Destroy geom if there are no consumers left. */
	if (LIST_EMPTY(&gp->consumer)) {
		ZFS_LOG(1, "Destroyed geom %s.", gp->name);
		g_wither_geom(cp->geom, error);
	}
	vdev_geom_release(vd);
	/* Both methods below work, but in a bit different way. */
#if 0
	vd->vdev_reopen_wanted = 1;
#else
	vd->vdev_stat.vs_aux = VDEV_AUX_OPEN_FAILED;
	vdev_set_state(vd, B_TRUE, VDEV_STATE_CANT_OPEN, vd->vdev_stat.vs_aux);
#endif
}

static struct g_consumer *
vdev_geom_attach(struct g_provider *pp, int write)
{
	struct g_geom *gp;
	struct g_consumer *cp;

	g_topology_assert();

	ZFS_LOG(1, "Attaching to %s.", pp->name);
	/* Do we have geom already? No? Create one. */
	LIST_FOREACH(gp, &zfs_vdev_class.geom, geom) {
		if (!(gp->flags & G_GEOM_WITHER))
			break;
	}
	if (gp == NULL) {
		gp = g_new_geomf(&zfs_vdev_class, "zfs::vdev");
		gp->orphan = vdev_geom_orphan;
		cp = g_new_consumer(gp);
		if (g_attach(cp, pp) != 0) {
			g_wither_geom(gp, ENXIO);
			return (NULL);
		}
		if (g_access(cp, 1, write, 1) != 0) {
			g_wither_geom(gp, ENXIO);
			return (NULL);
		}
		ZFS_LOG(1, "Created geom and consumer for %s.", pp->name);
	} else {
		/* Check if we are already connected to this provider. */
		LIST_FOREACH(cp, &gp->consumer, consumer) {
			if (cp->provider == pp) {
				ZFS_LOG(1, "Found consumer for %s.", pp->name);
				break;
			}
		}
		if (cp == NULL) {
			cp = g_new_consumer(gp);
			if (g_attach(cp, pp) != 0) {
				g_destroy_consumer(cp);
				return (NULL);
			}
			if (g_access(cp, 1, write, 1) != 0) {
				g_detach(cp);
				g_destroy_consumer(cp);
				return (NULL);
			}
			ZFS_LOG(1, "Created consumer for %s.", pp->name);
		} else {
			if (g_access(cp, 1, cp->acw > 0 ? 0 : write, 1) != 0)
				return (NULL);
			ZFS_LOG(1, "Used existing consumer for %s.", pp->name);
		}
	}
	return (cp);
}

static void
vdev_geom_detach(void *arg, int flag __unused)
{
	struct g_geom *gp;
	struct g_consumer *cp;

	g_topology_assert();
	cp = arg;
	gp = cp->geom;

	ZFS_LOG(1, "Closing access to %s.", cp->provider->name);
	g_access(cp, -1, 0, -1);
	/* Destroy consumer on last close. */
	if (cp->acr == 0 && cp->ace == 0) {
		ZFS_LOG(1, "Destroyed consumer to %s.", cp->provider->name);
		if (cp->acw > 0)
			g_access(cp, 0, -cp->acw, 0);
		g_detach(cp);
		g_destroy_consumer(cp);
	}
	/* Destroy geom if there are no consumers left. */
	if (LIST_EMPTY(&gp->consumer)) {
		ZFS_LOG(1, "Destroyed geom %s.", gp->name);
		g_wither_geom(gp, ENXIO);
	}
}

static void
vdev_geom_worker(void *arg)
{
	vdev_geom_ctx_t *ctx;
	zio_t *zio;
	struct bio *bp;

	ctx = arg;
	for (;;) {
		mtx_lock(&ctx->gc_queue_mtx);
		bp = bioq_takefirst(&ctx->gc_queue);
		if (bp == NULL) {
			if (ctx->gc_state == 1) {
				ctx->gc_state = 2;
				wakeup_one(&ctx->gc_state);
				mtx_unlock(&ctx->gc_queue_mtx);
				kthread_exit(0);
			}
			msleep(&ctx->gc_queue, &ctx->gc_queue_mtx,
			    PRIBIO | PDROP, "vgeom:io", 0);
			continue;
		}
		mtx_unlock(&ctx->gc_queue_mtx);
		zio = bp->bio_caller1;
		zio->io_error = bp->bio_error;
		if (bp->bio_cmd == BIO_FLUSH && bp->bio_error == ENOTSUP) {
			vdev_t *vd;

			/*
			 * If we get ENOTSUP, we know that no future
			 * attempts will ever succeed.  In this case we
			 * set a persistent bit so that we don't bother
			 * with the ioctl in the future.
			 */
			vd = zio->io_vd;
			vd->vdev_nowritecache = B_TRUE;
		}
		g_destroy_bio(bp);
		zio_next_stage_async(zio);
	}
}

static int
vdev_geom_open(vdev_t *vd, uint64_t *psize, uint64_t *ashift)
{
	vdev_geom_ctx_t *ctx;
	struct g_provider *pp;
	struct g_consumer *cp;
	int owned;

	/*
	 * We must have a pathname, and it must be absolute.
	 */
	if (vd->vdev_path == NULL || vd->vdev_path[0] != '/') {
		vd->vdev_stat.vs_aux = VDEV_AUX_BAD_LABEL;
		return (EINVAL);
	}

	if ((owned = mtx_owned(&Giant)))
		mtx_unlock(&Giant);
	g_topology_lock();
	pp = g_provider_by_name(vd->vdev_path + sizeof("/dev/") - 1);
	if (pp == NULL) {
		g_topology_unlock();
		if (owned)
			mtx_lock(&Giant);
		vd->vdev_stat.vs_aux = VDEV_AUX_BAD_LABEL;
		return (EINVAL);
	}
	cp = vdev_geom_attach(pp, !!(spa_mode & FWRITE));
	g_topology_unlock();
	if (owned)
		mtx_lock(&Giant);
	if (cp == NULL) {
		vd->vdev_stat.vs_aux = VDEV_AUX_OPEN_FAILED;
		return (EACCES);
	}

	/*
	 * Determine the actual size of the device.
	 */
	*psize = pp->mediasize;

	/*
	 * Determine the device's minimum transfer size.
	 */
	*ashift = highbit(MAX(pp->sectorsize, SPA_MINBLOCKSIZE)) - 1;

	/*
	 * Clear the nowritecache bit, so that on a vdev_reopen() we will
	 * try again.
	 */
	vd->vdev_nowritecache = B_FALSE;

	cp->private = vd;

	ctx = kmem_zalloc(sizeof(*ctx), KM_SLEEP);
	bioq_init(&ctx->gc_queue);
	mtx_init(&ctx->gc_queue_mtx, "zfs:vdev:geom:queue", NULL, MTX_DEF);
	ctx->gc_consumer = cp;
	ctx->gc_state = 0;

	vd->vdev_tsd = ctx;

	kthread_create(vdev_geom_worker, ctx, NULL, 0, 0, "vdev:worker %s",
	    pp->name);

	return (0);
}

static void
vdev_geom_close(vdev_t *vd)
{
	vdev_geom_ctx_t *ctx;
	struct g_consumer *cp;

	if ((ctx = vd->vdev_tsd) == NULL)
		return;
	if ((cp = ctx->gc_consumer) == NULL)
		return;
	vdev_geom_release(vd);
	g_post_event(vdev_geom_detach, cp, M_WAITOK, NULL);
}

static void
vdev_geom_io_intr(struct bio *bp)
{
	vdev_geom_ctx_t *ctx;
	zio_t *zio;

	zio = bp->bio_caller1;
	ctx = zio->io_vd->vdev_tsd;

	mtx_lock(&ctx->gc_queue_mtx);
	bioq_insert_tail(&ctx->gc_queue, bp);
	wakeup_one(&ctx->gc_queue);
	mtx_unlock(&ctx->gc_queue_mtx);
}

static void
vdev_geom_io_start(zio_t *zio)
{
	vdev_t *vd;
	vdev_geom_ctx_t *ctx;
	struct g_consumer *cp;
	struct bio *bp;
	int error;

	cp = NULL;

	vd = zio->io_vd;
	ctx = vd->vdev_tsd;
	if (ctx != NULL)
		cp = ctx->gc_consumer;

	if (zio->io_type == ZIO_TYPE_IOCTL) {
		zio_vdev_io_bypass(zio);

		/* XXPOLICY */
		if (vdev_is_dead(vd)) {
			zio->io_error = ENXIO;
			zio_next_stage_async(zio);
			return;
		}

		switch (zio->io_cmd) {

		case DKIOCFLUSHWRITECACHE:
			if (vd->vdev_nowritecache) {
				zio->io_error = ENOTSUP;
				break;
			}

			goto sendreq;
		default:
			zio->io_error = ENOTSUP;
		}

		zio_next_stage_async(zio);
		return;
	}

	if (zio->io_type == ZIO_TYPE_READ && vdev_cache_read(zio) == 0)
		return;

	if ((zio = vdev_queue_io(zio)) == NULL)
		return;

sendreq:

	error = vdev_is_dead(vd) ? ENXIO : vdev_error_inject(vd, zio);
	if (error == 0 && cp == NULL)
		error = ENXIO;
	if (error) {
		zio->io_error = error;
		zio_next_stage_async(zio);
		return;
	}

	bp = g_alloc_bio();
	bp->bio_caller1 = zio;
	switch (zio->io_type) {
	case ZIO_TYPE_READ:
	case ZIO_TYPE_WRITE:
		bp->bio_cmd = zio->io_type == ZIO_TYPE_READ ? BIO_READ : BIO_WRITE;
		bp->bio_data = zio->io_data;
		bp->bio_offset = zio->io_offset;
		bp->bio_length = zio->io_size;
		break;
	case ZIO_TYPE_IOCTL:
		bp->bio_cmd = BIO_FLUSH;
		bp->bio_data = NULL;
		bp->bio_offset = cp->provider->mediasize;
		bp->bio_length = 0;
		break;
	}
	bp->bio_done = vdev_geom_io_intr;

	g_io_request(bp, cp);
}

static void
vdev_geom_io_done(zio_t *zio)
{
	vdev_queue_io_done(zio);

	if (zio->io_type == ZIO_TYPE_WRITE)
		vdev_cache_write(zio);

	if (zio_injection_enabled && zio->io_error == 0)
		zio->io_error = zio_handle_device_injection(zio->io_vd, EIO);

	zio_next_stage(zio);
}

vdev_ops_t vdev_geom_ops = {
	vdev_geom_open,
	vdev_geom_close,
	vdev_default_asize,
	vdev_geom_io_start,
	vdev_geom_io_done,
	NULL,
	VDEV_TYPE_DISK,		/* name of this vdev type */
	B_TRUE			/* leaf vdev */
};
