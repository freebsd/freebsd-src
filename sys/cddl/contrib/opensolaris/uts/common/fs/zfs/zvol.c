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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

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
#include <sys/dmu_traverse.h>
#include <sys/dnode.h>
#include <sys/dsl_dataset.h>
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
#include <sys/vdev_impl.h>
#include <sys/zvol.h>
#include <geom/geom.h>

#include "zfs_namecheck.h"

#define	ZVOL_DUMPSIZE	"dumpsize"

struct g_class zfs_zvol_class = {
	.name = "ZFS::ZVOL",
	.version = G_VERSION,
};

DECLARE_GEOM_CLASS(zfs_zvol_class, zfs_zvol);

/*
 * This lock protects the zvol_state structure from being modified
 * while it's being used, e.g. an open that comes in before a create
 * finishes.  It also protects temporary opens of the dataset so that,
 * e.g., an open doesn't get a spurious EBUSY.
 */
static kmutex_t zvol_state_lock;
static uint32_t zvol_minors;

#define	NUM_EXTENTS	((SPA_MAXBLOCKSIZE) / sizeof (zvol_extent_t))

typedef struct zvol_extent {
	dva_t		ze_dva;		/* dva associated with this extent */
	uint64_t	ze_stride;	/* extent stride */
	uint64_t	ze_size;	/* number of blocks in extent */
} zvol_extent_t;

/*
 * The list of extents associated with the dump device
 */
typedef struct zvol_ext_list {
	zvol_extent_t		zl_extents[NUM_EXTENTS];
	struct zvol_ext_list	*zl_next;
} zvol_ext_list_t;

/*
 * The in-core state of each volume.
 */
typedef struct zvol_state {
	char		zv_name[MAXPATHLEN]; /* pool/dd name */
	uint64_t	zv_volsize;	/* amount of space we advertise */
	uint64_t	zv_volblocksize; /* volume block size */
	struct g_provider *zv_provider;	/* GEOM provider */
	uint8_t		zv_min_bs;	/* minimum addressable block shift */
	uint8_t		zv_flags;	/* readonly; dumpified */
	objset_t	*zv_objset;	/* objset handle */
	uint32_t	zv_mode;	/* DS_MODE_* flags at open time */
	uint32_t	zv_total_opens;	/* total open count */
	zilog_t		*zv_zilog;	/* ZIL handle */
	zvol_ext_list_t	*zv_list;	/* List of extents for dump */
	uint64_t	zv_txg_assign;	/* txg to assign during ZIL replay */
	znode_t		zv_znode;	/* for range locking */
	int		zv_state;
	struct bio_queue_head zv_queue;
	struct mtx	zv_queue_mtx;	/* zv_queue mutex */
} zvol_state_t;

/*
 * zvol specific flags
 */
#define	ZVOL_RDONLY	0x1
#define	ZVOL_DUMPIFIED	0x2
#define	ZVOL_EXCL	0x4

/*
 * zvol maximum transfer in one DMU tx.
 */
int zvol_maxphys = DMU_MAX_ACCESS/2;

extern int zfs_set_prop_nvlist(const char *, nvlist_t *);
static int zvol_get_data(void *arg, lr_write_t *lr, char *buf, zio_t *zio);
static int zvol_dumpify(zvol_state_t *zv);
static int zvol_dump_fini(zvol_state_t *zv);
static int zvol_dump_init(zvol_state_t *zv, boolean_t resize);

static void
zvol_size_changed(zvol_state_t *zv, major_t maj)
{

}

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

	if (newval)
		zv->zv_flags |= ZVOL_RDONLY;
	else
		zv->zv_flags &= ~ZVOL_RDONLY;
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
	ASSERT(MUTEX_HELD(&zvol_state_lock));

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
	mutex_enter(&zvol_state_lock);

	zv = pp->private;
	if (zv == NULL) {
		if (acr <= 0 && acw <= 0 && ace <= 0)
			return (0);
		mutex_exit(&zvol_state_lock);
		return (pp->error);
	}

	ASSERT(zv->zv_objset != NULL);

	if (acw > 0 &&
	    ((zv->zv_flags & ZVOL_RDONLY) ||
	     (zv->zv_mode & DS_MODE_READONLY))) {
		mutex_exit(&zvol_state_lock);
		return (EROFS);
	}

	zv->zv_total_opens += acr + acw + ace;

	mutex_exit(&zvol_state_lock);

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
		if (error) {
			/* convert checksum errors into IO errors */
			if (error == ECKSUM)
				error = EIO;
			break;
		}
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
				kproc_exit(0);
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

		if (bp->bio_cmd == BIO_FLUSH && !zil_disable)
			zil_commit(zv->zv_zilog, UINT64_MAX, ZVOL_OBJ);

		g_io_deliver(bp, bp->bio_error);
	}
}

