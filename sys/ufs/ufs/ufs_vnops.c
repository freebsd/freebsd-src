/*-
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
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)ufs_vnops.c	8.27 (Berkeley) 5/27/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_mac.h"
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
#include <sys/jail.h>

#include <machine/mutex.h>

#include <security/mac/mac_framework.h>

#include <sys/file.h>		/* XXX */

#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <fs/fifofs/fifo.h>

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
#endif

#include <ufs/ffs/ffs_extern.h>

static vop_access_t	ufs_access;
static int ufs_chmod(struct vnode *, int, struct ucred *, struct thread *);
static int ufs_chown(struct vnode *, uid_t, gid_t, struct ucred *, struct thread *);
static vop_close_t	ufs_close;
static vop_create_t	ufs_create;
static vop_getattr_t	ufs_getattr;
static vop_link_t	ufs_link;
static int ufs_makeinode(int mode, struct vnode *, struct vnode **, struct componentname *);
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
static vop_kqfilter_t	ufsfifo_kqfilter;

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
	if ((vp->v_mount->mnt_flag & MNT_RDONLY) != 0)
		goto out;
	if ((ip->i_flag & (IN_ACCESS | IN_CHANGE | IN_UPDATE)) == 0)
		return;

	if ((vp->v_type == VBLK || vp->v_type == VCHR) && !DOINGSOFTDEP(vp))
		ip->i_flag |= IN_LAZYMOD;
	else if (((vp->v_mount->mnt_kern_flag &
		    (MNTK_SUSPENDED | MNTK_SUSPEND)) == 0) ||
		    (ip->i_flag & (IN_CHANGE | IN_UPDATE)))
		ip->i_flag |= IN_MODIFIED;
	else if (ip->i_flag & IN_ACCESS)
		ip->i_flag |= IN_LAZYACCESS;
	vfs_timestamp(&ts);
	if (ip->i_flag & IN_ACCESS) {
		DIP_SET(ip, i_atime, ts.tv_sec);
		DIP_SET(ip, i_atimensec, ts.tv_nsec);
	}
	if (ip->i_flag & IN_UPDATE) {
		DIP_SET(ip, i_mtime, ts.tv_sec);
		DIP_SET(ip, i_mtimensec, ts.tv_nsec);
		ip->i_modrev++;
	}
	if (ip->i_flag & IN_CHANGE) {
		DIP_SET(ip, i_ctime, ts.tv_sec);
		DIP_SET(ip, i_ctimensec, ts.tv_nsec);
	}

 out:
	ip->i_flag &= ~(IN_ACCESS | IN_CHANGE | IN_UPDATE);
}

void
ufs_itimes(struct vnode *vp)
{

	VI_LOCK(vp);
	ufs_itimes_locked(vp);
	VI_UNLOCK(vp);
}

/*
 * Create a regular file
 */
static int
ufs_create(ap)
	struct vop_create_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	int error;

	error =
	    ufs_makeinode(MAKEIMODE(ap->a_vap->va_type, ap->a_vap->va_mode),
	    ap->a_dvp, ap->a_vpp, ap->a_cnp);
	if (error)
		return (error);
	return (0);
}

/*
 * Mknod vnode call
 */
/* ARGSUSED */
static int
ufs_mknod(ap)
	struct vop_mknod_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	struct vattr *vap = ap->a_vap;
	struct vnode **vpp = ap->a_vpp;
	struct inode *ip;
	ino_t ino;
	int error;

	error = ufs_makeinode(MAKEIMODE(vap->va_type, vap->va_mode),
	    ap->a_dvp, vpp, ap->a_cnp);
	if (error)
		return (error);
	ip = VTOI(*vpp);
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	if (vap->va_rdev != VNOVAL) {
		/*
		 * Want to be able to use this to make badblock
		 * inodes, so don't truncate the dev number.
		 */
		DIP_SET(ip, i_rdev, vap->va_rdev);
	}
	/*
	 * Remove inode, then reload it through VFS_VGET so it is
	 * checked to see if it is an alias of an existing entry in
	 * the inode cache.  XXX I don't believe this is necessary now.
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
	/*
	 * Files marked append-only must be opened for appending.
	 */
	if ((ip->i_flags & APPEND) &&
	    (ap->a_mode & (FWRITE | O_APPEND)) == FWRITE)
		return (EPERM);
	vnode_create_vobject(vp, DIP(ip, i_size), ap->a_td);
	return (0);
}

/*
 * Close called.
 *
 * Update the times on the inode.
 */
/* ARGSUSED */
static int
ufs_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	int usecount;

	VI_LOCK(vp);
	usecount = vp->v_usecount;
	if (usecount > 1)
		ufs_itimes_locked(vp);
	VI_UNLOCK(vp);
	return (0);
}

static int
ufs_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	mode_t mode = ap->a_mode;
	int error;
#ifdef QUOTA
	int relocked;
#endif
#ifdef UFS_ACL
	struct acl *acl;
#endif

	/*
	 * Disallow write attempts on read-only filesystems;
	 * unless the file is a socket, fifo, or a block or
	 * character device resident on the filesystem.
	 */
	if (mode & VWRITE) {
		switch (vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
#ifdef QUOTA
			if (VOP_ISLOCKED(vp) != LK_EXCLUSIVE) {
				relocked = 1;
				vhold(vp);
				vn_lock(vp, LK_UPGRADE | LK_RETRY);
				VI_LOCK(vp);
				vdropl(vp);
				if (vp->v_iflag & VI_DOOMED) {
					VI_UNLOCK(vp);
					error = ENOENT;
					goto relock;
				}
				VI_UNLOCK(vp);
			} else
				relocked = 0;
			error = getinoquota(ip);
relock:
			if (relocked)
				vn_lock(vp, LK_DOWNGRADE | LK_RETRY);
			if (error != 0)
				return (error);
#endif
			break;
		default:
			break;
		}
	}

	/* If immutable bit set, nobody gets to write it. */
	if ((mode & VWRITE) && (ip->i_flags & (IMMUTABLE | SF_SNAPSHOT)))
		return (EPERM);

#ifdef UFS_ACL
	if ((vp->v_mount->mnt_flag & MNT_ACLS) != 0) {
		acl = uma_zalloc(acl_zone, M_WAITOK);
		error = VOP_GETACL(vp, ACL_TYPE_ACCESS, acl, ap->a_cred,
		    ap->a_td);
		switch (error) {
		case EOPNOTSUPP:
			error = vaccess(vp->v_type, ip->i_mode, ip->i_uid,
			    ip->i_gid, ap->a_mode, ap->a_cred, NULL);
			break;
		case 0:
			error = vaccess_acl_posix1e(vp->v_type, ip->i_uid,
			    ip->i_gid, acl, ap->a_mode, ap->a_cred, NULL);
			break;
		default:
			printf(
"ufs_access(): Error retrieving ACL on object (%d).\n",
			    error);
			/*
			 * XXX: Fall back until debugged.  Should
			 * eventually possibly log an error, and return
			 * EPERM for safety.
			 */
			error = vaccess(vp->v_type, ip->i_mode, ip->i_uid,
			    ip->i_gid, ap->a_mode, ap->a_cred, NULL);
		}
		uma_zfree(acl_zone, acl);
	} else
