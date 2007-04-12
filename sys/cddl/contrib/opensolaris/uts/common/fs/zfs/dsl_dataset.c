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

#include <sys/dmu_objset.h>
#include <sys/dsl_dataset.h>
#include <sys/dsl_dir.h>
#include <sys/dsl_prop.h>
#include <sys/dsl_synctask.h>
#include <sys/dmu_traverse.h>
#include <sys/dmu_tx.h>
#include <sys/arc.h>
#include <sys/zio.h>
#include <sys/zap.h>
#include <sys/unique.h>
#include <sys/zfs_context.h>
#include <sys/zfs_ioctl.h>

static dsl_checkfunc_t dsl_dataset_destroy_begin_check;
static dsl_syncfunc_t dsl_dataset_destroy_begin_sync;
static dsl_checkfunc_t dsl_dataset_rollback_check;
static dsl_syncfunc_t dsl_dataset_rollback_sync;
static dsl_checkfunc_t dsl_dataset_destroy_check;
static dsl_syncfunc_t dsl_dataset_destroy_sync;

#define	DS_REF_MAX	(1ULL << 62)

#define	DSL_DEADLIST_BLOCKSIZE	SPA_MAXBLOCKSIZE

/*
 * We use weighted reference counts to express the various forms of exclusion
 * between different open modes.  A STANDARD open is 1 point, an EXCLUSIVE open
 * is DS_REF_MAX, and a PRIMARY open is little more than half of an EXCLUSIVE.
 * This makes the exclusion logic simple: the total refcnt for all opens cannot
 * exceed DS_REF_MAX.  For example, EXCLUSIVE opens are exclusive because their
 * weight (DS_REF_MAX) consumes the entire refcnt space.  PRIMARY opens consume
 * just over half of the refcnt space, so there can't be more than one, but it
 * can peacefully coexist with any number of STANDARD opens.
 */
static uint64_t ds_refcnt_weight[DS_MODE_LEVELS] = {
	0,			/* DS_MODE_NONE - invalid		*/
	1,			/* DS_MODE_STANDARD - unlimited number	*/
	(DS_REF_MAX >> 1) + 1,	/* DS_MODE_PRIMARY - only one of these	*/
	DS_REF_MAX		/* DS_MODE_EXCLUSIVE - no other opens	*/
};


void
dsl_dataset_block_born(dsl_dataset_t *ds, blkptr_t *bp, dmu_tx_t *tx)
{
	int used = bp_get_dasize(tx->tx_pool->dp_spa, bp);
	int compressed = BP_GET_PSIZE(bp);
	int uncompressed = BP_GET_UCSIZE(bp);

	dprintf_bp(bp, "born, ds=%p\n", ds);

	ASSERT(dmu_tx_is_syncing(tx));
	/* It could have been compressed away to nothing */
	if (BP_IS_HOLE(bp))
		return;
	ASSERT(BP_GET_TYPE(bp) != DMU_OT_NONE);
	ASSERT3U(BP_GET_TYPE(bp), <, DMU_OT_NUMTYPES);
	if (ds == NULL) {
		/*
		 * Account for the meta-objset space in its placeholder
		 * dsl_dir.
		 */
		ASSERT3U(compressed, ==, uncompressed); /* it's all metadata */
		dsl_dir_diduse_space(tx->tx_pool->dp_mos_dir,
		    used, compressed, uncompressed, tx);
		dsl_dir_dirty(tx->tx_pool->dp_mos_dir, tx);
		return;
	}
	dmu_buf_will_dirty(ds->ds_dbuf, tx);
	mutex_enter(&ds->ds_lock);
	ds->ds_phys->ds_used_bytes += used;
	ds->ds_phys->ds_compressed_bytes += compressed;
	ds->ds_phys->ds_uncompressed_bytes += uncompressed;
	ds->ds_phys->ds_unique_bytes += used;
	mutex_exit(&ds->ds_lock);
	dsl_dir_diduse_space(ds->ds_dir,
	    used, compressed, uncompressed, tx);
}

void
dsl_dataset_block_kill(dsl_dataset_t *ds, blkptr_t *bp, zio_t *pio,
    dmu_tx_t *tx)
{
	int used = bp_get_dasize(tx->tx_pool->dp_spa, bp);
	int compressed = BP_GET_PSIZE(bp);
	int uncompressed = BP_GET_UCSIZE(bp);

	ASSERT(dmu_tx_is_syncing(tx));
	/* No block pointer => nothing to free */
	if (BP_IS_HOLE(bp))
		return;

	ASSERT(used > 0);
	if (ds == NULL) {
		int err;
		/*
		 * Account for the meta-objset space in its placeholder
		 * dataset.
		 */
		err = arc_free(pio, tx->tx_pool->dp_spa,
		    tx->tx_txg, bp, NULL, NULL, pio ? ARC_NOWAIT: ARC_WAIT);
		ASSERT(err == 0);

		dsl_dir_diduse_space(tx->tx_pool->dp_mos_dir,
		    -used, -compressed, -uncompressed, tx);
		dsl_dir_dirty(tx->tx_pool->dp_mos_dir, tx);
		return;
	}
	ASSERT3P(tx->tx_pool, ==, ds->ds_dir->dd_pool);

	dmu_buf_will_dirty(ds->ds_dbuf, tx);

	if (bp->blk_birth > ds->ds_phys->ds_prev_snap_txg) {
		int err;

		dprintf_bp(bp, "freeing: %s", "");
		err = arc_free(pio, tx->tx_pool->dp_spa,
		    tx->tx_txg, bp, NULL, NULL, pio ? ARC_NOWAIT: ARC_WAIT);
		ASSERT(err == 0);

		mutex_enter(&ds->ds_lock);
		/* XXX unique_bytes is not accurate for head datasets */
		/* ASSERT3U(ds->ds_phys->ds_unique_bytes, >=, used); */
		ds->ds_phys->ds_unique_bytes -= used;
		mutex_exit(&ds->ds_lock);
		dsl_dir_diduse_space(ds->ds_dir,
		    -used, -compressed, -uncompressed, tx);
	} else {
		dprintf_bp(bp, "putting on dead list: %s", "");
		VERIFY(0 == bplist_enqueue(&ds->ds_deadlist, bp, tx));
		/* if (bp->blk_birth > prev prev snap txg) prev unique += bs */
		if (ds->ds_phys->ds_prev_snap_obj != 0) {
			ASSERT3U(ds->ds_prev->ds_object, ==,
			    ds->ds_phys->ds_prev_snap_obj);
			ASSERT(ds->ds_prev->ds_phys->ds_num_children > 0);
			if (ds->ds_prev->ds_phys->ds_next_snap_obj ==
			    ds->ds_object && bp->blk_birth >
			    ds->ds_prev->ds_phys->ds_prev_snap_txg) {
				dmu_buf_will_dirty(ds->ds_prev->ds_dbuf, tx);
				mutex_enter(&ds->ds_prev->ds_lock);
				ds->ds_prev->ds_phys->ds_unique_bytes +=
				    used;
				mutex_exit(&ds->ds_prev->ds_lock);
			}
		}
	}
	mutex_enter(&ds->ds_lock);
	ASSERT3U(ds->ds_phys->ds_used_bytes, >=, used);
	ds->ds_phys->ds_used_bytes -= used;
	ASSERT3U(ds->ds_phys->ds_compressed_bytes, >=, compressed);
	ds->ds_phys->ds_compressed_bytes -= compressed;
	ASSERT3U(ds->ds_phys->ds_uncompressed_bytes, >=, uncompressed);
	ds->ds_phys->ds_uncompressed_bytes -= uncompressed;
	mutex_exit(&ds->ds_lock);
}

uint64_t
dsl_dataset_prev_snap_txg(dsl_dataset_t *ds)
{
	uint64_t trysnap = 0;

	if (ds == NULL)
		return (0);
	/*
	 * The snapshot creation could fail, but that would cause an
	 * incorrect FALSE return, which would only result in an
	 * overestimation of the amount of space that an operation would
	 * consume, which is OK.
	 *
	 * There's also a small window where we could miss a pending
	 * snapshot, because we could set the sync task in the quiescing
	 * phase.  So this should only be used as a guess.
	 */
	if (ds->ds_trysnap_txg >
	    spa_last_synced_txg(ds->ds_dir->dd_pool->dp_spa))
		trysnap = ds->ds_trysnap_txg;
	return (MAX(ds->ds_phys->ds_prev_snap_txg, trysnap));
}

int
dsl_dataset_block_freeable(dsl_dataset_t *ds, uint64_t blk_birth)
{
	return (blk_birth > dsl_dataset_prev_snap_txg(ds));
}

/* ARGSUSED */
static void
dsl_dataset_evict(dmu_buf_t *db, void *dsv)
{
	dsl_dataset_t *ds = dsv;
	dsl_pool_t *dp = ds->ds_dir->dd_pool;

	/* open_refcount == DS_REF_MAX when deleting */
	ASSERT(ds->ds_open_refcount == 0 ||
	    ds->ds_open_refcount == DS_REF_MAX);

	dprintf_ds(ds, "evicting %s\n", "");

	unique_remove(ds->ds_phys->ds_fsid_guid);

	if (ds->ds_user_ptr != NULL)
		ds->ds_user_evict_func(ds, ds->ds_user_ptr);

	if (ds->ds_prev) {
		dsl_dataset_close(ds->ds_prev, DS_MODE_NONE, ds);
		ds->ds_prev = NULL;
	}

	bplist_close(&ds->ds_deadlist);
	dsl_dir_close(ds->ds_dir, ds);

	if (list_link_active(&ds->ds_synced_link))
		list_remove(&dp->dp_synced_objsets, ds);

	mutex_destroy(&ds->ds_lock);
	mutex_destroy(&ds->ds_deadlist.bpl_lock);

	kmem_free(ds, sizeof (dsl_dataset_t));
}

