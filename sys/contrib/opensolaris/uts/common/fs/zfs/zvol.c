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
/*
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * ZFS volume emulation driver.
 *
 * Makes a DMU object look like a volume of arbitrary size, up to 2^64 bytes.
 * Volumes are accessed through the symbolic links named:
 *
 * /dev/zvol/dsk/<pool_name>/<dataset_name>
 * /dev/zvol/rdsk/<pool_name>/<dataset_name>
 *
 * These links are created by the ZFS-specific devfsadm link generator.
 * Volumes are persistent through reboot.  No user command needs to be
 * run before opening and using a device.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/cmn_err.h>
#include <sys/stat.h>
#include <sys/zap.h>
#include <sys/spa.h>
#include <sys/zio.h>
#include <sys/dsl_prop.h>
#include <sys/dkio.h>
#include <sys/byteorder.h>
#include <sys/sunddi.h>
#include <sys/dirent.h>
#include <sys/policy.h>
#include <sys/fs/zfs.h>
#include <sys/zfs_ioctl.h>
#include <sys/zil.h>
#include <sys/refcount.h>
#include <sys/zfs_znode.h>
#include <sys/zfs_rlock.h>
#include <geom/geom.h>

#include "zfs_namecheck.h"

struct g_class zfs_zvol_class = {
	.name = "ZFS::ZVOL",
	.version = G_VERSION,
};

DECLARE_GEOM_CLASS(zfs_zvol_class, zfs_zvol);

#define	ZVOL_OBJ		1ULL
#define	ZVOL_ZAP_OBJ		2ULL

static uint32_t zvol_minors;

/*
 * The in-core state of each volume.
 */
typedef struct zvol_state {
	char		zv_name[MAXPATHLEN]; /* pool/dd name */
	uint64_t	zv_volsize;	/* amount of space we advertise */
	uint64_t	zv_volblocksize; /* volume block size */
	struct g_provider *zv_provider;	/* GEOM provider */
	uint8_t		zv_min_bs;	/* minimum addressable block shift */
	uint8_t		zv_readonly;	/* hard readonly; like write-protect */
	objset_t	*zv_objset;	/* objset handle */
	uint32_t	zv_mode;	/* DS_MODE_* flags at open time */
	uint32_t	zv_total_opens;	/* total open count */
	zilog_t		*zv_zilog;	/* ZIL handle */
	uint64_t	zv_txg_assign;	/* txg to assign during ZIL replay */
	znode_t		zv_znode;	/* for range locking */
	int		zv_state;
	struct bio_queue_head zv_queue;
	struct mtx	zv_queue_mtx;	/* zv_queue mutex */
} zvol_state_t;

/*
 * zvol maximum transfer in one DMU tx.
 */
int zvol_maxphys = DMU_MAX_ACCESS/2;

static int zvol_get_data(void *arg, lr_write_t *lr, char *buf, zio_t *zio);

int
zvol_check_volsize(uint64_t volsize, uint64_t blocksize)
{
	if (volsize == 0)
		return (EINVAL);

	if (volsize % blocksize != 0)
		return (EINVAL);

#ifdef _ILP32
	if (volsize - 1 > SPEC_MAXOFFSET_T)
		return (EOVERFLOW);
#endif
	return (0);
}

int
zvol_check_volblocksize(uint64_t volblocksize)
{
	if (volblocksize < SPA_MINBLOCKSIZE ||
	    volblocksize > SPA_MAXBLOCKSIZE ||
	    !ISP2(volblocksize))
		return (EDOM);

	return (0);
}

static void
zvol_readonly_changed_cb(void *arg, uint64_t newval)
{
	zvol_state_t *zv = arg;

	zv->zv_readonly = (uint8_t)newval;
}

int
zvol_get_stats(objset_t *os, nvlist_t *nv)
{
	int error;
	dmu_object_info_t doi;
	uint64_t val;


	error = zap_lookup(os, ZVOL_ZAP_OBJ, "size", 8, 1, &val);
	if (error)
		return (error);

	dsl_prop_nvlist_add_uint64(nv, ZFS_PROP_VOLSIZE, val);

	error = dmu_object_info(os, ZVOL_OBJ, &doi);

	if (error == 0) {
		dsl_prop_nvlist_add_uint64(nv, ZFS_PROP_VOLBLOCKSIZE,
		    doi.doi_data_block_size);
	}

	return (error);
}

