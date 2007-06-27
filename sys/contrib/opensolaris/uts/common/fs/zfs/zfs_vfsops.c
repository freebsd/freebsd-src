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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/acl.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/mntent.h>
#include <sys/mount.h>
#include <sys/cmn_err.h>
#include <sys/zfs_znode.h>
#include <sys/zfs_dir.h>
#include <sys/zil.h>
#include <sys/fs/zfs.h>
#include <sys/dmu.h>
#include <sys/dsl_prop.h>
#include <sys/dsl_dataset.h>
#include <sys/spa.h>
#include <sys/zap.h>
#include <sys/varargs.h>
#include <sys/policy.h>
#include <sys/atomic.h>
#include <sys/zfs_ioctl.h>
#include <sys/zfs_ctldir.h>
#include <sys/sunddi.h>
#include <sys/dnlc.h>

struct mtx zfs_debug_mtx;
MTX_SYSINIT(zfs_debug_mtx, &zfs_debug_mtx, "zfs_debug", MTX_DEF);
SYSCTL_NODE(_vfs, OID_AUTO, zfs, CTLFLAG_RW, 0, "ZFS file system");
int zfs_debug_level = 0;
TUNABLE_INT("vfs.zfs.debug", &zfs_debug_level);
SYSCTL_INT(_vfs_zfs, OID_AUTO, debug, CTLFLAG_RW, &zfs_debug_level, 0,
    "Debug level");

static int zfs_mount(vfs_t *vfsp, kthread_t *td);
static int zfs_umount(vfs_t *vfsp, int fflag, kthread_t *td);
static int zfs_root(vfs_t *vfsp, int flags, vnode_t **vpp, kthread_t *td);
static int zfs_statfs(vfs_t *vfsp, struct statfs *statp, kthread_t *td);
static int zfs_vget(vfs_t *vfsp, ino_t ino, int flags, vnode_t **vpp);
static int zfs_sync(vfs_t *vfsp, int waitfor, kthread_t *td);
static int zfs_fhtovp(vfs_t *vfsp, fid_t *fidp, vnode_t **vpp);
static void zfs_objset_close(zfsvfs_t *zfsvfs);
static void zfs_freevfs(vfs_t *vfsp);

static struct vfsops zfs_vfsops = {
	.vfs_mount =		zfs_mount,
	.vfs_unmount =		zfs_umount,
	.vfs_root =		zfs_root,
	.vfs_statfs =		zfs_statfs,
	.vfs_vget =		zfs_vget,
	.vfs_sync =		zfs_sync,
	.vfs_fhtovp =		zfs_fhtovp,
};

VFS_SET(zfs_vfsops, zfs, VFCF_JAIL);

/*
 * We need to keep a count of active fs's.
 * This is necessary to prevent our module
 * from being unloaded after a umount -f
 */
static uint32_t	zfs_active_fs_count = 0;

/*ARGSUSED*/
static int
zfs_sync(vfs_t *vfsp, int waitfor, kthread_t *td)
{

	/*
	 * Data integrity is job one.  We don't want a compromised kernel
	 * writing to the storage pool, so we never sync during panic.
	 */
	if (panicstr)
		return (0);

	if (vfsp != NULL) {
		/*
		 * Sync a specific filesystem.
		 */
		zfsvfs_t *zfsvfs = vfsp->vfs_data;
		int error;

		error = vfs_stdsync(vfsp, waitfor, td);
		if (error != 0)
			return (error);

		ZFS_ENTER(zfsvfs);
		if (zfsvfs->z_log != NULL)
			zil_commit(zfsvfs->z_log, UINT64_MAX, 0);
		else
			txg_wait_synced(dmu_objset_pool(zfsvfs->z_os), 0);
		ZFS_EXIT(zfsvfs);
	} else {
		/*
		 * Sync all ZFS filesystems.  This is what happens when you
		 * run sync(1M).  Unlike other filesystems, ZFS honors the
		 * request by waiting for all pools to commit all dirty data.
		 */
		spa_sync_allpools();
	}

	return (0);
}

static void
atime_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;

	if (newval == TRUE) {
		zfsvfs->z_atime = TRUE;
		zfsvfs->z_vfs->vfs_flag &= ~MNT_NOATIME;
		vfs_clearmntopt(zfsvfs->z_vfs, MNTOPT_NOATIME);
		vfs_setmntopt(zfsvfs->z_vfs, MNTOPT_ATIME, NULL, 0);
	} else {
		zfsvfs->z_atime = FALSE;
		zfsvfs->z_vfs->vfs_flag |= MNT_NOATIME;
		vfs_clearmntopt(zfsvfs->z_vfs, MNTOPT_ATIME);
		vfs_setmntopt(zfsvfs->z_vfs, MNTOPT_NOATIME, NULL, 0);
	}
}

