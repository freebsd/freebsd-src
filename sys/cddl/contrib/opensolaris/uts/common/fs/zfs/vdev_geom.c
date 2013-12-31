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
 *
 * Portions Copyright (c) 2012 Martin Matuska <mm@FreeBSD.org>
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

static g_attrchanged_t vdev_geom_attrchanged;
struct g_class zfs_vdev_class = {
	.name = "ZFS::VDEV",
	.version = G_VERSION,
	.attrchanged = vdev_geom_attrchanged,
};

DECLARE_GEOM_CLASS(zfs_vdev_class, zfs_vdev);

SYSCTL_DECL(_vfs_zfs_vdev);
/* Don't send BIO_FLUSH. */
static int vdev_geom_bio_flush_disable = 0;
TUNABLE_INT("vfs.zfs.vdev.bio_flush_disable", &vdev_geom_bio_flush_disable);
SYSCTL_INT(_vfs_zfs_vdev, OID_AUTO, bio_flush_disable, CTLFLAG_RW,
    &vdev_geom_bio_flush_disable, 0, "Disable BIO_FLUSH");
/* Don't send BIO_DELETE. */
static int vdev_geom_bio_delete_disable = 0;
TUNABLE_INT("vfs.zfs.vdev.bio_delete_disable", &vdev_geom_bio_delete_disable);
SYSCTL_INT(_vfs_zfs_vdev, OID_AUTO, bio_delete_disable, CTLFLAG_RW,
    &vdev_geom_bio_delete_disable, 0, "Disable BIO_DELETE");

static void
vdev_geom_set_rotation_rate(vdev_t *vd, struct g_consumer *cp)
{ 
	int error;
	uint16_t rate;

	error = g_getattr("GEOM::rotation_rate", cp, &rate);
	if (error == 0)
		vd->vdev_rotation_rate = rate;
	else
		vd->vdev_rotation_rate = VDEV_RATE_UNKNOWN;
}

static void
vdev_geom_attrchanged(struct g_consumer *cp, const char *attr)
{
	vdev_t *vd;

	vd = cp->private;
	if (vd == NULL)
		return;

	if (strcmp(attr, "GEOM::rotation_rate") == 0) {
		vdev_geom_set_rotation_rate(vd, cp);
		return;
	}
}