static int
dsl_dataset_get_snapname(dsl_dataset_t *ds)
{
	dsl_dataset_phys_t *headphys;
	int err;
	dmu_buf_t *headdbuf;
	dsl_pool_t *dp = ds->ds_dir->dd_pool;
	objset_t *mos = dp->dp_meta_objset;

	if (ds->ds_snapname[0])
		return (0);
	if (ds->ds_phys->ds_next_snap_obj == 0)
		return (0);

	err = dmu_bonus_hold(mos, ds->ds_dir->dd_phys->dd_head_dataset_obj,
	    FTAG, &headdbuf);
	if (err)
		return (err);
	headphys = headdbuf->db_data;
	err = zap_value_search(dp->dp_meta_objset,
	    headphys->ds_snapnames_zapobj, ds->ds_object, ds->ds_snapname);
	dmu_buf_rele(headdbuf, FTAG);
	return (err);
}

int
dsl_dataset_open_obj(dsl_pool_t *dp, uint64_t dsobj, const char *snapname,
    int mode, void *tag, dsl_dataset_t **dsp)
{
	uint64_t weight = ds_refcnt_weight[DS_MODE_LEVEL(mode)];
	objset_t *mos = dp->dp_meta_objset;
	dmu_buf_t *dbuf;
	dsl_dataset_t *ds;
	int err;

	ASSERT(RW_LOCK_HELD(&dp->dp_config_rwlock) ||
	    dsl_pool_sync_context(dp));

	err = dmu_bonus_hold(mos, dsobj, tag, &dbuf);
	if (err)
		return (err);
	ds = dmu_buf_get_user(dbuf);
	if (ds == NULL) {
		dsl_dataset_t *winner;

		ds = kmem_zalloc(sizeof (dsl_dataset_t), KM_SLEEP);
		ds->ds_dbuf = dbuf;
		ds->ds_object = dsobj;
		ds->ds_phys = dbuf->db_data;

		mutex_init(&ds->ds_lock, NULL, MUTEX_DEFAULT, NULL);
		mutex_init(&ds->ds_deadlist.bpl_lock, NULL, MUTEX_DEFAULT,
		    NULL);

		err = bplist_open(&ds->ds_deadlist,
		    mos, ds->ds_phys->ds_deadlist_obj);
		if (err == 0) {
			err = dsl_dir_open_obj(dp,
			    ds->ds_phys->ds_dir_obj, NULL, ds, &ds->ds_dir);
		}
		if (err) {
			/*
			 * we don't really need to close the blist if we
			 * just opened it.
			 */
			mutex_destroy(&ds->ds_lock);
			mutex_destroy(&ds->ds_deadlist.bpl_lock);
			kmem_free(ds, sizeof (dsl_dataset_t));
			dmu_buf_rele(dbuf, tag);
			return (err);
		}

		if (ds->ds_dir->dd_phys->dd_head_dataset_obj == dsobj) {
			ds->ds_snapname[0] = '\0';
			if (ds->ds_phys->ds_prev_snap_obj) {
				err = dsl_dataset_open_obj(dp,
				    ds->ds_phys->ds_prev_snap_obj, NULL,
				    DS_MODE_NONE, ds, &ds->ds_prev);
			}
		} else {
			if (snapname) {
#ifdef ZFS_DEBUG
				dsl_dataset_phys_t *headphys;
				dmu_buf_t *headdbuf;
				err = dmu_bonus_hold(mos,
				    ds->ds_dir->dd_phys->dd_head_dataset_obj,
				    FTAG, &headdbuf);
				if (err == 0) {
					headphys = headdbuf->db_data;
					uint64_t foundobj;
					err = zap_lookup(dp->dp_meta_objset,
					    headphys->ds_snapnames_zapobj,
					    snapname, sizeof (foundobj), 1,
					    &foundobj);
					ASSERT3U(foundobj, ==, dsobj);
					dmu_buf_rele(headdbuf, FTAG);
				}
#endif
				(void) strcat(ds->ds_snapname, snapname);
			} else if (zfs_flags & ZFS_DEBUG_SNAPNAMES) {
				err = dsl_dataset_get_snapname(ds);
			}
		}

		if (err == 0) {
			winner = dmu_buf_set_user_ie(dbuf, ds, &ds->ds_phys,
			    dsl_dataset_evict);
		}
		if (err || winner) {
			bplist_close(&ds->ds_deadlist);
			if (ds->ds_prev) {
				dsl_dataset_close(ds->ds_prev,
				    DS_MODE_NONE, ds);
			}
			dsl_dir_close(ds->ds_dir, ds);
			mutex_destroy(&ds->ds_lock);
			mutex_destroy(&ds->ds_deadlist.bpl_lock);
			kmem_free(ds, sizeof (dsl_dataset_t));
			if (err) {
				dmu_buf_rele(dbuf, tag);
				return (err);
			}
			ds = winner;
		} else {
			uint64_t new =
			    unique_insert(ds->ds_phys->ds_fsid_guid);
			if (new != ds->ds_phys->ds_fsid_guid) {
				/* XXX it won't necessarily be synced... */
				ds->ds_phys->ds_fsid_guid = new;
			}
		}
	}
	ASSERT3P(ds->ds_dbuf, ==, dbuf);
	ASSERT3P(ds->ds_phys, ==, dbuf->db_data);

	mutex_enter(&ds->ds_lock);
	if ((DS_MODE_LEVEL(mode) == DS_MODE_PRIMARY &&
	    (ds->ds_phys->ds_flags & DS_FLAG_INCONSISTENT) &&
	    !DS_MODE_IS_INCONSISTENT(mode)) ||
	    (ds->ds_open_refcount + weight > DS_REF_MAX)) {
		mutex_exit(&ds->ds_lock);
		dsl_dataset_close(ds, DS_MODE_NONE, tag);
		return (EBUSY);
	}
	ds->ds_open_refcount += weight;
	mutex_exit(&ds->ds_lock);

	*dsp = ds;
	return (0);
}

int
dsl_dataset_open_spa(spa_t *spa, const char *name, int mode,
    void *tag, dsl_dataset_t **dsp)
{
	dsl_dir_t *dd;
	dsl_pool_t *dp;
	const char *tail;
	uint64_t obj;
	dsl_dataset_t *ds = NULL;
	int err = 0;

	err = dsl_dir_open_spa(spa, name, FTAG, &dd, &tail);
	if (err)
		return (err);

	dp = dd->dd_pool;
	obj = dd->dd_phys->dd_head_dataset_obj;
	rw_enter(&dp->dp_config_rwlock, RW_READER);
	if (obj == 0) {
		/* A dataset with no associated objset */
		err = ENOENT;
		goto out;
	}

	if (tail != NULL) {
		objset_t *mos = dp->dp_meta_objset;

		err = dsl_dataset_open_obj(dp, obj, NULL,
		    DS_MODE_NONE, tag, &ds);
		if (err)
			goto out;
		obj = ds->ds_phys->ds_snapnames_zapobj;
		dsl_dataset_close(ds, DS_MODE_NONE, tag);
		ds = NULL;

		if (tail[0] != '@') {
			err = ENOENT;
			goto out;
		}
		tail++;

		/* Look for a snapshot */
		if (!DS_MODE_IS_READONLY(mode)) {
			err = EROFS;
			goto out;
		}
		dprintf("looking for snapshot '%s'\n", tail);
		err = zap_lookup(mos, obj, tail, 8, 1, &obj);
		if (err)
			goto out;
	}
	err = dsl_dataset_open_obj(dp, obj, tail, mode, tag, &ds);

out:
	rw_exit(&dp->dp_config_rwlock);
	dsl_dir_close(dd, FTAG);

	ASSERT3U((err == 0), ==, (ds != NULL));
	/* ASSERT(ds == NULL || strcmp(name, ds->ds_name) == 0); */

	*dsp = ds;
	return (err);
}

int
dsl_dataset_open(const char *name, int mode, void *tag, dsl_dataset_t **dsp)
{
	return (dsl_dataset_open_spa(NULL, name, mode, tag, dsp));
}

void
dsl_dataset_name(dsl_dataset_t *ds, char *name)
{
	if (ds == NULL) {
		(void) strcpy(name, "mos");
	} else {
		dsl_dir_name(ds->ds_dir, name);
		VERIFY(0 == dsl_dataset_get_snapname(ds));
		if (ds->ds_snapname[0]) {
			(void) strcat(name, "@");
			if (!MUTEX_HELD(&ds->ds_lock)) {
				/*
				 * We use a "recursive" mutex so that we
				 * can call dprintf_ds() with ds_lock held.
				 */
				mutex_enter(&ds->ds_lock);
				(void) strcat(name, ds->ds_snapname);
				mutex_exit(&ds->ds_lock);
			} else {
				(void) strcat(name, ds->ds_snapname);
			}
		}
	}
}

static int
dsl_dataset_namelen(dsl_dataset_t *ds)
{
	int result;

	if (ds == NULL) {
		result = 3;	/* "mos" */
	} else {
		result = dsl_dir_namelen(ds->ds_dir);
		VERIFY(0 == dsl_dataset_get_snapname(ds));
		if (ds->ds_snapname[0]) {
			++result;	/* adding one for the @-sign */
			if (!MUTEX_HELD(&ds->ds_lock)) {
				/* see dsl_datset_name */
				mutex_enter(&ds->ds_lock);
				result += strlen(ds->ds_snapname);
				mutex_exit(&ds->ds_lock);
			} else {
				result += strlen(ds->ds_snapname);
			}
		}
	}

	return (result);
}

void
dsl_dataset_close(dsl_dataset_t *ds, int mode, void *tag)
{
	uint64_t weight = ds_refcnt_weight[DS_MODE_LEVEL(mode)];
	mutex_enter(&ds->ds_lock);
	ASSERT3U(ds->ds_open_refcount, >=, weight);
	ds->ds_open_refcount -= weight;
	dprintf_ds(ds, "closing mode %u refcount now 0x%llx\n",
	    mode, ds->ds_open_refcount);
	mutex_exit(&ds->ds_lock);

	dmu_buf_rele(ds->ds_dbuf, tag);
}