#endif /* !UFS_ACL */
		error = vaccess(vp->v_type, ip->i_mode, ip->i_uid, ip->i_gid,
		    ap->a_mode, ap->a_cred, NULL);
	return (error);
}

/* ARGSUSED */
static int
ufs_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct vattr *vap = ap->a_vap;

	VI_LOCK(vp);
	ufs_itimes_locked(vp);
	if (ip->i_ump->um_fstype == UFS1) {
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
	vap->va_fsid = dev2udev(ip->i_dev);
	vap->va_fileid = ip->i_number;
	vap->va_mode = ip->i_mode & ~IFMT;
	vap->va_nlink = ip->i_effnlink;
	vap->va_uid = ip->i_uid;
	vap->va_gid = ip->i_gid;
	if (ip->i_ump->um_fstype == UFS1) {
		vap->va_rdev = ip->i_din1->di_rdev;
		vap->va_size = ip->i_din1->di_size;
		vap->va_mtime.tv_sec = ip->i_din1->di_mtime;
		vap->va_mtime.tv_nsec = ip->i_din1->di_mtimensec;
		vap->va_ctime.tv_sec = ip->i_din1->di_ctime;
		vap->va_ctime.tv_nsec = ip->i_din1->di_ctimensec;
		vap->va_birthtime.tv_sec = 0;
		vap->va_birthtime.tv_nsec = 0;
		vap->va_bytes = dbtob((u_quad_t)ip->i_din1->di_blocks);
	} else {
		vap->va_rdev = ip->i_din2->di_rdev;
		vap->va_size = ip->i_din2->di_size;
		vap->va_mtime.tv_sec = ip->i_din2->di_mtime;
		vap->va_mtime.tv_nsec = ip->i_din2->di_mtimensec;
		vap->va_ctime.tv_sec = ip->i_din2->di_ctime;
		vap->va_ctime.tv_nsec = ip->i_din2->di_ctimensec;
		vap->va_birthtime.tv_sec = ip->i_din2->di_birthtime;
		vap->va_birthtime.tv_nsec = ip->i_din2->di_birthnsec;
		vap->va_bytes = dbtob((u_quad_t)ip->i_din2->di_blocks);
	}
	vap->va_flags = ip->i_flags;
	vap->va_gen = ip->i_gen;
	vap->va_blocksize = vp->v_mount->mnt_stat.f_iosize;
	vap->va_type = IFTOVT(ip->i_mode);
	vap->va_filerev = ip->i_modrev;
	return (0);
}

/*
 * Set attribute vnode op. called from several syscalls
 */
static int
ufs_setattr(ap)
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct vattr *vap = ap->a_vap;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct ucred *cred = ap->a_cred;
	struct thread *td = ap->a_td;
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
	/*
	 * Mark for update the file's access time for vfs_mark_atime().
	 * We are doing this here to avoid some of the checks done
	 * below -- this operation is done by request of the kernel and
	 * should bypass some security checks.  Things like read-only
	 * checks get handled by other levels (e.g., ffs_update()).
	 */
	if (vap->va_vaflags & VA_MARK_ATIME) {
		ip->i_flag |= IN_ACCESS;
		return (0);
	}
	if (vap->va_flags != VNOVAL) {
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
		 * processes if the security.jail.chflags_allowed sysctl is
		 * is non-zero; otherwise, they behave like unprivileged
		 * processes.
		 */
		if (!priv_check_cred(cred, PRIV_VFS_SYSFLAGS, 0)) {
			if (ip->i_flags
			    & (SF_NOUNLINK | SF_IMMUTABLE | SF_APPEND)) {
				error = securelevel_gt(cred, 0);
				if (error)
					return (error);
			}
			/* Snapshot flag cannot be set or cleared */
			if (((vap->va_flags & SF_SNAPSHOT) != 0 &&
			     (ip->i_flags & SF_SNAPSHOT) == 0) ||
			    ((vap->va_flags & SF_SNAPSHOT) == 0 &&
			     (ip->i_flags & SF_SNAPSHOT) != 0))
				return (EPERM);
			ip->i_flags = vap->va_flags;
			DIP_SET(ip, i_flags, vap->va_flags);
		} else {
			if (ip->i_flags
			    & (SF_NOUNLINK | SF_IMMUTABLE | SF_APPEND) ||
			    (vap->va_flags & UF_SETTABLE) != vap->va_flags)
				return (EPERM);
			ip->i_flags &= SF_SETTABLE;
			ip->i_flags |= (vap->va_flags & UF_SETTABLE);
			DIP_SET(ip, i_flags, ip->i_flags);
		}
		ip->i_flag |= IN_CHANGE;
		if (vap->va_flags & (IMMUTABLE | APPEND))
			return (0);
	}
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
			if ((ip->i_flags & SF_SNAPSHOT) != 0)
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
		if ((error = UFS_TRUNCATE(vp, vap->va_size, IO_NORMAL,
		    cred, td)) != 0)
			return (error);
	}
	if (vap->va_atime.tv_sec != VNOVAL ||
	    vap->va_mtime.tv_sec != VNOVAL ||
	    vap->va_birthtime.tv_sec != VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		if ((ip->i_flags & SF_SNAPSHOT) != 0)
			return (EPERM);
		/*
		 * From utimes(2):
		 * If times is NULL, ... The caller must be the owner of
		 * the file, have permission to write the file, or be the
		 * super-user.
		 * If times is non-NULL, ... The caller must be the owner of
		 * the file or be the super-user.
		 *
		 * Possibly for historical reasons, try to use VADMIN in
		 * preference to VWRITE for a NULL timestamp.  This means we
		 * will return EACCES in preference to EPERM if neither
		 * check succeeds.
		 */
		if (vap->va_vaflags & VA_UTIMES_NULL) {
			error = VOP_ACCESS(vp, VADMIN, cred, td);
			if (error)
				error = VOP_ACCESS(vp, VWRITE, cred, td);
		} else
			error = VOP_ACCESS(vp, VADMIN, cred, td);
		if (error)
			return (error);
		if (vap->va_atime.tv_sec != VNOVAL)
			ip->i_flag |= IN_ACCESS;
		if (vap->va_mtime.tv_sec != VNOVAL)
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
		if (vap->va_birthtime.tv_sec != VNOVAL &&
		    ip->i_ump->um_fstype == UFS2)
			ip->i_flag |= IN_MODIFIED;
		ufs_itimes(vp);
		if (vap->va_atime.tv_sec != VNOVAL) {
			DIP_SET(ip, i_atime, vap->va_atime.tv_sec);
			DIP_SET(ip, i_atimensec, vap->va_atime.tv_nsec);
		}
		if (vap->va_mtime.tv_sec != VNOVAL) {
			DIP_SET(ip, i_mtime, vap->va_mtime.tv_sec);
			DIP_SET(ip, i_mtimensec, vap->va_mtime.tv_nsec);
		}
		if (vap->va_birthtime.tv_sec != VNOVAL &&
		    ip->i_ump->um_fstype == UFS2) {
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
		if ((ip->i_flags & SF_SNAPSHOT) != 0 && (vap->va_mode &
		   (S_IXUSR | S_IWUSR | S_IXGRP | S_IWGRP | S_IXOTH | S_IWOTH)))
			return (EPERM);
		error = ufs_chmod(vp, (int)vap->va_mode, cred, td);
	}
	return (error);
}