static void
xattr_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;

	if (newval == TRUE) {
		/* XXX locking on vfs_flag? */
#ifdef TODO
		zfsvfs->z_vfs->vfs_flag |= VFS_XATTR;
#endif
		vfs_clearmntopt(zfsvfs->z_vfs, MNTOPT_NOXATTR);
		vfs_setmntopt(zfsvfs->z_vfs, MNTOPT_XATTR, NULL, 0);
	} else {
		/* XXX locking on vfs_flag? */
#ifdef TODO
		zfsvfs->z_vfs->vfs_flag &= ~VFS_XATTR;
#endif
		vfs_clearmntopt(zfsvfs->z_vfs, MNTOPT_XATTR);
		vfs_setmntopt(zfsvfs->z_vfs, MNTOPT_NOXATTR, NULL, 0);
	}
}

static void
blksz_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;

	if (newval < SPA_MINBLOCKSIZE ||
	    newval > SPA_MAXBLOCKSIZE || !ISP2(newval))
		newval = SPA_MAXBLOCKSIZE;

	zfsvfs->z_max_blksz = newval;
	zfsvfs->z_vfs->vfs_bsize = newval;
}

static void
readonly_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;

	if (newval) {
		/* XXX locking on vfs_flag? */
		zfsvfs->z_vfs->vfs_flag |= VFS_RDONLY;
		vfs_clearmntopt(zfsvfs->z_vfs, MNTOPT_RW);
		vfs_setmntopt(zfsvfs->z_vfs, MNTOPT_RO, NULL, 0);
	} else {
		/* XXX locking on vfs_flag? */
		zfsvfs->z_vfs->vfs_flag &= ~VFS_RDONLY;
		vfs_clearmntopt(zfsvfs->z_vfs, MNTOPT_RO);
		vfs_setmntopt(zfsvfs->z_vfs, MNTOPT_RW, NULL, 0);
	}
}

static void
setuid_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;

	if (newval == FALSE) {
		zfsvfs->z_vfs->vfs_flag |= VFS_NOSETUID;
		vfs_clearmntopt(zfsvfs->z_vfs, MNTOPT_SETUID);
		vfs_setmntopt(zfsvfs->z_vfs, MNTOPT_NOSETUID, NULL, 0);
	} else {
		zfsvfs->z_vfs->vfs_flag &= ~VFS_NOSETUID;
		vfs_clearmntopt(zfsvfs->z_vfs, MNTOPT_NOSETUID);
		vfs_setmntopt(zfsvfs->z_vfs, MNTOPT_SETUID, NULL, 0);
	}
}

static void
exec_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;

	if (newval == FALSE) {
		zfsvfs->z_vfs->vfs_flag |= VFS_NOEXEC;
		vfs_clearmntopt(zfsvfs->z_vfs, MNTOPT_EXEC);
		vfs_setmntopt(zfsvfs->z_vfs, MNTOPT_NOEXEC, NULL, 0);
	} else {
		zfsvfs->z_vfs->vfs_flag &= ~VFS_NOEXEC;
		vfs_clearmntopt(zfsvfs->z_vfs, MNTOPT_NOEXEC);
		vfs_setmntopt(zfsvfs->z_vfs, MNTOPT_EXEC, NULL, 0);
	}
}

static void
snapdir_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;

	zfsvfs->z_show_ctldir = newval;
}

static void
acl_mode_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;

	zfsvfs->z_acl_mode = newval;
}

static void
acl_inherit_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;

	zfsvfs->z_acl_inherit = newval;
}