void
dsl_dataset_create_root(dsl_pool_t *dp, uint64_t *ddobjp, dmu_tx_t *tx)
{
	objset_t *mos = dp->dp_meta_objset;
	dmu_buf_t *dbuf;
	dsl_dataset_phys_t *dsphys;
	dsl_dataset_t *ds;
	uint64_t dsobj;
	dsl_dir_t *dd;

	dsl_dir_create_root(mos, ddobjp, tx);
	VERIFY(0 == dsl_dir_open_obj(dp, *ddobjp, NULL, FTAG, &dd));

	dsobj = dmu_object_alloc(mos, DMU_OT_DSL_DATASET, 0,
	    DMU_OT_DSL_DATASET, sizeof (dsl_dataset_phys_t), tx);
	VERIFY(0 == dmu_bonus_hold(mos, dsobj, FTAG, &dbuf));
	dmu_buf_will_dirty(dbuf, tx);
	dsphys = dbuf->db_data;
	dsphys->ds_dir_obj = dd->dd_object;
	dsphys->ds_fsid_guid = unique_create();
	unique_remove(dsphys->ds_fsid_guid); /* it isn't open yet */
	(void) random_get_pseudo_bytes((void*)&dsphys->ds_guid,
	    sizeof (dsphys->ds_guid));
	dsphys->ds_snapnames_zapobj =
	    zap_create(mos, DMU_OT_DSL_DS_SNAP_MAP, DMU_OT_NONE, 0, tx);
	dsphys->ds_creation_time = gethrestime_sec();
	dsphys->ds_creation_txg = tx->tx_txg;
	dsphys->ds_deadlist_obj =
	    bplist_create(mos, DSL_DEADLIST_BLOCKSIZE, tx);
	dmu_buf_rele(dbuf, FTAG);

	dmu_buf_will_dirty(dd->dd_dbuf, tx);
	dd->dd_phys->dd_head_dataset_obj = dsobj;
	dsl_dir_close(dd, FTAG);

	VERIFY(0 ==
	    dsl_dataset_open_obj(dp, dsobj, NULL, DS_MODE_NONE, FTAG, &ds));
	(void) dmu_objset_create_impl(dp->dp_spa, ds,
	    &ds->ds_phys->ds_bp, DMU_OST_ZFS, tx);
	dsl_dataset_close(ds, DS_MODE_NONE, FTAG);
}

uint64_t
dsl_dataset_create_sync(dsl_dir_t *pdd,
    const char *lastname, dsl_dataset_t *clone_parent, dmu_tx_t *tx)
{
	dsl_pool_t *dp = pdd->dd_pool;
	dmu_buf_t *dbuf;
	dsl_dataset_phys_t *dsphys;
	uint64_t dsobj, ddobj;
	objset_t *mos = dp->dp_meta_objset;
	dsl_dir_t *dd;

	ASSERT(clone_parent == NULL || clone_parent->ds_dir->dd_pool == dp);
	ASSERT(clone_parent == NULL ||
	    clone_parent->ds_phys->ds_num_children > 0);
	ASSERT(lastname[0] != '@');
	ASSERT(dmu_tx_is_syncing(tx));

	ddobj = dsl_dir_create_sync(pdd, lastname, tx);
	VERIFY(0 == dsl_dir_open_obj(dp, ddobj, lastname, FTAG, &dd));

	dsobj = dmu_object_alloc(mos, DMU_OT_DSL_DATASET, 0,
	    DMU_OT_DSL_DATASET, sizeof (dsl_dataset_phys_t), tx);
	VERIFY(0 == dmu_bonus_hold(mos, dsobj, FTAG, &dbuf));
	dmu_buf_will_dirty(dbuf, tx);
	dsphys = dbuf->db_data;
	dsphys->ds_dir_obj = dd->dd_object;
	dsphys->ds_fsid_guid = unique_create();
	unique_remove(dsphys->ds_fsid_guid); /* it isn't open yet */
	(void) random_get_pseudo_bytes((void*)&dsphys->ds_guid,
	    sizeof (dsphys->ds_guid));
	dsphys->ds_snapnames_zapobj =
	    zap_create(mos, DMU_OT_DSL_DS_SNAP_MAP, DMU_OT_NONE, 0, tx);
	dsphys->ds_creation_time = gethrestime_sec();
	dsphys->ds_creation_txg = tx->tx_txg;
	dsphys->ds_deadlist_obj =
	    bplist_create(mos, DSL_DEADLIST_BLOCKSIZE, tx);
	if (clone_parent) {
		dsphys->ds_prev_snap_obj = clone_parent->ds_object;
		dsphys->ds_prev_snap_txg =
		    clone_parent->ds_phys->ds_creation_txg;
		dsphys->ds_used_bytes =
		    clone_parent->ds_phys->ds_used_bytes;
		dsphys->ds_compressed_bytes =
		    clone_parent->ds_phys->ds_compressed_bytes;
		dsphys->ds_uncompressed_bytes =
		    clone_parent->ds_phys->ds_uncompressed_bytes;
		dsphys->ds_bp = clone_parent->ds_phys->ds_bp;

		dmu_buf_will_dirty(clone_parent->ds_dbuf, tx);
		clone_parent->ds_phys->ds_num_children++;

		dmu_buf_will_dirty(dd->dd_dbuf, tx);
		dd->dd_phys->dd_clone_parent_obj = clone_parent->ds_object;
	}
	dmu_buf_rele(dbuf, FTAG);

	dmu_buf_will_dirty(dd->dd_dbuf, tx);
	dd->dd_phys->dd_head_dataset_obj = dsobj;
	dsl_dir_close(dd, FTAG);

	return (dsobj);
}

struct destroyarg {
	dsl_sync_task_group_t *dstg;
	char *snapname;
	char *failed;
};

static int
dsl_snapshot_destroy_one(char *name, void *arg)
{
	struct destroyarg *da = arg;
	dsl_dataset_t *ds;
	char *cp;
	int err;

	(void) strcat(name, "@");
	(void) strcat(name, da->snapname);
	err = dsl_dataset_open(name,
	    DS_MODE_EXCLUSIVE | DS_MODE_READONLY | DS_MODE_INCONSISTENT,
	    da->dstg, &ds);
	cp = strchr(name, '@');
	*cp = '\0';
	if (err == ENOENT)
		return (0);
	if (err) {
		(void) strcpy(da->failed, name);
		return (err);
	}

	dsl_sync_task_create(da->dstg, dsl_dataset_destroy_check,
	    dsl_dataset_destroy_sync, ds, da->dstg, 0);
	return (0);
}

/*
 * Destroy 'snapname' in all descendants of 'fsname'.
 */
#pragma weak dmu_snapshots_destroy = dsl_snapshots_destroy
int
dsl_snapshots_destroy(char *fsname, char *snapname)
{
	int err;
	struct destroyarg da;
	dsl_sync_task_t *dst;
	spa_t *spa;
	char *cp;

	cp = strchr(fsname, '/');
	if (cp) {
		*cp = '\0';
		err = spa_open(fsname, &spa, FTAG);
		*cp = '/';
	} else {
		err = spa_open(fsname, &spa, FTAG);
	}
	if (err)
		return (err);
	da.dstg = dsl_sync_task_group_create(spa_get_dsl(spa));
	da.snapname = snapname;
	da.failed = fsname;

	err = dmu_objset_find(fsname,
	    dsl_snapshot_destroy_one, &da, DS_FIND_CHILDREN);

	if (err == 0)
		err = dsl_sync_task_group_wait(da.dstg);

	for (dst = list_head(&da.dstg->dstg_tasks); dst;
	    dst = list_next(&da.dstg->dstg_tasks, dst)) {
		dsl_dataset_t *ds = dst->dst_arg1;
		if (dst->dst_err) {
			dsl_dataset_name(ds, fsname);
			cp = strchr(fsname, '@');
			*cp = '\0';
		}
		/*
		 * If it was successful, destroy_sync would have
		 * closed the ds
		 */
		if (err)
			dsl_dataset_close(ds, DS_MODE_EXCLUSIVE, da.dstg);
	}

	dsl_sync_task_group_destroy(da.dstg);
	spa_close(spa, FTAG);
	return (err);
}

int
dsl_dataset_destroy(const char *name)
{
	int err;
	dsl_sync_task_group_t *dstg;
	objset_t *os;
	dsl_dataset_t *ds;
	dsl_dir_t *dd;
	uint64_t obj;

	if (strchr(name, '@')) {
		/* Destroying a snapshot is simpler */
		err = dsl_dataset_open(name,
		    DS_MODE_EXCLUSIVE | DS_MODE_READONLY | DS_MODE_INCONSISTENT,
		    FTAG, &ds);
		if (err)
			return (err);
		err = dsl_sync_task_do(ds->ds_dir->dd_pool,
		    dsl_dataset_destroy_check, dsl_dataset_destroy_sync,
		    ds, FTAG, 0);
		if (err)
			dsl_dataset_close(ds, DS_MODE_EXCLUSIVE, FTAG);
		return (err);
	}

	err = dmu_objset_open(name, DMU_OST_ANY,
	    DS_MODE_EXCLUSIVE | DS_MODE_INCONSISTENT, &os);
	if (err)
		return (err);
	ds = os->os->os_dsl_dataset;
	dd = ds->ds_dir;

	/*
	 * Check for errors and mark this ds as inconsistent, in
	 * case we crash while freeing the objects.
	 */
	err = dsl_sync_task_do(dd->dd_pool, dsl_dataset_destroy_begin_check,
	    dsl_dataset_destroy_begin_sync, ds, NULL, 0);
	if (err) {
		dmu_objset_close(os);
		return (err);
	}

	/*
	 * remove the objects in open context, so that we won't
	 * have too much to do in syncing context.
	 */
	for (obj = 0; err == 0; err = dmu_object_next(os, &obj, FALSE,
	    ds->ds_phys->ds_prev_snap_txg)) {
		dmu_tx_t *tx = dmu_tx_create(os);
		dmu_tx_hold_free(tx, obj, 0, DMU_OBJECT_END);
		dmu_tx_hold_bonus(tx, obj);
		err = dmu_tx_assign(tx, TXG_WAIT);
		if (err) {
			/*
			 * Perhaps there is not enough disk
			 * space.  Just deal with it from
			 * dsl_dataset_destroy_sync().
			 */
			dmu_tx_abort(tx);
			continue;
		}
		VERIFY(0 == dmu_object_free(os, obj, tx));
		dmu_tx_commit(tx);
	}
	/* Make sure it's not dirty before we finish destroying it. */
	txg_wait_synced(dd->dd_pool, 0);

	dmu_objset_close(os);
	if (err != ESRCH)
		return (err);

	err = dsl_dataset_open(name,
	    DS_MODE_EXCLUSIVE | DS_MODE_READONLY | DS_MODE_INCONSISTENT,
	    FTAG, &ds);
	if (err)
		return (err);

	err = dsl_dir_open(name, FTAG, &dd, NULL);
	if (err) {
		dsl_dataset_close(ds, DS_MODE_EXCLUSIVE, FTAG);
		return (err);
	}

	/*
	 * Blow away the dsl_dir + head dataset.
	 */
	dstg = dsl_sync_task_group_create(ds->ds_dir->dd_pool);
	dsl_sync_task_create(dstg, dsl_dataset_destroy_check,
	    dsl_dataset_destroy_sync, ds, FTAG, 0);
	dsl_sync_task_create(dstg, dsl_dir_destroy_check,
	    dsl_dir_destroy_sync, dd, FTAG, 0);
	err = dsl_sync_task_group_wait(dstg);
	dsl_sync_task_group_destroy(dstg);
	/* if it is successful, *destroy_sync will close the ds+dd */
	if (err) {
		dsl_dataset_close(ds, DS_MODE_EXCLUSIVE, FTAG);
		dsl_dir_close(dd, FTAG);
	}
	return (err);
}

