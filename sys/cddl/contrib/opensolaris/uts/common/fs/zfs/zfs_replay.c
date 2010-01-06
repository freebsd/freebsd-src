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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/vfs.h>
#include <sys/fs/zfs.h>
#include <sys/zfs_znode.h>
#include <sys/zfs_dir.h>
#include <sys/zfs_acl.h>
#include <sys/spa.h>
#include <sys/zil.h>
#include <sys/byteorder.h>
#include <sys/stat.h>
#include <sys/acl.h>
#include <sys/atomic.h>
#include <sys/cred.h>
#include <sys/namei.h>

/*
 * Functions to replay ZFS intent log (ZIL) records
 * The functions are called through a function vector (zfs_replay_vector)
 * which is indexed by the transaction type.
 */

static void
zfs_init_vattr(vattr_t *vap, uint64_t mask, uint64_t mode,
	uint64_t uid, uint64_t gid, uint64_t rdev, uint64_t nodeid)
{
	VATTR_NULL(vap);
	vap->va_mask = (uint_t)mask;
	if (mask & AT_TYPE)
		vap->va_type = IFTOVT(mode);
	if (mask & AT_MODE)
		vap->va_mode = mode & MODEMASK;
	if (mask & AT_UID)
		vap->va_uid = (uid_t)uid;
	if (mask & AT_GID)
		vap->va_gid = (gid_t)gid;
	vap->va_rdev = zfs_cmpldev(rdev);
	vap->va_nodeid = nodeid;
}

/* ARGSUSED */
static int
zfs_replay_error(zfsvfs_t *zfsvfs, lr_t *lr, boolean_t byteswap)
{
	return (ENOTSUP);
}

static int
zfs_replay_create(zfsvfs_t *zfsvfs, lr_create_t *lr, boolean_t byteswap)
{
	char *name = (char *)(lr + 1);	/* name follows lr_create_t */
	char *link;			/* symlink content follows name */
	znode_t *dzp;
	vnode_t *vp = NULL;
	vattr_t va;
	struct componentname cn;
	int error;

	if (byteswap)
		byteswap_uint64_array(lr, sizeof (*lr));

	if ((error = zfs_zget(zfsvfs, lr->lr_doid, &dzp)) != 0)
		return (error);

	zfs_init_vattr(&va, AT_TYPE | AT_MODE | AT_UID | AT_GID,
	    lr->lr_mode, lr->lr_uid, lr->lr_gid, lr->lr_rdev, lr->lr_foid);

	/*
	 * All forms of zfs create (create, mkdir, mkxattrdir, symlink)
	 * eventually end up in zfs_mknode(), which assigns the object's
	 * creation time and generation number.  The generic VOP_CREATE()
	 * doesn't have either concept, so we smuggle the values inside
	 * the vattr's otherwise unused va_ctime and va_nblocks fields.
	 */
	ZFS_TIME_DECODE(&va.va_ctime, lr->lr_crtime);
	va.va_nblocks = lr->lr_gen;

	cn.cn_nameptr = name;
	cn.cn_cred = kcred;
	cn.cn_thread = curthread;
	cn.cn_flags = SAVENAME;

	vn_lock(ZTOV(dzp), LK_EXCLUSIVE | LK_RETRY, curthread);
	switch ((int)lr->lr_common.lrc_txtype) {
	case TX_CREATE:
		error = VOP_CREATE(ZTOV(dzp), &vp, &cn, &va);
		break;
	case TX_MKDIR:
		error = VOP_MKDIR(ZTOV(dzp), &vp, &cn, &va);
		break;
	case TX_MKXATTR:
		error = zfs_make_xattrdir(dzp, &va, &vp, kcred);
		break;
	case TX_SYMLINK:
		link = name + strlen(name) + 1;
		error = VOP_SYMLINK(ZTOV(dzp), &vp, &cn, &va, link);
		break;
	default:
		error = ENOTSUP;
	}
	VOP_UNLOCK(ZTOV(dzp), 0, curthread);

	if (error == 0 && vp != NULL) {
		VOP_UNLOCK(vp, 0, curthread);
		VN_RELE(vp);
	}

	VN_RELE(ZTOV(dzp));

	return (error);
}