static int
zfs_refresh_properties(vfs_t *vfsp)
{
	zfsvfs_t *zfsvfs = vfsp->vfs_data;

	/*
	 * Remount operations default to "rw" unless "ro" is explicitly
	 * specified.
	 */
	if (vfs_optionisset(vfsp, MNTOPT_RO, NULL)) {
		readonly_changed_cb(zfsvfs, B_TRUE);
	} else {
		if (!dmu_objset_is_snapshot(zfsvfs->z_os))
			readonly_changed_cb(zfsvfs, B_FALSE);
		else if (vfs_optionisset(vfsp, MNTOPT_RW, NULL))
			return (EROFS);
	}

	if (vfs_optionisset(vfsp, MNTOPT_NOSUID, NULL)) {
		setuid_changed_cb(zfsvfs, B_FALSE);
	} else {
		if (vfs_optionisset(vfsp, MNTOPT_NOSETUID, NULL))
			setuid_changed_cb(zfsvfs, B_FALSE);
		else if (vfs_optionisset(vfsp, MNTOPT_SETUID, NULL))
			setuid_changed_cb(zfsvfs, B_TRUE);
	}

	if (vfs_optionisset(vfsp, MNTOPT_NOEXEC, NULL))
		exec_changed_cb(zfsvfs, B_FALSE);
	else if (vfs_optionisset(vfsp, MNTOPT_EXEC, NULL))
		exec_changed_cb(zfsvfs, B_TRUE);

	if (vfs_optionisset(vfsp, MNTOPT_ATIME, NULL))
		atime_changed_cb(zfsvfs, B_TRUE);
	else if (vfs_optionisset(vfsp, MNTOPT_NOATIME, NULL))
		atime_changed_cb(zfsvfs, B_FALSE);

	if (vfs_optionisset(vfsp, MNTOPT_XATTR, NULL))
		xattr_changed_cb(zfsvfs, B_TRUE);
	else if (vfs_optionisset(vfsp, MNTOPT_NOXATTR, NULL))
		xattr_changed_cb(zfsvfs, B_FALSE);

	return (0);
}

static int
zfs_register_callbacks(vfs_t *vfsp)
{
	struct dsl_dataset *ds = NULL;
	objset_t *os = NULL;
	zfsvfs_t *zfsvfs = NULL;
	int readonly, do_readonly = FALSE;
	int setuid, do_setuid = FALSE;
	int exec, do_exec = FALSE;
	int xattr, do_xattr = FALSE;
	int error = 0;

	ASSERT(vfsp);
	zfsvfs = vfsp->vfs_data;
	ASSERT(zfsvfs);
	os = zfsvfs->z_os;

	/*
	 * The act of registering our callbacks will destroy any mount
	 * options we may have.  In order to enable temporary overrides
	 * of mount options, we stash away the current values and
	 * restore them after we register the callbacks.
	 */
	if (vfs_optionisset(vfsp, MNTOPT_RO, NULL)) {
		readonly = B_TRUE;
		do_readonly = B_TRUE;
	} else if (vfs_optionisset(vfsp, MNTOPT_RW, NULL)) {
		readonly = B_FALSE;
		do_readonly = B_TRUE;
	}
	if (vfs_optionisset(vfsp, MNTOPT_NOSUID, NULL)) {
		setuid = B_FALSE;
		do_setuid = B_TRUE;
	} else {
		if (vfs_optionisset(vfsp, MNTOPT_NOSETUID, NULL)) {
			setuid = B_FALSE;
			do_setuid = B_TRUE;
		} else if (vfs_optionisset(vfsp, MNTOPT_SETUID, NULL)) {
			setuid = B_TRUE;
			do_setuid = B_TRUE;
		}
	}
	if (vfs_optionisset(vfsp, MNTOPT_NOEXEC, NULL)) {
		exec = B_FALSE;
		do_exec = B_TRUE;
	} else if (vfs_optionisset(vfsp, MNTOPT_EXEC, NULL)) {
		exec = B_TRUE;
		do_exec = B_TRUE;
	}
	if (vfs_optionisset(vfsp, MNTOPT_NOXATTR, NULL)) {
		xattr = B_FALSE;
		do_xattr = B_TRUE;
	} else if (vfs_optionisset(vfsp, MNTOPT_XATTR, NULL)) {
		xattr = B_TRUE;
		do_xattr = B_TRUE;
	}

	/*
	 * Register property callbacks.
	 *
	 * It would probably be fine to just check for i/o error from
	 * the first prop_register(), but I guess I like to go
	 * overboard...
	 */
	ds = dmu_objset_ds(os);
	error = dsl_prop_register(ds, "atime", atime_changed_cb, zfsvfs);
	error = error ? error : dsl_prop_register(ds,
	    "xattr", xattr_changed_cb, zfsvfs);
	error = error ? error : dsl_prop_register(ds,
	    "recordsize", blksz_changed_cb, zfsvfs);
	error = error ? error : dsl_prop_register(ds,
	    "readonly", readonly_changed_cb, zfsvfs);
	error = error ? error : dsl_prop_register(ds,
	    "setuid", setuid_changed_cb, zfsvfs);
	error = error ? error : dsl_prop_register(ds,
	    "exec", exec_changed_cb, zfsvfs);
	error = error ? error : dsl_prop_register(ds,
	    "snapdir", snapdir_changed_cb, zfsvfs);
	error = error ? error : dsl_prop_register(ds,
	    "aclmode", acl_mode_changed_cb, zfsvfs);
	error = error ? error : dsl_prop_register(ds,
	    "aclinherit", acl_inherit_changed_cb, zfsvfs);
	if (error)
		goto unregister;

	/*
	 * Invoke our callbacks to restore temporary mount options.
	 */
	if (do_readonly)
		readonly_changed_cb(zfsvfs, readonly);
	if (do_setuid)
		setuid_changed_cb(zfsvfs, setuid);
	if (do_exec)
		exec_changed_cb(zfsvfs, exec);
	if (do_xattr)
		xattr_changed_cb(zfsvfs, xattr);

	return (0);

unregister:
	/*
	 * We may attempt to unregister some callbacks that are not
	 * registered, but this is OK; it will simply return ENOMSG,
	 * which we will ignore.
	 */
	(void) dsl_prop_unregister(ds, "atime", atime_changed_cb, zfsvfs);
	(void) dsl_prop_unregister(ds, "xattr", xattr_changed_cb, zfsvfs);
	(void) dsl_prop_unregister(ds, "recordsize", blksz_changed_cb, zfsvfs);
	(void) dsl_prop_unregister(ds, "readonly", readonly_changed_cb, zfsvfs);
	(void) dsl_prop_unregister(ds, "setuid", setuid_changed_cb, zfsvfs);
	(void) dsl_prop_unregister(ds, "exec", exec_changed_cb, zfsvfs);
	(void) dsl_prop_unregister(ds, "snapdir", snapdir_changed_cb, zfsvfs);
	(void) dsl_prop_unregister(ds, "aclmode", acl_mode_changed_cb, zfsvfs);
	(void) dsl_prop_unregister(ds, "aclinherit", acl_inherit_changed_cb,
	    zfsvfs);
	return (error);

}

