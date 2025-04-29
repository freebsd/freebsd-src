/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1989, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include "opt_quota.h"
#include "opt_suiddir.h"
#include "opt_ufs.h"
#include "opt_ffs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/stat.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/priv.h>
#include <sys/refcount.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/lockf.h>
#include <sys/conf.h>
#include <sys/acl.h>
#include <sys/smr.h>

#include <security/audit/audit.h>
#include <security/mac/mac_framework.h>

#include <sys/file.h>		/* XXX */

#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <ufs/ufs/acl.h>
#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>
#ifdef UFS_DIRHASH
#include <ufs/ufs/dirhash.h>
#endif
#ifdef UFS_GJOURNAL
#include <ufs/ufs/gjournal.h>
FEATURE(ufs_gjournal, "Journaling support through GEOM for UFS");
#endif

#ifdef QUOTA
FEATURE(ufs_quota, "UFS disk quotas support");
FEATURE(ufs_quota64, "64bit UFS disk quotas support");
#endif

#ifdef SUIDDIR
FEATURE(suiddir,
    "Give all new files in directory the same ownership as the directory");
#endif

VFS_SMR_DECLARE;

#include <ufs/ffs/ffs_extern.h>

static vop_accessx_t	ufs_accessx;
vop_fplookup_vexec_t ufs_fplookup_vexec;
static int ufs_chmod(struct vnode *, int, struct ucred *, struct thread *);
static int ufs_chown(struct vnode *, uid_t, gid_t, struct ucred *,
    struct thread *);
static vop_close_t	ufs_close;
static vop_create_t	ufs_create;
static vop_stat_t	ufs_stat;
static vop_getattr_t	ufs_getattr;
static vop_ioctl_t	ufs_ioctl;
static vop_link_t	ufs_link;
static int ufs_makeinode(int mode, struct vnode *, struct vnode **,
    struct componentname *, const char *);
static vop_mmapped_t	ufs_mmapped;
static vop_mkdir_t	ufs_mkdir;
static vop_mknod_t	ufs_mknod;
static vop_open_t	ufs_open;
static vop_pathconf_t	ufs_pathconf;
static vop_print_t	ufs_print;
static vop_readlink_t	ufs_readlink;
static vop_remove_t	ufs_remove;
static vop_rename_t	ufs_rename;
static vop_rmdir_t	ufs_rmdir;
static vop_setattr_t	ufs_setattr;
static vop_strategy_t	ufs_strategy;
static vop_symlink_t	ufs_symlink;
static vop_whiteout_t	ufs_whiteout;
static vop_close_t	ufsfifo_close;

SYSCTL_NODE(_vfs, OID_AUTO, ufs, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "UFS filesystem");

/*
 * A virgin directory (no blushing please).
 */
static struct dirtemplate mastertemplate = {
	0, 12, DT_DIR, 1, ".",
	0, DIRBLKSIZ - 12, DT_DIR, 2, ".."
};
static struct odirtemplate omastertemplate = {
	0, 12, 1, ".",
	0, DIRBLKSIZ - 12, 2, ".."
};

static void
ufs_itimes_locked(struct vnode *vp)
{
	struct inode *ip;
	struct timespec ts;

	ASSERT_VI_LOCKED(vp, __func__);

	ip = VTOI(vp);
	if (UFS_RDONLY(ip))
		goto out;
	if ((ip->i_flag & (IN_ACCESS | IN_CHANGE | IN_UPDATE)) == 0)
		return;

	if ((vp->v_type == VBLK || vp->v_type == VCHR) && !DOINGSOFTDEP(vp))
		UFS_INODE_SET_FLAG(ip, IN_LAZYMOD);
	else if (((vp->v_mount->mnt_kern_flag &
		    (MNTK_SUSPENDED | MNTK_SUSPEND)) == 0) ||
		    (ip->i_flag & (IN_CHANGE | IN_UPDATE)))
		UFS_INODE_SET_FLAG(ip, IN_MODIFIED);
	else if (ip->i_flag & IN_ACCESS)
		UFS_INODE_SET_FLAG(ip, IN_LAZYACCESS);
	vfs_timestamp(&ts);
	if (ip->i_flag & IN_ACCESS) {
		DIP_SET(ip, i_atime, ts.tv_sec);
		DIP_SET(ip, i_atimensec, ts.tv_nsec);
	}
	if (ip->i_flag & IN_UPDATE) {
		DIP_SET(ip, i_mtime, ts.tv_sec);
		DIP_SET(ip, i_mtimensec, ts.tv_nsec);
	}
	if (ip->i_flag & IN_CHANGE) {
		DIP_SET(ip, i_ctime, ts.tv_sec);
		DIP_SET(ip, i_ctimensec, ts.tv_nsec);
		DIP_SET(ip, i_modrev, DIP(ip, i_modrev) + 1);
	}

 out:
	ip->i_flag &= ~(IN_ACCESS | IN_CHANGE | IN_UPDATE);
}

void
ufs_itimes(struct vnode *vp)
{
	struct inode *ip;

	ip = VTOI(vp);
	if ((ip->i_flag & (IN_ACCESS | IN_CHANGE | IN_UPDATE)) == 0)
		return;

	VI_LOCK(vp);
	ufs_itimes_locked(vp);
	VI_UNLOCK(vp);
}

static int
ufs_sync_nlink1(struct mount *mp)
{
	int error;

	error = vfs_busy(mp, 0);
	if (error == 0) {
		VFS_SYNC(mp, MNT_WAIT);
		vfs_unbusy(mp);
		error = ERELOOKUP;
	}
	vfs_rel(mp);
	return (error);
}

static int
ufs_sync_nlink(struct vnode *vp, struct vnode *vp1)
{
	struct inode *ip;
	struct mount *mp;
	int error;

	ip = VTOI(vp);
	if (ip->i_nlink < UFS_LINK_MAX)
		return (0);
	if (!DOINGSOFTDEP(vp) || ip->i_effnlink >= UFS_LINK_MAX)
		return (EMLINK);

	mp = vp->v_mount;
	vfs_ref(mp);
	VOP_UNLOCK(vp);
	if (vp1 != NULL)
		VOP_UNLOCK(vp1);
	error = ufs_sync_nlink1(mp);
	vn_lock_pair(vp, false, LK_EXCLUSIVE, vp1, false, LK_EXCLUSIVE);
	return (error);
}

/*
 * Create a regular file
 */
static int
ufs_create(
	struct vop_create_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap)
{
	int error;

	error =
	    ufs_makeinode(MAKEIMODE(ap->a_vap->va_type, ap->a_vap->va_mode),
	    ap->a_dvp, ap->a_vpp, ap->a_cnp, "ufs_create");
	if (error != 0)
		return (error);
	if ((ap->a_cnp->cn_flags & MAKEENTRY) != 0)
		cache_enter(ap->a_dvp, *ap->a_vpp, ap->a_cnp);
	return (0);
}

/*
 * Mknod vnode call
 */
/* ARGSUSED */
static int
ufs_mknod(
	struct vop_mknod_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap)
{
	struct vattr *vap = ap->a_vap;
	struct vnode **vpp = ap->a_vpp;
	struct inode *ip;
	ino_t ino;
	int error;

	error = ufs_makeinode(MAKEIMODE(vap->va_type, vap->va_mode),
	    ap->a_dvp, vpp, ap->a_cnp, "ufs_mknod");
	if (error)
		return (error);
	ip = VTOI(*vpp);
	UFS_INODE_SET_FLAG(ip, IN_ACCESS | IN_CHANGE | IN_UPDATE);
	if (vap->va_rdev != VNOVAL) {
		/*
		 * Want to be able to use this to make badblock
		 * inodes, so don't truncate the dev number.
		 */
		DIP_SET(ip, i_rdev, vap->va_rdev);
	}
	/*
	 * Remove inode, then reload it through VFS_VGET().  This is
	 * needed to do further inode initialization, for instance
	 * fifo, which was too early for VFS_VGET() done as part of
	 * UFS_VALLOC().
	 */
	(*vpp)->v_type = VNON;
	ino = ip->i_number;	/* Save this before vgone() invalidates ip. */
	vgone(*vpp);
	vput(*vpp);
	error = VFS_VGET(ap->a_dvp->v_mount, ino, LK_EXCLUSIVE, vpp);
	if (error) {
		*vpp = NULL;
		return (error);
	}
	return (0);
}

/*
 * Open called.
 */
/* ARGSUSED */
static int
ufs_open(struct vop_open_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip;

	if (vp->v_type == VCHR || vp->v_type == VBLK)
		return (EOPNOTSUPP);

	ip = VTOI(vp);
	vnode_create_vobject(vp, DIP(ip, i_size), ap->a_td);
	if (vp->v_type == VREG && (vn_irflag_read(vp) & VIRF_PGREAD) == 0 &&
	    ip->i_ump->um_bsize >= PAGE_SIZE) {
		vn_irflag_set_cond(vp, VIRF_PGREAD);
	}

	/*
	 * Files marked append-only must be opened for appending.
	 */
	if ((ip->i_flags & APPEND) &&
	    (ap->a_mode & (FWRITE | O_APPEND)) == FWRITE)
		return (EPERM);

	return (0);
}

/*
 * Close called.
 *
 * Update the times on the inode.
 */
/* ARGSUSED */
static int
ufs_close(
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap)
{
	struct vnode *vp = ap->a_vp;

	ufs_itimes(vp);
	return (0);
}

static int
ufs_accessx(
	struct vop_accessx_args /* {
		struct vnode *a_vp;
		accmode_t a_accmode;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap)
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	accmode_t accmode = ap->a_accmode;
	int error;
#ifdef UFS_ACL
	struct acl *acl;
	acl_type_t type;
#endif

	/*
	 * Disallow write attempts on read-only filesystems;
	 * unless the file is a socket, fifo, or a block or
	 * character device resident on the filesystem.
	 */
	if (accmode & VMODIFY_PERMS) {
		switch (vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
#ifdef QUOTA
			/*
			 * Inode is accounted in the quotas only if struct
			 * dquot is attached to it. VOP_ACCESS() is called
			 * from vn_open_cred() and provides a convenient
			 * point to call getinoquota().  The lock mode is
			 * exclusive when the file is opening for write.
			 */
			if (VOP_ISLOCKED(vp) == LK_EXCLUSIVE) {
				error = getinoquota(ip);
				if (error != 0)
					return (error);
			}
#endif
			break;
		default:
			break;
		}
	}

	/*
	 * If immutable bit set, nobody gets to write it.  "& ~VADMIN_PERMS"
	 * permits the owner of the file to remove the IMMUTABLE flag.
	 */
	if ((accmode & (VMODIFY_PERMS & ~VADMIN_PERMS)) &&
	    (ip->i_flags & (IMMUTABLE | SF_SNAPSHOT)))
		return (EPERM);

#ifdef UFS_ACL
	if ((vp->v_mount->mnt_flag & (MNT_ACLS | MNT_NFS4ACLS)) != 0) {
		if (vp->v_mount->mnt_flag & MNT_NFS4ACLS)
			type = ACL_TYPE_NFS4;
		else
			type = ACL_TYPE_ACCESS;

		acl = acl_alloc(M_WAITOK);
		if (type == ACL_TYPE_NFS4)
			error = ufs_getacl_nfs4_internal(vp, acl, ap->a_td);
		else
			error = VOP_GETACL(vp, type, acl, ap->a_cred, ap->a_td);
		switch (error) {
		case 0:
			if (type == ACL_TYPE_NFS4) {
				error = vaccess_acl_nfs4(vp->v_type, ip->i_uid,
				    ip->i_gid, acl, accmode, ap->a_cred);
			} else {
				error = vfs_unixify_accmode(&accmode);
				if (error == 0)
					error = vaccess_acl_posix1e(vp->v_type, ip->i_uid,
					    ip->i_gid, acl, accmode, ap->a_cred);
			}
			break;
		default:
			if (error != EOPNOTSUPP)
				printf(
"ufs_accessx(): Error retrieving ACL on object (%d).\n",
				    error);
			/*
			 * XXX: Fall back until debugged.  Should
			 * eventually possibly log an error, and return
			 * EPERM for safety.
			 */
			error = vfs_unixify_accmode(&accmode);
			if (error == 0)
				error = vaccess(vp->v_type, ip->i_mode,
				    ip->i_uid, ip->i_gid, accmode, ap->a_cred);
		}
		acl_free(acl);

		return (error);
	}
#endif /* !UFS_ACL */
	error = vfs_unixify_accmode(&accmode);
	if (error == 0)
		error = vaccess(vp->v_type, ip->i_mode, ip->i_uid, ip->i_gid,
		    accmode, ap->a_cred);
	return (error);
}

/*
 * VOP_FPLOOKUP_VEXEC routines are subject to special circumstances, see
 * the comment above cache_fplookup for details.
 */
