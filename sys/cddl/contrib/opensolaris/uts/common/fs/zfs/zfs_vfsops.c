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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

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
#include <sys/dsl_deleg.h>
#include <sys/spa.h>
#include <sys/zap.h>
#include <sys/varargs.h>
#include <sys/policy.h>
#include <sys/atomic.h>
#include <sys/zfs_ioctl.h>
#include <sys/zfs_ctldir.h>
#include <sys/zfs_fuid.h>
#include <sys/sunddi.h>
#include <sys/dnlc.h>
#include <sys/dmu_objset.h>
#include <sys/spa_boot.h>
#include <sys/vdev_impl.h>	/* VDEV_BOOT_VERSION */

struct mtx zfs_debug_mtx;
MTX_SYSINIT(zfs_debug_mtx, &zfs_debug_mtx, "zfs_debug", MTX_DEF);

SYSCTL_NODE(_vfs, OID_AUTO, zfs, CTLFLAG_RW, 0, "ZFS file system");

int zfs_super_owner = 0;
SYSCTL_INT(_vfs_zfs, OID_AUTO, super_owner, CTLFLAG_RW, &zfs_super_owner, 0,
    "File system owner can perform privileged operation on his file systems");

int zfs_debug_level = 0;
TUNABLE_INT("vfs.zfs.debug", &zfs_debug_level);
SYSCTL_INT(_vfs_zfs, OID_AUTO, debug, CTLFLAG_RW, &zfs_debug_level, 0,
    "Debug level");

SYSCTL_NODE(_vfs_zfs, OID_AUTO, version, CTLFLAG_RD, 0, "ZFS versions");
static int zfs_version_acl = ZFS_ACL_VERSION;
SYSCTL_INT(_vfs_zfs_version, OID_AUTO, acl, CTLFLAG_RD, &zfs_version_acl, 0,
    "ZFS_ACL_VERSION");
static int zfs_version_dmu_backup_header = DMU_BACKUP_HEADER_VERSION;
SYSCTL_INT(_vfs_zfs_version, OID_AUTO, dmu_backup_header, CTLFLAG_RD,
    &zfs_version_dmu_backup_header, 0, "DMU_BACKUP_HEADER_VERSION");
static int zfs_version_dmu_backup_stream = DMU_BACKUP_STREAM_VERSION;
SYSCTL_INT(_vfs_zfs_version, OID_AUTO, dmu_backup_stream, CTLFLAG_RD,
    &zfs_version_dmu_backup_stream, 0, "DMU_BACKUP_STREAM_VERSION");
static int zfs_version_spa = SPA_VERSION;
SYSCTL_INT(_vfs_zfs_version, OID_AUTO, spa, CTLFLAG_RD, &zfs_version_spa, 0,
    "SPA_VERSION");
static int zfs_version_vdev_boot = VDEV_BOOT_VERSION;
SYSCTL_INT(_vfs_zfs_version, OID_AUTO, vdev_boot, CTLFLAG_RD,
    &zfs_version_vdev_boot, 0, "VDEV_BOOT_VERSION");
static int zfs_version_zpl = ZPL_VERSION;
SYSCTL_INT(_vfs_zfs_version, OID_AUTO, zpl, CTLFLAG_RD, &zfs_version_zpl, 0,
    "ZPL_VERSION");

static int zfs_mount(vfs_t *vfsp);
static int zfs_umount(vfs_t *vfsp, int fflag);
static int zfs_root(vfs_t *vfsp, int flags, vnode_t **vpp);
static int zfs_statfs(vfs_t *vfsp, struct statfs *statp);
static int zfs_vget(vfs_t *vfsp, ino_t ino, int flags, vnode_t **vpp);
static int zfs_sync(vfs_t *vfsp, int waitfor);
static int zfs_checkexp(vfs_t *vfsp, struct sockaddr *nam, int *extflagsp,
    struct ucred **credanonp, int *numsecflavors, int **secflavors);
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
	.vfs_checkexp =		zfs_checkexp,
	.vfs_fhtovp =		zfs_fhtovp,
};

VFS_SET(zfs_vfsops, zfs, VFCF_JAIL | VFCF_DELEGADMIN);

/*
 * We need to keep a count of active fs's.
 * This is necessary to prevent our module
 * from being unloaded after a umount -f
 */