static zvol_state_t *
zvol_minor_lookup(const char *name)
{
	struct g_provider *pp;
	struct g_geom *gp;

	g_topology_assert();

	LIST_FOREACH(gp, &zfs_zvol_class.geom, geom) {
		LIST_FOREACH(pp, &gp->provider, provider) {
			if (strcmp(pp->name + sizeof(ZVOL_DEV_DIR), name) == 0)
				return (pp->private);
		}
	}

	return (NULL);
}

static int
zvol_access(struct g_provider *pp, int acr, int acw, int ace)
{
	zvol_state_t *zv;

	g_topology_assert();

	zv = pp->private;
	if (zv == NULL) {
		if (acr <= 0 && acw <= 0 && ace <= 0)
			return (0);
		return (pp->error);
	}

	ASSERT(zv->zv_objset != NULL);

	if (acw > 0 && (zv->zv_readonly || (zv->zv_mode & DS_MODE_READONLY)))
		return (EROFS);

	zv->zv_total_opens += acr + acw + ace;

	return (0);
}

/*
 * zvol_log_write() handles synchronous writes using TX_WRITE ZIL transactions.
 *
 * We store data in the log buffers if it's small enough.
 * Otherwise we will later flush the data out via dmu_sync().
 */
ssize_t zvol_immediate_write_sz = 32768;

static void
zvol_log_write(zvol_state_t *zv, dmu_tx_t *tx, offset_t off, ssize_t len)
{
	uint32_t blocksize = zv->zv_volblocksize;
	lr_write_t *lr;

	while (len) {
		ssize_t nbytes = MIN(len, blocksize - P2PHASE(off, blocksize));
		itx_t *itx = zil_itx_create(TX_WRITE, sizeof (*lr));

		itx->itx_wr_state =
		    len > zvol_immediate_write_sz ?  WR_INDIRECT : WR_NEED_COPY;
		itx->itx_private = zv;
		lr = (lr_write_t *)&itx->itx_lr;
		lr->lr_foid = ZVOL_OBJ;
		lr->lr_offset = off;
		lr->lr_length = nbytes;
		lr->lr_blkoff = off - P2ALIGN_TYPED(off, blocksize, uint64_t);
		BP_ZERO(&lr->lr_blkptr);

		(void) zil_itx_assign(zv->zv_zilog, itx, tx);
		len -= nbytes;
		off += nbytes;
	}
}

static void
zvol_start(struct bio *bp)
{
	zvol_state_t *zv;

	switch (bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
	case BIO_FLUSH:
		zv = bp->bio_to->private;
		ASSERT(zv != NULL);
		mtx_lock(&zv->zv_queue_mtx);
		bioq_insert_tail(&zv->zv_queue, bp);
		wakeup_one(&zv->zv_queue);
		mtx_unlock(&zv->zv_queue_mtx);
		break;
	case BIO_DELETE:
	case BIO_GETATTR:
	default:
		g_io_deliver(bp, EOPNOTSUPP);
		break;
	}
}

static void
zvol_serve_one(zvol_state_t *zv, struct bio *bp)
{
	uint64_t off, volsize;
	size_t size, resid;
	char *addr;
	objset_t *os;
	rl_t *rl;
	int error = 0;
	boolean_t reading;

	off = bp->bio_offset;
	volsize = zv->zv_volsize;

	os = zv->zv_objset;
	ASSERT(os != NULL);

	addr = bp->bio_data;
	resid = bp->bio_length;

	error = 0;

	/*
	 * There must be no buffer changes when doing a dmu_sync() because
	 * we can't change the data whilst calculating the checksum.
	 * A better approach than a per zvol rwlock would be to lock ranges.
	 */
	reading = (bp->bio_cmd == BIO_READ);
	rl = zfs_range_lock(&zv->zv_znode, off, resid,
	    reading ? RL_READER : RL_WRITER);

	while (resid != 0 && off < volsize) {

		size = MIN(resid, zvol_maxphys); /* zvol_maxphys per tx */

		if (size > volsize - off)	/* don't write past the end */
			size = volsize - off;

		if (reading) {
			error = dmu_read(os, ZVOL_OBJ, off, size, addr);
		} else {
			dmu_tx_t *tx = dmu_tx_create(os);
			dmu_tx_hold_write(tx, ZVOL_OBJ, off, size);
			error = dmu_tx_assign(tx, TXG_WAIT);
			if (error) {
				dmu_tx_abort(tx);
			} else {
				dmu_write(os, ZVOL_OBJ, off, size, addr, tx);
				zvol_log_write(zv, tx, off, size);
				dmu_tx_commit(tx);
			}
		}
		if (error)
			break;
		off += size;
		addr += size;
		resid -= size;
	}
	zfs_range_unlock(rl);

	bp->bio_completed = bp->bio_length - resid;
	if (bp->bio_completed < bp->bio_length)
		bp->bio_error = (off > volsize ? EINVAL : error);
}