int
dsl_dataset_rollback(dsl_dataset_t *ds)
{
	ASSERT3U(ds->ds_open_refcount, ==, DS_REF_MAX);
	return (dsl_sync_task_do(ds->ds_dir->dd_pool,
	    dsl_dataset_rollback_check, dsl_dataset_rollback_sync,
	    ds, NULL, 0));
}

void *
dsl_dataset_set_user_ptr(dsl_dataset_t *ds,
    void *p, dsl_dataset_evict_func_t func)
{
	void *old;

	mutex_enter(&ds->ds_lock);
	old = ds->ds_user_ptr;
	if (old == NULL) {
		ds->ds_user_ptr = p;
		ds->ds_user_evict_func = func;
	}
	mutex_exit(&ds->ds_lock);
	return (old);
}

void *
dsl_dataset_get_user_ptr(dsl_dataset_t *ds)
{
	return (ds->ds_user_ptr);
}


blkptr_t *
dsl_dataset_get_blkptr(dsl_dataset_t *ds)
{
	return (&ds->ds_phys->ds_bp);
}

void
dsl_dataset_set_blkptr(dsl_dataset_t *ds, blkptr_t *bp, dmu_tx_t *tx)
{
	ASSERT(dmu_tx_is_syncing(tx));
	/* If it's the meta-objset, set dp_meta_rootbp */
	if (ds == NULL) {
		tx->tx_pool->dp_meta_rootbp = *bp;
	} else {
		dmu_buf_will_dirty(ds->ds_dbuf, tx);
		ds->ds_phys->ds_bp = *bp;
	}
}

spa_t *
dsl_dataset_get_spa(dsl_dataset_t *ds)
{
	return (ds->ds_dir->dd_pool->dp_spa);
}

void
dsl_dataset_dirty(dsl_dataset_t *ds, dmu_tx_t *tx)
{
	dsl_pool_t *dp;

	if (ds == NULL) /* this is the meta-objset */
		return;

	ASSERT(ds->ds_user_ptr != NULL);

	if (ds->ds_phys->ds_next_snap_obj != 0)
		panic("dirtying snapshot!");

	dp = ds->ds_dir->dd_pool;

	if (txg_list_add(&dp->dp_dirty_datasets, ds, tx->tx_txg) == 0) {
		/* up the hold count until we can be written out */
		dmu_buf_add_ref(ds->ds_dbuf, ds);
	}
}

struct killarg {
	uint64_t *usedp;
	uint64_t *compressedp;
	uint64_t *uncompressedp;
	zio_t *zio;
	dmu_tx_t *tx;
};

static int
kill_blkptr(traverse_blk_cache_t *bc, spa_t *spa, void *arg)
{
	struct killarg *ka = arg;
	blkptr_t *bp = &bc->bc_blkptr;

	ASSERT3U(bc->bc_errno, ==, 0);

	/*
	 * Since this callback is not called concurrently, no lock is
	 * needed on the accounting values.
	 */
	*ka->usedp += bp_get_dasize(spa, bp);
	*ka->compressedp += BP_GET_PSIZE(bp);
	*ka->uncompressedp += BP_GET_UCSIZE(bp);
	/* XXX check for EIO? */
	(void) arc_free(ka->zio, spa, ka->tx->tx_txg, bp, NULL, NULL,
	    ARC_NOWAIT);
	return (0);
}

/* ARGSUSED */
static int
dsl_dataset_rollback_check(void *arg1, void *arg2, dmu_tx_t *tx)
{
	dsl_dataset_t *ds = arg1;

	/*
	 * There must be a previous snapshot.  I suppose we could roll
	 * it back to being empty (and re-initialize the upper (ZPL)
	 * layer).  But for now there's no way to do this via the user
	 * interface.
	 */
	if (ds->ds_phys->ds_prev_snap_txg == 0)
		return (EINVAL);

	/*
	 * This must not be a snapshot.
	 */
	if (ds->ds_phys->ds_next_snap_obj != 0)
		return (EINVAL);

	/*
	 * If we made changes this txg, traverse_dsl_dataset won't find
	 * them.  Try again.
	 */
	if (ds->ds_phys->ds_bp.blk_birth >= tx->tx_txg)
		return (EAGAIN);

	return (0);
}

/* ARGSUSED */
static void
dsl_dataset_rollback_sync(void *arg1, void *arg2, dmu_tx_t *tx)
{
	dsl_dataset_t *ds = arg1;
	objset_t *mos = ds->ds_dir->dd_pool->dp_meta_objset;

	dmu_buf_will_dirty(ds->ds_dbuf, tx);

	/* Zero out the deadlist. */
	bplist_close(&ds->ds_deadlist);
	bplist_destroy(mos, ds->ds_phys->ds_deadlist_obj, tx);
	ds->ds_phys->ds_deadlist_obj =
	    bplist_create(mos, DSL_DEADLIST_BLOCKSIZE, tx);
	VERIFY(0 == bplist_open(&ds->ds_deadlist, mos,
	    ds->ds_phys->ds_deadlist_obj));

	{
		/* Free blkptrs that we gave birth to */
		zio_t *zio;
		uint64_t used = 0, compressed = 0, uncompressed = 0;
		struct killarg ka;

		zio = zio_root(tx->tx_pool->dp_spa, NULL, NULL,
		    ZIO_FLAG_MUSTSUCCEED);
		ka.usedp = &used;
		ka.compressedp = &compressed;
		ka.uncompressedp = &uncompressed;
		ka.zio = zio;
		ka.tx = tx;
		(void) traverse_dsl_dataset(ds, ds->ds_phys->ds_prev_snap_txg,
		    ADVANCE_POST, kill_blkptr, &ka);
		(void) zio_wait(zio);

		dsl_dir_diduse_space(ds->ds_dir,
		    -used, -compressed, -uncompressed, tx);
	}

	/* Change our contents to that of the prev snapshot */
	ASSERT3U(ds->ds_prev->ds_object, ==, ds->ds_phys->ds_prev_snap_obj);
	ds->ds_phys->ds_bp = ds->ds_prev->ds_phys->ds_bp;
	ds->ds_phys->ds_used_bytes = ds->ds_prev->ds_phys->ds_used_bytes;
	ds->ds_phys->ds_compressed_bytes =
	    ds->ds_prev->ds_phys->ds_compressed_bytes;
	ds->ds_phys->ds_uncompressed_bytes =
	    ds->ds_prev->ds_phys->ds_uncompressed_bytes;
	ds->ds_phys->ds_flags = ds->ds_prev->ds_phys->ds_flags;
	ds->ds_phys->ds_unique_bytes = 0;

	if (ds->ds_prev->ds_phys->ds_next_snap_obj == ds->ds_object) {
		dmu_buf_will_dirty(ds->ds_prev->ds_dbuf, tx);
		ds->ds_prev->ds_phys->ds_unique_bytes = 0;
	}
}

/* ARGSUSED */
static int
dsl_dataset_destroy_begin_check(void *arg1, void *arg2, dmu_tx_t *tx)
{
	dsl_dataset_t *ds = arg1;

	/*
	 * Can't delete a head dataset if there are snapshots of it.
	 * (Except if the only snapshots are from the branch we cloned
	 * from.)
	 */
	if (ds->ds_prev != NULL &&
	    ds->ds_prev->ds_phys->ds_next_snap_obj == ds->ds_object)
		return (EINVAL);

	return (0);
}

/* ARGSUSED */
static void
dsl_dataset_destroy_begin_sync(void *arg1, void *arg2, dmu_tx_t *tx)
{
	dsl_dataset_t *ds = arg1;

	/* Mark it as inconsistent on-disk, in case we crash */
	dmu_buf_will_dirty(ds->ds_dbuf, tx);
	ds->ds_phys->ds_flags |= DS_FLAG_INCONSISTENT;
}

