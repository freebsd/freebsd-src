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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/dmu.h>
#include <sys/dmu_impl.h>
#include <sys/dmu_tx.h>
#include <sys/dbuf.h>
#include <sys/dnode.h>
#include <sys/zfs_context.h>
#include <sys/dmu_objset.h>
#include <sys/dmu_traverse.h>
#include <sys/dsl_dataset.h>
#include <sys/dsl_dir.h>
#include <sys/dsl_pool.h>
#include <sys/dsl_synctask.h>
#include <sys/zfs_ioctl.h>
#include <sys/zap.h>
#include <sys/zio_checksum.h>

struct backuparg {
	dmu_replay_record_t *drr;
	kthread_t *td;
	struct file *fp;
	objset_t *os;
	zio_cksum_t zc;
	int err;
};

static int
dump_bytes(struct backuparg *ba, void *buf, int len)
{
	struct uio auio;
	struct iovec aiov;

	ASSERT3U(len % 8, ==, 0);

	fletcher_4_incremental_native(buf, len, &ba->zc);

	aiov.iov_base = buf;
	aiov.iov_len = len;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = len;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_WRITE;
	auio.uio_offset = (off_t)-1;
	auio.uio_td = ba->td;
#ifdef _KERNEL
	if (ba->fp->f_type == DTYPE_VNODE)
		bwillwrite();
	ba->err = fo_write(ba->fp, &auio, ba->td->td_ucred, 0, ba->td);
#else
	fprintf(stderr, "%s: returning EOPNOTSUPP\n", __func__);
	ba->err = EOPNOTSUPP;
#endif

	return (ba->err);
}

static int
dump_free(struct backuparg *ba, uint64_t object, uint64_t offset,
    uint64_t length)
{
	/* write a FREE record */
	bzero(ba->drr, sizeof (dmu_replay_record_t));
	ba->drr->drr_type = DRR_FREE;
	ba->drr->drr_u.drr_free.drr_object = object;
	ba->drr->drr_u.drr_free.drr_offset = offset;
	ba->drr->drr_u.drr_free.drr_length = length;

	if (dump_bytes(ba, ba->drr, sizeof (dmu_replay_record_t)))
		return (EINTR);
	return (0);
}

static int
dump_data(struct backuparg *ba, dmu_object_type_t type,
    uint64_t object, uint64_t offset, int blksz, void *data)
{
	/* write a DATA record */
	bzero(ba->drr, sizeof (dmu_replay_record_t));
	ba->drr->drr_type = DRR_WRITE;
	ba->drr->drr_u.drr_write.drr_object = object;
	ba->drr->drr_u.drr_write.drr_type = type;
	ba->drr->drr_u.drr_write.drr_offset = offset;
	ba->drr->drr_u.drr_write.drr_length = blksz;

	if (dump_bytes(ba, ba->drr, sizeof (dmu_replay_record_t)))
		return (EINTR);
	if (dump_bytes(ba, data, blksz))
		return (EINTR);
	return (0);
}

static int
dump_freeobjects(struct backuparg *ba, uint64_t firstobj, uint64_t numobjs)
{
	/* write a FREEOBJECTS record */
	bzero(ba->drr, sizeof (dmu_replay_record_t));
	ba->drr->drr_type = DRR_FREEOBJECTS;
	ba->drr->drr_u.drr_freeobjects.drr_firstobj = firstobj;
	ba->drr->drr_u.drr_freeobjects.drr_numobjs = numobjs;

	if (dump_bytes(ba, ba->drr, sizeof (dmu_replay_record_t)))
		return (EINTR);
	return (0);
}

static int
dump_dnode(struct backuparg *ba, uint64_t object, dnode_phys_t *dnp)
{
	if (dnp == NULL || dnp->dn_type == DMU_OT_NONE)
		return (dump_freeobjects(ba, object, 1));

	/* write an OBJECT record */
	bzero(ba->drr, sizeof (dmu_replay_record_t));
	ba->drr->drr_type = DRR_OBJECT;
	ba->drr->drr_u.drr_object.drr_object = object;
	ba->drr->drr_u.drr_object.drr_type = dnp->dn_type;
	ba->drr->drr_u.drr_object.drr_bonustype = dnp->dn_bonustype;
	ba->drr->drr_u.drr_object.drr_blksz =
	    dnp->dn_datablkszsec << SPA_MINBLOCKSHIFT;
	ba->drr->drr_u.drr_object.drr_bonuslen = dnp->dn_bonuslen;
	ba->drr->drr_u.drr_object.drr_checksum = dnp->dn_checksum;
	ba->drr->drr_u.drr_object.drr_compress = dnp->dn_compress;

	if (dump_bytes(ba, ba->drr, sizeof (dmu_replay_record_t)))
		return (EINTR);

	if (dump_bytes(ba, DN_BONUS(dnp), P2ROUNDUP(dnp->dn_bonuslen, 8)))
		return (EINTR);

	/* free anything past the end of the file */
	if (dump_free(ba, object, (dnp->dn_maxblkid + 1) *
	    (dnp->dn_datablkszsec << SPA_MINBLOCKSHIFT), -1ULL))
		return (EINTR);
	if (ba->err)
		return (EINTR);
	return (0);
}

