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
#include <sys/disk.h>
#include <sys/spa.h>
#include <sys/spa_impl.h>
#include <sys/vdev_impl.h>
#include <sys/fs/zfs.h>
#include <sys/zio.h>
#include <geom/geom.h>
#include <geom/geom_int.h>

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
	if (cp->acr + cp->acw + cp->ace > 0)
		g_access(cp, -cp->acr, -cp->acw, -cp->ace);
	ZFS_LOG(1, "Destroyed consumer to %s.", cp->provider->name);
	g_detach(cp);
	g_destroy_consumer(cp);
	/* Destroy geom if there are no consumers left. */
	if (LIST_EMPTY(&gp->consumer)) {
		ZFS_LOG(1, "Destroyed geom %s.", gp->name);
		g_wither_geom(gp, error);
	}
	vdev_geom_release(vd);

	vd->vdev_remove_wanted = B_TRUE;
	spa_async_request(vd->vdev_spa, SPA_ASYNC_REMOVE);
}

static struct g_consumer *
vdev_geom_attach(struct g_provider *pp)
{
	struct g_geom *gp;
	struct g_consumer *cp;

	g_topology_assert();

	ZFS_LOG(1, "Attaching to %s.", pp->name);
	/* Do we have geom already? No? Create one. */
	LIST_FOREACH(gp, &zfs_vdev_class.geom, geom) {
		if (gp->flags & G_GEOM_WITHER)
			continue;
		if (strcmp(gp->name, "zfs::vdev") != 0)
			continue;
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
		if (g_access(cp, 1, 0, 1) != 0) {
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
			if (g_access(cp, 1, 0, 1) != 0) {
				g_detach(cp);
				g_destroy_consumer(cp);
				return (NULL);
			}
			ZFS_LOG(1, "Created consumer for %s.", pp->name);
		} else {
			if (g_access(cp, 1, 0, 1) != 0)
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

	thread_lock(curthread);
	sched_prio(curthread, PRIBIO);
	thread_unlock(curthread);

	ctx = arg;
	for (;;) {
		mtx_lock(&ctx->gc_queue_mtx);
		bp = bioq_takefirst(&ctx->gc_queue);
		if (bp == NULL) {
			if (ctx->gc_state == 1) {
				ctx->gc_state = 2;
				wakeup_one(&ctx->gc_state);
				mtx_unlock(&ctx->gc_queue_mtx);
				kthread_exit();
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
		zio_interrupt(zio);
	}
}

static uint64_t
nvlist_get_guid(nvlist_t *list)
{
	nvpair_t *elem = NULL;
	uint64_t value;

	while ((elem = nvlist_next_nvpair(list, elem)) != NULL) {
		if (nvpair_type(elem) == DATA_TYPE_UINT64 &&
		    strcmp(nvpair_name(elem), "guid") == 0) {
			VERIFY(nvpair_value_uint64(elem, &value) == 0);
			return (value);
		}
	}
	return (0);
}

static int
vdev_geom_io(struct g_consumer *cp, int cmd, void *data, off_t offset, off_t size)
{
	struct bio *bp;
	u_char *p;
	off_t off;
	int error;

	ASSERT((offset % cp->provider->sectorsize) == 0);
	ASSERT((size % cp->provider->sectorsize) == 0);

	bp = g_alloc_bio();
	off = offset;
	offset += size;
	p = data;
	error = 0;

	for (; off < offset; off += MAXPHYS, p += MAXPHYS, size -= MAXPHYS) {
		bzero(bp, sizeof(*bp));
		bp->bio_cmd = cmd;
		bp->bio_done = NULL;
		bp->bio_offset = off;
		bp->bio_length = MIN(size, MAXPHYS);
		bp->bio_data = p;
		g_io_request(bp, cp);
		error = biowait(bp, "vdev_geom_io");
		if (error != 0)
			break;
	}

	g_destroy_bio(bp);
	return (error);
}

static uint64_t
vdev_geom_read_guid(struct g_consumer *cp)
{
	struct g_provider *pp;
	vdev_label_t *label;
	char *p, *buf;
	size_t buflen;
	uint64_t psize;
	off_t offset, size;
	uint64_t guid;
	int error, l, len, iszvol;

	g_topology_assert_not();

	pp = cp->provider;
	ZFS_LOG(1, "Reading guid from %s...", pp->name);
	if (g_getattr("ZFS::iszvol", cp, &iszvol) == 0 && iszvol) {
		ZFS_LOG(1, "Skipping ZVOL-based provider %s.", pp->name);
		return (0);
	}

	psize = pp->mediasize;
	psize = P2ALIGN(psize, (uint64_t)sizeof(vdev_label_t));

	size = sizeof(*label) + pp->sectorsize -
	    ((sizeof(*label) - 1) % pp->sectorsize) - 1;

	guid = 0;
	label = kmem_alloc(size, KM_SLEEP);
	buflen = sizeof(label->vl_vdev_phys.vp_nvlist);

	for (l = 0; l < VDEV_LABELS; l++) {
		nvlist_t *config = NULL;

		offset = vdev_label_offset(psize, l, 0);
		if ((offset % pp->sectorsize) != 0)
			continue;

		if (vdev_geom_io(cp, BIO_READ, label, offset, size) != 0)
			continue;
		buf = label->vl_vdev_phys.vp_nvlist;

		if (nvlist_unpack(buf, buflen, &config, 0) != 0)
			continue;

		guid = nvlist_get_guid(config);
		nvlist_free(config);
		if (guid != 0)
			break;
	}

	kmem_free(label, size);
	if (guid != 0)
		ZFS_LOG(1, "guid for %s is %ju", pp->name, (uintmax_t)guid);
	return (guid);
}

struct vdev_geom_find {
	uint64_t guid;
	struct g_consumer *cp;
};

static void
vdev_geom_taste_orphan(struct g_consumer *cp)
{

	KASSERT(1 == 0, ("%s called while tasting %s.", __func__,
	    cp->provider->name));
}

static void
vdev_geom_attach_by_guid_event(void *arg, int flags __unused)
{
	struct vdev_geom_find *ap;
	struct g_class *mp;
	struct g_geom *gp, *zgp;
	struct g_provider *pp;
	struct g_consumer *zcp;
	uint64_t guid;

	g_topology_assert();

	ap = arg;

	zgp = g_new_geomf(&zfs_vdev_class, "zfs::vdev::taste");
	/* This orphan function should be never called. */
	zgp->orphan = vdev_geom_taste_orphan;
	zcp = g_new_consumer(zgp);

	LIST_FOREACH(mp, &g_classes, class) {
		if (mp == &zfs_vdev_class)
			continue;
		LIST_FOREACH(gp, &mp->geom, geom) {
			if (gp->flags & G_GEOM_WITHER)
				continue;
			LIST_FOREACH(pp, &gp->provider, provider) {
				if (pp->flags & G_PF_WITHER)
					continue;
				g_attach(zcp, pp);
				if (g_access(zcp, 1, 0, 0) != 0) {
					g_detach(zcp);
					continue;
				}
				g_topology_unlock();
				guid = vdev_geom_read_guid(zcp);
				g_topology_lock();
				g_access(zcp, -1, 0, 0);
				g_detach(zcp);
				if (guid != ap->guid)
					continue;
				ap->cp = vdev_geom_attach(pp);
				if (ap->cp == NULL) {
					printf("ZFS WARNING: Unable to attach to %s.",
					    pp->name);
					continue;
				}
				goto end;
			}
		}
	}
	ap->cp = NULL;
end:
	g_destroy_consumer(zcp);
	g_destroy_geom(zgp);
}

static struct g_consumer *
vdev_geom_attach_by_guid(uint64_t guid)
{
	struct vdev_geom_find *ap;
	struct g_consumer *cp;

	ap = kmem_zalloc(sizeof(*ap), KM_SLEEP);
	ap->guid = guid;
	g_waitfor_event(vdev_geom_attach_by_guid_event, ap, M_WAITOK, NULL);
	cp = ap->cp;
	kmem_free(ap, sizeof(*ap));
	return (cp);
}

static struct g_consumer *
vdev_geom_open_by_guid(vdev_t *vd)
{
	struct g_consumer *cp;
	char *buf;
	size_t len;

	ZFS_LOG(1, "Searching by guid [%ju].", (uintmax_t)vd->vdev_guid);
	cp = vdev_geom_attach_by_guid(vd->vdev_guid);
	if (cp != NULL) {
		len = strlen(cp->provider->name) + strlen("/dev/") + 1;
		buf = kmem_alloc(len, KM_SLEEP);

		snprintf(buf, len, "/dev/%s", cp->provider->name);
		spa_strfree(vd->vdev_path);
		vd->vdev_path = buf;

		ZFS_LOG(1, "Attach by guid [%ju] succeeded, provider %s.",
		    (uintmax_t)vd->vdev_guid, vd->vdev_path);
	} else {
		ZFS_LOG(1, "Search by guid [%ju] failed.",
		    (uintmax_t)vd->vdev_guid);
	}

	return (cp);
}

static struct g_consumer *
vdev_geom_open_by_path(vdev_t *vd, int check_guid)
{
	struct g_provider *pp;
	struct g_consumer *cp;
	uint64_t guid;

	cp = NULL;
	g_topology_lock();
	pp = g_provider_by_name(vd->vdev_path + sizeof("/dev/") - 1);
	if (pp != NULL) {
		ZFS_LOG(1, "Found provider by name %s.", vd->vdev_path);
		cp = vdev_geom_attach(pp);
		if (cp != NULL && check_guid) {
			g_topology_unlock();
			guid = vdev_geom_read_guid(cp);
			g_topology_lock();
			if (guid != vd->vdev_guid) {
				vdev_geom_detach(cp, 0);
				cp = NULL;
				ZFS_LOG(1, "guid mismatch for provider %s: "
				    "%ju != %ju.", vd->vdev_path,
				    (uintmax_t)vd->vdev_guid, (uintmax_t)guid);
			} else {
				ZFS_LOG(1, "guid match for provider %s.",
				    vd->vdev_path);
			}
		}
	}
	g_topology_unlock();

	return (cp);
}

static int
vdev_geom_open(vdev_t *vd, uint64_t *psize, uint64_t *ashift)
{
	vdev_geom_ctx_t *ctx;
	struct g_provider *pp;
	struct g_consumer *cp;
	int error, owned;

	/*
	 * We must have a pathname, and it must be absolute.
	 */
	if (vd->vdev_path == NULL || vd->vdev_path[0] != '/') {
		vd->vdev_stat.vs_aux = VDEV_AUX_BAD_LABEL;
		return (EINVAL);
	}

	vd->vdev_tsd = NULL;

	if ((owned = mtx_owned(&Giant)))
		mtx_unlock(&Giant);
	error = 0;

	/*
	 * If we're creating pool, just find GEOM provider by its name
	 * and ignore GUID mismatches.
	 */
	if (vd->vdev_spa->spa_load_state == SPA_LOAD_NONE)
		cp = vdev_geom_open_by_path(vd, 0);
	else {
		cp = vdev_geom_open_by_path(vd, 1);
		if (cp == NULL) {
			/*
			 * The device at vd->vdev_path doesn't have the
			 * expected guid. The disks might have merely
			 * moved around so try all other GEOM providers
			 * to find one with the right guid.
			 */
			cp = vdev_geom_open_by_guid(vd);
		}
	}

	if (cp == NULL) {
		ZFS_LOG(1, "Provider %s not found.", vd->vdev_path);
		error = ENOENT;
	} else if (cp->acw == 0 && (spa_mode & FWRITE) != 0) {
		g_topology_lock();
		error = g_access(cp, 0, 1, 0);
		if (error != 0) {
			printf("ZFS WARNING: Unable to open %s for writing (error=%d).",
			    vd->vdev_path, error);
			vdev_geom_detach(cp, 0);
			cp = NULL;
		}
		g_topology_unlock();
	}
	if (owned)
		mtx_lock(&Giant);
	if (cp == NULL) {
		vd->vdev_stat.vs_aux = VDEV_AUX_OPEN_FAILED;
		return (error);
	}

	cp->private = vd;

	ctx = kmem_zalloc(sizeof(*ctx), KM_SLEEP);
	bioq_init(&ctx->gc_queue);
	mtx_init(&ctx->gc_queue_mtx, "zfs:vdev:geom:queue", NULL, MTX_DEF);
	ctx->gc_consumer = cp;
	ctx->gc_state = 0;

	vd->vdev_tsd = ctx;
	pp = cp->provider;

	kproc_kthread_add(vdev_geom_worker, ctx, &zfsproc, NULL, 0, 0,
	    "zfskern", "vdev %s", pp->name);

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

	if ((zio->io_error = bp->bio_error) == 0 && bp->bio_resid != 0)
		zio->io_error = EIO;

	mtx_lock(&ctx->gc_queue_mtx);
	bioq_insert_tail(&ctx->gc_queue, bp);
	wakeup_one(&ctx->gc_queue);
	mtx_unlock(&ctx->gc_queue_mtx);
}

static int
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
		/* XXPOLICY */
		if (!vdev_readable(vd)) {
			zio->io_error = ENXIO;
			return (ZIO_PIPELINE_CONTINUE);
		}

		switch (zio->io_cmd) {

		case DKIOCFLUSHWRITECACHE:

			if (zfs_nocacheflush)
				break;

			if (vd->vdev_nowritecache) {
				zio->io_error = ENOTSUP;
				break;
			}

			goto sendreq;
		default:
			zio->io_error = ENOTSUP;
		}

		return (ZIO_PIPELINE_CONTINUE);
	}
sendreq:
	if (cp == NULL) {
		zio->io_error = ENXIO;
		return (ZIO_PIPELINE_CONTINUE);
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

	return (ZIO_PIPELINE_STOP);
}

static void
vdev_geom_io_done(zio_t *zio)
{
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