int
ufs_fplookup_vexec(
	struct vop_fplookup_vexec_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap)
{
	struct vnode *vp;
	struct inode *ip;
	struct ucred *cred;
	mode_t all_x, mode;

	vp = ap->a_vp;
	ip = VTOI_SMR(vp);
	if (__predict_false(ip == NULL))
		return (EAGAIN);

	/*
	 * XXX ACL race
	 *
	 * ACLs are not supported and UFS clears/sets this flag on mount and
	 * remount. However, we may still be racing with seeing them and there
	 * is no provision to make sure they were accounted for. This matches
	 * the behavior of the locked case, since the lookup there is also
	 * racy: mount takes no measures to block anyone from progressing.
	 */
	all_x = S_IXUSR | S_IXGRP | S_IXOTH;
	mode = atomic_load_short(&ip->i_mode);
	if (__predict_true((mode & all_x) == all_x))
		return (0);

	cred = ap->a_cred;
	return (vaccess_vexec_smr(mode, ip->i_uid, ip->i_gid, cred));
}

/* ARGSUSED */
static int
ufs_stat(struct vop_stat_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct stat *sb = ap->a_sb;
	int error;

	error = vop_stat_helper_pre(ap);
	if (__predict_false(error))
		return (error);

	VI_LOCK(vp);
	ufs_itimes_locked(vp);
	if (I_IS_UFS1(ip)) {
		sb->st_atim.tv_sec = ip->i_din1->di_atime;
		sb->st_atim.tv_nsec = ip->i_din1->di_atimensec;
	} else {
		sb->st_atim.tv_sec = ip->i_din2->di_atime;
		sb->st_atim.tv_nsec = ip->i_din2->di_atimensec;
	}
	VI_UNLOCK(vp);

	sb->st_dev = dev2udev(ITOUMP(ip)->um_dev);
	sb->st_ino = ip->i_number;
	sb->st_mode = (ip->i_mode & ~IFMT) | VTTOIF(vp->v_type);
	sb->st_nlink = ip->i_effnlink;
	sb->st_uid = ip->i_uid;
	sb->st_gid = ip->i_gid;
	if (I_IS_UFS1(ip)) {
		sb->st_rdev = ip->i_din1->di_rdev;
		sb->st_size = ip->i_din1->di_size;
		sb->st_mtim.tv_sec = ip->i_din1->di_mtime;
		sb->st_mtim.tv_nsec = ip->i_din1->di_mtimensec;
		sb->st_ctim.tv_sec = ip->i_din1->di_ctime;
		sb->st_ctim.tv_nsec = ip->i_din1->di_ctimensec;
		sb->st_birthtim.tv_sec = -1;
		sb->st_birthtim.tv_nsec = 0;
		sb->st_blocks = dbtob((uint64_t)ip->i_din1->di_blocks) / S_BLKSIZE;
		sb->st_filerev = ip->i_din1->di_modrev;
	} else {
		sb->st_rdev = ip->i_din2->di_rdev;
		sb->st_size = ip->i_din2->di_size;
		sb->st_mtim.tv_sec = ip->i_din2->di_mtime;
		sb->st_mtim.tv_nsec = ip->i_din2->di_mtimensec;
		sb->st_ctim.tv_sec = ip->i_din2->di_ctime;
		sb->st_ctim.tv_nsec = ip->i_din2->di_ctimensec;
		sb->st_birthtim.tv_sec = ip->i_din2->di_birthtime;
		sb->st_birthtim.tv_nsec = ip->i_din2->di_birthnsec;
		sb->st_blocks = dbtob((uint64_t)ip->i_din2->di_blocks) / S_BLKSIZE;
		sb->st_filerev = ip->i_din2->di_modrev;
	}

	sb->st_blksize = max(PAGE_SIZE, vp->v_mount->mnt_stat.f_iosize);
	sb->st_flags = ip->i_flags;
	sb->st_gen = ip->i_gen;

	return (vop_stat_helper_post(ap, error));
}

/* ARGSUSED */
static int
ufs_getattr(
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
	} */ *ap)
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct vattr *vap = ap->a_vap;

	VI_LOCK(vp);
	ufs_itimes_locked(vp);
	if (I_IS_UFS1(ip)) {
		vap->va_atime.tv_sec = ip->i_din1->di_atime;
		vap->va_atime.tv_nsec = ip->i_din1->di_atimensec;
	} else {
		vap->va_atime.tv_sec = ip->i_din2->di_atime;
		vap->va_atime.tv_nsec = ip->i_din2->di_atimensec;
	}
	VI_UNLOCK(vp);
	/*
	 * Copy from inode table
	 */
	vap->va_fsid = dev2udev(ITOUMP(ip)->um_dev);
	vap->va_fileid = ip->i_number;
	vap->va_mode = ip->i_mode & ~IFMT;
	vap->va_nlink = ip->i_effnlink;
	vap->va_uid = ip->i_uid;
	vap->va_gid = ip->i_gid;
	if (I_IS_UFS1(ip)) {
		vap->va_rdev = ip->i_din1->di_rdev;
		vap->va_size = ip->i_din1->di_size;
		vap->va_mtime.tv_sec = ip->i_din1->di_mtime;
		vap->va_mtime.tv_nsec = ip->i_din1->di_mtimensec;
		vap->va_ctime.tv_sec = ip->i_din1->di_ctime;
		vap->va_ctime.tv_nsec = ip->i_din1->di_ctimensec;
		vap->va_bytes = dbtob((uint64_t)ip->i_din1->di_blocks);
		vap->va_filerev = ip->i_din1->di_modrev;
	} else {
		vap->va_rdev = ip->i_din2->di_rdev;
		vap->va_size = ip->i_din2->di_size;
		vap->va_mtime.tv_sec = ip->i_din2->di_mtime;
		vap->va_mtime.tv_nsec = ip->i_din2->di_mtimensec;
		vap->va_ctime.tv_sec = ip->i_din2->di_ctime;
		vap->va_ctime.tv_nsec = ip->i_din2->di_ctimensec;
		vap->va_birthtime.tv_sec = ip->i_din2->di_birthtime;
		vap->va_birthtime.tv_nsec = ip->i_din2->di_birthnsec;
		vap->va_bytes = dbtob((uint64_t)ip->i_din2->di_blocks);
		vap->va_filerev = ip->i_din2->di_modrev;
	}
	vap->va_flags = ip->i_flags;
	vap->va_gen = ip->i_gen;
	vap->va_blocksize = vp->v_mount->mnt_stat.f_iosize;
	vap->va_type = IFTOVT(ip->i_mode);
	return (0);
}

/*
 * Set attribute vnode op. called from several syscalls
 */
static int
ufs_setattr(
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
	} */ *ap)
{
	struct vattr *vap = ap->a_vap;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct ucred *cred = ap->a_cred;
	struct thread *td = curthread;
	int error;

	/*
	 * Check for unsettable attributes.
	 */
	if ((vap->va_type != VNON) || (vap->va_nlink != VNOVAL) ||
	    (vap->va_fsid != VNOVAL) || (vap->va_fileid != VNOVAL) ||
	    (vap->va_blocksize != VNOVAL) || (vap->va_rdev != VNOVAL) ||
	    ((int)vap->va_bytes != VNOVAL) || (vap->va_gen != VNOVAL)) {
		return (EINVAL);
	}
	if (vap->va_flags != VNOVAL) {
		if ((vap->va_flags & ~(SF_APPEND | SF_ARCHIVED | SF_IMMUTABLE |
		    SF_NOUNLINK | SF_SNAPSHOT | UF_APPEND | UF_ARCHIVE |
		    UF_HIDDEN | UF_IMMUTABLE | UF_NODUMP | UF_NOUNLINK |
		    UF_OFFLINE | UF_OPAQUE | UF_READONLY | UF_REPARSE |
		    UF_SPARSE | UF_SYSTEM)) != 0)
			return (EOPNOTSUPP);
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		/*
		 * Callers may only modify the file flags on objects they
		 * have VADMIN rights for.
		 */
		if ((error = VOP_ACCESS(vp, VADMIN, cred, td)))
			return (error);
		/*
		 * Unprivileged processes are not permitted to unset system
		 * flags, or modify flags if any system flags are set.
		 * Privileged non-jail processes may not modify system flags
		 * if securelevel > 0 and any existing system flags are set.
		 * Privileged jail processes behave like privileged non-jail
		 * processes if the PR_ALLOW_CHFLAGS permission bit is set;
		 * otherwise, they behave like unprivileged processes.
		 */
		if (!priv_check_cred(cred, PRIV_VFS_SYSFLAGS)) {
			if (ip->i_flags &
			    (SF_NOUNLINK | SF_IMMUTABLE | SF_APPEND)) {
				error = securelevel_gt(cred, 0);
				if (error)
					return (error);
			}
			/* The snapshot flag cannot be toggled. */
			if ((vap->va_flags ^ ip->i_flags) & SF_SNAPSHOT)
				return (EPERM);
		} else {
			if (ip->i_flags &
			    (SF_NOUNLINK | SF_IMMUTABLE | SF_APPEND) ||
			    ((vap->va_flags ^ ip->i_flags) & SF_SETTABLE))
				return (EPERM);
		}
		ip->i_flags = vap->va_flags;
		DIP_SET(ip, i_flags, vap->va_flags);
		UFS_INODE_SET_FLAG(ip, IN_CHANGE);
		error = UFS_UPDATE(vp, 0);
		if (ip->i_flags & (IMMUTABLE | APPEND))
			return (error);
	}
	/*
	 * If immutable or append, no one can change any of its attributes
	 * except the ones already handled (in some cases, file flags
	 * including the immutability flags themselves for the superuser).
	 */
	if (ip->i_flags & (IMMUTABLE | APPEND))
		return (EPERM);
	/*
	 * Go through the fields and update iff not VNOVAL.
	 */
	if (vap->va_uid != (uid_t)VNOVAL || vap->va_gid != (gid_t)VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		if ((error = ufs_chown(vp, vap->va_uid, vap->va_gid, cred,
		    td)) != 0)
			return (error);
	}
	if (vap->va_size != VNOVAL) {
		/*
		 * XXX most of the following special cases should be in
		 * callers instead of in N filesystems.  The VDIR check
		 * mostly already is.
		 */
		switch (vp->v_type) {
		case VDIR:
			return (EISDIR);
		case VLNK:
		case VREG:
			/*
			 * Truncation should have an effect in these cases.
			 * Disallow it if the filesystem is read-only or
			 * the file is being snapshotted.
			 */
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
			if (IS_SNAPSHOT(ip))
				return (EPERM);
			break;
		default:
			/*
			 * According to POSIX, the result is unspecified
			 * for file types other than regular files,
			 * directories and shared memory objects.  We
			 * don't support shared memory objects in the file
			 * system, and have dubious support for truncating
			 * symlinks.  Just ignore the request in other cases.
			 */
			return (0);
		}
		error = vn_rlimit_trunc(vap->va_size, td);
		if (error != 0)
			return (error);
		if ((error = UFS_TRUNCATE(vp, vap->va_size, IO_NORMAL |
		    ((vap->va_vaflags & VA_SYNC) != 0 ? IO_SYNC : 0),
		    cred)) != 0)
			return (error);
	}
	if (vap->va_atime.tv_sec != VNOVAL ||
	    vap->va_mtime.tv_sec != VNOVAL ||
	    vap->va_birthtime.tv_sec != VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		if (IS_SNAPSHOT(ip))
			return (EPERM);
		error = vn_utimes_perm(vp, vap, cred, td);
		if (error != 0)
			return (error);
		UFS_INODE_SET_FLAG(ip, IN_CHANGE | IN_MODIFIED);
		if (vap->va_atime.tv_sec != VNOVAL) {
			ip->i_flag &= ~IN_ACCESS;
			DIP_SET(ip, i_atime, vap->va_atime.tv_sec);
			DIP_SET(ip, i_atimensec, vap->va_atime.tv_nsec);
		}
		if (vap->va_mtime.tv_sec != VNOVAL) {
			ip->i_flag &= ~IN_UPDATE;
			DIP_SET(ip, i_mtime, vap->va_mtime.tv_sec);
			DIP_SET(ip, i_mtimensec, vap->va_mtime.tv_nsec);
		}
		if (vap->va_birthtime.tv_sec != VNOVAL && I_IS_UFS2(ip)) {
			ip->i_din2->di_birthtime = vap->va_birthtime.tv_sec;
			ip->i_din2->di_birthnsec = vap->va_birthtime.tv_nsec;
		}
		error = UFS_UPDATE(vp, 0);
		if (error)
			return (error);
	}
	error = 0;
	if (vap->va_mode != (mode_t)VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		if (IS_SNAPSHOT(ip) && (vap->va_mode & (S_IXUSR | S_IWUSR |
		    S_IXGRP | S_IWGRP | S_IXOTH | S_IWOTH)) != 0)
			return (EPERM);
		error = ufs_chmod(vp, (int)vap->va_mode, cred, td);
	}
	return (error);
}