/*
 * Change the mode on a file.
 * Inode must be locked before calling.
 */
static int
ufs_chmod(vp, mode, cred, td)
	struct vnode *vp;
	int mode;
	struct ucred *cred;
	struct thread *td;
{
	struct inode *ip = VTOI(vp);
	int error;

	/*
	 * To modify the permissions on a file, must possess VADMIN
	 * for that file.
	 */
	if ((error = VOP_ACCESS(vp, VADMIN, cred, td)))
		return (error);
	/*
	 * Privileged processes may set the sticky bit on non-directories,
	 * as well as set the setgid bit on a file with a group that the
	 * process is not a member of.  Both of these are allowed in
	 * jail(8).
	 */
	if (vp->v_type != VDIR && (mode & S_ISTXT)) {
		if (priv_check_cred(cred, PRIV_VFS_STICKYFILE, 0))
			return (EFTYPE);
	}
	if (!groupmember(ip->i_gid, cred) && (mode & ISGID)) {
		error = priv_check_cred(cred, PRIV_VFS_SETGID, 0);
		if (error)
			return (error);
	}
	ip->i_mode &= ~ALLPERMS;
	ip->i_mode |= (mode & ALLPERMS);
	DIP_SET(ip, i_mode, ip->i_mode);
	ip->i_flag |= IN_CHANGE;
	return (0);
}

/*
 * Perform chown operation on inode ip;
 * inode must be locked prior to call.
 */
static int
ufs_chown(vp, uid, gid, cred, td)
	struct vnode *vp;
	uid_t uid;
	gid_t gid;
	struct ucred *cred;
	struct thread *td;
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
	if ((error = VOP_ACCESS(vp, VADMIN, cred, td)))
		return (error);
	/*
	 * To change the owner of a file, or change the group of a file to a
	 * group of which we are not a member, the caller must have
	 * privilege.
	 */
	if ((uid != ip->i_uid || 
	    (gid != ip->i_gid && !groupmember(gid, cred))) &&
	    (error = priv_check_cred(cred, PRIV_VFS_CHOWN, 0)))
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
	(void) chkdq(ip, -change, cred, CHOWN);
	(void) chkiq(ip, -1, cred, CHOWN);
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
	ip->i_flag |= IN_CHANGE;
	if ((ip->i_mode & (ISUID | ISGID)) && (ouid != uid || ogid != gid)) {
		if (priv_check_cred(cred, PRIV_VFS_RETAINSUGID, 0)) {
			ip->i_mode &= ~(ISUID | ISGID);
			DIP_SET(ip, i_mode, ip->i_mode);
		}
	}
	return (0);
}

static int
ufs_remove(ap)
	struct vop_remove_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct inode *ip;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	int error;
	struct thread *td;

	td = curthread;
	ip = VTOI(vp);
	if ((ip->i_flags & (NOUNLINK | IMMUTABLE | APPEND)) ||
	    (VTOI(dvp)->i_flags & APPEND)) {
		error = EPERM;
		goto out;
	}
#ifdef UFS_GJOURNAL
	ufs_gjournal_orphan(vp);
#endif
	error = ufs_dirremove(dvp, ip, ap->a_cnp->cn_flags, 0);
	if (ip->i_nlink <= 0)
		vp->v_vflag |= VV_NOSYNC;
	if ((ip->i_flags & SF_SNAPSHOT) != 0) {
		/*
		 * Avoid deadlock where another thread is trying to
		 * update the inodeblock for dvp and is waiting on
		 * snaplk.  Temporary unlock the vnode lock for the
		 * unlinked file and sync the directory.  This should
		 * allow vput() of the directory to not block later on
		 * while holding the snapshot vnode locked, assuming
		 * that the directory hasn't been unlinked too.
		 */
		VOP_UNLOCK(vp, 0);
		(void) VOP_FSYNC(dvp, MNT_WAIT, td);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	}
out:
	return (error);
}

/*
 * link vnode call
 */
static int
ufs_link(ap)
	struct vop_link_args /* {
		struct vnode *a_tdvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct vnode *tdvp = ap->a_tdvp;
	struct componentname *cnp = ap->a_cnp;
	struct inode *ip;
	struct direct newdir;
	int error;

#ifdef INVARIANTS
	if ((cnp->cn_flags & HASBUF) == 0)
		panic("ufs_link: no name");
#endif
	if (tdvp->v_mount != vp->v_mount) {
		error = EXDEV;
		goto out;
	}
	ip = VTOI(vp);
	if ((nlink_t)ip->i_nlink >= LINK_MAX) {
		error = EMLINK;
		goto out;
	}
	if (ip->i_flags & (IMMUTABLE | APPEND)) {
		error = EPERM;
		goto out;
	}
	ip->i_effnlink++;
	ip->i_nlink++;
	DIP_SET(ip, i_nlink, ip->i_nlink);
	ip->i_flag |= IN_CHANGE;
	if (DOINGSOFTDEP(vp))
		softdep_change_linkcnt(ip);
	error = UFS_UPDATE(vp, !(DOINGSOFTDEP(vp) | DOINGASYNC(vp)));
	if (!error) {
		ufs_makedirentry(ip, cnp, &newdir);
		error = ufs_direnter(tdvp, vp, &newdir, cnp, NULL);
	}

	if (error) {
		ip->i_effnlink--;
		ip->i_nlink--;
		DIP_SET(ip, i_nlink, ip->i_nlink);
		ip->i_flag |= IN_CHANGE;
		if (DOINGSOFTDEP(vp))
			softdep_change_linkcnt(ip);
	}
out:
	return (error);
}

/*
 * whiteout vnode call
 */