static void
zvol_worker(void *arg)
{
	zvol_state_t *zv;
	struct bio *bp;

	zv = arg;
	for (;;) {
		mtx_lock(&zv->zv_queue_mtx);
		bp = bioq_takefirst(&zv->zv_queue);
		if (bp == NULL) {
			if (zv->zv_state == 1) {
				zv->zv_state = 2;
				wakeup(&zv->zv_state);
				mtx_unlock(&zv->zv_queue_mtx);
				kthread_exit(0);
			}
			msleep(&zv->zv_queue, &zv->zv_queue_mtx, PRIBIO | PDROP,
			    "zvol:io", 0);
			continue;
		}
		mtx_unlock(&zv->zv_queue_mtx);
		switch (bp->bio_cmd) {
		case BIO_FLUSH:
			break;
		case BIO_READ:
		case BIO_WRITE:
			zvol_serve_one(zv, bp);
			break;
		}

		if (bp->bio_cmd != BIO_READ && !zil_disable)
			zil_commit(zv->zv_zilog, UINT64_MAX, ZVOL_OBJ);

		g_io_deliver(bp, bp->bio_error);
	}
}

void
zvol_create_cb(objset_t *os, void *arg, dmu_tx_t *tx)
{
	zfs_create_data_t *zc = arg;
	int error;
	uint64_t volblocksize, volsize;

	VERIFY(nvlist_lookup_uint64(zc->zc_props,
	    zfs_prop_to_name(ZFS_PROP_VOLSIZE), &volsize) == 0);
	if (nvlist_lookup_uint64(zc->zc_props,
	    zfs_prop_to_name(ZFS_PROP_VOLBLOCKSIZE), &volblocksize) != 0)
		volblocksize = zfs_prop_default_numeric(ZFS_PROP_VOLBLOCKSIZE);

	/*
	 * These properites must be removed from the list so the generic
	 * property setting step won't apply to them.
	 */
	VERIFY(nvlist_remove_all(zc->zc_props,
	    zfs_prop_to_name(ZFS_PROP_VOLSIZE)) == 0);
	(void) nvlist_remove_all(zc->zc_props,
	    zfs_prop_to_name(ZFS_PROP_VOLBLOCKSIZE));

	error = dmu_object_claim(os, ZVOL_OBJ, DMU_OT_ZVOL, volblocksize,
	    DMU_OT_NONE, 0, tx);
	ASSERT(error == 0);

	error = zap_create_claim(os, ZVOL_ZAP_OBJ, DMU_OT_ZVOL_PROP,
	    DMU_OT_NONE, 0, tx);
	ASSERT(error == 0);

	error = zap_update(os, ZVOL_ZAP_OBJ, "size", 8, 1, &volsize, tx);
	ASSERT(error == 0);
}

/*
 * Replay a TX_WRITE ZIL transaction that didn't get committed
 * after a system failure
 */
