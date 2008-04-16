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

/* Portions Copyright 2007 Jeremy Teo */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef _KERNEL
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/resource.h>
#include <sys/mntent.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/unistd.h>
#include <sys/atomic.h>
#include <sys/zfs_dir.h>
#include <sys/zfs_acl.h>
#include <sys/zfs_ioctl.h>
#include <sys/zfs_rlock.h>
#include <sys/fs/zfs.h>
#endif /* _KERNEL */

#include <sys/dmu.h>
#include <sys/refcount.h>
#include <sys/stat.h>
#include <sys/zap.h>
#include <sys/zfs_znode.h>
#include <sys/refcount.h>

/* Used by fstat(1). */
SYSCTL_INT(_debug_sizeof, OID_AUTO, znode, CTLFLAG_RD, 0, sizeof(znode_t),
    "sizeof(znode_t)");

/*
 * Functions needed for userland (ie: libzpool) are not put under
 * #ifdef_KERNEL; the rest of the functions have dependencies
 * (such as VFS logic) that will not compile easily in userland.
 */
#ifdef _KERNEL
struct kmem_cache *znode_cache = NULL;

/*ARGSUSED*/
static void
znode_pageout_func(dmu_buf_t *dbuf, void *user_ptr)
{
	znode_t *zp = user_ptr;
	vnode_t *vp;

	mutex_enter(&zp->z_lock);
	vp = ZTOV(zp);
	if (vp == NULL) {
		mutex_exit(&zp->z_lock);
		zfs_znode_free(zp);
	} else if (vp->v_count == 0) {
		ZTOV(zp) = NULL;
		vhold(vp);
		mutex_exit(&zp->z_lock);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		vrecycle(vp, curthread);
		VOP_UNLOCK(vp, 0);
		vdrop(vp);
		zfs_znode_free(zp);
	} else {
		/* signal force unmount that this znode can be freed */
		zp->z_dbuf = NULL;
		mutex_exit(&zp->z_lock);
	}
}

extern struct vop_vector zfs_vnodeops;
extern struct vop_vector zfs_fifoops;

/*
 * XXX: We cannot use this function as a cache constructor, because
 *      there is one global cache for all file systems and we need
 *      to pass vfsp here, which is not possible, because argument
 *      'cdrarg' is defined at kmem_cache_create() time.
 */
static int
zfs_znode_cache_constructor(void *buf, void *cdrarg, int kmflags)
{
	znode_t *zp = buf;
	vnode_t *vp;
	vfs_t *vfsp = cdrarg;
	int error;

	if (cdrarg != NULL) {
		error = getnewvnode("zfs", vfsp, &zfs_vnodeops, &vp);
		ASSERT(error == 0);
		zp->z_vnode = vp;
		vp->v_data = (caddr_t)zp;
		VN_LOCK_AREC(vp);
		VN_LOCK_ASHARE(vp);
	} else {
		zp->z_vnode = NULL;
	}
	mutex_init(&zp->z_lock, NULL, MUTEX_DEFAULT, NULL);
	rw_init(&zp->z_map_lock, NULL, RW_DEFAULT, NULL);
	rw_init(&zp->z_parent_lock, NULL, RW_DEFAULT, NULL);
	rw_init(&zp->z_name_lock, NULL, RW_DEFAULT, NULL);
	mutex_init(&zp->z_acl_lock, NULL, MUTEX_DEFAULT, NULL);

	mutex_init(&zp->z_range_lock, NULL, MUTEX_DEFAULT, NULL);
	avl_create(&zp->z_range_avl, zfs_range_compare,
	    sizeof (rl_t), offsetof(rl_t, r_node));

	zp->z_dbuf_held = 0;
	zp->z_dirlocks = 0;
	return (0);
}

/*ARGSUSED*/
static void
zfs_znode_cache_destructor(void *buf, void *cdarg)
{
	znode_t *zp = buf;

	ASSERT(zp->z_dirlocks == 0);
	mutex_destroy(&zp->z_lock);
	rw_destroy(&zp->z_map_lock);
	rw_destroy(&zp->z_parent_lock);
	rw_destroy(&zp->z_name_lock);
	mutex_destroy(&zp->z_acl_lock);
	mutex_destroy(&zp->z_range_lock);
	avl_destroy(&zp->z_range_avl);

	ASSERT(zp->z_dbuf_held == 0);
}

void
zfs_znode_init(void)
{
	/*
	 * Initialize zcache
	 */
	ASSERT(znode_cache == NULL);
	znode_cache = kmem_cache_create("zfs_znode_cache",
	    sizeof (znode_t), 0, /* zfs_znode_cache_constructor */ NULL,
	    zfs_znode_cache_destructor, NULL, NULL, NULL, 0);
}