static int
zfs_domount(vfs_t *vfsp, char *osname, kthread_t *td)
{
	cred_t *cr = td->td_ucred;
	uint64_t recordsize, readonly;
	int error = 0;
	int mode;
	zfsvfs_t *zfsvfs;
	znode_t *zp = NULL;

	ASSERT(vfsp);
	ASSERT(osname);

	/*
	 * Initialize the zfs-specific filesystem structure.
	 * Should probably make this a kmem cache, shuffle fields,
	 * and just bzero up to z_hold_mtx[].
	 */
	zfsvfs = kmem_zalloc(sizeof (zfsvfs_t), KM_SLEEP);
	zfsvfs->z_vfs = vfsp;
	zfsvfs->z_parent = zfsvfs;
	zfsvfs->z_assign = TXG_NOWAIT;
	zfsvfs->z_max_blksz = SPA_MAXBLOCKSIZE;
	zfsvfs->z_show_ctldir = ZFS_SNAPDIR_VISIBLE;

	mutex_init(&zfsvfs->z_znodes_lock, NULL, MUTEX_DEFAULT, NULL);
	list_create(&zfsvfs->z_all_znodes, sizeof (znode_t),
	    offsetof(znode_t, z_link_node));
	rw_init(&zfsvfs->z_um_lock, NULL, RW_DEFAULT, NULL);

	if (error = dsl_prop_get_integer(osname, "recordsize", &recordsize,
	    NULL))
		goto out;
	zfsvfs->z_vfs->vfs_bsize = recordsize;

	vfsp->vfs_data = zfsvfs;
	vfsp->mnt_flag |= MNT_LOCAL;
	vfsp->mnt_kern_flag |= MNTK_MPSAFE;
	vfsp->mnt_kern_flag |= MNTK_LOOKUP_SHARED;

	if (error = dsl_prop_get_integer(osname, "readonly", &readonly, NULL))
		goto out;

	if (readonly)
		mode = DS_MODE_PRIMARY | DS_MODE_READONLY;
	else
		mode = DS_MODE_PRIMARY;

	error = dmu_objset_open(osname, DMU_OST_ZFS, mode, &zfsvfs->z_os);
	if (error == EROFS) {
		mode = DS_MODE_PRIMARY | DS_MODE_READONLY;
		error = dmu_objset_open(osname, DMU_OST_ZFS, mode,
		    &zfsvfs->z_os);
	}

	if (error)
		goto out;

	if (error = zfs_init_fs(zfsvfs, &zp, cr))
		goto out;

	if (dmu_objset_is_snapshot(zfsvfs->z_os)) {
		uint64_t xattr;

		ASSERT(mode & DS_MODE_READONLY);
		atime_changed_cb(zfsvfs, B_FALSE);
		readonly_changed_cb(zfsvfs, B_TRUE);
		if (error = dsl_prop_get_integer(osname, "xattr", &xattr, NULL))
			goto out;
		xattr_changed_cb(zfsvfs, xattr);
		zfsvfs->z_issnap = B_TRUE;
	} else {
		error = zfs_register_callbacks(vfsp);
		if (error)
			goto out;

		zfs_unlinked_drain(zfsvfs);

		/*
		 * Parse and replay the intent log.
		 */
		zil_replay(zfsvfs->z_os, zfsvfs, &zfsvfs->z_assign,
		    zfs_replay_vector);

		if (!zil_disable)
			zfsvfs->z_log = zil_open(zfsvfs->z_os, zfs_get_data);
	}

	vfs_mountedfrom(vfsp, osname);

	if (!zfsvfs->z_issnap)
		zfsctl_create(zfsvfs);
out:
	if (error) {
		if (zfsvfs->z_os)
			dmu_objset_close(zfsvfs->z_os);
		rw_destroy(&zfsvfs->z_um_lock);
		mutex_destroy(&zfsvfs->z_znodes_lock);
		kmem_free(zfsvfs, sizeof (zfsvfs_t));
	} else {
		atomic_add_32(&zfs_active_fs_count, 1);
	}

	return (error);

}