#ifdef UFS_ACL
static int
ufs_update_nfs4_acl_after_mode_change(struct vnode *vp, int mode,
    int file_owner_id, struct ucred *cred, struct thread *td)
{
	int error;
	struct acl *aclp;

	aclp = acl_alloc(M_WAITOK);
	error = ufs_getacl_nfs4_internal(vp, aclp, td);
	/*
	 * We don't have to handle EOPNOTSUPP here, as the filesystem claims
	 * it supports ACLs.
	 */
	if (error)
		goto out;

	acl_nfs4_sync_acl_from_mode(aclp, mode, file_owner_id);
	error = ufs_setacl_nfs4_internal(vp, aclp, td);

out:
	acl_free(aclp);
	return (error);
}
#endif /* UFS_ACL */

static int
ufs_mmapped(
	struct vop_mmapped_args /* {
		struct vnode *a_vp;
	} */ *ap)
{
	struct vnode *vp;
	struct inode *ip;
	struct mount *mp;

	vp = ap->a_vp;
	ip = VTOI(vp);
	mp = vp->v_mount;

	if ((mp->mnt_flag & (MNT_NOATIME | MNT_RDONLY)) == 0)
		UFS_INODE_SET_FLAG_SHARED(ip, IN_ACCESS);
	/*
	 * XXXKIB No UFS_UPDATE(ap->a_vp, 0) there.
	 */
	return (0);
}

/*
 * Change the mode on a file.
 * Inode must be locked before calling.
 */
static int
ufs_chmod(struct vnode *vp, int mode, struct ucred *cred, struct thread *td)
{
	struct inode *ip = VTOI(vp);
	int newmode, error;

	/*
	 * To modify the permissions on a file, must possess VADMIN
	 * for that file.
	 */
	if ((error = VOP_ACCESSX(vp, VWRITE_ACL, cred, td)))
		return (error);
	/*
	 * Privileged processes may set the sticky bit on non-directories,
	 * as well as set the setgid bit on a file with a group that the
	 * process is not a member of.  Both of these are allowed in
	 * jail(8).
	 */
	if (vp->v_type != VDIR && (mode & S_ISTXT)) {
		if (priv_check_cred(cred, PRIV_VFS_STICKYFILE))
			return (EFTYPE);
	}
	if (!groupmember(ip->i_gid, cred) && (mode & ISGID)) {
		error = priv_check_cred(cred, PRIV_VFS_SETGID);
		if (error)
			return (error);
	}

	/*
	 * Deny setting setuid if we are not the file owner.
	 */
	if ((mode & ISUID) && ip->i_uid != cred->cr_uid) {
		error = priv_check_cred(cred, PRIV_VFS_ADMIN);
		if (error)
			return (error);
	}

	newmode = ip->i_mode & ~ALLPERMS;
	newmode |= (mode & ALLPERMS);
	UFS_INODE_SET_MODE(ip, newmode);
	DIP_SET(ip, i_mode, ip->i_mode);
	UFS_INODE_SET_FLAG(ip, IN_CHANGE);
#ifdef UFS_ACL
	if ((vp->v_mount->mnt_flag & MNT_NFS4ACLS) != 0)
		error = ufs_update_nfs4_acl_after_mode_change(vp, mode, ip->i_uid, cred, td);
#endif
	if (error == 0 && (ip->i_flag & IN_CHANGE) != 0)
		error = UFS_UPDATE(vp, 0);

	return (error);
}

/*
 * Perform chown operation on inode ip;
 * inode must be locked prior to call.
 */
static int
ufs_chown(struct vnode *vp, uid_t uid, gid_t gid, struct ucred *cred,
    struct thread *td)
{
	struct inode *ip = VTOI(vp);
	uid_t ouid;
	gid_t ogid;
	int error = 0;
#ifdef QUOTA
	int i;
	ufs2_daddr_t change;
#endif

	if (uid == (uid_t)VNOVAL)
		uid = ip->i_uid;
	if (gid == (gid_t)VNOVAL)
		gid = ip->i_gid;
	/*
	 * To modify the ownership of a file, must possess VADMIN for that
	 * file.
	 */
	if ((error = VOP_ACCESSX(vp, VWRITE_OWNER, cred, td)))
		return (error);
	/*
	 * To change the owner of a file, or change the group of a file to a
	 * group of which we are not a member, the caller must have
	 * privilege.
	 */
	if (((uid != ip->i_uid && uid != cred->cr_uid) || 
	    (gid != ip->i_gid && !groupmember(gid, cred))) &&
	    (error = priv_check_cred(cred, PRIV_VFS_CHOWN)))
		return (error);
	ogid = ip->i_gid;
	ouid = ip->i_uid;
#ifdef QUOTA
	if ((error = getinoquota(ip)) != 0)
		return (error);
	if (ouid == uid) {
		dqrele(vp, ip->i_dquot[USRQUOTA]);
		ip->i_dquot[USRQUOTA] = NODQUOT;
	}
	if (ogid == gid) {
		dqrele(vp, ip->i_dquot[GRPQUOTA]);
		ip->i_dquot[GRPQUOTA] = NODQUOT;
	}
	change = DIP(ip, i_blocks);
	(void) chkdq(ip, -change, cred, CHOWN|FORCE);
	(void) chkiq(ip, -1, cred, CHOWN|FORCE);
	for (i = 0; i < MAXQUOTAS; i++) {
		dqrele(vp, ip->i_dquot[i]);
		ip->i_dquot[i] = NODQUOT;
	}
#endif
	ip->i_gid = gid;
	DIP_SET(ip, i_gid, gid);
	ip->i_uid = uid;
	DIP_SET(ip, i_uid, uid);
#ifdef QUOTA
	if ((error = getinoquota(ip)) == 0) {
		if (ouid == uid) {
			dqrele(vp, ip->i_dquot[USRQUOTA]);
			ip->i_dquot[USRQUOTA] = NODQUOT;
		}
		if (ogid == gid) {
			dqrele(vp, ip->i_dquot[GRPQUOTA]);
			ip->i_dquot[GRPQUOTA] = NODQUOT;
		}
		if ((error = chkdq(ip, change, cred, CHOWN)) == 0) {
			if ((error = chkiq(ip, 1, cred, CHOWN)) == 0)
				goto good;
			else
				(void) chkdq(ip, -change, cred, CHOWN|FORCE);
		}
		for (i = 0; i < MAXQUOTAS; i++) {
			dqrele(vp, ip->i_dquot[i]);
			ip->i_dquot[i] = NODQUOT;
		}
	}
	ip->i_gid = ogid;
	DIP_SET(ip, i_gid, ogid);
	ip->i_uid = ouid;
	DIP_SET(ip, i_uid, ouid);
	if (getinoquota(ip) == 0) {
		if (ouid == uid) {
			dqrele(vp, ip->i_dquot[USRQUOTA]);
			ip->i_dquot[USRQUOTA] = NODQUOT;
		}
		if (ogid == gid) {
			dqrele(vp, ip->i_dquot[GRPQUOTA]);
			ip->i_dquot[GRPQUOTA] = NODQUOT;
		}
		(void) chkdq(ip, change, cred, FORCE|CHOWN);
		(void) chkiq(ip, 1, cred, FORCE|CHOWN);
		(void) getinoquota(ip);
	}
	return (error);
good:
	if (getinoquota(ip))
		panic("ufs_chown: lost quota");
#endif /* QUOTA */
	UFS_INODE_SET_FLAG(ip, IN_CHANGE);
	if ((ip->i_mode & (ISUID | ISGID)) && (ouid != uid || ogid != gid)) {
		if (priv_check_cred(cred, PRIV_VFS_RETAINSUGID)) {
			UFS_INODE_SET_MODE(ip, ip->i_mode & ~(ISUID | ISGID));
			DIP_SET(ip, i_mode, ip->i_mode);
		}
	}
	error = UFS_UPDATE(vp, 0);
	return (error);
}

static int
ufs_remove(
	struct vop_remove_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap)
{
	struct inode *ip;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	int error;
	struct thread *td;

	td = curthread;
	ip = VTOI(vp);
	if ((ip->i_flags & (NOUNLINK | IMMUTABLE | APPEND)) ||
	    (VTOI(dvp)->i_flags & APPEND))
		return (EPERM);
	if (DOINGSUJ(dvp)) {
		error = softdep_prelink(dvp, vp, ap->a_cnp);
		if (error != 0) {
			MPASS(error == ERELOOKUP);
			return (error);
		}
	}

#ifdef UFS_GJOURNAL
	ufs_gjournal_orphan(vp);
#endif
	error = ufs_dirremove(dvp, ip, ap->a_cnp->cn_flags, 0);
	if (ip->i_nlink <= 0)
		vp->v_vflag |= VV_NOSYNC;
	if (IS_SNAPSHOT(ip)) {
		/*
		 * Avoid deadlock where another thread is trying to
		 * update the inodeblock for dvp and is waiting on
		 * snaplk.  Temporary unlock the vnode lock for the
		 * unlinked file and sync the directory.  This should
		 * allow vput() of the directory to not block later on
		 * while holding the snapshot vnode locked, assuming
		 * that the directory hasn't been unlinked too.
		 */
		VOP_UNLOCK(vp);
		(void) VOP_FSYNC(dvp, MNT_WAIT, td);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	}
	return (error);
}

static void
print_bad_link_count(const char *funcname, struct vnode *dvp)
{
	struct inode *dip;

	dip = VTOI(dvp);
	uprintf("%s: Bad link count %d on parent inode %jd in file system %s\n",
	    funcname, dip->i_effnlink, (intmax_t)dip->i_number,
	    dvp->v_mount->mnt_stat.f_mntonname);
}

/*
 * link vnode call
 */
static int
ufs_link(
	struct vop_link_args /* {
		struct vnode *a_tdvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap)
{
	struct vnode *vp = ap->a_vp;
	struct vnode *tdvp = ap->a_tdvp;
	struct componentname *cnp = ap->a_cnp;
	struct inode *ip;
	struct direct newdir;
	int error;

	if (DOINGSUJ(tdvp)) {
		error = softdep_prelink(tdvp, vp, cnp);
		if (error != 0) {
			MPASS(error == ERELOOKUP);
			return (error);
		}
	}

	if (VTOI(tdvp)->i_effnlink < 2) {
		print_bad_link_count("ufs_link", tdvp);
		error = EINVAL;
		goto out;
	}
	error = ufs_sync_nlink(vp, tdvp);
	if (error != 0)
		goto out;
	ip = VTOI(vp);

	/*
	 * The file may have been removed after namei dropped the original
	 * lock.
	 */
	if (ip->i_effnlink == 0) {
		error = ENOENT;
		goto out;
	}
	if (ip->i_flags & (IMMUTABLE | APPEND)) {
		error = EPERM;
		goto out;
	}

	ip->i_effnlink++;
	ip->i_nlink++;
	DIP_SET_NLINK(ip, ip->i_nlink);
	UFS_INODE_SET_FLAG(ip, IN_CHANGE);
	if (DOINGSOFTDEP(vp))
		softdep_setup_link(VTOI(tdvp), ip);
	error = UFS_UPDATE(vp, !DOINGSOFTDEP(vp) && !DOINGASYNC(vp));
	if (!error) {
		ufs_makedirentry(ip, cnp, &newdir);
		error = ufs_direnter(tdvp, vp, &newdir, cnp, NULL);
	}

	if (error) {
		ip->i_effnlink--;
		ip->i_nlink--;
		DIP_SET_NLINK(ip, ip->i_nlink);
		UFS_INODE_SET_FLAG(ip, IN_CHANGE);
		if (DOINGSOFTDEP(vp))
			softdep_revert_link(VTOI(tdvp), ip);
	}
out:
	return (error);
}

/*
 * whiteout vnode call
 */
static int
ufs_whiteout(
	struct vop_whiteout_args /* {
		struct vnode *a_dvp;
		struct componentname *a_cnp;
		int a_flags;
	} */ *ap)
{
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct direct newdir;
	int error = 0;

	if (DOINGSUJ(dvp) && (ap->a_flags == CREATE ||
	    ap->a_flags == DELETE)) {
		error = softdep_prelink(dvp, NULL, cnp);
		if (error != 0) {
			MPASS(error == ERELOOKUP);
			return (error);
		}
	}

	switch (ap->a_flags) {
	case LOOKUP:
		/* 4.4 format directories support whiteout operations */
		if (!OFSFMT(dvp))
			return (0);
		return (EOPNOTSUPP);

	case CREATE:
		/* create a new directory whiteout */
#ifdef INVARIANTS
		if (OFSFMT(dvp))
			panic("ufs_whiteout: old format filesystem");
#endif

		newdir.d_ino = UFS_WINO;
		newdir.d_namlen = cnp->cn_namelen;
		bcopy(cnp->cn_nameptr, newdir.d_name, (unsigned)cnp->cn_namelen + 1);
		newdir.d_type = DT_WHT;
		error = ufs_direnter(dvp, NULL, &newdir, cnp, NULL);
		break;

	case DELETE:
		/* remove an existing directory whiteout */
#ifdef INVARIANTS
		if (OFSFMT(dvp))
			panic("ufs_whiteout: old format filesystem");
#endif

		cnp->cn_flags &= ~DOWHITEOUT;
		error = ufs_dirremove(dvp, NULL, cnp->cn_flags, 0);
		break;
	default:
		panic("ufs_whiteout: unknown op");
	}
	return (error);
}

