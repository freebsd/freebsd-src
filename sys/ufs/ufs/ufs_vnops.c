/*
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 * $FreeBSD$
 */

#include "opt_mac.h"
#include "opt_quota.h"
#include "opt_suiddir.h"
#include "opt_ufs.h"

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
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/lockf.h>
#include <sys/event.h>
#include <sys/conf.h>
#include <sys/acl.h>
#include <sys/mac.h>

#include <machine/mutex.h>

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

static int ufs_access(struct vop_access_args *);
static int ufs_advlock(struct vop_advlock_args *);
static int ufs_chmod(struct vnode *, int, struct ucred *, struct thread *);
static int ufs_chown(struct vnode *, uid_t, gid_t, struct ucred *, struct thread *);
static int ufs_close(struct vop_close_args *);
static int ufs_create(struct vop_create_args *);
static int ufs_getattr(struct vop_getattr_args *);
static int ufs_link(struct vop_link_args *);
static int ufs_makeinode(int mode, struct vnode *, struct vnode **, struct componentname *);
static int ufs_mkdir(struct vop_mkdir_args *);
static int ufs_mknod(struct vop_mknod_args *);
static int ufs_open(struct vop_open_args *);
static int ufs_pathconf(struct vop_pathconf_args *);
static int ufs_print(struct vop_print_args *);
static int ufs_readlink(struct vop_readlink_args *);
static int ufs_remove(struct vop_remove_args *);
static int ufs_rename(struct vop_rename_args *);
static int ufs_rmdir(struct vop_rmdir_args *);
static int ufs_setattr(struct vop_setattr_args *);
static int ufs_strategy(struct vop_strategy_args *);
static int ufs_symlink(struct vop_symlink_args *);
static int ufs_whiteout(struct vop_whiteout_args *);
static int ufsfifo_close(struct vop_close_args *);
static int ufsfifo_kqfilter(struct vop_kqfilter_args *);
static int ufsfifo_read(struct vop_read_args *);
static int ufsfifo_write(struct vop_write_args *);
static int ufsspec_close(struct vop_close_args *);
static int ufsspec_read(struct vop_read_args *);
static int ufsspec_write(struct vop_write_args *);
static int filt_ufsread(struct knote *kn, long hint);
static int filt_ufswrite(struct knote *kn, long hint);
static int filt_ufsvnode(struct knote *kn, long hint);
static void filt_ufsdetach(struct knote *kn);
static int ufs_kqfilter(struct vop_kqfilter_args *ap);

union _qcvt {
	int64_t qcvt;
	int32_t val[2];
};
#define SETHIGH(q, h) { \
	union _qcvt tmp; \
	tmp.qcvt = (q); \
	tmp.val[_QUAD_HIGHWORD] = (h); \
	(q) = tmp.qcvt; \
}
#define SETLOW(q, l) { \
	union _qcvt tmp; \
	tmp.qcvt = (q); \
	tmp.val[_QUAD_LOWWORD] = (l); \
	(q) = tmp.qcvt; \
}

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

void
ufs_itimes(vp)
	struct vnode *vp;
{
	struct inode *ip;
	struct timespec ts;

	ip = VTOI(vp);
	if ((ip->i_flag & (IN_ACCESS | IN_CHANGE | IN_UPDATE)) == 0)
		return;
	if ((vp->v_type == VBLK || vp->v_type == VCHR) && !DOINGSOFTDEP(vp))
		ip->i_flag |= IN_LAZYMOD;
	else
		ip->i_flag |= IN_MODIFIED;
	if ((vp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
		vfs_timestamp(&ts);
		if (ip->i_flag & IN_ACCESS) {
			DIP(ip, i_atime) = ts.tv_sec;
			DIP(ip, i_atimensec) = ts.tv_nsec;
		}
		if (ip->i_flag & IN_UPDATE) {
			DIP(ip, i_mtime) = ts.tv_sec;
			DIP(ip, i_mtimensec) = ts.tv_nsec;
			ip->i_modrev++;
		}
		if (ip->i_flag & IN_CHANGE) {
			DIP(ip, i_ctime) = ts.tv_sec;
			DIP(ip, i_ctimensec) = ts.tv_nsec;
		}
	}
	ip->i_flag &= ~(IN_ACCESS | IN_CHANGE | IN_UPDATE);
}

/*
 * Create a regular file
 */
int
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
	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	return (0);
}

/*
 * Mknod vnode call
 */
/* ARGSUSED */
int
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
	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	ip = VTOI(*vpp);
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	if (vap->va_rdev != VNOVAL) {
		/*
		 * Want to be able to use this to make badblock
		 * inodes, so don't truncate the dev number.
		 */
		DIP(ip, i_rdev) = vap->va_rdev;
	}
	/*
	 * Remove inode, then reload it through VFS_VGET so it is
	 * checked to see if it is an alias of an existing entry in
	 * the inode cache.
	 */
	vput(*vpp);
	(*vpp)->v_type = VNON;
	ino = ip->i_number;	/* Save this before vgone() invalidates ip. */
	vgone(*vpp);
	error = VFS_VGET(ap->a_dvp->v_mount, ino, LK_EXCLUSIVE, vpp);
	if (error) {
		*vpp = NULL;
		return (error);
	}
	return (0);
}

/*
 * Open called.
 *
 * Nothing to do.
 */