static uint32_t	zfs_active_fs_count = 0;

/*ARGSUSED*/
static int
zfs_sync(vfs_t *vfsp, int waitfor)
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

		error = vfs_stdsync(vfsp, waitfor);
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

/*
 * The nbmand mount option can be changed at mount time.
 * We can't allow it to be toggled on live file systems or incorrect
 * behavior may be seen from cifs clients
 *
 * This property isn't registered via dsl_prop_register(), but this callback
 * will be called when a file system is first mounted
 */
static void
nbmand_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;
	if (newval == FALSE) {
		vfs_clearmntopt(zfsvfs->z_vfs, MNTOPT_NBMAND);
		vfs_setmntopt(zfsvfs->z_vfs, MNTOPT_NONBMAND, NULL, 0);
	} else {
		vfs_clearmntopt(zfsvfs->z_vfs, MNTOPT_NONBMAND);
		vfs_setmntopt(zfsvfs->z_vfs, MNTOPT_NBMAND, NULL, 0);
	}
}

static void
snapdir_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;

	zfsvfs->z_show_ctldir = newval;
}

static void
vscan_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;

	zfsvfs->z_vscan = newval;
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
zfs_register_callbacks(vfs_t *vfsp)
{
	struct dsl_dataset *ds = NULL;
	objset_t *os = NULL;
	zfsvfs_t *zfsvfs = NULL;
	uint64_t nbmand;
	int readonly, do_readonly = FALSE;
	int setuid, do_setuid = FALSE;
	int exec, do_exec = FALSE;
	int xattr, do_xattr = FALSE;
	int atime, do_atime = FALSE;
	int error = 0;

	ASSERT(vfsp);
	zfsvfs = vfsp->vfs_data;
	ASSERT(zfsvfs);
	os = zfsvfs->z_os;

	/*
	 * This function can be called for a snapshot when we update snapshot's
	 * mount point, which isn't really supported.
	 */
	if (dmu_objset_is_snapshot(os))
		return (EOPNOTSUPP);

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
	if (vfs_optionisset(vfsp, MNTOPT_NOATIME, NULL)) {
		atime = B_FALSE;
		do_atime = B_TRUE;
	} else if (vfs_optionisset(vfsp, MNTOPT_ATIME, NULL)) {
		atime = B_TRUE;
		do_atime = B_TRUE;
	}

	/*
	 * nbmand is a special property.  It can only be changed at
	 * mount time.
	 *
	 * This is weird, but it is documented to only be changeable
	 * at mount time.
	 */
	if (vfs_optionisset(vfsp, MNTOPT_NONBMAND, NULL)) {
		nbmand = B_FALSE;
	} else if (vfs_optionisset(vfsp, MNTOPT_NBMAND, NULL)) {
		nbmand = B_TRUE;
	} else {
		char osname[MAXNAMELEN];

		dmu_objset_name(os, osname);
		if (error = dsl_prop_get_integer(osname, "nbmand", &nbmand,
		    NULL)) {
			return (error);
		}
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
	error = error ? error : dsl_prop_register(ds,
	    "vscan", vscan_changed_cb, zfsvfs);
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
	if (do_atime)
		atime_changed_cb(zfsvfs, atime);

	nbmand_changed_cb(zfsvfs, nbmand);

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
	(void) dsl_prop_unregister(ds, "vscan", vscan_changed_cb, zfsvfs);
	return (error);

}

static int
zfsvfs_setup(zfsvfs_t *zfsvfs, boolean_t mounting)
{
	int error;

	error = zfs_register_callbacks(zfsvfs->z_vfs);
	if (error)
		return (error);

	/*
	 * Set the objset user_ptr to track its zfsvfs.
	 */
	mutex_enter(&zfsvfs->z_os->os->os_user_ptr_lock);
	dmu_objset_set_user(zfsvfs->z_os, zfsvfs);
	mutex_exit(&zfsvfs->z_os->os->os_user_ptr_lock);

	/*
	 * If we are not mounting (ie: online recv), then we don't
	 * have to worry about replaying the log as we blocked all
	 * operations out since we closed the ZIL.
	 */
	if (mounting) {
		boolean_t readonly;

		/*
		 * During replay we remove the read only flag to
		 * allow replays to succeed.
		 */
		readonly = zfsvfs->z_vfs->vfs_flag & VFS_RDONLY;
		zfsvfs->z_vfs->vfs_flag &= ~VFS_RDONLY;

		/*
		 * Parse and replay the intent log.
		 */
		zil_replay(zfsvfs->z_os, zfsvfs, &zfsvfs->z_assign,
		    zfs_replay_vector, zfs_unlinked_drain);

		zfs_unlinked_drain(zfsvfs);
		zfsvfs->z_vfs->vfs_flag |= readonly; /* restore readonly bit */
	}

	if (!zil_disable)
		zfsvfs->z_log = zil_open(zfsvfs->z_os, zfs_get_data);

	return (0);
}

static void
zfs_freezfsvfs(zfsvfs_t *zfsvfs)
{
	mutex_destroy(&zfsvfs->z_znodes_lock);
	mutex_destroy(&zfsvfs->z_online_recv_lock);
	list_destroy(&zfsvfs->z_all_znodes);
	rrw_destroy(&zfsvfs->z_teardown_lock);
	rw_destroy(&zfsvfs->z_teardown_inactive_lock);
	rw_destroy(&zfsvfs->z_fuid_lock);
	kmem_free(zfsvfs, sizeof (zfsvfs_t));
}

static int
zfs_domount(vfs_t *vfsp, char *osname)
{
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
	mutex_init(&zfsvfs->z_online_recv_lock, NULL, MUTEX_DEFAULT, NULL);
	list_create(&zfsvfs->z_all_znodes, sizeof (znode_t),
	    offsetof(znode_t, z_link_node));
	rrw_init(&zfsvfs->z_teardown_lock);
	rw_init(&zfsvfs->z_teardown_inactive_lock, NULL, RW_DEFAULT, NULL);
	rw_init(&zfsvfs->z_fuid_lock, NULL, RW_DEFAULT, NULL);

	if (error = dsl_prop_get_integer(osname, "recordsize", &recordsize,
	    NULL))
		goto out;
	zfsvfs->z_vfs->vfs_bsize = recordsize;

	vfsp->vfs_data = zfsvfs;
	vfsp->mnt_flag |= MNT_LOCAL;
	vfsp->mnt_kern_flag |= MNTK_MPSAFE;
	vfsp->mnt_kern_flag |= MNTK_LOOKUP_SHARED;
	vfsp->mnt_kern_flag |= MNTK_SHARED_WRITES;

	if (error = dsl_prop_get_integer(osname, "readonly", &readonly, NULL))
		goto out;

	mode = DS_MODE_OWNER;
	if (readonly)
		mode |= DS_MODE_READONLY;

	error = dmu_objset_open(osname, DMU_OST_ZFS, mode, &zfsvfs->z_os);
	if (error == EROFS) {
		mode = DS_MODE_OWNER | DS_MODE_READONLY;
		error = dmu_objset_open(osname, DMU_OST_ZFS, mode,
		    &zfsvfs->z_os);
	}

	if (error)
		goto out;

	if (error = zfs_init_fs(zfsvfs, &zp))
		goto out;

	/*
	 * Set features for file system.
	 */
	zfsvfs->z_use_fuids = USE_FUIDS(zfsvfs->z_version, zfsvfs->z_os);
	if (zfsvfs->z_use_fuids) {
		vfs_set_feature(vfsp, VFSFT_XVATTR);
		vfs_set_feature(vfsp, VFSFT_SYSATTR_VIEWS);
		vfs_set_feature(vfsp, VFSFT_ACEMASKONACCESS);
		vfs_set_feature(vfsp, VFSFT_ACLONCREATE);
	}
	if (zfsvfs->z_case == ZFS_CASE_INSENSITIVE) {
		vfs_set_feature(vfsp, VFSFT_DIRENTFLAGS);
		vfs_set_feature(vfsp, VFSFT_CASEINSENSITIVE);
		vfs_set_feature(vfsp, VFSFT_NOCASESENSITIVE);
	} else if (zfsvfs->z_case == ZFS_CASE_MIXED) {
		vfs_set_feature(vfsp, VFSFT_DIRENTFLAGS);
		vfs_set_feature(vfsp, VFSFT_CASEINSENSITIVE);
	}

	if (dmu_objset_is_snapshot(zfsvfs->z_os)) {
		uint64_t pval;

		ASSERT(mode & DS_MODE_READONLY);
		atime_changed_cb(zfsvfs, B_FALSE);
		readonly_changed_cb(zfsvfs, B_TRUE);
		if (error = dsl_prop_get_integer(osname, "xattr", &pval, NULL))
			goto out;
		xattr_changed_cb(zfsvfs, pval);
		zfsvfs->z_issnap = B_TRUE;
	} else {
		error = zfsvfs_setup(zfsvfs, B_TRUE);
	}

	vfs_mountedfrom(vfsp, osname);

	if (!zfsvfs->z_issnap)
		zfsctl_create(zfsvfs);
out:
	if (error) {
		if (zfsvfs->z_os)
			dmu_objset_close(zfsvfs->z_os);
		zfs_freezfsvfs(zfsvfs);
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

		VERIFY(dsl_prop_unregister(ds, "vscan",
		    vscan_changed_cb, zfsvfs) == 0);
	}
}

/*ARGSUSED*/
static int
zfs_mount(vfs_t *vfsp)
{
	kthread_t	*td = curthread;
	vnode_t		*mvp = vfsp->mnt_vnodecovered;
	cred_t		*cr = td->td_ucred;
	char		*osname;
	int		error = 0;
	int		canwrite;

	if (vfs_getopt(vfsp->mnt_optnew, "from", (void **)&osname, NULL))
		return (EINVAL);

	/*
	 * If full-owner-access is enabled and delegated administration is
	 * turned on, we must set nosuid.
	 */
	if (zfs_super_owner &&
	    dsl_deleg_access(osname, ZFS_DELEG_PERM_MOUNT, cr) != ECANCELED) {
		secpolicy_fs_mount_clearopts(cr, vfsp);
	}

	/*
	 * Check for mount privilege?
	 *
	 * If we don't have privilege then see if
	 * we have local permission to allow it
	 */
	error = secpolicy_fs_mount(cr, mvp, vfsp);
	if (error) {
		error = dsl_deleg_access(osname, ZFS_DELEG_PERM_MOUNT, cr);
		if (error != 0)
			goto out;

		if (!(vfsp->vfs_flag & MS_REMOUNT)) {
			vattr_t		vattr;

			/*
			 * Make sure user is the owner of the mount point
			 * or has sufficient privileges.
			 */

			vattr.va_mask = AT_UID;

			vn_lock(mvp, LK_SHARED | LK_RETRY);
			if (error = VOP_GETATTR(mvp, &vattr, cr)) {
				VOP_UNLOCK(mvp, 0);
				goto out;
			}

#if 0 /* CHECK THIS! Is probably needed for zfs_suser. */
			if (secpolicy_vnode_owner(mvp, cr, vattr.va_uid) != 0 &&
			    VOP_ACCESS(mvp, VWRITE, cr, td) != 0) {
				error = EPERM;
				goto out;
			}
#else
			if (error = secpolicy_vnode_owner(mvp, cr, vattr.va_uid)) {
				VOP_UNLOCK(mvp, 0);
				goto out;
			}

			if (error = VOP_ACCESS(mvp, VWRITE, cr, td)) {
				VOP_UNLOCK(mvp, 0);
				goto out;
			}
			VOP_UNLOCK(mvp, 0);
#endif
		}

		secpolicy_fs_mount_clearopts(cr, vfsp);
	}

	/*
	 * Refuse to mount a filesystem if we are in a local zone and the
	 * dataset is not visible.
	 */
	if (!INGLOBALZONE(curthread) &&
	    (!zone_dataset_visible(osname, &canwrite) || !canwrite)) {
		error = EPERM;
		goto out;
	}

	/*
	 * When doing a remount, we simply refresh our temporary properties
	 * according to those options set in the current VFS options.
	 */
	if (vfsp->vfs_flag & MS_REMOUNT) {
		/* refresh mount options */
		zfs_unregister_callbacks(vfsp->vfs_data);
		error = zfs_register_callbacks(vfsp);
		goto out;
	}

	DROP_GIANT();
	error = zfs_domount(vfsp, osname);
	PICKUP_GIANT();
out:
	return (error);
}

static int
zfs_statfs(vfs_t *vfsp, struct statfs *statp)
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
zfs_root(vfs_t *vfsp, int flags, vnode_t **vpp)
{
	zfsvfs_t *zfsvfs = vfsp->vfs_data;
	znode_t *rootzp;
	int error;

	ZFS_ENTER(zfsvfs);

	error = zfs_zget(zfsvfs, zfsvfs->z_root, &rootzp);
	if (error == 0) {
		*vpp = ZTOV(rootzp);
		error = vn_lock(*vpp, flags);
		(*vpp)->v_vflag |= VV_ROOT;
	}

	ZFS_EXIT(zfsvfs);
	return (error);
}

/*
 * Teardown the zfsvfs::z_os.
 *
 * Note, if 'unmounting' if FALSE, we return with the 'z_teardown_lock'
 * and 'z_teardown_inactive_lock' held.
 */
static int
zfsvfs_teardown(zfsvfs_t *zfsvfs, boolean_t unmounting)
{
	znode_t	*zp;

	rrw_enter(&zfsvfs->z_teardown_lock, RW_WRITER, FTAG);

	if (!unmounting) {
		/*
		 * We purge the parent filesystem's vfsp as the parent
		 * filesystem and all of its snapshots have their vnode's
		 * v_vfsp set to the parent's filesystem's vfsp.  Note,
		 * 'z_parent' is self referential for non-snapshots.
		 */
		(void) dnlc_purge_vfsp(zfsvfs->z_parent->z_vfs, 0);
	}

	/*
	 * Close the zil. NB: Can't close the zil while zfs_inactive
	 * threads are blocked as zil_close can call zfs_inactive.
	 */
	if (zfsvfs->z_log) {
		zil_close(zfsvfs->z_log);
		zfsvfs->z_log = NULL;
	}

	rw_enter(&zfsvfs->z_teardown_inactive_lock, RW_WRITER);

	/*
	 * If we are not unmounting (ie: online recv) and someone already
	 * unmounted this file system while we were doing the switcheroo,
	 * or a reopen of z_os failed then just bail out now.
	 */
	if (!unmounting && (zfsvfs->z_unmounted || zfsvfs->z_os == NULL)) {
		rw_exit(&zfsvfs->z_teardown_inactive_lock);
		rrw_exit(&zfsvfs->z_teardown_lock, FTAG);
		return (EIO);
	}

	/*
	 * At this point there are no vops active, and any new vops will
	 * fail with EIO since we have z_teardown_lock for writer (only
	 * relavent for forced unmount).
	 *
	 * Release all holds on dbufs.
	 */
	mutex_enter(&zfsvfs->z_znodes_lock);
	for (zp = list_head(&zfsvfs->z_all_znodes); zp != NULL;
	    zp = list_next(&zfsvfs->z_all_znodes, zp))
		if (zp->z_dbuf) {
			ASSERT(ZTOV(zp)->v_count >= 0);
			zfs_znode_dmu_fini(zp);
		}
	mutex_exit(&zfsvfs->z_znodes_lock);

	/*
	 * If we are unmounting, set the unmounted flag and let new vops
	 * unblock.  zfs_inactive will have the unmounted behavior, and all
	 * other vops will fail with EIO.
	 */
	if (unmounting) {
		zfsvfs->z_unmounted = B_TRUE;
		rrw_exit(&zfsvfs->z_teardown_lock, FTAG);
		rw_exit(&zfsvfs->z_teardown_inactive_lock);

#ifdef __FreeBSD__
		/*
		 * Some znodes might not be fully reclaimed, wait for them.
		 */
		mutex_enter(&zfsvfs->z_znodes_lock);
		while (list_head(&zfsvfs->z_all_znodes) != NULL) {
			msleep(zfsvfs, &zfsvfs->z_znodes_lock, 0,
			    "zteardown", 0);
		}
		mutex_exit(&zfsvfs->z_znodes_lock);
#endif
	}

	/*
	 * z_os will be NULL if there was an error in attempting to reopen
	 * zfsvfs, so just return as the properties had already been
	 * unregistered and cached data had been evicted before.
	 */
	if (zfsvfs->z_os == NULL)
		return (0);

	/*
	 * Unregister properties.
	 */
	zfs_unregister_callbacks(zfsvfs);

	/*
	 * Evict cached data
	 */
	if (dmu_objset_evict_dbufs(zfsvfs->z_os)) {
		txg_wait_synced(dmu_objset_pool(zfsvfs->z_os), 0);
		(void) dmu_objset_evict_dbufs(zfsvfs->z_os);
	}

	return (0);
}

/*ARGSUSED*/
static int
zfs_umount(vfs_t *vfsp, int fflag)
{
	zfsvfs_t *zfsvfs = vfsp->vfs_data;
	objset_t *os;
	cred_t *cr = curthread->td_ucred;
	int ret;

	if (fflag & MS_FORCE) {
		/* TODO: Force unmount is not well implemented yet, so deny it. */
		ZFS_LOG(0, "Force unmount is experimental - report any problems.");
	}

	ret = secpolicy_fs_unmount(cr, vfsp);
	if (ret) {
		ret = dsl_deleg_access((char *)refstr_value(vfsp->vfs_resource),
		    ZFS_DELEG_PERM_MOUNT, cr);
		if (ret)
			return (ret);
	}
	/*
	 * We purge the parent filesystem's vfsp as the parent filesystem
	 * and all of its snapshots have their vnode's v_vfsp set to the
	 * parent's filesystem's vfsp.  Note, 'z_parent' is self
	 * referential for non-snapshots.
	 */
	(void) dnlc_purge_vfsp(zfsvfs->z_parent->z_vfs, 0);

	/*
	 * Unmount any snapshots mounted under .zfs before unmounting the
	 * dataset itself.
	 */
	if (zfsvfs->z_ctldir != NULL) {
		if ((ret = zfsctl_umount_snapshots(vfsp, fflag, cr)) != 0)
			return (ret);
		ret = vflush(vfsp, 0, 0, curthread);
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
	ret = vflush(vfsp, 1, (fflag & MS_FORCE) ? FORCECLOSE : 0, curthread);
	if (ret != 0) {
		if (!zfsvfs->z_issnap) {
			zfsctl_create(zfsvfs);
			ASSERT(zfsvfs->z_ctldir != NULL);
		}
		return (ret);
	}

	if (!(fflag & MS_FORCE)) {
		/*
		 * Check the number of active vnodes in the file system.
		 * Our count is maintained in the vfs structure, but the
		 * number is off by 1 to indicate a hold on the vfs
		 * structure itself.
		 *
		 * The '.zfs' directory maintains a reference of its
		 * own, and any active references underneath are
		 * reflected in the vnode count.
		 */
		if (zfsvfs->z_ctldir == NULL) {
			if (vfsp->vfs_count > 1)
				return (EBUSY);
		} else {
			if (vfsp->vfs_count > 2 ||
			    zfsvfs->z_ctldir->v_count > 1)
				return (EBUSY);
		}
	} else {
		MNT_ILOCK(vfsp);
		vfsp->mnt_kern_flag |= MNTK_UNMOUNTF;
		MNT_IUNLOCK(vfsp);
	}

	VERIFY(zfsvfs_teardown(zfsvfs, B_TRUE) == 0);
	os = zfsvfs->z_os;

	/*
	 * z_os will be NULL if there was an error in
	 * attempting to reopen zfsvfs.
	 */
	if (os != NULL) {
		/*
		 * Unset the objset user_ptr.
		 */
		mutex_enter(&os->os->os_user_ptr_lock);
		dmu_objset_set_user(os, NULL);
		mutex_exit(&os->os->os_user_ptr_lock);

		/*
		 * Finally release the objset
		 */
		dmu_objset_close(os);
	}

	/*
	 * We can now safely destroy the '.zfs' directory node.
	 */
	if (zfsvfs->z_ctldir != NULL)
		zfsctl_destroy(zfsvfs);
	if (zfsvfs->z_issnap) {
		vnode_t *svp = vfsp->mnt_vnodecovered;

		ASSERT(svp->v_count == 2 || svp->v_count == 1);
		if (svp->v_count == 2)
			VN_RELE(svp);
	}
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
		vn_lock(*vpp, flags);
	}
	ZFS_EXIT(zfsvfs);
	return (err);
}

static int
zfs_checkexp(vfs_t *vfsp, struct sockaddr *nam, int *extflagsp,
    struct ucred **credanonp, int *numsecflavors, int **secflavors)
{
	zfsvfs_t *zfsvfs = vfsp->vfs_data;

	/*
	 * If this is regular file system vfsp is the same as
	 * zfsvfs->z_parent->z_vfs, but if it is snapshot,
	 * zfsvfs->z_parent->z_vfs represents parent file system
	 * which we have to use here, because only this file system
	 * has mnt_export configured.
	 */
	vfsp = zfsvfs->z_parent->z_vfs;

	return (vfs_stdcheckexp(zfsvfs->z_parent->z_vfs, nam, extflagsp,
	    credanonp, numsecflavors, secflavors));
}


static int
zfs_fhtovp(vfs_t *vfsp, fid_t *fidp, vnode_t **vpp)
{
	zfsvfs_t	*zfsvfs = vfsp->vfs_data;
	znode_t		*zp;
	uint64_t	object = 0;
	uint64_t	fid_gen = 0;
	uint64_t	gen_mask;
	uint64_t	zp_gen;
	int		i, err;

	*vpp = NULL;

	ZFS_ENTER(zfsvfs);

	/*
	 * On FreeBSD we are already called with snapshot's mount point
	 * and not the mount point of its parent.
	 */
#ifndef __FreeBSD__
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
#endif

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
			    0, NULL, NULL, NULL, NULL, NULL) == 0);
		} else {
			VN_HOLD(*vpp);
		}
		vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY);
		ZFS_EXIT(zfsvfs);
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
	vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY);
	vnode_create_vobject(*vpp, zp->z_phys->zp_size, curthread);
	ZFS_EXIT(zfsvfs);
	return (0);
}