static int
zvol_replay_write(zvol_state_t *zv, lr_write_t *lr, boolean_t byteswap)
{
	objset_t *os = zv->zv_objset;
	char *data = (char *)(lr + 1);	/* data follows lr_write_t */
	uint64_t off = lr->lr_offset;
	uint64_t len = lr->lr_length;
	dmu_tx_t *tx;
	int error;

	if (byteswap)
		byteswap_uint64_array(lr, sizeof (*lr));

	tx = dmu_tx_create(os);
	dmu_tx_hold_write(tx, ZVOL_OBJ, off, len);
	error = dmu_tx_assign(tx, zv->zv_txg_assign);
	if (error) {
		dmu_tx_abort(tx);
	} else {
		dmu_write(os, ZVOL_OBJ, off, len, data, tx);
		dmu_tx_commit(tx);
	}

	return (error);
}

/* ARGSUSED */
static int
zvol_replay_err(zvol_state_t *zv, lr_t *lr, boolean_t byteswap)
{
	return (ENOTSUP);
}

/*
 * Callback vectors for replaying records.
 * Only TX_WRITE is needed for zvol.
 */
zil_replay_func_t *zvol_replay_vector[TX_MAX_TYPE] = {
	zvol_replay_err,	/* 0 no such transaction type */
	zvol_replay_err,	/* TX_CREATE */
	zvol_replay_err,	/* TX_MKDIR */
	zvol_replay_err,	/* TX_MKXATTR */
	zvol_replay_err,	/* TX_SYMLINK */
	zvol_replay_err,	/* TX_REMOVE */
	zvol_replay_err,	/* TX_RMDIR */
	zvol_replay_err,	/* TX_LINK */
	zvol_replay_err,	/* TX_RENAME */
	zvol_replay_write,	/* TX_WRITE */
	zvol_replay_err,	/* TX_TRUNCATE */
	zvol_replay_err,	/* TX_SETATTR */
	zvol_replay_err,	/* TX_ACL */
};

/*
 * Create a minor node for the specified volume.
 */
int
zvol_create_minor(const char *name, dev_t dev)
{
	struct g_provider *pp;
	struct g_geom *gp;
	zvol_state_t *zv;
	objset_t *os;
	dmu_object_info_t doi;
	uint64_t volsize;
	int ds_mode = DS_MODE_PRIMARY;
	int error;

	DROP_GIANT();
	g_topology_lock();

	if ((zv = zvol_minor_lookup(name)) != NULL) {
		error = EEXIST;
		goto end;
	}

	if (strchr(name, '@') != 0)
		ds_mode |= DS_MODE_READONLY;

	error = dmu_objset_open(name, DMU_OST_ZVOL, ds_mode, &os);
	if (error)
		goto end;

	g_topology_unlock();
	PICKUP_GIANT();
	error = zap_lookup(os, ZVOL_ZAP_OBJ, "size", 8, 1, &volsize);
	DROP_GIANT();
	g_topology_lock();
	if (error) {
		dmu_objset_close(os);
		goto end;
	}

	gp = g_new_geomf(&zfs_zvol_class, "zfs::zvol::%s", name);
	gp->start = zvol_start;
	gp->access = zvol_access;
	pp = g_new_providerf(gp, "%s/%s", ZVOL_DEV_DIR, name);
	pp->mediasize = volsize;
	pp->sectorsize = DEV_BSIZE;

	zv = kmem_zalloc(sizeof(*zv), KM_SLEEP);
	(void) strcpy(zv->zv_name, name);
	zv->zv_min_bs = DEV_BSHIFT;
	zv->zv_provider = pp;
	zv->zv_volsize = pp->mediasize;
	zv->zv_objset = os;
	zv->zv_mode = ds_mode;
	zv->zv_zilog = zil_open(os, zvol_get_data);
	mutex_init(&zv->zv_znode.z_range_lock, NULL, MUTEX_DEFAULT, NULL);
	avl_create(&zv->zv_znode.z_range_avl, zfs_range_compare,
	    sizeof (rl_t), offsetof(rl_t, r_node));


	/* get and cache the blocksize */
	error = dmu_object_info(os, ZVOL_OBJ, &doi);
	ASSERT(error == 0);
	zv->zv_volblocksize = doi.doi_data_block_size;

	zil_replay(os, zv, &zv->zv_txg_assign, zvol_replay_vector);

	/* XXX this should handle the possible i/o error */
	VERIFY(dsl_prop_register(dmu_objset_ds(zv->zv_objset),
	    "readonly", zvol_readonly_changed_cb, zv) == 0);

	pp->private = zv;
	g_error_provider(pp, 0);

	bioq_init(&zv->zv_queue);
	mtx_init(&zv->zv_queue_mtx, "zvol", NULL, MTX_DEF);
	zv->zv_state = 0;
	kthread_create(zvol_worker, zv, NULL, 0, 0, "zvol:worker %s", pp->name);

	zvol_minors++;
end:
	g_topology_unlock();
	PICKUP_GIANT();

	return (error);
}