/* ARGSUSED */
int
ufs_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{

	/*
	 * Files marked append-only must be opened for appending.
	 */
	if ((VTOI(ap->a_vp)->i_flags & APPEND) &&
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
int
ufs_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct mount *mp;

	mtx_lock(&vp->v_interlock);
	if (vp->v_usecount > 1) {
		ufs_itimes(vp);
		mtx_unlock(&vp->v_interlock);
	} else {
		mtx_unlock(&vp->v_interlock);
		/*
		 * If we are closing the last reference to an unlinked
		 * file, then it will be freed by the inactive routine.
		 * Because the freeing causes a the filesystem to be
		 * modified, it must be held up during periods when the
		 * filesystem is suspended.
		 *
		 * XXX - EAGAIN is returned to prevent vn_close from
		 * repeating the vrele operation.
		 */
		if (vp->v_type == VREG && VTOI(vp)->i_effnlink == 0) {
			(void) vn_start_write(vp, &mp, V_WAIT);
			vrele(vp);
			vn_finished_write(mp);
			return (EAGAIN);
		}
	}
	return (0);
}

int
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
#ifdef UFS_ACL
	struct acl *acl;
	int len;
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
			if ((error = getinoquota(ip)) != 0)
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
	MALLOC(acl, struct acl *, sizeof(*acl), M_ACL, M_WAITOK);
	len = sizeof(*acl);
	error = VOP_GETACL(vp, ACL_TYPE_ACCESS, acl, ap->a_cred, ap->a_td);
	switch (error) {
	case EOPNOTSUPP:
		error = vaccess(vp->v_type, ip->i_mode, ip->i_uid, ip->i_gid,
		    ap->a_mode, ap->a_cred, NULL);
		break;
	case 0:
		error = vaccess_acl_posix1e(vp->v_type, ip->i_uid, ip->i_gid,
		    acl, ap->a_mode, ap->a_cred, NULL);
		break;
	default:
		printf("ufs_access(): Error retrieving ACL on object (%d).\n",
		    error);
		/*
		 * XXX: Fall back until debugged.  Should eventually
		 * possibly log an error, and return EPERM for safety.
		 */
		error = vaccess(vp->v_type, ip->i_mode, ip->i_uid, ip->i_gid,
		    ap->a_mode, ap->a_cred, NULL);
	}
	FREE(acl, M_ACL);
#else
	error = vaccess(vp->v_type, ip->i_mode, ip->i_uid, ip->i_gid,
	    ap->a_mode, ap->a_cred, NULL);
#endif
	return (error);
}

/* ARGSUSED */
int
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

	ufs_itimes(vp);
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
		vap->va_atime.tv_sec = ip->i_din1->di_atime;
		vap->va_atime.tv_nsec = ip->i_din1->di_atimensec;
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
		vap->va_atime.tv_sec = ip->i_din2->di_atime;
		vap->va_atime.tv_nsec = ip->i_din2->di_atimensec;
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
int
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
		 * Unprivileged processes and privileged processes in
		 * jail() are not permitted to unset system flags, or
		 * modify flags if any system flags are set.
		 * Privileged non-jail processes may not modify system flags
		 * if securelevel > 0 and any existing system flags are set.
		 */
		if (!suser_cred(cred, PRISON_ROOT)) {
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
			DIP(ip, i_flags) = vap->va_flags;
		} else {
			if (ip->i_flags
			    & (SF_NOUNLINK | SF_IMMUTABLE | SF_APPEND) ||
			    (vap->va_flags & UF_SETTABLE) != vap->va_flags)
				return (EPERM);
			ip->i_flags &= SF_SETTABLE;
			ip->i_flags |= (vap->va_flags & UF_SETTABLE);
			DIP(ip, i_flags) = ip->i_flags;
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
		 * Disallow write attempts on read-only filesystems;
		 * unless the file is a socket, fifo, or a block or
		 * character device resident on the filesystem.
		 */
		switch (vp->v_type) {
		case VDIR:
			return (EISDIR);
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
			if ((ip->i_flags & SF_SNAPSHOT) != 0)
				return (EPERM);
			break;
		default:
			break;
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
		 */
		if ((error = VOP_ACCESS(vp, VADMIN, cred, td)) &&
		    ((vap->va_vaflags & VA_UTIMES_NULL) == 0 ||
		    (error = VOP_ACCESS(vp, VWRITE, cred, td))))
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
			DIP(ip, i_atime) = vap->va_atime.tv_sec;
			DIP(ip, i_atimensec) = vap->va_atime.tv_nsec;
		}
		if (vap->va_mtime.tv_sec != VNOVAL) {
			DIP(ip, i_mtime) = vap->va_mtime.tv_sec;
			DIP(ip, i_mtimensec) = vap->va_mtime.tv_nsec;
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
	VN_KNOTE(vp, NOTE_ATTRIB);
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
	 * process is not a member of.
	 */
	if (suser_cred(cred, PRISON_ROOT)) {
		if (vp->v_type != VDIR && (mode & S_ISTXT))
			return (EFTYPE);
		if (!groupmember(ip->i_gid, cred) && (mode & ISGID))
			return (EPERM);
	}
	ip->i_mode &= ~ALLPERMS;
	ip->i_mode |= (mode & ALLPERMS);
	DIP(ip, i_mode) = ip->i_mode;
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
	 * To modify the ownership of a file, must possess VADMIN
	 * for that file.
	 */
	if ((error = VOP_ACCESS(vp, VADMIN, cred, td)))
		return (error);
	/*
	 * To change the owner of a file, or change the group of a file
	 * to a group of which we are not a member, the caller must
	 * have privilege.
	 */
	if ((uid != ip->i_uid || 
	    (gid != ip->i_gid && !groupmember(gid, cred))) &&
	    (error = suser_cred(cred, PRISON_ROOT)))
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
	DIP(ip, i_gid) = gid;
	ip->i_uid = uid;
	DIP(ip, i_uid) = uid;
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
	DIP(ip, i_gid) = ogid;
	ip->i_uid = ouid;
	DIP(ip, i_uid) = ouid;
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
	if (suser_cred(cred, PRISON_ROOT) && (ouid != uid || ogid != gid)) {
		ip->i_mode &= ~(ISUID | ISGID);
		DIP(ip, i_mode) = ip->i_mode;
	}
	return (0);
}

int
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

	ip = VTOI(vp);
	if ((ip->i_flags & (NOUNLINK | IMMUTABLE | APPEND)) ||
	    (VTOI(dvp)->i_flags & APPEND)) {
		error = EPERM;
		goto out;
	}
	error = ufs_dirremove(dvp, ip, ap->a_cnp->cn_flags, 0);
	if (ip->i_nlink <= 0)
		vp->v_vflag |= VV_NOSYNC;
	VN_KNOTE(vp, NOTE_DELETE);
	VN_KNOTE(dvp, NOTE_WRITE);
out:
	return (error);
}

