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
 * $Id: ufs_vnops.c,v 1.78 1998/02/09 06:11:14 eivind Exp $
 */

#include "opt_quota.h"
#include "opt_suiddir.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/lockf.h>
#include <sys/poll.h>

#include <vm/vm_zone.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <miscfs/specfs/specdev.h>
#include <miscfs/fifofs/fifo.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

static int ufs_abortop __P((struct vop_abortop_args *));
static int ufs_access __P((struct vop_access_args *));
static int ufs_advlock __P((struct vop_advlock_args *));
static int ufs_chmod __P((struct vnode *, int, struct ucred *, struct proc *));
static int ufs_chown __P((struct vnode *, uid_t, gid_t, struct ucred *, struct proc *));
static int ufs_close __P((struct vop_close_args *));
static int ufs_create __P((struct vop_create_args *));
static int ufs_getattr __P((struct vop_getattr_args *));
static int ufs_link __P((struct vop_link_args *));
static int ufs_makeinode __P((int mode, struct vnode *, struct vnode **, struct componentname *));
static int ufs_missingop __P((struct vop_generic_args *ap));
static int ufs_mkdir __P((struct vop_mkdir_args *));
static int ufs_mknod __P((struct vop_mknod_args *));
static int ufs_mmap __P((struct vop_mmap_args *));
static int ufs_open __P((struct vop_open_args *));
static int ufs_pathconf __P((struct vop_pathconf_args *));
static int ufs_print __P((struct vop_print_args *));
static int ufs_readdir __P((struct vop_readdir_args *));
static int ufs_readlink __P((struct vop_readlink_args *));
static int ufs_remove __P((struct vop_remove_args *));
static int ufs_rename __P((struct vop_rename_args *));
static int ufs_rmdir __P((struct vop_rmdir_args *));
static int ufs_setattr __P((struct vop_setattr_args *));
static int ufs_strategy __P((struct vop_strategy_args *));
static int ufs_symlink __P((struct vop_symlink_args *));
static int ufs_whiteout __P((struct vop_whiteout_args *));
static int ufsfifo_close __P((struct vop_close_args *));
static int ufsfifo_read __P((struct vop_read_args *));
static int ufsfifo_write __P((struct vop_write_args *));
static int ufsspec_close __P((struct vop_close_args *));
static int ufsspec_read __P((struct vop_read_args *));
static int ufsspec_write __P((struct vop_write_args *));


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
	VN_POLLEVENT(ap->a_dvp, POLLWRITE);
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
	int error;

	error = ufs_makeinode(MAKEIMODE(vap->va_type, vap->va_mode),
	    ap->a_dvp, vpp, ap->a_cnp);
	if (error)
		return (error);
	VN_POLLEVENT(ap->a_dvp, POLLWRITE);
	ip = VTOI(*vpp);
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	if (vap->va_rdev != VNOVAL) {
		/*
		 * Want to be able to use this to make badblock
		 * inodes, so don't truncate the dev number.
		 */
		ip->i_rdev = vap->va_rdev;
	}
	/*
	 * Remove inode so that it will be reloaded by VFS_VGET and
	 * checked to see if it is an alias of an existing entry in
	 * the inode cache.
	 */
	vput(*vpp);
	(*vpp)->v_type = VNON;
	vgone(*vpp);
	*vpp = 0;
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
		struct proc *a_p;
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
		struct proc *a_p;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct inode *ip = VTOI(vp);

	simple_lock(&vp->v_interlock);
	if (vp->v_usecount > 1)
		ITIMES(ip, &time, &time);
	simple_unlock(&vp->v_interlock);
	return (0);
}

int
ufs_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct ucred *cred = ap->a_cred;
	mode_t mask, mode = ap->a_mode;
	register gid_t *gp;
	int i;
#ifdef QUOTA
	int error;
#endif

	/*
	 * Disallow write attempts on read-only file systems;
	 * unless the file is a socket, fifo, or a block or
	 * character device resident on the file system.
	 */
	if (mode & VWRITE) {
		switch (vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
#ifdef QUOTA
			if (error = getinoquota(ip))
				return (error);
#endif
			break;
		default:
			break;
		}
	}

	/* If immutable bit set, nobody gets to write it. */
	if ((mode & VWRITE) && (ip->i_flags & IMMUTABLE))
		return (EPERM);

	/* Otherwise, user id 0 always gets access. */
	if (cred->cr_uid == 0)
		return (0);

	mask = 0;

	/* Otherwise, check the owner. */
	if (cred->cr_uid == ip->i_uid) {
		if (mode & VEXEC)
			mask |= S_IXUSR;
		if (mode & VREAD)
			mask |= S_IRUSR;
		if (mode & VWRITE)
			mask |= S_IWUSR;
		return ((ip->i_mode & mask) == mask ? 0 : EACCES);
	}

	/* Otherwise, check the groups. */
	for (i = 0, gp = cred->cr_groups; i < cred->cr_ngroups; i++, gp++)
		if (ip->i_gid == *gp) {
			if (mode & VEXEC)
				mask |= S_IXGRP;
			if (mode & VREAD)
				mask |= S_IRGRP;
			if (mode & VWRITE)
				mask |= S_IWGRP;
			return ((ip->i_mode & mask) == mask ? 0 : EACCES);
		}

	/* Otherwise, check everyone else. */
	if (mode & VEXEC)
		mask |= S_IXOTH;
	if (mode & VREAD)
		mask |= S_IROTH;
	if (mode & VWRITE)
		mask |= S_IWOTH;
	return ((ip->i_mode & mask) == mask ? 0 : EACCES);
}