void
zfs_unregister_callbacks(zfsvfs_t *zfsvfs)
{
	objset_t *os = zfsvfs->z_os;
	struct dsl_dataset *ds;

	/*
	 * Unregister properties.
	 */
	if (!dmu_objset_is_snapshot(os)) {
		ds = dmu_objset_ds(os);
		VERIFY(dsl_prop_unregister(ds, "atime", atime_changed_cb,
		    zfsvfs) == 0);

		VERIFY(dsl_prop_unregister(ds, "xattr", xattr_changed_cb,
		    zfsvfs) == 0);

		VERIFY(dsl_prop_unregister(ds, "recordsize", blksz_changed_cb,
		    zfsvfs) == 0);

		VERIFY(dsl_prop_unregister(ds, "readonly", readonly_changed_cb,
		    zfsvfs) == 0);

		VERIFY(dsl_prop_unregister(ds, "setuid", setuid_changed_cb,
		    zfsvfs) == 0);

		VERIFY(dsl_prop_unregister(ds, "exec", exec_changed_cb,
		    zfsvfs) == 0);

		VERIFY(dsl_prop_unregister(ds, "snapdir", snapdir_changed_cb,
		    zfsvfs) == 0);

		VERIFY(dsl_prop_unregister(ds, "aclmode", acl_mode_changed_cb,
		    zfsvfs) == 0);

		VERIFY(dsl_prop_unregister(ds, "aclinherit",
		    acl_inherit_changed_cb, zfsvfs) == 0);
	}
}

/*ARGSUSED*/
static int
zfs_mount(vfs_t *vfsp, kthread_t *td)
{
	char *from;
	int error;

	/*
	 * When doing a remount, we simply refresh our temporary properties
	 * according to those options set in the current VFS options.
	 */
	if (vfsp->vfs_flag & MS_REMOUNT)
		return (zfs_refresh_properties(vfsp));

	if (vfs_getopt(vfsp->mnt_optnew, "from", (void **)&from, NULL))
		return (EINVAL);

	DROP_GIANT();
	error = zfs_domount(vfsp, from, td);
	PICKUP_GIANT();
	return (error);
}