/*
 * link vnode call
 */
int
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
	struct thread *td = cnp->cn_thread;
	struct inode *ip;
	struct direct newdir;
	int error;

#ifdef DIAGNOSTIC
	if ((cnp->cn_flags & HASBUF) == 0)
		panic("ufs_link: no name");
#endif
	if (tdvp->v_mount != vp->v_mount) {
		error = EXDEV;
		goto out2;
	}
	if (tdvp != vp && (error = vn_lock(vp, LK_EXCLUSIVE, td))) {
		goto out2;
	}
	ip = VTOI(vp);
	if ((nlink_t)ip->i_nlink >= LINK_MAX) {
		error = EMLINK;
		goto out1;
	}
	if (ip->i_flags & (IMMUTABLE | APPEND)) {
		error = EPERM;
		goto out1;
	}
	ip->i_effnlink++;
	ip->i_nlink++;
	DIP(ip, i_nlink) = ip->i_nlink;
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
		DIP(ip, i_nlink) = ip->i_nlink;
		ip->i_flag |= IN_CHANGE;
		if (DOINGSOFTDEP(vp))
			softdep_change_linkcnt(ip);
	}
out1:
	if (tdvp != vp)
		VOP_UNLOCK(vp, 0, td);
out2:
	VN_KNOTE(vp, NOTE_LINK);
	VN_KNOTE(tdvp, NOTE_WRITE);
	return (error);
}

/*
 * whiteout vnode call
 */
int
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
#ifdef DIAGNOSTIC
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
#ifdef DIAGNOSTIC
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
int
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

#ifdef DIAGNOSTIC
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

	if ((error = vn_lock(fvp, LK_EXCLUSIVE, td)) != 0)
		goto abortit;
	dp = VTOI(fdvp);
	ip = VTOI(fvp);
	if (ip->i_nlink >= LINK_MAX) {
		VOP_UNLOCK(fvp, 0, td);
		error = EMLINK;
		goto abortit;
	}
	if ((ip->i_flags & (NOUNLINK | IMMUTABLE | APPEND))
	    || (dp->i_flags & APPEND)) {
		VOP_UNLOCK(fvp, 0, td);
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
			VOP_UNLOCK(fvp, 0, td);
			error = EINVAL;
			goto abortit;
		}
		ip->i_flag |= IN_RENAME;
		oldparent = dp->i_number;
		doingdirectory = 1;
	}
	VN_KNOTE(fdvp, NOTE_WRITE);		/* XXX right place? */
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
	DIP(ip, i_nlink) = ip->i_nlink;
	ip->i_flag |= IN_CHANGE;
	if (DOINGSOFTDEP(fvp))
		softdep_change_linkcnt(ip);
	if ((error = UFS_UPDATE(fvp, !(DOINGSOFTDEP(fvp) |
				       DOINGASYNC(fvp)))) != 0) {
		VOP_UNLOCK(fvp, 0, td);
		goto bad;
	}

	/*
	 * If ".." must be changed (ie the directory gets a new
	 * parent) then the source directory must not be in the
	 * directory heirarchy above the target, as this would
	 * orphan everything below the source directory. Also
	 * the user must have write permission in the source so
	 * as to be able to change "..". We must repeat the call
	 * to namei, as the parent directory is unlocked by the
	 * call to checkpath().
	 */
	error = VOP_ACCESS(fvp, VWRITE, tcnp->cn_cred, tcnp->cn_thread);
	VOP_UNLOCK(fvp, 0, td);
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
			DIP(dp, i_nlink) = dp->i_nlink;
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
				DIP(dp, i_nlink) = dp->i_nlink;
				dp->i_flag |= IN_CHANGE;
				if (DOINGSOFTDEP(tdvp))
					softdep_change_linkcnt(dp);
				(void)UFS_UPDATE(tdvp, 1);
			}
			goto bad;
		}
		VN_KNOTE(tdvp, NOTE_WRITE);
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
				DIP(dp, i_nlink) = dp->i_nlink;
				dp->i_flag |= IN_CHANGE;
			}
			xp->i_nlink--;
			DIP(xp, i_nlink) = xp->i_nlink;
			xp->i_flag |= IN_CHANGE;
			ioflag = IO_NORMAL;
			if (DOINGASYNC(tvp))
				ioflag |= IO_SYNC;
			if ((error = UFS_TRUNCATE(tvp, (off_t)0, ioflag,
			    tcnp->cn_cred, tcnp->cn_thread)) != 0)
				goto bad;
		}
		VN_KNOTE(tdvp, NOTE_WRITE);
		vput(tdvp);
		VN_KNOTE(tvp, NOTE_DELETE);
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
	VN_KNOTE(fvp, NOTE_RENAME);
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
	if (vn_lock(fvp, LK_EXCLUSIVE, td) == 0) {
		ip->i_effnlink--;
		ip->i_nlink--;
		DIP(ip, i_nlink) = ip->i_nlink;
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
int
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

#ifdef DIAGNOSTIC
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
	DIP(ip, i_gid) = dp->i_gid;
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
			DIP(ip, i_uid) = dp->i_uid;
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
				ucred.cr_groups[0] = dp->i_gid;
				ucp = &ucred;
			}