/* ARGSUSED */
int
ufs_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct inode *ip = VTOI(vp);
	register struct vattr *vap = ap->a_vap;

	ITIMES(ip, &time, &time);
	/*
	 * Copy from inode table
	 */
	vap->va_fsid = ip->i_dev;
	vap->va_fileid = ip->i_number;
	vap->va_mode = ip->i_mode & ~IFMT;
	vap->va_nlink = ip->i_effnlink;
	vap->va_uid = ip->i_uid;
	vap->va_gid = ip->i_gid;
	vap->va_rdev = (dev_t)ip->i_rdev;
	vap->va_size = ip->i_din.di_size;
	vap->va_atime.tv_sec = ip->i_atime;
	vap->va_atime.tv_nsec = ip->i_atimensec;
	vap->va_mtime.tv_sec = ip->i_mtime;
	vap->va_mtime.tv_nsec = ip->i_mtimensec;
	vap->va_ctime.tv_sec = ip->i_ctime;
	vap->va_ctime.tv_nsec = ip->i_ctimensec;
	vap->va_flags = ip->i_flags;
	vap->va_gen = ip->i_gen;
	/* this doesn't belong here */
	if (vp->v_type == VBLK)
		vap->va_blocksize = BLKDEV_IOSIZE;
	else if (vp->v_type == VCHR)
		vap->va_blocksize = MAXBSIZE;
	else
		vap->va_blocksize = vp->v_mount->mnt_stat.f_iosize;
	vap->va_bytes = dbtob((u_quad_t)ip->i_blocks);
	vap->va_type = vp->v_type;
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
		struct proc *a_p;
	} */ *ap;
{
	struct vattr *vap = ap->a_vap;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct ucred *cred = ap->a_cred;
	struct proc *p = ap->a_p;
	struct timeval atimeval, mtimeval;
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
		if (cred->cr_uid != ip->i_uid &&
		    (error = suser(cred, &p->p_acflag)))
			return (error);
		if (cred->cr_uid == 0) {
			if ((ip->i_flags
			    & (SF_NOUNLINK | SF_IMMUTABLE | SF_APPEND)) &&
			    securelevel > 0)
				return (EPERM);
			ip->i_flags = vap->va_flags;
		} else {
			if (ip->i_flags
			    & (SF_NOUNLINK | SF_IMMUTABLE | SF_APPEND) ||
			    (vap->va_flags & UF_SETTABLE) != vap->va_flags)
				return (EPERM);
			ip->i_flags &= SF_SETTABLE;
			ip->i_flags |= (vap->va_flags & UF_SETTABLE);
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
		if (error = ufs_chown(vp, vap->va_uid, vap->va_gid, cred, p))
			return (error);
	}
	if (vap->va_size != VNOVAL) {
		/*
		 * Disallow write attempts on read-only file systems;
		 * unless the file is a socket, fifo, or a block or
		 * character device resident on the file system.
		 */
		switch (vp->v_type) {
		case VDIR:
			return (EISDIR);
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
			break;
		default:
			break;
		}
		if (error = UFS_TRUNCATE(vp, vap->va_size, 0, cred, p))
			return (error);
	}
	ip = VTOI(vp);
	if (vap->va_atime.tv_sec != VNOVAL || vap->va_mtime.tv_sec != VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		if (cred->cr_uid != ip->i_uid &&
		    (error = suser(cred, &p->p_acflag)) &&
		    ((vap->va_vaflags & VA_UTIMES_NULL) == 0 ||
		    (error = VOP_ACCESS(vp, VWRITE, cred, p))))
			return (error);
		if (vap->va_atime.tv_sec != VNOVAL)
			ip->i_flag |= IN_ACCESS;
		if (vap->va_mtime.tv_sec != VNOVAL)
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
		atimeval.tv_sec = vap->va_atime.tv_sec;
		atimeval.tv_usec = vap->va_atime.tv_nsec / 1000;
		mtimeval.tv_sec = vap->va_mtime.tv_sec;
		mtimeval.tv_usec = vap->va_mtime.tv_nsec / 1000;
		error = UFS_UPDATE(vp, &atimeval, &mtimeval, 0);
		if (error)
			return (error);
	}
	error = 0;
	if (vap->va_mode != (mode_t)VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		error = ufs_chmod(vp, (int)vap->va_mode, cred, p);
	}
	VN_POLLEVENT(vp, POLLATTRIB);
	return (error);
}

/*
 * Change the mode on a file.
 * Inode must be locked before calling.
 */
static int
ufs_chmod(vp, mode, cred, p)
	register struct vnode *vp;
	register int mode;
	register struct ucred *cred;
	struct proc *p;
{
	register struct inode *ip = VTOI(vp);
	int error;

	if (cred->cr_uid != ip->i_uid) {
	    error = suser(cred, &p->p_acflag);
	    if (error)
		return (error);
	}
	if (cred->cr_uid) {
		if (vp->v_type != VDIR && (mode & S_ISTXT))
			return (EFTYPE);
		if (!groupmember(ip->i_gid, cred) && (mode & ISGID))
			return (EPERM);
	}
	ip->i_mode &= ~ALLPERMS;
	ip->i_mode |= (mode & ALLPERMS);
	ip->i_flag |= IN_CHANGE;
	return (0);
}

/*
 * Perform chown operation on inode ip;
 * inode must be locked prior to call.
 */