#define	BP_SPAN(dnp, level) \
	(((uint64_t)dnp->dn_datablkszsec) << (SPA_MINBLOCKSHIFT + \
	(level) * (dnp->dn_indblkshift - SPA_BLKPTRSHIFT)))

static int
backup_cb(traverse_blk_cache_t *bc, spa_t *spa, void *arg)
{
	struct backuparg *ba = arg;
	uint64_t object = bc->bc_bookmark.zb_object;
	int level = bc->bc_bookmark.zb_level;
	uint64_t blkid = bc->bc_bookmark.zb_blkid;
	blkptr_t *bp = bc->bc_blkptr.blk_birth ? &bc->bc_blkptr : NULL;
	dmu_object_type_t type = bp ? BP_GET_TYPE(bp) : DMU_OT_NONE;
	void *data = bc->bc_data;
	int err = 0;

	if (SIGPENDING(curthread))
		return (EINTR);

	ASSERT(data || bp == NULL);

	if (bp == NULL && object == 0) {
		uint64_t span = BP_SPAN(bc->bc_dnode, level);
		uint64_t dnobj = (blkid * span) >> DNODE_SHIFT;
		err = dump_freeobjects(ba, dnobj, span >> DNODE_SHIFT);
	} else if (bp == NULL) {
		uint64_t span = BP_SPAN(bc->bc_dnode, level);
		err = dump_free(ba, object, blkid * span, span);
	} else if (data && level == 0 && type == DMU_OT_DNODE) {
		dnode_phys_t *blk = data;
		int i;
		int blksz = BP_GET_LSIZE(bp);

		for (i = 0; i < blksz >> DNODE_SHIFT; i++) {
			uint64_t dnobj =
			    (blkid << (DNODE_BLOCK_SHIFT - DNODE_SHIFT)) + i;
			err = dump_dnode(ba, dnobj, blk+i);
			if (err)
				break;
		}
	} else if (level == 0 &&
	    type != DMU_OT_DNODE && type != DMU_OT_OBJSET) {
		int blksz = BP_GET_LSIZE(bp);
		if (data == NULL) {
			uint32_t aflags = ARC_WAIT;
			arc_buf_t *abuf;
			zbookmark_t zb;

			zb.zb_objset = ba->os->os->os_dsl_dataset->ds_object;
			zb.zb_object = object;
			zb.zb_level = level;
			zb.zb_blkid = blkid;
			(void) arc_read(NULL, spa, bp,
			    dmu_ot[type].ot_byteswap, arc_getbuf_func, &abuf,
			    ZIO_PRIORITY_ASYNC_READ, ZIO_FLAG_MUSTSUCCEED,
			    &aflags, &zb);

			if (abuf) {
				err = dump_data(ba, type, object, blkid * blksz,
				    blksz, abuf->b_data);
				(void) arc_buf_remove_ref(abuf, &abuf);
			}
		} else {
			err = dump_data(ba, type, object, blkid * blksz,
			    blksz, data);
		}
	}

	ASSERT(err == 0 || err == EINTR);
	return (err);
}