void
zfs_znode_fini(void)
{
	/*
	 * Cleanup zcache
	 */
	if (znode_cache)
		kmem_cache_destroy(znode_cache);
	znode_cache = NULL;
}

/*
 * zfs_init_fs - Initialize the zfsvfs struct and the file system
 *	incore "master" object.  Verify version compatibility.
 */
int
zfs_init_fs(zfsvfs_t *zfsvfs, znode_t **zpp, cred_t *cr)
{
	objset_t	*os = zfsvfs->z_os;
	uint64_t	version = ZPL_VERSION;
	int		i, error;
	dmu_object_info_t doi;
	uint64_t fsid_guid;

	*zpp = NULL;

	/*
	 * XXX - hack to auto-create the pool root filesystem at
	 * the first attempted mount.
	 */
	if (dmu_object_info(os, MASTER_NODE_OBJ, &doi) == ENOENT) {
		dmu_tx_t *tx = dmu_tx_create(os);

		dmu_tx_hold_zap(tx, DMU_NEW_OBJECT, TRUE, NULL); /* master */
		dmu_tx_hold_zap(tx, DMU_NEW_OBJECT, TRUE, NULL); /* del queue */
		dmu_tx_hold_bonus(tx, DMU_NEW_OBJECT); /* root node */
		error = dmu_tx_assign(tx, TXG_WAIT);
		ASSERT3U(error, ==, 0);
		zfs_create_fs(os, cr, tx);
		dmu_tx_commit(tx);
	}

	error = zap_lookup(os, MASTER_NODE_OBJ, ZPL_VERSION_OBJ, 8, 1,
	    &version);
	if (error) {
		return (error);
	} else if (version != ZPL_VERSION) {
		(void) printf("Mismatched versions:  File system "
		    "is version %lld on-disk format, which is "
		    "incompatible with this software version %lld!",
		    (u_longlong_t)version, ZPL_VERSION);
		return (ENOTSUP);
	}

	/*
	 * The fsid is 64 bits, composed of an 8-bit fs type, which
	 * separates our fsid from any other filesystem types, and a
	 * 56-bit objset unique ID.  The objset unique ID is unique to
	 * all objsets open on this system, provided by unique_create().
	 * The 8-bit fs type must be put in the low bits of fsid[1]
	 * because that's where other Solaris filesystems put it.
	 */
	fsid_guid = dmu_objset_fsid_guid(os);
	ASSERT((fsid_guid & ~((1ULL<<56)-1)) == 0);
	zfsvfs->z_vfs->vfs_fsid.val[0] = fsid_guid;
	zfsvfs->z_vfs->vfs_fsid.val[1] = ((fsid_guid>>32) << 8) |
	    zfsvfs->z_vfs->mnt_vfc->vfc_typenum & 0xFF;

	error = zap_lookup(os, MASTER_NODE_OBJ, ZFS_ROOT_OBJ, 8, 1,
	    &zfsvfs->z_root);
	if (error)
		return (error);
	ASSERT(zfsvfs->z_root != 0);

	/*
	 * Create the per mount vop tables.
	 */

	/*
	 * Initialize zget mutex's
	 */
	for (i = 0; i != ZFS_OBJ_MTX_SZ; i++)
		mutex_init(&zfsvfs->z_hold_mtx[i], NULL, MUTEX_DEFAULT, NULL);

	error = zfs_zget(zfsvfs, zfsvfs->z_root, zpp);
	if (error)
		return (error);
	ASSERT3U((*zpp)->z_id, ==, zfsvfs->z_root);

	error = zap_lookup(os, MASTER_NODE_OBJ, ZFS_UNLINKED_SET, 8, 1,
	    &zfsvfs->z_unlinkedobj);
	if (error)
		return (error);

	return (0);
}

/*
 * define a couple of values we need available
 * for both 64 and 32 bit environments.
 */
#ifndef NBITSMINOR64
#define	NBITSMINOR64	32
#endif
#ifndef MAXMAJ64
#define	MAXMAJ64	0xffffffffUL
#endif
#ifndef	MAXMIN64
#define	MAXMIN64	0xffffffffUL
#endif
#ifndef major
#define	major(x)	((int)(((u_int)(x) >> 8)&0xff))	/* major number */
#endif
#ifndef minor
#define	minor(x)	((int)((x)&0xffff00ff))		/* minor number */
#endif