static int
ufs_chown(vp, uid, gid, cred, p)
	register struct vnode *vp;
	uid_t uid;
	gid_t gid;
	struct ucred *cred;
	struct proc *p;
{
	register struct inode *ip = VTOI(vp);
	uid_t ouid;
	gid_t ogid;
	int error = 0;
#ifdef QUOTA
	register int i;
	long change;
#endif

	if (uid == (uid_t)VNOVAL)
		uid = ip->i_uid;
	if (gid == (gid_t)VNOVAL)
		gid = ip->i_gid;
	/*
	 * If we don't own the file, are trying to change the owner
	 * of the file, or are not a member of the target group,
	 * the caller must be superuser or the call fails.
	 */
	if ((cred->cr_uid != ip->i_uid || uid != ip->i_uid ||
	    (gid != ip->i_gid && !groupmember((gid_t)gid, cred))) &&
	    (error = suser(cred, &p->p_acflag)))
		return (error);
	ogid = ip->i_gid;
	ouid = ip->i_uid;
#ifdef QUOTA
	if (error = getinoquota(ip))
		return (error);
	if (ouid == uid) {
		dqrele(vp, ip->i_dquot[USRQUOTA]);
		ip->i_dquot[USRQUOTA] = NODQUOT;
	}
	if (ogid == gid) {
		dqrele(vp, ip->i_dquot[GRPQUOTA]);
		ip->i_dquot[GRPQUOTA] = NODQUOT;
	}
	change = ip->i_blocks;
	(void) chkdq(ip, -change, cred, CHOWN);
	(void) chkiq(ip, -1, cred, CHOWN);
	for (i = 0; i < MAXQUOTAS; i++) {
		dqrele(vp, ip->i_dquot[i]);
		ip->i_dquot[i] = NODQUOT;
	}
#endif
	ip->i_gid = gid;
	ip->i_uid = uid;
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
	ip->i_uid = ouid;
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
	if (cred->cr_uid != 0 && (ouid != uid || ogid != gid))
		ip->i_mode &= ~(ISUID | ISGID);
	return (0);
}

/*
 * Mmap a file
 *
 * NB Currently unsupported.
 */