static int
zfs_statfs(vfs_t *vfsp, struct statfs *statp, kthread_t *td)
{
	zfsvfs_t *zfsvfs = vfsp->vfs_data;
	uint64_t refdbytes, availbytes, usedobjs, availobjs;

	statp->f_version = STATFS_VERSION;

	ZFS_ENTER(zfsvfs);

	dmu_objset_space(zfsvfs->z_os,
	    &refdbytes, &availbytes, &usedobjs, &availobjs);

	/*
	 * The underlying storage pool actually uses multiple block sizes.
	 * We report the fragsize as the smallest block size we support,
	 * and we report our blocksize as the filesystem's maximum blocksize.
	 */
	statp->f_bsize = zfsvfs->z_vfs->vfs_bsize;
	statp->f_iosize = zfsvfs->z_vfs->vfs_bsize;

	/*
	 * The following report "total" blocks of various kinds in the
	 * file system, but reported in terms of f_frsize - the
	 * "fragment" size.
	 */

	statp->f_blocks = (refdbytes + availbytes) / statp->f_bsize;
	statp->f_bfree = availbytes / statp->f_bsize;
	statp->f_bavail = statp->f_bfree; /* no root reservation */

	/*
	 * statvfs() should really be called statufs(), because it assumes
	 * static metadata.  ZFS doesn't preallocate files, so the best
	 * we can do is report the max that could possibly fit in f_files,
	 * and that minus the number actually used in f_ffree.
	 * For f_ffree, report the smaller of the number of object available
	 * and the number of blocks (each object will take at least a block).
	 */
	statp->f_ffree = MIN(availobjs, statp->f_bfree);
	statp->f_files = statp->f_ffree + usedobjs;

	/*
	 * We're a zfs filesystem.
	 */
	(void) strlcpy(statp->f_fstypename, "zfs", sizeof(statp->f_fstypename));

	strlcpy(statp->f_mntfromname, vfsp->mnt_stat.f_mntfromname,
	    sizeof(statp->f_mntfromname));
	strlcpy(statp->f_mntonname, vfsp->mnt_stat.f_mntonname,
	    sizeof(statp->f_mntonname));

	statp->f_namemax = ZFS_MAXNAMELEN;

	ZFS_EXIT(zfsvfs);
	return (0);
}

static int
zfs_root(vfs_t *vfsp, int flags, vnode_t **vpp, kthread_t *td)
{
	zfsvfs_t *zfsvfs = vfsp->vfs_data;
	znode_t *rootzp;
	int error;

	ZFS_ENTER(zfsvfs);

	error = zfs_zget(zfsvfs, zfsvfs->z_root, &rootzp);
	if (error == 0) {
		*vpp = ZTOV(rootzp);
		error = vn_lock(*vpp, flags, td);
		(*vpp)->v_vflag |= VV_ROOT;
	}

	ZFS_EXIT(zfsvfs);
	return (error);
}

/*ARGSUSED*/
static int
zfs_umount(vfs_t *vfsp, int fflag, kthread_t *td)
{
	zfsvfs_t *zfsvfs = vfsp->vfs_data;
	cred_t *cr = td->td_ucred;
	int ret;

	if ((ret = secpolicy_fs_unmount(cr, vfsp)) != 0)
		return (ret);

	(void) dnlc_purge_vfsp(vfsp, 0);

	/*
	 * Unmount any snapshots mounted under .zfs before unmounting the
	 * dataset itself.
	 */
	if (zfsvfs->z_ctldir != NULL) {
		if ((ret = zfsctl_umount_snapshots(vfsp, fflag, cr)) != 0)
			return (ret);
		ret = vflush(vfsp, 0, 0, td);
		ASSERT(ret == EBUSY);
		if (!(fflag & MS_FORCE)) {
			if (zfsvfs->z_ctldir->v_count > 1)
				return (EBUSY);
			ASSERT(zfsvfs->z_ctldir->v_count == 1);
		}
		zfsctl_destroy(zfsvfs);
		ASSERT(zfsvfs->z_ctldir == NULL);
	}

	/*
	 * Flush all the files.
	 */
	ret = vflush(vfsp, 1, (fflag & MS_FORCE) ? FORCECLOSE : 0, td);
	if (ret != 0) {
		if (!zfsvfs->z_issnap) {
			zfsctl_create(zfsvfs);
			ASSERT(zfsvfs->z_ctldir != NULL);
		}
		return (ret);
	}

	if (fflag & MS_FORCE) {
		MNT_ILOCK(vfsp);
		vfsp->mnt_kern_flag |= MNTK_UNMOUNTF;
		MNT_IUNLOCK(vfsp);
		zfsvfs->z_unmounted1 = B_TRUE;

		/*
		 * Wait for all zfs threads to leave zfs.
		 * Grabbing a rwlock as reader in all vops and
		 * as writer here doesn't work because it too easy to get
		 * multiple reader enters as zfs can re-enter itself.
		 * This can lead to deadlock if there is an intervening
		 * rw_enter as writer.
		 * So a file system threads ref count (z_op_cnt) is used.
		 * A polling loop on z_op_cnt may seem inefficient, but
		 * - this saves all threads on exit from having to grab a
		 *   mutex in order to cv_signal
		 * - only occurs on forced unmount in the rare case when
		 *   there are outstanding threads within the file system.
		 */
		while (zfsvfs->z_op_cnt) {
			delay(1);
		}
	}

	zfs_objset_close(zfsvfs);
	VFS_RELE(vfsp);
	zfs_freevfs(vfsp);

	return (0);
}

