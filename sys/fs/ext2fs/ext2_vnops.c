/*-
 *  modified for EXT2FS support in Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
/*-
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)ufs_vnops.c	8.7 (Berkeley) 2/3/94
 *	@(#)ufs_vnops.c 8.27 (Berkeley) 5/27/95
 * $FreeBSD$
 */

#include "opt_suiddir.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/stat.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/endian.h>
#include <sys/priv.h>
#include <sys/rwlock.h>
#include <sys/mount.h>
#include <sys/unistd.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/lockf.h>
#include <sys/event.h>
#include <sys/conf.h>
#include <sys/file.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>

#include "opt_directio.h"

#include <ufs/ufs/dir.h>

#include <fs/ext2fs/fs.h>
#include <fs/ext2fs/inode.h>
#include <fs/ext2fs/ext2_extern.h>
#include <fs/ext2fs/ext2fs.h>
#include <fs/ext2fs/ext2_dinode.h>
#include <fs/ext2fs/ext2_dir.h>
#include <fs/ext2fs/ext2_mount.h>

static int ext2_makeinode(int mode, struct vnode *, struct vnode **, struct componentname *);
static void ext2_itimes_locked(struct vnode *);
static int ext4_ext_read(struct vop_read_args *);
static int ext2_ind_read(struct vop_read_args *);

static vop_access_t	ext2_access;
static int ext2_chmod(struct vnode *, int, struct ucred *, struct thread *);
static int ext2_chown(struct vnode *, uid_t, gid_t, struct ucred *,
    struct thread *);
static vop_close_t	ext2_close;
static vop_create_t	ext2_create;
static vop_fsync_t	ext2_fsync;
static vop_getpages_t	ext2_getpages;
static vop_getattr_t	ext2_getattr;
static vop_ioctl_t	ext2_ioctl;
static vop_link_t	ext2_link;
static vop_mkdir_t	ext2_mkdir;
static vop_mknod_t	ext2_mknod;
static vop_open_t	ext2_open;
static vop_pathconf_t	ext2_pathconf;
static vop_print_t	ext2_print;
static vop_read_t	ext2_read;
static vop_readlink_t	ext2_readlink;
static vop_remove_t	ext2_remove;
static vop_rename_t	ext2_rename;
static vop_rmdir_t	ext2_rmdir;
static vop_setattr_t	ext2_setattr;
static vop_strategy_t	ext2_strategy;
static vop_symlink_t	ext2_symlink;
static vop_write_t	ext2_write;
static vop_vptofh_t	ext2_vptofh;
static vop_close_t	ext2fifo_close;
static vop_kqfilter_t	ext2fifo_kqfilter;

/* Global vfs data structures for ext2. */
struct vop_vector ext2_vnodeops = {
	.vop_default =		&default_vnodeops,
	.vop_access =		ext2_access,
	.vop_bmap =		ext2_bmap,
	.vop_cachedlookup =	ext2_lookup,
	.vop_close =		ext2_close,
	.vop_create =		ext2_create,
	.vop_fsync =		ext2_fsync,
	.vop_getpages =		ext2_getpages,
	.vop_getattr =		ext2_getattr,
	.vop_inactive =		ext2_inactive,
	.vop_ioctl =		ext2_ioctl,
	.vop_link =		ext2_link,
	.vop_lookup =		vfs_cache_lookup,
	.vop_mkdir =		ext2_mkdir,
	.vop_mknod =		ext2_mknod,
	.vop_open =		ext2_open,
	.vop_pathconf =		ext2_pathconf,
	.vop_poll =		vop_stdpoll,
	.vop_print =		ext2_print,
	.vop_read =		ext2_read,
	.vop_readdir =		ext2_readdir,
	.vop_readlink =		ext2_readlink,
	.vop_reallocblks =	ext2_reallocblks,
	.vop_reclaim =		ext2_reclaim,
	.vop_remove =		ext2_remove,
	.vop_rename =		ext2_rename,
	.vop_rmdir =		ext2_rmdir,
	.vop_setattr =		ext2_setattr,
	.vop_strategy =		ext2_strategy,
	.vop_symlink =		ext2_symlink,
	.vop_write =		ext2_write,
	.vop_vptofh =		ext2_vptofh,
};

struct vop_vector ext2_fifoops = {
	.vop_default =		&fifo_specops,
	.vop_access =		ext2_access,
	.vop_close =		ext2fifo_close,
	.vop_fsync =		ext2_fsync,
	.vop_getattr =		ext2_getattr,
	.vop_inactive =		ext2_inactive,
	.vop_kqfilter =		ext2fifo_kqfilter,
	.vop_print =		ext2_print,
	.vop_read =		VOP_PANIC,
	.vop_reclaim =		ext2_reclaim,
	.vop_setattr =		ext2_setattr,
	.vop_write =		VOP_PANIC,
	.vop_vptofh =		ext2_vptofh,
};

/*
 * A virgin directory (no blushing please).
 * Note that the type and namlen fields are reversed relative to ext2.
 * Also, we don't use `struct odirtemplate', since it would just cause
 * endianness problems.
 */
static struct dirtemplate mastertemplate = {
	0, 12, 1, EXT2_FT_DIR, ".",
	0, DIRBLKSIZ - 12, 2, EXT2_FT_DIR, ".."
};
static struct dirtemplate omastertemplate = {
	0, 12, 1, EXT2_FT_UNKNOWN, ".",
	0, DIRBLKSIZ - 12, 2, EXT2_FT_UNKNOWN, ".."
};