/* ARGSUSED */
int
ufs_mmap(ap)
	struct vop_mmap_args /* {
		struct vnode *a_vp;
		int  a_fflags;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{

	return (EINVAL);
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
	VN_POLLEVENT(vp, POLLNLINK);
	VN_POLLEVENT(dvp, POLLWRITE);
out:
	if (dvp == vp)
		vrele(vp);
	else
		vput(vp);
	vput(dvp);
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
	struct proc *p = cnp->cn_proc;
	struct inode *ip;
	struct timeval tv;
	struct direct newdir;
	int error;

#ifdef DIAGNOSTIC
	if ((cnp->cn_flags & HASBUF) == 0)
		panic("ufs_link: no name");
#endif
	if (tdvp->v_mount != vp->v_mount) {
		VOP_ABORTOP(tdvp, cnp);
		error = EXDEV;
		goto out2;
	}
	if (tdvp != vp && (error = vn_lock(vp, LK_EXCLUSIVE, p))) {
		VOP_ABORTOP(tdvp, cnp);
		goto out2;
	}
	ip = VTOI(vp);
	if ((nlink_t)ip->i_nlink >= LINK_MAX) {
		VOP_ABORTOP(tdvp, cnp);
		error = EMLINK;
		goto out1;
	}
	if (ip->i_flags & (IMMUTABLE | APPEND)) {
		VOP_ABORTOP(tdvp, cnp);
		error = EPERM;
		goto out1;
	}
	ip->i_effnlink++;
	ip->i_nlink++;
	ip->i_flag |= IN_CHANGE;
	if (DOINGSOFTDEP(vp))
		softdep_increase_linkcnt(ip);
	gettime(&tv);
	error = UFS_UPDATE(vp, &tv, &tv, !DOINGSOFTDEP(vp));
	if (!error) {
		ufs_makedirentry(ip, cnp, &newdir);
		error = ufs_direnter(tdvp, vp, &newdir, cnp, NULL);
	}

	if (error) {
		ip->i_effnlink--;
		ip->i_nlink--;
		ip->i_flag |= IN_CHANGE;
	}
	zfree(namei_zone, cnp->cn_pnbuf);
out1:
	if (tdvp != vp)
		VOP_UNLOCK(vp, 0, p);
out2:
	VN_POLLEVENT(vp, POLLNLINK);
	VN_POLLEVENT(tdvp, POLLWRITE);
	vput(tdvp);
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
	if (cnp->cn_flags & HASBUF) {
		zfree(namei_zone, cnp->cn_pnbuf);
		cnp->cn_flags &= ~HASBUF;
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
	register struct vnode *tdvp = ap->a_tdvp;
	struct vnode *fvp = ap->a_fvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct componentname *tcnp = ap->a_tcnp;
	struct componentname *fcnp = ap->a_fcnp;
	struct proc *p = fcnp->cn_proc;
	struct inode *ip, *xp, *dp;
	struct direct newdir;
	struct timeval tv;
	int doingdirectory = 0, oldparent = 0, newparent = 0;
	int error = 0;

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
		VOP_ABORTOP(tdvp, tcnp); /* XXX, why not in NFS? */
		if (tdvp == tvp)
			vrele(tdvp);
		else
			vput(tdvp);
		if (tvp)
			vput(tvp);
		VOP_ABORTOP(fdvp, fcnp); /* XXX, why not in NFS? */
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
	 * Check if just deleting a link name or if we've lost a race.
	 * If another process completes the same rename after we've looked
	 * up the source and have blocked looking up the target, then the
	 * source and target inodes may be identical now although the
	 * names were never linked.
	 */
	if (fvp == tvp) {
		if (fvp->v_type == VDIR) {
			/*
			 * Linked directories are impossible, so we must
			 * have lost the race.  Pretend that the rename
			 * completed before the lookup.
			 */
#ifdef UFS_RENAME_DEBUG
			printf("ufs_rename: fvp == tvp for directories\n");
#endif
			error = ENOENT;
			goto abortit;
		}

		/* Release destination completely. */
		VOP_ABORTOP(tdvp, tcnp);
		vput(tdvp);
		vput(tvp);

		/*
		 * Delete source.  There is another race now that everything
		 * is unlocked, but this doesn't cause any new complications.
		 * Relookup() may find a file that is unrelated to the
		 * original one, or it may fail.  Too bad.
		 */
		vrele(fdvp);
		vrele(fvp);
		fcnp->cn_flags &= ~MODMASK;
		fcnp->cn_flags |= LOCKPARENT | LOCKLEAF;
		if ((fcnp->cn_flags & SAVESTART) == 0)
			panic("ufs_rename: lost from startdir");
		fcnp->cn_nameiop = DELETE;
		VREF(fdvp);
		error = relookup(fdvp, &fvp, fcnp);
		if (error == 0)
			vrele(fdvp);
		if (fvp == NULL) {
#ifdef UFS_RENAME_DEBUG
			printf("ufs_rename: from name disappeared\n");
#endif
			return (ENOENT);
		}
		return (VOP_REMOVE(fdvp, fvp, fcnp));
	}
	if (error = vn_lock(fvp, LK_EXCLUSIVE, p))
		goto abortit;
	dp = VTOI(fdvp);
	ip = VTOI(fvp);
	if ((ip->i_flags & (NOUNLINK | IMMUTABLE | APPEND))
	    || (dp->i_flags & APPEND)) {
		VOP_UNLOCK(fvp, 0, p);
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
			VOP_UNLOCK(fvp, 0, p);
			error = EINVAL;
			goto abortit;
		}
		ip->i_flag |= IN_RENAME;
		oldparent = dp->i_number;
		doingdirectory++;
	}
	VN_POLLEVENT(fdvp, POLLWRITE);
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
	ip->i_flag |= IN_CHANGE;
	if (DOINGSOFTDEP(fvp))
		softdep_increase_linkcnt(ip);
	gettime(&tv);
	if (error = UFS_UPDATE(fvp, &tv, &tv, !DOINGSOFTDEP(fvp))) {
		VOP_UNLOCK(fvp, 0, p);
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
	error = VOP_ACCESS(fvp, VWRITE, tcnp->cn_cred, tcnp->cn_proc);
	VOP_UNLOCK(fvp, 0, p);
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
			dp->i_flag |= IN_CHANGE;
			if (DOINGSOFTDEP(tdvp))
				softdep_increase_linkcnt(dp);
			error = UFS_UPDATE(tdvp, &tv, &tv, !DOINGSOFTDEP(tdvp));
			if (error)
				goto bad;
		}
		ufs_makedirentry(ip, tcnp, &newdir);
		error = ufs_direnter(tdvp, NULL, &newdir, tcnp, NULL);
		if (error) {
			if (doingdirectory && newparent) {
				dp->i_effnlink--;
				dp->i_nlink--;
				dp->i_flag |= IN_CHANGE;
				(void)UFS_UPDATE(tdvp, &tv, &tv, 1);
			}
			goto bad;
		}
		VN_POLLEVENT(tdvp, POLLWRITE);
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
		 * If the parent directory is "sticky", then the user must
		 * own the parent directory, or the destination of the rename,
		 * otherwise the destination may not be changed (except by
		 * root). This implements append-only directories.
		 */
		if ((dp->i_mode & S_ISTXT) && tcnp->cn_cred->cr_uid != 0 &&
		    tcnp->cn_cred->cr_uid != dp->i_uid &&
		    xp->i_uid != tcnp->cn_cred->cr_uid) {
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
		    IFTODT(ip->i_mode), doingdirectory);
		if (error)
			goto bad;
		if (doingdirectory) {
			dp->i_effnlink--;
			dp->i_flag |= IN_CHANGE;
			xp->i_effnlink--;
			xp->i_flag |= IN_CHANGE;
		}
		VN_POLLEVENT(tdvp, POLLWRITE);
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
			dp->i_nlink--;
			xp->i_nlink--;
			if ((error = UFS_TRUNCATE(tvp, (off_t)0, IO_SYNC,
			    tcnp->cn_cred, tcnp->cn_proc)) != 0)
				goto bad;
		}
		vput(tdvp);
		VN_POLLEVENT(tvp, POLLNLINK); /* XXX this right? */
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
		 * From name has disappeared.
		 */
		if (doingdirectory)
			panic("ufs_rename: lost dir entry");
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
		if (doingdirectory)
			panic("ufs_rename: lost dir entry");
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
	if (vn_lock(fvp, LK_EXCLUSIVE, p) == 0) {
		ip->i_effnlink--;
		ip->i_nlink--;
		ip->i_flag |= IN_CHANGE;
		ip->i_flag &= ~IN_RENAME;
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
	register struct vnode *dvp = ap->a_dvp;
	register struct vattr *vap = ap->a_vap;
	register struct componentname *cnp = ap->a_cnp;
	register struct inode *ip, *dp;
	struct vnode *tvp;
	struct buf *bp;
	struct dirtemplate dirtemplate, *dtp;
	struct direct newdir;
	struct timeval tv;
	int error, dmode;

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
#ifdef SUIDDIR
	{
#ifdef QUOTA
		struct ucred ucred, *ucp;
		ucp = cnp->cn_cred;
#endif			I
		/*
		 * If we are hacking owners here, (only do this where told to)
		 * and we are not giving it TOO root, (would subvert quotas)
		 * then go ahead and give it to the other user.
		 * The new directory also inherits the SUID bit.
		 * If user's UID and dir UID are the same,
		 * 'give it away' so that the SUID is still forced on.
		 */
		if ((dvp->v_mount->mnt_flag & MNT_SUIDDIR) &&
		    (dp->i_mode & ISUID) && dp->i_uid) {
			dmode |= ISUID;
			ip->i_uid = dp->i_uid;
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
		} else
			ip->i_uid = cnp->cn_cred->cr_uid;
#ifdef QUOTA
		if ((error = getinoquota(ip)) ||
	    	    (error = chkiq(ip, 1, ucp, 0))) {
			zfree(namei_zone, cnp->cn_pnbuf);
			UFS_VFREE(tvp, ip->i_number, dmode);
			vput(tvp);
			vput(dvp);
			return (error);
		}
#endif
	}
#else	/* !SUIDDIR */
	ip->i_uid = cnp->cn_cred->cr_uid;
#ifdef QUOTA
	if ((error = getinoquota(ip)) ||
	    (error = chkiq(ip, 1, cnp->cn_cred, 0))) {
		zfree(namei_zone, cnp->cn_pnbuf);
		UFS_VFREE(tvp, ip->i_number, dmode);
		vput(tvp);
		vput(dvp);
		return (error);
	}
#endif
#endif	/* !SUIDDIR */
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	ip->i_mode = dmode;
	tvp->v_type = VDIR;	/* Rest init'd in getnewvnode(). */
	ip->i_effnlink = 2;
	ip->i_nlink = 2;
	if (DOINGSOFTDEP(tvp))
		softdep_increase_linkcnt(ip);
	if (cnp->cn_flags & ISWHITEOUT)
		ip->i_flags |= UF_OPAQUE;

	/*
	 * Bump link count in parent directory to reflect work done below.
	 * Should be done before reference is created so cleanup is
	 * possible if we crash.
	 */
	dp->i_effnlink++;
	dp->i_nlink++;
	dp->i_flag |= IN_CHANGE;
	if (DOINGSOFTDEP(dvp))
		softdep_increase_linkcnt(dp);
	gettime(&tv);
        error = UFS_UPDATE(tvp, &tv, &tv, !DOINGSOFTDEP(dvp));
	if (error)
		goto bad;

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
	if ((error = VOP_BALLOC(tvp, (off_t)0, DIRBLKSIZ, cnp->cn_cred,
	    B_CLRBUF, &bp)) != 0)
		goto bad;
	ip->i_size = DIRBLKSIZ;
	ip->i_flag |= IN_CHANGE | IN_UPDATE;
	vnode_pager_setsize(tvp, (u_long)ip->i_size);
	bcopy((caddr_t)&dirtemplate, (caddr_t)bp->b_data, sizeof dirtemplate);
	if ((error = UFS_UPDATE(tvp, &tv, &tv, !DOINGSOFTDEP(tvp))) != 0) {
		(void)VOP_BWRITE(bp);
		goto bad;
	}
	VN_POLLEVENT(dvp, POLLWRITE); /* XXX right place? */
	/*
	 * Directory set up, now install it's entry in the parent directory.
	 *
	 * If we are not doing soft dependencies, then we must write out the
	 * buffer containing the new directory body before entering the new 
	 * name in the parent. If we are doing soft dependencies, then the
	 * buffer containing the new directory body will be passed to and
	 * released in the soft dependency code after the code has attached
	 * an appropriate ordering dependency to the buffer which ensures that
	 * the buffer is written before the new name is written in the parent.
	 */
	if (!DOINGSOFTDEP(dvp) && ((error = VOP_BWRITE(bp)) != 0))
		goto bad;
	ufs_makedirentry(ip, cnp, &newdir);
	error = ufs_direnter(dvp, tvp, &newdir, cnp, bp);
	
bad:
	if (error == 0) {
		*ap->a_vpp = tvp;
	} else {
		dp->i_effnlink--;
		dp->i_nlink--;
		dp->i_flag |= IN_CHANGE;
		/*
		 * No need to do an explicit VOP_TRUNCATE here, vrele will
		 * do this for us because we set the link count to 0.
		 */
		ip->i_effnlink = 0;
		ip->i_nlink = 0;
		ip->i_flag |= IN_CHANGE;
		vput(tvp);
	}
out:
	zfree(namei_zone, cnp->cn_pnbuf);
	vput(dvp);
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
	int error;

	ip = VTOI(vp);
	dp = VTOI(dvp);

	/*
	 * Do not remove a directory that is in the process of being renamed.
	 * Verify the directory is empty (and valid). Rmdir ".." will not be
	 * valid since ".." will contain a reference to the current directory
	 * and thus be non-empty.
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
	/*
	 * Delete reference to directory before purging
	 * inode.  If we crash in between, the directory
	 * will be reattached to lost+found,
	 */
	error = ufs_dirremove(dvp, ip, cnp->cn_flags, 1);
	if (error)
		goto out;
	VN_POLLEVENT(dvp, POLLWRITE|POLLNLINK);
	cache_purge(dvp);
	/*
	 * Truncate inode. The only stuff left in the directory is "." and
	 * "..". The "." reference is inconsequential since we are quashing
	 * it. We have removed the "." reference and the reference in the
	 * parent directory, but there may be other hard links. So,
	 * ufs_dirremove will set the UF_IMMUTABLE flag to ensure that no
	 * new entries are made. The soft dependency code will arrange to
	 * do these operations after the parent directory entry has been
	 * deleted on disk, so when running with that code we avoid doing
	 * them now.
	 */
	dp->i_effnlink--;
	dp->i_flag |= IN_CHANGE;
	ip->i_effnlink--;
	ip->i_flag |= IN_CHANGE;
	if (!DOINGSOFTDEP(vp)) {
		dp->i_nlink--;
		ip->i_nlink--;
		error = UFS_TRUNCATE(vp, (off_t)0, IO_SYNC, cnp->cn_cred,
		    cnp->cn_proc);
	}
	cache_purge(vp);
out:
	vput(dvp);
	VN_POLLEVENT(vp, POLLNLINK);
	vput(vp);
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
	register struct vnode *vp, **vpp = ap->a_vpp;
	register struct inode *ip;
	int len, error;

	error = ufs_makeinode(IFLNK | ap->a_vap->va_mode, ap->a_dvp,
	    vpp, ap->a_cnp);
	if (error)
		return (error);
	VN_POLLEVENT(ap->a_dvp, POLLWRITE);
	vp = *vpp;
	len = strlen(ap->a_target);
	if (len < vp->v_mount->mnt_maxsymlinklen) {
		ip = VTOI(vp);
		bcopy(ap->a_target, (char *)ip->i_shortlink, len);
		ip->i_size = len;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	} else
		error = vn_rdwr(UIO_WRITE, vp, ap->a_target, len, (off_t)0,
		    UIO_SYSSPACE, IO_NODELOCKED, ap->a_cnp->cn_cred, (int *)0,
		    (struct proc *)0);
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
	register struct uio *uio = ap->a_uio;
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
	count -= (uio->uio_offset + count) & (DIRBLKSIZ -1);
	if (count <= 0)
		return (EINVAL);
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
	register struct vnode *vp = ap->a_vp;
	register struct inode *ip = VTOI(vp);
	int isize;

	isize = ip->i_size;
	if ((isize < vp->v_mount->mnt_maxsymlinklen) ||
	    (ip->i_din.di_blocks == 0)) {	/* XXX - for old fastlink support */
		uiomove((char *)ip->i_shortlink, isize, ap->a_uio);
		return (0);
	}
	return (VOP_READ(vp, ap->a_uio, 0, ap->a_cred));
}