static volatile int rename_restarts;
SYSCTL_INT(_vfs_ufs, OID_AUTO, rename_restarts, CTLFLAG_RD,
    __DEVOLATILE(int *, &rename_restarts), 0,
    "Times rename had to restart due to lock contention");

/*
 * Rename system call.
 * 	rename("foo", "bar");
 * is essentially
 *	unlink("bar");
 *	link("foo", "bar");
 *	unlink("foo");
 * but ``atomically''.  Can't do full commit without saving state in the
 * inode on disk which isn't feasible at this time.  Best we can do is
 * always guarantee the target exists.
 *
 * Basic algorithm is:
 *
 * 1) Bump link count on source while we're linking it to the
 *    target.  This also ensure the inode won't be deleted out
 *    from underneath us while we work (it may be truncated by
 *    a concurrent `trunc' or `open' for creation).
 * 2) Link source to destination.  If destination already exists,
 *    delete it first.
 * 3) Unlink source reference to inode if still around. If a
 *    directory was moved and the parent of the destination
 *    is different from the source, patch the ".." entry in the
 *    directory.
 */
static int
ufs_rename(
	struct vop_rename_args  /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap)
{
	struct vnode *tvp = ap->a_tvp;
	struct vnode *tdvp = ap->a_tdvp;
	struct vnode *fvp = ap->a_fvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct vnode *nvp;
	struct componentname *tcnp = ap->a_tcnp;
	struct componentname *fcnp = ap->a_fcnp;
	struct thread *td = curthread;
	struct inode *fip, *tip, *tdp, *fdp;
	struct direct newdir;
	off_t endoff;
	int doingdirectory, newparent;
	int error = 0;
	struct mount *mp;
	ino_t ino;
	seqc_t fdvp_s, fvp_s, tdvp_s, tvp_s;
	bool checkpath_locked, want_seqc_end;

	checkpath_locked = want_seqc_end = false;