static void
ext2_itimes_locked(struct vnode *vp)
{
	struct inode *ip;
	struct timespec ts;

	ASSERT_VI_LOCKED(vp, __func__);	

	ip = VTOI(vp);
	if ((ip->i_flag & (IN_ACCESS | IN_CHANGE | IN_UPDATE)) == 0)
		return;
	if ((vp->v_type == VBLK || vp->v_type == VCHR))
		ip->i_flag |= IN_LAZYMOD;
	else
		ip->i_flag |= IN_MODIFIED;
	if ((vp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
		vfs_timestamp(&ts);
		if (ip->i_flag & IN_ACCESS) {
			ip->i_atime = ts.tv_sec;
			ip->i_atimensec = ts.tv_nsec;
		}
		if (ip->i_flag & IN_UPDATE) {
			ip->i_mtime = ts.tv_sec;
			ip->i_mtimensec = ts.tv_nsec;
			ip->i_modrev++;
		}
		if (ip->i_flag & IN_CHANGE) {
			ip->i_ctime = ts.tv_sec;
			ip->i_ctimensec = ts.tv_nsec;
		}
	}
	ip->i_flag &= ~(IN_ACCESS | IN_CHANGE | IN_UPDATE);
}

void
ext2_itimes(struct vnode *vp)
{

	VI_LOCK(vp);
	ext2_itimes_locked(vp);
	VI_UNLOCK(vp);
}

/*
 * Create a regular file
 */
static int
ext2_create(struct vop_create_args *ap)
{
	int error;

	error =
	    ext2_makeinode(MAKEIMODE(ap->a_vap->va_type, ap->a_vap->va_mode),
	    ap->a_dvp, ap->a_vpp, ap->a_cnp);
	if (error != 0)
		return (error);
	if ((ap->a_cnp->cn_flags & MAKEENTRY) != 0)
		cache_enter(ap->a_dvp, *ap->a_vpp, ap->a_cnp);
	return (0);
}

static int
ext2_open(struct vop_open_args *ap)
{

	if (ap->a_vp->v_type == VBLK || ap->a_vp->v_type == VCHR)
		return (EOPNOTSUPP);

	/*
	 * Files marked append-only must be opened for appending.
	 */
	if ((VTOI(ap->a_vp)->i_flags & APPEND) &&
	    (ap->a_mode & (FWRITE | O_APPEND)) == FWRITE)
		return (EPERM);

	vnode_create_vobject(ap->a_vp, VTOI(ap->a_vp)->i_size, ap->a_td);

	return (0);
}

/*
 * Close called.
 *
 * Update the times on the inode.
 */
static int
ext2_close(struct vop_close_args *ap)
{
	struct vnode *vp = ap->a_vp;

	VI_LOCK(vp);
	if (vp->v_usecount > 1)
		ext2_itimes_locked(vp);
	VI_UNLOCK(vp);
	return (0);
}

static int
ext2_access(struct vop_access_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	accmode_t accmode = ap->a_accmode;
	int error;

	if (vp->v_type == VBLK || vp->v_type == VCHR)
		return (EOPNOTSUPP);

	/*
	 * Disallow write attempts on read-only file systems;
	 * unless the file is a socket, fifo, or a block or
	 * character device resident on the file system.
	 */
	if (accmode & VWRITE) {
		switch (vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
			break;
		default:
			break;
		}
	}

	/* If immutable bit set, nobody gets to write it. */
	if ((accmode & VWRITE) && (ip->i_flags & (SF_IMMUTABLE | SF_SNAPSHOT)))
		return (EPERM);

	error = vaccess(vp->v_type, ip->i_mode, ip->i_uid, ip->i_gid,
	    ap->a_accmode, ap->a_cred, NULL);
	return (error);
}

static int
ext2_getattr(struct vop_getattr_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct vattr *vap = ap->a_vap;

	ext2_itimes(vp);
	/*
	 * Copy from inode table
	 */
	vap->va_fsid = dev2udev(ip->i_devvp->v_rdev);
	vap->va_fileid = ip->i_number;
	vap->va_mode = ip->i_mode & ~IFMT;
	vap->va_nlink = ip->i_nlink;
	vap->va_uid = ip->i_uid;
	vap->va_gid = ip->i_gid;
	vap->va_rdev = ip->i_rdev;
	vap->va_size = ip->i_size;
	vap->va_atime.tv_sec = ip->i_atime;
	vap->va_atime.tv_nsec = E2DI_HAS_XTIME(ip) ? ip->i_atimensec : 0;
	vap->va_mtime.tv_sec = ip->i_mtime;
	vap->va_mtime.tv_nsec = E2DI_HAS_XTIME(ip) ? ip->i_mtimensec : 0;
	vap->va_ctime.tv_sec = ip->i_ctime;
	vap->va_ctime.tv_nsec = E2DI_HAS_XTIME(ip) ? ip->i_ctimensec : 0;
	if E2DI_HAS_XTIME(ip) {
		vap->va_birthtime.tv_sec = ip->i_birthtime;
		vap->va_birthtime.tv_nsec = ip->i_birthnsec;
	}
	vap->va_flags = ip->i_flags;
	vap->va_gen = ip->i_gen;
	vap->va_blocksize = vp->v_mount->mnt_stat.f_iosize;
	vap->va_bytes = dbtob((u_quad_t)ip->i_blocks);
	vap->va_type = IFTOVT(ip->i_mode);
	vap->va_filerev = ip->i_modrev;
	return (0);
}

/*
 * Set attribute vnode op. called from several syscalls
 */
static int
ext2_setattr(struct vop_setattr_args *ap)
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
		/* Disallow flags not supported by ext2fs. */
		if(vap->va_flags & ~(SF_APPEND | SF_IMMUTABLE | UF_NODUMP))
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
		 * Unprivileged processes and privileged processes in
		 * jail() are not permitted to unset system flags, or
		 * modify flags if any system flags are set.
		 * Privileged non-jail processes may not modify system flags
		 * if securelevel > 0 and any existing system flags are set.
		 */
		if (!priv_check_cred(cred, PRIV_VFS_SYSFLAGS, 0)) {
			if (ip->i_flags & (SF_IMMUTABLE | SF_APPEND)) {
				error = securelevel_gt(cred, 0);
				if (error)
					return (error);
			}
		} else {
			if (ip->i_flags & (SF_IMMUTABLE | SF_APPEND) ||
			    ((vap->va_flags ^ ip->i_flags) & SF_SETTABLE))
				return (EPERM);
		}
		ip->i_flags = vap->va_flags;
		ip->i_flag |= IN_CHANGE;
		if (ip->i_flags & (IMMUTABLE | APPEND))
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
		if ((error = ext2_chown(vp, vap->va_uid, vap->va_gid, cred,
		    td)) != 0)
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
		if ((error = ext2_truncate(vp, vap->va_size, 0, cred, td)) != 0)
			return (error);
	}
	if (vap->va_atime.tv_sec != VNOVAL || vap->va_mtime.tv_sec != VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
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
		ext2_itimes(vp);
		if (vap->va_atime.tv_sec != VNOVAL) {
			ip->i_atime = vap->va_atime.tv_sec;
			ip->i_atimensec = vap->va_atime.tv_nsec;
		}
		if (vap->va_mtime.tv_sec != VNOVAL) {
			ip->i_mtime = vap->va_mtime.tv_sec;
			ip->i_mtimensec = vap->va_mtime.tv_nsec;
		}
		ip->i_birthtime = vap->va_birthtime.tv_sec;
		ip->i_birthnsec = vap->va_birthtime.tv_nsec;
		error = ext2_update(vp, 0);
		if (error)
			return (error);
	}
	error = 0;
	if (vap->va_mode != (mode_t)VNOVAL) {
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);
		error = ext2_chmod(vp, (int)vap->va_mode, cred, td);
	}
	return (error);
}

/*
 * Change the mode on a file.
 * Inode must be locked before calling.
 */