/*
 * Ufs abort op, called after namei() when a CREATE/DELETE isn't actually
 * done. If a buffer has been saved in anticipation of a CREATE, delete it.
 */
/* ARGSUSED */
int
ufs_abortop(ap)
	struct vop_abortop_args /* {
		struct vnode *a_dvp;
		struct componentname *a_cnp;
	} */ *ap;
{
	if ((ap->a_cnp->cn_flags & (HASBUF | SAVESTART)) == HASBUF)
		zfree(namei_zone, ap->a_cnp->cn_pnbuf);
	return (0);
}

/*
 * Calculate the logical to physical mapping if not done already,
 * then call the device strategy routine.
 */
int
ufs_strategy(ap)
	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap;
{
	register struct buf *bp = ap->a_bp;
	register struct vnode *vp = bp->b_vp;
	register struct inode *ip;
	int error;

	ip = VTOI(vp);
	if (vp->v_type == VBLK || vp->v_type == VCHR)
		panic("ufs_strategy: spec");
	if (bp->b_blkno == bp->b_lblkno) {
		error = VOP_BMAP(vp, bp->b_lblkno, NULL, &bp->b_blkno, NULL, NULL);
		if (error) {
			bp->b_error = error;
			bp->b_flags |= B_ERROR;
			biodone(bp);
			return (error);
		}
		if ((long)bp->b_blkno == -1)
			vfs_bio_clrbuf(bp);
	}
	if ((long)bp->b_blkno == -1) {
		biodone(bp);
		return (0);
	}
	vp = ip->i_devvp;
	bp->b_dev = vp->v_rdev;
	VOCALL (vp->v_op, VOFFSET(vop_strategy), ap);
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
	register struct vnode *vp = ap->a_vp;
	register struct inode *ip = VTOI(vp);

	printf("tag VT_UFS, ino %ld, on dev %d, %d", ip->i_number,
		major(ip->i_dev), minor(ip->i_dev));
	if (vp->v_type == VFIFO)
		fifo_printinfo(vp);
	lockmgr_printinfo(&ip->i_lock);
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

	/*
	 * Set access flag.
	 */
	if (!(ap->a_vp->v_mount->mnt_flag & MNT_NOATIME))
		VTOI(ap->a_vp)->i_flag |= IN_ACCESS;
	return (VOCALL (spec_vnodeop_p, VOFFSET(vop_read), ap));
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

	/*
	 * Set update and change flags.
	 */
	VTOI(ap->a_vp)->i_flag |= IN_CHANGE | IN_UPDATE;
	return (VOCALL (spec_vnodeop_p, VOFFSET(vop_write), ap));
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
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);

	simple_lock(&vp->v_interlock);
	if (ap->a_vp->v_usecount > 1)
		ITIMES(ip, &time, &time);
	simple_unlock(&vp->v_interlock);
	return (VOCALL (spec_vnodeop_p, VOFFSET(vop_close), ap));
}