	endoff = 0;
	mp = tdvp->v_mount;
	VOP_UNLOCK(tdvp);
	if (tvp && tvp != tdvp)
		VOP_UNLOCK(tvp);
	/*
	 * Check for cross-device rename.
	 */
	if ((fvp->v_mount != tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
		mp = NULL;
		goto releout;
	}

	fdvp_s = fvp_s = tdvp_s = tvp_s = SEQC_MOD;
relock:
	/* 
	 * We need to acquire 2 to 4 locks depending on whether tvp is NULL
	 * and fdvp and tdvp are the same directory.  Subsequently we need
	 * to double-check all paths and in the directory rename case we
	 * need to verify that we are not creating a directory loop.  To
	 * handle this we acquire all but fdvp using non-blocking
	 * acquisitions.  If we fail to acquire any lock in the path we will
	 * drop all held locks, acquire the new lock in a blocking fashion,
	 * and then release it and restart the rename.  This acquire/release
	 * step ensures that we do not spin on a lock waiting for release.
	 */
	error = vn_lock(fdvp, LK_EXCLUSIVE);
	if (error)
		goto releout;
	if (vn_lock(tdvp, LK_EXCLUSIVE | LK_NOWAIT) != 0) {
		VOP_UNLOCK(fdvp);
		error = vn_lock(tdvp, LK_EXCLUSIVE);
		if (error)
			goto releout;
		VOP_UNLOCK(tdvp);
		atomic_add_int(&rename_restarts, 1);
		goto relock;
	}
	/*
	 * Re-resolve fvp to be certain it still exists and fetch the
	 * correct vnode.
	 */
	error = ufs_lookup_ino(fdvp, NULL, fcnp, &ino);
	if (error) {
		VOP_UNLOCK(fdvp);
		VOP_UNLOCK(tdvp);
		goto releout;
	}
	error = VFS_VGET(mp, ino, LK_EXCLUSIVE | LK_NOWAIT, &nvp);
	if (error) {
		VOP_UNLOCK(fdvp);
		VOP_UNLOCK(tdvp);
		if (error != EBUSY)
			goto releout;
		error = VFS_VGET(mp, ino, LK_EXCLUSIVE, &nvp);
		if (error != 0)
			goto releout;
		VOP_UNLOCK(nvp);
		vrele(fvp);
		fvp = nvp;
		atomic_add_int(&rename_restarts, 1);
		goto relock;
	}
	vrele(fvp);
	fvp = nvp;
	/*
	 * Re-resolve tvp and acquire the vnode lock if present.
	 */
	error = ufs_lookup_ino(tdvp, NULL, tcnp, &ino);
	if (error != 0 && error != EJUSTRETURN) {
		VOP_UNLOCK(fdvp);
		VOP_UNLOCK(tdvp);
		VOP_UNLOCK(fvp);
		goto releout;
	}
	/*
	 * If tvp disappeared we just carry on.
	 */
	if (error == EJUSTRETURN && tvp != NULL) {
		vrele(tvp);
		tvp = NULL;
	}
	/*
	 * Get the tvp ino if the lookup succeeded.  We may have to restart
	 * if the non-blocking acquire fails.
	 */
	if (error == 0) {
		nvp = NULL;
		error = VFS_VGET(mp, ino, LK_EXCLUSIVE | LK_NOWAIT, &nvp);
		if (tvp)
			vrele(tvp);
		tvp = nvp;
		if (error) {
			VOP_UNLOCK(fdvp);
			VOP_UNLOCK(tdvp);
			VOP_UNLOCK(fvp);
			if (error != EBUSY)
				goto releout;
			error = VFS_VGET(mp, ino, LK_EXCLUSIVE, &nvp);
			if (error != 0)
				goto releout;
			vput(nvp);
			atomic_add_int(&rename_restarts, 1);
			goto relock;
		}
	}

	if (DOINGSUJ(fdvp) &&
	    (seqc_in_modify(fdvp_s) || !vn_seqc_consistent(fdvp, fdvp_s) ||
	     seqc_in_modify(fvp_s) || !vn_seqc_consistent(fvp, fvp_s) ||
	     seqc_in_modify(tdvp_s) || !vn_seqc_consistent(tdvp, tdvp_s) ||
	     (tvp != NULL && (seqc_in_modify(tvp_s) ||
	     !vn_seqc_consistent(tvp, tvp_s))))) {
		error = softdep_prerename(fdvp, fvp, tdvp, tvp);
		if (error != 0)
			goto releout;
	}

	fdp = VTOI(fdvp);
	fip = VTOI(fvp);
	tdp = VTOI(tdvp);
	tip = NULL;
	if (tvp)
		tip = VTOI(tvp);
	if (tvp && ((VTOI(tvp)->i_flags & (NOUNLINK | IMMUTABLE | APPEND)) ||
	    (VTOI(tdvp)->i_flags & APPEND))) {
		error = EPERM;
		goto unlockout;
	}
	/*
	 * Renaming a file to itself has no effect.  The upper layers should
	 * not call us in that case.  However, things could change after
	 * we drop the locks above.
	 */
	if (fvp == tvp) {
		error = 0;
		goto unlockout;
	}
	doingdirectory = 0;
	newparent = 0;
	ino = fip->i_number;
	if (fip->i_nlink >= UFS_LINK_MAX) {
		if (!DOINGSOFTDEP(fvp) || fip->i_effnlink >= UFS_LINK_MAX) {
			error = EMLINK;
			goto unlockout;
		}
		vfs_ref(mp);
		MPASS(!want_seqc_end);
		if (checkpath_locked) {
			sx_xunlock(&VFSTOUFS(mp)->um_checkpath_lock);
			checkpath_locked = false;
		}
		VOP_UNLOCK(fdvp);
		VOP_UNLOCK(fvp);
		vref(tdvp);
		if (tvp != NULL)
			vref(tvp);
		VOP_VPUT_PAIR(tdvp, &tvp, true);
		error = ufs_sync_nlink1(mp);
		vrele(fdvp);
		vrele(fvp);
		vrele(tdvp);
		if (tvp != NULL)
			vrele(tvp);
		return (error);
	}
	if ((fip->i_flags & (NOUNLINK | IMMUTABLE | APPEND))
	    || (fdp->i_flags & APPEND)) {
		error = EPERM;
		goto unlockout;
	}
	if ((fip->i_mode & IFMT) == IFDIR) {
		/*
		 * Avoid ".", "..", and aliases of "." for obvious reasons.
		 */
		if ((fcnp->cn_namelen == 1 && fcnp->cn_nameptr[0] == '.') ||
		    fdp == fip ||
		    (fcnp->cn_flags | tcnp->cn_flags) & ISDOTDOT) {
			error = EINVAL;
			goto unlockout;
		}
		if (fdp->i_number != tdp->i_number)
			newparent = tdp->i_number;
		doingdirectory = 1;
	}
	if ((fvp->v_type == VDIR && fvp->v_mountedhere != NULL) ||
	    (tvp != NULL && tvp->v_type == VDIR &&
	    tvp->v_mountedhere != NULL)) {
		error = EXDEV;
		goto unlockout;
	}

	/*
	 * If ".." must be changed (ie the directory gets a new
	 * parent) then the source directory must not be in the
	 * directory hierarchy above the target, as this would
	 * orphan everything below the source directory. Also
	 * the user must have write permission in the source so
	 * as to be able to change "..".
	 */
	if (doingdirectory && newparent) {
		error = VOP_ACCESS(fvp, VWRITE, tcnp->cn_cred, curthread);
		if (error)
			goto unlockout;

		sx_xlock(&VFSTOUFS(mp)->um_checkpath_lock);
		checkpath_locked = true;
		error = ufs_checkpath(ino, fdp->i_number, tdp, tcnp->cn_cred,
		    &ino);
		/*
		 * We encountered a lock that we have to wait for.  Unlock
		 * everything else and VGET before restarting.
		 */
		if (ino) {
			sx_xunlock(&VFSTOUFS(mp)->um_checkpath_lock);
			checkpath_locked = false;
			VOP_UNLOCK(fdvp);
			VOP_UNLOCK(fvp);
			VOP_UNLOCK(tdvp);
			if (tvp)
				VOP_UNLOCK(tvp);
			error = VFS_VGET(mp, ino, LK_SHARED, &nvp);
			if (error == 0)
				vput(nvp);
			atomic_add_int(&rename_restarts, 1);
			goto relock;
		}
		if (error)
			goto unlockout;
	}
	if (fip->i_effnlink == 0 || fdp->i_effnlink == 0 ||
	    tdp->i_effnlink == 0)
		panic("Bad effnlink fip %p, fdp %p, tdp %p", fip, fdp, tdp);

	if (tvp != NULL)
		vn_seqc_write_begin(tvp);
	vn_seqc_write_begin(tdvp);
	vn_seqc_write_begin(fvp);
	vn_seqc_write_begin(fdvp);
	want_seqc_end = true;

	/*
	 * 1) Bump link count while we're moving stuff
	 *    around.  If we crash somewhere before
	 *    completing our work, the link count
	 *    may be wrong, but correctable.
	 */
	fip->i_effnlink++;
	fip->i_nlink++;
	DIP_SET_NLINK(fip, fip->i_nlink);
	UFS_INODE_SET_FLAG(fip, IN_CHANGE);
	if (DOINGSOFTDEP(fvp))
		softdep_setup_link(tdp, fip);
	error = UFS_UPDATE(fvp, !DOINGSOFTDEP(fvp) && !DOINGASYNC(fvp));
	if (error)
		goto bad;

	/*
	 * 2) If target doesn't exist, link the target
	 *    to the source and unlink the source.
	 *    Otherwise, rewrite the target directory
	 *    entry to reference the source inode and
	 *    expunge the original entry's existence.
	 */
	if (tip == NULL) {
		if (ITODEV(tdp) != ITODEV(fip))
			panic("ufs_rename: EXDEV");
		if (doingdirectory && newparent) {
			/*
			 * Account for ".." in new directory.
			 * When source and destination have the same
			 * parent we don't adjust the link count.  The
			 * actual link modification is completed when
			 * .. is rewritten below.
			 */
			if (tdp->i_nlink >= UFS_LINK_MAX) {
				fip->i_effnlink--;
				fip->i_nlink--;
				DIP_SET_NLINK(fip, fip->i_nlink);
				UFS_INODE_SET_FLAG(fip, IN_CHANGE);
				if (DOINGSOFTDEP(fvp))
					softdep_revert_link(tdp, fip);
				if (!DOINGSOFTDEP(tdvp) ||
				    tdp->i_effnlink >= UFS_LINK_MAX) {
					error = EMLINK;
					goto unlockout;
				}
				MPASS(want_seqc_end);
				if (tvp != NULL)
					vn_seqc_write_end(tvp);
				vn_seqc_write_end(tdvp);
				vn_seqc_write_end(fvp);
				vn_seqc_write_end(fdvp);
				want_seqc_end = false;
				vfs_ref(mp);
				MPASS(checkpath_locked);
				sx_xunlock(&VFSTOUFS(mp)->um_checkpath_lock);
				checkpath_locked = false;
				VOP_UNLOCK(fdvp);
				VOP_UNLOCK(fvp);
				vref(tdvp);
				if (tvp != NULL)
					vref(tvp);
				VOP_VPUT_PAIR(tdvp, &tvp, true);
				error = ufs_sync_nlink1(mp);
				vrele(fdvp);
				vrele(fvp);
				vrele(tdvp);
				if (tvp != NULL)
					vrele(tvp);
				return (error);
			}
		}
		ufs_makedirentry(fip, tcnp, &newdir);
		error = ufs_direnter(tdvp, NULL, &newdir, tcnp, NULL);
		if (error)
			goto bad;
		/* Setup tdvp for directory compaction if needed. */
		if (I_COUNT(tdp) != 0 && I_ENDOFF(tdp) != 0 &&
		    I_ENDOFF(tdp) < tdp->i_size)
			endoff = I_ENDOFF(tdp);
	} else {
		if (ITODEV(tip) != ITODEV(tdp) || ITODEV(tip) != ITODEV(fip))
			panic("ufs_rename: EXDEV");
		/*
		 * Short circuit rename(foo, foo).
		 */
		if (tip->i_number == fip->i_number)
			panic("ufs_rename: same file");
		/*
		 * If the parent directory is "sticky", then the caller
		 * must possess VADMIN for the parent directory, or the
		 * destination of the rename.  This implements append-only
		 * directories.
		 */
		if ((tdp->i_mode & S_ISTXT) &&
		    VOP_ACCESS(tdvp, VADMIN, tcnp->cn_cred, td) &&
		    VOP_ACCESS(tvp, VADMIN, tcnp->cn_cred, td)) {
			error = EPERM;
			goto bad;
		}
		/*
		 * Target must be empty if a directory and have no links
		 * to it. Also, ensure source and target are compatible
		 * (both directories, or both not directories).
		 */
		if ((tip->i_mode & IFMT) == IFDIR) {
			if ((tip->i_effnlink > 2) ||
			    !ufs_dirempty(tip, tdp->i_number, tcnp->cn_cred,
			    (tcnp->cn_flags & IGNOREWHITEOUT) != 0)) {
				error = ENOTEMPTY;
				goto bad;
			}
			if (!doingdirectory) {
				error = ENOTDIR;
				goto bad;
			}
			cache_purge(tdvp);
		} else if (doingdirectory) {
			error = EISDIR;
			goto bad;
		}
		if (doingdirectory) {
			if (!newparent) {
				tdp->i_effnlink--;
				if (DOINGSOFTDEP(tdvp))
					softdep_change_linkcnt(tdp);
			}
			tip->i_effnlink--;
			if (DOINGSOFTDEP(tvp))
				softdep_change_linkcnt(tip);
		}
		error = ufs_dirrewrite(tdp, tip, fip->i_number,
		    IFTODT(fip->i_mode),
		    (doingdirectory && newparent) ? newparent : doingdirectory);
		if (error) {
			if (doingdirectory) {
				if (!newparent) {
					tdp->i_effnlink++;
					if (DOINGSOFTDEP(tdvp))
						softdep_change_linkcnt(tdp);
				}
				tip->i_effnlink++;
				if (DOINGSOFTDEP(tvp))
					softdep_change_linkcnt(tip);
			}
			goto bad;
		}
		if (doingdirectory && !DOINGSOFTDEP(tvp)) {
			/*
			 * The only stuff left in the directory is "."
			 * and "..". The "." reference is inconsequential
			 * since we are quashing it. We have removed the "."
			 * reference and the reference in the parent directory,
			 * but there may be other hard links. The soft
			 * dependency code will arrange to do these operations
			 * after the parent directory entry has been deleted on
			 * disk, so when running with that code we avoid doing
			 * them now.
			 */
			if (!newparent) {
				tdp->i_nlink--;
				DIP_SET_NLINK(tdp, tdp->i_nlink);
				UFS_INODE_SET_FLAG(tdp, IN_CHANGE);
			}
			tip->i_nlink--;
			DIP_SET_NLINK(tip, tip->i_nlink);
			UFS_INODE_SET_FLAG(tip, IN_CHANGE);
		}
	}

	/*
	 * 3) Unlink the source.  We have to resolve the path again to
	 * fixup the directory offset and count for ufs_dirremove.
	 */
	if (fdvp == tdvp) {
		error = ufs_lookup_ino(fdvp, NULL, fcnp, &ino);
		if (error)
			panic("ufs_rename: from entry went away!");
		if (ino != fip->i_number)
			panic("ufs_rename: ino mismatch %ju != %ju\n",
			    (uintmax_t)ino, (uintmax_t)fip->i_number);
	}
	/*
	 * If the source is a directory with a
	 * new parent, the link count of the old
	 * parent directory must be decremented
	 * and ".." set to point to the new parent.
	 */
	if (doingdirectory && newparent) {
		/*
		 * Set the directory depth based on its new parent.
		 */
		DIP_SET(fip, i_dirdepth, DIP(tdp, i_dirdepth) + 1);
		/*
		 * If tip exists we simply use its link, otherwise we must
		 * add a new one.
		 */
		if (tip == NULL) {
			tdp->i_effnlink++;
			tdp->i_nlink++;
			DIP_SET_NLINK(tdp, tdp->i_nlink);
			UFS_INODE_SET_FLAG(tdp, IN_CHANGE);
			if (DOINGSOFTDEP(tdvp))
				softdep_setup_dotdot_link(tdp, fip);
			error = UFS_UPDATE(tdvp, !DOINGSOFTDEP(tdvp) &&
			    !DOINGASYNC(tdvp));
			/* Don't go to bad here as the new link exists. */
			if (error)
				goto unlockout;
		} else if (DOINGSUJ(tdvp))
			/* Journal must account for each new link. */
			softdep_setup_dotdot_link(tdp, fip);
		SET_I_OFFSET(fip, mastertemplate.dot_reclen);
		if (ufs_dirrewrite(fip, fdp, newparent, DT_DIR, 0) != 0)
			ufs_dirbad(fip, mastertemplate.dot_reclen,
			    "rename: missing .. entry");
		cache_purge(fdvp);
	}
	error = ufs_dirremove(fdvp, fip, fcnp->cn_flags, 0);
	/*
	 * The kern_renameat() looks up the fvp using the DELETE flag, which
	 * causes the removal of the name cache entry for fvp.
	 * As the relookup of the fvp is done in two steps:
	 * ufs_lookup_ino() and then VFS_VGET(), another thread might do a
	 * normal lookup of the from name just before the VFS_VGET() call,
	 * causing the cache entry to be re-instantiated.
	 *
	 * The same issue also applies to tvp if it exists as
	 * otherwise we may have a stale name cache entry for the new
	 * name that references the old i-node if it has other links
	 * or open file descriptors.
	 */
	cache_vop_rename(fdvp, fvp, tdvp, tvp, fcnp, tcnp);

unlockout:
	if (want_seqc_end) {
		if (tvp != NULL)
			vn_seqc_write_end(tvp);
		vn_seqc_write_end(tdvp);
		vn_seqc_write_end(fvp);
		vn_seqc_write_end(fdvp);
	}

	if (checkpath_locked)
		sx_xunlock(&VFSTOUFS(mp)->um_checkpath_lock);

	vput(fdvp);
	vput(fvp);

	/*
	 * If compaction or fsync was requested do it in
	 * ffs_vput_pair() now that other locks are no longer needed.
	 */
	if (error == 0 && endoff != 0) {
		UFS_INODE_SET_FLAG(tdp, IN_ENDOFF);
		SET_I_ENDOFF(tdp, endoff);
	}
	VOP_VPUT_PAIR(tdvp, &tvp, true);
	return (error);

bad:
	fip->i_effnlink--;
	fip->i_nlink--;
	DIP_SET_NLINK(fip, fip->i_nlink);
	UFS_INODE_SET_FLAG(fip, IN_CHANGE);
	if (DOINGSOFTDEP(fvp))
		softdep_revert_link(tdp, fip);
	goto unlockout;

releout:
	if (want_seqc_end) {
		if (tvp != NULL)
			vn_seqc_write_end(tvp);
		vn_seqc_write_end(tdvp);
		vn_seqc_write_end(fvp);
		vn_seqc_write_end(fdvp);
	}

	vrele(fdvp);
	vrele(fvp);
	vrele(tdvp);
	if (tvp)
		vrele(tvp);

	return (error);
}

#ifdef UFS_ACL
static int
ufs_do_posix1e_acl_inheritance_dir(struct vnode *dvp, struct vnode *tvp,
    mode_t dmode, struct ucred *cred, struct thread *td)
{
	int error;
	struct inode *ip = VTOI(tvp);
	struct acl *dacl, *acl;

	acl = acl_alloc(M_WAITOK);
	dacl = acl_alloc(M_WAITOK);

	/*
	 * Retrieve default ACL from parent, if any.
	 */
	error = VOP_GETACL(dvp, ACL_TYPE_DEFAULT, acl, cred, td);
	switch (error) {
	case 0:
		/*
		 * Retrieved a default ACL, so merge mode and ACL if
		 * necessary.  If the ACL is empty, fall through to
		 * the "not defined or available" case.
		 */
		if (acl->acl_cnt != 0) {
			dmode = acl_posix1e_newfilemode(dmode, acl);
			UFS_INODE_SET_MODE(ip, dmode);
			DIP_SET(ip, i_mode, dmode);
			*dacl = *acl;
			ufs_sync_acl_from_inode(ip, acl);
			break;
		}
		/* FALLTHROUGH */

	case EOPNOTSUPP:
		/*
		 * Just use the mode as-is.
		 */
		UFS_INODE_SET_MODE(ip, dmode);
		DIP_SET(ip, i_mode, dmode);
		error = 0;
		goto out;

	default:
		goto out;
	}

	/*
	 * XXX: If we abort now, will Soft Updates notify the extattr
	 * code that the EAs for the file need to be released?
	 */
	error = VOP_SETACL(tvp, ACL_TYPE_ACCESS, acl, cred, td);
	if (error == 0)
		error = VOP_SETACL(tvp, ACL_TYPE_DEFAULT, dacl, cred, td);
	switch (error) {
	case 0:
		break;

	case EOPNOTSUPP:
		/*
		 * XXX: This should not happen, as EOPNOTSUPP above
		 * was supposed to free acl.
		 */
		printf("ufs_mkdir: VOP_GETACL() but no VOP_SETACL()\n");
		/*
		panic("ufs_mkdir: VOP_GETACL() but no VOP_SETACL()");
		 */
		break;

	default:
		goto out;
	}

out:
	acl_free(acl);
	acl_free(dacl);

	return (error);
}

static int
ufs_do_posix1e_acl_inheritance_file(struct vnode *dvp, struct vnode *tvp,
    mode_t mode, struct ucred *cred, struct thread *td)
{
	int error;
	struct inode *ip = VTOI(tvp);
	struct acl *acl;

	acl = acl_alloc(M_WAITOK);

	/*
	 * Retrieve default ACL for parent, if any.
	 */
	error = VOP_GETACL(dvp, ACL_TYPE_DEFAULT, acl, cred, td);
	switch (error) {
	case 0:
		/*
		 * Retrieved a default ACL, so merge mode and ACL if
		 * necessary.
		 */
		if (acl->acl_cnt != 0) {
			/*
			 * Two possible ways for default ACL to not
			 * be present.  First, the EA can be
			 * undefined, or second, the default ACL can
			 * be blank.  If it's blank, fall through to
			 * the it's not defined case.
			 */
			mode = acl_posix1e_newfilemode(mode, acl);
			UFS_INODE_SET_MODE(ip, mode);
			DIP_SET(ip, i_mode, mode);
			ufs_sync_acl_from_inode(ip, acl);
			break;
		}
		/* FALLTHROUGH */

	case EOPNOTSUPP:
		/*
		 * Just use the mode as-is.
		 */
		UFS_INODE_SET_MODE(ip, mode);
		DIP_SET(ip, i_mode, mode);
		error = 0;
		goto out;

	default:
		goto out;
	}

	/*
	 * XXX: If we abort now, will Soft Updates notify the extattr
	 * code that the EAs for the file need to be released?
	 */
	error = VOP_SETACL(tvp, ACL_TYPE_ACCESS, acl, cred, td);
	switch (error) {
	case 0:
		break;

	case EOPNOTSUPP:
		/*
		 * XXX: This should not happen, as EOPNOTSUPP above was
		 * supposed to free acl.
		 */
		printf("ufs_do_posix1e_acl_inheritance_file: VOP_GETACL() "
		    "but no VOP_SETACL()\n");
		/* panic("ufs_do_posix1e_acl_inheritance_file: VOP_GETACL() "
		    "but no VOP_SETACL()"); */
		break;

	default:
		goto out;
	}

out:
	acl_free(acl);

	return (error);
}

static int
ufs_do_nfs4_acl_inheritance(struct vnode *dvp, struct vnode *tvp,
    mode_t child_mode, struct ucred *cred, struct thread *td)
{
	int error;
	struct acl *parent_aclp, *child_aclp;

	parent_aclp = acl_alloc(M_WAITOK);
	child_aclp = acl_alloc(M_WAITOK | M_ZERO);

	error = ufs_getacl_nfs4_internal(dvp, parent_aclp, td);
	if (error)
		goto out;
	acl_nfs4_compute_inherited_acl(parent_aclp, child_aclp,
	    child_mode, VTOI(tvp)->i_uid, tvp->v_type == VDIR);
	error = ufs_setacl_nfs4_internal(tvp, child_aclp, td);
	if (error)
		goto out;
out:
	acl_free(parent_aclp);
	acl_free(child_aclp);

	return (error);
}
#endif

/*
 * Mkdir system call
 */
static int
ufs_mkdir(
	struct vop_mkdir_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap)
{
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	struct inode *ip, *dp;
	struct vnode *tvp;
	struct buf *bp;
	struct dirtemplate dirtemplate, *dtp;
	struct direct newdir;
	int error, dmode;
	long blkoff;