/*
 * Remove minor node for the specified volume.
 */
int
zvol_remove_minor(const char *name)
{
	struct g_provider *pp;
	zvol_state_t *zv;
	int error = 0;

	DROP_GIANT();
	g_topology_lock();

	if ((zv = zvol_minor_lookup(name)) == NULL) {
		error = ENXIO;
		goto end;
	}

	if (zv->zv_total_opens != 0) {
		error = EBUSY;
		goto end;
	}

	VERIFY(dsl_prop_unregister(dmu_objset_ds(zv->zv_objset),
	    "readonly", zvol_readonly_changed_cb, zv) == 0);

	mtx_lock(&zv->zv_queue_mtx);
	zv->zv_state = 1;
	wakeup_one(&zv->zv_queue);
	while (zv->zv_state != 2)
		msleep(&zv->zv_state, &zv->zv_queue_mtx, 0, "zvol:w", 0);
	mtx_unlock(&zv->zv_queue_mtx);
	mtx_destroy(&zv->zv_queue_mtx);

	pp = zv->zv_provider;
	pp->private = NULL;
	g_wither_geom(pp->geom, ENXIO);

	zil_close(zv->zv_zilog);
	zv->zv_zilog = NULL;
	dmu_objset_close(zv->zv_objset);
	zv->zv_objset = NULL;
	avl_destroy(&zv->zv_znode.z_range_avl);
	mutex_destroy(&zv->zv_znode.z_range_lock);

	kmem_free(zv, sizeof(*zv));

	zvol_minors--;
end:
	g_topology_unlock();
	PICKUP_GIANT();

	return (error);
}

int
zvol_set_volsize(const char *name, dev_t dev, uint64_t volsize)
{
	zvol_state_t *zv;
	dmu_tx_t *tx;
	int error;
	dmu_object_info_t doi;

	DROP_GIANT();
	g_topology_lock();

	if ((zv = zvol_minor_lookup(name)) == NULL) {
		error = ENXIO;
		goto end;
	}

	if ((error = dmu_object_info(zv->zv_objset, ZVOL_OBJ, &doi)) != 0 ||
	    (error = zvol_check_volsize(volsize,
	    doi.doi_data_block_size)) != 0) {
		goto end;
	}

	if (zv->zv_readonly || (zv->zv_mode & DS_MODE_READONLY)) {
		error = EROFS;
		goto end;
	}

	tx = dmu_tx_create(zv->zv_objset);
	dmu_tx_hold_zap(tx, ZVOL_ZAP_OBJ, TRUE, NULL);
	dmu_tx_hold_free(tx, ZVOL_OBJ, volsize, DMU_OBJECT_END);
	error = dmu_tx_assign(tx, TXG_WAIT);
	if (error) {
		dmu_tx_abort(tx);
		goto end;
	}

	error = zap_update(zv->zv_objset, ZVOL_ZAP_OBJ, "size", 8, 1,
	    &volsize, tx);
	if (error == 0) {
		error = dmu_free_range(zv->zv_objset, ZVOL_OBJ, volsize,
		    DMU_OBJECT_END, tx);
	}

	dmu_tx_commit(tx);

	if (error == 0) {
		zv->zv_volsize = volsize;
		zv->zv_provider->mediasize = volsize;	/* XXX: Not supported. */
	}
end:
	g_topology_unlock();
	PICKUP_GIANT();

	return (error);
}