#endif
		} else {
			ip->i_uid = cnp->cn_cred->cr_uid;
			DIP(ip, i_uid) = ip->i_uid;
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
	DIP(ip, i_uid) = ip->i_uid;
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
	MALLOC(acl, struct acl *, sizeof(*acl), M_ACL, M_WAITOK);
	MALLOC(dacl, struct acl *, sizeof(*acl), M_ACL, M_WAITOK);

	/*
	 * Retrieve default ACL from parent, if any.
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
			 * Two possible ways for default ACL to not be
			 * present.  First, the EA can be undefined,
			 * or second, the default ACL can be blank.
			 * If it's blank, fall through to the it's
			 * not defined case.
			 */
			ip->i_mode = dmode;
			DIP(ip, i_mode) = dmode;
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
		DIP(ip, i_mode) = dmode;
		FREE(acl, M_ACL);
		FREE(dacl, M_ACL);
		dacl = acl = NULL;
		break;
	
	default:
		UFS_VFREE(tvp, ip->i_number, dmode);
		vput(tvp);
		return (error);
	}
#else /* !UFS_ACL */
	ip->i_mode = dmode;
	DIP(ip, i_mode) = dmode;
#endif /* !UFS_ACL */
	tvp->v_type = VDIR;	/* Rest init'd in getnewvnode(). */
	ip->i_effnlink = 2;
	ip->i_nlink = 2;
	DIP(ip, i_nlink) = 2;
	if (DOINGSOFTDEP(tvp))
		softdep_change_linkcnt(ip);
	if (cnp->cn_flags & ISWHITEOUT) {
		ip->i_flags |= UF_OPAQUE;
		DIP(ip, i_flags) = ip->i_flags;
	}

	/*
	 * Bump link count in parent directory to reflect work done below.
	 * Should be done before reference is created so cleanup is
	 * possible if we crash.
	 */
	dp->i_effnlink++;
	dp->i_nlink++;
	DIP(dp, i_nlink) = dp->i_nlink;
	dp->i_flag |= IN_CHANGE;
	if (DOINGSOFTDEP(dvp))
		softdep_change_linkcnt(dp);
	error = UFS_UPDATE(tvp, !(DOINGSOFTDEP(dvp) | DOINGASYNC(dvp)));
	if (error)
		goto bad;
#ifdef MAC
	error = vop_stdcreatevnode_ea(dvp, tvp, cnp->cn_cred);
	if (error)
		goto bad;
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
			FREE(acl, M_ACL);
			FREE(dacl, M_ACL);
			goto bad;
		}
		FREE(acl, M_ACL);
		FREE(dacl, M_ACL);
	}
#endif /* !UFS_ACL */

	/*
	 * Initialize directory with "." and ".." from static template.
	 */
	if (dvp->v_mount->mnt_maxsymlinklen > 0
	)
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
	DIP(ip, i_size) = DIRBLKSIZ;
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
		(void)BUF_WRITE(bp);
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
	else if (!DOINGSOFTDEP(dvp) && ((error = BUF_WRITE(bp))))
		goto bad;
	ufs_makedirentry(ip, cnp, &newdir);
	error = ufs_direnter(dvp, tvp, &newdir, cnp, bp);
	