/*
 * Block out VOPs and close zfsvfs_t::z_os
 *
 * Note, if successful, then we return with the 'z_teardown_lock' and
 * 'z_teardown_inactive_lock' write held.
 */
int
zfs_suspend_fs(zfsvfs_t *zfsvfs, char *name, int *mode)
{
	int error;

	if ((error = zfsvfs_teardown(zfsvfs, B_FALSE)) != 0)
		return (error);

	*mode = zfsvfs->z_os->os_mode;
	dmu_objset_name(zfsvfs->z_os, name);
	dmu_objset_close(zfsvfs->z_os);

	return (0);
}

/*
 * Reopen zfsvfs_t::z_os and release VOPs.
 */
int
zfs_resume_fs(zfsvfs_t *zfsvfs, const char *osname, int mode)
{
	int err;

	ASSERT(RRW_WRITE_HELD(&zfsvfs->z_teardown_lock));
	ASSERT(RW_WRITE_HELD(&zfsvfs->z_teardown_inactive_lock));

	err = dmu_objset_open(osname, DMU_OST_ZFS, mode, &zfsvfs->z_os);
	if (err) {
		zfsvfs->z_os = NULL;
	} else {
		znode_t *zp;

		VERIFY(zfsvfs_setup(zfsvfs, B_FALSE) == 0);

		/*
		 * Attempt to re-establish all the active znodes with
		 * their dbufs.  If a zfs_rezget() fails, then we'll let
		 * any potential callers discover that via ZFS_ENTER_VERIFY_VP
		 * when they try to use their znode.
		 */
		mutex_enter(&zfsvfs->z_znodes_lock);
		for (zp = list_head(&zfsvfs->z_all_znodes); zp;
		    zp = list_next(&zfsvfs->z_all_znodes, zp)) {
			(void) zfs_rezget(zp);
		}
		mutex_exit(&zfsvfs->z_znodes_lock);

	}

	/* release the VOPs */
	rw_exit(&zfsvfs->z_teardown_inactive_lock);
	rrw_exit(&zfsvfs->z_teardown_lock, FTAG);

	if (err) {
		/*
		 * Since we couldn't reopen zfsvfs::z_os, force
		 * unmount this file system.
		 */
		if (vn_vfswlock(zfsvfs->z_vfs->vfs_vnodecovered) == 0)
			(void) dounmount(zfsvfs->z_vfs, MS_FORCE, curthread);
	}
	return (err);
}