/*
 * Create special expldev for ZFS private use.
 * Can't use standard expldev since it doesn't do
 * what we want.  The standard expldev() takes a
 * dev32_t in LP64 and expands it to a long dev_t.
 * We need an interface that takes a dev32_t in ILP32
 * and expands it to a long dev_t.
 */
static uint64_t
zfs_expldev(dev_t dev)
{
	return (((uint64_t)major(dev) << NBITSMINOR64) | minor(dev));
}
/*
 * Special cmpldev for ZFS private use.
 * Can't use standard cmpldev since it takes
 * a long dev_t and compresses it to dev32_t in
 * LP64.  We need to do a compaction of a long dev_t
 * to a dev32_t in ILP32.
 */
dev_t
zfs_cmpldev(uint64_t dev)
{
	return (makedev((dev >> NBITSMINOR64), (dev & MAXMIN64)));
}

/*
 * Construct a new znode/vnode and intialize.
 *
 * This does not do a call to dmu_set_user() that is
 * up to the caller to do, in case you don't want to
 * return the znode
 */
static znode_t *
zfs_znode_alloc(zfsvfs_t *zfsvfs, dmu_buf_t *db, uint64_t obj_num, int blksz)
{
	znode_t	*zp;
	vnode_t *vp;
	int error;

	zp = kmem_cache_alloc(znode_cache, KM_SLEEP);
	zfs_znode_cache_constructor(zp, zfsvfs->z_vfs, 0);

	ASSERT(zp->z_dirlocks == NULL);

	zp->z_phys = db->db_data;
	zp->z_zfsvfs = zfsvfs;
	zp->z_unlinked = 0;
	zp->z_atime_dirty = 0;
	zp->z_dbuf_held = 0;
	zp->z_mapcnt = 0;
	zp->z_last_itx = 0;
	zp->z_dbuf = db;
	zp->z_id = obj_num;
	zp->z_blksz = blksz;
	zp->z_seq = 0x7A4653;
	zp->z_sync_cnt = 0;

	mutex_enter(&zfsvfs->z_znodes_lock);
	list_insert_tail(&zfsvfs->z_all_znodes, zp);
	mutex_exit(&zfsvfs->z_znodes_lock);

	vp = ZTOV(zp);
	if (vp == NULL)
		return (zp);

	error = insmntque(vp, zfsvfs->z_vfs);
	KASSERT(error == 0, ("insmntque() failed: error %d", error));

	vp->v_type = IFTOVT((mode_t)zp->z_phys->zp_mode);
	switch (vp->v_type) {
	case VDIR:
		zp->z_zn_prefetch = B_TRUE; /* z_prefetch default is enabled */
		break;
	case VFIFO:
		vp->v_op = &zfs_fifoops;
		break;
	}

	return (zp);
}

static void
zfs_znode_dmu_init(znode_t *zp)
{
	znode_t		*nzp;
	zfsvfs_t	*zfsvfs = zp->z_zfsvfs;
	dmu_buf_t	*db = zp->z_dbuf;

	mutex_enter(&zp->z_lock);

	nzp = dmu_buf_set_user_ie(db, zp, &zp->z_phys, znode_pageout_func);

	/*
	 * there should be no
	 * concurrent zgets on this object.
	 */
	ASSERT3P(nzp, ==, NULL);

	/*
	 * Slap on VROOT if we are the root znode
	 */
	if (zp->z_id == zfsvfs->z_root) {
		ZTOV(zp)->v_flag |= VROOT;
	}

	ASSERT(zp->z_dbuf_held == 0);
	zp->z_dbuf_held = 1;
	VFS_HOLD(zfsvfs->z_vfs);
	mutex_exit(&zp->z_lock);
}

/*
 * Create a new DMU object to hold a zfs znode.
 *
 *	IN:	dzp	- parent directory for new znode
 *		vap	- file attributes for new znode
 *		tx	- dmu transaction id for zap operations
 *		cr	- credentials of caller
 *		flag	- flags:
 *			  IS_ROOT_NODE	- new object will be root
 *			  IS_XATTR	- new object is an attribute
 *			  IS_REPLAY	- intent log replay
 *
 *	OUT:	oid	- ID of created object
 *
 */