bad:
	if (error == 0) {
		VN_KNOTE(dvp, NOTE_WRITE | NOTE_LINK);
		*ap->a_vpp = tvp;
	} else {
		dp->i_effnlink--;
		dp->i_nlink--;
		DIP(dp, i_nlink) = dp->i_nlink;
		dp->i_flag |= IN_CHANGE;
		if (DOINGSOFTDEP(dvp))
			softdep_change_linkcnt(dp);
		/*
		 * No need to do an explicit VOP_TRUNCATE here, vrele will
		 * do this for us because we set the link count to 0.
		 */
		ip->i_effnlink = 0;
		ip->i_nlink = 0;
		DIP(ip, i_nlink) = 0;
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
int
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
	if (ip->i_flag & IN_RENAME) {
		error = EINVAL;
		goto out;
	}
	if (ip->i_effnlink != 2 ||
	    !ufs_dirempty(ip, dp->i_number, cnp->cn_cred)) {
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
	VN_KNOTE(dvp, NOTE_WRITE | NOTE_LINK);
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
		DIP(dp, i_nlink) = dp->i_nlink;
		dp->i_flag |= IN_CHANGE;
		ip->i_nlink--;
		DIP(ip, i_nlink) = ip->i_nlink;
		ip->i_flag |= IN_CHANGE;
		ioflag = IO_NORMAL;
		if (DOINGASYNC(vp))
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
	VN_KNOTE(vp, NOTE_DELETE);
	return (error);
}

/*
 * symlink -- make a symbolic link
 */
int
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
	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	vp = *vpp;
	len = strlen(ap->a_target);
	if (len < vp->v_mount->mnt_maxsymlinklen) {
		ip = VTOI(vp);
		bcopy(ap->a_target, SHORTLINK(ip), len);
		ip->i_size = len;
		DIP(ip, i_size) = len;
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
		int *ncookies;
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
		     (uio->uio_iov->iov_base - (uio->uio_offset - off));
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
int
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
		uiomove(SHORTLINK(ip), isize, ap->a_uio);
		return (0);
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
int
ufs_strategy(ap)
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap;
{
	struct buf *bp = ap->a_bp;
	struct vnode *vp = ap->a_vp;
	struct inode *ip;
	ufs2_daddr_t blkno;
	int error;

	ip = VTOI(vp);
	if (vp->v_type == VBLK || vp->v_type == VCHR)
		panic("ufs_strategy: spec");
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
	vp = ip->i_devvp;
	bp->b_dev = vp->v_rdev;
	VOP_STRATEGY(vp, bp);
	return (0);
}

/*
 * Print out the contents of an inode.
 */
int
ufs_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);

	printf("tag VT_UFS, ino %lu, on dev %s (%d, %d)",
	    (u_long)ip->i_number, devtoname(ip->i_dev), major(ip->i_dev),
	    minor(ip->i_dev));
	if (vp->v_type == VFIFO)
		fifo_printinfo(vp);
	lockmgr_printinfo(&vp->v_lock);
	printf("\n");
	return (0);
}

/*
 * Read wrapper for special devices.
 */
int
ufsspec_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	int error, resid;
	struct inode *ip;
	struct uio *uio;

	uio = ap->a_uio;
	resid = uio->uio_resid;
	error = VOCALL(spec_vnodeop_p, VOFFSET(vop_read), ap);
	/*
	 * The inode may have been revoked during the call, so it must not
	 * be accessed blindly here or in the other wrapper functions.
	 */
	ip = VTOI(ap->a_vp);
	if (ip != NULL && (uio->uio_resid != resid || (error == 0 && resid != 0)))
		ip->i_flag |= IN_ACCESS;
	return (error);
}

/*
 * Write wrapper for special devices.
 */
int
ufsspec_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	int error, resid;
	struct inode *ip;
	struct uio *uio;

	uio = ap->a_uio;
	resid = uio->uio_resid;
	error = VOCALL(spec_vnodeop_p, VOFFSET(vop_write), ap);
	ip = VTOI(ap->a_vp);
	if (ip != NULL && (uio->uio_resid != resid || (error == 0 && resid != 0)))
		VTOI(ap->a_vp)->i_flag |= IN_CHANGE | IN_UPDATE;
	return (error);
}

/*
 * Close wrapper for special devices.
 *
 * Update the times on the inode then do device close.
 */
int
ufsspec_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	mtx_lock(&vp->v_interlock);
	if (vp->v_usecount > 1)
		ufs_itimes(vp);
	mtx_unlock(&vp->v_interlock);
	return (VOCALL(spec_vnodeop_p, VOFFSET(vop_close), ap));
}

/*
 * Read wrapper for fifos.
 */
int
ufsfifo_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	int error, resid;
	struct inode *ip;
	struct uio *uio;

	uio = ap->a_uio;
	resid = uio->uio_resid;
	error = VOCALL(fifo_vnodeop_p, VOFFSET(vop_read), ap);
	ip = VTOI(ap->a_vp);
	if ((ap->a_vp->v_mount->mnt_flag & MNT_NOATIME) == 0 && ip != NULL &&
	    (uio->uio_resid != resid || (error == 0 && resid != 0)))
		VTOI(ap->a_vp)->i_flag |= IN_ACCESS;
	return (error);
}

/*
 * Write wrapper for fifos.
 */
int
ufsfifo_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	int error, resid;
	struct inode *ip;
	struct uio *uio;

	uio = ap->a_uio;
	resid = uio->uio_resid;
	error = VOCALL(fifo_vnodeop_p, VOFFSET(vop_write), ap);
	ip = VTOI(ap->a_vp);
	if (ip != NULL && (uio->uio_resid != resid || (error == 0 && resid != 0)))
		VTOI(ap->a_vp)->i_flag |= IN_CHANGE | IN_UPDATE;
	return (error);
}

/*
 * Close wrapper for fifos.
 *
 * Update the times on the inode then do device close.
 */
int
ufsfifo_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	mtx_lock(&vp->v_interlock);
	if (vp->v_usecount > 1)
		ufs_itimes(vp);
	mtx_unlock(&vp->v_interlock);
	return (VOCALL(fifo_vnodeop_p, VOFFSET(vop_close), ap));
}

/*
 * Kqfilter wrapper for fifos.
 *
 * Fall through to ufs kqfilter routines if needed 
 */
int
ufsfifo_kqfilter(ap)
	struct vop_kqfilter_args *ap;
{
	int error;

	error = VOCALL(fifo_vnodeop_p, VOFFSET(vop_kqfilter), ap);
	if (error)
		error = ufs_kqfilter(ap);
	return (error);
}

/*
 * Return POSIX pathconf information applicable to ufs filesystems.
 */