static void
zfs_freevfs(vfs_t *vfsp)
{
	zfsvfs_t *zfsvfs = vfsp->vfs_data;
	int i;

	for (i = 0; i != ZFS_OBJ_MTX_SZ; i++)
		mutex_destroy(&zfsvfs->z_hold_mtx[i]);

	zfs_fuid_destroy(zfsvfs);
	zfs_freezfsvfs(zfsvfs);

	atomic_add_32(&zfs_active_fs_count, -1);
}

#ifdef __i386__
static int desiredvnodes_backup;
#endif

static void
zfs_vnodes_adjust(void)
{
#ifdef __i386__
	int newdesiredvnodes;

	desiredvnodes_backup = desiredvnodes;

	/*
	 * We calculate newdesiredvnodes the same way it is done in
	 * vntblinit(). If it is equal to desiredvnodes, it means that
	 * it wasn't tuned by the administrator and we can tune it down.
	 */
	newdesiredvnodes = min(maxproc + cnt.v_page_count / 4, 2 *
	    vm_kmem_size / (5 * (sizeof(struct vm_object) +
	    sizeof(struct vnode))));
	if (newdesiredvnodes == desiredvnodes)
		desiredvnodes = (3 * newdesiredvnodes) / 4;
#endif
}

static void
zfs_vnodes_adjust_back(void)
{

#ifdef __i386__
	desiredvnodes = desiredvnodes_backup;
#endif
}