static int
zfs_vget(vfs_t *vfsp, ino_t ino, int flags, vnode_t **vpp)
{
	zfsvfs_t	*zfsvfs = vfsp->vfs_data;
	znode_t		*zp;
	int 		err;

	ZFS_ENTER(zfsvfs);
	err = zfs_zget(zfsvfs, ino, &zp);
	if (err == 0 && zp->z_unlinked) {
		VN_RELE(ZTOV(zp));
		err = EINVAL;
	}
	if (err != 0)
		*vpp = NULL;
	else {
		*vpp = ZTOV(zp);
		vn_lock(*vpp, flags, curthread);
	}
	ZFS_EXIT(zfsvfs);
	return (err);
}

static int
zfs_fhtovp(vfs_t *vfsp, fid_t *fidp, vnode_t **vpp)
{
	kthread_t	*td = curthread;
	zfsvfs_t	*zfsvfs = vfsp->vfs_data;
	znode_t		*zp;
	uint64_t	object = 0;
	uint64_t	fid_gen = 0;
	uint64_t	gen_mask;
	uint64_t	zp_gen;
	int		i, err;

	*vpp = NULL;

	ZFS_ENTER(zfsvfs);

	if (fidp->fid_len == LONG_FID_LEN) {
		zfid_long_t	*zlfid = (zfid_long_t *)fidp;
		uint64_t	objsetid = 0;
		uint64_t	setgen = 0;

		for (i = 0; i < sizeof (zlfid->zf_setid); i++)
			objsetid |= ((uint64_t)zlfid->zf_setid[i]) << (8 * i);

		for (i = 0; i < sizeof (zlfid->zf_setgen); i++)
			setgen |= ((uint64_t)zlfid->zf_setgen[i]) << (8 * i);

		ZFS_EXIT(zfsvfs);

		err = zfsctl_lookup_objset(vfsp, objsetid, &zfsvfs);
		if (err)
			return (EINVAL);
		ZFS_ENTER(zfsvfs);
	}

	if (fidp->fid_len == SHORT_FID_LEN || fidp->fid_len == LONG_FID_LEN) {
		zfid_short_t	*zfid = (zfid_short_t *)fidp;

		for (i = 0; i < sizeof (zfid->zf_object); i++)
			object |= ((uint64_t)zfid->zf_object[i]) << (8 * i);

		for (i = 0; i < sizeof (zfid->zf_gen); i++)
			fid_gen |= ((uint64_t)zfid->zf_gen[i]) << (8 * i);
	} else {
		ZFS_EXIT(zfsvfs);
		return (EINVAL);
	}

	/* A zero fid_gen means we are in the .zfs control directories */
	if (fid_gen == 0 &&
	    (object == ZFSCTL_INO_ROOT || object == ZFSCTL_INO_SNAPDIR)) {
		*vpp = zfsvfs->z_ctldir;
		ASSERT(*vpp != NULL);
		if (object == ZFSCTL_INO_SNAPDIR) {
			VERIFY(zfsctl_root_lookup(*vpp, "snapshot", vpp, NULL,
			    0, NULL, NULL) == 0);
		} else {
			VN_HOLD(*vpp);
		}
		ZFS_EXIT(zfsvfs);
		/* XXX: LK_RETRY? */
		vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY, td);
		return (0);
	}

	gen_mask = -1ULL >> (64 - 8 * i);

	dprintf("getting %llu [%u mask %llx]\n", object, fid_gen, gen_mask);
	if (err = zfs_zget(zfsvfs, object, &zp)) {
		ZFS_EXIT(zfsvfs);
		return (err);
	}
	zp_gen = zp->z_phys->zp_gen & gen_mask;
	if (zp_gen == 0)
		zp_gen = 1;
	if (zp->z_unlinked || zp_gen != fid_gen) {
		dprintf("znode gen (%u) != fid gen (%u)\n", zp_gen, fid_gen);
		VN_RELE(ZTOV(zp));
		ZFS_EXIT(zfsvfs);
		return (EINVAL);
	}

	*vpp = ZTOV(zp);
	/* XXX: LK_RETRY? */
	vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY, td);
	vnode_create_vobject(*vpp, zp->z_phys->zp_size, td);
	ZFS_EXIT(zfsvfs);
	return (0);
}