static int
ext2_chmod(struct vnode *vp, int mode, struct ucred *cred, struct thread *td)
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
	if (vp->v_type != VDIR && (mode & S_ISTXT)) {
		error = priv_check_cred(cred, PRIV_VFS_STICKYFILE, 0);
		if (error)
			return (EFTYPE);
	}
	if (!groupmember(ip->i_gid, cred) && (mode & ISGID)) {
		error = priv_check_cred(cred, PRIV_VFS_SETGID, 0);
		if (error)
			return (error);
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
ext2_chown(struct vnode *vp, uid_t uid, gid_t gid, struct ucred *cred,
    struct thread *td)
{
	struct inode *ip = VTOI(vp);
	uid_t ouid;
	gid_t ogid;
	int error = 0;

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
	if (uid != ip->i_uid || (gid != ip->i_gid &&
	    !groupmember(gid, cred))) {
		error = priv_check_cred(cred, PRIV_VFS_CHOWN, 0);
		if (error)
			return (error);
	}
	ogid = ip->i_gid;
	ouid = ip->i_uid;
	ip->i_gid = gid;
	ip->i_uid = uid;
	ip->i_flag |= IN_CHANGE;
	if ((ip->i_mode & (ISUID | ISGID)) && (ouid != uid || ogid != gid)) {
		if (priv_check_cred(cred, PRIV_VFS_RETAINSUGID, 0) != 0)
			ip->i_mode &= ~(ISUID | ISGID);
	}
	return (0);
}

/*
 * Synch an open file.
 */
/* ARGSUSED */
static int
ext2_fsync(struct vop_fsync_args *ap)
{
	/*
	 * Flush all dirty buffers associated with a vnode.
	 */

	vop_stdfsync(ap);

	return (ext2_update(ap->a_vp, ap->a_waitfor == MNT_WAIT));
}

/*
 * Mknod vnode call
 */
/* ARGSUSED */
static int
ext2_mknod(struct vop_mknod_args *ap)
{
	struct vattr *vap = ap->a_vap;
	struct vnode **vpp = ap->a_vpp;
	struct inode *ip;
	ino_t ino;
	int error;

	error = ext2_makeinode(MAKEIMODE(vap->va_type, vap->va_mode),
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
		ip->i_rdev = vap->va_rdev;
	}
	/*
	 * Remove inode, then reload it through VFS_VGET so it is
	 * checked to see if it is an alias of an existing entry in
	 * the inode cache.	 XXX I don't believe this is necessary now.
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

static int
ext2_remove(struct vop_remove_args *ap)
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
	error = ext2_dirremove(dvp, ap->a_cnp);
	if (error == 0) {
		ip->i_nlink--;
		ip->i_flag |= IN_CHANGE;
	}
out:
	return (error);
}

/*
 * link vnode call
 */
static int
ext2_link(struct vop_link_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct vnode *tdvp = ap->a_tdvp;
	struct componentname *cnp = ap->a_cnp;
	struct inode *ip;
	int error;

#ifdef INVARIANTS
	if ((cnp->cn_flags & HASBUF) == 0)
		panic("ext2_link: no name");
#endif
	ip = VTOI(vp);
	if ((nlink_t)ip->i_nlink >= EXT2_LINK_MAX) {
		error = EMLINK;
		goto out;
	}
	if (ip->i_flags & (IMMUTABLE | APPEND)) {
		error = EPERM;
		goto out;
	}
	ip->i_nlink++;
	ip->i_flag |= IN_CHANGE;
	error = ext2_update(vp, !DOINGASYNC(vp));
	if (!error)
		error = ext2_direnter(ip, tdvp, cnp);
	if (error) {
		ip->i_nlink--;
		ip->i_flag |= IN_CHANGE;
	}
out:
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
ext2_rename(struct vop_rename_args *ap)
{
	struct vnode *tvp = ap->a_tvp;
	struct vnode *tdvp = ap->a_tdvp;
	struct vnode *fvp = ap->a_fvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct componentname *tcnp = ap->a_tcnp;
	struct componentname *fcnp = ap->a_fcnp;
	struct inode *ip, *xp, *dp;
	struct dirtemplate dirbuf;
	int doingdirectory = 0, oldparent = 0, newparent = 0;
	int error = 0;
	u_char namlen;

#ifdef INVARIANTS
	if ((tcnp->cn_flags & HASBUF) == 0 ||
	    (fcnp->cn_flags & HASBUF) == 0)
		panic("ext2_rename: no name");
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
		printf("ext2_rename: fvp == tvp (can't happen)\n");
		error = 0;
		goto abortit;
	}

	if ((error = vn_lock(fvp, LK_EXCLUSIVE)) != 0)
		goto abortit;
	dp = VTOI(fdvp);
	ip = VTOI(fvp);
	if (ip->i_nlink >= EXT2_LINK_MAX) {
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
		doingdirectory++;
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
	ip->i_nlink++;
	ip->i_flag |= IN_CHANGE;
	if ((error = ext2_update(fvp, !DOINGASYNC(fvp))) != 0) {
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
		error = ext2_checkpath(ip, dp, tcnp->cn_cred);
		if (error)
			goto out;
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
		if (dp->i_devvp != ip->i_devvp)
			panic("ext2_rename: EXDEV");
		/*
		 * Account for ".." in new directory.
		 * When source and destination have the same
		 * parent we don't fool with the link count.
		 */
		if (doingdirectory && newparent) {
			if ((nlink_t)dp->i_nlink >= EXT2_LINK_MAX) {
				error = EMLINK;
				goto bad;
			}
			dp->i_nlink++;
			dp->i_flag |= IN_CHANGE;
			error = ext2_update(tdvp, !DOINGASYNC(tdvp));
			if (error)
				goto bad;
		}
		error = ext2_direnter(ip, tdvp, tcnp);
		if (error) {
			if (doingdirectory && newparent) {
				dp->i_nlink--;
				dp->i_flag |= IN_CHANGE;
				(void)ext2_update(tdvp, 1);
			}
			goto bad;
		}
		vput(tdvp);
	} else {
		if (xp->i_devvp != dp->i_devvp || xp->i_devvp != ip->i_devvp)
		       panic("ext2_rename: EXDEV");
		/*
		 * Short circuit rename(foo, foo).
		 */
		if (xp->i_number == ip->i_number)
			panic("ext2_rename: same file");
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
			if (! ext2_dirempty(xp, dp->i_number, tcnp->cn_cred) || 
			    xp->i_nlink > 2) {
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
		error = ext2_dirrewrite(dp, ip, tcnp);
		if (error)
			goto bad;
		/*
		 * If the target directory is in the same
		 * directory as the source directory,
		 * decrement the link count on the parent
		 * of the target directory.
		 */
		if (doingdirectory && !newparent) {
			dp->i_nlink--;
			dp->i_flag |= IN_CHANGE;
		}
		vput(tdvp);
		/*
		 * Adjust the link count of the target to
		 * reflect the dirrewrite above.  If this is
		 * a directory it is empty and there are
		 * no links to it, so we can squash the inode and
		 * any space associated with it.  We disallowed
		 * renaming over top of a directory with links to
		 * it above, as the remaining link would point to
		 * a directory without "." or ".." entries.
		 */
		xp->i_nlink--;
		if (doingdirectory) {
			if (--xp->i_nlink != 0)
				panic("ext2_rename: linked directory");
			error = ext2_truncate(tvp, (off_t)0, IO_SYNC,
			    tcnp->cn_cred, tcnp->cn_thread);
		}
		xp->i_flag |= IN_CHANGE;
		vput(tvp);
		xp = NULL;
	}

	/*
	 * 3) Unlink the source.
	 */
	fcnp->cn_flags &= ~MODMASK;
	fcnp->cn_flags |= LOCKPARENT | LOCKLEAF;
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
			panic("ext2_rename: lost dir entry");
		vrele(ap->a_fvp);
		return (0);
	}
	/*
	 * Ensure that the directory entry still exists and has not
	 * changed while the new name has been entered. If the source is
	 * a file then the entry may have been unlinked or renamed. In
	 * either case there is no further work to be done. If the source
	 * is a directory then it cannot have been rmdir'ed; its link
	 * count of three would cause a rmdir to fail with ENOTEMPTY.
	 * The IN_RENAME flag ensures that it cannot be moved by another
	 * rename.
	 */
	if (xp != ip) {
		if (doingdirectory)
			panic("ext2_rename: lost dir entry");
	} else {
		/*
		 * If the source is a directory with a
		 * new parent, the link count of the old
		 * parent directory must be decremented
		 * and ".." set to point to the new parent.
		 */
		if (doingdirectory && newparent) {
			dp->i_nlink--;
			dp->i_flag |= IN_CHANGE;
			error = vn_rdwr(UIO_READ, fvp, (caddr_t)&dirbuf,
				sizeof(struct dirtemplate), (off_t)0,
				UIO_SYSSPACE, IO_NODELOCKED | IO_NOMACCHECK,
				tcnp->cn_cred, NOCRED, NULL, NULL);
			if (error == 0) {
				/* Like ufs little-endian: */
				namlen = dirbuf.dotdot_type;
				if (namlen != 2 ||
				    dirbuf.dotdot_name[0] != '.' ||
				    dirbuf.dotdot_name[1] != '.') {
					ext2_dirbad(xp, (doff_t)12,
					    "rename: mangled dir");
				} else {
					dirbuf.dotdot_ino = newparent;
					(void) vn_rdwr(UIO_WRITE, fvp,
					    (caddr_t)&dirbuf,
					    sizeof(struct dirtemplate),
					    (off_t)0, UIO_SYSSPACE,
					    IO_NODELOCKED | IO_SYNC |
					    IO_NOMACCHECK, tcnp->cn_cred,
					    NOCRED, NULL, NULL);
					cache_purge(fdvp);
				}
			}
		}
		error = ext2_dirremove(fdvp, fcnp);
		if (!error) {
			xp->i_nlink--;
			xp->i_flag |= IN_CHANGE;
		}
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
static int
ext2_mkdir(struct vop_mkdir_args *ap)
{
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	struct inode *ip, *dp;
	struct vnode *tvp;
	struct dirtemplate dirtemplate, *dtp;
	int error, dmode;

#ifdef INVARIANTS
	if ((cnp->cn_flags & HASBUF) == 0)
		panic("ext2_mkdir: no name");
#endif
	dp = VTOI(dvp);
	if ((nlink_t)dp->i_nlink >= EXT2_LINK_MAX) {
		error = EMLINK;
		goto out;
	}
	dmode = vap->va_mode & 0777;
	dmode |= IFDIR;
	/*
	 * Must simulate part of ext2_makeinode here to acquire the inode,
	 * but not have it entered in the parent directory. The entry is
	 * made later after writing "." and ".." entries.
	 */
	error = ext2_valloc(dvp, dmode, cnp->cn_cred, &tvp);
	if (error)
		goto out;
	ip = VTOI(tvp);
	ip->i_gid = dp->i_gid;
#ifdef SUIDDIR
	{
		/*
		 * if we are hacking owners here, (only do this where told to)
		 * and we are not giving it TOO root, (would subvert quotas)
		 * then go ahead and give it to the other user.
		 * The new directory also inherits the SUID bit. 
		 * If user's UID and dir UID are the same,
		 * 'give it away' so that the SUID is still forced on.
		 */
		if ( (dvp->v_mount->mnt_flag & MNT_SUIDDIR) &&
		   (dp->i_mode & ISUID) && dp->i_uid) {
			dmode |= ISUID;
			ip->i_uid = dp->i_uid;
		} else {
			ip->i_uid = cnp->cn_cred->cr_uid;
		}
	}
#else
	ip->i_uid = cnp->cn_cred->cr_uid;
#endif
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	ip->i_mode = dmode;
	tvp->v_type = VDIR;	/* Rest init'd in getnewvnode(). */
	ip->i_nlink = 2;
	if (cnp->cn_flags & ISWHITEOUT)
		ip->i_flags |= UF_OPAQUE;
	error = ext2_update(tvp, 1);

	/*
	 * Bump link count in parent directory
	 * to reflect work done below.  Should
	 * be done before reference is created
	 * so reparation is possible if we crash.
	 */
	dp->i_nlink++;
	dp->i_flag |= IN_CHANGE;
	error = ext2_update(dvp, !DOINGASYNC(dvp));
	if (error)
		goto bad;

	/* Initialize directory with "." and ".." from static template. */
	if (EXT2_HAS_INCOMPAT_FEATURE(ip->i_e2fs,
	    EXT2F_INCOMPAT_FTYPE))
		dtp = &mastertemplate;
	else
		dtp = &omastertemplate;
	dirtemplate = *dtp;
	dirtemplate.dot_ino = ip->i_number;
	dirtemplate.dotdot_ino = dp->i_number;
	/* note that in ext2 DIRBLKSIZ == blocksize, not DEV_BSIZE 
	 * so let's just redefine it - for this function only
	 */
#undef  DIRBLKSIZ 
#define DIRBLKSIZ  VTOI(dvp)->i_e2fs->e2fs_bsize
	dirtemplate.dotdot_reclen = DIRBLKSIZ - 12;
	error = vn_rdwr(UIO_WRITE, tvp, (caddr_t)&dirtemplate,
	    sizeof(dirtemplate), (off_t)0, UIO_SYSSPACE,
	    IO_NODELOCKED | IO_SYNC | IO_NOMACCHECK, cnp->cn_cred, NOCRED,
	    NULL, NULL);
	if (error) {
		dp->i_nlink--;
		dp->i_flag |= IN_CHANGE;
		goto bad;
	}
	if (DIRBLKSIZ > VFSTOEXT2(dvp->v_mount)->um_mountp->mnt_stat.f_bsize)
		/* XXX should grow with balloc() */
		panic("ext2_mkdir: blksize");
	else {
		ip->i_size = DIRBLKSIZ;
		ip->i_flag |= IN_CHANGE;
	}

	/* Directory set up, now install its entry in the parent directory. */
	error = ext2_direnter(ip, dvp, cnp);
	if (error) {
		dp->i_nlink--;
		dp->i_flag |= IN_CHANGE;
	}
bad:
	/*
	 * No need to do an explicit VOP_TRUNCATE here, vrele will do this
	 * for us because we set the link count to 0.
	 */
	if (error) {
		ip->i_nlink = 0;
		ip->i_flag |= IN_CHANGE;
		vput(tvp);
	} else
		*ap->a_vpp = tvp;
out:
	return (error);
#undef  DIRBLKSIZ
#define DIRBLKSIZ  DEV_BSIZE
}

/*
 * Rmdir system call.
 */
static int
ext2_rmdir(struct vop_rmdir_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct inode *ip, *dp;
	int error;

	ip = VTOI(vp);
	dp = VTOI(dvp);

	/*
	 * Verify the directory is empty (and valid).
	 * (Rmdir ".." won't be valid since
	 *  ".." will contain a reference to
	 *  the current directory and thus be
	 *  non-empty.)
	 */
	if (ip->i_nlink != 2 || !ext2_dirempty(ip, dp->i_number, cnp->cn_cred)) {
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
	error = ext2_dirremove(dvp, cnp);
	if (error)
		goto out;
	dp->i_nlink--;
	dp->i_flag |= IN_CHANGE;
	cache_purge(dvp);
	VOP_UNLOCK(dvp, 0);
	/*
	 * Truncate inode.  The only stuff left
	 * in the directory is "." and "..".  The
	 * "." reference is inconsequential since
	 * we're quashing it.  The ".." reference
	 * has already been adjusted above.  We've
	 * removed the "." reference and the reference
	 * in the parent directory, but there may be
	 * other hard links so decrement by 2 and
	 * worry about them later.
	 */
	ip->i_nlink -= 2;
	error = ext2_truncate(vp, (off_t)0, IO_SYNC, cnp->cn_cred,
	    cnp->cn_thread);
	cache_purge(ITOV(ip));
	if (vn_lock(dvp, LK_EXCLUSIVE | LK_NOWAIT) != 0) {
		VOP_UNLOCK(vp, 0);
		vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	}
out:
	return (error);
}

/*
 * symlink -- make a symbolic link
 */
static int
ext2_symlink(struct vop_symlink_args *ap)
{
	struct vnode *vp, **vpp = ap->a_vpp;
	struct inode *ip;
	int len, error;

	error = ext2_makeinode(IFLNK | ap->a_vap->va_mode, ap->a_dvp,
	    vpp, ap->a_cnp);
	if (error)
		return (error);
	vp = *vpp;
	len = strlen(ap->a_target);
	if (len < vp->v_mount->mnt_maxsymlinklen) {
		ip = VTOI(vp);
		bcopy(ap->a_target, (char *)ip->i_shortlink, len);
		ip->i_size = len;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	} else
		error = vn_rdwr(UIO_WRITE, vp, ap->a_target, len, (off_t)0,
		    UIO_SYSSPACE, IO_NODELOCKED | IO_NOMACCHECK,
		    ap->a_cnp->cn_cred, NOCRED, NULL, NULL);
	if (error)
		vput(vp);
	return (error);
}

/*
 * Return target name of a symbolic link
 */
static int
ext2_readlink(struct vop_readlink_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	int isize;

	isize = ip->i_size;
	if (isize < vp->v_mount->mnt_maxsymlinklen) {
		uiomove((char *)ip->i_shortlink, isize, ap->a_uio);
		return (0);
	}
	return (VOP_READ(vp, ap->a_uio, 0, ap->a_cred));
}

/*
 * Calculate the logical to physical mapping if not done already,
 * then call the device strategy routine.
 *
 * In order to be able to swap to a file, the ext2_bmaparray() operation may not
 * deadlock on memory.  See ext2_bmap() for details.
 */
static int
ext2_strategy(struct vop_strategy_args *ap)
{
	struct buf *bp = ap->a_bp;
	struct vnode *vp = ap->a_vp;
	struct bufobj *bo;
	daddr_t blkno;
	int error;

	if (vp->v_type == VBLK || vp->v_type == VCHR)
		panic("ext2_strategy: spec");
	if (bp->b_blkno == bp->b_lblkno) {
		error = ext2_bmaparray(vp, bp->b_lblkno, &blkno, NULL, NULL);
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
	bo = VFSTOEXT2(vp->v_mount)->um_bo;
	BO_STRATEGY(bo, bp);
	return (0);
}

/*
 * Print out the contents of an inode.
 */
static int
ext2_print(struct vop_print_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);

	vn_printf(ip->i_devvp, "\tino %lu", (u_long)ip->i_number);
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
ext2fifo_close(struct vop_close_args *ap)
{
	struct vnode *vp = ap->a_vp;

	VI_LOCK(vp);
	if (vp->v_usecount > 1)
		ext2_itimes_locked(vp);
	VI_UNLOCK(vp);
	return (fifo_specops.vop_close(ap));
}

/*
 * Kqfilter wrapper for fifos.
 *
 * Fall through to ext2 kqfilter routines if needed 
 */
static int
ext2fifo_kqfilter(struct vop_kqfilter_args *ap)
{
	int error;

	error = fifo_specops.vop_kqfilter(ap);
	if (error)
		error = vfs_kqfilter(ap);
	return (error);
}

/*
 * Return POSIX pathconf information applicable to ext2 filesystems.
 */
static int
ext2_pathconf(struct vop_pathconf_args *ap)
{
	int error = 0;

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = EXT2_LINK_MAX;
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
	case _PC_MIN_HOLE_SIZE:
		*ap->a_retval = ap->a_vp->v_mount->mnt_stat.f_iosize;
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
 * Vnode pointer to File handle
 */
/* ARGSUSED */
static int
ext2_vptofh(struct vop_vptofh_args *ap)
{
	struct inode *ip;
	struct ufid *ufhp;

	ip = VTOI(ap->a_vp);
	ufhp = (struct ufid *)ap->a_fhp;
	ufhp->ufid_len = sizeof(struct ufid);
	ufhp->ufid_ino = ip->i_number;
	ufhp->ufid_gen = ip->i_gen;
	return (0);
}

/*
 * Initialize the vnode associated with a new inode, handle aliased
 * vnodes.
 */
int
ext2_vinit(struct mount *mntp, struct vop_vector *fifoops, struct vnode **vpp)
{
	struct inode *ip;
	struct vnode *vp;

	vp = *vpp;
	ip = VTOI(vp);
	vp->v_type = IFTOVT(ip->i_mode);
	if (vp->v_type == VFIFO)
		vp->v_op = fifoops;

	if (ip->i_number == EXT2_ROOTINO)
		vp->v_vflag |= VV_ROOT;
	ip->i_modrev = init_va_filerev();
	*vpp = vp;
	return (0);
}

/*
 * Allocate a new inode.
 */
static int
ext2_makeinode(int mode, struct vnode *dvp, struct vnode **vpp,
    struct componentname *cnp)
{
	struct inode *ip, *pdir;
	struct vnode *tvp;
	int error;

	pdir = VTOI(dvp);
#ifdef INVARIANTS
	if ((cnp->cn_flags & HASBUF) == 0)
		panic("ext2_makeinode: no name");
#endif
	*vpp = NULL;
	if ((mode & IFMT) == 0)
		mode |= IFREG;

	error = ext2_valloc(dvp, mode, cnp->cn_cred, &tvp);
	if (error) {
		return (error);
	}
	ip = VTOI(tvp);
	ip->i_gid = pdir->i_gid;
#ifdef SUIDDIR
	{
		/*
		 * if we are
		 * not the owner of the directory,
		 * and we are hacking owners here, (only do this where told to)
		 * and we are not giving it TOO root, (would subvert quotas)
		 * then go ahead and give it to the other user.
		 * Note that this drops off the execute bits for security.
		 */
		if ( (dvp->v_mount->mnt_flag & MNT_SUIDDIR) &&
		     (pdir->i_mode & ISUID) &&
		     (pdir->i_uid != cnp->cn_cred->cr_uid) && pdir->i_uid) {
			ip->i_uid = pdir->i_uid;
			mode &= ~07111;
		} else {
			ip->i_uid = cnp->cn_cred->cr_uid;
		}
	}
#else
	ip->i_uid = cnp->cn_cred->cr_uid;
#endif
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	ip->i_mode = mode;
	tvp->v_type = IFTOVT(mode);	/* Rest init'd in getnewvnode(). */
	ip->i_nlink = 1;
	if ((ip->i_mode & ISGID) && !groupmember(ip->i_gid, cnp->cn_cred)) {
		if (priv_check_cred(cnp->cn_cred, PRIV_VFS_RETAINSUGID, 0))
			ip->i_mode &= ~ISGID;
	}

	if (cnp->cn_flags & ISWHITEOUT)
		ip->i_flags |= UF_OPAQUE;

	/*
	 * Make sure inode goes to disk before directory entry.
	 */
	error = ext2_update(tvp, !DOINGASYNC(tvp));
	if (error)
		goto bad;
	error = ext2_direnter(ip, dvp, cnp);
	if (error)
		goto bad;

	*vpp = tvp;
	return (0);

bad:
	/*
	 * Write error occurred trying to update the inode
	 * or the directory so must deallocate the inode.
	 */
	ip->i_nlink = 0;
	ip->i_flag |= IN_CHANGE;
	vput(tvp);
	return (error);
}

/*
 * Vnode op for reading.
 */
static int
ext2_read(struct vop_read_args *ap)
{
	struct vnode *vp;
	struct inode *ip;
	int error;

	vp = ap->a_vp;
	ip = VTOI(vp);

	/*EXT4_EXT_LOCK(ip);*/
	if (ip->i_flag & IN_E4EXTENTS)
		error = ext4_ext_read(ap);
	else
		error = ext2_ind_read(ap);
	/*EXT4_EXT_UNLOCK(ip);*/
	return (error);
}

/*
 * Vnode op for reading.
 */
static int
ext2_ind_read(struct vop_read_args *ap)
{
	struct vnode *vp;
	struct inode *ip;
	struct uio *uio;
	struct m_ext2fs *fs;
	struct buf *bp;
	daddr_t lbn, nextlbn;
	off_t bytesinfile;
	long size, xfersize, blkoffset;
	int error, orig_resid, seqcount;
	int ioflag;

	vp = ap->a_vp;
	uio = ap->a_uio;
	ioflag = ap->a_ioflag;

	seqcount = ap->a_ioflag >> IO_SEQSHIFT;
	ip = VTOI(vp);

#ifdef INVARIANTS
	if (uio->uio_rw != UIO_READ)
		panic("%s: mode", "ext2_read");

	if (vp->v_type == VLNK) {
		if ((int)ip->i_size < vp->v_mount->mnt_maxsymlinklen)
			panic("%s: short symlink", "ext2_read");
	} else if (vp->v_type != VREG && vp->v_type != VDIR)
		panic("%s: type %d", "ext2_read", vp->v_type);
#endif
	orig_resid = uio->uio_resid;
	KASSERT(orig_resid >= 0, ("ext2_read: uio->uio_resid < 0"));
	if (orig_resid == 0)
		return (0);
	KASSERT(uio->uio_offset >= 0, ("ext2_read: uio->uio_offset < 0"));
	fs = ip->i_e2fs;
	if (uio->uio_offset < ip->i_size &&
	    uio->uio_offset >= fs->e2fs_maxfilesize)
	    	return (EOVERFLOW);

	for (error = 0, bp = NULL; uio->uio_resid > 0; bp = NULL) {
		if ((bytesinfile = ip->i_size - uio->uio_offset) <= 0)
			break;
		lbn = lblkno(fs, uio->uio_offset);
		nextlbn = lbn + 1;
		size = blksize(fs, ip, lbn);
		blkoffset = blkoff(fs, uio->uio_offset);

		xfersize = fs->e2fs_fsize - blkoffset;
		if (uio->uio_resid < xfersize)
			xfersize = uio->uio_resid;
		if (bytesinfile < xfersize)
			xfersize = bytesinfile;

		if (lblktosize(fs, nextlbn) >= ip->i_size)
			error = bread(vp, lbn, size, NOCRED, &bp);
		else if ((vp->v_mount->mnt_flag & MNT_NOCLUSTERR) == 0) {
			error = cluster_read(vp, ip->i_size, lbn, size,
			    NOCRED, blkoffset + uio->uio_resid, seqcount,
			    0, &bp);
		} else if (seqcount > 1) {
			u_int nextsize = blksize(fs, ip, nextlbn);
			error = breadn(vp, lbn,
			    size, &nextlbn, &nextsize, 1, NOCRED, &bp);
		} else
			error = bread(vp, lbn, size, NOCRED, &bp);
		if (error) {
			brelse(bp);
			bp = NULL;
			break;
		}

		/*
		 * If IO_DIRECT then set B_DIRECT for the buffer.  This
		 * will cause us to attempt to release the buffer later on
		 * and will cause the buffer cache to attempt to free the
		 * underlying pages.
		 */
		if (ioflag & IO_DIRECT)
			bp->b_flags |= B_DIRECT;

		/*
		 * We should only get non-zero b_resid when an I/O error
		 * has occurred, which should cause us to break above.
		 * However, if the short read did not cause an error,
		 * then we want to ensure that we do not uiomove bad
		 * or uninitialized data.
		 */
		size -= bp->b_resid;
		if (size < xfersize) {
			if (size == 0)
				break;
			xfersize = size;
		}
		error = uiomove((char *)bp->b_data + blkoffset,
			(int)xfersize, uio);
		if (error)
			break;

		if (ioflag & (IO_VMIO|IO_DIRECT)) {
			/*
			 * If it's VMIO or direct I/O, then we don't
			 * need the buf, mark it available for
			 * freeing. If it's non-direct VMIO, the VM has
			 * the data.
			 */
			bp->b_flags |= B_RELBUF;
			brelse(bp);
		} else {
			/*
			 * Otherwise let whoever
			 * made the request take care of
			 * freeing it. We just queue
			 * it onto another list.
			 */
			bqrelse(bp);
		}
	}

	/* 
	 * This can only happen in the case of an error
	 * because the loop above resets bp to NULL on each iteration
	 * and on normal completion has not set a new value into it.
	 * so it must have come from a 'break' statement
	 */
	if (bp != NULL) {
		if (ioflag & (IO_VMIO|IO_DIRECT)) {
			bp->b_flags |= B_RELBUF;
			brelse(bp);
		} else {
			bqrelse(bp);
		}
	}

	if ((error == 0 || uio->uio_resid != orig_resid) &&
	    (vp->v_mount->mnt_flag & (MNT_NOATIME | MNT_RDONLY)) == 0)
		ip->i_flag |= IN_ACCESS;
	return (error);
}

static int
ext2_ioctl(struct vop_ioctl_args *ap)
{

	switch (ap->a_command) {
	case FIOSEEKDATA:
	case FIOSEEKHOLE:
		return (vn_bmap_seekhole(ap->a_vp, ap->a_command,
		    (off_t *)ap->a_data, ap->a_cred));
	default:
		return (ENOTTY);
	}
}

/*
 * this function handles ext4 extents block mapping
 */
static int
ext4_ext_read(struct vop_read_args *ap)
{
	static unsigned char zeroes[EXT2_MAX_BLOCK_SIZE];
	struct vnode *vp;
	struct inode *ip;
	struct uio *uio;
	struct m_ext2fs *fs;
	struct buf *bp;
	struct ext4_extent nex, *ep;
	struct ext4_extent_path path;
	daddr_t lbn, newblk;
	off_t bytesinfile;
	int cache_type;
	ssize_t orig_resid;
	int error;
	long size, xfersize, blkoffset;

	vp = ap->a_vp;
	ip = VTOI(vp);
	uio = ap->a_uio;
	memset(&path, 0, sizeof(path));

	orig_resid = uio->uio_resid;
	KASSERT(orig_resid >= 0, ("%s: uio->uio_resid < 0", __func__));
	if (orig_resid == 0)
		return (0);
	KASSERT(uio->uio_offset >= 0, ("%s: uio->uio_offset < 0", __func__));
	fs = ip->i_e2fs;
	if (uio->uio_offset < ip->i_size && uio->uio_offset >= fs->e2fs_maxfilesize)
		return (EOVERFLOW);

	while (uio->uio_resid > 0) {
		if ((bytesinfile = ip->i_size - uio->uio_offset) <= 0)
			break;
		lbn = lblkno(fs, uio->uio_offset);
		size = blksize(fs, ip, lbn);
		blkoffset = blkoff(fs, uio->uio_offset);

		xfersize = fs->e2fs_fsize - blkoffset;
		xfersize = MIN(xfersize, uio->uio_resid);
		xfersize = MIN(xfersize, bytesinfile);

		/* get block from ext4 extent cache */
		cache_type = ext4_ext_in_cache(ip, lbn, &nex);
		switch (cache_type) {
		case EXT4_EXT_CACHE_NO:
			ext4_ext_find_extent(fs, ip, lbn, &path);
			if (path.ep_is_sparse)
				ep = &path.ep_sparse_ext;
			else
				ep = path.ep_ext;
			if (ep == NULL)
				return (EIO);

			ext4_ext_put_cache(ip, ep,
			    path.ep_is_sparse ? EXT4_EXT_CACHE_GAP : EXT4_EXT_CACHE_IN);

			newblk = lbn - ep->e_blk + (ep->e_start_lo |
			    (daddr_t)ep->e_start_hi << 32);

			if (path.ep_bp != NULL) {
				brelse(path.ep_bp);
				path.ep_bp = NULL;
			}
			break;

		case EXT4_EXT_CACHE_GAP:
			/* block has not been allocated yet */
			break;

		case EXT4_EXT_CACHE_IN:
			newblk = lbn - nex.e_blk + (nex.e_start_lo |
			    (daddr_t)nex.e_start_hi << 32);
			break;

		default:
			panic("%s: invalid cache type", __func__);
		}

		if (cache_type == EXT4_EXT_CACHE_GAP ||
		    (cache_type == EXT4_EXT_CACHE_NO && path.ep_is_sparse)) {
			if (xfersize > sizeof(zeroes))
				xfersize = sizeof(zeroes);
			error = uiomove(zeroes, xfersize, uio);
			if (error)
				return (error);
		} else {
			error = bread(ip->i_devvp, fsbtodb(fs, newblk), size,
			    NOCRED, &bp);
			if (error) {
				brelse(bp);
				return (error);
			}

			size -= bp->b_resid;
			if (size < xfersize) {
				if (size == 0) {
					bqrelse(bp);
					break;
				}
				xfersize = size;
			}
			error = uiomove(bp->b_data + blkoffset, xfersize, uio);
			bqrelse(bp);
			if (error)
				return (error);
		}
	}

	return (0);
}

/*
 * Vnode op for writing.
 */
static int
ext2_write(struct vop_write_args *ap)
{
	struct vnode *vp;
	struct uio *uio;
	struct inode *ip;
	struct m_ext2fs *fs;
	struct buf *bp;
	daddr_t lbn;
	off_t osize;
	int blkoffset, error, flags, ioflag, resid, size, seqcount, xfersize;

	ioflag = ap->a_ioflag;
	uio = ap->a_uio;
	vp = ap->a_vp;

	seqcount = ioflag >> IO_SEQSHIFT;
	ip = VTOI(vp);

#ifdef INVARIANTS
	if (uio->uio_rw != UIO_WRITE)
		panic("%s: mode", "ext2_write");
#endif

	switch (vp->v_type) {
	case VREG:
		if (ioflag & IO_APPEND)
			uio->uio_offset = ip->i_size;
		if ((ip->i_flags & APPEND) && uio->uio_offset != ip->i_size)
			return (EPERM);
		/* FALLTHROUGH */
	case VLNK:
		break;
	case VDIR:
		/* XXX differs from ffs -- this is called from ext2_mkdir(). */
		if ((ioflag & IO_SYNC) == 0)
		panic("ext2_write: nonsync dir write");
		break;
	default:
		panic("ext2_write: type %p %d (%jd,%jd)", (void *)vp,
		    vp->v_type, (intmax_t)uio->uio_offset,
		    (intmax_t)uio->uio_resid);
	}

	KASSERT(uio->uio_resid >= 0, ("ext2_write: uio->uio_resid < 0"));
	KASSERT(uio->uio_offset >= 0, ("ext2_write: uio->uio_offset < 0"));
	fs = ip->i_e2fs;
	if ((uoff_t)uio->uio_offset + uio->uio_resid > fs->e2fs_maxfilesize)
		return (EFBIG);
	/*
	 * Maybe this should be above the vnode op call, but so long as
	 * file servers have no limits, I don't think it matters.
	 */
	if (vn_rlimit_fsize(vp, uio, uio->uio_td))
		return (EFBIG);

	resid = uio->uio_resid;
	osize = ip->i_size;
	if (seqcount > BA_SEQMAX)
		flags = BA_SEQMAX << BA_SEQSHIFT;
	else
		flags = seqcount << BA_SEQSHIFT;
	if ((ioflag & IO_SYNC) && !DOINGASYNC(vp))
		flags |= IO_SYNC;

	for (error = 0; uio->uio_resid > 0;) {
		lbn = lblkno(fs, uio->uio_offset);
		blkoffset = blkoff(fs, uio->uio_offset);
		xfersize = fs->e2fs_fsize - blkoffset;
		if (uio->uio_resid < xfersize)
			xfersize = uio->uio_resid;
		if (uio->uio_offset + xfersize > ip->i_size)
			vnode_pager_setsize(vp, uio->uio_offset + xfersize);

		/*
		 * We must perform a read-before-write if the transfer size
		 * does not cover the entire buffer.
		 */
		if (fs->e2fs_bsize > xfersize)
			flags |= BA_CLRBUF;
		else
			flags &= ~BA_CLRBUF;
		error = ext2_balloc(ip, lbn, blkoffset + xfersize,
		    ap->a_cred, &bp, flags);
		if (error != 0)
			break;

		if ((ioflag & (IO_SYNC|IO_INVAL)) == (IO_SYNC|IO_INVAL))
			bp->b_flags |= B_NOCACHE;
		if (uio->uio_offset + xfersize > ip->i_size)
			ip->i_size = uio->uio_offset + xfersize;
		size = blksize(fs, ip, lbn) - bp->b_resid;
		if (size < xfersize)
			xfersize = size;

		error =
		    uiomove((char *)bp->b_data + blkoffset, (int)xfersize, uio);
		/*
		 * If the buffer is not already filled and we encounter an
		 * error while trying to fill it, we have to clear out any
		 * garbage data from the pages instantiated for the buffer.
		 * If we do not, a failed uiomove() during a write can leave
		 * the prior contents of the pages exposed to a userland mmap.
		 *
		 * Note that we need only clear buffers with a transfer size
		 * equal to the block size because buffers with a shorter
		 * transfer size were cleared above by the call to ext2_balloc()
		 * with the BA_CLRBUF flag set.
		 *
		 * If the source region for uiomove identically mmaps the
		 * buffer, uiomove() performed the NOP copy, and the buffer
		 * content remains valid because the page fault handler
		 * validated the pages.
		 */
		if (error != 0 && (bp->b_flags & B_CACHE) == 0 &&
		    fs->e2fs_bsize == xfersize)
			vfs_bio_clrbuf(bp);
		if (ioflag & (IO_VMIO|IO_DIRECT)) {
			bp->b_flags |= B_RELBUF;
		}

		/*
		 * If IO_SYNC each buffer is written synchronously.  Otherwise
		 * if we have a severe page deficiency write the buffer
		 * asynchronously.  Otherwise try to cluster, and if that
		 * doesn't do it then either do an async write (if O_DIRECT),
		 * or a delayed write (if not).
		 */
		if (ioflag & IO_SYNC) {
			(void)bwrite(bp);
		} else if (vm_page_count_severe() ||
		    buf_dirty_count_severe() ||
		    (ioflag & IO_ASYNC)) {
			bp->b_flags |= B_CLUSTEROK;
			bawrite(bp);
		} else if (xfersize + blkoffset == fs->e2fs_fsize) {
			if ((vp->v_mount->mnt_flag & MNT_NOCLUSTERW) == 0) {
				bp->b_flags |= B_CLUSTEROK;
				cluster_write(vp, bp, ip->i_size, seqcount, 0);
			} else {
				bawrite(bp);
			}
		} else if (ioflag & IO_DIRECT) {
			bp->b_flags |= B_CLUSTEROK;
			bawrite(bp);
		} else {
			bp->b_flags |= B_CLUSTEROK;
			bdwrite(bp);
		}
		if (error || xfersize == 0)
			break;
	}
	/*
	 * If we successfully wrote any data, and we are not the superuser
	 * we clear the setuid and setgid bits as a precaution against
	 * tampering.
	 */
	if ((ip->i_mode & (ISUID | ISGID)) && resid > uio->uio_resid &&
	    ap->a_cred) {
		if (priv_check_cred(ap->a_cred, PRIV_VFS_RETAINSUGID, 0))
			ip->i_mode &= ~(ISUID | ISGID);
	}
	if (error) {
		if (ioflag & IO_UNIT) {
			(void)ext2_truncate(vp, osize,
			    ioflag & IO_SYNC, ap->a_cred, uio->uio_td);
			uio->uio_offset -= resid - uio->uio_resid;
			uio->uio_resid = resid;
		}
	}
	if (uio->uio_resid != resid) {
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		if (ioflag & IO_SYNC)
			error = ext2_update(vp, 1);
	}
	return (error);
}

/*
 * get page routine
 */
static int
ext2_getpages(struct vop_getpages_args *ap)
{
	int i;
	vm_page_t mreq;
	int pcount;

	pcount = round_page(ap->a_count) / PAGE_SIZE;
	mreq = ap->a_m[ap->a_reqpage];

	/*
	 * if ANY DEV_BSIZE blocks are valid on a large filesystem block,
	 * then the entire page is valid.  Since the page may be mapped,
	 * user programs might reference data beyond the actual end of file
	 * occuring within the page.  We have to zero that data.
	 */
	VM_OBJECT_WLOCK(mreq->object);
	if (mreq->valid) {
		if (mreq->valid != VM_PAGE_BITS_ALL)
			vm_page_zero_invalid(mreq, TRUE);
		for (i = 0; i < pcount; i++) {
			if (i != ap->a_reqpage) {
				vm_page_lock(ap->a_m[i]);
				vm_page_free(ap->a_m[i]);
				vm_page_unlock(ap->a_m[i]);
			}
		}
		VM_OBJECT_WUNLOCK(mreq->object);
		return VM_PAGER_OK;
	}
	VM_OBJECT_WUNLOCK(mreq->object);

	return vnode_pager_generic_getpages(ap->a_vp, ap->a_m,
					    ap->a_count,
					    ap->a_reqpage);
}