	dp = VTOI(dvp);
	error = ufs_sync_nlink(dvp, NULL);
	if (error != 0)
		goto out;
	dmode = vap->va_mode & 0777;
	dmode |= IFDIR;

	/*
	 * Must simulate part of ufs_makeinode here to acquire the inode,
	 * but not have it entered in the parent directory. The entry is
	 * made later after writing "." and ".." entries.
	 */
	if (dp->i_effnlink < 2) {
		print_bad_link_count("ufs_mkdir", dvp);
		error = EINVAL;
		goto out;
	}

	if (DOINGSUJ(dvp)) {
		error = softdep_prelink(dvp, NULL, cnp);
		if (error != 0) {
			MPASS(error == ERELOOKUP);
			return (error);
		}
	}

	error = UFS_VALLOC(dvp, dmode, cnp->cn_cred, &tvp);
	if (error)
		goto out;
	vn_seqc_write_begin(tvp);
	ip = VTOI(tvp);
	ip->i_gid = dp->i_gid;
	DIP_SET(ip, i_gid, dp->i_gid);
#ifdef SUIDDIR
	{
#ifdef QUOTA
		struct ucred ucred, *ucp;
		gid_t ucred_group;
		ucp = cnp->cn_cred;
#endif
		/*
		 * If we are hacking owners here, (only do this where told to)
		 * and we are not giving it TO root, (would subvert quotas)
		 * then go ahead and give it to the other user.
		 * The new directory also inherits the SUID bit.
		 * If user's UID and dir UID are the same,
		 * 'give it away' so that the SUID is still forced on.
		 */
		if ((dvp->v_mount->mnt_flag & MNT_SUIDDIR) &&
		    (dp->i_mode & ISUID) && dp->i_uid) {
			dmode |= ISUID;
			ip->i_uid = dp->i_uid;
			DIP_SET(ip, i_uid, dp->i_uid);
#ifdef QUOTA
			if (dp->i_uid != cnp->cn_cred->cr_uid) {
				/*
				 * Make sure the correct user gets charged
				 * for the space.
				 * Make a dummy credential for the victim.
				 * XXX This seems to never be accessed out of
				 * our context so a stack variable is ok.
				 */
				ucred.cr_ref = 1;
				ucred.cr_uid = ip->i_uid;
				ucred.cr_ngroups = 1;
				ucred.cr_groups = &ucred_group;
				ucred.cr_groups[0] = dp->i_gid;
				ucp = &ucred;
			}
#endif
		} else {
			ip->i_uid = cnp->cn_cred->cr_uid;
			DIP_SET(ip, i_uid, ip->i_uid);
		}
#ifdef QUOTA
		if ((error = getinoquota(ip)) ||
	    	    (error = chkiq(ip, 1, ucp, 0))) {
			if (DOINGSOFTDEP(tvp))
				softdep_revert_link(dp, ip);
			UFS_VFREE(tvp, ip->i_number, dmode);
			vn_seqc_write_end(tvp);
			vgone(tvp);
			vput(tvp);
			return (error);
		}
#endif
	}
#else	/* !SUIDDIR */
	ip->i_uid = cnp->cn_cred->cr_uid;
	DIP_SET(ip, i_uid, ip->i_uid);
#ifdef QUOTA
	if ((error = getinoquota(ip)) ||
	    (error = chkiq(ip, 1, cnp->cn_cred, 0))) {
		if (DOINGSOFTDEP(tvp))
			softdep_revert_link(dp, ip);
		UFS_VFREE(tvp, ip->i_number, dmode);
		vn_seqc_write_end(tvp);
		vgone(tvp);
		vput(tvp);
		return (error);
	}
#endif
#endif	/* !SUIDDIR */
	UFS_INODE_SET_FLAG(ip, IN_ACCESS | IN_CHANGE | IN_UPDATE);
	UFS_INODE_SET_MODE(ip, dmode);
	DIP_SET(ip, i_mode, dmode);
	tvp->v_type = VDIR;	/* Rest init'd in getnewvnode(). */
	ip->i_effnlink = 2;
	ip->i_nlink = 2;
	DIP_SET_NLINK(ip, 2);
	DIP_SET(ip, i_dirdepth, DIP(dp,i_dirdepth) + 1);

	if (cnp->cn_flags & ISWHITEOUT) {
		ip->i_flags |= UF_OPAQUE;
		DIP_SET(ip, i_flags, ip->i_flags);
	}

	/*
	 * Bump link count in parent directory to reflect work done below.
	 * Should be done before reference is created so cleanup is
	 * possible if we crash.
	 */
	dp->i_effnlink++;
	dp->i_nlink++;
	DIP_SET_NLINK(dp, dp->i_nlink);
	UFS_INODE_SET_FLAG(dp, IN_CHANGE);
	if (DOINGSOFTDEP(dvp))
		softdep_setup_mkdir(dp, ip);
	error = UFS_UPDATE(dvp, !DOINGSOFTDEP(dvp) && !DOINGASYNC(dvp));
	if (error)
		goto bad;
#ifdef MAC
	if (dvp->v_mount->mnt_flag & MNT_MULTILABEL) {
		error = mac_vnode_create_extattr(cnp->cn_cred, dvp->v_mount,
		    dvp, tvp, cnp);
		if (error)
			goto bad;
	}
#endif
#ifdef UFS_ACL
	if (dvp->v_mount->mnt_flag & MNT_ACLS) {
		error = ufs_do_posix1e_acl_inheritance_dir(dvp, tvp, dmode,
		    cnp->cn_cred, curthread);
		if (error)
			goto bad;
	} else if (dvp->v_mount->mnt_flag & MNT_NFS4ACLS) {
		error = ufs_do_nfs4_acl_inheritance(dvp, tvp, dmode,
		    cnp->cn_cred, curthread);
		if (error)
			goto bad;
	}
#endif /* !UFS_ACL */

	/*
	 * Initialize directory with "." and ".." from static template.
	 */
	if (!OFSFMT(dvp))
		dtp = &mastertemplate;
	else
		dtp = (struct dirtemplate *)&omastertemplate;
	dirtemplate = *dtp;
	dirtemplate.dot_ino = ip->i_number;
	dirtemplate.dotdot_ino = dp->i_number;
	vnode_pager_setsize(tvp, DIRBLKSIZ);
	if ((error = UFS_BALLOC(tvp, (off_t)0, DIRBLKSIZ, cnp->cn_cred,
	    BA_CLRBUF, &bp)) != 0)
		goto bad;
	ip->i_size = DIRBLKSIZ;
	DIP_SET(ip, i_size, DIRBLKSIZ);
	UFS_INODE_SET_FLAG(ip, IN_SIZEMOD | IN_CHANGE | IN_UPDATE);
	bcopy((caddr_t)&dirtemplate, (caddr_t)bp->b_data, sizeof dirtemplate);
	if (DOINGSOFTDEP(tvp)) {
		/*
		 * Ensure that the entire newly allocated block is a
		 * valid directory so that future growth within the
		 * block does not have to ensure that the block is
		 * written before the inode.
		 */
		blkoff = DIRBLKSIZ;
		while (blkoff < bp->b_bcount) {
			((struct direct *)
			   (bp->b_data + blkoff))->d_reclen = DIRBLKSIZ;
			blkoff += DIRBLKSIZ;
		}
	}
	if ((error = UFS_UPDATE(tvp, !DOINGSOFTDEP(tvp) &&
	    !DOINGASYNC(tvp))) != 0) {
		(void)bwrite(bp);
		goto bad;
	}
	/*
	 * Directory set up, now install its entry in the parent directory.
	 *
	 * If we are not doing soft dependencies, then we must write out the
	 * buffer containing the new directory body before entering the new 
	 * name in the parent. If we are doing soft dependencies, then the
	 * buffer containing the new directory body will be passed to and
	 * released in the soft dependency code after the code has attached
	 * an appropriate ordering dependency to the buffer which ensures that
	 * the buffer is written before the new name is written in the parent.
	 */
	if (DOINGASYNC(dvp))
		bdwrite(bp);
	else if (!DOINGSOFTDEP(dvp) && ((error = bwrite(bp))))
		goto bad;
	ufs_makedirentry(ip, cnp, &newdir);
	error = ufs_direnter(dvp, tvp, &newdir, cnp, bp);

bad:
	if (error == 0) {
		*ap->a_vpp = tvp;
		vn_seqc_write_end(tvp);
	} else {
		dp->i_effnlink--;
		dp->i_nlink--;
		DIP_SET_NLINK(dp, dp->i_nlink);
		UFS_INODE_SET_FLAG(dp, IN_CHANGE);
		/*
		 * No need to do an explicit VOP_TRUNCATE here, vrele will
		 * do this for us because we set the link count to 0.
		 */
		ip->i_effnlink = 0;
		ip->i_nlink = 0;
		DIP_SET_NLINK(ip, 0);
		UFS_INODE_SET_FLAG(ip, IN_CHANGE);
		if (DOINGSOFTDEP(tvp))
			softdep_revert_mkdir(dp, ip);
		vn_seqc_write_end(tvp);
		vgone(tvp);
		vput(tvp);
	}
out:
	return (error);
}

/*
 * Rmdir system call.
 */
static int
ufs_rmdir(
	struct vop_rmdir_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap)
{
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct inode *ip, *dp;
	int error;

	ip = VTOI(vp);
	dp = VTOI(dvp);