/* ARGSUSED */
static int
dsl_dataset_destroy_check(void *arg1, void *arg2, dmu_tx_t *tx)
{
	dsl_dataset_t *ds = arg1;

	/* Can't delete a branch point. */
	if (ds->ds_phys->ds_num_children > 1)
		return (EEXIST);

	/*
	 * Can't delete a head dataset if there are snapshots of it.
	 * (Except if the only snapshots are from the branch we cloned
	 * from.)
	 */
	if (ds->ds_prev != NULL &&
	    ds->ds_prev->ds_phys->ds_next_snap_obj == ds->ds_object)
		return (EINVAL);

	/*
	 * If we made changes this txg, traverse_dsl_dataset won't find
	 * them.  Try again.
	 */
	if (ds->ds_phys->ds_bp.blk_birth >= tx->tx_txg)
		return (EAGAIN);

	/* XXX we should do some i/o error checking... */
	return (0);
}

static void
dsl_dataset_destroy_sync(void *arg1, void *tag, dmu_tx_t *tx)
{
	dsl_dataset_t *ds = arg1;
	uint64_t used = 0, compressed = 0, uncompressed = 0;
	zio_t *zio;
	int err;
	int after_branch_point = FALSE;
	dsl_pool_t *dp = ds->ds_dir->dd_pool;
	objset_t *mos = dp->dp_meta_objset;
	dsl_dataset_t *ds_prev = NULL;
	uint64_t obj;

	ASSERT3U(ds->ds_open_refcount, ==, DS_REF_MAX);
	ASSERT3U(ds->ds_phys->ds_num_children, <=, 1);
	ASSERT(ds->ds_prev == NULL ||
	    ds->ds_prev->ds_phys->ds_next_snap_obj != ds->ds_object);
	ASSERT3U(ds->ds_phys->ds_bp.blk_birth, <=, tx->tx_txg);

	ASSERT(RW_WRITE_HELD(&dp->dp_config_rwlock));

	obj = ds->ds_object;

	if (ds->ds_phys->ds_prev_snap_obj != 0) {
		if (ds->ds_prev) {
			ds_prev = ds->ds_prev;
		} else {
			VERIFY(0 == dsl_dataset_open_obj(dp,
			    ds->ds_phys->ds_prev_snap_obj, NULL,
			    DS_MODE_NONE, FTAG, &ds_prev));
		}
		after_branch_point =
		    (ds_prev->ds_phys->ds_next_snap_obj != obj);

		dmu_buf_will_dirty(ds_prev->ds_dbuf, tx);
		if (after_branch_point &&
		    ds->ds_phys->ds_next_snap_obj == 0) {
			/* This clone is toast. */
			ASSERT(ds_prev->ds_phys->ds_num_children > 1);
			ds_prev->ds_phys->ds_num_children--;
		} else if (!after_branch_point) {
			ds_prev->ds_phys->ds_next_snap_obj =
			    ds->ds_phys->ds_next_snap_obj;
		}
	}

	zio = zio_root(dp->dp_spa, NULL, NULL, ZIO_FLAG_MUSTSUCCEED);

	if (ds->ds_phys->ds_next_snap_obj != 0) {
		blkptr_t bp;
		dsl_dataset_t *ds_next;
		uint64_t itor = 0;

		spa_scrub_restart(dp->dp_spa, tx->tx_txg);

		VERIFY(0 == dsl_dataset_open_obj(dp,
		    ds->ds_phys->ds_next_snap_obj, NULL,
		    DS_MODE_NONE, FTAG, &ds_next));
		ASSERT3U(ds_next->ds_phys->ds_prev_snap_obj, ==, obj);

		dmu_buf_will_dirty(ds_next->ds_dbuf, tx);
		ds_next->ds_phys->ds_prev_snap_obj =
		    ds->ds_phys->ds_prev_snap_obj;
		ds_next->ds_phys->ds_prev_snap_txg =
		    ds->ds_phys->ds_prev_snap_txg;
		ASSERT3U(ds->ds_phys->ds_prev_snap_txg, ==,
		    ds_prev ? ds_prev->ds_phys->ds_creation_txg : 0);

		/*
		 * Transfer to our deadlist (which will become next's
		 * new deadlist) any entries from next's current
		 * deadlist which were born before prev, and free the
		 * other entries.
		 *
		 * XXX we're doing this long task with the config lock held
		 */
		while (bplist_iterate(&ds_next->ds_deadlist, &itor,
		    &bp) == 0) {
			if (bp.blk_birth <= ds->ds_phys->ds_prev_snap_txg) {
				VERIFY(0 == bplist_enqueue(&ds->ds_deadlist,
				    &bp, tx));
				if (ds_prev && !after_branch_point &&
				    bp.blk_birth >
				    ds_prev->ds_phys->ds_prev_snap_txg) {
					ds_prev->ds_phys->ds_unique_bytes +=
					    bp_get_dasize(dp->dp_spa, &bp);
				}
			} else {
				used += bp_get_dasize(dp->dp_spa, &bp);
				compressed += BP_GET_PSIZE(&bp);
				uncompressed += BP_GET_UCSIZE(&bp);
				/* XXX check return value? */
				(void) arc_free(zio, dp->dp_spa, tx->tx_txg,
				    &bp, NULL, NULL, ARC_NOWAIT);
			}
		}

		/* free next's deadlist */
		bplist_close(&ds_next->ds_deadlist);
		bplist_destroy(mos, ds_next->ds_phys->ds_deadlist_obj, tx);

		/* set next's deadlist to our deadlist */
		ds_next->ds_phys->ds_deadlist_obj =
		    ds->ds_phys->ds_deadlist_obj;
		VERIFY(0 == bplist_open(&ds_next->ds_deadlist, mos,
		    ds_next->ds_phys->ds_deadlist_obj));
		ds->ds_phys->ds_deadlist_obj = 0;

		if (ds_next->ds_phys->ds_next_snap_obj != 0) {
			/*
			 * Update next's unique to include blocks which
			 * were previously shared by only this snapshot
			 * and it.  Those blocks will be born after the
			 * prev snap and before this snap, and will have
			 * died after the next snap and before the one
			 * after that (ie. be on the snap after next's
			 * deadlist).
			 *
			 * XXX we're doing this long task with the
			 * config lock held
			 */
			dsl_dataset_t *ds_after_next;

			VERIFY(0 == dsl_dataset_open_obj(dp,
			    ds_next->ds_phys->ds_next_snap_obj, NULL,
			    DS_MODE_NONE, FTAG, &ds_after_next));
			itor = 0;
			while (bplist_iterate(&ds_after_next->ds_deadlist,
			    &itor, &bp) == 0) {
				if (bp.blk_birth >
				    ds->ds_phys->ds_prev_snap_txg &&
				    bp.blk_birth <=
				    ds->ds_phys->ds_creation_txg) {
					ds_next->ds_phys->ds_unique_bytes +=
					    bp_get_dasize(dp->dp_spa, &bp);
				}
			}

			dsl_dataset_close(ds_after_next, DS_MODE_NONE, FTAG);
			ASSERT3P(ds_next->ds_prev, ==, NULL);
		} else {
			/*
			 * It would be nice to update the head dataset's
			 * unique.  To do so we would have to traverse
			 * it for blocks born after ds_prev, which is
			 * pretty expensive just to maintain something
			 * for debugging purposes.
			 */
			ASSERT3P(ds_next->ds_prev, ==, ds);
			dsl_dataset_close(ds_next->ds_prev, DS_MODE_NONE,
			    ds_next);
			if (ds_prev) {
				VERIFY(0 == dsl_dataset_open_obj(dp,
				    ds->ds_phys->ds_prev_snap_obj, NULL,
				    DS_MODE_NONE, ds_next, &ds_next->ds_prev));
			} else {
				ds_next->ds_prev = NULL;
			}
		}
		dsl_dataset_close(ds_next, DS_MODE_NONE, FTAG);

		/*
		 * NB: unique_bytes is not accurate for head objsets
		 * because we don't update it when we delete the most
		 * recent snapshot -- see above comment.
		 */
		ASSERT3U(used, ==, ds->ds_phys->ds_unique_bytes);
	} else {
		/*
		 * There's no next snapshot, so this is a head dataset.
		 * Destroy the deadlist.  Unless it's a clone, the
		 * deadlist should be empty.  (If it's a clone, it's
		 * safe to ignore the deadlist contents.)
		 */
		struct killarg ka;

		ASSERT(after_branch_point || bplist_empty(&ds->ds_deadlist));
		bplist_close(&ds->ds_deadlist);
		bplist_destroy(mos, ds->ds_phys->ds_deadlist_obj, tx);
		ds->ds_phys->ds_deadlist_obj = 0;

		/*
		 * Free everything that we point to (that's born after
		 * the previous snapshot, if we are a clone)
		 *
		 * XXX we're doing this long task with the config lock held
		 */
		ka.usedp = &used;
		ka.compressedp = &compressed;
		ka.uncompressedp = &uncompressed;
		ka.zio = zio;
		ka.tx = tx;
		err = traverse_dsl_dataset(ds, ds->ds_phys->ds_prev_snap_txg,
		    ADVANCE_POST, kill_blkptr, &ka);
		ASSERT3U(err, ==, 0);
	}

	err = zio_wait(zio);
	ASSERT3U(err, ==, 0);

	dsl_dir_diduse_space(ds->ds_dir, -used, -compressed, -uncompressed, tx);

	if (ds->ds_phys->ds_snapnames_zapobj) {
		err = zap_destroy(mos, ds->ds_phys->ds_snapnames_zapobj, tx);
		ASSERT(err == 0);
	}

	if (ds->ds_dir->dd_phys->dd_head_dataset_obj == ds->ds_object) {
		/* Erase the link in the dataset */
		dmu_buf_will_dirty(ds->ds_dir->dd_dbuf, tx);
		ds->ds_dir->dd_phys->dd_head_dataset_obj = 0;
		/*
		 * dsl_dir_sync_destroy() called us, they'll destroy
		 * the dataset.
		 */
	} else {
		/* remove from snapshot namespace */
		dsl_dataset_t *ds_head;
		VERIFY(0 == dsl_dataset_open_obj(dp,
		    ds->ds_dir->dd_phys->dd_head_dataset_obj, NULL,
		    DS_MODE_NONE, FTAG, &ds_head));
		VERIFY(0 == dsl_dataset_get_snapname(ds));
#ifdef ZFS_DEBUG
		{
			uint64_t val;
			err = zap_lookup(mos,
			    ds_head->ds_phys->ds_snapnames_zapobj,
			    ds->ds_snapname, 8, 1, &val);
			ASSERT3U(err, ==, 0);
			ASSERT3U(val, ==, obj);
		}
#endif
		err = zap_remove(mos, ds_head->ds_phys->ds_snapnames_zapobj,
		    ds->ds_snapname, tx);
		ASSERT(err == 0);
		dsl_dataset_close(ds_head, DS_MODE_NONE, FTAG);
	}

	if (ds_prev && ds->ds_prev != ds_prev)
		dsl_dataset_close(ds_prev, DS_MODE_NONE, FTAG);

	spa_clear_bootfs(dp->dp_spa, ds->ds_object, tx);
	dsl_dataset_close(ds, DS_MODE_EXCLUSIVE, tag);
	VERIFY(0 == dmu_object_free(mos, obj, tx));

}