/*
 * Read wrapper for fifo's
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

	/*
	 * Set access flag.
	 */
	if (!(ap->a_vp->v_mount->mnt_flag & MNT_NOATIME))
		VTOI(ap->a_vp)->i_flag |= IN_ACCESS;
	return (VOCALL (fifo_vnodeop_p, VOFFSET(vop_read), ap));
}

/*
 * Write wrapper for fifo's.
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
	/*
	 * Set update and change flags.
	 */
	VTOI(ap->a_vp)->i_flag |= IN_CHANGE | IN_UPDATE;
	return (VOCALL (fifo_vnodeop_p, VOFFSET(vop_write), ap));
}

/*
 * Close wrapper for fifo's.
 *
 * Update the times on the inode then do device close.
 */
int
ufsfifo_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);

	simple_lock(&vp->v_interlock);
	if (ap->a_vp->v_usecount > 1)
		ITIMES(ip, &time, &time);
	simple_unlock(&vp->v_interlock);
	return (VOCALL (fifo_vnodeop_p, VOFFSET(vop_close), ap));
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
	register struct inode *ip = VTOI(ap->a_vp);

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
	struct vnode *vp, *nvp;

	vp = *vpp;
	ip = VTOI(vp);
	switch(vp->v_type = IFTOVT(ip->i_mode)) {
	case VCHR:
	case VBLK:
		vp->v_op = specops;
		nvp = checkalias(vp, ip->i_rdev, mntp);
		if (nvp) {
			/*
			 * Discard unneeded vnode, but save its inode.
			 * Note that the lock is carried over in the inode
			 * to the replacement vnode.
			 */
			nvp->v_data = vp->v_data;
			vp->v_data = NULL;
			vp->v_op = spec_vnodeop_p;
			vrele(vp);
			vgone(vp);
			/*
			 * Reinitialize aliased inode.
			 */
			vp = nvp;
			ip->i_vnode = vp;
		}
		break;
	case VFIFO:
		vp->v_op = fifoops;
		break;
	default:
		break;

	}
	if (ip->i_number == ROOTINO)
		vp->v_flag |= VROOT;
	/*
	 * Initialize modrev times
	 */
	SETHIGH(ip->i_modrev, mono_time.tv_sec);
	SETLOW(ip->i_modrev, mono_time.tv_usec * 4294);
	*vpp = vp;
	return (0);
}