int
dmu_sendbackup(objset_t *tosnap, objset_t *fromsnap, struct file *fp)
{
	dsl_dataset_t *ds = tosnap->os->os_dsl_dataset;
	dsl_dataset_t *fromds = fromsnap ? fromsnap->os->os_dsl_dataset : NULL;
	dmu_replay_record_t *drr;
	struct backuparg ba;
	int err;

	/* tosnap must be a snapshot */
	if (ds->ds_phys->ds_next_snap_obj == 0)
		return (EINVAL);

	/* fromsnap must be an earlier snapshot from the same fs as tosnap */
	if (fromds && (ds->ds_dir != fromds->ds_dir ||
	    fromds->ds_phys->ds_creation_txg >=
	    ds->ds_phys->ds_creation_txg))
		return (EXDEV);

	drr = kmem_zalloc(sizeof (dmu_replay_record_t), KM_SLEEP);
	drr->drr_type = DRR_BEGIN;
	drr->drr_u.drr_begin.drr_magic = DMU_BACKUP_MAGIC;
	drr->drr_u.drr_begin.drr_version = DMU_BACKUP_VERSION;
	drr->drr_u.drr_begin.drr_creation_time =
	    ds->ds_phys->ds_creation_time;
	drr->drr_u.drr_begin.drr_type = tosnap->os->os_phys->os_type;
	drr->drr_u.drr_begin.drr_toguid = ds->ds_phys->ds_guid;
	if (fromds)
		drr->drr_u.drr_begin.drr_fromguid = fromds->ds_phys->ds_guid;
	dsl_dataset_name(ds, drr->drr_u.drr_begin.drr_toname);

	ba.drr = drr;
	ba.td = curthread;
	ba.fp = fp;
	ba.os = tosnap;
	ZIO_SET_CHECKSUM(&ba.zc, 0, 0, 0, 0);

	if (dump_bytes(&ba, drr, sizeof (dmu_replay_record_t))) {
		kmem_free(drr, sizeof (dmu_replay_record_t));
		return (ba.err);
	}

	err = traverse_dsl_dataset(ds,
	    fromds ? fromds->ds_phys->ds_creation_txg : 0,
	    ADVANCE_PRE | ADVANCE_HOLES | ADVANCE_DATA | ADVANCE_NOLOCK,
	    backup_cb, &ba);

	if (err) {
		if (err == EINTR && ba.err)
			err = ba.err;
		kmem_free(drr, sizeof (dmu_replay_record_t));
		return (err);
	}

	bzero(drr, sizeof (dmu_replay_record_t));
	drr->drr_type = DRR_END;
	drr->drr_u.drr_end.drr_checksum = ba.zc;

	if (dump_bytes(&ba, drr, sizeof (dmu_replay_record_t))) {
		kmem_free(drr, sizeof (dmu_replay_record_t));
		return (ba.err);
	}

	kmem_free(drr, sizeof (dmu_replay_record_t));

	return (0);
}

struct restorearg {
	int err;
	int byteswap;
	kthread_t *td;
	struct file *fp;
	char *buf;
	uint64_t voff;
	int buflen; /* number of valid bytes in buf */
	int bufoff; /* next offset to read */
	int bufsize; /* amount of memory allocated for buf */
	zio_cksum_t zc;
};

/* ARGSUSED */
static int
replay_incremental_check(void *arg1, void *arg2, dmu_tx_t *tx)
{
	dsl_dataset_t *ds = arg1;
	struct drr_begin *drrb = arg2;
	const char *snapname;
	int err;
	uint64_t val;

	/* must already be a snapshot of this fs */
	if (ds->ds_phys->ds_prev_snap_obj == 0)
		return (ENODEV);

	/* most recent snapshot must match fromguid */
	if (ds->ds_prev->ds_phys->ds_guid != drrb->drr_fromguid)
		return (ENODEV);
	/* must not have any changes since most recent snapshot */
	if (ds->ds_phys->ds_bp.blk_birth >
	    ds->ds_prev->ds_phys->ds_creation_txg)
		return (ETXTBSY);

	/* new snapshot name must not exist */
	snapname = strrchr(drrb->drr_toname, '@');
	if (snapname == NULL)
		return (EEXIST);

	snapname++;
	err = zap_lookup(ds->ds_dir->dd_pool->dp_meta_objset,
	    ds->ds_phys->ds_snapnames_zapobj, snapname, 8, 1, &val);
	if (err == 0)
		return (EEXIST);
	if (err != ENOENT)
		return (err);

	return (0);
}

/* ARGSUSED */
static void
replay_incremental_sync(void *arg1, void *arg2, dmu_tx_t *tx)
{
	dsl_dataset_t *ds = arg1;
	dmu_buf_will_dirty(ds->ds_dbuf, tx);
	ds->ds_phys->ds_flags |= DS_FLAG_INCONSISTENT;
}