/* ARGSUSED */
int
dsl_dataset_snapshot_check(void *arg1, void *arg2, dmu_tx_t *tx)
{
	objset_t *os = arg1;
	dsl_dataset_t *ds = os->os->os_dsl_dataset;
	const char *snapname = arg2;
	objset_t *mos = ds->ds_dir->dd_pool->dp_meta_objset;
	int err;
	uint64_t value;

	/*
	 * We don't allow multiple snapshots of the same txg.  If there
	 * is already one, try again.
	 */
	if (ds->ds_phys->ds_prev_snap_txg >= tx->tx_txg)
		return (EAGAIN);

	/*
	 * Check for conflicting name snapshot name.
	 */
	err = zap_lookup(mos, ds->ds_phys->ds_snapnames_zapobj,
	    snapname, 8, 1, &value);
	if (err == 0)
		return (EEXIST);
	if (err != ENOENT)
		return (err);

	/*
	 * Check that the dataset's name is not too long.  Name consists
	 * of the dataset's length + 1 for the @-sign + snapshot name's length
	 */
	if (dsl_dataset_namelen(ds) + 1 + strlen(snapname) >= MAXNAMELEN)
		return (ENAMETOOLONG);

	ds->ds_trysnap_txg = tx->tx_txg;
	return (0);
}

void
dsl_dataset_snapshot_sync(void *arg1, void *arg2, dmu_tx_t *tx)
{
	objset_t *os = arg1;
	dsl_dataset_t *ds = os->os->os_dsl_dataset;
	const char *snapname = arg2;
	dsl_pool_t *dp = ds->ds_dir->dd_pool;
	dmu_buf_t *dbuf;
	dsl_dataset_phys_t *dsphys;
	uint64_t dsobj;
	objset_t *mos = dp->dp_meta_objset;
	int err;

	spa_scrub_restart(dp->dp_spa, tx->tx_txg);
	ASSERT(RW_WRITE_HELD(&dp->dp_config_rwlock));

	dsobj = dmu_object_alloc(mos, DMU_OT_DSL_DATASET, 0,
	    DMU_OT_DSL_DATASET, sizeof (dsl_dataset_phys_t), tx);
	VERIFY(0 == dmu_bonus_hold(mos, dsobj, FTAG, &dbuf));
	dmu_buf_will_dirty(dbuf, tx);
	dsphys = dbuf->db_data;
	dsphys->ds_dir_obj = ds->ds_dir->dd_object;
	dsphys->ds_fsid_guid = unique_create();
	unique_remove(dsphys->ds_fsid_guid); /* it isn't open yet */
	(void) random_get_pseudo_bytes((void*)&dsphys->ds_guid,
	    sizeof (dsphys->ds_guid));
	dsphys->ds_prev_snap_obj = ds->ds_phys->ds_prev_snap_obj;
	dsphys->ds_prev_snap_txg = ds->ds_phys->ds_prev_snap_txg;
	dsphys->ds_next_snap_obj = ds->ds_object;
	dsphys->ds_num_children = 1;
	dsphys->ds_creation_time = gethrestime_sec();
	dsphys->ds_creation_txg = tx->tx_txg;
	dsphys->ds_deadlist_obj = ds->ds_phys->ds_deadlist_obj;
	dsphys->ds_used_bytes = ds->ds_phys->ds_used_bytes;
	dsphys->ds_compressed_bytes = ds->ds_phys->ds_compressed_bytes;
	dsphys->ds_uncompressed_bytes = ds->ds_phys->ds_uncompressed_bytes;
	dsphys->ds_flags = ds->ds_phys->ds_flags;
	dsphys->ds_bp = ds->ds_phys->ds_bp;
	dmu_buf_rele(dbuf, FTAG);

	ASSERT3U(ds->ds_prev != 0, ==, ds->ds_phys->ds_prev_snap_obj != 0);
	if (ds->ds_prev) {
		ASSERT(ds->ds_prev->ds_phys->ds_next_snap_obj ==
		    ds->ds_object ||
		    ds->ds_prev->ds_phys->ds_num_children > 1);
		if (ds->ds_prev->ds_phys->ds_next_snap_obj == ds->ds_object) {
			dmu_buf_will_dirty(ds->ds_prev->ds_dbuf, tx);
			ASSERT3U(ds->ds_phys->ds_prev_snap_txg, ==,
			    ds->ds_prev->ds_phys->ds_creation_txg);
			ds->ds_prev->ds_phys->ds_next_snap_obj = dsobj;
		}
	}

	bplist_close(&ds->ds_deadlist);
	dmu_buf_will_dirty(ds->ds_dbuf, tx);
	ASSERT3U(ds->ds_phys->ds_prev_snap_txg, <, dsphys->ds_creation_txg);
	ds->ds_phys->ds_prev_snap_obj = dsobj;
	ds->ds_phys->ds_prev_snap_txg = dsphys->ds_creation_txg;
	ds->ds_phys->ds_unique_bytes = 0;
	ds->ds_phys->ds_deadlist_obj =
	    bplist_create(mos, DSL_DEADLIST_BLOCKSIZE, tx);
	VERIFY(0 == bplist_open(&ds->ds_deadlist, mos,
	    ds->ds_phys->ds_deadlist_obj));

	dprintf("snap '%s' -> obj %llu\n", snapname, dsobj);
	err = zap_add(mos, ds->ds_phys->ds_snapnames_zapobj,
	    snapname, 8, 1, &dsobj, tx);
	ASSERT(err == 0);

	if (ds->ds_prev)
		dsl_dataset_close(ds->ds_prev, DS_MODE_NONE, ds);
	VERIFY(0 == dsl_dataset_open_obj(dp,
	    ds->ds_phys->ds_prev_snap_obj, snapname,
	    DS_MODE_NONE, ds, &ds->ds_prev));
}

void
dsl_dataset_sync(dsl_dataset_t *ds, zio_t *zio, dmu_tx_t *tx)
{
	ASSERT(dmu_tx_is_syncing(tx));
	ASSERT(ds->ds_user_ptr != NULL);
	ASSERT(ds->ds_phys->ds_next_snap_obj == 0);

	dsl_dir_dirty(ds->ds_dir, tx);
	dmu_objset_sync(ds->ds_user_ptr, zio, tx);
	/* Unneeded? bplist_close(&ds->ds_deadlist); */
}

void
dsl_dataset_stats(dsl_dataset_t *ds, nvlist_t *nv)
{
	dsl_dir_stats(ds->ds_dir, nv);

	dsl_prop_nvlist_add_uint64(nv, ZFS_PROP_CREATION,
	    ds->ds_phys->ds_creation_time);
	dsl_prop_nvlist_add_uint64(nv, ZFS_PROP_CREATETXG,
	    ds->ds_phys->ds_creation_txg);
	dsl_prop_nvlist_add_uint64(nv, ZFS_PROP_REFERENCED,
	    ds->ds_phys->ds_used_bytes);

	if (ds->ds_phys->ds_next_snap_obj) {
		/*
		 * This is a snapshot; override the dd's space used with
		 * our unique space and compression ratio.
		 */
		dsl_prop_nvlist_add_uint64(nv, ZFS_PROP_USED,
		    ds->ds_phys->ds_unique_bytes);
		dsl_prop_nvlist_add_uint64(nv, ZFS_PROP_COMPRESSRATIO,
		    ds->ds_phys->ds_compressed_bytes == 0 ? 100 :
		    (ds->ds_phys->ds_uncompressed_bytes * 100 /
		    ds->ds_phys->ds_compressed_bytes));
	}
}

void
dsl_dataset_fast_stat(dsl_dataset_t *ds, dmu_objset_stats_t *stat)
{
	stat->dds_creation_txg = ds->ds_phys->ds_creation_txg;
	stat->dds_inconsistent = ds->ds_phys->ds_flags & DS_FLAG_INCONSISTENT;
	if (ds->ds_phys->ds_next_snap_obj) {
		stat->dds_is_snapshot = B_TRUE;
		stat->dds_num_clones = ds->ds_phys->ds_num_children - 1;
	}

	/* clone origin is really a dsl_dir thing... */
	if (ds->ds_dir->dd_phys->dd_clone_parent_obj) {
		dsl_dataset_t *ods;

		rw_enter(&ds->ds_dir->dd_pool->dp_config_rwlock, RW_READER);
		VERIFY(0 == dsl_dataset_open_obj(ds->ds_dir->dd_pool,
		    ds->ds_dir->dd_phys->dd_clone_parent_obj,
		    NULL, DS_MODE_NONE, FTAG, &ods));
		dsl_dataset_name(ods, stat->dds_clone_of);
		dsl_dataset_close(ods, DS_MODE_NONE, FTAG);
		rw_exit(&ds->ds_dir->dd_pool->dp_config_rwlock);
	}
}

uint64_t
dsl_dataset_fsid_guid(dsl_dataset_t *ds)
{
	return (ds->ds_phys->ds_fsid_guid);
}