int
ufs_pathconf(ap)
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		int *a_retval;
	} */ *ap;
{

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = LINK_MAX;
		return (0);
	case _PC_NAME_MAX:
		*ap->a_retval = NAME_MAX;
		return (0);
	case _PC_PATH_MAX:
		*ap->a_retval = PATH_MAX;
		return (0);
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_NO_TRUNC:
		*ap->a_retval = 1;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Advisory record locking support
 */
int
ufs_advlock(ap)
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		caddr_t  a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
	} */ *ap;
{
	struct inode *ip = VTOI(ap->a_vp);

	return (lf_advlock(ap, &(ip->i_lockf), ip->i_size));
}

/*
 * Initialize the vnode associated with a new inode, handle aliased
 * vnodes.
 */
int
ufs_vinit(mntp, specops, fifoops, vpp)
	struct mount *mntp;
	vop_t **specops;
	vop_t **fifoops;
	struct vnode **vpp;
{
	struct inode *ip;
	struct vnode *vp;
	struct timeval tv;

	vp = *vpp;
	ip = VTOI(vp);
	switch(vp->v_type = IFTOVT(ip->i_mode)) {
	case VCHR:
	case VBLK:
		vp->v_op = specops;
		vp = addaliasu(vp, DIP(ip, i_rdev));
		ip->i_vnode = vp;
		break;
	case VFIFO:
		vp->v_op = fifoops;
		break;
	default:
		break;

	}
	ASSERT_VOP_LOCKED(vp, "ufs_vinit");
	if (ip->i_number == ROOTINO)
		vp->v_vflag |= VV_ROOT;
	/*
	 * Initialize modrev times
	 */
	getmicrouptime(&tv);
	SETHIGH(ip->i_modrev, tv.tv_sec);
	SETLOW(ip->i_modrev, tv.tv_usec * 4294);
	*vpp = vp;
	return (0);
}

/*
 * Allocate a new inode.
 * Vnode dvp must be locked.
 */
int
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
#ifdef DIAGNOSTIC
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
	DIP(ip, i_gid) = pdir->i_gid;
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
			DIP(ip, i_uid) = ip->i_uid;
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
			ucred.cr_groups[0] = pdir->i_gid;
			ucp = &ucred;
#endif
		} else {
			ip->i_uid = cnp->cn_cred->cr_uid;
			DIP(ip, i_uid) = ip->i_uid;
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
	DIP(ip, i_uid) = ip->i_uid;
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
	MALLOC(acl, struct acl *, sizeof(*acl), M_ACL, M_WAITOK);
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
			 * Two possible ways for default ACL to not be
			 * present.  First, the EA can be undefined,
			 * or second, the default ACL can be blank.
			 * If it's blank, fall through to the it's
			 * not defined case.
			 */
			ip->i_mode = mode;
			DIP(ip, i_mode) = mode;
			ufs_sync_acl_from_inode(ip, acl);
			break;
		}

	case EOPNOTSUPP:
		/*
		 * Just use the mode as-is.
		 */
		ip->i_mode = mode;
		DIP(ip, i_mode) = mode;
		FREE(acl, M_ACL);
		acl = NULL;
		break;

	default:
		UFS_VFREE(tvp, ip->i_number, mode);
		vput(tvp);
		return (error);
	}
#else /* !UFS_ACL */
	ip->i_mode = mode;
	DIP(ip, i_mode) = mode;
#endif /* !UFS_ACL */
	tvp->v_type = IFTOVT(mode);	/* Rest init'd in getnewvnode(). */
	ip->i_effnlink = 1;
	ip->i_nlink = 1;
	DIP(ip, i_nlink) = 1;
	if (DOINGSOFTDEP(tvp))
		softdep_change_linkcnt(ip);
	if ((ip->i_mode & ISGID) && !groupmember(ip->i_gid, cnp->cn_cred) &&
	    suser_cred(cnp->cn_cred, PRISON_ROOT)) {
		ip->i_mode &= ~ISGID;
		DIP(ip, i_mode) = ip->i_mode;
	}

	if (cnp->cn_flags & ISWHITEOUT) {
		ip->i_flags |= UF_OPAQUE;
		DIP(ip, i_flags) = ip->i_flags;
	}

	/*
	 * Make sure inode goes to disk before directory entry.
	 */
	error = UFS_UPDATE(tvp, !(DOINGSOFTDEP(tvp) | DOINGASYNC(tvp)));
	if (error)
		goto bad;
#ifdef MAC
	error = vop_stdcreatevnode_ea(dvp, tvp, cnp->cn_cred);
	if (error)
		goto bad;
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
			FREE(acl, M_ACL);
			goto bad;
		}
		FREE(acl, M_ACL);
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
	DIP(ip, i_nlink) = 0;
	ip->i_flag |= IN_CHANGE;
	if (DOINGSOFTDEP(tvp))
		softdep_change_linkcnt(ip);
	vput(tvp);
	return (error);
}

static struct filterops ufsread_filtops = 
	{ 1, NULL, filt_ufsdetach, filt_ufsread };
static struct filterops ufswrite_filtops = 
	{ 1, NULL, filt_ufsdetach, filt_ufswrite };
static struct filterops ufsvnode_filtops = 
	{ 1, NULL, filt_ufsdetach, filt_ufsvnode };