static int
zfs_replay_remove(zfsvfs_t *zfsvfs, lr_remove_t *lr, boolean_t byteswap)
{
	char *name = (char *)(lr + 1);	/* name follows lr_remove_t */
	znode_t *dzp;
	struct componentname cn;
	vnode_t *vp;
	int error;

	if (byteswap)
		byteswap_uint64_array(lr, sizeof (*lr));

	if ((error = zfs_zget(zfsvfs, lr->lr_doid, &dzp)) != 0)
		return (error);

	bzero(&cn, sizeof(cn));
	cn.cn_nameptr = name;
	cn.cn_namelen = strlen(name);
	cn.cn_nameiop = DELETE;
	cn.cn_flags = ISLASTCN | SAVENAME;
	cn.cn_lkflags = LK_EXCLUSIVE | LK_RETRY;
	cn.cn_cred = kcred;
	cn.cn_thread = curthread;
	vn_lock(ZTOV(dzp), LK_EXCLUSIVE | LK_RETRY, curthread);
	error = VOP_LOOKUP(ZTOV(dzp), &vp, &cn);
	if (error != 0) {
		VOP_UNLOCK(ZTOV(dzp), 0, curthread);
		goto fail;
	}

	switch ((int)lr->lr_common.lrc_txtype) {
	case TX_REMOVE:
		error = VOP_REMOVE(ZTOV(dzp), vp, &cn);
		break;
	case TX_RMDIR:
		error = VOP_RMDIR(ZTOV(dzp), vp, &cn);
		break;
	default:
		error = ENOTSUP;
	}
	vput(vp);
	VOP_UNLOCK(ZTOV(dzp), 0, curthread);
fail:
	VN_RELE(ZTOV(dzp));

	return (error);
}

static int
zfs_replay_link(zfsvfs_t *zfsvfs, lr_link_t *lr, boolean_t byteswap)
{
	char *name = (char *)(lr + 1);	/* name follows lr_link_t */
	znode_t *dzp, *zp;
	struct componentname cn;
	int error;

	if (byteswap)
		byteswap_uint64_array(lr, sizeof (*lr));

	if ((error = zfs_zget(zfsvfs, lr->lr_doid, &dzp)) != 0)
		return (error);

	if ((error = zfs_zget(zfsvfs, lr->lr_link_obj, &zp)) != 0) {
		VN_RELE(ZTOV(dzp));
		return (error);
	}

	cn.cn_nameptr = name;
	cn.cn_cred = kcred;
	cn.cn_thread = curthread;
	cn.cn_flags = SAVENAME;

	vn_lock(ZTOV(dzp), LK_EXCLUSIVE | LK_RETRY, curthread);
	vn_lock(ZTOV(zp), LK_EXCLUSIVE | LK_RETRY, curthread);
	error = VOP_LINK(ZTOV(dzp), ZTOV(zp), &cn);
	VOP_UNLOCK(ZTOV(zp), 0, curthread);
	VOP_UNLOCK(ZTOV(dzp), 0, curthread);

	VN_RELE(ZTOV(zp));
	VN_RELE(ZTOV(dzp));

	return (error);
}

static int
zfs_replay_rename(zfsvfs_t *zfsvfs, lr_rename_t *lr, boolean_t byteswap)
{
	char *sname = (char *)(lr + 1);	/* sname and tname follow lr_rename_t */
	char *tname = sname + strlen(sname) + 1;
	znode_t *sdzp, *tdzp;
	struct componentname scn, tcn;
	vnode_t *svp, *tvp;
	kthread_t *td = curthread;
	int error;

	if (byteswap)
		byteswap_uint64_array(lr, sizeof (*lr));

	if ((error = zfs_zget(zfsvfs, lr->lr_sdoid, &sdzp)) != 0)
		return (error);

	if ((error = zfs_zget(zfsvfs, lr->lr_tdoid, &tdzp)) != 0) {
		VN_RELE(ZTOV(sdzp));
		return (error);
	}

	svp = tvp = NULL;

	bzero(&scn, sizeof(scn));
	scn.cn_nameptr = sname;
	scn.cn_namelen = strlen(sname);
	scn.cn_nameiop = DELETE;
	scn.cn_flags = ISLASTCN | SAVENAME;
	scn.cn_lkflags = LK_EXCLUSIVE | LK_RETRY;
	scn.cn_cred = kcred;
	scn.cn_thread = td;
	vn_lock(ZTOV(sdzp), LK_EXCLUSIVE | LK_RETRY, td);
	error = VOP_LOOKUP(ZTOV(sdzp), &svp, &scn);
	VOP_UNLOCK(ZTOV(sdzp), 0, td);
	if (error != 0)
		goto fail;
	VOP_UNLOCK(svp, 0, td);

	bzero(&tcn, sizeof(tcn));
	tcn.cn_nameptr = tname;
	tcn.cn_namelen = strlen(tname);
	tcn.cn_nameiop = RENAME;
	tcn.cn_flags = ISLASTCN | SAVENAME;
	tcn.cn_lkflags = LK_EXCLUSIVE | LK_RETRY;
	tcn.cn_cred = kcred;
	tcn.cn_thread = td;
	vn_lock(ZTOV(tdzp), LK_EXCLUSIVE | LK_RETRY, td);
	error = VOP_LOOKUP(ZTOV(tdzp), &tvp, &tcn);
	if (error == EJUSTRETURN)
		tvp = NULL;
	else if (error != 0) {
		VOP_UNLOCK(ZTOV(tdzp), 0, td);
		goto fail;
	}

	error = VOP_RENAME(ZTOV(sdzp), svp, &scn, ZTOV(tdzp), tvp, &tcn);
	return (error);
fail:
	if (svp != NULL)
		vrele(svp);
	if (tvp != NULL)
		vrele(tvp);
	VN_RELE(ZTOV(tdzp));
	VN_RELE(ZTOV(sdzp));

	return (error);
}