static void
vdev_geom_orphan(struct g_consumer *cp)
{
	vdev_t *vd;

	g_topology_assert();

	vd = cp->private;
	if (vd == NULL)
		return;

	/*
	 * Orphan callbacks occur from the GEOM event thread.
	 * Concurrent with this call, new I/O requests may be
	 * working their way through GEOM about to find out
	 * (only once executed by the g_down thread) that we've
	 * been orphaned from our disk provider.  These I/Os
	 * must be retired before we can detach our consumer.
	 * This is most easily achieved by acquiring the
	 * SPA ZIO configuration lock as a writer, but doing
	 * so with the GEOM topology lock held would cause
	 * a lock order reversal.  Instead, rely on the SPA's
	 * async removal support to invoke a close on this
	 * vdev once it is safe to do so.
	 */
	zfs_post_remove(vd->vdev_spa, vd);
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
	cp->flags |= G_CF_DIRECT_SEND | G_CF_DIRECT_RECEIVE;
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

static uint64_t
nvlist_get_guid(nvlist_t *list)
{
	uint64_t value;

	value = 0;
	nvlist_lookup_uint64(list, ZPOOL_CONFIG_GUID, &value);
	return (value);
}

static int
vdev_geom_io(struct g_consumer *cp, int cmd, void *data, off_t offset, off_t size)
{
	struct bio *bp;
	u_char *p;
	off_t off, maxio;
	int error;

	ASSERT((offset % cp->provider->sectorsize) == 0);
	ASSERT((size % cp->provider->sectorsize) == 0);

	bp = g_alloc_bio();
	off = offset;
	offset += size;
	p = data;
	maxio = MAXPHYS - (MAXPHYS % cp->provider->sectorsize);
	error = 0;

	for (; off < offset; off += maxio, p += maxio, size -= maxio) {
		bzero(bp, sizeof(*bp));
		bp->bio_cmd = cmd;
		bp->bio_done = NULL;
		bp->bio_offset = off;
		bp->bio_length = MIN(size, maxio);
		bp->bio_data = p;
		g_io_request(bp, cp);
		error = biowait(bp, "vdev_geom_io");
		if (error != 0)
			break;
	}

	g_destroy_bio(bp);
	return (error);
}

static void
vdev_geom_taste_orphan(struct g_consumer *cp)
{

	KASSERT(1 == 0, ("%s called while tasting %s.", __func__,
	    cp->provider->name));
}

static int
vdev_geom_read_config(struct g_consumer *cp, nvlist_t **config)
{
	struct g_provider *pp;
	vdev_label_t *label;
	char *p, *buf;
	size_t buflen;
	uint64_t psize;
	off_t offset, size;
	uint64_t guid, state, txg;
	int error, l, len;

	g_topology_assert_not();

	pp = cp->provider;
	ZFS_LOG(1, "Reading config from %s...", pp->name);

	psize = pp->mediasize;
	psize = P2ALIGN(psize, (uint64_t)sizeof(vdev_label_t));

	size = sizeof(*label) + pp->sectorsize -
	    ((sizeof(*label) - 1) % pp->sectorsize) - 1;

	guid = 0;
	label = kmem_alloc(size, KM_SLEEP);
	buflen = sizeof(label->vl_vdev_phys.vp_nvlist);

	*config = NULL;
	for (l = 0; l < VDEV_LABELS; l++) {

		offset = vdev_label_offset(psize, l, 0);
		if ((offset % pp->sectorsize) != 0)
			continue;

		if (vdev_geom_io(cp, BIO_READ, label, offset, size) != 0)
			continue;
		buf = label->vl_vdev_phys.vp_nvlist;

		if (nvlist_unpack(buf, buflen, config, 0) != 0)
			continue;

		if (nvlist_lookup_uint64(*config, ZPOOL_CONFIG_POOL_STATE,
		    &state) != 0 || state > POOL_STATE_L2CACHE) {
			nvlist_free(*config);
			*config = NULL;
			continue;
		}

		if (state != POOL_STATE_SPARE && state != POOL_STATE_L2CACHE &&
		    (nvlist_lookup_uint64(*config, ZPOOL_CONFIG_POOL_TXG,
		    &txg) != 0 || txg == 0)) {
			nvlist_free(*config);
			*config = NULL;
			continue;
		}

		break;
	}

	kmem_free(label, size);
	return (*config == NULL ? ENOENT : 0);
}

static void
resize_configs(nvlist_t ***configs, uint64_t *count, uint64_t id)
{
	nvlist_t **new_configs;
	uint64_t i;

	if (id < *count)
		return;
	new_configs = kmem_zalloc((id + 1) * sizeof(nvlist_t *),
	    KM_SLEEP);
	for (i = 0; i < *count; i++)
		new_configs[i] = (*configs)[i];
	if (*configs != NULL)
		kmem_free(*configs, *count * sizeof(void *));
	*configs = new_configs;
	*count = id + 1;
}

static void
process_vdev_config(nvlist_t ***configs, uint64_t *count, nvlist_t *cfg,
    const char *name, uint64_t* known_pool_guid)
{
	nvlist_t *vdev_tree;
	uint64_t pool_guid;
	uint64_t vdev_guid, known_guid;
	uint64_t id, txg, known_txg;
	char *pname;
	int i;

	if (nvlist_lookup_string(cfg, ZPOOL_CONFIG_POOL_NAME, &pname) != 0 ||
	    strcmp(pname, name) != 0)
		goto ignore;

	if (nvlist_lookup_uint64(cfg, ZPOOL_CONFIG_POOL_GUID, &pool_guid) != 0)
		goto ignore;

	if (nvlist_lookup_uint64(cfg, ZPOOL_CONFIG_TOP_GUID, &vdev_guid) != 0)
		goto ignore;

	if (nvlist_lookup_nvlist(cfg, ZPOOL_CONFIG_VDEV_TREE, &vdev_tree) != 0)
		goto ignore;

	if (nvlist_lookup_uint64(vdev_tree, ZPOOL_CONFIG_ID, &id) != 0)
		goto ignore;

	VERIFY(nvlist_lookup_uint64(cfg, ZPOOL_CONFIG_POOL_TXG, &txg) == 0);

	if (*known_pool_guid != 0) {
		if (pool_guid != *known_pool_guid)
			goto ignore;
	} else
		*known_pool_guid = pool_guid;

	resize_configs(configs, count, id);

	if ((*configs)[id] != NULL) {
		VERIFY(nvlist_lookup_uint64((*configs)[id],
		    ZPOOL_CONFIG_POOL_TXG, &known_txg) == 0);
		if (txg <= known_txg)
			goto ignore;
		nvlist_free((*configs)[id]);
	}

	(*configs)[id] = cfg;
	return;

ignore:
	nvlist_free(cfg);
}

static int
vdev_geom_attach_taster(struct g_consumer *cp, struct g_provider *pp)
{
	int error;

	if (pp->flags & G_PF_WITHER)
		return (EINVAL);
	g_attach(cp, pp);
	error = g_access(cp, 1, 0, 0);
	if (error == 0) {
		if (pp->sectorsize > VDEV_PAD_SIZE || !ISP2(pp->sectorsize))
			error = EINVAL;
		else if (pp->mediasize < SPA_MINDEVSIZE)
			error = EINVAL;
		if (error != 0)
			g_access(cp, -1, 0, 0);
	}
	if (error != 0)
		g_detach(cp);
	return (error);
}

static void
vdev_geom_detach_taster(struct g_consumer *cp)
{
	g_access(cp, -1, 0, 0);
	g_detach(cp);
}

int
vdev_geom_read_pool_label(const char *name,
    nvlist_t ***configs, uint64_t *count)
{
	struct g_class *mp;
	struct g_geom *gp, *zgp;
	struct g_provider *pp;
	struct g_consumer *zcp;
	nvlist_t *vdev_cfg;
	uint64_t pool_guid;
	int error;

	DROP_GIANT();
	g_topology_lock();

	zgp = g_new_geomf(&zfs_vdev_class, "zfs::vdev::taste");
	/* This orphan function should be never called. */
	zgp->orphan = vdev_geom_taste_orphan;
	zcp = g_new_consumer(zgp);

	*configs = NULL;
	*count = 0;
	pool_guid = 0;
	LIST_FOREACH(mp, &g_classes, class) {
		if (mp == &zfs_vdev_class)
			continue;
		LIST_FOREACH(gp, &mp->geom, geom) {
			if (gp->flags & G_GEOM_WITHER)
				continue;
			LIST_FOREACH(pp, &gp->provider, provider) {
				if (pp->flags & G_PF_WITHER)
					continue;
				if (vdev_geom_attach_taster(zcp, pp) != 0)
					continue;
				g_topology_unlock();
				error = vdev_geom_read_config(zcp, &vdev_cfg);
				g_topology_lock();
				vdev_geom_detach_taster(zcp);
				if (error)
					continue;
				ZFS_LOG(1, "successfully read vdev config");

				process_vdev_config(configs, count,
				    vdev_cfg, name, &pool_guid);
			}
		}
	}

	g_destroy_consumer(zcp);
	g_destroy_geom(zgp);
	g_topology_unlock();
	PICKUP_GIANT();

	return (*count > 0 ? 0 : ENOENT);
}

static uint64_t
vdev_geom_read_guid(struct g_consumer *cp)
{
	nvlist_t *config;
	uint64_t guid;

	g_topology_assert_not();

	guid = 0;
	if (vdev_geom_read_config(cp, &config) == 0) {
		guid = nvlist_get_guid(config);
		nvlist_free(config);
	}
	return (guid);
}

static struct g_consumer *
vdev_geom_attach_by_guid(uint64_t guid)
{
	struct g_class *mp;
	struct g_geom *gp, *zgp;
	struct g_provider *pp;
	struct g_consumer *cp, *zcp;
	uint64_t pguid;

	g_topology_assert();

	zgp = g_new_geomf(&zfs_vdev_class, "zfs::vdev::taste");
	/* This orphan function should be never called. */
	zgp->orphan = vdev_geom_taste_orphan;
	zcp = g_new_consumer(zgp);

	cp = NULL;
	LIST_FOREACH(mp, &g_classes, class) {
		if (mp == &zfs_vdev_class)
			continue;
		LIST_FOREACH(gp, &mp->geom, geom) {
			if (gp->flags & G_GEOM_WITHER)
				continue;
			LIST_FOREACH(pp, &gp->provider, provider) {
				if (vdev_geom_attach_taster(zcp, pp) != 0)
					continue;
				g_topology_unlock();
				pguid = vdev_geom_read_guid(zcp);
				g_topology_lock();
				vdev_geom_detach_taster(zcp);
				if (pguid != guid)
					continue;
				cp = vdev_geom_attach(pp);
				if (cp == NULL) {
					printf("ZFS WARNING: Unable to attach to %s.\n",
					    pp->name);
					continue;
				}
				break;
			}
			if (cp != NULL)
				break;
		}
		if (cp != NULL)
			break;
	}
end:
	g_destroy_consumer(zcp);
	g_destroy_geom(zgp);
	return (cp);
}

static struct g_consumer *
vdev_geom_open_by_guid(vdev_t *vd)
{
	struct g_consumer *cp;
	char *buf;
	size_t len;

	g_topology_assert();

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

	g_topology_assert();

	cp = NULL;
	pp = g_provider_by_name(vd->vdev_path + sizeof("/dev/") - 1);
	if (pp != NULL) {
		ZFS_LOG(1, "Found provider by name %s.", vd->vdev_path);
		cp = vdev_geom_attach(pp);
		if (cp != NULL && check_guid && ISP2(pp->sectorsize) &&
		    pp->sectorsize <= VDEV_PAD_SIZE) {
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

	return (cp);
}

static int
vdev_geom_open(vdev_t *vd, uint64_t *psize, uint64_t *max_psize,
    uint64_t *logical_ashift, uint64_t *physical_ashift)
{
	struct g_provider *pp;
	struct g_consumer *cp;
	size_t bufsize;
	int error;

	/*
	 * We must have a pathname, and it must be absolute.
	 */
	if (vd->vdev_path == NULL || vd->vdev_path[0] != '/') {
		vd->vdev_stat.vs_aux = VDEV_AUX_BAD_LABEL;
		return (EINVAL);
	}

	vd->vdev_tsd = NULL;

	DROP_GIANT();
	g_topology_lock();
	error = 0;

	/*
	 * If we're creating or splitting a pool, just find the GEOM provider
	 * by its name and ignore GUID mismatches.
	 */
	if (vd->vdev_spa->spa_load_state == SPA_LOAD_NONE ||
	    vd->vdev_spa->spa_splitting_newspa == B_TRUE)
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
	} else if (cp->provider->sectorsize > VDEV_PAD_SIZE ||
	    !ISP2(cp->provider->sectorsize)) {
		ZFS_LOG(1, "Provider %s has unsupported sectorsize.",
		    vd->vdev_path);
		vdev_geom_detach(cp, 0);
		error = EINVAL;
		cp = NULL;
	} else if (cp->acw == 0 && (spa_mode(vd->vdev_spa) & FWRITE) != 0) {
		int i;

		for (i = 0; i < 5; i++) {
			error = g_access(cp, 0, 1, 0);
			if (error == 0)
				break;
			g_topology_unlock();
			tsleep(vd, 0, "vdev", hz / 2);
			g_topology_lock();
		}
		if (error != 0) {
			printf("ZFS WARNING: Unable to open %s for writing (error=%d).\n",
			    vd->vdev_path, error);
			vdev_geom_detach(cp, 0);
			cp = NULL;
		}
	}
	g_topology_unlock();
	PICKUP_GIANT();
	if (cp == NULL) {
		vd->vdev_stat.vs_aux = VDEV_AUX_OPEN_FAILED;
		return (error);
	}

	cp->private = vd;
	vd->vdev_tsd = cp;
	pp = cp->provider;

	/*
	 * Determine the actual size of the device.
	 */
	*max_psize = *psize = pp->mediasize;

	/*
	 * Determine the device's minimum transfer size and preferred
	 * transfer size.
	 */
	*logical_ashift = highbit(MAX(pp->sectorsize, SPA_MINBLOCKSIZE)) - 1;
	*physical_ashift = 0;
	if (pp->stripesize)
		*physical_ashift = highbit(pp->stripesize) - 1;

	/*
	 * Clear the nowritecache settings, so that on a vdev_reopen()
	 * we will try again.
	 */
	vd->vdev_nowritecache = B_FALSE;

	if (vd->vdev_physpath != NULL)
		spa_strfree(vd->vdev_physpath);
	bufsize = sizeof("/dev/") + strlen(pp->name);
	vd->vdev_physpath = kmem_alloc(bufsize, KM_SLEEP);
	snprintf(vd->vdev_physpath, bufsize, "/dev/%s", pp->name);

	/*
	 * Determine the device's rotation rate.
	 */
	vdev_geom_set_rotation_rate(vd, cp);

	return (0);
}

static void
vdev_geom_close(vdev_t *vd)
{
	struct g_consumer *cp;

	cp = vd->vdev_tsd;
	if (cp == NULL)
		return;
	vd->vdev_tsd = NULL;
	vd->vdev_delayed_close = B_FALSE;
	cp->private = NULL;	/* XXX locking */
	g_post_event(vdev_geom_detach, cp, M_WAITOK, NULL);
}

static void
vdev_geom_io_intr(struct bio *bp)
{
	vdev_t *vd;
	zio_t *zio;

	zio = bp->bio_caller1;
	vd = zio->io_vd;
	zio->io_error = bp->bio_error;
	if (zio->io_error == 0 && bp->bio_resid != 0)
		zio->io_error = EIO;
	if (bp->bio_cmd == BIO_FLUSH && bp->bio_error == ENOTSUP) {
		/*
		 * If we get ENOTSUP, we know that no future
		 * attempts will ever succeed.  In this case we
		 * set a persistent bit so that we don't bother
		 * with the ioctl in the future.
		 */
		vd->vdev_nowritecache = B_TRUE;
	}
	if (bp->bio_cmd == BIO_DELETE && bp->bio_error == ENOTSUP) {
		/*
		 * If we get ENOTSUP, we know that no future
		 * attempts will ever succeed.  In this case we
		 * set a persistent bit so that we don't bother
		 * with the ioctl in the future.
		 */
		vd->vdev_notrim = B_TRUE;
	}
	if (zio->io_error == ENXIO && !vd->vdev_remove_wanted) {
		/*
		 * If provider's error is set we assume it is being
		 * removed.
		 */
		if (bp->bio_to->error != 0) {
			vd->vdev_remove_wanted = B_TRUE;
			spa_async_request(zio->io_spa, SPA_ASYNC_REMOVE);
		} else if (!vd->vdev_delayed_close) {
			vd->vdev_delayed_close = B_TRUE;
		}
	}
	g_destroy_bio(bp);
	zio_interrupt(zio);
}

static int
vdev_geom_io_start(zio_t *zio)
{
	vdev_t *vd;
	struct g_consumer *cp;
	struct bio *bp;
	int error;

	vd = zio->io_vd;

	if (zio->io_type == ZIO_TYPE_IOCTL) {
		/* XXPOLICY */
		if (!vdev_readable(vd)) {
			zio->io_error = ENXIO;
			return (ZIO_PIPELINE_CONTINUE);
		}

		switch (zio->io_cmd) {
		case DKIOCFLUSHWRITECACHE:
			if (zfs_nocacheflush || vdev_geom_bio_flush_disable)
				break;
			if (vd->vdev_nowritecache) {
				zio->io_error = ENOTSUP;
				break;
			}
			goto sendreq;
		case DKIOCTRIM:
			if (vdev_geom_bio_delete_disable)
				break;
			if (vd->vdev_notrim) {
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
	cp = vd->vdev_tsd;
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
		switch (zio->io_cmd) {
		case DKIOCFLUSHWRITECACHE:
			bp->bio_cmd = BIO_FLUSH;
			bp->bio_flags |= BIO_ORDERED;
			bp->bio_data = NULL;
			bp->bio_offset = cp->provider->mediasize;
			bp->bio_length = 0;
			break;
		case DKIOCTRIM:
			bp->bio_cmd = BIO_DELETE;
			bp->bio_data = NULL;
			bp->bio_offset = zio->io_offset;
			bp->bio_length = zio->io_size;
			break;
		}
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

static void
vdev_geom_hold(vdev_t *vd)
{
}

static void
vdev_geom_rele(vdev_t *vd)
{
}

vdev_ops_t vdev_geom_ops = {
	vdev_geom_open,
	vdev_geom_close,
	vdev_default_asize,
	vdev_geom_io_start,
	vdev_geom_io_done,
	NULL,
	vdev_geom_hold,
	vdev_geom_rele,
	VDEV_TYPE_DISK,		/* name of this vdev type */
	B_TRUE			/* leaf vdev */
};