/* ARGSUSED */
static int
replay_full_check(void *arg1, void *arg2, dmu_tx_t *tx)
{
	dsl_dir_t *dd = arg1;
	struct drr_begin *drrb = arg2;
	objset_t *mos = dd->dd_pool->dp_meta_objset;
	char *cp;
	uint64_t val;
	int err;

	cp = strchr(drrb->drr_toname, '@');
	*cp = '\0';
	err = zap_lookup(mos, dd->dd_phys->dd_child_dir_zapobj,
	    strrchr(drrb->drr_toname, '/') + 1,
	    sizeof (uint64_t), 1, &val);
	*cp = '@';

	if (err != ENOENT)
		return (err ? err : EEXIST);

	return (0);
}

static void
replay_full_sync(void *arg1, void *arg2, dmu_tx_t *tx)
{
	dsl_dir_t *dd = arg1;
	struct drr_begin *drrb = arg2;
	char *cp;
	dsl_dataset_t *ds;
	uint64_t dsobj;

	cp = strchr(drrb->drr_toname, '@');
	*cp = '\0';
	dsobj = dsl_dataset_create_sync(dd, strrchr(drrb->drr_toname, '/') + 1,
	    NULL, tx);
	*cp = '@';

	VERIFY(0 == dsl_dataset_open_obj(dd->dd_pool, dsobj, NULL,
	    DS_MODE_EXCLUSIVE, FTAG, &ds));

	(void) dmu_objset_create_impl(dsl_dataset_get_spa(ds),
	    ds, &ds->ds_phys->ds_bp, drrb->drr_type, tx);

	dmu_buf_will_dirty(ds->ds_dbuf, tx);
	ds->ds_phys->ds_flags |= DS_FLAG_INCONSISTENT;

	dsl_dataset_close(ds, DS_MODE_EXCLUSIVE, FTAG);
}

static int
replay_end_check(void *arg1, void *arg2, dmu_tx_t *tx)
{
	objset_t *os = arg1;
	struct drr_begin *drrb = arg2;
	char *snapname;

	/* XXX verify that drr_toname is in dd */

	snapname = strchr(drrb->drr_toname, '@');
	if (snapname == NULL)
		return (EINVAL);
	snapname++;

	return (dsl_dataset_snapshot_check(os, snapname, tx));
}

static void
replay_end_sync(void *arg1, void *arg2, dmu_tx_t *tx)
{
	objset_t *os = arg1;
	struct drr_begin *drrb = arg2;
	char *snapname;
	dsl_dataset_t *ds, *hds;

	snapname = strchr(drrb->drr_toname, '@') + 1;

	dsl_dataset_snapshot_sync(os, snapname, tx);

	/* set snapshot's creation time and guid */
	hds = os->os->os_dsl_dataset;
	VERIFY(0 == dsl_dataset_open_obj(hds->ds_dir->dd_pool,
	    hds->ds_phys->ds_prev_snap_obj, NULL,
	    DS_MODE_PRIMARY | DS_MODE_READONLY | DS_MODE_INCONSISTENT,
	    FTAG, &ds));

	dmu_buf_will_dirty(ds->ds_dbuf, tx);
	ds->ds_phys->ds_creation_time = drrb->drr_creation_time;
	ds->ds_phys->ds_guid = drrb->drr_toguid;
	ds->ds_phys->ds_flags &= ~DS_FLAG_INCONSISTENT;

	dsl_dataset_close(ds, DS_MODE_PRIMARY, FTAG);

	dmu_buf_will_dirty(hds->ds_dbuf, tx);
	hds->ds_phys->ds_flags &= ~DS_FLAG_INCONSISTENT;
}

static int
restore_bytes(struct restorearg *ra, void *buf, int len, off_t off, int *resid)
{
	struct uio auio;
	struct iovec aiov;
	int error;

	aiov.iov_base = buf;
	aiov.iov_len = len;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = len;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_READ;
	auio.uio_offset = off;
	auio.uio_td = ra->td;
#ifdef _KERNEL
	error = fo_read(ra->fp, &auio, ra->td->td_ucred, FOF_OFFSET, ra->td);
#else
	fprintf(stderr, "%s: returning EOPNOTSUPP\n", __func__);
	error = EOPNOTSUPP;
#endif
	*resid = auio.uio_resid;
	return (error);
}