static int
ufs_whiteout(ap)
	struct vop_whiteout_args /* {
		struct vnode *a_dvp;
		struct componentname *a_cnp;
		int a_flags;
	} */ *ap;
{
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct direct newdir;
	int error = 0;

	switch (ap->a_flags) {
	case LOOKUP:
		/* 4.4 format directories support whiteout operations */
		if (dvp->v_mount->mnt_maxsymlinklen > 0)
			return (0);
		return (EOPNOTSUPP);

	case CREATE:
		/* create a new directory whiteout */
#ifdef INVARIANTS
		if ((cnp->cn_flags & SAVENAME) == 0)
			panic("ufs_whiteout: missing name");
		if (dvp->v_mount->mnt_maxsymlinklen <= 0)
			panic("ufs_whiteout: old format filesystem");
#endif

		newdir.d_ino = WINO;
		newdir.d_namlen = cnp->cn_namelen;
		bcopy(cnp->cn_nameptr, newdir.d_name, (unsigned)cnp->cn_namelen + 1);
		newdir.d_type = DT_WHT;
		error = ufs_direnter(dvp, NULL, &newdir, cnp, NULL);
		break;

	case DELETE:
		/* remove an existing directory whiteout */
#ifdef INVARIANTS
		if (dvp->v_mount->mnt_maxsymlinklen <= 0)
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
ufs_rename(ap)
	struct vop_rename_args  /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap;
{
	struct vnode *tvp = ap->a_tvp;
	struct vnode *tdvp = ap->a_tdvp;
	struct vnode *fvp = ap->a_fvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct componentname *tcnp = ap->a_tcnp;
	struct componentname *fcnp = ap->a_fcnp;
	struct thread *td = fcnp->cn_thread;
	struct inode *ip, *xp, *dp;
	struct direct newdir;
	int doingdirectory = 0, oldparent = 0, newparent = 0;
	int error = 0, ioflag;

#ifdef INVARIANTS
	if ((tcnp->cn_flags & HASBUF) == 0 ||
	    (fcnp->cn_flags & HASBUF) == 0)
		panic("ufs_rename: no name");
#endif
	/*
	 * Check for cross-device rename.
	 */
	if ((fvp->v_mount != tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
abortit:
		if (tdvp == tvp)
			vrele(tdvp);
		else
			vput(tdvp);
		if (tvp)
			vput(tvp);
		vrele(fdvp);
		vrele(fvp);
		return (error);
	}

	if (tvp && ((VTOI(tvp)->i_flags & (NOUNLINK | IMMUTABLE | APPEND)) ||
	    (VTOI(tdvp)->i_flags & APPEND))) {
		error = EPERM;
		goto abortit;
	}

	/*
	 * Renaming a file to itself has no effect.  The upper layers should
	 * not call us in that case.  Temporarily just warn if they do.
	 */
	if (fvp == tvp) {
		printf("ufs_rename: fvp == tvp (can't happen)\n");
		error = 0;
		goto abortit;
	}

	if ((error = vn_lock(fvp, LK_EXCLUSIVE)) != 0)
		goto abortit;
	dp = VTOI(fdvp);
	ip = VTOI(fvp);
	if (ip->i_nlink >= LINK_MAX) {
		VOP_UNLOCK(fvp, 0);
		error = EMLINK;
		goto abortit;
	}
	if ((ip->i_flags & (NOUNLINK | IMMUTABLE | APPEND))
	    || (dp->i_flags & APPEND)) {
		VOP_UNLOCK(fvp, 0);
		error = EPERM;
		goto abortit;
	}
	if ((ip->i_mode & IFMT) == IFDIR) {
		/*
		 * Avoid ".", "..", and aliases of "." for obvious reasons.
		 */
		if ((fcnp->cn_namelen == 1 && fcnp->cn_nameptr[0] == '.') ||
		    dp == ip || (fcnp->cn_flags | tcnp->cn_flags) & ISDOTDOT ||
		    (ip->i_flag & IN_RENAME)) {
			VOP_UNLOCK(fvp, 0);
			error = EINVAL;
			goto abortit;
		}
		ip->i_flag |= IN_RENAME;
		oldparent = dp->i_number;
		doingdirectory = 1;
	}
	vrele(fdvp);

	/*
	 * When the target exists, both the directory
	 * and target vnodes are returned locked.
	 */
	dp = VTOI(tdvp);
	xp = NULL;
	if (tvp)
		xp = VTOI(tvp);

	/*
	 * 1) Bump link count while we're moving stuff
	 *    around.  If we crash somewhere before
	 *    completing our work, the link count
	 *    may be wrong, but correctable.
	 */
	ip->i_effnlink++;
	ip->i_nlink++;
	DIP_SET(ip, i_nlink, ip->i_nlink);
	ip->i_flag |= IN_CHANGE;
	if (DOINGSOFTDEP(fvp))
		softdep_change_linkcnt(ip);
	if ((error = UFS_UPDATE(fvp, !(DOINGSOFTDEP(fvp) |
				       DOINGASYNC(fvp)))) != 0) {
		VOP_UNLOCK(fvp, 0);
		goto bad;
	}

	/*
	 * If ".." must be changed (ie the directory gets a new
	 * parent) then the source directory must not be in the
	 * directory hierarchy above the target, as this would
	 * orphan everything below the source directory. Also
	 * the user must have write permission in the source so
	 * as to be able to change "..". We must repeat the call
	 * to namei, as the parent directory is unlocked by the
	 * call to checkpath().
	 */
	error = VOP_ACCESS(fvp, VWRITE, tcnp->cn_cred, tcnp->cn_thread);
	VOP_UNLOCK(fvp, 0);
	if (oldparent != dp->i_number)
		newparent = dp->i_number;
	if (doingdirectory && newparent) {
		if (error)	/* write access check above */
			goto bad;
		if (xp != NULL)
			vput(tvp);
		error = ufs_checkpath(ip, dp, tcnp->cn_cred);
		if (error)
			goto out;
		if ((tcnp->cn_flags & SAVESTART) == 0)
			panic("ufs_rename: lost to startdir");
		VREF(tdvp);
		error = relookup(tdvp, &tvp, tcnp);
		if (error)
			goto out;
		vrele(tdvp);
		dp = VTOI(tdvp);
		xp = NULL;
		if (tvp)
			xp = VTOI(tvp);
	}
	/*
	 * 2) If target doesn't exist, link the target
	 *    to the source and unlink the source.
	 *    Otherwise, rewrite the target directory
	 *    entry to reference the source inode and
	 *    expunge the original entry's existence.
	 */
	if (xp == NULL) {
		if (dp->i_dev != ip->i_dev)
			panic("ufs_rename: EXDEV");
		/*
		 * Account for ".." in new directory.
		 * When source and destination have the same
		 * parent we don't fool with the link count.
		 */
		if (doingdirectory && newparent) {
			if ((nlink_t)dp->i_nlink >= LINK_MAX) {
				error = EMLINK;
				goto bad;
			}
			dp->i_effnlink++;
			dp->i_nlink++;
			DIP_SET(dp, i_nlink, dp->i_nlink);
			dp->i_flag |= IN_CHANGE;
			if (DOINGSOFTDEP(tdvp))
				softdep_change_linkcnt(dp);
			error = UFS_UPDATE(tdvp, !(DOINGSOFTDEP(tdvp) |
						   DOINGASYNC(tdvp)));
			if (error)
				goto bad;
		}
		ufs_makedirentry(ip, tcnp, &newdir);
		error = ufs_direnter(tdvp, NULL, &newdir, tcnp, NULL);
		if (error) {
			if (doingdirectory && newparent) {
				dp->i_effnlink--;
				dp->i_nlink--;
				DIP_SET(dp, i_nlink, dp->i_nlink);
				dp->i_flag |= IN_CHANGE;
				if (DOINGSOFTDEP(tdvp))
					softdep_change_linkcnt(dp);
				(void)UFS_UPDATE(tdvp, 1);
			}
			goto bad;
		}
		vput(tdvp);
	} else {
		if (xp->i_dev != dp->i_dev || xp->i_dev != ip->i_dev)
			panic("ufs_rename: EXDEV");
		/*
		 * Short circuit rename(foo, foo).
		 */
		if (xp->i_number == ip->i_number)
			panic("ufs_rename: same file");
		/*
		 * If the parent directory is "sticky", then the caller
		 * must possess VADMIN for the parent directory, or the
		 * destination of the rename.  This implements append-only
		 * directories.
		 */
		if ((dp->i_mode & S_ISTXT) &&
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
		if ((xp->i_mode&IFMT) == IFDIR) {
			if ((xp->i_effnlink > 2) ||
			    !ufs_dirempty(xp, dp->i_number, tcnp->cn_cred)) {
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
		error = ufs_dirrewrite(dp, xp, ip->i_number,
		    IFTODT(ip->i_mode),
		    (doingdirectory && newparent) ? newparent : doingdirectory);
		if (error)
			goto bad;
		if (doingdirectory) {
			if (!newparent) {
				dp->i_effnlink--;
				if (DOINGSOFTDEP(tdvp))
					softdep_change_linkcnt(dp);
			}
			xp->i_effnlink--;
			if (DOINGSOFTDEP(tvp))
				softdep_change_linkcnt(xp);
		}
		if (doingdirectory && !DOINGSOFTDEP(tvp)) {
			/*
			 * Truncate inode. The only stuff left in the directory
			 * is "." and "..". The "." reference is inconsequential
			 * since we are quashing it. We have removed the "."
			 * reference and the reference in the parent directory,
			 * but there may be other hard links. The soft
			 * dependency code will arrange to do these operations
			 * after the parent directory entry has been deleted on
			 * disk, so when running with that code we avoid doing
			 * them now.
			 */
			if (!newparent) {
				dp->i_nlink--;
				DIP_SET(dp, i_nlink, dp->i_nlink);
				dp->i_flag |= IN_CHANGE;
			}
			xp->i_nlink--;
			DIP_SET(xp, i_nlink, xp->i_nlink);
			xp->i_flag |= IN_CHANGE;
			ioflag = IO_NORMAL;
			if (!DOINGASYNC(tvp))
				ioflag |= IO_SYNC;
			if ((error = UFS_TRUNCATE(tvp, (off_t)0, ioflag,
			    tcnp->cn_cred, tcnp->cn_thread)) != 0)
				goto bad;
		}
		vput(tdvp);
		vput(tvp);
		xp = NULL;
	}

	/*
	 * 3) Unlink the source.
	 */
	fcnp->cn_flags &= ~MODMASK;
	fcnp->cn_flags |= LOCKPARENT | LOCKLEAF;
	if ((fcnp->cn_flags & SAVESTART) == 0)
		panic("ufs_rename: lost from startdir");
	VREF(fdvp);
	error = relookup(fdvp, &fvp, fcnp);
	if (error == 0)
		vrele(fdvp);
	if (fvp != NULL) {
		xp = VTOI(fvp);
		dp = VTOI(fdvp);
	} else {
		/*
		 * From name has disappeared.  IN_RENAME is not sufficient
		 * to protect against directory races due to timing windows,
		 * so we have to remove the panic.  XXX the only real way
		 * to solve this issue is at a much higher level.  By the
		 * time we hit ufs_rename() it's too late.
		 */
#if 0
		if (doingdirectory)
			panic("ufs_rename: lost dir entry");
#endif
		vrele(ap->a_fvp);
		return (0);
	}
	/*
	 * Ensure that the directory entry still exists and has not
	 * changed while the new name has been entered. If the source is
	 * a file then the entry may have been unlinked or renamed. In
	 * either case there is no further work to be done. If the source
	 * is a directory then it cannot have been rmdir'ed; the IN_RENAME
	 * flag ensures that it cannot be moved by another rename or removed
	 * by a rmdir.
	 */
	if (xp != ip) {
		/*
		 * From name resolves to a different inode.  IN_RENAME is
		 * not sufficient protection against timing window races
		 * so we can't panic here.  XXX the only real way
		 * to solve this issue is at a much higher level.  By the
		 * time we hit ufs_rename() it's too late.
		 */
#if 0
		if (doingdirectory)
			panic("ufs_rename: lost dir entry");
#endif
	} else {
		/*
		 * If the source is a directory with a
		 * new parent, the link count of the old
		 * parent directory must be decremented
		 * and ".." set to point to the new parent.
		 */
		if (doingdirectory && newparent) {
			xp->i_offset = mastertemplate.dot_reclen;
			ufs_dirrewrite(xp, dp, newparent, DT_DIR, 0);
			cache_purge(fdvp);
		}
		error = ufs_dirremove(fdvp, xp, fcnp->cn_flags, 0);
		xp->i_flag &= ~IN_RENAME;
	}
	if (dp)
		vput(fdvp);
	if (xp)
		vput(fvp);
	vrele(ap->a_fvp);
	return (error);

bad:
	if (xp)
		vput(ITOV(xp));
	vput(ITOV(dp));
out:
	if (doingdirectory)
		ip->i_flag &= ~IN_RENAME;
	if (vn_lock(fvp, LK_EXCLUSIVE) == 0) {
		ip->i_effnlink--;
		ip->i_nlink--;
		DIP_SET(ip, i_nlink, ip->i_nlink);
		ip->i_flag |= IN_CHANGE;
		ip->i_flag &= ~IN_RENAME;
		if (DOINGSOFTDEP(fvp))
			softdep_change_linkcnt(ip);
		vput(fvp);
	} else
		vrele(fvp);
	return (error);
}

/*
 * Mkdir system call
 */
static int
ufs_mkdir(ap)
	struct vop_mkdir_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	struct inode *ip, *dp;
	struct vnode *tvp;
	struct buf *bp;
	struct dirtemplate dirtemplate, *dtp;
	struct direct newdir;
#ifdef UFS_ACL
	struct acl *acl, *dacl;
#endif
	int error, dmode;
	long blkoff;

#ifdef INVARIANTS
	if ((cnp->cn_flags & HASBUF) == 0)
		panic("ufs_mkdir: no name");
#endif
	dp = VTOI(dvp);
	if ((nlink_t)dp->i_nlink >= LINK_MAX) {
		error = EMLINK;
		goto out;
	}
	dmode = vap->va_mode & 0777;
	dmode |= IFDIR;
	/*
	 * Must simulate part of ufs_makeinode here to acquire the inode,
	 * but not have it entered in the parent directory. The entry is
	 * made later after writing "." and ".." entries.
	 */
	error = UFS_VALLOC(dvp, dmode, cnp->cn_cred, &tvp);
	if (error)
		goto out;
	ip = VTOI(tvp);
	ip->i_gid = dp->i_gid;
	DIP_SET(ip, i_gid, dp->i_gid);
#ifdef SUIDDIR
	{
#ifdef QUOTA
		struct ucred ucred, *ucp;
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
				refcount_init(&ucred.cr_ref, 1);
				ucred.cr_uid = ip->i_uid;
				ucred.cr_ngroups = 1;
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
			UFS_VFREE(tvp, ip->i_number, dmode);
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
		UFS_VFREE(tvp, ip->i_number, dmode);
		vput(tvp);
		return (error);
	}
#endif
#endif	/* !SUIDDIR */
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
#ifdef UFS_ACL
	acl = dacl = NULL;
	if ((dvp->v_mount->mnt_flag & MNT_ACLS) != 0) {
		acl = uma_zalloc(acl_zone, M_WAITOK);
		dacl = uma_zalloc(acl_zone, M_WAITOK);

		/*
		 * Retrieve default ACL from parent, if any.
		 */
		error = VOP_GETACL(dvp, ACL_TYPE_DEFAULT, acl, cnp->cn_cred,
		    cnp->cn_thread);
		switch (error) {
		case 0:
			/*
			 * Retrieved a default ACL, so merge mode and ACL if
			 * necessary.  If the ACL is empty, fall through to
			 * the "not defined or available" case.
			 */
			if (acl->acl_cnt != 0) {
				dmode = acl_posix1e_newfilemode(dmode, acl);
				ip->i_mode = dmode;
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
			ip->i_mode = dmode;
			DIP_SET(ip, i_mode, dmode);
			uma_zfree(acl_zone, acl);
			uma_zfree(acl_zone, dacl);
			dacl = acl = NULL;
			break;
		
		default:
			UFS_VFREE(tvp, ip->i_number, dmode);
			vput(tvp);
			uma_zfree(acl_zone, acl);
			uma_zfree(acl_zone, dacl);
			return (error);
		}
	} else {
#endif /* !UFS_ACL */
		ip->i_mode = dmode;
		DIP_SET(ip, i_mode, dmode);
#ifdef UFS_ACL
	}
#endif
	tvp->v_type = VDIR;	/* Rest init'd in getnewvnode(). */
	ip->i_effnlink = 2;
	ip->i_nlink = 2;
	DIP_SET(ip, i_nlink, 2);
	if (DOINGSOFTDEP(tvp))
		softdep_change_linkcnt(ip);
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
	DIP_SET(dp, i_nlink, dp->i_nlink);
	dp->i_flag |= IN_CHANGE;
	if (DOINGSOFTDEP(dvp))
		softdep_change_linkcnt(dp);
	error = UFS_UPDATE(tvp, !(DOINGSOFTDEP(dvp) | DOINGASYNC(dvp)));
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
	if (acl != NULL) {
		/*
		 * XXX: If we abort now, will Soft Updates notify the extattr
		 * code that the EAs for the file need to be released?
		 */
		error = VOP_SETACL(tvp, ACL_TYPE_ACCESS, acl, cnp->cn_cred,
		    cnp->cn_thread);
		if (error == 0)
			error = VOP_SETACL(tvp, ACL_TYPE_DEFAULT, dacl,
			    cnp->cn_cred, cnp->cn_thread);
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
			uma_zfree(acl_zone, acl);
			uma_zfree(acl_zone, dacl);
			dacl = acl = NULL;
			goto bad;
		}
		uma_zfree(acl_zone, acl);
		uma_zfree(acl_zone, dacl);
		dacl = acl = NULL;
	}
#endif /* !UFS_ACL */

	/*
	 * Initialize directory with "." and ".." from static template.
	 */
	if (dvp->v_mount->mnt_maxsymlinklen > 0)
		dtp = &mastertemplate;
	else
		dtp = (struct dirtemplate *)&omastertemplate;
	dirtemplate = *dtp;
	dirtemplate.dot_ino = ip->i_number;
	dirtemplate.dotdot_ino = dp->i_number;
	if ((error = UFS_BALLOC(tvp, (off_t)0, DIRBLKSIZ, cnp->cn_cred,
	    BA_CLRBUF, &bp)) != 0)
		goto bad;
	ip->i_size = DIRBLKSIZ;
	DIP_SET(ip, i_size, DIRBLKSIZ);
	ip->i_flag |= IN_CHANGE | IN_UPDATE;
	vnode_pager_setsize(tvp, (u_long)ip->i_size);
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
	if ((error = UFS_UPDATE(tvp, !(DOINGSOFTDEP(tvp) |
				       DOINGASYNC(tvp)))) != 0) {
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
	} else {
#ifdef UFS_ACL
		if (acl != NULL)
			uma_zfree(acl_zone, acl);
		if (dacl != NULL)
			uma_zfree(acl_zone, dacl);
#endif
		dp->i_effnlink--;
		dp->i_nlink--;
		DIP_SET(dp, i_nlink, dp->i_nlink);
		dp->i_flag |= IN_CHANGE;
		if (DOINGSOFTDEP(dvp))
			softdep_change_linkcnt(dp);
		/*
		 * No need to do an explicit VOP_TRUNCATE here, vrele will
		 * do this for us because we set the link count to 0.
		 */
		ip->i_effnlink = 0;
		ip->i_nlink = 0;
		DIP_SET(ip, i_nlink, 0);
		ip->i_flag |= IN_CHANGE;
		if (DOINGSOFTDEP(tvp))
			softdep_change_linkcnt(ip);
		vput(tvp);
	}
out:
	return (error);
}

/*
 * Rmdir system call.
 */
static int
ufs_rmdir(ap)
	struct vop_rmdir_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct inode *ip, *dp;
	int error, ioflag;

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
	if ((ip->i_flag & IN_RENAME) || ip->i_effnlink < 2) {
		error = EINVAL;
		goto out;
	}
	if (!ufs_dirempty(ip, dp->i_number, cnp->cn_cred)) {
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
	if (DOINGSOFTDEP(vp)) {
		softdep_change_linkcnt(dp);
		softdep_change_linkcnt(ip);
	}
	error = ufs_dirremove(dvp, ip, cnp->cn_flags, 1);
	if (error) {
		dp->i_effnlink++;
		ip->i_effnlink++;
		if (DOINGSOFTDEP(vp)) {
			softdep_change_linkcnt(dp);
			softdep_change_linkcnt(ip);
		}
		goto out;
	}
	cache_purge(dvp);
	/*
	 * Truncate inode. The only stuff left in the directory is "." and
	 * "..". The "." reference is inconsequential since we are quashing
	 * it. The soft dependency code will arrange to do these operations
	 * after the parent directory entry has been deleted on disk, so
	 * when running with that code we avoid doing them now.
	 */
	if (!DOINGSOFTDEP(vp)) {
		dp->i_nlink--;
		DIP_SET(dp, i_nlink, dp->i_nlink);
		dp->i_flag |= IN_CHANGE;
		ip->i_nlink--;
		DIP_SET(ip, i_nlink, ip->i_nlink);
		ip->i_flag |= IN_CHANGE;
		ioflag = IO_NORMAL;
		if (!DOINGASYNC(vp))
			ioflag |= IO_SYNC;
		error = UFS_TRUNCATE(vp, (off_t)0, ioflag, cnp->cn_cred,
		    cnp->cn_thread);
	}
	cache_purge(vp);
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
ufs_symlink(ap)
	struct vop_symlink_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap;
{
	struct vnode *vp, **vpp = ap->a_vpp;
	struct inode *ip;
	int len, error;

	error = ufs_makeinode(IFLNK | ap->a_vap->va_mode, ap->a_dvp,
	    vpp, ap->a_cnp);
	if (error)
		return (error);
	vp = *vpp;
	len = strlen(ap->a_target);
	if (len < vp->v_mount->mnt_maxsymlinklen) {
		ip = VTOI(vp);
		bcopy(ap->a_target, SHORTLINK(ip), len);
		ip->i_size = len;
		DIP_SET(ip, i_size, len);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	} else
		error = vn_rdwr(UIO_WRITE, vp, ap->a_target, len, (off_t)0,
		    UIO_SYSSPACE, IO_NODELOCKED | IO_NOMACCHECK,
		    ap->a_cnp->cn_cred, NOCRED, (int *)0, (struct thread *)0);
	if (error)
		vput(vp);
	return (error);
}

/*
 * Vnode op for reading directories.
 *
 * The routine below assumes that the on-disk format of a directory
 * is the same as that defined by <sys/dirent.h>. If the on-disk
 * format changes, then it will be necessary to do a conversion
 * from the on-disk format that read returns to the format defined
 * by <sys/dirent.h>.
 */
int
ufs_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		int *a_ncookies;
		u_long **a_cookies;
	} */ *ap;
{
	struct uio *uio = ap->a_uio;
	int error;
	size_t count, lost;
	off_t off;

	if (ap->a_ncookies != NULL)
		/*
		 * Ensure that the block is aligned.  The caller can use
		 * the cookies to determine where in the block to start.
		 */
		uio->uio_offset &= ~(DIRBLKSIZ - 1);
	off = uio->uio_offset;
	count = uio->uio_resid;
	/* Make sure we don't return partial entries. */
	if (count <= ((uio->uio_offset + count) & (DIRBLKSIZ -1)))
		return (EINVAL);
	count -= (uio->uio_offset + count) & (DIRBLKSIZ -1);
	lost = uio->uio_resid - count;
	uio->uio_resid = count;
	uio->uio_iov->iov_len = count;
#	if (BYTE_ORDER == LITTLE_ENDIAN)
		if (ap->a_vp->v_mount->mnt_maxsymlinklen > 0) {
			error = VOP_READ(ap->a_vp, uio, 0, ap->a_cred);
		} else {
			struct dirent *dp, *edp;
			struct uio auio;
			struct iovec aiov;
			caddr_t dirbuf;
			int readcnt;
			u_char tmp;

			auio = *uio;
			auio.uio_iov = &aiov;
			auio.uio_iovcnt = 1;
			auio.uio_segflg = UIO_SYSSPACE;
			aiov.iov_len = count;
			MALLOC(dirbuf, caddr_t, count, M_TEMP, M_WAITOK);
			aiov.iov_base = dirbuf;
			error = VOP_READ(ap->a_vp, &auio, 0, ap->a_cred);
			if (error == 0) {
				readcnt = count - auio.uio_resid;
				edp = (struct dirent *)&dirbuf[readcnt];
				for (dp = (struct dirent *)dirbuf; dp < edp; ) {
					tmp = dp->d_namlen;
					dp->d_namlen = dp->d_type;
					dp->d_type = tmp;
					if (dp->d_reclen > 0) {
						dp = (struct dirent *)
						    ((char *)dp + dp->d_reclen);
					} else {
						error = EIO;
						break;
					}
				}
				if (dp >= edp)
					error = uiomove(dirbuf, readcnt, uio);
			}
			FREE(dirbuf, M_TEMP);
		}
#	else
		error = VOP_READ(ap->a_vp, uio, 0, ap->a_cred);
#	endif
	if (!error && ap->a_ncookies != NULL) {
		struct dirent* dpStart;
		struct dirent* dpEnd;
		struct dirent* dp;
		int ncookies;
		u_long *cookies;
		u_long *cookiep;

		if (uio->uio_segflg != UIO_SYSSPACE || uio->uio_iovcnt != 1)
			panic("ufs_readdir: unexpected uio from NFS server");
		dpStart = (struct dirent *)
		    ((char *)uio->uio_iov->iov_base - (uio->uio_offset - off));
		dpEnd = (struct dirent *) uio->uio_iov->iov_base;
		for (dp = dpStart, ncookies = 0;
		     dp < dpEnd;
		     dp = (struct dirent *)((caddr_t) dp + dp->d_reclen))
			ncookies++;
		MALLOC(cookies, u_long *, ncookies * sizeof(u_long), M_TEMP,
		    M_WAITOK);
		for (dp = dpStart, cookiep = cookies;
		     dp < dpEnd;
		     dp = (struct dirent *)((caddr_t) dp + dp->d_reclen)) {
			off += dp->d_reclen;
			*cookiep++ = (u_long) off;
		}
		*ap->a_ncookies = ncookies;
		*ap->a_cookies = cookies;
	}
	uio->uio_resid += lost;
	if (ap->a_eofflag)
	    *ap->a_eofflag = VTOI(ap->a_vp)->i_size <= uio->uio_offset;
	return (error);
}

/*
 * Return target name of a symbolic link
 */
static int
ufs_readlink(ap)
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	doff_t isize;

	isize = ip->i_size;
	if ((isize < vp->v_mount->mnt_maxsymlinklen) ||
	    DIP(ip, i_blocks) == 0) { /* XXX - for old fastlink support */
		return (uiomove(SHORTLINK(ip), isize, ap->a_uio));
	}
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
ufs_strategy(ap)
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap;
{
	struct buf *bp = ap->a_bp;
	struct vnode *vp = ap->a_vp;
	struct bufobj *bo;
	struct inode *ip;
	ufs2_daddr_t blkno;
	int error;

	ip = VTOI(vp);
	if (bp->b_blkno == bp->b_lblkno) {
		error = ufs_bmaparray(vp, bp->b_lblkno, &blkno, bp, NULL, NULL);
		bp->b_blkno = blkno;
		if (error) {
			bp->b_error = error;
			bp->b_ioflags |= BIO_ERROR;
			bufdone(bp);
			return (error);
		}
		if ((long)bp->b_blkno == -1)
			vfs_bio_clrbuf(bp);
	}
	if ((long)bp->b_blkno == -1) {
		bufdone(bp);
		return (0);
	}
	bp->b_iooffset = dbtob(bp->b_blkno);
	bo = ip->i_umbufobj;
	BO_STRATEGY(bo, bp);
	return (0);
}

/*
 * Print out the contents of an inode.
 */
static int
ufs_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);

	printf("\tino %lu, on dev %s", (u_long)ip->i_number,
	    devtoname(ip->i_dev));
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
ufsfifo_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	int usecount;

	VI_LOCK(vp);
	usecount = vp->v_usecount;
	if (usecount > 1)
		ufs_itimes_locked(vp);
	VI_UNLOCK(vp);
	return (fifo_specops.vop_close(ap));
}

/*
 * Kqfilter wrapper for fifos.
 *
 * Fall through to ufs kqfilter routines if needed 
 */
static int
ufsfifo_kqfilter(ap)
	struct vop_kqfilter_args *ap;
{
	int error;

	error = fifo_specops.vop_kqfilter(ap);
	if (error)
		error = vfs_kqfilter(ap);
	return (error);
}

/*
 * Return POSIX pathconf information applicable to ufs filesystems.
 */
static int
ufs_pathconf(ap)
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		int *a_retval;
	} */ *ap;
{
	int error;

	error = 0;
	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = LINK_MAX;
		break;
	case _PC_NAME_MAX:
		*ap->a_retval = NAME_MAX;
		break;
	case _PC_PATH_MAX:
		*ap->a_retval = PATH_MAX;
		break;
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		break;
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		break;
	case _PC_NO_TRUNC:
		*ap->a_retval = 1;
		break;
	case _PC_ACL_EXTENDED:
#ifdef UFS_ACL
		if (ap->a_vp->v_mount->mnt_flag & MNT_ACLS)
			*ap->a_retval = 1;
		else
			*ap->a_retval = 0;
#else
		*ap->a_retval = 0;
#endif
		break;
	case _PC_ACL_PATH_MAX:
#ifdef UFS_ACL
		if (ap->a_vp->v_mount->mnt_flag & MNT_ACLS)
			*ap->a_retval = ACL_MAX_ENTRIES;
		else
			*ap->a_retval = 3;
#else
		*ap->a_retval = 3;
#endif
		break;
	case _PC_MAC_PRESENT:
#ifdef MAC
		if (ap->a_vp->v_mount->mnt_flag & MNT_MULTILABEL)
			*ap->a_retval = 1;
		else
			*ap->a_retval = 0;
#else
		*ap->a_retval = 0;
#endif
		break;
	case _PC_ASYNC_IO:
		/* _PC_ASYNC_IO should have been handled by upper layers. */
		KASSERT(0, ("_PC_ASYNC_IO should not get here"));
		error = EINVAL;
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
		error = EINVAL;
		break;
	}
	return (error);
}

/*
 * Initialize the vnode associated with a new inode, handle aliased
 * vnodes.
 */
int
ufs_vinit(mntp, fifoops, vpp)
	struct mount *mntp;
	struct vop_vector *fifoops;
	struct vnode **vpp;
{
	struct inode *ip;
	struct vnode *vp;

	vp = *vpp;
	ip = VTOI(vp);
	vp->v_type = IFTOVT(ip->i_mode);
	if (vp->v_type == VFIFO)
		vp->v_op = fifoops;
	ASSERT_VOP_LOCKED(vp, "ufs_vinit");
	if (ip->i_number == ROOTINO)
		vp->v_vflag |= VV_ROOT;
	ip->i_modrev = init_va_filerev();
	*vpp = vp;
	return (0);
}

/*
 * Allocate a new inode.
 * Vnode dvp must be locked.
 */
static int
ufs_makeinode(mode, dvp, vpp, cnp)
	int mode;
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
{
	struct inode *ip, *pdir;
	struct direct newdir;
	struct vnode *tvp;
#ifdef UFS_ACL
	struct acl *acl;
#endif
	int error;

	pdir = VTOI(dvp);
#ifdef INVARIANTS
	if ((cnp->cn_flags & HASBUF) == 0)
		panic("ufs_makeinode: no name");
#endif
	*vpp = NULL;
	if ((mode & IFMT) == 0)
		mode |= IFREG;

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
			refcount_init(&ucred.cr_ref, 1);
			ucred.cr_uid = ip->i_uid;
			ucred.cr_ngroups = 1;
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
			UFS_VFREE(tvp, ip->i_number, mode);
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
		UFS_VFREE(tvp, ip->i_number, mode);
		vput(tvp);
		return (error);
	}
#endif
#endif	/* !SUIDDIR */
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
#ifdef UFS_ACL
	acl = NULL;
	if ((dvp->v_mount->mnt_flag & MNT_ACLS) != 0) {
		acl = uma_zalloc(acl_zone, M_WAITOK);

		/*
		 * Retrieve default ACL for parent, if any.
		 */
		error = VOP_GETACL(dvp, ACL_TYPE_DEFAULT, acl, cnp->cn_cred,
		    cnp->cn_thread);
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
				ip->i_mode = mode;
				DIP_SET(ip, i_mode, mode);
				ufs_sync_acl_from_inode(ip, acl);
				break;
			}
			/* FALLTHROUGH */
	
		case EOPNOTSUPP:
			/*
			 * Just use the mode as-is.
			 */
			ip->i_mode = mode;
			DIP_SET(ip, i_mode, mode);
			uma_zfree(acl_zone, acl);
			acl = NULL;
			break;
	
		default:
			UFS_VFREE(tvp, ip->i_number, mode);
			vput(tvp);
			uma_zfree(acl_zone, acl);
			acl = NULL;
			return (error);
		}
	} else {
#endif
		ip->i_mode = mode;
		DIP_SET(ip, i_mode, mode);
#ifdef UFS_ACL
	}