void
zfs_mknode(znode_t *dzp, vattr_t *vap, uint64_t *oid, dmu_tx_t *tx, cred_t *cr,
	uint_t flag, znode_t **zpp, int bonuslen)
{
	dmu_buf_t	*dbp;
	znode_phys_t	*pzp;
	znode_t		*zp;
	zfsvfs_t	*zfsvfs = dzp->z_zfsvfs;
	timestruc_t	now;
	uint64_t	gen;
	int		err;

	ASSERT(vap && (vap->va_mask & (AT_TYPE|AT_MODE)) == (AT_TYPE|AT_MODE));

	if (zfsvfs->z_assign >= TXG_INITIAL) {		/* ZIL replay */
		*oid = vap->va_nodeid;
		flag |= IS_REPLAY;
		now = vap->va_ctime;		/* see zfs_replay_create() */
		gen = vap->va_nblocks;		/* ditto */
	} else {
		*oid = 0;
		gethrestime(&now);
		gen = dmu_tx_get_txg(tx);
	}

	/*
	 * Create a new DMU object.
	 */
	/*
	 * There's currently no mechanism for pre-reading the blocks that will
	 * be to needed allocate a new object, so we accept the small chance
	 * that there will be an i/o error and we will fail one of the
	 * assertions below.
	 */
	if (vap->va_type == VDIR) {
		if (flag & IS_REPLAY) {
			err = zap_create_claim(zfsvfs->z_os, *oid,
			    DMU_OT_DIRECTORY_CONTENTS,
			    DMU_OT_ZNODE, sizeof (znode_phys_t) + bonuslen, tx);
			ASSERT3U(err, ==, 0);
		} else {
			*oid = zap_create(zfsvfs->z_os,
			    DMU_OT_DIRECTORY_CONTENTS,
			    DMU_OT_ZNODE, sizeof (znode_phys_t) + bonuslen, tx);
		}
	} else {
		if (flag & IS_REPLAY) {
			err = dmu_object_claim(zfsvfs->z_os, *oid,
			    DMU_OT_PLAIN_FILE_CONTENTS, 0,
			    DMU_OT_ZNODE, sizeof (znode_phys_t) + bonuslen, tx);
			ASSERT3U(err, ==, 0);
		} else {
			*oid = dmu_object_alloc(zfsvfs->z_os,
			    DMU_OT_PLAIN_FILE_CONTENTS, 0,
			    DMU_OT_ZNODE, sizeof (znode_phys_t) + bonuslen, tx);
		}
	}
	VERIFY(0 == dmu_bonus_hold(zfsvfs->z_os, *oid, NULL, &dbp));
	dmu_buf_will_dirty(dbp, tx);

	/*
	 * Initialize the znode physical data to zero.
	 */
	ASSERT(dbp->db_size >= sizeof (znode_phys_t));
	bzero(dbp->db_data, dbp->db_size);
	pzp = dbp->db_data;

	/*
	 * If this is the root, fix up the half-initialized parent pointer
	 * to reference the just-allocated physical data area.
	 */
	if (flag & IS_ROOT_NODE) {
		dzp->z_phys = pzp;
		dzp->z_id = *oid;
	}

	/*
	 * If parent is an xattr, so am I.
	 */
	if (dzp->z_phys->zp_flags & ZFS_XATTR)
		flag |= IS_XATTR;

	if (vap->va_type == VBLK || vap->va_type == VCHR) {
		pzp->zp_rdev = zfs_expldev(vap->va_rdev);
	}

	if (vap->va_type == VDIR) {
		pzp->zp_size = 2;		/* contents ("." and "..") */
		pzp->zp_links = (flag & (IS_ROOT_NODE | IS_XATTR)) ? 2 : 1;
	}

	pzp->zp_parent = dzp->z_id;
	if (flag & IS_XATTR)
		pzp->zp_flags |= ZFS_XATTR;

	pzp->zp_gen = gen;

	ZFS_TIME_ENCODE(&now, pzp->zp_crtime);
	ZFS_TIME_ENCODE(&now, pzp->zp_ctime);

	if (vap->va_mask & AT_ATIME) {
		ZFS_TIME_ENCODE(&vap->va_atime, pzp->zp_atime);
	} else {
		ZFS_TIME_ENCODE(&now, pzp->zp_atime);
	}

	if (vap->va_mask & AT_MTIME) {
		ZFS_TIME_ENCODE(&vap->va_mtime, pzp->zp_mtime);
	} else {
		ZFS_TIME_ENCODE(&now, pzp->zp_mtime);
	}

	pzp->zp_mode = MAKEIMODE(vap->va_type, vap->va_mode);
	zp = zfs_znode_alloc(zfsvfs, dbp, *oid, 0);

	zfs_perm_init(zp, dzp, flag, vap, tx, cr);

	if (zpp) {
		kmutex_t *hash_mtx = ZFS_OBJ_MUTEX(zp);

		mutex_enter(hash_mtx);
		zfs_znode_dmu_init(zp);
		mutex_exit(hash_mtx);

		*zpp = zp;
	} else {
		if (ZTOV(zp) != NULL)
			ZTOV(zp)->v_count = 0;
		dmu_buf_rele(dbp, NULL);
		zfs_znode_free(zp);
	}
}