void
zvol_init_extent(zvol_extent_t *ze, blkptr_t *bp)
{
	ze->ze_dva = bp->blk_dva[0];	/* structure assignment */
	ze->ze_stride = 0;
	ze->ze_size = 1;
}

/* extent mapping arg */
struct maparg {
	zvol_ext_list_t	*ma_list;
	zvol_extent_t	*ma_extent;
	int		ma_gang;
};

/*ARGSUSED*/
static int
zvol_map_block(traverse_blk_cache_t *bc, spa_t *spa, void *arg)
{
	zbookmark_t *zb = &bc->bc_bookmark;
	blkptr_t *bp = &bc->bc_blkptr;
	void *data = bc->bc_data;
	dnode_phys_t *dnp = bc->bc_dnode;
	struct maparg *ma = (struct maparg *)arg;
	uint64_t stride;

	/* If there is an error, then keep trying to make progress */
	if (bc->bc_errno)
		return (ERESTART);

#ifdef ZFS_DEBUG
	if (zb->zb_level == -1) {
		ASSERT3U(BP_GET_TYPE(bp), ==, DMU_OT_OBJSET);
		ASSERT3U(BP_GET_LEVEL(bp), ==, 0);
	} else {
		ASSERT3U(BP_GET_TYPE(bp), ==, dnp->dn_type);
		ASSERT3U(BP_GET_LEVEL(bp), ==, zb->zb_level);
	}

	if (zb->zb_level > 0) {
		uint64_t fill = 0;
		blkptr_t *bpx, *bpend;

		for (bpx = data, bpend = bpx + BP_GET_LSIZE(bp) / sizeof (*bpx);
		    bpx < bpend; bpx++) {
			if (bpx->blk_birth != 0) {
				fill += bpx->blk_fill;
			} else {
				ASSERT(bpx->blk_fill == 0);
			}
		}
		ASSERT3U(fill, ==, bp->blk_fill);
	}

	if (zb->zb_level == 0 && dnp->dn_type == DMU_OT_DNODE) {
		uint64_t fill = 0;
		dnode_phys_t *dnx, *dnend;

		for (dnx = data, dnend = dnx + (BP_GET_LSIZE(bp)>>DNODE_SHIFT);
		    dnx < dnend; dnx++) {
			if (dnx->dn_type != DMU_OT_NONE)
				fill++;
		}
		ASSERT3U(fill, ==, bp->blk_fill);
	}
#endif

	if (zb->zb_level || dnp->dn_type == DMU_OT_DNODE)
		return (0);

	/* Abort immediately if we have encountered gang blocks */
	if (BP_IS_GANG(bp)) {
		ma->ma_gang++;
		return (EINTR);
	}

	/* first time? */
	if (ma->ma_extent->ze_size == 0) {
		zvol_init_extent(ma->ma_extent, bp);
		return (0);
	}

	stride = (DVA_GET_OFFSET(&bp->blk_dva[0])) -
	    ((DVA_GET_OFFSET(&ma->ma_extent->ze_dva)) +
	    (ma->ma_extent->ze_size - 1) * (ma->ma_extent->ze_stride));
	if (DVA_GET_VDEV(BP_IDENTITY(bp)) ==
	    DVA_GET_VDEV(&ma->ma_extent->ze_dva)) {
		if (ma->ma_extent->ze_stride == 0) {
			/* second block in this extent */
			ma->ma_extent->ze_stride = stride;
			ma->ma_extent->ze_size++;
			return (0);
		} else if (ma->ma_extent->ze_stride == stride) {
			/*
			 * the block we allocated has the same
			 * stride
			 */
			ma->ma_extent->ze_size++;
			return (0);
		}
	}

	/*
	 * dtrace -n 'zfs-dprintf
	 * /stringof(arg0) == "zvol.c"/
	 * {
	 *	printf("%s: %s", stringof(arg1), stringof(arg3))
	 * } '
	 */
	dprintf("ma_extent 0x%lx mrstride 0x%lx stride %lx\n",
	    ma->ma_extent->ze_size, ma->ma_extent->ze_stride, stride);
	dprintf_bp(bp, "%s", "next blkptr:");
	/* start a new extent */
	if (ma->ma_extent == &ma->ma_list->zl_extents[NUM_EXTENTS - 1]) {
		ma->ma_list->zl_next = kmem_zalloc(sizeof (zvol_ext_list_t),
		    KM_SLEEP);
		ma->ma_list = ma->ma_list->zl_next;
		ma->ma_extent = &ma->ma_list->zl_extents[0];
	} else {
		ma->ma_extent++;
	}
	zvol_init_extent(ma->ma_extent, bp);
	return (0);
}