static void *
restore_read(struct restorearg *ra, int len)
{
	void *rv;

	/* some things will require 8-byte alignment, so everything must */
	ASSERT3U(len % 8, ==, 0);

	while (ra->buflen - ra->bufoff < len) {
		int resid;
		int leftover = ra->buflen - ra->bufoff;

		(void) memmove(ra->buf, ra->buf + ra->bufoff, leftover);

		ra->err = restore_bytes(ra, (caddr_t)ra->buf + leftover,
		    ra->bufsize - leftover, ra->voff, &resid);

		ra->voff += ra->bufsize - leftover - resid;
		ra->buflen = ra->bufsize - resid;
		ra->bufoff = 0;
		if (resid == ra->bufsize - leftover)
			ra->err = EINVAL;
		if (ra->err)
			return (NULL);
		/* Could compute checksum here? */
	}

	ASSERT3U(ra->bufoff % 8, ==, 0);
	ASSERT3U(ra->buflen - ra->bufoff, >=, len);
	rv = ra->buf + ra->bufoff;
	ra->bufoff += len;
	if (ra->byteswap)
		fletcher_4_incremental_byteswap(rv, len, &ra->zc);
	else
		fletcher_4_incremental_native(rv, len, &ra->zc);
	return (rv);
}

static void
backup_byteswap(dmu_replay_record_t *drr)
{
#define	DO64(X) (drr->drr_u.X = BSWAP_64(drr->drr_u.X))
#define	DO32(X) (drr->drr_u.X = BSWAP_32(drr->drr_u.X))
	drr->drr_type = BSWAP_32(drr->drr_type);
	switch (drr->drr_type) {
	case DRR_BEGIN:
		DO64(drr_begin.drr_magic);
		DO64(drr_begin.drr_version);
		DO64(drr_begin.drr_creation_time);
		DO32(drr_begin.drr_type);
		DO64(drr_begin.drr_toguid);
		DO64(drr_begin.drr_fromguid);
		break;
	case DRR_OBJECT:
		DO64(drr_object.drr_object);
		/* DO64(drr_object.drr_allocation_txg); */
		DO32(drr_object.drr_type);
		DO32(drr_object.drr_bonustype);
		DO32(drr_object.drr_blksz);
		DO32(drr_object.drr_bonuslen);
		break;
	case DRR_FREEOBJECTS:
		DO64(drr_freeobjects.drr_firstobj);
		DO64(drr_freeobjects.drr_numobjs);
		break;
	case DRR_WRITE:
		DO64(drr_write.drr_object);
		DO32(drr_write.drr_type);
		DO64(drr_write.drr_offset);
		DO64(drr_write.drr_length);
		break;
	case DRR_FREE:
		DO64(drr_free.drr_object);
		DO64(drr_free.drr_offset);
		DO64(drr_free.drr_length);
		break;
	case DRR_END:
		DO64(drr_end.drr_checksum.zc_word[0]);
		DO64(drr_end.drr_checksum.zc_word[1]);
		DO64(drr_end.drr_checksum.zc_word[2]);
		DO64(drr_end.drr_checksum.zc_word[3]);
		break;
	}
#undef DO64
#undef DO32
}