int
zfs_zget(zfsvfs_t *zfsvfs, uint64_t obj_num, znode_t **zpp)
{
	dmu_object_info_t doi;
	dmu_buf_t	*db;
	znode_t		*zp;
	vnode_t		*vp;
	int err;

	*zpp = NULL;

	ZFS_OBJ_HOLD_ENTER(zfsvfs, obj_num);

	err = dmu_bonus_hold(zfsvfs->z_os, obj_num, NULL, &db);
	if (err) {
		ZFS_OBJ_HOLD_EXIT(zfsvfs, obj_num);
		return (err);
	}

	dmu_object_info_from_db(db, &doi);
	if (doi.doi_bonus_type != DMU_OT_ZNODE ||
	    doi.doi_bonus_size < sizeof (znode_phys_t)) {
		dmu_buf_rele(db, NULL);
		ZFS_OBJ_HOLD_EXIT(zfsvfs, obj_num);
		return (EINVAL);
	}

	ASSERT(db->db_object == obj_num);
	ASSERT(db->db_offset == -1);
	ASSERT(db->db_data != NULL);

	zp = dmu_buf_get_user(db);

	if (zp != NULL) {
		mutex_enter(&zp->z_lock);

		ASSERT3U(zp->z_id, ==, obj_num);
		if (zp->z_unlinked) {
			dmu_buf_rele(db, NULL);
			mutex_exit(&zp->z_lock);
			ZFS_OBJ_HOLD_EXIT(zfsvfs, obj_num);
			return (ENOENT);
		} else if (zp->z_dbuf_held) {
			dmu_buf_rele(db, NULL);
		} else {
			zp->z_dbuf_held = 1;
			VFS_HOLD(zfsvfs->z_vfs);
		}

		if (ZTOV(zp) != NULL)
			VN_HOLD(ZTOV(zp));
		else {
			err = getnewvnode("zfs", zfsvfs->z_vfs, &zfs_vnodeops,
			    &zp->z_vnode);
			ASSERT(err == 0);
			vp = ZTOV(zp);
			vp->v_data = (caddr_t)zp;
			VN_LOCK_AREC(vp);
			VN_LOCK_ASHARE(vp);
			vp->v_type = IFTOVT((mode_t)zp->z_phys->zp_mode);
			if (vp->v_type == VDIR)
				zp->z_zn_prefetch = B_TRUE;	/* z_prefetch default is enabled */
			err = insmntque(vp, zfsvfs->z_vfs);
			KASSERT(err == 0, ("insmntque() failed: error %d", err));
		}
		mutex_exit(&zp->z_lock);
		ZFS_OBJ_HOLD_EXIT(zfsvfs, obj_num);
		*zpp = zp;
		return (0);
	}

	/*
	 * Not found create new znode/vnode
	 */
	zp = zfs_znode_alloc(zfsvfs, db, obj_num, doi.doi_data_block_size);
	ASSERT3U(zp->z_id, ==, obj_num);
	zfs_znode_dmu_init(zp);
	ZFS_OBJ_HOLD_EXIT(zfsvfs, obj_num);
	*zpp = zp;
	return (0);
}

void
zfs_znode_delete(znode_t *zp, dmu_tx_t *tx)
{
	zfsvfs_t *zfsvfs = zp->z_zfsvfs;
	int error;

	ZFS_OBJ_HOLD_ENTER(zfsvfs, zp->z_id);
	if (zp->z_phys->zp_acl.z_acl_extern_obj) {
		error = dmu_object_free(zfsvfs->z_os,
		    zp->z_phys->zp_acl.z_acl_extern_obj, tx);
		ASSERT3U(error, ==, 0);
	}
	error = dmu_object_free(zfsvfs->z_os, zp->z_id, tx);
	ASSERT3U(error, ==, 0);
	zp->z_dbuf_held = 0;
	ZFS_OBJ_HOLD_EXIT(zfsvfs, zp->z_id);
	dmu_buf_rele(zp->z_dbuf, NULL);
}