static int
ufs_kqfilter(ap)
	struct vop_kqfilter_args /* {
		struct vnode *a_vp;
		struct knote *a_kn;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct knote *kn = ap->a_kn;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &ufsread_filtops;
		break;
	case EVFILT_WRITE:
		kn->kn_fop = &ufswrite_filtops;
		break;
	case EVFILT_VNODE:
		kn->kn_fop = &ufsvnode_filtops;
		break;
	default:
		return (1);
	}

	kn->kn_hook = (caddr_t)vp;

	if (vp->v_pollinfo == NULL)
		v_addpollinfo(vp);
	mtx_lock(&vp->v_pollinfo->vpi_lock);
	SLIST_INSERT_HEAD(&vp->v_pollinfo->vpi_selinfo.si_note, kn, kn_selnext);
	mtx_unlock(&vp->v_pollinfo->vpi_lock);

	return (0);
}

static void
filt_ufsdetach(struct knote *kn)
{
	struct vnode *vp = (struct vnode *)kn->kn_hook;

	KASSERT(vp->v_pollinfo != NULL, ("Mising v_pollinfo"));
	mtx_lock(&vp->v_pollinfo->vpi_lock);
	SLIST_REMOVE(&vp->v_pollinfo->vpi_selinfo.si_note,
	    kn, knote, kn_selnext);
	mtx_unlock(&vp->v_pollinfo->vpi_lock);
}

/*ARGSUSED*/
static int
filt_ufsread(struct knote *kn, long hint)
{
	struct vnode *vp = (struct vnode *)kn->kn_hook;
	struct inode *ip = VTOI(vp);

	/*
	 * filesystem is gone, so set the EOF flag and schedule 
	 * the knote for deletion.
	 */
	if (hint == NOTE_REVOKE) {
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
		return (1);
	}

        kn->kn_data = ip->i_size - kn->kn_fp->f_offset;
        return (kn->kn_data != 0);
}

/*ARGSUSED*/
static int
filt_ufswrite(struct knote *kn, long hint)
{

	/*
	 * filesystem is gone, so set the EOF flag and schedule 
	 * the knote for deletion.
	 */
	if (hint == NOTE_REVOKE)
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);

        kn->kn_data = 0;
        return (1);
}

static int
filt_ufsvnode(struct knote *kn, long hint)
{

	if (kn->kn_sfflags & hint)
		kn->kn_fflags |= hint;
	if (hint == NOTE_REVOKE) {
		kn->kn_flags |= EV_EOF;
		return (1);
	}
	return (kn->kn_fflags != 0);
}

/* Global vfs data structures for ufs. */
static vop_t **ufs_vnodeop_p;
static struct vnodeopv_entry_desc ufs_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vop_defaultop },
	{ &vop_fsync_desc,		(vop_t *) vop_panic },
	{ &vop_read_desc,		(vop_t *) vop_panic },
	{ &vop_reallocblks_desc,	(vop_t *) vop_panic },
	{ &vop_write_desc,		(vop_t *) vop_panic },
	{ &vop_access_desc,		(vop_t *) ufs_access },
	{ &vop_advlock_desc,		(vop_t *) ufs_advlock },
	{ &vop_bmap_desc,		(vop_t *) ufs_bmap },
	{ &vop_cachedlookup_desc,	(vop_t *) ufs_lookup },
	{ &vop_close_desc,		(vop_t *) ufs_close },
	{ &vop_create_desc,		(vop_t *) ufs_create },
	{ &vop_getattr_desc,		(vop_t *) ufs_getattr },
	{ &vop_inactive_desc,		(vop_t *) ufs_inactive },
	{ &vop_islocked_desc,		(vop_t *) vop_stdislocked },
	{ &vop_link_desc,		(vop_t *) ufs_link },
	{ &vop_lock_desc,		(vop_t *) vop_stdlock },
	{ &vop_lookup_desc,		(vop_t *) vfs_cache_lookup },
	{ &vop_mkdir_desc,		(vop_t *) ufs_mkdir },
	{ &vop_mknod_desc,		(vop_t *) ufs_mknod },
	{ &vop_open_desc,		(vop_t *) ufs_open },
	{ &vop_pathconf_desc,		(vop_t *) ufs_pathconf },
	{ &vop_poll_desc,		(vop_t *) vop_stdpoll },
	{ &vop_kqfilter_desc,		(vop_t *) ufs_kqfilter },
	{ &vop_getwritemount_desc, 	(vop_t *) vop_stdgetwritemount },
	{ &vop_print_desc,		(vop_t *) ufs_print },
	{ &vop_readdir_desc,		(vop_t *) ufs_readdir },
	{ &vop_readlink_desc,		(vop_t *) ufs_readlink },
	{ &vop_reclaim_desc,		(vop_t *) ufs_reclaim },
#ifdef MAC
	{ &vop_refreshlabel_desc,	(vop_t *) vop_stdrefreshlabel_ea },
#endif
	{ &vop_remove_desc,		(vop_t *) ufs_remove },
	{ &vop_rename_desc,		(vop_t *) ufs_rename },
	{ &vop_rmdir_desc,		(vop_t *) ufs_rmdir },
	{ &vop_setattr_desc,		(vop_t *) ufs_setattr },
#ifdef MAC
	{ &vop_setlabel_desc,		(vop_t *) vop_stdsetlabel_ea },
#endif
	{ &vop_strategy_desc,		(vop_t *) ufs_strategy },
	{ &vop_symlink_desc,		(vop_t *) ufs_symlink },
	{ &vop_unlock_desc,		(vop_t *) vop_stdunlock },
	{ &vop_whiteout_desc,		(vop_t *) ufs_whiteout },
#ifdef UFS_EXTATTR
	{ &vop_getextattr_desc,		(vop_t *) ufs_getextattr },
	{ &vop_setextattr_desc,		(vop_t *) ufs_setextattr },
#endif
#ifdef UFS_ACL
	{ &vop_getacl_desc,		(vop_t *) ufs_getacl },
	{ &vop_setacl_desc,		(vop_t *) ufs_setacl },
	{ &vop_aclcheck_desc,		(vop_t *) ufs_aclcheck },