	/*
	 * Do not remove a directory that is in the process of being renamed.
	 * Verify the directory is empty (and valid). Rmdir ".." will not be
	 * valid since ".." will contain a reference to the current directory
	 * and thus be non-empty. Do not allow the removal of mounted on
	 * directories (this can happen when an NFS exported filesystem
	 * tries to remove a locally mounted on directory).
	 */
	error = 0;
	if (dp->i_effnlink <= 2) {
		if (dp->i_effnlink == 2)
			print_bad_link_count("ufs_rmdir", dvp);
		error = EINVAL;
		goto out;
	}
	if (!ufs_dirempty(ip, dp->i_number, cnp->cn_cred,
	    (cnp->cn_flags & IGNOREWHITEOUT) != 0)) {
		error = ENOTEMPTY;
		goto out;
	}
	if ((dp->i_flags & APPEND)
	    || (ip->i_flags & (NOUNLINK | IMMUTABLE | APPEND))) {
		error = EPERM;
		goto out;
	}
	if (vp->v_mountedhere != 0) {
		error = EINVAL;
		goto out;
	}
	if (DOINGSUJ(dvp)) {
		error = softdep_prelink(dvp, vp, cnp);
		if (error != 0) {
			MPASS(error == ERELOOKUP);
			return (error);
		}
	}

#ifdef UFS_GJOURNAL
	ufs_gjournal_orphan(vp);
#endif
	/*
	 * Delete reference to directory before purging
	 * inode.  If we crash in between, the directory
	 * will be reattached to lost+found,
	 */
	dp->i_effnlink--;
	ip->i_effnlink--;
	if (DOINGSOFTDEP(vp))
		softdep_setup_rmdir(dp, ip);
	error = ufs_dirremove(dvp, ip, cnp->cn_flags, 1);
	if (error) {
		dp->i_effnlink++;
		ip->i_effnlink++;
		if (DOINGSOFTDEP(vp))
			softdep_revert_rmdir(dp, ip);
		goto out;
	}
	/*
	 * The only stuff left in the directory is "." and "..". The "."
	 * reference is inconsequential since we are quashing it. The soft
	 * dependency code will arrange to do these operations after
	 * the parent directory entry has been deleted on disk, so
	 * when running with that code we avoid doing them now.
	 */
	if (!DOINGSOFTDEP(vp)) {
		dp->i_nlink--;
		DIP_SET_NLINK(dp, dp->i_nlink);
		UFS_INODE_SET_FLAG(dp, IN_CHANGE);
		error = UFS_UPDATE(dvp, 0);
		ip->i_nlink--;
		DIP_SET_NLINK(ip, ip->i_nlink);
		UFS_INODE_SET_FLAG(ip, IN_CHANGE);
	}
	cache_vop_rmdir(dvp, vp);
#ifdef UFS_DIRHASH
	/* Kill any active hash; i_effnlink == 0, so it will not come back. */
	if (ip->i_dirhash != NULL)
		ufsdirhash_free(ip);
#endif
out:
	return (error);
}

/*
 * symlink -- make a symbolic link
 */
static int
ufs_symlink(
	struct vop_symlink_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		const char *a_target;
	} */ *ap)
{
	struct vnode *vp, **vpp = ap->a_vpp;
	struct inode *ip;
	int len, error;

	error = ufs_makeinode(IFLNK | ap->a_vap->va_mode, ap->a_dvp,
	    vpp, ap->a_cnp, "ufs_symlink");
	if (error)
		return (error);
	vp = *vpp;
	len = strlen(ap->a_target);
	if (len < VFSTOUFS(vp->v_mount)->um_maxsymlinklen) {
		ip = VTOI(vp);
		bcopy(ap->a_target, DIP(ip, i_shortlink), len);
		ip->i_size = len;
		DIP_SET(ip, i_size, len);
		UFS_INODE_SET_FLAG(ip, IN_SIZEMOD | IN_CHANGE | IN_UPDATE);
		error = UFS_UPDATE(vp, 0);
	} else
		error = vn_rdwr(UIO_WRITE, vp, __DECONST(void *, ap->a_target),
		    len, (off_t)0, UIO_SYSSPACE, IO_NODELOCKED | IO_NOMACCHECK,
		    ap->a_cnp->cn_cred, NOCRED, NULL, NULL);
	if (error)
		vput(vp);
	return (error);
}

/*
 * Vnode op for reading directories.
 */
int
ufs_readdir(
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		int *a_ncookies;
		uint64_t **a_cookies;
	} */ *ap)
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct buf *bp;
	struct inode *ip;
	struct direct *dp, *edp;
	uint64_t *cookies;
	struct dirent dstdp;
	off_t offset, startoffset;
	size_t readcnt, skipcnt;
	ssize_t startresid;
	uint64_t ncookies;
	int error;

	if (uio->uio_offset < 0)
		return (EINVAL);
	ip = VTOI(vp);
	if (ip->i_effnlink == 0) {
		*ap->a_eofflag = 1;
		return (0);
	}
	if (ap->a_ncookies != NULL) {
		if (uio->uio_resid < 0)
			ncookies = 0;
		else
			ncookies = uio->uio_resid;
		if (uio->uio_offset >= ip->i_size)
			ncookies = 0;
		else if (ip->i_size - uio->uio_offset < ncookies)
			ncookies = ip->i_size - uio->uio_offset;
		ncookies = ncookies / (offsetof(struct direct, d_name) + 4) + 1;
		cookies = malloc(ncookies * sizeof(*cookies), M_TEMP, M_WAITOK);
		*ap->a_ncookies = ncookies;
		*ap->a_cookies = cookies;
	} else {
		ncookies = 0;
		cookies = NULL;
	}
	offset = startoffset = uio->uio_offset;
	startresid = uio->uio_resid;
	error = 0;
	while (error == 0 && uio->uio_resid > 0 &&
	    uio->uio_offset < ip->i_size) {
		error = UFS_BLKATOFF(vp, uio->uio_offset, NULL, &bp);
		if (error)
			break;
		if (bp->b_offset + bp->b_bcount > ip->i_size)
			readcnt = ip->i_size - bp->b_offset;
		else
			readcnt = bp->b_bcount;
		skipcnt = (size_t)(uio->uio_offset - bp->b_offset) &
		    ~(size_t)(DIRBLKSIZ - 1);
		offset = bp->b_offset + skipcnt;
		dp = (struct direct *)&bp->b_data[skipcnt];
		edp = (struct direct *)&bp->b_data[readcnt];
		while (error == 0 && uio->uio_resid > 0 && dp < edp) {
			if (dp->d_reclen <= offsetof(struct direct, d_name) ||
			    (caddr_t)dp + dp->d_reclen > (caddr_t)edp) {
				error = EIO;
				break;
			}
#if BYTE_ORDER == LITTLE_ENDIAN
			/* Old filesystem format. */
			if (OFSFMT(vp)) {
				dstdp.d_namlen = dp->d_type;
				dstdp.d_type = dp->d_namlen;
			} else
#endif
			{
				dstdp.d_namlen = dp->d_namlen;
				dstdp.d_type = dp->d_type;
			}
			if (offsetof(struct direct, d_name) + dstdp.d_namlen >
			    dp->d_reclen) {
				error = EIO;
				break;
			}
			if (offset < startoffset || dp->d_ino == 0)
				goto nextentry;
			dstdp.d_fileno = dp->d_ino;
			dstdp.d_reclen = GENERIC_DIRSIZ(&dstdp);
			bcopy(dp->d_name, dstdp.d_name, dstdp.d_namlen);
			/* NOTE: d_off is the offset of the *next* entry. */
			dstdp.d_off = offset + dp->d_reclen;
			dirent_terminate(&dstdp);
			if (dstdp.d_reclen > uio->uio_resid) {
				if (uio->uio_resid == startresid)
					error = EINVAL;
				else
					error = EJUSTRETURN;
				break;
			}
			/* Advance dp. */
			error = uiomove((caddr_t)&dstdp, dstdp.d_reclen, uio);
			if (error)
				break;
			if (cookies != NULL) {
				KASSERT(ncookies > 0,
				    ("ufs_readdir: cookies buffer too small"));
				*cookies = offset + dp->d_reclen;
				cookies++;
				ncookies--;
			}
nextentry:
			offset += dp->d_reclen;
			dp = (struct direct *)((caddr_t)dp + dp->d_reclen);
		}
		bqrelse(bp);
		uio->uio_offset = offset;
	}
	/* We need to correct uio_offset. */
	uio->uio_offset = offset;
	if (error == EJUSTRETURN)
		error = 0;
	if (ap->a_ncookies != NULL) {
		if (error == 0) {
			*ap->a_ncookies -= ncookies;
		} else {
			free(*ap->a_cookies, M_TEMP);
			*ap->a_ncookies = 0;
			*ap->a_cookies = NULL;
		}
	}
	if (error == 0 && ap->a_eofflag)
		*ap->a_eofflag = ip->i_size <= uio->uio_offset;
	return (error);
}

/*
 * Return target name of a symbolic link
 */
static int
ufs_readlink(
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap)
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	doff_t isize;

	isize = ip->i_size;
	if (isize < VFSTOUFS(vp->v_mount)->um_maxsymlinklen)
		return (uiomove(DIP(ip, i_shortlink), isize, ap->a_uio));
	return (VOP_READ(vp, ap->a_uio, 0, ap->a_cred));
}

/*
 * Calculate the logical to physical mapping if not done already,
 * then call the device strategy routine.
 *
 * In order to be able to swap to a file, the ufs_bmaparray() operation may not
 * deadlock on memory.  See ufs_bmap() for details.
 */
static int
ufs_strategy(
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap)
{
	struct buf *bp = ap->a_bp;
	struct vnode *vp = ap->a_vp;
	ufs2_daddr_t blkno;
	int error;

	if (bp->b_blkno == bp->b_lblkno) {
		error = ufs_bmaparray(vp, bp->b_lblkno, &blkno, bp, NULL, NULL);
		bp->b_blkno = blkno;
		if (error) {
			bp->b_error = error;
			bp->b_ioflags |= BIO_ERROR;
			bufdone(bp);
			return (0);
		}
		if ((long)bp->b_blkno == -1)
			vfs_bio_clrbuf(bp);
	}
	if ((long)bp->b_blkno == -1) {
		bufdone(bp);
		return (0);
	}
	bp->b_iooffset = dbtob(bp->b_blkno);
	BO_STRATEGY(VFSTOUFS(vp->v_mount)->um_bo, bp);
	return (0);
}

/*
 * Print out the contents of an inode.
 */
static int
ufs_print(
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap)
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);

	printf("\tnlink=%d, effnlink=%d, size=%jd", ip->i_nlink,
	    ip->i_effnlink, (intmax_t)ip->i_size);
	if (I_IS_UFS2(ip))
		printf(", extsize %d", ip->i_din2->di_extsize);
	printf("\n\tgeneration=%jx, uid=%d, gid=%d, flags=0x%b\n",
	    (uintmax_t)ip->i_gen, ip->i_uid, ip->i_gid,
	    (uint32_t)ip->i_flags, PRINT_INODE_FLAGS);
	printf("\tino %ju, on dev %s", (intmax_t)ip->i_number,
	    devtoname(ITODEV(ip)));
	if (vp->v_type == VFIFO)
		fifo_printinfo(vp);
	printf("\n");
	return (0);
}

/*
 * Close wrapper for fifos.
 *
 * Update the times on the inode then do device close.
 */
static int
ufsfifo_close(
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap)
{

	ufs_close(ap);
	return (fifo_specops.vop_close(ap));
}

/*
 * Return POSIX pathconf information applicable to ufs filesystems.
 */
static int
ufs_pathconf(
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		int *a_retval;
	} */ *ap)
{
	int error;

	error = 0;
	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = UFS_LINK_MAX;
		break;
	case _PC_NAME_MAX:
		*ap->a_retval = UFS_MAXNAMLEN;
		break;
	case _PC_PIPE_BUF:
		if (ap->a_vp->v_type == VDIR || ap->a_vp->v_type == VFIFO)
			*ap->a_retval = PIPE_BUF;
		else
			error = EINVAL;
		break;
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		break;
	case _PC_NO_TRUNC:
		*ap->a_retval = 1;
		break;
#ifdef UFS_ACL
	case _PC_ACL_EXTENDED:
		if (ap->a_vp->v_mount->mnt_flag & MNT_ACLS)
			*ap->a_retval = 1;
		else
			*ap->a_retval = 0;
		break;
	case _PC_ACL_NFS4:
		if (ap->a_vp->v_mount->mnt_flag & MNT_NFS4ACLS)
			*ap->a_retval = 1;
		else
			*ap->a_retval = 0;
		break;
#endif
	case _PC_ACL_PATH_MAX:
#ifdef UFS_ACL
		if (ap->a_vp->v_mount->mnt_flag & (MNT_ACLS | MNT_NFS4ACLS))
			*ap->a_retval = ACL_MAX_ENTRIES;
		else
			*ap->a_retval = 3;
#else
		*ap->a_retval = 3;
#endif
		break;
#ifdef MAC
	case _PC_MAC_PRESENT:
		if (ap->a_vp->v_mount->mnt_flag & MNT_MULTILABEL)
			*ap->a_retval = 1;
		else
			*ap->a_retval = 0;
		break;
#endif
	case _PC_MIN_HOLE_SIZE:
		*ap->a_retval = ap->a_vp->v_mount->mnt_stat.f_iosize;
		break;
	case _PC_PRIO_IO:
		*ap->a_retval = 0;
		break;
	case _PC_SYNC_IO:
		*ap->a_retval = 0;
		break;
	case _PC_ALLOC_SIZE_MIN:
		*ap->a_retval = ap->a_vp->v_mount->mnt_stat.f_bsize;
		break;
	case _PC_FILESIZEBITS:
		*ap->a_retval = 64;
		break;
	case _PC_REC_INCR_XFER_SIZE:
		*ap->a_retval = ap->a_vp->v_mount->mnt_stat.f_iosize;
		break;
	case _PC_REC_MAX_XFER_SIZE:
		*ap->a_retval = -1; /* means ``unlimited'' */
		break;
	case _PC_REC_MIN_XFER_SIZE:
		*ap->a_retval = ap->a_vp->v_mount->mnt_stat.f_iosize;
		break;
	case _PC_REC_XFER_ALIGN:
		*ap->a_retval = PAGE_SIZE;
		break;
	case _PC_SYMLINK_MAX:
		*ap->a_retval = MAXPATHLEN;
		break;

	default:
		error = vop_stdpathconf(ap);
		break;
	}
	return (error);
}