int
zvol_set_volblocksize(const char *name, uint64_t volblocksize)
{
	zvol_state_t *zv;
	dmu_tx_t *tx;
	int error;

	DROP_GIANT();
	g_topology_lock();

	if ((zv = zvol_minor_lookup(name)) == NULL) {
		error = ENXIO;
		goto end;
	}

	if (zv->zv_readonly || (zv->zv_mode & DS_MODE_READONLY)) {
		error = EROFS;
		goto end;
	}

	tx = dmu_tx_create(zv->zv_objset);
	dmu_tx_hold_bonus(tx, ZVOL_OBJ);
	error = dmu_tx_assign(tx, TXG_WAIT);
	if (error) {
		dmu_tx_abort(tx);
	} else {
		error = dmu_object_set_blocksize(zv->zv_objset, ZVOL_OBJ,
		    volblocksize, 0, tx);
		if (error == ENOTSUP)
			error = EBUSY;
		dmu_tx_commit(tx);
		/* XXX: Not supported. */
#if 0
		if (error == 0)
			zv->zv_provider->sectorsize = zc->zc_volblocksize;
#endif
	}
end:
	g_topology_unlock();
	PICKUP_GIANT();

	return (error);
}

void
zvol_get_done(dmu_buf_t *db, void *vzgd)
{
	zgd_t *zgd = (zgd_t *)vzgd;
	rl_t *rl = zgd->zgd_rl;

	dmu_buf_rele(db, vzgd);
	zfs_range_unlock(rl);
	zil_add_vdev(zgd->zgd_zilog, DVA_GET_VDEV(BP_IDENTITY(zgd->zgd_bp)));
	kmem_free(zgd, sizeof (zgd_t));
}

/*
 * Get data to generate a TX_WRITE intent log record.
 */
static int
zvol_get_data(void *arg, lr_write_t *lr, char *buf, zio_t *zio)
{
	zvol_state_t *zv = arg;
	objset_t *os = zv->zv_objset;
	dmu_buf_t *db;
	rl_t *rl;
	zgd_t *zgd;
	uint64_t boff; 			/* block starting offset */
	int dlen = lr->lr_length;	/* length of user data */
	int error;

	ASSERT(zio);
	ASSERT(dlen != 0);

	/*
	 * Write records come in two flavors: immediate and indirect.
	 * For small writes it's cheaper to store the data with the
	 * log record (immediate); for large writes it's cheaper to
	 * sync the data and get a pointer to it (indirect) so that
	 * we don't have to write the data twice.
	 */
	if (buf != NULL) /* immediate write */
		return (dmu_read(os, ZVOL_OBJ, lr->lr_offset, dlen, buf));

	zgd = (zgd_t *)kmem_alloc(sizeof (zgd_t), KM_SLEEP);
	zgd->zgd_zilog = zv->zv_zilog;
	zgd->zgd_bp = &lr->lr_blkptr;

	/*
	 * Lock the range of the block to ensure that when the data is
	 * written out and it's checksum is being calculated that no other
	 * thread can change the block.
	 */
	boff = P2ALIGN_TYPED(lr->lr_offset, zv->zv_volblocksize, uint64_t);
	rl = zfs_range_lock(&zv->zv_znode, boff, zv->zv_volblocksize,
	    RL_READER);
	zgd->zgd_rl = rl;

	VERIFY(0 == dmu_buf_hold(os, ZVOL_OBJ, lr->lr_offset, zgd, &db));
	error = dmu_sync(zio, db, &lr->lr_blkptr,
	    lr->lr_common.lrc_txg, zvol_get_done, zgd);
	if (error == 0)
		zil_add_vdev(zv->zv_zilog,
		    DVA_GET_VDEV(BP_IDENTITY(&lr->lr_blkptr)));
	/*
	 * If we get EINPROGRESS, then we need to wait for a
	 * write IO initiated by dmu_sync() to complete before
	 * we can release this dbuf.  We will finish everything
	 * up in the zvol_get_done() callback.
	 */
	if (error == EINPROGRESS)
		return (0);
	dmu_buf_rele(db, zgd);
	zfs_range_unlock(rl);
	kmem_free(zgd, sizeof (zgd_t));
	return (error);
}

int
zvol_busy(void)
{
	return (zvol_minors != 0);
}

void
zvol_init(void)
{
	ZFS_LOG(1, "ZVOL Initialized.");
}

void
zvol_fini(void)
{
	ZFS_LOG(1, "ZVOL Deinitialized.");
}