void
dsl_dataset_space(dsl_dataset_t *ds,
    uint64_t *refdbytesp, uint64_t *availbytesp,
    uint64_t *usedobjsp, uint64_t *availobjsp)
{
	*refdbytesp = ds->ds_phys->ds_used_bytes;
	*availbytesp = dsl_dir_space_available(ds->ds_dir, NULL, 0, TRUE);
	*usedobjsp = ds->ds_phys->ds_bp.blk_fill;
	*availobjsp = DN_MAX_OBJECT - *usedobjsp;
}

/* ARGSUSED */
static int
dsl_dataset_snapshot_rename_check(void *arg1, void *arg2, dmu_tx_t *tx)
{
	dsl_dataset_t *ds = arg1;
	char *newsnapname = arg2;
	dsl_dir_t *dd = ds->ds_dir;
	objset_t *mos = dd->dd_pool->dp_meta_objset;
	dsl_dataset_t *hds;
	uint64_t val;
	int err;

	err = dsl_dataset_open_obj(dd->dd_pool,
	    dd->dd_phys->dd_head_dataset_obj, NULL, DS_MODE_NONE, FTAG, &hds);
	if (err)
		return (err);

	/* new name better not be in use */
	err = zap_lookup(mos, hds->ds_phys->ds_snapnames_zapobj,
	    newsnapname, 8, 1, &val);
	dsl_dataset_close(hds, DS_MODE_NONE, FTAG);

	if (err == 0)
		err = EEXIST;
	else if (err == ENOENT)
		err = 0;

	/* dataset name + 1 for the "@" + the new snapshot name must fit */
	if (dsl_dir_namelen(ds->ds_dir) + 1 + strlen(newsnapname) >= MAXNAMELEN)
		err = ENAMETOOLONG;

	return (err);
}

static void
dsl_dataset_snapshot_rename_sync(void *arg1, void *arg2, dmu_tx_t *tx)
{
	dsl_dataset_t *ds = arg1;
	char *newsnapname = arg2;
	dsl_dir_t *dd = ds->ds_dir;
	objset_t *mos = dd->dd_pool->dp_meta_objset;
	dsl_dataset_t *hds;
	int err;

	ASSERT(ds->ds_phys->ds_next_snap_obj != 0);

	VERIFY(0 == dsl_dataset_open_obj(dd->dd_pool,
	    dd->dd_phys->dd_head_dataset_obj, NULL, DS_MODE_NONE, FTAG, &hds));

	VERIFY(0 == dsl_dataset_get_snapname(ds));
	err = zap_remove(mos, hds->ds_phys->ds_snapnames_zapobj,
	    ds->ds_snapname, tx);
	ASSERT3U(err, ==, 0);
	mutex_enter(&ds->ds_lock);
	(void) strcpy(ds->ds_snapname, newsnapname);
	mutex_exit(&ds->ds_lock);
	err = zap_add(mos, hds->ds_phys->ds_snapnames_zapobj,
	    ds->ds_snapname, 8, 1, &ds->ds_object, tx);
	ASSERT3U(err, ==, 0);

	dsl_dataset_close(hds, DS_MODE_NONE, FTAG);
}

struct renamearg {
	dsl_sync_task_group_t *dstg;
	char failed[MAXPATHLEN];
	char *oldsnap;
	char *newsnap;
};

static int
dsl_snapshot_rename_one(char *name, void *arg)
{
	struct renamearg *ra = arg;
	dsl_dataset_t *ds = NULL;
	char *cp;
	int err;

	cp = name + strlen(name);
	*cp = '@';
	(void) strcpy(cp + 1, ra->oldsnap);
	err = dsl_dataset_open(name, DS_MODE_READONLY | DS_MODE_STANDARD,
	    ra->dstg, &ds);
	if (err == ENOENT) {
		*cp = '\0';
		return (0);
	}
	if (err) {
		(void) strcpy(ra->failed, name);
		*cp = '\0';
		dsl_dataset_close(ds, DS_MODE_STANDARD, ra->dstg);
		return (err);
	}

#ifdef _KERNEL
	/* for all filesystems undergoing rename, we'll need to unmount it */
	(void) zfs_unmount_snap(name, NULL);
#endif

	*cp = '\0';

	dsl_sync_task_create(ra->dstg, dsl_dataset_snapshot_rename_check,
	    dsl_dataset_snapshot_rename_sync, ds, ra->newsnap, 0);

	return (0);
}

static int
dsl_recursive_rename(char *oldname, const char *newname)
{
	int err;
	struct renamearg *ra;
	dsl_sync_task_t *dst;
	spa_t *spa;
	char *cp, *fsname = spa_strdup(oldname);
	int len = strlen(oldname);

	/* truncate the snapshot name to get the fsname */
	cp = strchr(fsname, '@');
	*cp = '\0';

	cp = strchr(fsname, '/');
	if (cp) {
		*cp = '\0';
		err = spa_open(fsname, &spa, FTAG);
		*cp = '/';
	} else {
		err = spa_open(fsname, &spa, FTAG);
	}
	if (err) {
		kmem_free(fsname, len + 1);
		return (err);
	}
	ra = kmem_alloc(sizeof (struct renamearg), KM_SLEEP);
	ra->dstg = dsl_sync_task_group_create(spa_get_dsl(spa));

	ra->oldsnap = strchr(oldname, '@') + 1;
	ra->newsnap = strchr(newname, '@') + 1;
	*ra->failed = '\0';

	err = dmu_objset_find(fsname, dsl_snapshot_rename_one, ra,
	    DS_FIND_CHILDREN);
	kmem_free(fsname, len + 1);

	if (err == 0) {
		err = dsl_sync_task_group_wait(ra->dstg);
	}

	for (dst = list_head(&ra->dstg->dstg_tasks); dst;
	    dst = list_next(&ra->dstg->dstg_tasks, dst)) {
		dsl_dataset_t *ds = dst->dst_arg1;
		if (dst->dst_err) {
			dsl_dir_name(ds->ds_dir, ra->failed);
			(void) strcat(ra->failed, "@");
			(void) strcat(ra->failed, ra->newsnap);
		}
		dsl_dataset_close(ds, DS_MODE_STANDARD, ra->dstg);
	}

	(void) strcpy(oldname, ra->failed);

	dsl_sync_task_group_destroy(ra->dstg);
	kmem_free(ra, sizeof (struct renamearg));
	spa_close(spa, FTAG);
	return (err);
}

#pragma weak dmu_objset_rename = dsl_dataset_rename
int
dsl_dataset_rename(char *oldname, const char *newname,
    boolean_t recursive)
{
	dsl_dir_t *dd;
	dsl_dataset_t *ds;
	const char *tail;
	int err;

	err = dsl_dir_open(oldname, FTAG, &dd, &tail);
	if (err)
		return (err);
	if (tail == NULL) {
		err = dsl_dir_rename(dd, newname);
		dsl_dir_close(dd, FTAG);
		return (err);
	}
	if (tail[0] != '@') {
		/* the name ended in a nonexistant component */
		dsl_dir_close(dd, FTAG);
		return (ENOENT);
	}

	dsl_dir_close(dd, FTAG);

	/* new name must be snapshot in same filesystem */
	tail = strchr(newname, '@');
	if (tail == NULL)
		return (EINVAL);
	tail++;
	if (strncmp(oldname, newname, tail - newname) != 0)
		return (EXDEV);

	if (recursive) {
		err = dsl_recursive_rename(oldname, newname);
	} else {
		err = dsl_dataset_open(oldname,
		    DS_MODE_READONLY | DS_MODE_STANDARD, FTAG, &ds);
		if (err)
			return (err);

		err = dsl_sync_task_do(ds->ds_dir->dd_pool,
		    dsl_dataset_snapshot_rename_check,
		    dsl_dataset_snapshot_rename_sync, ds, (char *)tail, 1);

		dsl_dataset_close(ds, DS_MODE_STANDARD, FTAG);
	}

	return (err);
}

struct promotearg {
	uint64_t used, comp, uncomp, unique;
	uint64_t newnext_obj, snapnames_obj;
};