#endif
	tvp->v_type = IFTOVT(mode);	/* Rest init'd in getnewvnode(). */
	ip->i_effnlink = 1;
	ip->i_nlink = 1;
	DIP_SET(ip, i_nlink, 1);
	if (DOINGSOFTDEP(tvp))
		softdep_change_linkcnt(ip);
	if ((ip->i_mode & ISGID) && !groupmember(ip->i_gid, cnp->cn_cred) &&
	    priv_check_cred(cnp->cn_cred, PRIV_VFS_SETGID, 0)) {
		ip->i_mode &= ~ISGID;
		DIP_SET(ip, i_mode, ip->i_mode);
	}

	if (cnp->cn_flags & ISWHITEOUT) {
		ip->i_flags |= UF_OPAQUE;
		DIP_SET(ip, i_flags, ip->i_flags);
	}

	/*
	 * Make sure inode goes to disk before directory entry.
	 */
	error = UFS_UPDATE(tvp, !(DOINGSOFTDEP(tvp) | DOINGASYNC(tvp)));
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
	if (acl != NULL) {
		/*
		 * XXX: If we abort now, will Soft Updates notify the extattr
		 * code that the EAs for the file need to be released?
		 */
		error = VOP_SETACL(tvp, ACL_TYPE_ACCESS, acl, cnp->cn_cred,
		    cnp->cn_thread);
		switch (error) {
		case 0:
			break;

		case EOPNOTSUPP:
			/*
			 * XXX: This should not happen, as EOPNOTSUPP above was
			 * supposed to free acl.
			 */
			printf("ufs_makeinode: VOP_GETACL() but no "
			    "VOP_SETACL()\n");
			/* panic("ufs_makeinode: VOP_GETACL() but no "
			    "VOP_SETACL()"); */
			break;

		default:
			uma_zfree(acl_zone, acl);
			goto bad;
		}
		uma_zfree(acl_zone, acl);
	}