#endif
	{ NULL, NULL }
};
static struct vnodeopv_desc ufs_vnodeop_opv_desc =
	{ &ufs_vnodeop_p, ufs_vnodeop_entries };

static vop_t **ufs_specop_p;
static struct vnodeopv_entry_desc ufs_specop_entries[] = {
	{ &vop_default_desc,		(vop_t *) spec_vnoperate },
	{ &vop_fsync_desc,		(vop_t *) vop_panic },
	{ &vop_access_desc,		(vop_t *) ufs_access },
	{ &vop_close_desc,		(vop_t *) ufsspec_close },
	{ &vop_getattr_desc,		(vop_t *) ufs_getattr },
	{ &vop_inactive_desc,		(vop_t *) ufs_inactive },
	{ &vop_islocked_desc,		(vop_t *) vop_stdislocked },
	{ &vop_lock_desc,		(vop_t *) vop_stdlock },
	{ &vop_print_desc,		(vop_t *) ufs_print },
	{ &vop_read_desc,		(vop_t *) ufsspec_read },
	{ &vop_reclaim_desc,		(vop_t *) ufs_reclaim },
#ifdef MAC
	{ &vop_refreshlabel_desc,	(vop_t *) vop_stdrefreshlabel_ea },
#endif
	{ &vop_setattr_desc,		(vop_t *) ufs_setattr },
#ifdef MAC
	{ &vop_setlabel_desc,		(vop_t *) vop_stdsetlabel_ea },
#endif
	{ &vop_unlock_desc,		(vop_t *) vop_stdunlock },
	{ &vop_write_desc,		(vop_t *) ufsspec_write },
#ifdef UFS_EXTATTR
	{ &vop_getextattr_desc,		(vop_t *) ufs_getextattr },
	{ &vop_setextattr_desc,		(vop_t *) ufs_setextattr },
#endif
#ifdef UFS_ACL
	{ &vop_getacl_desc,		(vop_t *) ufs_getacl },
	{ &vop_setacl_desc,		(vop_t *) ufs_setacl },
	{ &vop_aclcheck_desc,		(vop_t *) ufs_aclcheck },
#endif
	{NULL, NULL}
};
static struct vnodeopv_desc ufs_specop_opv_desc =
	{ &ufs_specop_p, ufs_specop_entries };

static vop_t **ufs_fifoop_p;
static struct vnodeopv_entry_desc ufs_fifoop_entries[] = {
	{ &vop_default_desc,		(vop_t *) fifo_vnoperate },
	{ &vop_fsync_desc,		(vop_t *) vop_panic },
	{ &vop_access_desc,		(vop_t *) ufs_access },
	{ &vop_close_desc,		(vop_t *) ufsfifo_close },
	{ &vop_getattr_desc,		(vop_t *) ufs_getattr },
	{ &vop_inactive_desc,		(vop_t *) ufs_inactive },
	{ &vop_islocked_desc,		(vop_t *) vop_stdislocked },
	{ &vop_kqfilter_desc,		(vop_t *) ufsfifo_kqfilter },
	{ &vop_lock_desc,		(vop_t *) vop_stdlock },
	{ &vop_print_desc,		(vop_t *) ufs_print },
	{ &vop_read_desc,		(vop_t *) ufsfifo_read },
	{ &vop_reclaim_desc,		(vop_t *) ufs_reclaim },
#ifdef MAC
	{ &vop_refreshlabel_desc,	(vop_t *) vop_stdrefreshlabel_ea },
#endif
	{ &vop_setattr_desc,		(vop_t *) ufs_setattr },
#ifdef MAC
	{ &vop_setlabel_desc,		(vop_t *) vop_stdsetlabel_ea },
#endif
	{ &vop_unlock_desc,		(vop_t *) vop_stdunlock },
	{ &vop_write_desc,		(vop_t *) ufsfifo_write },
#ifdef UFS_EXTATTR
	{ &vop_getextattr_desc,		(vop_t *) ufs_getextattr },
	{ &vop_setextattr_desc,		(vop_t *) ufs_setextattr },
#endif
#ifdef UFS_ACL
	{ &vop_getacl_desc,		(vop_t *) ufs_getacl },
	{ &vop_setacl_desc,		(vop_t *) ufs_setacl },
	{ &vop_aclcheck_desc,		(vop_t *) ufs_aclcheck },
#endif
	{ NULL, NULL }
};
static struct vnodeopv_desc ufs_fifoop_opv_desc =
	{ &ufs_fifoop_p, ufs_fifoop_entries };

VNODEOP_SET(ufs_vnodeop_opv_desc);
VNODEOP_SET(ufs_specop_opv_desc);
VNODEOP_SET(ufs_fifoop_opv_desc);

int
ufs_vnoperate(ap)
	struct vop_generic_args /* {
		struct vnodeop_desc *a_desc;
	} */ *ap;
{
	return (VOCALL(ufs_vnodeop_p, ap->a_desc->vdesc_offset, ap));
}

int
ufs_vnoperatefifo(ap)
	struct vop_generic_args /* {
		struct vnodeop_desc *a_desc;
	} */ *ap;
{
	return (VOCALL(ufs_fifoop_p, ap->a_desc->vdesc_offset, ap));
}

int
ufs_vnoperatespec(ap)
	struct vop_generic_args /* {
		struct vnodeop_desc *a_desc;
	} */ *ap;
{
	return (VOCALL(ufs_specop_p, ap->a_desc->vdesc_offset, ap));
}