void
zfs_zinactive(znode_t *zp)
{
	vnode_t	*vp = ZTOV(zp);
	zfsvfs_t *zfsvfs = zp->z_zfsvfs;
	uint64_t z_id = zp->z_id;

	ASSERT(zp->z_dbuf_held && zp->z_phys);

	/*
	 * Don't allow a zfs_zget() while were trying to release this znode
	 */
	ZFS_OBJ_HOLD_ENTER(zfsvfs, z_id);

	mutex_enter(&zp->z_lock);
	VI_LOCK(vp);
	if (vp->v_count > 0) {
		/*
		 * If the hold count is greater than zero, somebody has
		 * obtained a new reference on this znode while we were
		 * processing it here, so we are done.
		 */
		VI_UNLOCK(vp);
		mutex_exit(&zp->z_lock);
		ZFS_OBJ_HOLD_EXIT(zfsvfs, z_id);
		return;
	}
	VI_UNLOCK(vp);

	/*
	 * If this was the last reference to a file with no links,
	 * remove the file from the file system.
	 */
	if (zp->z_unlinked) {
		ZTOV(zp) = NULL;
		mutex_exit(&zp->z_lock);
		ZFS_OBJ_HOLD_EXIT(zfsvfs, z_id);
		ASSERT(vp->v_count == 0);
		vrecycle(vp, curthread);
		zfs_rmnode(zp);
		VFS_RELE(zfsvfs->z_vfs);
		return;
	}
	ASSERT(zp->z_phys);
	ASSERT(zp->z_dbuf_held);
	mutex_exit(&zp->z_lock);
	ZFS_OBJ_HOLD_EXIT(zfsvfs, z_id);
}

void
zfs_znode_free(znode_t *zp)
{
	zfsvfs_t *zfsvfs = zp->z_zfsvfs;

	mutex_enter(&zfsvfs->z_znodes_lock);
	list_remove(&zfsvfs->z_all_znodes, zp);
	mutex_exit(&zfsvfs->z_znodes_lock);

	kmem_cache_free(znode_cache, zp);
}

void
zfs_time_stamper_locked(znode_t *zp, uint_t flag, dmu_tx_t *tx)
{
	timestruc_t	now;

	ASSERT(MUTEX_HELD(&zp->z_lock));

	gethrestime(&now);

	if (tx) {
		dmu_buf_will_dirty(zp->z_dbuf, tx);
		zp->z_atime_dirty = 0;
		zp->z_seq++;
	} else {
		zp->z_atime_dirty = 1;
	}

	if (flag & AT_ATIME)
		ZFS_TIME_ENCODE(&now, zp->z_phys->zp_atime);

	if (flag & AT_MTIME)
		ZFS_TIME_ENCODE(&now, zp->z_phys->zp_mtime);

	if (flag & AT_CTIME)
		ZFS_TIME_ENCODE(&now, zp->z_phys->zp_ctime);
}

/*
 * Update the requested znode timestamps with the current time.
 * If we are in a transaction, then go ahead and mark the znode
 * dirty in the transaction so the timestamps will go to disk.
 * Otherwise, we will get pushed next time the znode is updated
 * in a transaction, or when this znode eventually goes inactive.
 *
 * Why is this OK?
 *  1 - Only the ACCESS time is ever updated outside of a transaction.
 *  2 - Multiple consecutive updates will be collapsed into a single
 *	znode update by the transaction grouping semantics of the DMU.
 */
void
zfs_time_stamper(znode_t *zp, uint_t flag, dmu_tx_t *tx)
{
	mutex_enter(&zp->z_lock);
	zfs_time_stamper_locked(zp, flag, tx);
	mutex_exit(&zp->z_lock);
}

/*
 * Grow the block size for a file.
 *
 *	IN:	zp	- znode of file to free data in.
 *		size	- requested block size
 *		tx	- open transaction.
 *
 * NOTE: this function assumes that the znode is write locked.
 */
void
zfs_grow_blocksize(znode_t *zp, uint64_t size, dmu_tx_t *tx)
{
	int		error;
	u_longlong_t	dummy;

	if (size <= zp->z_blksz)
		return;
	/*
	 * If the file size is already greater than the current blocksize,
	 * we will not grow.  If there is more than one block in a file,
	 * the blocksize cannot change.
	 */
	if (zp->z_blksz && zp->z_phys->zp_size > zp->z_blksz)
		return;

	error = dmu_object_set_blocksize(zp->z_zfsvfs->z_os, zp->z_id,
	    size, 0, tx);
	if (error == ENOTSUP)
		return;
	ASSERT3U(error, ==, 0);

	/* What blocksize did we actually get? */
	dmu_object_size_from_db(zp->z_dbuf, &zp->z_blksz, &dummy);
}

/*
 * Free space in a file.
 *
 *	IN:	zp	- znode of file to free data in.
 *		off	- start of section to free.
 *		len	- length of section to free (0 => to EOF).
 *		flag	- current file open mode flags.
 *
 * 	RETURN:	0 if success
 *		error code if failure
 */