static void
zfs_objset_close(zfsvfs_t *zfsvfs)
{
	znode_t		*zp, *nextzp;
	objset_t	*os = zfsvfs->z_os;

	/*
	 * For forced unmount, at this point all vops except zfs_inactive
	 * are erroring EIO. We need to now suspend zfs_inactive threads
	 * while we are freeing dbufs before switching zfs_inactive
	 * to use behaviour without a objset.
	 */
	rw_enter(&zfsvfs->z_um_lock, RW_WRITER);

	/*
	 * Release all holds on dbufs
	 * Note, although we have stopped all other vop threads and
	 * zfs_inactive(), the dmu can callback via znode_pageout_func()
	 * which can zfs_znode_free() the znode.
	 * So we lock z_all_znodes; search the list for a held
	 * dbuf; drop the lock (we know zp can't disappear if we hold
	 * a dbuf lock; then regrab the lock and restart.
	 */
	mutex_enter(&zfsvfs->z_znodes_lock);
	for (zp = list_head(&zfsvfs->z_all_znodes); zp; zp = nextzp) {
		nextzp = list_next(&zfsvfs->z_all_znodes, zp);
		if (zp->z_dbuf_held) {
			/* dbufs should only be held when force unmounting */
			zp->z_dbuf_held = 0;
			mutex_exit(&zfsvfs->z_znodes_lock);
			dmu_buf_rele(zp->z_dbuf, NULL);
			/* Start again */
			mutex_enter(&zfsvfs->z_znodes_lock);
			nextzp = list_head(&zfsvfs->z_all_znodes);
		}
	}
	mutex_exit(&zfsvfs->z_znodes_lock);

	/*
	 * Unregister properties.
	 */
	if (!dmu_objset_is_snapshot(os))
		zfs_unregister_callbacks(zfsvfs);

	/*
	 * Switch zfs_inactive to behaviour without an objset.
	 * It just tosses cached pages and frees the znode & vnode.
	 * Then re-enable zfs_inactive threads in that new behaviour.
	 */
	zfsvfs->z_unmounted2 = B_TRUE;
	rw_exit(&zfsvfs->z_um_lock); /* re-enable any zfs_inactive threads */

	/*
	 * Close the zil. Can't close the zil while zfs_inactive
	 * threads are blocked as zil_close can call zfs_inactive.
	 */
	if (zfsvfs->z_log) {
		zil_close(zfsvfs->z_log);
		zfsvfs->z_log = NULL;
	}

	/*
	 * Evict all dbufs so that cached znodes will be freed
	 */
	if (dmu_objset_evict_dbufs(os, 1)) {
		txg_wait_synced(dmu_objset_pool(zfsvfs->z_os), 0);
		(void) dmu_objset_evict_dbufs(os, 0);
	}

	/*
	 * Finally close the objset
	 */
	dmu_objset_close(os);
}

static void
zfs_freevfs(vfs_t *vfsp)
{
	zfsvfs_t *zfsvfs = vfsp->vfs_data;
	int i;

	for (i = 0; i != ZFS_OBJ_MTX_SZ; i++)
		mutex_destroy(&zfsvfs->z_hold_mtx[i]);
	rw_destroy(&zfsvfs->z_um_lock);
	mutex_destroy(&zfsvfs->z_znodes_lock);
	kmem_free(zfsvfs, sizeof (zfsvfs_t));

	atomic_add_32(&zfs_active_fs_count, -1);
}

void
zfs_init(void)
{

	printf("ZFS filesystem version " ZFS_VERSION_STRING "\n");

	/*
	 * Initialize .zfs directory structures
	 */
	zfsctl_init();

	/*
	 * Initialize znode cache, vnode ops, etc...
	 */
	zfs_znode_init();
}

void
zfs_fini(void)
{
	zfsctl_fini();
	zfs_znode_fini();
}

int
zfs_busy(void)
{
	return (zfs_active_fs_count != 0);
}