/*
 * Allocate a new inode.
 */
int
ufs_makeinode(mode, dvp, vpp, cnp)
	int mode;
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
{
	register struct inode *ip, *pdir;
	struct direct newdir;
	struct timeval tv;
	struct vnode *tvp;
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
	if (error) {
		zfree(namei_zone, cnp->cn_pnbuf);
		vput(dvp);
		return (error);
	}
	ip = VTOI(tvp);
	ip->i_gid = pdir->i_gid;
#ifdef SUIDDIR
	{
#ifdef QUOTA
		struct ucred ucred, *ucp;
		ucp = cnp->cn_cred;
#endif			I
		/*
		 * If we are not the owner of the directory,
		 * and we are hacking owners here, (only do this where told to)
		 * and we are not giving it TOO root, (would subvert quotas)
		 * then go ahead and give it to the other user.
		 * Note that this drops off the execute bits for security.
		 */
		if ((dvp->v_mount->mnt_flag & MNT_SUIDDIR) &&
		    (pdir->i_mode & ISUID) &&
		    (pdir->i_uid != cnp->cn_cred->cr_uid) && pdir->i_uid) {
			ip->i_uid = pdir->i_uid;
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
		} else
			ip->i_uid = cnp->cn_cred->cr_uid;

#ifdef QUOTA
		if ((error = getinoquota(ip)) ||
	    	    (error = chkiq(ip, 1, ucp, 0))) {
			zfree(namei_zone, cnp->cn_pnbuf);
			UFS_VFREE(tvp, ip->i_number, mode);
			vput(tvp);
			vput(dvp);
			return (error);
		}
#endif
	}
#else	/* !SUIDDIR */
	ip->i_uid = cnp->cn_cred->cr_uid;
#ifdef QUOTA
	if ((error = getinoquota(ip)) ||
	    (error = chkiq(ip, 1, cnp->cn_cred, 0))) {
		zfree(namei_zone, cnp->cn_pnbuf);
		UFS_VFREE(tvp, ip->i_number, mode);
		vput(tvp);
		vput(dvp);
		return (error);
	}
#endif
#endif	/* !SUIDDIR */
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	ip->i_mode = mode;
	tvp->v_type = IFTOVT(mode);	/* Rest init'd in getnewvnode(). */
	ip->i_effnlink = 1;
	ip->i_nlink = 1;
	if (DOINGSOFTDEP(tvp))
		softdep_increase_linkcnt(ip);
	if ((ip->i_mode & ISGID) && !groupmember(ip->i_gid, cnp->cn_cred) &&
	    suser(cnp->cn_cred, NULL))
		ip->i_mode &= ~ISGID;

	if (cnp->cn_flags & ISWHITEOUT)
		ip->i_flags |= UF_OPAQUE;

	/*
	 * Make sure inode goes to disk before directory entry.
	 */
	gettime(&tv);
	error = UFS_UPDATE(tvp, &tv, &tv, !DOINGSOFTDEP(tvp));
	if (error)
		goto bad;
	ufs_makedirentry(ip, cnp, &newdir);
	error = ufs_direnter(dvp, tvp, &newdir, cnp, NULL);
	if (error)
		goto bad;

	if ((cnp->cn_flags & SAVESTART) == 0)
		zfree(namei_zone, cnp->cn_pnbuf);
	vput(dvp);
	*vpp = tvp;
	return (0);

bad:
	/*
	 * Write error occurred trying to update the inode
	 * or the directory so must deallocate the inode.
	 */
	zfree(namei_zone, cnp->cn_pnbuf);
	vput(dvp);
	ip->i_effnlink = 0;
	ip->i_nlink = 0;
	ip->i_flag |= IN_CHANGE;
	vput(tvp);
	return (error);
}