int
zfs_freesp(znode_t *zp, uint64_t off, uint64_t len, int flag, boolean_t log)
{
	vnode_t *vp = ZTOV(zp);
	dmu_tx_t *tx;
	zfsvfs_t *zfsvfs = zp->z_zfsvfs;
	zilog_t *zilog = zfsvfs->z_log;
	rl_t *rl;
	uint64_t end = off + len;
	uint64_t size, new_blksz;
	int error;

	if (ZTOV(zp)->v_type == VFIFO)
		return (0);

	/*
	 * If we will change zp_size then lock the whole file,
	 * otherwise just lock the range being freed.
	 */
	if (len == 0 || off + len > zp->z_phys->zp_size) {
		rl = zfs_range_lock(zp, 0, UINT64_MAX, RL_WRITER);
	} else {
		rl = zfs_range_lock(zp, off, len, RL_WRITER);
		/* recheck, in case zp_size changed */
		if (off + len > zp->z_phys->zp_size) {
			/* lost race: file size changed, lock whole file */
			zfs_range_unlock(rl);
			rl = zfs_range_lock(zp, 0, UINT64_MAX, RL_WRITER);
		}
	}

	/*
	 * Nothing to do if file already at desired length.
	 */
	size = zp->z_phys->zp_size;
	if (len == 0 && size == off && off != 0) {
		zfs_range_unlock(rl);
		return (0);
	}

	tx = dmu_tx_create(zfsvfs->z_os);
	dmu_tx_hold_bonus(tx, zp->z_id);
	new_blksz = 0;
	if (end > size &&
	    (!ISP2(zp->z_blksz) || zp->z_blksz < zfsvfs->z_max_blksz)) {
		/*
		 * We are growing the file past the current block size.
		 */
		if (zp->z_blksz > zp->z_zfsvfs->z_max_blksz) {
			ASSERT(!ISP2(zp->z_blksz));
			new_blksz = MIN(end, SPA_MAXBLOCKSIZE);
		} else {
			new_blksz = MIN(end, zp->z_zfsvfs->z_max_blksz);
		}
		dmu_tx_hold_write(tx, zp->z_id, 0, MIN(end, new_blksz));
	} else if (off < size) {
		/*
		 * If len == 0, we are truncating the file.
		 */
		dmu_tx_hold_free(tx, zp->z_id, off, len ? len : DMU_OBJECT_END);
	}

	error = dmu_tx_assign(tx, zfsvfs->z_assign);
	if (error) {
		if (error == ERESTART && zfsvfs->z_assign == TXG_NOWAIT)
			dmu_tx_wait(tx);
		dmu_tx_abort(tx);
		zfs_range_unlock(rl);
		return (error);
	}

	if (new_blksz)
		zfs_grow_blocksize(zp, new_blksz, tx);

	if (end > size || len == 0)
		zp->z_phys->zp_size = end;

	if (off < size) {
		objset_t *os = zfsvfs->z_os;
		uint64_t rlen = len;

		if (len == 0)
			rlen = -1;
		else if (end > size)
			rlen = size - off;
		VERIFY(0 == dmu_free_range(os, zp->z_id, off, rlen, tx));
	}

	if (log) {
		zfs_time_stamper(zp, CONTENT_MODIFIED, tx);
		zfs_log_truncate(zilog, tx, TX_TRUNCATE, zp, off, len);
	}

	zfs_range_unlock(rl);

	dmu_tx_commit(tx);

	/*
	 * Clear any mapped pages in the truncated region.  This has to
	 * happen outside of the transaction to avoid the possibility of
	 * a deadlock with someone trying to push a page that we are
	 * about to invalidate.
	 */
	rw_enter(&zp->z_map_lock, RW_WRITER);
	if (end > size)
		vnode_pager_setsize(vp, end);
	else if (len == 0) {
#if 0
		error = vtruncbuf(vp, curthread->td_ucred, curthread, end, PAGE_SIZE);
#else
		error = vinvalbuf(vp, V_SAVE, curthread, 0, 0);
		vnode_pager_setsize(vp, end);
#endif
	}
	rw_exit(&zp->z_map_lock);

	return (0);
}