/* ARGSUSED */
void
zvol_create_cb(objset_t *os, void *arg, cred_t *cr, dmu_tx_t *tx)
{
	zfs_creat_t *zct = arg;
	nvlist_t *nvprops = zct->zct_props;
	int error;
	uint64_t volblocksize, volsize;

	VERIFY(nvlist_lookup_uint64(nvprops,
	    zfs_prop_to_name(ZFS_PROP_VOLSIZE), &volsize) == 0);
	if (nvlist_lookup_uint64(nvprops,
	    zfs_prop_to_name(ZFS_PROP_VOLBLOCKSIZE), &volblocksize) != 0)
		volblocksize = zfs_prop_default_numeric(ZFS_PROP_VOLBLOCKSIZE);

	/*
	 * These properties must be removed from the list so the generic
	 * property setting step won't apply to them.
	 */
	VERIFY(nvlist_remove_all(nvprops,
	    zfs_prop_to_name(ZFS_PROP_VOLSIZE)) == 0);
	(void) nvlist_remove_all(nvprops,
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
 * reconstruct dva that gets us to the desired offset (offset
 * is in bytes)
 */
int
zvol_get_dva(zvol_state_t *zv, uint64_t offset, dva_t *dva)
{
	zvol_ext_list_t	*zl;
	zvol_extent_t	*ze;
	int		idx;
	uint64_t	tmp;

	if ((zl = zv->zv_list) == NULL)
		return (EIO);
	idx = 0;
	ze =  &zl->zl_extents[0];
	while (offset >= ze->ze_size * zv->zv_volblocksize) {
		offset -= ze->ze_size * zv->zv_volblocksize;

		if (idx == NUM_EXTENTS - 1) {
			/* we've reached the end of this array */
			ASSERT(zl->zl_next != NULL);
			if (zl->zl_next == NULL)
				return (-1);
			zl = zl->zl_next;
			ze = &zl->zl_extents[0];
			idx = 0;
		} else {
			ze++;
			idx++;
		}
	}
	DVA_SET_VDEV(dva, DVA_GET_VDEV(&ze->ze_dva));
	tmp = DVA_GET_OFFSET((&ze->ze_dva));
	tmp += (ze->ze_stride * (offset / zv->zv_volblocksize));
	DVA_SET_OFFSET(dva, tmp);
	return (0);
}

static void
zvol_free_extents(zvol_state_t *zv)
{
	zvol_ext_list_t *zl;
	zvol_ext_list_t *tmp;

	if (zv->zv_list != NULL) {
		zl = zv->zv_list;
		while (zl != NULL) {
			tmp = zl->zl_next;
			kmem_free(zl, sizeof (zvol_ext_list_t));
			zl = tmp;
		}
		zv->zv_list = NULL;
	}
}

int
zvol_get_lbas(zvol_state_t *zv)
{
	struct maparg	ma;
	zvol_ext_list_t	*zl;
	zvol_extent_t	*ze;
	uint64_t	blocks = 0;
	int		err;

	ma.ma_list = zl = kmem_zalloc(sizeof (zvol_ext_list_t), KM_SLEEP);
	ma.ma_extent = &ma.ma_list->zl_extents[0];
	ma.ma_gang = 0;
	zv->zv_list = ma.ma_list;

	err = traverse_zvol(zv->zv_objset, ADVANCE_PRE, zvol_map_block, &ma);
	if (err == EINTR && ma.ma_gang) {
		/*
		 * We currently don't support dump devices when the pool
		 * is so fragmented that our allocation has resulted in
		 * gang blocks.
		 */
		zvol_free_extents(zv);
		return (EFRAGS);
	}
	ASSERT3U(err, ==, 0);

	ze = &zl->zl_extents[0];
	while (ze) {
		blocks += ze->ze_size;
		if (ze == &zl->zl_extents[NUM_EXTENTS - 1]) {
			zl = zl->zl_next;
			ze = &zl->zl_extents[0];
		} else {
			ze++;
		}
	}
	if (blocks != (zv->zv_volsize / zv->zv_volblocksize)) {
		zvol_free_extents(zv);
		return (EIO);
	}

	return (0);
}

/*
 * Create a minor node (plus a whole lot more) for the specified volume.
 */
int
zvol_create_minor(const char *name, major_t maj)
{
	struct g_provider *pp;
	struct g_geom *gp;
	zvol_state_t *zv;
	objset_t *os;
	dmu_object_info_t doi;
	uint64_t volsize;
	int ds_mode = DS_MODE_OWNER;
	int error;

	DROP_GIANT();
	g_topology_lock();
	mutex_enter(&zvol_state_lock);

	if ((zv = zvol_minor_lookup(name)) != NULL) {
		error = EEXIST;
		goto end;
	}

	if (strchr(name, '@') != 0)
		ds_mode |= DS_MODE_READONLY;

	error = dmu_objset_open(name, DMU_OST_ZVOL, ds_mode, &os);
	if (error)
		goto end;

	error = zap_lookup(os, ZVOL_ZAP_OBJ, "size", 8, 1, &volsize);
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

	zil_replay(os, zv, &zv->zv_txg_assign, zvol_replay_vector, NULL);

	/* XXX this should handle the possible i/o error */
	VERIFY(dsl_prop_register(dmu_objset_ds(zv->zv_objset),
	    "readonly", zvol_readonly_changed_cb, zv) == 0);

	pp->private = zv;
	g_error_provider(pp, 0);

	bioq_init(&zv->zv_queue);
	mtx_init(&zv->zv_queue_mtx, "zvol", NULL, MTX_DEF);
	zv->zv_state = 0;
	kproc_create(zvol_worker, zv, NULL, 0, 0, "zvol:worker %s", pp->name);

	zvol_minors++;
end:
	mutex_exit(&zvol_state_lock);
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
	mutex_enter(&zvol_state_lock);

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
	mutex_exit(&zvol_state_lock);
	g_topology_unlock();
	PICKUP_GIANT();

	return (error);
}

int
zvol_prealloc(zvol_state_t *zv)
{
	objset_t *os = zv->zv_objset;
	dmu_tx_t *tx;
	void *data;
	uint64_t refd, avail, usedobjs, availobjs;
	uint64_t resid = zv->zv_volsize;
	uint64_t off = 0;

	/* Check the space usage before attempting to allocate the space */
	dmu_objset_space(os, &refd, &avail, &usedobjs, &availobjs);
	if (avail < zv->zv_volsize)
		return (ENOSPC);

	/* Free old extents if they exist */
	zvol_free_extents(zv);

	/* allocate the blocks by writing each one */
	data = kmem_zalloc(SPA_MAXBLOCKSIZE, KM_SLEEP);

	while (resid != 0) {
		int error;
		uint64_t bytes = MIN(resid, SPA_MAXBLOCKSIZE);

		tx = dmu_tx_create(os);
		dmu_tx_hold_write(tx, ZVOL_OBJ, off, bytes);
		error = dmu_tx_assign(tx, TXG_WAIT);
		if (error) {
			dmu_tx_abort(tx);
			kmem_free(data, SPA_MAXBLOCKSIZE);
			(void) dmu_free_long_range(os, ZVOL_OBJ, 0, off);
			return (error);
		}
		dmu_write(os, ZVOL_OBJ, off, bytes, data, tx);
		dmu_tx_commit(tx);
		off += bytes;
		resid -= bytes;
	}
	kmem_free(data, SPA_MAXBLOCKSIZE);
	txg_wait_synced(dmu_objset_pool(os), 0);

	return (0);
}

int
zvol_update_volsize(zvol_state_t *zv, major_t maj, uint64_t volsize)
{
	dmu_tx_t *tx;
	int error;

	ASSERT(MUTEX_HELD(&zvol_state_lock));

	tx = dmu_tx_create(zv->zv_objset);
	dmu_tx_hold_zap(tx, ZVOL_ZAP_OBJ, TRUE, NULL);
	error = dmu_tx_assign(tx, TXG_WAIT);
	if (error) {
		dmu_tx_abort(tx);
		return (error);
	}

	error = zap_update(zv->zv_objset, ZVOL_ZAP_OBJ, "size", 8, 1,
	    &volsize, tx);
	dmu_tx_commit(tx);

	if (error == 0)
		error = dmu_free_long_range(zv->zv_objset,
		    ZVOL_OBJ, volsize, DMU_OBJECT_END);

	/*
	 * If we are using a faked-up state (zv_provider == NULL) then don't
	 * try to update the in-core zvol state.
	 */
	if (error == 0 && zv->zv_provider) {
		zv->zv_volsize = volsize;
		zvol_size_changed(zv, maj);
	}
	return (error);
}

int
zvol_set_volsize(const char *name, major_t maj, uint64_t volsize)
{
	zvol_state_t *zv;
	int error;
	dmu_object_info_t doi;
	uint64_t old_volsize = 0ULL;
	zvol_state_t state = { 0 };

	DROP_GIANT();
	g_topology_lock();
	mutex_enter(&zvol_state_lock);

	if ((zv = zvol_minor_lookup(name)) == NULL) {
		/*
		 * If we are doing a "zfs clone -o volsize=", then the
		 * minor node won't exist yet.
		 */
		error = dmu_objset_open(name, DMU_OST_ZVOL, DS_MODE_OWNER,
		    &state.zv_objset);
		if (error != 0)
			goto out;
		zv = &state;
	}
	old_volsize = zv->zv_volsize;

	if ((error = dmu_object_info(zv->zv_objset, ZVOL_OBJ, &doi)) != 0 ||
	    (error = zvol_check_volsize(volsize,
	    doi.doi_data_block_size)) != 0)
		goto out;

	if (zv->zv_flags & ZVOL_RDONLY || (zv->zv_mode & DS_MODE_READONLY)) {
		error = EROFS;
		goto out;
	}

	error = zvol_update_volsize(zv, maj, volsize);

#if 0
	/*
	 * Reinitialize the dump area to the new size. If we
	 * failed to resize the dump area then restore the it back to
	 * it's original size.
	 */
	if (error == 0 && zv->zv_flags & ZVOL_DUMPIFIED) {
		if ((error = zvol_dumpify(zv)) != 0 ||
		    (error = dumpvp_resize()) != 0) {
			(void) zvol_update_volsize(zv, maj, old_volsize);
			error = zvol_dumpify(zv);
		}
	}
#endif

out:
	if (state.zv_objset)
		dmu_objset_close(state.zv_objset);

	mutex_exit(&zvol_state_lock);
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
	mutex_enter(&zvol_state_lock);

	if ((zv = zvol_minor_lookup(name)) == NULL) {
		error = ENXIO;
		goto end;
	}
	if (zv->zv_flags & ZVOL_RDONLY || (zv->zv_mode & DS_MODE_READONLY)) {
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
	mutex_exit(&zvol_state_lock);
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
	zil_add_block(zgd->zgd_zilog, zgd->zgd_bp);
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
	 * written out and its checksum is being calculated that no other
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
		zil_add_block(zv->zv_zilog, &lr->lr_blkptr);
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
	mutex_init(&zvol_state_lock, NULL, MUTEX_DEFAULT, NULL);
	ZFS_LOG(1, "ZVOL Initialized.");
}

void
zvol_fini(void)
{
	mutex_destroy(&zvol_state_lock);
	ZFS_LOG(1, "ZVOL Deinitialized.");
}

static boolean_t
zvol_is_swap(zvol_state_t *zv)
{
	vnode_t *vp;
	boolean_t ret = B_FALSE;
	char *devpath;
	size_t devpathlen;
	int error;

#if 0
	devpathlen = strlen(ZVOL_FULL_DEV_DIR) + strlen(zv->zv_name) + 1;
	devpath = kmem_alloc(devpathlen, KM_SLEEP);
	(void) sprintf(devpath, "%s%s", ZVOL_FULL_DEV_DIR, zv->zv_name);
	error = lookupname(devpath, UIO_SYSSPACE, FOLLOW, NULLVPP, &vp);
	kmem_free(devpath, devpathlen);

	ret = !error && IS_SWAPVP(common_specvp(vp));

	if (vp != NULL)
		VN_RELE(vp);
#endif

	return (ret);
}

static int
zvol_dump_init(zvol_state_t *zv, boolean_t resize)
{
	dmu_tx_t *tx;
	int error = 0;
	objset_t *os = zv->zv_objset;
	nvlist_t *nv = NULL;
	uint64_t checksum, compress, refresrv;

	ASSERT(MUTEX_HELD(&zvol_state_lock));

	tx = dmu_tx_create(os);
	dmu_tx_hold_zap(tx, ZVOL_ZAP_OBJ, TRUE, NULL);
	error = dmu_tx_assign(tx, TXG_WAIT);
	if (error) {
		dmu_tx_abort(tx);
		return (error);
	}

	/*
	 * If we are resizing the dump device then we only need to
	 * update the refreservation to match the newly updated
	 * zvolsize. Otherwise, we save off the original state of the
	 * zvol so that we can restore them if the zvol is ever undumpified.
	 */
	if (resize) {
		error = zap_update(os, ZVOL_ZAP_OBJ,
		    zfs_prop_to_name(ZFS_PROP_REFRESERVATION), 8, 1,
		    &zv->zv_volsize, tx);
	} else {
		error = dsl_prop_get_integer(zv->zv_name,
		    zfs_prop_to_name(ZFS_PROP_COMPRESSION), &compress, NULL);
		error = error ? error : dsl_prop_get_integer(zv->zv_name,
		    zfs_prop_to_name(ZFS_PROP_CHECKSUM), &checksum, NULL);
		error = error ? error : dsl_prop_get_integer(zv->zv_name,
		    zfs_prop_to_name(ZFS_PROP_REFRESERVATION), &refresrv, NULL);

		error = error ? error : zap_update(os, ZVOL_ZAP_OBJ,
		    zfs_prop_to_name(ZFS_PROP_COMPRESSION), 8, 1,
		    &compress, tx);
		error = error ? error : zap_update(os, ZVOL_ZAP_OBJ,
		    zfs_prop_to_name(ZFS_PROP_CHECKSUM), 8, 1, &checksum, tx);
		error = error ? error : zap_update(os, ZVOL_ZAP_OBJ,
		    zfs_prop_to_name(ZFS_PROP_REFRESERVATION), 8, 1,
		    &refresrv, tx);
	}
	dmu_tx_commit(tx);

	/* Truncate the file */
	if (!error)
		error = dmu_free_long_range(zv->zv_objset,
		    ZVOL_OBJ, 0, DMU_OBJECT_END);

	if (error)
		return (error);

	/*
	 * We only need update the zvol's property if we are initializing
	 * the dump area for the first time.
	 */
	if (!resize) {
		VERIFY(nvlist_alloc(&nv, NV_UNIQUE_NAME, KM_SLEEP) == 0);
		VERIFY(nvlist_add_uint64(nv,
		    zfs_prop_to_name(ZFS_PROP_REFRESERVATION), 0) == 0);
		VERIFY(nvlist_add_uint64(nv,
		    zfs_prop_to_name(ZFS_PROP_COMPRESSION),
		    ZIO_COMPRESS_OFF) == 0);
		VERIFY(nvlist_add_uint64(nv,
		    zfs_prop_to_name(ZFS_PROP_CHECKSUM),
		    ZIO_CHECKSUM_OFF) == 0);

		error = zfs_set_prop_nvlist(zv->zv_name, nv);
		nvlist_free(nv);

		if (error)
			return (error);
	}

	/* Allocate the space for the dump */
	error = zvol_prealloc(zv);
	return (error);
}

static int
zvol_dumpify(zvol_state_t *zv)
{
	int error = 0;
	uint64_t dumpsize = 0;
	dmu_tx_t *tx;
	objset_t *os = zv->zv_objset;

	if (zv->zv_flags & ZVOL_RDONLY || (zv->zv_mode & DS_MODE_READONLY))
		return (EROFS);

	/*
	 * We do not support swap devices acting as dump devices.
	 */
	if (zvol_is_swap(zv))
		return (ENOTSUP);

	if (zap_lookup(zv->zv_objset, ZVOL_ZAP_OBJ, ZVOL_DUMPSIZE,
	    8, 1, &dumpsize) != 0 || dumpsize != zv->zv_volsize) {
		boolean_t resize = (dumpsize > 0) ? B_TRUE : B_FALSE;

		if ((error = zvol_dump_init(zv, resize)) != 0) {
			(void) zvol_dump_fini(zv);
			return (error);
		}
	}

	/*
	 * Build up our lba mapping.
	 */
	error = zvol_get_lbas(zv);
	if (error) {
		(void) zvol_dump_fini(zv);
		return (error);
	}

	tx = dmu_tx_create(os);
	dmu_tx_hold_zap(tx, ZVOL_ZAP_OBJ, TRUE, NULL);
	error = dmu_tx_assign(tx, TXG_WAIT);
	if (error) {
		dmu_tx_abort(tx);
		(void) zvol_dump_fini(zv);
		return (error);
	}

	zv->zv_flags |= ZVOL_DUMPIFIED;
	error = zap_update(os, ZVOL_ZAP_OBJ, ZVOL_DUMPSIZE, 8, 1,
	    &zv->zv_volsize, tx);
	dmu_tx_commit(tx);

	if (error) {
		(void) zvol_dump_fini(zv);
		return (error);
	}

	txg_wait_synced(dmu_objset_pool(os), 0);
	return (0);
}

static int
zvol_dump_fini(zvol_state_t *zv)
{
	dmu_tx_t *tx;
	objset_t *os = zv->zv_objset;
	nvlist_t *nv;
	int error = 0;
	uint64_t checksum, compress, refresrv;

	/*
	 * Attempt to restore the zvol back to its pre-dumpified state.
	 * This is a best-effort attempt as it's possible that not all
	 * of these properties were initialized during the dumpify process
	 * (i.e. error during zvol_dump_init).
	 */

	tx = dmu_tx_create(os);
	dmu_tx_hold_zap(tx, ZVOL_ZAP_OBJ, TRUE, NULL);
	error = dmu_tx_assign(tx, TXG_WAIT);
	if (error) {
		dmu_tx_abort(tx);
		return (error);
	}
	(void) zap_remove(os, ZVOL_ZAP_OBJ, ZVOL_DUMPSIZE, tx);
	dmu_tx_commit(tx);

	(void) zap_lookup(zv->zv_objset, ZVOL_ZAP_OBJ,
	    zfs_prop_to_name(ZFS_PROP_CHECKSUM), 8, 1, &checksum);
	(void) zap_lookup(zv->zv_objset, ZVOL_ZAP_OBJ,
	    zfs_prop_to_name(ZFS_PROP_COMPRESSION), 8, 1, &compress);
	(void) zap_lookup(zv->zv_objset, ZVOL_ZAP_OBJ,
	    zfs_prop_to_name(ZFS_PROP_REFRESERVATION), 8, 1, &refresrv);

	VERIFY(nvlist_alloc(&nv, NV_UNIQUE_NAME, KM_SLEEP) == 0);
	(void) nvlist_add_uint64(nv,
	    zfs_prop_to_name(ZFS_PROP_CHECKSUM), checksum);
	(void) nvlist_add_uint64(nv,
	    zfs_prop_to_name(ZFS_PROP_COMPRESSION), compress);
	(void) nvlist_add_uint64(nv,
	    zfs_prop_to_name(ZFS_PROP_REFRESERVATION), refresrv);
	(void) zfs_set_prop_nvlist(zv->zv_name, nv);
	nvlist_free(nv);

	zvol_free_extents(zv);
	zv->zv_flags &= ~ZVOL_DUMPIFIED;
	(void) dmu_free_long_range(os, ZVOL_OBJ, 0, DMU_OBJECT_END);

	return (0);
}