static int
zfs_replay_write(zfsvfs_t *zfsvfs, lr_write_t *lr, boolean_t byteswap)
{
	char *data = (char *)(lr + 1);	/* data follows lr_write_t */
	znode_t	*zp;
	int error;
	ssize_t resid;

	if (byteswap)
		byteswap_uint64_array(lr, sizeof (*lr));

	if ((error = zfs_zget(zfsvfs, lr->lr_foid, &zp)) != 0) {
		/*
		 * As we can log writes out of order, it's possible the
		 * file has been removed. In this case just drop the write
		 * and return success.
		 */
		if (error == ENOENT)
			error = 0;
		return (error);
	}

	error = vn_rdwr(UIO_WRITE, ZTOV(zp), data, lr->lr_length,
	    lr->lr_offset, UIO_SYSSPACE, 0, RLIM64_INFINITY, kcred, &resid);

	VN_RELE(ZTOV(zp));

	return (error);
}

static int
zfs_replay_truncate(zfsvfs_t *zfsvfs, lr_truncate_t *lr, boolean_t byteswap)
{

	ZFS_LOG(0, "Unexpected code path, report to pjd@FreeBSD.org");
	return (EOPNOTSUPP);
}

static int
zfs_replay_setattr(zfsvfs_t *zfsvfs, lr_setattr_t *lr, boolean_t byteswap)
{
	znode_t *zp;
	vattr_t va;
	vnode_t *vp;
	int error;

	if (byteswap)
		byteswap_uint64_array(lr, sizeof (*lr));

	if ((error = zfs_zget(zfsvfs, lr->lr_foid, &zp)) != 0) {
		/*
		 * As we can log setattrs out of order, it's possible the
		 * file has been removed. In this case just drop the setattr
		 * and return success.
		 */
		if (error == ENOENT)
			error = 0;
		return (error);
	}

	zfs_init_vattr(&va, lr->lr_mask, lr->lr_mode,
	    lr->lr_uid, lr->lr_gid, 0, lr->lr_foid);

	va.va_size = lr->lr_size;
	ZFS_TIME_DECODE(&va.va_atime, lr->lr_atime);
	ZFS_TIME_DECODE(&va.va_mtime, lr->lr_mtime);

	vp = ZTOV(zp);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, curthread);
	error = VOP_SETATTR(vp, &va, kcred, curthread);
	VOP_UNLOCK(vp, 0, curthread);
	VN_RELE(vp);

	return (error);
}

static int
zfs_replay_acl(zfsvfs_t *zfsvfs, lr_acl_t *lr, boolean_t byteswap)
{
	ace_t *ace = (ace_t *)(lr + 1);	/* ace array follows lr_acl_t */
#ifdef TODO
	vsecattr_t vsa;
#endif
	znode_t *zp;
	int error;

	if (byteswap) {
		byteswap_uint64_array(lr, sizeof (*lr));
		zfs_ace_byteswap(ace, lr->lr_aclcnt);
	}

	if ((error = zfs_zget(zfsvfs, lr->lr_foid, &zp)) != 0) {
		/*
		 * As we can log acls out of order, it's possible the
		 * file has been removed. In this case just drop the acl
		 * and return success.
		 */
		if (error == ENOENT)
			error = 0;
		return (error);
	}

#ifdef TODO
	bzero(&vsa, sizeof (vsa));
	vsa.vsa_mask = VSA_ACE | VSA_ACECNT;
	vsa.vsa_aclcnt = lr->lr_aclcnt;
	vsa.vsa_aclentp = ace;

	error = VOP_SETSECATTR(ZTOV(zp), &vsa, 0, kcred);
#else
	error = EOPNOTSUPP;
#endif

	VN_RELE(ZTOV(zp));

	return (error);
}

/*
 * Callback vectors for replaying records
 */
zil_replay_func_t *zfs_replay_vector[TX_MAX_TYPE] = {
	zfs_replay_error,	/* 0 no such transaction type */
	zfs_replay_create,	/* TX_CREATE */
	zfs_replay_create,	/* TX_MKDIR */
	zfs_replay_create,	/* TX_MKXATTR */
	zfs_replay_create,	/* TX_SYMLINK */
	zfs_replay_remove,	/* TX_REMOVE */
	zfs_replay_remove,	/* TX_RMDIR */
	zfs_replay_link,	/* TX_LINK */
	zfs_replay_rename,	/* TX_RENAME */
	zfs_replay_write,	/* TX_WRITE */
	zfs_replay_truncate,	/* TX_TRUNCATE */
	zfs_replay_setattr,	/* TX_SETATTR */
	zfs_replay_acl,		/* TX_ACL */
};