void
zfs_create_fs(objset_t *os, cred_t *cr, dmu_tx_t *tx)
{
	zfsvfs_t	zfsvfs;
	uint64_t	moid, doid, roid = 0;
	uint64_t	version = ZPL_VERSION;
	int		error;
	znode_t		*rootzp = NULL;
	vattr_t		vattr;

	/*
	 * First attempt to create master node.
	 */
	/*
	 * In an empty objset, there are no blocks to read and thus
	 * there can be no i/o errors (which we assert below).
	 */
	moid = MASTER_NODE_OBJ;
	error = zap_create_claim(os, moid, DMU_OT_MASTER_NODE,
	    DMU_OT_NONE, 0, tx);
	ASSERT(error == 0);

	/*
	 * Set starting attributes.
	 */

	error = zap_update(os, moid, ZPL_VERSION_OBJ, 8, 1, &version, tx);
	ASSERT(error == 0);

	/*
	 * Create a delete queue.
	 */
	doid = zap_create(os, DMU_OT_UNLINKED_SET, DMU_OT_NONE, 0, tx);

	error = zap_add(os, moid, ZFS_UNLINKED_SET, 8, 1, &doid, tx);
	ASSERT(error == 0);

	/*
	 * Create root znode.  Create minimal znode/vnode/zfsvfs
	 * to allow zfs_mknode to work.
	 */
	vattr.va_mask = AT_MODE|AT_UID|AT_GID|AT_TYPE;
	vattr.va_type = VDIR;
	vattr.va_mode = S_IFDIR|0755;
	vattr.va_uid = UID_ROOT;
	vattr.va_gid = GID_WHEEL;

	rootzp = kmem_cache_alloc(znode_cache, KM_SLEEP);
	zfs_znode_cache_constructor(rootzp, NULL, 0);
	rootzp->z_zfsvfs = &zfsvfs;
	rootzp->z_unlinked = 0;
	rootzp->z_atime_dirty = 0;
	rootzp->z_dbuf_held = 0;

	bzero(&zfsvfs, sizeof (zfsvfs_t));

	zfsvfs.z_os = os;
	zfsvfs.z_assign = TXG_NOWAIT;
	zfsvfs.z_parent = &zfsvfs;

	mutex_init(&zfsvfs.z_znodes_lock, NULL, MUTEX_DEFAULT, NULL);
	list_create(&zfsvfs.z_all_znodes, sizeof (znode_t),
	    offsetof(znode_t, z_link_node));

	zfs_mknode(rootzp, &vattr, &roid, tx, cr, IS_ROOT_NODE, NULL, 0);
	ASSERT3U(rootzp->z_id, ==, roid);
	error = zap_add(os, moid, ZFS_ROOT_OBJ, 8, 1, &roid, tx);
	ASSERT(error == 0);

	mutex_destroy(&zfsvfs.z_znodes_lock);
	kmem_cache_free(znode_cache, rootzp);
}
#endif /* _KERNEL */

/*
 * Given an object number, return its parent object number and whether
 * or not the object is an extended attribute directory.
 */
static int
zfs_obj_to_pobj(objset_t *osp, uint64_t obj, uint64_t *pobjp, int *is_xattrdir)
{
	dmu_buf_t *db;
	dmu_object_info_t doi;
	znode_phys_t *zp;
	int error;

	if ((error = dmu_bonus_hold(osp, obj, FTAG, &db)) != 0)
		return (error);

	dmu_object_info_from_db(db, &doi);
	if (doi.doi_bonus_type != DMU_OT_ZNODE ||
	    doi.doi_bonus_size < sizeof (znode_phys_t)) {
		dmu_buf_rele(db, FTAG);
		return (EINVAL);
	}

	zp = db->db_data;
	*pobjp = zp->zp_parent;
	*is_xattrdir = ((zp->zp_flags & ZFS_XATTR) != 0) &&
	    S_ISDIR(zp->zp_mode);
	dmu_buf_rele(db, FTAG);

	return (0);
}

int
zfs_obj_to_path(objset_t *osp, uint64_t obj, char *buf, int len)
{
	char *path = buf + len - 1;
	int error;

	*path = '\0';

	for (;;) {
		uint64_t pobj;
		char component[MAXNAMELEN + 2];
		size_t complen;
		int is_xattrdir;

		if ((error = zfs_obj_to_pobj(osp, obj, &pobj,
		    &is_xattrdir)) != 0)
			break;

		if (pobj == obj) {
			if (path[0] != '/')
				*--path = '/';
			break;
		}

		component[0] = '/';
		if (is_xattrdir) {
			(void) sprintf(component + 1, "<xattrdir>");
		} else {
			error = zap_value_search(osp, pobj, obj, component + 1);
			if (error != 0)
				break;
		}

		complen = strlen(component);
		path -= complen;
		ASSERT(path >= buf);
		bcopy(component, path, complen);
		obj = pobj;
	}

	if (error == 0)
		(void) memmove(buf, path, buf + len - path);
	return (error);
}