/*
 * Initialize the vnode associated with a new inode, handle aliased
 * vnodes.
 */
int
ufs_vinit(struct mount *mntp, struct vop_vector *fifoops, struct vnode **vpp)
{
	struct inode *ip;
	struct vnode *vp;

	vp = *vpp;
	ASSERT_VOP_LOCKED(vp, "ufs_vinit");
	ip = VTOI(vp);
	vp->v_type = IFTOVT(ip->i_mode);
	/*
	 * Only unallocated inodes should be of type VNON.
	 */
	if (ip->i_mode != 0 && vp->v_type == VNON)
		return (EINVAL);
	if (vp->v_type == VFIFO)
		vp->v_op = fifoops;
	if (ip->i_number == UFS_ROOTINO)
		vp->v_vflag |= VV_ROOT;
	*vpp = vp;
	return (0);
}

/*
 * Allocate a new inode.
 * Vnode dvp must be locked.
 */
static int
ufs_makeinode(int mode, struct vnode *dvp, struct vnode **vpp,
    struct componentname *cnp, const char *callfunc)
{
	struct inode *ip, *pdir;
	struct direct newdir;
	struct vnode *tvp;
	int error;

	pdir = VTOI(dvp);
	*vpp = NULL;
	if ((mode & IFMT) == 0)
		mode |= IFREG;

	if (pdir->i_effnlink < 2) {
		print_bad_link_count(callfunc, dvp);
		return (EINVAL);
	}
	if (DOINGSUJ(dvp)) {
		error = softdep_prelink(dvp, NULL, cnp);
		if (error != 0) {
			MPASS(error == ERELOOKUP);
			return (error);
		}
	}
	error = UFS_VALLOC(dvp, mode, cnp->cn_cred, &tvp);
	if (error)
		return (error);
	ip = VTOI(tvp);
	ip->i_gid = pdir->i_gid;
	DIP_SET(ip, i_gid, pdir->i_gid);
#ifdef SUIDDIR
	{
#ifdef QUOTA
		struct ucred ucred, *ucp;
		gid_t ucred_group;
		ucp = cnp->cn_cred;
#endif
		/*
		 * If we are not the owner of the directory,
		 * and we are hacking owners here, (only do this where told to)
		 * and we are not giving it TO root, (would subvert quotas)
		 * then go ahead and give it to the other user.
		 * Note that this drops off the execute bits for security.
		 */
		if ((dvp->v_mount->mnt_flag & MNT_SUIDDIR) &&
		    (pdir->i_mode & ISUID) &&
		    (pdir->i_uid != cnp->cn_cred->cr_uid) && pdir->i_uid) {
			ip->i_uid = pdir->i_uid;
			DIP_SET(ip, i_uid, ip->i_uid);
			mode &= ~07111;
#ifdef QUOTA
			/*
			 * Make sure the correct user gets charged
			 * for the space.
			 * Quickly knock up a dummy credential for the victim.
			 * XXX This seems to never be accessed out of our
			 * context so a stack variable is ok.
			 */
			ucred.cr_ref = 1;
			ucred.cr_uid = ip->i_uid;
			ucred.cr_ngroups = 1;
			ucred.cr_groups = &ucred_group;
			ucred.cr_groups[0] = pdir->i_gid;
			ucp = &ucred;
#endif
		} else {
			ip->i_uid = cnp->cn_cred->cr_uid;
			DIP_SET(ip, i_uid, ip->i_uid);
		}

#ifdef QUOTA
		if ((error = getinoquota(ip)) ||
	    	    (error = chkiq(ip, 1, ucp, 0))) {
			if (DOINGSOFTDEP(tvp))
				softdep_revert_link(pdir, ip);
			UFS_VFREE(tvp, ip->i_number, mode);
			vgone(tvp);
			vput(tvp);
			return (error);
		}
#endif
	}
#else	/* !SUIDDIR */
	ip->i_uid = cnp->cn_cred->cr_uid;
	DIP_SET(ip, i_uid, ip->i_uid);
#ifdef QUOTA
	if ((error = getinoquota(ip)) ||
	    (error = chkiq(ip, 1, cnp->cn_cred, 0))) {
		if (DOINGSOFTDEP(tvp))
			softdep_revert_link(pdir, ip);
		UFS_VFREE(tvp, ip->i_number, mode);
		vgone(tvp);
		vput(tvp);
		return (error);
	}
#endif
#endif	/* !SUIDDIR */
	vn_seqc_write_begin(tvp); /* Mostly to cover asserts */
	UFS_INODE_SET_FLAG(ip, IN_ACCESS | IN_CHANGE | IN_UPDATE);
	UFS_INODE_SET_MODE(ip, mode);
	DIP_SET(ip, i_mode, mode);
	tvp->v_type = IFTOVT(mode);	/* Rest init'd in getnewvnode(). */
	ip->i_effnlink = 1;
	ip->i_nlink = 1;
	DIP_SET_NLINK(ip, 1);
	if (DOINGSOFTDEP(tvp))
		softdep_setup_create(VTOI(dvp), ip);
	if ((ip->i_mode & ISGID) && !groupmember(ip->i_gid, cnp->cn_cred) &&
	    priv_check_cred(cnp->cn_cred, PRIV_VFS_SETGID)) {
		UFS_INODE_SET_MODE(ip, ip->i_mode & ~ISGID);
		DIP_SET(ip, i_mode, ip->i_mode);
	}

	if (cnp->cn_flags & ISWHITEOUT) {
		ip->i_flags |= UF_OPAQUE;
		DIP_SET(ip, i_flags, ip->i_flags);
	}

	/*
	 * Make sure inode goes to disk before directory entry.
	 */
	error = UFS_UPDATE(tvp, !DOINGSOFTDEP(tvp) && !DOINGASYNC(tvp));
	if (error)
		goto bad;
#ifdef MAC
	if (dvp->v_mount->mnt_flag & MNT_MULTILABEL) {
		error = mac_vnode_create_extattr(cnp->cn_cred, dvp->v_mount,
		    dvp, tvp, cnp);
		if (error)
			goto bad;
	}
#endif
#ifdef UFS_ACL
	if (dvp->v_mount->mnt_flag & MNT_ACLS) {
		error = ufs_do_posix1e_acl_inheritance_file(dvp, tvp, mode,
		    cnp->cn_cred, curthread);
		if (error)
			goto bad;
	} else if (dvp->v_mount->mnt_flag & MNT_NFS4ACLS) {
		error = ufs_do_nfs4_acl_inheritance(dvp, tvp, mode,
		    cnp->cn_cred, curthread);
		if (error)
			goto bad;
	}
#endif /* !UFS_ACL */
	ufs_makedirentry(ip, cnp, &newdir);
	error = ufs_direnter(dvp, tvp, &newdir, cnp, NULL);
	if (error)
		goto bad;
	vn_seqc_write_end(tvp);
	*vpp = tvp;
	return (0);

bad:
	/*
	 * Write error occurred trying to update the inode
	 * or the directory so must deallocate the inode.
	 */
	ip->i_effnlink = 0;
	ip->i_nlink = 0;
	DIP_SET_NLINK(ip, 0);
	UFS_INODE_SET_FLAG(ip, IN_CHANGE);
	if (DOINGSOFTDEP(tvp))
		softdep_revert_create(VTOI(dvp), ip);
	vn_seqc_write_end(tvp);
	vgone(tvp);
	vput(tvp);
	return (error);
}

static int
ufs_ioctl(struct vop_ioctl_args *ap)
{
	struct vnode *vp;
	int error;

	vp = ap->a_vp;
	switch (ap->a_command) {
	case FIOSEEKDATA:
		error = vn_lock(vp, LK_EXCLUSIVE);
		if (error == 0) {
			error = ufs_bmap_seekdata(vp, (off_t *)ap->a_data);
			VOP_UNLOCK(vp);
		} else
			error = EBADF;
		return (error);
	case FIOSEEKHOLE:
		return (vn_bmap_seekhole(vp, ap->a_command, (off_t *)ap->a_data,
		    ap->a_cred));
	default:
		return (ENOTTY);
	}
}

static int
ufs_read_pgcache(struct vop_read_pgcache_args *ap)
{
	struct uio *uio;
	struct vnode *vp;

	uio = ap->a_uio;
	vp = ap->a_vp;
	VNPASS((vn_irflag_read(vp) & VIRF_PGREAD) != 0, vp);

	if (uio->uio_resid > ptoa(io_hold_cnt) || uio->uio_offset < 0 ||
	    (ap->a_ioflag & IO_DIRECT) != 0)
		return (EJUSTRETURN);
	return (vn_read_from_obj(vp, uio));
}

/* Global vfs data structures for ufs. */
struct vop_vector ufs_vnodeops = {
	.vop_default =		&default_vnodeops,
	.vop_fsync =		VOP_PANIC,
	.vop_read =		VOP_PANIC,
	.vop_reallocblks =	VOP_PANIC,
	.vop_write =		VOP_PANIC,
	.vop_accessx =		ufs_accessx,
	.vop_bmap =		ufs_bmap,
	.vop_fplookup_vexec =	ufs_fplookup_vexec,
	.vop_fplookup_symlink =	VOP_EAGAIN,
	.vop_cachedlookup =	ufs_lookup,
	.vop_close =		ufs_close,
	.vop_create =		ufs_create,
	.vop_stat =		ufs_stat,
	.vop_getattr =		ufs_getattr,
	.vop_inactive =		ufs_inactive,
	.vop_ioctl =		ufs_ioctl,
	.vop_link =		ufs_link,
	.vop_lookup =		vfs_cache_lookup,
	.vop_mmapped =		ufs_mmapped,
	.vop_mkdir =		ufs_mkdir,
	.vop_mknod =		ufs_mknod,
	.vop_need_inactive =	ufs_need_inactive,
	.vop_open =		ufs_open,
	.vop_pathconf =		ufs_pathconf,
	.vop_poll =		vop_stdpoll,
	.vop_print =		ufs_print,
	.vop_read_pgcache =	ufs_read_pgcache,
	.vop_readdir =		ufs_readdir,
	.vop_readlink =		ufs_readlink,
	.vop_reclaim =		ufs_reclaim,
	.vop_remove =		ufs_remove,
	.vop_rename =		ufs_rename,
	.vop_rmdir =		ufs_rmdir,
	.vop_setattr =		ufs_setattr,
#ifdef MAC
	.vop_setlabel =		vop_stdsetlabel_ea,
#endif
	.vop_strategy =		ufs_strategy,
	.vop_symlink =		ufs_symlink,
	.vop_whiteout =		ufs_whiteout,
#ifdef UFS_EXTATTR
	.vop_getextattr =	ufs_getextattr,
	.vop_deleteextattr =	ufs_deleteextattr,
	.vop_setextattr =	ufs_setextattr,
#endif
#ifdef UFS_ACL
	.vop_getacl =		ufs_getacl,
	.vop_setacl =		ufs_setacl,
	.vop_aclcheck =		ufs_aclcheck,
#endif
};
VFS_VOP_VECTOR_REGISTER(ufs_vnodeops);

struct vop_vector ufs_fifoops = {
	.vop_default =		&fifo_specops,
	.vop_fsync =		VOP_PANIC,
	.vop_accessx =		ufs_accessx,
	.vop_close =		ufsfifo_close,
	.vop_getattr =		ufs_getattr,
	.vop_inactive =		ufs_inactive,
	.vop_pathconf = 	ufs_pathconf,
	.vop_print =		ufs_print,
	.vop_read =		VOP_PANIC,
	.vop_reclaim =		ufs_reclaim,
	.vop_setattr =		ufs_setattr,
#ifdef MAC
	.vop_setlabel =		vop_stdsetlabel_ea,
#endif
	.vop_write =		VOP_PANIC,
#ifdef UFS_EXTATTR
	.vop_getextattr =	ufs_getextattr,
	.vop_deleteextattr =	ufs_deleteextattr,
	.vop_setextattr =	ufs_setextattr,
#endif
#ifdef UFS_ACL
	.vop_getacl =		ufs_getacl,
	.vop_setacl =		ufs_setacl,
	.vop_aclcheck =		ufs_aclcheck,
#endif
	.vop_fplookup_vexec =	VOP_EAGAIN,
	.vop_fplookup_symlink =	VOP_EAGAIN,
};
VFS_VOP_VECTOR_REGISTER(ufs_fifoops);