static int
restore_object(struct restorearg *ra, objset_t *os, struct drr_object *drro)
{
	int err;
	dmu_tx_t *tx;

	err = dmu_object_info(os, drro->drr_object, NULL);

	if (err != 0 && err != ENOENT)
		return (EINVAL);

	if (drro->drr_type == DMU_OT_NONE ||
	    drro->drr_type >= DMU_OT_NUMTYPES ||
	    drro->drr_bonustype >= DMU_OT_NUMTYPES ||
	    drro->drr_checksum >= ZIO_CHECKSUM_FUNCTIONS ||
	    drro->drr_compress >= ZIO_COMPRESS_FUNCTIONS ||
	    P2PHASE(drro->drr_blksz, SPA_MINBLOCKSIZE) ||
	    drro->drr_blksz < SPA_MINBLOCKSIZE ||
	    drro->drr_blksz > SPA_MAXBLOCKSIZE ||
	    drro->drr_bonuslen > DN_MAX_BONUSLEN) {
		return (EINVAL);
	}

	tx = dmu_tx_create(os);

	if (err == ENOENT) {
		/* currently free, want to be allocated */
		dmu_tx_hold_bonus(tx, DMU_NEW_OBJECT);
		dmu_tx_hold_write(tx, DMU_NEW_OBJECT, 0, 1);
		err = dmu_tx_assign(tx, TXG_WAIT);
		if (err) {
			dmu_tx_abort(tx);
			return (err);
		}
		err = dmu_object_claim(os, drro->drr_object,
		    drro->drr_type, drro->drr_blksz,
		    drro->drr_bonustype, drro->drr_bonuslen, tx);
	} else {
		/* currently allocated, want to be allocated */
		dmu_tx_hold_bonus(tx, drro->drr_object);
		/*
		 * We may change blocksize, so need to
		 * hold_write
		 */
		dmu_tx_hold_write(tx, drro->drr_object, 0, 1);
		err = dmu_tx_assign(tx, TXG_WAIT);
		if (err) {
			dmu_tx_abort(tx);
			return (err);
		}

		err = dmu_object_reclaim(os, drro->drr_object,
		    drro->drr_type, drro->drr_blksz,
		    drro->drr_bonustype, drro->drr_bonuslen, tx);
	}
	if (err) {
		dmu_tx_commit(tx);
		return (EINVAL);
	}

	dmu_object_set_checksum(os, drro->drr_object, drro->drr_checksum, tx);
	dmu_object_set_compress(os, drro->drr_object, drro->drr_compress, tx);

	if (drro->drr_bonuslen) {
		dmu_buf_t *db;
		void *data;
		VERIFY(0 == dmu_bonus_hold(os, drro->drr_object, FTAG, &db));
		dmu_buf_will_dirty(db, tx);

		ASSERT3U(db->db_size, ==, drro->drr_bonuslen);
		data = restore_read(ra, P2ROUNDUP(db->db_size, 8));
		if (data == NULL) {
			dmu_tx_commit(tx);
			return (ra->err);
		}
		bcopy(data, db->db_data, db->db_size);
		if (ra->byteswap) {
			dmu_ot[drro->drr_bonustype].ot_byteswap(db->db_data,
			    drro->drr_bonuslen);
		}
		dmu_buf_rele(db, FTAG);
	}
	dmu_tx_commit(tx);
	return (0);
}

/* ARGSUSED */
static int
restore_freeobjects(struct restorearg *ra, objset_t *os,
    struct drr_freeobjects *drrfo)
{
	uint64_t obj;

	if (drrfo->drr_firstobj + drrfo->drr_numobjs < drrfo->drr_firstobj)
		return (EINVAL);

	for (obj = drrfo->drr_firstobj;
	    obj < drrfo->drr_firstobj + drrfo->drr_numobjs;
	    (void) dmu_object_next(os, &obj, FALSE, 0)) {
		dmu_tx_t *tx;
		int err;

		if (dmu_object_info(os, obj, NULL) != 0)
			continue;

		tx = dmu_tx_create(os);
		dmu_tx_hold_bonus(tx, obj);
		err = dmu_tx_assign(tx, TXG_WAIT);
		if (err) {
			dmu_tx_abort(tx);
			return (err);
		}
		err = dmu_object_free(os, obj, tx);
		dmu_tx_commit(tx);
		if (err && err != ENOENT)
			return (EINVAL);
	}
	return (0);
}

static int
restore_write(struct restorearg *ra, objset_t *os,
    struct drr_write *drrw)
{
	dmu_tx_t *tx;
	void *data;
	int err;

	if (drrw->drr_offset + drrw->drr_length < drrw->drr_offset ||
	    drrw->drr_type >= DMU_OT_NUMTYPES)
		return (EINVAL);

	data = restore_read(ra, drrw->drr_length);
	if (data == NULL)
		return (ra->err);

	if (dmu_object_info(os, drrw->drr_object, NULL) != 0)
		return (EINVAL);

	tx = dmu_tx_create(os);

	dmu_tx_hold_write(tx, drrw->drr_object,
	    drrw->drr_offset, drrw->drr_length);
	err = dmu_tx_assign(tx, TXG_WAIT);
	if (err) {
		dmu_tx_abort(tx);
		return (err);
	}
	if (ra->byteswap)
		dmu_ot[drrw->drr_type].ot_byteswap(data, drrw->drr_length);
	dmu_write(os, drrw->drr_object,
	    drrw->drr_offset, drrw->drr_length, data, tx);
	dmu_tx_commit(tx);
	return (0);
}

/* ARGSUSED */
static int
restore_free(struct restorearg *ra, objset_t *os,
    struct drr_free *drrf)
{
	dmu_tx_t *tx;
	int err;

	if (drrf->drr_length != -1ULL &&
	    drrf->drr_offset + drrf->drr_length < drrf->drr_offset)
		return (EINVAL);

	if (dmu_object_info(os, drrf->drr_object, NULL) != 0)
		return (EINVAL);

	tx = dmu_tx_create(os);