static int
ufs_missingop(ap)
	struct vop_generic_args *ap;
{

	panic("no vop function for %s in ufs child", ap->a_desc->vdesc_name);
	return (EOPNOTSUPP);
}

/* Global vfs data structures for ufs. */
static vop_t **ufs_vnodeop_p;
static struct vnodeopv_entry_desc ufs_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vop_defaultop },
	{ &vop_fsync_desc,		(vop_t *) ufs_missingop },
	{ &vop_read_desc,		(vop_t *) ufs_missingop },
	{ &vop_reallocblks_desc,	(vop_t *) ufs_missingop },
	{ &vop_write_desc,		(vop_t *) ufs_missingop },
	{ &vop_abortop_desc,		(vop_t *) ufs_abortop },
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
	{ &vop_mmap_desc,		(vop_t *) ufs_mmap },
	{ &vop_open_desc,		(vop_t *) ufs_open },
	{ &vop_pathconf_desc,		(vop_t *) ufs_pathconf },
	{ &vop_poll_desc,		(vop_t *) vop_stdpoll },
	{ &vop_print_desc,		(vop_t *) ufs_print },
	{ &vop_readdir_desc,		(vop_t *) ufs_readdir },
	{ &vop_readlink_desc,		(vop_t *) ufs_readlink },
	{ &vop_reclaim_desc,		(vop_t *) ufs_reclaim },
	{ &vop_remove_desc,		(vop_t *) ufs_remove },
	{ &vop_rename_desc,		(vop_t *) ufs_rename },
	{ &vop_rmdir_desc,		(vop_t *) ufs_rmdir },
	{ &vop_setattr_desc,		(vop_t *) ufs_setattr },
	{ &vop_strategy_desc,		(vop_t *) ufs_strategy },
	{ &vop_symlink_desc,		(vop_t *) ufs_symlink },
	{ &vop_unlock_desc,		(vop_t *) vop_stdunlock },
	{ &vop_whiteout_desc,		(vop_t *) ufs_whiteout },
	{ NULL, NULL }
};
static struct vnodeopv_desc ufs_vnodeop_opv_desc =
	{ &ufs_vnodeop_p, ufs_vnodeop_entries };

static vop_t **ufs_specop_p;
static struct vnodeopv_entry_desc ufs_specop_entries[] = {
	{ &vop_default_desc,		(vop_t *) spec_vnoperate },
	{ &vop_fsync_desc,		(vop_t *) ufs_missingop },
	{ &vop_access_desc,		(vop_t *) ufs_access },
	{ &vop_close_desc,		(vop_t *) ufsspec_close },
	{ &vop_getattr_desc,		(vop_t *) ufs_getattr },
	{ &vop_inactive_desc,		(vop_t *) ufs_inactive },
	{ &vop_islocked_desc,		(vop_t *) vop_stdislocked },
	{ &vop_lock_desc,		(vop_t *) vop_stdlock },
	{ &vop_print_desc,		(vop_t *) ufs_print },
	{ &vop_read_desc,		(vop_t *) ufsspec_read },
	{ &vop_reclaim_desc,		(vop_t *) ufs_reclaim },
	{ &vop_setattr_desc,		(vop_t *) ufs_setattr },
	{ &vop_unlock_desc,		(vop_t *) vop_stdunlock },
	{ &vop_write_desc,		(vop_t *) ufsspec_write },
	{ NULL, NULL }
};
static struct vnodeopv_desc ufs_specop_opv_desc =
	{ &ufs_specop_p, ufs_specop_entries };

static vop_t **ufs_fifoop_p;
static struct vnodeopv_entry_desc ufs_fifoop_entries[] = {
	{ &vop_default_desc,		(vop_t *) fifo_vnoperate },
	{ &vop_fsync_desc,		(vop_t *) ufs_missingop },
	{ &vop_access_desc,		(vop_t *) ufs_access },
	{ &vop_close_desc,		(vop_t *) ufsfifo_close },
	{ &vop_getattr_desc,		(vop_t *) ufs_getattr },
	{ &vop_inactive_desc,		(vop_t *) ufs_inactive },
	{ &vop_islocked_desc,		(vop_t *) vop_stdislocked },
	{ &vop_lock_desc,		(vop_t *) vop_stdlock },
	{ &vop_print_desc,		(vop_t *) ufs_print },
	{ &vop_read_desc,		(vop_t *) ufsfifo_read },
	{ &vop_reclaim_desc,		(vop_t *) ufs_reclaim },
	{ &vop_setattr_desc,		(vop_t *) ufs_setattr },
	{ &vop_unlock_desc,		(vop_t *) vop_stdunlock },
	{ &vop_write_desc,		(vop_t *) ufsfifo_write },
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