void
zfs_init(void)
{

	printf("ZFS filesystem version " SPA_VERSION_STRING "\n");

	/*
	 * Initialize znode cache, vnode ops, etc...
	 */
	zfs_znode_init();

	/*
	 * Initialize .zfs directory structures
	 */
	zfsctl_init();

	/*
	 * Reduce number of vnode. Originally number of vnodes is calculated
	 * with UFS inode in mind. We reduce it here, because it's too big for
	 * ZFS/i386.
	 */
	zfs_vnodes_adjust();
}

void
zfs_fini(void)
{
	zfsctl_fini();
	zfs_znode_fini();
	zfs_vnodes_adjust_back();
}

int
zfs_busy(void)
{
	return (zfs_active_fs_count != 0);
}

int
zfs_set_version(const char *name, uint64_t newvers)
{
	int error;
	objset_t *os;
	dmu_tx_t *tx;
	uint64_t curvers;

	/*
	 * XXX for now, require that the filesystem be unmounted.  Would
	 * be nice to find the zfsvfs_t and just update that if
	 * possible.
	 */

	if (newvers < ZPL_VERSION_INITIAL || newvers > ZPL_VERSION)
		return (EINVAL);

	error = dmu_objset_open(name, DMU_OST_ZFS, DS_MODE_OWNER, &os);
	if (error)
		return (error);

	error = zap_lookup(os, MASTER_NODE_OBJ, ZPL_VERSION_STR,
	    8, 1, &curvers);
	if (error)
		goto out;
	if (newvers < curvers) {
		error = EINVAL;
		goto out;
	}

	tx = dmu_tx_create(os);
	dmu_tx_hold_zap(tx, MASTER_NODE_OBJ, 0, ZPL_VERSION_STR);
	error = dmu_tx_assign(tx, TXG_WAIT);
	if (error) {
		dmu_tx_abort(tx);
		goto out;
	}
	error = zap_update(os, MASTER_NODE_OBJ, ZPL_VERSION_STR, 8, 1,
	    &newvers, tx);

	spa_history_internal_log(LOG_DS_UPGRADE,
	    dmu_objset_spa(os), tx, CRED(),
	    "oldver=%llu newver=%llu dataset = %llu", curvers, newvers,
	    dmu_objset_id(os));
	dmu_tx_commit(tx);

out:
	dmu_objset_close(os);
	return (error);
}
/*
 * Read a property stored within the master node.
 */
int
zfs_get_zplprop(objset_t *os, zfs_prop_t prop, uint64_t *value)
{
	const char *pname;
	int error = ENOENT;

	/*
	 * Look up the file system's value for the property.  For the
	 * version property, we look up a slightly different string.
	 */
	if (prop == ZFS_PROP_VERSION)
		pname = ZPL_VERSION_STR;
	else
		pname = zfs_prop_to_name(prop);

	if (os != NULL)
		error = zap_lookup(os, MASTER_NODE_OBJ, pname, 8, 1, value);

	if (error == ENOENT) {
		/* No value set, use the default value */
		switch (prop) {
		case ZFS_PROP_VERSION:
			*value = ZPL_VERSION;
			break;
		case ZFS_PROP_NORMALIZE:
		case ZFS_PROP_UTF8ONLY:
			*value = 0;
			break;
		case ZFS_PROP_CASE:
			*value = ZFS_CASE_SENSITIVE;
			break;
		default:
			return (error);
		}
		error = 0;
	}
	return (error);
}