#endif /* !UFS_ACL */
	ufs_makedirentry(ip, cnp, &newdir);
	error = ufs_direnter(dvp, tvp, &newdir, cnp, NULL);
	if (error)
		goto bad;
	*vpp = tvp;
	return (0);

bad:
	/*
	 * Write error occurred trying to update the inode
	 * or the directory so must deallocate the inode.
	 */
	ip->i_effnlink = 0;
	ip->i_nlink = 0;
	DIP_SET(ip, i_nlink, 0);
	ip->i_flag |= IN_CHANGE;
	if (DOINGSOFTDEP(tvp))
		softdep_change_linkcnt(ip);
	vput(tvp);
	return (error);
}

/* Global vfs data structures for ufs. */
struct vop_vector ufs_vnodeops = {
	.vop_default =		&default_vnodeops,
	.vop_fsync =		VOP_PANIC,
	.vop_read =		VOP_PANIC,
	.vop_reallocblks =	VOP_PANIC,
	.vop_write =		VOP_PANIC,
	.vop_access =		ufs_access,
	.vop_bmap =		ufs_bmap,
	.vop_cachedlookup =	ufs_lookup,
	.vop_close =		ufs_close,
	.vop_create =		ufs_create,
	.vop_getattr =		ufs_getattr,
	.vop_inactive =		ufs_inactive,
	.vop_link =		ufs_link,
	.vop_lookup =		vfs_cache_lookup,
	.vop_mkdir =		ufs_mkdir,
	.vop_mknod =		ufs_mknod,
	.vop_open =		ufs_open,
	.vop_pathconf =		ufs_pathconf,
	.vop_poll =		vop_stdpoll,
	.vop_print =		ufs_print,
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

struct vop_vector ufs_fifoops = {
	.vop_default =		&fifo_specops,
	.vop_fsync =		VOP_PANIC,
	.vop_access =		ufs_access,
	.vop_close =		ufsfifo_close,
	.vop_getattr =		ufs_getattr,
	.vop_inactive =		ufs_inactive,
	.vop_kqfilter =		ufsfifo_kqfilter,
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
};