static int
dsl_dataset_promote_check(void *arg1, void *arg2, dmu_tx_t *tx)
{
	dsl_dataset_t *hds = arg1;
	struct promotearg *pa = arg2;
	dsl_dir_t *dd = hds->ds_dir;
	dsl_pool_t *dp = hds->ds_dir->dd_pool;
	dsl_dir_t *pdd = NULL;
	dsl_dataset_t *ds = NULL;
	dsl_dataset_t *pivot_ds = NULL;
	dsl_dataset_t *newnext_ds = NULL;
	int err;
	char *name = NULL;
	uint64_t itor = 0;
	blkptr_t bp;

	bzero(pa, sizeof (*pa));

	/* Check that it is a clone */
	if (dd->dd_phys->dd_clone_parent_obj == 0)
		return (EINVAL);

	/* Since this is so expensive, don't do the preliminary check */
	if (!dmu_tx_is_syncing(tx))
		return (0);

	if (err = dsl_dataset_open_obj(dp,
	    dd->dd_phys->dd_clone_parent_obj,
	    NULL, DS_MODE_EXCLUSIVE, FTAG, &pivot_ds))
		goto out;
	pdd = pivot_ds->ds_dir;

	{
		dsl_dataset_t *phds;
		if (err = dsl_dataset_open_obj(dd->dd_pool,
		    pdd->dd_phys->dd_head_dataset_obj,
		    NULL, DS_MODE_NONE, FTAG, &phds))
			goto out;
		pa->snapnames_obj = phds->ds_phys->ds_snapnames_zapobj;
		dsl_dataset_close(phds, DS_MODE_NONE, FTAG);
	}

	if (hds->ds_phys->ds_flags & DS_FLAG_NOPROMOTE) {
		err = EXDEV;
		goto out;
	}

	/* find pivot point's new next ds */
	VERIFY(0 == dsl_dataset_open_obj(dd->dd_pool, hds->ds_object,
	    NULL, DS_MODE_NONE, FTAG, &newnext_ds));
	while (newnext_ds->ds_phys->ds_prev_snap_obj != pivot_ds->ds_object) {
		dsl_dataset_t *prev;

		if (err = dsl_dataset_open_obj(dd->dd_pool,
		    newnext_ds->ds_phys->ds_prev_snap_obj,
		    NULL, DS_MODE_NONE, FTAG, &prev))
			goto out;
		dsl_dataset_close(newnext_ds, DS_MODE_NONE, FTAG);
		newnext_ds = prev;
	}
	pa->newnext_obj = newnext_ds->ds_object;

	/* compute pivot point's new unique space */
	while ((err = bplist_iterate(&newnext_ds->ds_deadlist,
	    &itor, &bp)) == 0) {
		if (bp.blk_birth > pivot_ds->ds_phys->ds_prev_snap_txg)
			pa->unique += bp_get_dasize(dd->dd_pool->dp_spa, &bp);
	}
	if (err != ENOENT)
		goto out;

	/* Walk the snapshots that we are moving */
	name = kmem_alloc(MAXPATHLEN, KM_SLEEP);
	ds = pivot_ds;
	/* CONSTCOND */
	while (TRUE) {
		uint64_t val, dlused, dlcomp, dluncomp;
		dsl_dataset_t *prev;

		/* Check that the snapshot name does not conflict */
		dsl_dataset_name(ds, name);
		err = zap_lookup(dd->dd_pool->dp_meta_objset,
		    hds->ds_phys->ds_snapnames_zapobj, ds->ds_snapname,
		    8, 1, &val);
		if (err != ENOENT) {
			if (err == 0)
				err = EEXIST;
			goto out;
		}

		/*
		 * compute space to transfer.  Each snapshot gave birth to:
		 * (my used) - (prev's used) + (deadlist's used)
		 */
		pa->used += ds->ds_phys->ds_used_bytes;
		pa->comp += ds->ds_phys->ds_compressed_bytes;
		pa->uncomp += ds->ds_phys->ds_uncompressed_bytes;

		/* If we reach the first snapshot, we're done. */
		if (ds->ds_phys->ds_prev_snap_obj == 0)
			break;

		if (err = bplist_space(&ds->ds_deadlist,
		    &dlused, &dlcomp, &dluncomp))
			goto out;
		if (err = dsl_dataset_open_obj(dd->dd_pool,
		    ds->ds_phys->ds_prev_snap_obj, NULL, DS_MODE_EXCLUSIVE,
		    FTAG, &prev))
			goto out;
		pa->used += dlused - prev->ds_phys->ds_used_bytes;
		pa->comp += dlcomp - prev->ds_phys->ds_compressed_bytes;
		pa->uncomp += dluncomp - prev->ds_phys->ds_uncompressed_bytes;

		/*
		 * We could be a clone of a clone.  If we reach our
		 * parent's branch point, we're done.
		 */
		if (prev->ds_phys->ds_next_snap_obj != ds->ds_object) {
			dsl_dataset_close(prev, DS_MODE_EXCLUSIVE, FTAG);
			break;
		}
		if (ds != pivot_ds)
			dsl_dataset_close(ds, DS_MODE_EXCLUSIVE, FTAG);
		ds = prev;
	}

	/* Check that there is enough space here */
	err = dsl_dir_transfer_possible(pdd, dd, pa->used);

out:
	if (ds && ds != pivot_ds)
		dsl_dataset_close(ds, DS_MODE_EXCLUSIVE, FTAG);
	if (pivot_ds)
		dsl_dataset_close(pivot_ds, DS_MODE_EXCLUSIVE, FTAG);
	if (newnext_ds)
		dsl_dataset_close(newnext_ds, DS_MODE_NONE, FTAG);
	if (name)
		kmem_free(name, MAXPATHLEN);
	return (err);
}

static void
dsl_dataset_promote_sync(void *arg1, void *arg2, dmu_tx_t *tx)
{
	dsl_dataset_t *hds = arg1;
	struct promotearg *pa = arg2;
	dsl_dir_t *dd = hds->ds_dir;
	dsl_pool_t *dp = hds->ds_dir->dd_pool;
	dsl_dir_t *pdd = NULL;
	dsl_dataset_t *ds, *pivot_ds;
	char *name;

	ASSERT(dd->dd_phys->dd_clone_parent_obj != 0);
	ASSERT(0 == (hds->ds_phys->ds_flags & DS_FLAG_NOPROMOTE));

	VERIFY(0 == dsl_dataset_open_obj(dp,
	    dd->dd_phys->dd_clone_parent_obj,
	    NULL, DS_MODE_EXCLUSIVE, FTAG, &pivot_ds));
	/*
	 * We need to explicitly open pdd, since pivot_ds's pdd will be
	 * changing.
	 */
	VERIFY(0 == dsl_dir_open_obj(dp, pivot_ds->ds_dir->dd_object,
	    NULL, FTAG, &pdd));

	/* move snapshots to this dir */
	name = kmem_alloc(MAXPATHLEN, KM_SLEEP);
	ds = pivot_ds;
	/* CONSTCOND */
	while (TRUE) {
		dsl_dataset_t *prev;

		/* move snap name entry */
		dsl_dataset_name(ds, name);
		VERIFY(0 == zap_remove(dp->dp_meta_objset,
		    pa->snapnames_obj, ds->ds_snapname, tx));
		VERIFY(0 == zap_add(dp->dp_meta_objset,
		    hds->ds_phys->ds_snapnames_zapobj, ds->ds_snapname,
		    8, 1, &ds->ds_object, tx));

		/* change containing dsl_dir */
		dmu_buf_will_dirty(ds->ds_dbuf, tx);
		ASSERT3U(ds->ds_phys->ds_dir_obj, ==, pdd->dd_object);
		ds->ds_phys->ds_dir_obj = dd->dd_object;
		ASSERT3P(ds->ds_dir, ==, pdd);
		dsl_dir_close(ds->ds_dir, ds);
		VERIFY(0 == dsl_dir_open_obj(dp, dd->dd_object,
		    NULL, ds, &ds->ds_dir));

		ASSERT3U(dsl_prop_numcb(ds), ==, 0);

		if (ds->ds_phys->ds_prev_snap_obj == 0)
			break;

		VERIFY(0 == dsl_dataset_open_obj(dp,
		    ds->ds_phys->ds_prev_snap_obj, NULL, DS_MODE_EXCLUSIVE,
		    FTAG, &prev));

		if (prev->ds_phys->ds_next_snap_obj != ds->ds_object) {
			dsl_dataset_close(prev, DS_MODE_EXCLUSIVE, FTAG);
			break;
		}
		if (ds != pivot_ds)
			dsl_dataset_close(ds, DS_MODE_EXCLUSIVE, FTAG);
		ds = prev;
	}
	if (ds != pivot_ds)
		dsl_dataset_close(ds, DS_MODE_EXCLUSIVE, FTAG);

	/* change pivot point's next snap */
	dmu_buf_will_dirty(pivot_ds->ds_dbuf, tx);
	pivot_ds->ds_phys->ds_next_snap_obj = pa->newnext_obj;

	/* change clone_parent-age */
	dmu_buf_will_dirty(dd->dd_dbuf, tx);
	ASSERT3U(dd->dd_phys->dd_clone_parent_obj, ==, pivot_ds->ds_object);
	dd->dd_phys->dd_clone_parent_obj = pdd->dd_phys->dd_clone_parent_obj;
	dmu_buf_will_dirty(pdd->dd_dbuf, tx);
	pdd->dd_phys->dd_clone_parent_obj = pivot_ds->ds_object;

	/* change space accounting */
	dsl_dir_diduse_space(pdd, -pa->used, -pa->comp, -pa->uncomp, tx);
	dsl_dir_diduse_space(dd, pa->used, pa->comp, pa->uncomp, tx);
	pivot_ds->ds_phys->ds_unique_bytes = pa->unique;

	dsl_dir_close(pdd, FTAG);
	dsl_dataset_close(pivot_ds, DS_MODE_EXCLUSIVE, FTAG);
	kmem_free(name, MAXPATHLEN);
}

int
dsl_dataset_promote(const char *name)
{
	dsl_dataset_t *ds;
	int err;
	dmu_object_info_t doi;
	struct promotearg pa;

	err = dsl_dataset_open(name, DS_MODE_NONE, FTAG, &ds);
	if (err)
		return (err);

	err = dmu_object_info(ds->ds_dir->dd_pool->dp_meta_objset,
	    ds->ds_phys->ds_snapnames_zapobj, &doi);
	if (err) {
		dsl_dataset_close(ds, DS_MODE_NONE, FTAG);
		return (err);
	}

	/*
	 * Add in 128x the snapnames zapobj size, since we will be moving
	 * a bunch of snapnames to the promoted ds, and dirtying their
	 * bonus buffers.
	 */
	err = dsl_sync_task_do(ds->ds_dir->dd_pool,
	    dsl_dataset_promote_check,
	    dsl_dataset_promote_sync, ds, &pa, 2 + 2 * doi.doi_physical_blks);
	dsl_dataset_close(ds, DS_MODE_NONE, FTAG);
	return (err);
}

/*
 * Given a pool name and a dataset object number in that pool,
 * return the name of that dataset.
 */
int
dsl_dsobj_to_dsname(char *pname, uint64_t obj, char *buf)
{
	spa_t *spa;
	dsl_pool_t *dp;
	dsl_dataset_t *ds = NULL;
	int error;

	if ((error = spa_open(pname, &spa, FTAG)) != 0)
		return (error);
	dp = spa_get_dsl(spa);
	rw_enter(&dp->dp_config_rwlock, RW_READER);
	if ((error = dsl_dataset_open_obj(dp, obj,
	    NULL, DS_MODE_NONE, FTAG, &ds)) != 0) {
		rw_exit(&dp->dp_config_rwlock);
		spa_close(spa, FTAG);
		return (error);
	}
	dsl_dataset_name(ds, buf);
	dsl_dataset_close(ds, DS_MODE_NONE, FTAG);
	rw_exit(&dp->dp_config_rwlock);
	spa_close(spa, FTAG);

	return (0);
}