	dmu_tx_hold_free(tx, drrf->drr_object,
	    drrf->drr_offset, drrf->drr_length);
	err = dmu_tx_assign(tx, TXG_WAIT);
	if (err) {
		dmu_tx_abort(tx);
		return (err);
	}
	err = dmu_free_range(os, drrf->drr_object,
	    drrf->drr_offset, drrf->drr_length, tx);
	dmu_tx_commit(tx);
	return (err);
}

int
dmu_recvbackup(char *tosnap, struct drr_begin *drrb, uint64_t *sizep,
    boolean_t force, struct file *fp, uint64_t voffset)
{
	kthread_t *td = curthread;
	struct restorearg ra;
	dmu_replay_record_t *drr;
	char *cp;
	objset_t *os = NULL;
	zio_cksum_t pzc;

	bzero(&ra, sizeof (ra));
	ra.td = td;
	ra.fp = fp;
	ra.voff = voffset;
	ra.bufsize = 1<<20;
	ra.buf = kmem_alloc(ra.bufsize, KM_SLEEP);

	if (drrb->drr_magic == DMU_BACKUP_MAGIC) {
		ra.byteswap = FALSE;
	} else if (drrb->drr_magic == BSWAP_64(DMU_BACKUP_MAGIC)) {
		ra.byteswap = TRUE;
	} else {
		ra.err = EINVAL;
		goto out;
	}

	/*
	 * NB: this assumes that struct drr_begin will be the largest in
	 * dmu_replay_record_t's drr_u, and thus we don't need to pad it
	 * with zeros to make it the same length as we wrote out.
	 */
	((dmu_replay_record_t *)ra.buf)->drr_type = DRR_BEGIN;
	((dmu_replay_record_t *)ra.buf)->drr_pad = 0;
	((dmu_replay_record_t *)ra.buf)->drr_u.drr_begin = *drrb;
	if (ra.byteswap) {
		fletcher_4_incremental_byteswap(ra.buf,
		    sizeof (dmu_replay_record_t), &ra.zc);
	} else {
		fletcher_4_incremental_native(ra.buf,
		    sizeof (dmu_replay_record_t), &ra.zc);
	}
	(void) strcpy(drrb->drr_toname, tosnap); /* for the sync funcs */

	if (ra.byteswap) {
		drrb->drr_magic = BSWAP_64(drrb->drr_magic);
		drrb->drr_version = BSWAP_64(drrb->drr_version);
		drrb->drr_creation_time = BSWAP_64(drrb->drr_creation_time);
		drrb->drr_type = BSWAP_32(drrb->drr_type);
		drrb->drr_toguid = BSWAP_64(drrb->drr_toguid);
		drrb->drr_fromguid = BSWAP_64(drrb->drr_fromguid);
	}

	ASSERT3U(drrb->drr_magic, ==, DMU_BACKUP_MAGIC);

	if (drrb->drr_version != DMU_BACKUP_VERSION ||
	    drrb->drr_type >= DMU_OST_NUMTYPES ||
	    strchr(drrb->drr_toname, '@') == NULL) {
		ra.err = EINVAL;
		goto out;
	}

	/*
	 * Process the begin in syncing context.
	 */
	if (drrb->drr_fromguid) {
		/* incremental backup */
		dsl_dataset_t *ds = NULL;

		cp = strchr(tosnap, '@');
		*cp = '\0';
		ra.err = dsl_dataset_open(tosnap, DS_MODE_EXCLUSIVE, FTAG, &ds);
		*cp = '@';
		if (ra.err)
			goto out;

		/*
		 * Only do the rollback if the most recent snapshot
		 * matches the incremental source
		 */
		if (force) {
			if (ds->ds_prev == NULL ||
			    ds->ds_prev->ds_phys->ds_guid !=
			    drrb->drr_fromguid) {
				dsl_dataset_close(ds, DS_MODE_EXCLUSIVE, FTAG);
				kmem_free(ra.buf, ra.bufsize);
				return (ENODEV);
			}
			(void) dsl_dataset_rollback(ds);
		}
		ra.err = dsl_sync_task_do(ds->ds_dir->dd_pool,
		    replay_incremental_check, replay_incremental_sync,
		    ds, drrb, 1);
		dsl_dataset_close(ds, DS_MODE_EXCLUSIVE, FTAG);
	} else {
		/* full backup */
		dsl_dir_t *dd = NULL;
		const char *tail;

		/* can't restore full backup into topmost fs, for now */
		if (strrchr(drrb->drr_toname, '/') == NULL) {
			ra.err = EINVAL;
			goto out;
		}

		cp = strchr(tosnap, '@');
		*cp = '\0';
		ra.err = dsl_dir_open(tosnap, FTAG, &dd, &tail);
		*cp = '@';
		if (ra.err)
			goto out;
		if (tail == NULL) {
			ra.err = EEXIST;
			goto out;
		}

		ra.err = dsl_sync_task_do(dd->dd_pool, replay_full_check,
		    replay_full_sync, dd, drrb, 5);
		dsl_dir_close(dd, FTAG);
	}
	if (ra.err)
		goto out;

	/*
	 * Open the objset we are modifying.
	 */

	cp = strchr(tosnap, '@');
	*cp = '\0';
	ra.err = dmu_objset_open(tosnap, DMU_OST_ANY,
	    DS_MODE_PRIMARY | DS_MODE_INCONSISTENT, &os);
	*cp = '@';
	ASSERT3U(ra.err, ==, 0);

	/*
	 * Read records and process them.
	 */
	pzc = ra.zc;
	while (ra.err == 0 &&
	    NULL != (drr = restore_read(&ra, sizeof (*drr)))) {
		if (SIGPENDING(td)) {
			ra.err = EINTR;
			goto out;
		}

		if (ra.byteswap)
			backup_byteswap(drr);

		switch (drr->drr_type) {
		case DRR_OBJECT:
		{
			/*
			 * We need to make a copy of the record header,
			 * because restore_{object,write} may need to
			 * restore_read(), which will invalidate drr.
			 */
			struct drr_object drro = drr->drr_u.drr_object;
			ra.err = restore_object(&ra, os, &drro);
			break;
		}
		case DRR_FREEOBJECTS:
		{
			struct drr_freeobjects drrfo =
			    drr->drr_u.drr_freeobjects;
			ra.err = restore_freeobjects(&ra, os, &drrfo);
			break;
		}
		case DRR_WRITE:
		{
			struct drr_write drrw = drr->drr_u.drr_write;
			ra.err = restore_write(&ra, os, &drrw);
			break;
		}
		case DRR_FREE:
		{
			struct drr_free drrf = drr->drr_u.drr_free;
			ra.err = restore_free(&ra, os, &drrf);
			break;
		}
		case DRR_END:
		{
			struct drr_end drre = drr->drr_u.drr_end;
			/*
			 * We compare against the *previous* checksum
			 * value, because the stored checksum is of
			 * everything before the DRR_END record.
			 */
			if (drre.drr_checksum.zc_word[0] != 0 &&
			    !ZIO_CHECKSUM_EQUAL(drre.drr_checksum, pzc)) {
				ra.err = ECKSUM;
				goto out;
			}

			ra.err = dsl_sync_task_do(dmu_objset_ds(os)->
			    ds_dir->dd_pool, replay_end_check, replay_end_sync,
			    os, drrb, 3);
			goto out;
		}
		default:
			ra.err = EINVAL;
			goto out;
		}
		pzc = ra.zc;
	}

out:
	if (os)
		dmu_objset_close(os);

	/*
	 * Make sure we don't rollback/destroy unless we actually
	 * processed the begin properly.  'os' will only be set if this
	 * is the case.
	 */
	if (ra.err && os && tosnap && strchr(tosnap, '@')) {
		/*
		 * rollback or destroy what we created, so we don't
		 * leave it in the restoring state.
		 */
		dsl_dataset_t *ds;
		int err;

		cp = strchr(tosnap, '@');
		*cp = '\0';
		err = dsl_dataset_open(tosnap,
		    DS_MODE_EXCLUSIVE | DS_MODE_INCONSISTENT,
		    FTAG, &ds);
		if (err == 0) {
			txg_wait_synced(ds->ds_dir->dd_pool, 0);
			if (drrb->drr_fromguid) {
				/* incremental: rollback to most recent snap */
				(void) dsl_dataset_rollback(ds);
				dsl_dataset_close(ds, DS_MODE_EXCLUSIVE, FTAG);
			} else {
				/* full: destroy whole fs */
				dsl_dataset_close(ds, DS_MODE_EXCLUSIVE, FTAG);
				(void) dsl_dataset_destroy(tosnap);
			}
		}
		*cp = '@';
	}

	kmem_free(ra.buf, ra.bufsize);
	if (sizep)
		*sizep = ra.voff;
	return (ra.err);
}
