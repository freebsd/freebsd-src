/*
 * Copyright (c) 1989, 1993
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
 *	@(#)vfs_syscalls.c	8.13 (Berkeley) 4/15/94
 * $Id: vfs_syscalls.c,v 1.82 1997/11/13 00:28:50 julian Exp $
 */

/*
 * XXX - The following is required because of some magic done 
 * in getdirentries() below which is only done if the translucent
 * filesystem `UNION' is compiled into the kernel.  This is broken,
 * but I don't have time to study the code deeply enough to understand
 * what's going on and determine an appropriate fix.  -GAW
 */
#include "opt_union.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/namei.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/dirent.h>

#ifdef UNION
#include <miscfs/union/union.h>
#endif

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <sys/sysctl.h>

static int change_dir __P((struct nameidata *ndp, struct proc *p));
static void checkdirs __P((struct vnode *olddp));

static int	usermount = 0;	/* if 1, non-root can mount fs. */

SYSCTL_INT(_vfs, OID_AUTO, usermount, CTLFLAG_RW, &usermount, 0, "");

/*
 * Virtual File System System Calls
 */

/*
 * Mount a file system.
 */
#ifndef _SYS_SYSPROTO_H_
struct mount_args {
	char	*type;
	char	*path;
	int	flags;
	caddr_t	data;
};
#endif
/* ARGSUSED */
int
mount(p, uap)
	struct proc *p;
	register struct mount_args /* {
		syscallarg(char *) type;
		syscallarg(char *) path;
		syscallarg(int) flags;
		syscallarg(caddr_t) data;
	} */ *uap;
{
	struct vnode *vp;
	struct mount *mp;
	struct vfsconf *vfsp;
	int error, flag = 0, flag2 = 0;
	struct vattr va;
	u_long fstypenum;
	struct nameidata nd;
	char fstypename[MFSNAMELEN];

	if (usermount == 0 && (error = suser(p->p_ucred, &p->p_acflag)))
		return (error);

	/*
	 * Get vnode to be covered
	 */
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	if (SCARG(uap, flags) & MNT_UPDATE) {
		if ((vp->v_flag & VROOT) == 0) {
			vput(vp);
			return (EINVAL);
		}
		mp = vp->v_mount;
		flag = mp->mnt_flag;
		flag2 = mp->mnt_kern_flag;
		/*
		 * We only allow the filesystem to be reloaded if it
		 * is currently mounted read-only.
		 */
		if ((SCARG(uap, flags) & MNT_RELOAD) &&
		    ((mp->mnt_flag & MNT_RDONLY) == 0)) {
			vput(vp);
			return (EOPNOTSUPP);	/* Needs translation */
		}
		mp->mnt_flag |=
		    SCARG(uap, flags) & (MNT_RELOAD | MNT_FORCE | MNT_UPDATE);
		/*
		 * Only root, or the user that did the original mount is
		 * permitted to update it.
		 */
		if (mp->mnt_stat.f_owner != p->p_ucred->cr_uid &&
		    (error = suser(p->p_ucred, &p->p_acflag))) {
			vput(vp);
			return (error);
		}
		/*
		 * Do not allow NFS export by non-root users. Silently
		 * enforce MNT_NOSUID and MNT_NODEV for non-root users.
		 */
		if (p->p_ucred->cr_uid != 0) {
			if (SCARG(uap, flags) & MNT_EXPORTED) {
				vput(vp);
				return (EPERM);
			}
			SCARG(uap, flags) |= MNT_NOSUID | MNT_NODEV;
		}
		if (vfs_busy(mp, LK_NOWAIT, 0, p)) {
			vput(vp);
			return (EBUSY);
		}
		VOP_UNLOCK(vp, 0, p);
		goto update;
	}
	/*
	 * If the user is not root, ensure that they own the directory
	 * onto which we are attempting to mount.
	 */
	if ((error = VOP_GETATTR(vp, &va, p->p_ucred, p)) ||
	    (va.va_uid != p->p_ucred->cr_uid &&
	     (error = suser(p->p_ucred, &p->p_acflag)))) {
		vput(vp);
		return (error);
	}
	/*
	 * Do not allow NFS export by non-root users. Silently
	 * enforce MNT_NOSUID and MNT_NODEV for non-root users.
	 */
	if (p->p_ucred->cr_uid != 0) {
		if (SCARG(uap, flags) & MNT_EXPORTED) {
			vput(vp);
			return (EPERM);
		}
		SCARG(uap, flags) |= MNT_NOSUID | MNT_NODEV;
	}
	if (error = vinvalbuf(vp, V_SAVE, p->p_ucred, p, 0, 0))
		return (error);
	if (vp->v_type != VDIR) {
		vput(vp);
		return (ENOTDIR);
	}
#ifdef COMPAT_43
	/*
	 * Historically filesystem types were identified by number. If we
	 * get an integer for the filesystem type instead of a string, we
	 * check to see if it matches one of the historic filesystem types.
	 */
	fstypenum = (u_long)SCARG(uap, type);
	if (fstypenum < maxvfsconf) {
		for (vfsp = vfsconf; vfsp; vfsp = vfsp->vfc_next)
			if (vfsp->vfc_typenum == fstypenum)
				break;
		if (vfsp == NULL) {
			vput(vp);
			return (ENODEV);
		}
		strncpy(fstypename, vfsp->vfc_name, MFSNAMELEN);
	} else
#endif /* COMPAT_43 */
	if (error = copyinstr(SCARG(uap, type), fstypename, MFSNAMELEN, NULL)) {
		vput(vp);
		return (error);
	}
	for (vfsp = vfsconf; vfsp; vfsp = vfsp->vfc_next)
		if (!strcmp(vfsp->vfc_name, fstypename))
			break;
	if (vfsp == NULL) {
		vput(vp);
		return (ENODEV);
	}
	if (vp->v_mountedhere != NULL) {
		vput(vp);
		return (EBUSY);
	}

	/*
	 * Allocate and initialize the filesystem.
	 */
	mp = (struct mount *)malloc((u_long)sizeof(struct mount),
		M_MOUNT, M_WAITOK);
	bzero((char *)mp, (u_long)sizeof(struct mount));
	lockinit(&mp->mnt_lock, PVFS, "vfslock", 0, 0);
	(void)vfs_busy(mp, LK_NOWAIT, 0, p);
	mp->mnt_op = vfsp->vfc_vfsops;
	mp->mnt_vfc = vfsp;
	vfsp->vfc_refcount++;
	mp->mnt_stat.f_type = vfsp->vfc_typenum;
	mp->mnt_flag |= vfsp->vfc_flags & MNT_VISFLAGMASK;
	strncpy(mp->mnt_stat.f_fstypename, vfsp->vfc_name, MFSNAMELEN);
	vp->v_mountedhere = mp;
	mp->mnt_vnodecovered = vp;
	mp->mnt_stat.f_owner = p->p_ucred->cr_uid;
update:
	/*
	 * Set the mount level flags.
	 */
	if (SCARG(uap, flags) & MNT_RDONLY)
		mp->mnt_flag |= MNT_RDONLY;
	else if (mp->mnt_flag & MNT_RDONLY)
		mp->mnt_kern_flag |= MNTK_WANTRDWR;
	mp->mnt_flag &=~ (MNT_NOSUID | MNT_NOEXEC | MNT_NODEV |
	    MNT_SYNCHRONOUS | MNT_UNION | MNT_ASYNC | MNT_NOATIME |
	    MNT_NOCLUSTERR | MNT_NOCLUSTERW | MNT_SUIDDIR);
	mp->mnt_flag |= SCARG(uap, flags) & (MNT_NOSUID | MNT_NOEXEC |
	    MNT_NODEV | MNT_SYNCHRONOUS | MNT_UNION | MNT_ASYNC | MNT_FORCE |
	    MNT_NOATIME | MNT_NOCLUSTERR | MNT_NOCLUSTERW | MNT_SUIDDIR);
	/*
	 * Mount the filesystem.
	 */
	error = VFS_MOUNT(mp, SCARG(uap, path), SCARG(uap, data), &nd, p);
	if (mp->mnt_flag & MNT_UPDATE) {
		vrele(vp);
		if (mp->mnt_kern_flag & MNTK_WANTRDWR)
			mp->mnt_flag &= ~MNT_RDONLY;
		mp->mnt_flag &=~ (MNT_UPDATE | MNT_RELOAD | MNT_FORCE);
		mp->mnt_kern_flag &=~ MNTK_WANTRDWR;
		if (error) {
			mp->mnt_flag = flag;
			mp->mnt_kern_flag = flag2;
		}
		vfs_unbusy(mp, p);
		return (error);
	}
	/*
	 * Put the new filesystem on the mount list after root.
	 */
	cache_purge(vp);
	if (!error) {
		simple_lock(&mountlist_slock);
		CIRCLEQ_INSERT_TAIL(&mountlist, mp, mnt_list);
		simple_unlock(&mountlist_slock);
		checkdirs(vp);
		VOP_UNLOCK(vp, 0, p);
		vfs_unbusy(mp, p);
		if (error = VFS_START(mp, 0, p))
			vrele(vp);
	} else {
		mp->mnt_vnodecovered->v_mountedhere = (struct mount *)0;
		mp->mnt_vfc->vfc_refcount--;
		vfs_unbusy(mp, p);
		free((caddr_t)mp, M_MOUNT);
		vput(vp);
	}
	return (error);
}

/*
 * Scan all active processes to see if any of them have a current
 * or root directory onto which the new filesystem has just been
 * mounted. If so, replace them with the new mount point.
 */
static void
checkdirs(olddp)
	struct vnode *olddp;
{
	struct filedesc *fdp;
	struct vnode *newdp;
	struct proc *p;

	if (olddp->v_usecount == 1)
		return;
	if (VFS_ROOT(olddp->v_mountedhere, &newdp))
		panic("mount: lost mount");
	for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
		fdp = p->p_fd;
		if (fdp->fd_cdir == olddp) {
			vrele(fdp->fd_cdir);
			VREF(newdp);
			fdp->fd_cdir = newdp;
		}
		if (fdp->fd_rdir == olddp) {
			vrele(fdp->fd_rdir);
			VREF(newdp);
			fdp->fd_rdir = newdp;
		}
	}
	if (rootvnode == olddp) {
		vrele(rootvnode);
		VREF(newdp);
		rootvnode = newdp;
	}
	vput(newdp);
}

/*
 * Unmount a file system.
 *
 * Note: unmount takes a path to the vnode mounted on as argument,
 * not special file (as before).
 */
#ifndef _SYS_SYSPROTO_H_
struct unmount_args {
	char	*path;
	int	flags;
};
#endif
/* ARGSUSED */
int
unmount(p, uap)
	struct proc *p;
	register struct unmount_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
	} */ *uap;
{
	register struct vnode *vp;
	struct mount *mp;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	mp = vp->v_mount;

	/*
	 * Only root, or the user that did the original mount is
	 * permitted to unmount this filesystem.
	 */
	if ((mp->mnt_stat.f_owner != p->p_ucred->cr_uid) &&
	    (error = suser(p->p_ucred, &p->p_acflag))) {
		vput(vp);
		return (error);
	}

	/*
	 * Don't allow unmounting the root file system.
	 */
	if (mp->mnt_flag & MNT_ROOTFS) {
		vput(vp);
		return (EINVAL);
	}

	/*
	 * Must be the root of the filesystem
	 */
	if ((vp->v_flag & VROOT) == 0) {
		vput(vp);
		return (EINVAL);
	}
	vput(vp);
	return (dounmount(mp, SCARG(uap, flags), p));
}

/*
 * Do the actual file system unmount.
 */
int
dounmount(mp, flags, p)
	register struct mount *mp;
	int flags;
	struct proc *p;
{
	struct vnode *coveredvp;
	int error;

	simple_lock(&mountlist_slock);
	mp->mnt_kern_flag |= MNTK_UNMOUNT;
	lockmgr(&mp->mnt_lock, LK_DRAIN | LK_INTERLOCK, &mountlist_slock, p);

	if (mp->mnt_flag & MNT_EXPUBLIC)
		vfs_setpublicfs(NULL, NULL, NULL);

	mp->mnt_flag &=~ MNT_ASYNC;
	vfs_msync(mp, MNT_NOWAIT);
	vnode_pager_umount(mp);	/* release cached vnodes */
	cache_purgevfs(mp);	/* remove cache entries for this file sys */
	if (((mp->mnt_flag & MNT_RDONLY) ||
	     (error = VFS_SYNC(mp, MNT_WAIT, p->p_ucred, p)) == 0) ||
	    (flags & MNT_FORCE))
		error = VFS_UNMOUNT(mp, flags, p);
	simple_lock(&mountlist_slock);
	if (error) {
		mp->mnt_kern_flag &= ~MNTK_UNMOUNT;
		lockmgr(&mp->mnt_lock, LK_RELEASE | LK_INTERLOCK | LK_REENABLE,
		    &mountlist_slock, p);
		return (error);
	}
	CIRCLEQ_REMOVE(&mountlist, mp, mnt_list);
	if ((coveredvp = mp->mnt_vnodecovered) != NULLVP) {
		coveredvp->v_mountedhere = (struct mount *)0;
		vrele(coveredvp);
	}
	mp->mnt_vfc->vfc_refcount--;
	if (mp->mnt_vnodelist.lh_first != NULL)
		panic("unmount: dangling vnode");
	lockmgr(&mp->mnt_lock, LK_RELEASE | LK_INTERLOCK, &mountlist_slock, p);
	if (mp->mnt_kern_flag & MNTK_MWAIT)
		wakeup((caddr_t)mp);
	free((caddr_t)mp, M_MOUNT);
	return (0);
}

/*
 * Sync each mounted filesystem.
 */
#ifndef _SYS_SYSPROTO_H_
struct sync_args {
        int     dummy;
};
#endif

#ifdef DEBUG
int syncprt = 0;
SYSCTL_INT(_debug, 0, syncprt, CTLFLAG_RW, &syncprt, 0, "");
#endif

/* ARGSUSED */
int
sync(p, uap)
	struct proc *p;
	struct sync_args *uap;
{
	register struct mount *mp, *nmp;
	int asyncflag;

	simple_lock(&mountlist_slock);
	for (mp = mountlist.cqh_first; mp != (void *)&mountlist; mp = nmp) {
		if (vfs_busy(mp, LK_NOWAIT, &mountlist_slock, p)) {
			nmp = mp->mnt_list.cqe_next;
			continue;
		}
		if ((mp->mnt_flag & MNT_RDONLY) == 0) {
			asyncflag = mp->mnt_flag & MNT_ASYNC;
			mp->mnt_flag &= ~MNT_ASYNC;
			vfs_msync(mp, MNT_NOWAIT);
			VFS_SYNC(mp, MNT_NOWAIT, p != NULL ? p->p_ucred : NOCRED, p);
			if (asyncflag)
				mp->mnt_flag |= MNT_ASYNC;
		}
		simple_lock(&mountlist_slock);
		nmp = mp->mnt_list.cqe_next;
		vfs_unbusy(mp, p);
	}
	simple_unlock(&mountlist_slock);
#if 0
/*
 * XXX don't call vfs_bufstats() yet because that routine
 * was not imported in the Lite2 merge.
 */
#ifdef DIAGNOSTIC
	if (syncprt)
		vfs_bufstats();
#endif /* DIAGNOSTIC */
#endif
	return (0);
}

/*
 * Change filesystem quotas.
 */
#ifndef _SYS_SYSPROTO_H_
struct quotactl_args {
	char *path;
	int cmd;
	int uid;
	caddr_t arg;
};
#endif
/* ARGSUSED */
int
quotactl(p, uap)
	struct proc *p;
	register struct quotactl_args /* {
		syscallarg(char *) path;
		syscallarg(int) cmd;
		syscallarg(int) uid;
		syscallarg(caddr_t) arg;
	} */ *uap;
{
	register struct mount *mp;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	mp = nd.ni_vp->v_mount;
	vrele(nd.ni_vp);
	return (VFS_QUOTACTL(mp, SCARG(uap, cmd), SCARG(uap, uid),
	    SCARG(uap, arg), p));
}

/*
 * Get filesystem statistics.
 */
#ifndef _SYS_SYSPROTO_H_
struct statfs_args {
	char *path;
	struct statfs *buf;
};
#endif
/* ARGSUSED */
int
statfs(p, uap)
	struct proc *p;
	register struct statfs_args /* {
		syscallarg(char *) path;
		syscallarg(struct statfs *) buf;
	} */ *uap;
{
	register struct mount *mp;
	register struct statfs *sp;
	int error;
	struct nameidata nd;
	struct statfs sb;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	mp = nd.ni_vp->v_mount;
	sp = &mp->mnt_stat;
	vrele(nd.ni_vp);
	error = VFS_STATFS(mp, sp, p);
	if (error)
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	if (p->p_ucred->cr_uid != 0) {
		bcopy((caddr_t)sp, (caddr_t)&sb, sizeof(sb));
		sb.f_fsid.val[0] = sb.f_fsid.val[1] = 0;
		sp = &sb;
	}
	return (copyout((caddr_t)sp, (caddr_t)SCARG(uap, buf), sizeof(*sp)));
}

/*
 * Get filesystem statistics.
 */
#ifndef _SYS_SYSPROTO_H_
struct fstatfs_args {
	int fd;
	struct statfs *buf;
};
#endif
/* ARGSUSED */
int
fstatfs(p, uap)
	struct proc *p;
	register struct fstatfs_args /* {
		syscallarg(int) fd;
		syscallarg(struct statfs *) buf;
	} */ *uap;
{
	struct file *fp;
	struct mount *mp;
	register struct statfs *sp;
	int error;
	struct statfs sb;

	if (error = getvnode(p->p_fd, SCARG(uap, fd), &fp))
		return (error);
	mp = ((struct vnode *)fp->f_data)->v_mount;
	sp = &mp->mnt_stat;
	error = VFS_STATFS(mp, sp, p);
	if (error)
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	if (p->p_ucred->cr_uid != 0) {
		bcopy((caddr_t)sp, (caddr_t)&sb, sizeof(sb));
		sb.f_fsid.val[0] = sb.f_fsid.val[1] = 0;
		sp = &sb;
	}
	return (copyout((caddr_t)sp, (caddr_t)SCARG(uap, buf), sizeof(*sp)));
}

/*
 * Get statistics on all filesystems.
 */
#ifndef _SYS_SYSPROTO_H_
struct getfsstat_args {
	struct statfs *buf;
	long bufsize;
	int flags;
};
#endif
int
getfsstat(p, uap)
	struct proc *p;
	register struct getfsstat_args /* {
		syscallarg(struct statfs *) buf;
		syscallarg(long) bufsize;
		syscallarg(int) flags;
	} */ *uap;
{
	register struct mount *mp, *nmp;
	register struct statfs *sp;
	caddr_t sfsp;
	long count, maxcount, error;

	maxcount = SCARG(uap, bufsize) / sizeof(struct statfs);
	sfsp = (caddr_t)SCARG(uap, buf);
	count = 0;
	simple_lock(&mountlist_slock);
	for (mp = mountlist.cqh_first; mp != (void *)&mountlist; mp = nmp) {
		if (vfs_busy(mp, LK_NOWAIT, &mountlist_slock, p)) {
			nmp = mp->mnt_list.cqe_next;
			continue;
		}
		if (sfsp && count < maxcount) {
			sp = &mp->mnt_stat;
			/*
			 * If MNT_NOWAIT is specified, do not refresh the
			 * fsstat cache. MNT_WAIT overrides MNT_NOWAIT.
			 */
			if (((SCARG(uap, flags) & MNT_NOWAIT) == 0 ||
			    (SCARG(uap, flags) & MNT_WAIT)) &&
			    (error = VFS_STATFS(mp, sp, p))) {
				simple_lock(&mountlist_slock);
				nmp = mp->mnt_list.cqe_next;
				vfs_unbusy(mp, p);
				continue;
			}
			sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
			error = copyout((caddr_t)sp, sfsp, sizeof(*sp));
			if (error) {
				vfs_unbusy(mp, p);
				return (error);
			}
			sfsp += sizeof(*sp);
		}
		count++;
		simple_lock(&mountlist_slock);
		nmp = mp->mnt_list.cqe_next;
		vfs_unbusy(mp, p);
	}
	simple_unlock(&mountlist_slock);
	if (sfsp && count > maxcount)
		p->p_retval[0] = maxcount;
	else
		p->p_retval[0] = count;
	return (0);
}

/*
 * Change current working directory to a given file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct fchdir_args {
	int	fd;
};
#endif
/* ARGSUSED */
int
fchdir(p, uap)
	struct proc *p;
	struct fchdir_args /* {
		syscallarg(int) fd;
	} */ *uap;
{
	register struct filedesc *fdp = p->p_fd;
	struct vnode *vp, *tdp;
	struct mount *mp;
	struct file *fp;
	int error;

	if (error = getvnode(fdp, SCARG(uap, fd), &fp))
		return (error);
	vp = (struct vnode *)fp->f_data;
	VREF(vp);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (vp->v_type != VDIR)
		error = ENOTDIR;
	else
		error = VOP_ACCESS(vp, VEXEC, p->p_ucred, p);
	while (!error && (mp = vp->v_mountedhere) != NULL) {
		if (vfs_busy(mp, 0, 0, p))
			continue;
		error = VFS_ROOT(mp, &tdp);
		vfs_unbusy(mp, p);
		if (error)
			break;
		vput(vp);
		vp = tdp;
	}
	if (error) {
		vput(vp);
		return (error);
	}
	VOP_UNLOCK(vp, 0, p);
	vrele(fdp->fd_cdir);
	fdp->fd_cdir = vp;
	return (0);
}

/*
 * Change current working directory (``.'').
 */
#ifndef _SYS_SYSPROTO_H_
struct chdir_args {
	char	*path;
};
#endif
/* ARGSUSED */
int
chdir(p, uap)
	struct proc *p;
	struct chdir_args /* {
		syscallarg(char *) path;
	} */ *uap;
{
	register struct filedesc *fdp = p->p_fd;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = change_dir(&nd, p))
		return (error);
	vrele(fdp->fd_cdir);
	fdp->fd_cdir = nd.ni_vp;
	return (0);
}

/*
 * Change notion of root (``/'') directory.
 */
#ifndef _SYS_SYSPROTO_H_
struct chroot_args {
	char	*path;
};
#endif
/* ARGSUSED */
int
chroot(p, uap)
	struct proc *p;
	struct chroot_args /* {
		syscallarg(char *) path;
	} */ *uap;
{
	register struct filedesc *fdp = p->p_fd;
	int error;
	struct nameidata nd;

	error = suser(p->p_ucred, &p->p_acflag);
	if (error)
		return (error);
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = change_dir(&nd, p))
		return (error);
	if (fdp->fd_rdir != NULL)
		vrele(fdp->fd_rdir);
	fdp->fd_rdir = nd.ni_vp;
	return (0);
}

/*
 * Common routine for chroot and chdir.
 */
static int
change_dir(ndp, p)
	register struct nameidata *ndp;
	struct proc *p;
{
	struct vnode *vp;
	int error;

	error = namei(ndp);
	if (error)
		return (error);
	vp = ndp->ni_vp;
	if (vp->v_type != VDIR)
		error = ENOTDIR;
	else
		error = VOP_ACCESS(vp, VEXEC, p->p_ucred, p);
	if (error)
		vput(vp);
	else
		VOP_UNLOCK(vp, 0, p);
	return (error);
}

/*
 * Check permissions, allocate an open file structure,
 * and call the device open routine if any.
 */
#ifndef _SYS_SYSPROTO_H_
struct open_args {
	char	*path;
	int	flags;
	int	mode;
};
#endif
int
open(p, uap)
	struct proc *p;
	register struct open_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
		syscallarg(int) mode;
	} */ *uap;
{
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	register struct vnode *vp;
	int cmode, flags, oflags;
	struct file *nfp;
	int type, indx, error;
	struct flock lf;
	struct nameidata nd;

	oflags = SCARG(uap, flags);
	if ((oflags & O_ACCMODE) == O_ACCMODE)
		return (EINVAL);
	flags = FFLAGS(oflags);
	error = falloc(p, &nfp, &indx);
	if (error)
		return (error);
	fp = nfp;
	cmode = ((SCARG(uap, mode) &~ fdp->fd_cmask) & ALLPERMS) &~ S_ISTXT;
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	p->p_dupfd = -indx - 1;			/* XXX check for fdopen */
	error = vn_open(&nd, flags, cmode);
	if (error) {
		ffree(fp);
		if ((error == ENODEV || error == ENXIO) &&
		    p->p_dupfd >= 0 &&			/* XXX from fdopen */
		    (error =
			dupfdopen(fdp, indx, p->p_dupfd, flags, error)) == 0) {
			p->p_retval[0] = indx;
			return (0);
		}
		if (error == ERESTART)
			error = EINTR;
		fdp->fd_ofiles[indx] = NULL;
		return (error);
	}
	p->p_dupfd = 0;
	vp = nd.ni_vp;

	fp->f_flag = flags & FMASK;
	fp->f_type = (vp->v_type == VFIFO ? DTYPE_FIFO : DTYPE_VNODE);
	fp->f_ops = &vnops;
	fp->f_data = (caddr_t)vp;
	if (flags & (O_EXLOCK | O_SHLOCK)) {
		lf.l_whence = SEEK_SET;
		lf.l_start = 0;
		lf.l_len = 0;
		if (flags & O_EXLOCK)
			lf.l_type = F_WRLCK;
		else
			lf.l_type = F_RDLCK;
		type = F_FLOCK;
		if ((flags & FNONBLOCK) == 0)
			type |= F_WAIT;
		VOP_UNLOCK(vp, 0, p);
		if (error = VOP_ADVLOCK(vp, (caddr_t)fp, F_SETLK, &lf, type)) {
			(void) vn_close(vp, fp->f_flag, fp->f_cred, p);
			ffree(fp);
			fdp->fd_ofiles[indx] = NULL;
			return (error);
		}
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
		fp->f_flag |= FHASLOCK;
	}
	VOP_UNLOCK(vp, 0, p);
	p->p_retval[0] = indx;
	return (0);
}

#ifdef COMPAT_43
/*
 * Create a file.
 */
#ifndef _SYS_SYSPROTO_H_
struct ocreat_args {
	char	*path;
	int	mode;
};
#endif
int
ocreat(p, uap)
	struct proc *p;
	register struct ocreat_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap;
{
	struct open_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
		syscallarg(int) mode;
	} */ nuap;

	SCARG(&nuap, path) = SCARG(uap, path);
	SCARG(&nuap, mode) = SCARG(uap, mode);
	SCARG(&nuap, flags) = O_WRONLY | O_CREAT | O_TRUNC;
	return (open(p, &nuap));
}
#endif /* COMPAT_43 */

/*
 * Create a special file.
 */
#ifndef _SYS_SYSPROTO_H_
struct mknod_args {
	char	*path;
	int	mode;
	int	dev;
};
#endif
/* ARGSUSED */
int
mknod(p, uap)
	struct proc *p;
	register struct mknod_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
		syscallarg(int) dev;
	} */ *uap;
{
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	int whiteout;
	struct nameidata nd;

	error = suser(p->p_ucred, &p->p_acflag);
	if (error)
		return (error);
	NDINIT(&nd, CREATE, LOCKPARENT, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	if (vp != NULL)
		error = EEXIST;
	else {
		VATTR_NULL(&vattr);
		vattr.va_mode = (SCARG(uap, mode) & ALLPERMS) &~ p->p_fd->fd_cmask;
		vattr.va_rdev = SCARG(uap, dev);
		whiteout = 0;

		switch (SCARG(uap, mode) & S_IFMT) {
		case S_IFMT:	/* used by badsect to flag bad sectors */
			vattr.va_type = VBAD;
			break;
		case S_IFCHR:
			vattr.va_type = VCHR;
			break;
		case S_IFBLK:
			vattr.va_type = VBLK;
			break;
		case S_IFWHT:
			whiteout = 1;
			break;
		default:
			error = EINVAL;
			break;
		}
	}
	if (!error) {
		VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
		if (whiteout) {
			error = VOP_WHITEOUT(nd.ni_dvp, &nd.ni_cnd, CREATE);
			if (error)
				VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
			vput(nd.ni_dvp);
		} else {
			error = VOP_MKNOD(nd.ni_dvp, &nd.ni_vp,
						&nd.ni_cnd, &vattr);
		}
	} else {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (vp)
			vrele(vp);
	}
	ASSERT_VOP_UNLOCKED(nd.ni_dvp, "mknod");
	ASSERT_VOP_UNLOCKED(nd.ni_vp, "mknod");
	return (error);
}

/*
 * Create a named pipe.
 */
#ifndef _SYS_SYSPROTO_H_
struct mkfifo_args {
	char	*path;
	int	mode;
};
#endif
/* ARGSUSED */
int
mkfifo(p, uap)
	struct proc *p;
	register struct mkfifo_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap;
{
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, CREATE, LOCKPARENT, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	if (nd.ni_vp != NULL) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(nd.ni_vp);
		return (EEXIST);
	}
	VATTR_NULL(&vattr);
	vattr.va_type = VFIFO;
	vattr.va_mode = (SCARG(uap, mode) & ALLPERMS) &~ p->p_fd->fd_cmask;
	VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
	return (VOP_MKNOD(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr));
}

/*
 * Make a hard file link.
 */
#ifndef _SYS_SYSPROTO_H_
struct link_args {
	char	*path;
	char	*link;
};
#endif
/* ARGSUSED */
int
link(p, uap)
	struct proc *p;
	register struct link_args /* {
		syscallarg(char *) path;
		syscallarg(char *) link;
	} */ *uap;
{
	register struct vnode *vp;
	struct nameidata nd;
	int error;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	if (vp->v_type == VDIR)
		error = EPERM;		/* POSIX */
	else {
		NDINIT(&nd, CREATE, LOCKPARENT, UIO_USERSPACE, SCARG(uap, link), p);
		error = namei(&nd);
		if (!error) {
			if (nd.ni_vp != NULL) {
				VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
				if (nd.ni_dvp == nd.ni_vp)
					vrele(nd.ni_dvp);
				else
					vput(nd.ni_dvp);
				if (nd.ni_vp)
					vrele(nd.ni_vp);
				error = EEXIST;
			} else {
				VOP_LEASE(nd.ni_dvp, p, p->p_ucred,
				    LEASE_WRITE);
				VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
				error = VOP_LINK(nd.ni_dvp, vp, &nd.ni_cnd);
			}
		}
	}
	vrele(vp);
	ASSERT_VOP_UNLOCKED(nd.ni_dvp, "link");
	ASSERT_VOP_UNLOCKED(nd.ni_vp, "link");
	return (error);
}

/*
 * Make a symbolic link.
 */
#ifndef _SYS_SYSPROTO_H_
struct symlink_args {
	char	*path;
	char	*link;
};
#endif
/* ARGSUSED */
int
symlink(p, uap)
	struct proc *p;
	register struct symlink_args /* {
		syscallarg(char *) path;
		syscallarg(char *) link;
	} */ *uap;
{
	struct vattr vattr;
	char *path;
	int error;
	struct nameidata nd;

	path = zalloc(namei_zone);
	if (error = copyinstr(SCARG(uap, path), path, MAXPATHLEN, NULL))
		goto out;
	NDINIT(&nd, CREATE, LOCKPARENT, UIO_USERSPACE, SCARG(uap, link), p);
	if (error = namei(&nd))
		goto out;
	if (nd.ni_vp) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(nd.ni_vp);
		error = EEXIST;
		goto out;
	}
	VATTR_NULL(&vattr);
	vattr.va_mode = ACCESSPERMS &~ p->p_fd->fd_cmask;
	VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
	error = VOP_SYMLINK(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr, path);
	ASSERT_VOP_UNLOCKED(nd.ni_dvp, "symlink");
	ASSERT_VOP_UNLOCKED(nd.ni_vp, "symlink");
out:
	zfree(namei_zone, path);
	return (error);
}

/*
 * Delete a whiteout from the filesystem.
 */
/* ARGSUSED */
int
undelete(p, uap)
	struct proc *p;
	register struct undelete_args /* {
		syscallarg(char *) path;
	} */ *uap;
{
	int error;
	struct nameidata nd;

	NDINIT(&nd, DELETE, LOCKPARENT|DOWHITEOUT, UIO_USERSPACE,
	    SCARG(uap, path), p);
	error = namei(&nd);
	if (error)
		return (error);

	if (nd.ni_vp != NULLVP || !(nd.ni_cnd.cn_flags & ISWHITEOUT)) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (nd.ni_vp)
			vrele(nd.ni_vp);
		return (EEXIST);
	}

	VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
	if (error = VOP_WHITEOUT(nd.ni_dvp, &nd.ni_cnd, DELETE))
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
	vput(nd.ni_dvp);
	ASSERT_VOP_UNLOCKED(nd.ni_dvp, "undelete");
	ASSERT_VOP_UNLOCKED(nd.ni_vp, "undelete");
	return (error);
}

/*
 * Delete a name from the filesystem.
 */
#ifndef _SYS_SYSPROTO_H_
struct unlink_args {
	char	*path;
};
#endif
/* ARGSUSED */
int
unlink(p, uap)
	struct proc *p;
	struct unlink_args /* {
		syscallarg(char *) path;
	} */ *uap;
{
	register struct vnode *vp;
	int error;
	struct nameidata nd;

	NDINIT(&nd, DELETE, LOCKPARENT, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);

	if (vp->v_type == VDIR)
		error = EPERM;		/* POSIX */
	else {
		/*
		 * The root of a mounted filesystem cannot be deleted.
		 *
		 * XXX: can this only be a VDIR case?
		 */
		if (vp->v_flag & VROOT)
			error = EBUSY;
		else
			(void) vnode_pager_uncache(vp, p);
	}

	if (!error) {
		VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
		error = VOP_REMOVE(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
	} else {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (vp != NULLVP)
			vput(vp);
	}
	ASSERT_VOP_UNLOCKED(nd.ni_dvp, "unlink");
	ASSERT_VOP_UNLOCKED(nd.ni_vp, "unlink");
	return (error);
}

/*
 * Reposition read/write file offset.
 */
#ifndef _SYS_SYSPROTO_H_
struct lseek_args {
	int	fd;
	int	pad;
	off_t	offset;
	int	whence;
};
#endif
int
lseek(p, uap)
	struct proc *p;
	register struct lseek_args /* {
		syscallarg(int) fd;
		syscallarg(int) pad;
		syscallarg(off_t) offset;
		syscallarg(int) whence;
	} */ *uap;
{
	struct ucred *cred = p->p_ucred;
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	struct vattr vattr;
	int error;

	if ((u_int)SCARG(uap, fd) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[SCARG(uap, fd)]) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_VNODE)
		return (ESPIPE);
	switch (SCARG(uap, whence)) {
	case L_INCR:
		fp->f_offset += SCARG(uap, offset);
		break;
	case L_XTND:
		error=VOP_GETATTR((struct vnode *)fp->f_data, &vattr, cred, p);
		if (error)
			return (error);
		fp->f_offset = SCARG(uap, offset) + vattr.va_size;
		break;
	case L_SET:
		fp->f_offset = SCARG(uap, offset);
		break;
	default:
		return (EINVAL);
	}
	*(off_t *)(p->p_retval) = fp->f_offset;
	return (0);
}

#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
/*
 * Reposition read/write file offset.
 */
#ifndef _SYS_SYSPROTO_H_
struct olseek_args {
	int	fd;
	long	offset;
	int	whence;
};
#endif
int
olseek(p, uap)
	struct proc *p;
	register struct olseek_args /* {
		syscallarg(int) fd;
		syscallarg(long) offset;
		syscallarg(int) whence;
	} */ *uap;
{
	struct lseek_args /* {
		syscallarg(int) fd;
		syscallarg(int) pad;
		syscallarg(off_t) offset;
		syscallarg(int) whence;
	} */ nuap;
	int error;

	SCARG(&nuap, fd) = SCARG(uap, fd);
	SCARG(&nuap, offset) = SCARG(uap, offset);
	SCARG(&nuap, whence) = SCARG(uap, whence);
	error = lseek(p, &nuap);
	return (error);
}
#endif /* COMPAT_43 */

/*
 * Check access permissions.
 */
#ifndef _SYS_SYSPROTO_H_
struct access_args {
	char	*path;
	int	flags;
};
#endif
int
access(p, uap)
	struct proc *p;
	register struct access_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
	} */ *uap;
{
	register struct ucred *cred = p->p_ucred;
	register struct vnode *vp;
	int error, flags, t_gid, t_uid;
	struct nameidata nd;

	t_uid = cred->cr_uid;
	t_gid = cred->cr_groups[0];
	cred->cr_uid = p->p_cred->p_ruid;
	cred->cr_groups[0] = p->p_cred->p_rgid;
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = namei(&nd))
		goto out1;
	vp = nd.ni_vp;

	/* Flags == 0 means only check for existence. */
	if (SCARG(uap, flags)) {
		flags = 0;
		if (SCARG(uap, flags) & R_OK)
			flags |= VREAD;
		if (SCARG(uap, flags) & W_OK)
			flags |= VWRITE;
		if (SCARG(uap, flags) & X_OK)
			flags |= VEXEC;
		if ((flags & VWRITE) == 0 || (error = vn_writechk(vp)) == 0)
			error = VOP_ACCESS(vp, flags, cred, p);
	}
	vput(vp);
out1:
	cred->cr_uid = t_uid;
	cred->cr_groups[0] = t_gid;
	return (error);
}

#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
/*
 * Get file status; this version follows links.
 */
#ifndef _SYS_SYSPROTO_H_
struct ostat_args {
	char	*path;
	struct ostat *ub;
};
#endif
/* ARGSUSED */
int
ostat(p, uap)
	struct proc *p;
	register struct ostat_args /* {
		syscallarg(char *) path;
		syscallarg(struct ostat *) ub;
	} */ *uap;
{
	struct stat sb;
	struct ostat osb;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	error = vn_stat(nd.ni_vp, &sb, p);
	vput(nd.ni_vp);
	if (error)
		return (error);
	cvtstat(&sb, &osb);
	error = copyout((caddr_t)&osb, (caddr_t)SCARG(uap, ub), sizeof (osb));
	return (error);
}

/*
 * Get file status; this version does not follow links.
 */
#ifndef _SYS_SYSPROTO_H_
struct olstat_args {
	char	*path;
	struct ostat *ub;
};
#endif
/* ARGSUSED */
int
olstat(p, uap)
	struct proc *p;
	register struct olstat_args /* {
		syscallarg(char *) path;
		syscallarg(struct ostat *) ub;
	} */ *uap;
{
	struct vnode *vp;
	struct stat sb;
	struct ostat osb;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	error = vn_stat(vp, &sb, p);
	if (vp->v_type == VLNK)
		sb.st_mode |= S_IFLNK | ACCESSPERMS;	/* 0777 */
	vput(vp);
	if (error)
		return (error);
	cvtstat(&sb, &osb);
	error = copyout((caddr_t)&osb, (caddr_t)SCARG(uap, ub), sizeof (osb));
	return (error);
}

/*
 * Convert from an old to a new stat structure.
 */
void
cvtstat(st, ost)
	struct stat *st;
	struct ostat *ost;
{

	ost->st_dev = st->st_dev;
	ost->st_ino = st->st_ino;
	ost->st_mode = st->st_mode;
	ost->st_nlink = st->st_nlink;
	ost->st_uid = st->st_uid;
	ost->st_gid = st->st_gid;
	ost->st_rdev = st->st_rdev;
	if (st->st_size < (quad_t)1 << 32)
		ost->st_size = st->st_size;
	else
		ost->st_size = -2;
	ost->st_atime = st->st_atime;
	ost->st_mtime = st->st_mtime;
	ost->st_ctime = st->st_ctime;
	ost->st_blksize = st->st_blksize;
	ost->st_blocks = st->st_blocks;
	ost->st_flags = st->st_flags;
	ost->st_gen = st->st_gen;
}
#endif /* COMPAT_43 || COMPAT_SUNOS */

/*
 * Get file status; this version follows links.
 */
#ifndef _SYS_SYSPROTO_H_
struct stat_args {
	char	*path;
	struct stat *ub;
};
#endif
/* ARGSUSED */
int
stat(p, uap)
	struct proc *p;
	register struct stat_args /* {
		syscallarg(char *) path;
		syscallarg(struct stat *) ub;
	} */ *uap;
{
	struct stat sb;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	error = vn_stat(nd.ni_vp, &sb, p);
	vput(nd.ni_vp);
	if (error)
		return (error);
	error = copyout((caddr_t)&sb, (caddr_t)SCARG(uap, ub), sizeof (sb));
	return (error);
}

/*
 * Get file status; this version does not follow links.
 */
#ifndef _SYS_SYSPROTO_H_
struct lstat_args {
	char	*path;
	struct stat *ub;
};
#endif
/* ARGSUSED */
int
lstat(p, uap)
	struct proc *p;
	register struct lstat_args /* {
		syscallarg(char *) path;
		syscallarg(struct stat *) ub;
	} */ *uap;
{
	int error;
	struct vnode *vp;
	struct stat sb;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	error = vn_stat(vp, &sb, p);
	if (vp->v_type == VLNK)
		sb.st_mode |= S_IFLNK | ACCESSPERMS;	/* 0777 */
	vput(vp);
	if (error)
		return (error);
	error = copyout((caddr_t)&sb, (caddr_t)SCARG(uap, ub), sizeof (sb));
	return (error);
}

/*
 * Get configurable pathname variables.
 */
#ifndef _SYS_SYSPROTO_H_
struct pathconf_args {
	char	*path;
	int	name;
};
#endif
/* ARGSUSED */
int
pathconf(p, uap)
	struct proc *p;
	register struct pathconf_args /* {
		syscallarg(char *) path;
		syscallarg(int) name;
	} */ *uap;
{
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	error = VOP_PATHCONF(nd.ni_vp, SCARG(uap, name), p->p_retval);
	vput(nd.ni_vp);
	return (error);
}

/*
 * Return target name of a symbolic link.
 */
#ifndef _SYS_SYSPROTO_H_
struct readlink_args {
	char	*path;
	char	*buf;
	int	count;
};
#endif
/* ARGSUSED */
int
readlink(p, uap)
	struct proc *p;
	register struct readlink_args /* {
		syscallarg(char *) path;
		syscallarg(char *) buf;
		syscallarg(int) count;
	} */ *uap;
{
	register struct vnode *vp;
	struct iovec aiov;
	struct uio auio;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	if (vp->v_type != VLNK)
		error = EINVAL;
	else {
		aiov.iov_base = SCARG(uap, buf);
		aiov.iov_len = SCARG(uap, count);
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = 0;
		auio.uio_rw = UIO_READ;
		auio.uio_segflg = UIO_USERSPACE;
		auio.uio_procp = p;
		auio.uio_resid = SCARG(uap, count);
		error = VOP_READLINK(vp, &auio, p->p_ucred);
	}
	vput(vp);
	p->p_retval[0] = SCARG(uap, count) - auio.uio_resid;
	return (error);
}

/*
 * Change flags of a file given a path name.
 */
#ifndef _SYS_SYSPROTO_H_
struct chflags_args {
	char	*path;
	int	flags;
};
#endif
/* ARGSUSED */
int
chflags(p, uap)
	struct proc *p;
	register struct chflags_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
	} */ *uap;
{
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	VATTR_NULL(&vattr);
	vattr.va_flags = SCARG(uap, flags);
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	vput(vp);
	return (error);
}

/*
 * Change flags of a file given a file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct fchflags_args {
	int	fd;
	int	flags;
};
#endif
/* ARGSUSED */
int
fchflags(p, uap)
	struct proc *p;
	register struct fchflags_args /* {
		syscallarg(int) fd;
		syscallarg(int) flags;
	} */ *uap;
{
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	if (error = getvnode(p->p_fd, SCARG(uap, fd), &fp))
		return (error);
	vp = (struct vnode *)fp->f_data;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	VATTR_NULL(&vattr);
	vattr.va_flags = SCARG(uap, flags);
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	VOP_UNLOCK(vp, 0, p);
	return (error);
}

/*
 * Change mode of a file given path name.
 */
#ifndef _SYS_SYSPROTO_H_
struct chmod_args {
	char	*path;
	int	mode;
};
#endif
/* ARGSUSED */
int
chmod(p, uap)
	struct proc *p;
	register struct chmod_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap;
{
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	VATTR_NULL(&vattr);
	vattr.va_mode = SCARG(uap, mode) & ALLPERMS;
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	vput(vp);
	return (error);
}

/*
 * Change mode of a file given a file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct fchmod_args {
	int	fd;
	int	mode;
};
#endif
/* ARGSUSED */
int
fchmod(p, uap)
	struct proc *p;
	register struct fchmod_args /* {
		syscallarg(int) fd;
		syscallarg(int) mode;
	} */ *uap;
{
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	if (error = getvnode(p->p_fd, SCARG(uap, fd), &fp))
		return (error);
	vp = (struct vnode *)fp->f_data;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	VATTR_NULL(&vattr);
	vattr.va_mode = SCARG(uap, mode) & ALLPERMS;
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	VOP_UNLOCK(vp, 0, p);
	return (error);
}

/*
 * Set ownership given a path name.
 */
#ifndef _SYS_SYSPROTO_H_
struct chown_args {
	char	*path;
	int	uid;
	int	gid;
};
#endif
/* ARGSUSED */
int
chown(p, uap)
	struct proc *p;
	register struct chown_args /* {
		syscallarg(char *) path;
		syscallarg(int) uid;
		syscallarg(int) gid;
	} */ *uap;
{
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	VATTR_NULL(&vattr);
	vattr.va_uid = SCARG(uap, uid);
	vattr.va_gid = SCARG(uap, gid);
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	vput(vp);
	return (error);
}

/*
 * Set ownership given a path name, do not cross symlinks.
 */
#ifndef _SYS_SYSPROTO_H_
struct lchown_args {
	char	*path;
	int	uid;
	int	gid;
};
#endif
/* ARGSUSED */
int
lchown(p, uap)
	struct proc *p;
	register struct lchown_args /* {
		syscallarg(char *) path;
		syscallarg(int) uid;
		syscallarg(int) gid;
	} */ *uap;
{
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	VATTR_NULL(&vattr);
	vattr.va_uid = SCARG(uap, uid);
	vattr.va_gid = SCARG(uap, gid);
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	vput(vp);
	return (error);
}

/*
 * Set ownership given a file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct fchown_args {
	int	fd;
	int	uid;
	int	gid;
};
#endif
/* ARGSUSED */
int
fchown(p, uap)
	struct proc *p;
	register struct fchown_args /* {
		syscallarg(int) fd;
		syscallarg(int) uid;
		syscallarg(int) gid;
	} */ *uap;
{
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	if (error = getvnode(p->p_fd, SCARG(uap, fd), &fp))
		return (error);
	vp = (struct vnode *)fp->f_data;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	VATTR_NULL(&vattr);
	vattr.va_uid = SCARG(uap, uid);
	vattr.va_gid = SCARG(uap, gid);
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	VOP_UNLOCK(vp, 0, p);
	return (error);
}

/*
 * Set the access and modification times of a file.
 */
#ifndef _SYS_SYSPROTO_H_
struct utimes_args {
	char	*path;
	struct	timeval *tptr;
};
#endif
/* ARGSUSED */
int
utimes(p, uap)
	struct proc *p;
	register struct utimes_args /* {
		syscallarg(char *) path;
		syscallarg(struct timeval *) tptr;
	} */ *uap;
{
	register struct vnode *vp;
	struct timeval tv[2];
	struct vattr vattr;
	int error;
	struct nameidata nd;

	VATTR_NULL(&vattr);
	if (SCARG(uap, tptr) == NULL) {
		microtime(&tv[0]);
		tv[1] = tv[0];
		vattr.va_vaflags |= VA_UTIMES_NULL;
	} else if (error = copyin((caddr_t)SCARG(uap, tptr), (caddr_t)tv,
	    sizeof (tv)))
  		return (error);
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	vattr.va_atime.tv_sec = tv[0].tv_sec;
	vattr.va_atime.tv_nsec = tv[0].tv_usec * 1000;
	vattr.va_mtime.tv_sec = tv[1].tv_sec;
	vattr.va_mtime.tv_nsec = tv[1].tv_usec * 1000;
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	vput(vp);
	return (error);
}

/*
 * Truncate a file given its path name.
 */
#ifndef _SYS_SYSPROTO_H_
struct truncate_args {
	char	*path;
	int	pad;
	off_t	length;
};
#endif
/* ARGSUSED */
int
truncate(p, uap)
	struct proc *p;
	register struct truncate_args /* {
		syscallarg(char *) path;
		syscallarg(int) pad;
		syscallarg(off_t) length;
	} */ *uap;
{
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	if (uap->length < 0)
		return(EINVAL);
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (vp->v_type == VDIR)
		error = EISDIR;
	else if ((error = vn_writechk(vp)) == 0 &&
	    (error = VOP_ACCESS(vp, VWRITE, p->p_ucred, p)) == 0) {
		VATTR_NULL(&vattr);
		vattr.va_size = SCARG(uap, length);
		error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	}
	vput(vp);
	return (error);
}

/*
 * Truncate a file given a file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct ftruncate_args {
	int	fd;
	int	pad;
	off_t	length;
};
#endif
/* ARGSUSED */
int
ftruncate(p, uap)
	struct proc *p;
	register struct ftruncate_args /* {
		syscallarg(int) fd;
		syscallarg(int) pad;
		syscallarg(off_t) length;
	} */ *uap;
{
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	if (uap->length < 0)
		return(EINVAL);
	if (error = getvnode(p->p_fd, SCARG(uap, fd), &fp))
		return (error);
	if ((fp->f_flag & FWRITE) == 0)
		return (EINVAL);
	vp = (struct vnode *)fp->f_data;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (vp->v_type == VDIR)
		error = EISDIR;
	else if ((error = vn_writechk(vp)) == 0) {
		VATTR_NULL(&vattr);
		vattr.va_size = SCARG(uap, length);
		error = VOP_SETATTR(vp, &vattr, fp->f_cred, p);
	}
	VOP_UNLOCK(vp, 0, p);
	return (error);
}

#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
/*
 * Truncate a file given its path name.
 */
#ifndef _SYS_SYSPROTO_H_
struct otruncate_args {
	char	*path;
	long	length;
};
#endif
/* ARGSUSED */
int
otruncate(p, uap)
	struct proc *p;
	register struct otruncate_args /* {
		syscallarg(char *) path;
		syscallarg(long) length;
	} */ *uap;
{
	struct truncate_args /* {
		syscallarg(char *) path;
		syscallarg(int) pad;
		syscallarg(off_t) length;
	} */ nuap;

	SCARG(&nuap, path) = SCARG(uap, path);
	SCARG(&nuap, length) = SCARG(uap, length);
	return (truncate(p, &nuap));
}

/*
 * Truncate a file given a file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct oftruncate_args {
	int	fd;
	long	length;
};
#endif
/* ARGSUSED */
int
oftruncate(p, uap)
	struct proc *p;
	register struct oftruncate_args /* {
		syscallarg(int) fd;
		syscallarg(long) length;
	} */ *uap;
{
	struct ftruncate_args /* {
		syscallarg(int) fd;
		syscallarg(int) pad;
		syscallarg(off_t) length;
	} */ nuap;

	SCARG(&nuap, fd) = SCARG(uap, fd);
	SCARG(&nuap, length) = SCARG(uap, length);
	return (ftruncate(p, &nuap));
}
#endif /* COMPAT_43 || COMPAT_SUNOS */

/*
 * Sync an open file.
 */
#ifndef _SYS_SYSPROTO_H_
struct fsync_args {
	int	fd;
};
#endif
/* ARGSUSED */
int
fsync(p, uap)
	struct proc *p;
	struct fsync_args /* {
		syscallarg(int) fd;
	} */ *uap;
{
	register struct vnode *vp;
	struct file *fp;
	int error;

	if (error = getvnode(p->p_fd, SCARG(uap, fd), &fp))
		return (error);
	vp = (struct vnode *)fp->f_data;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (vp->v_object) {
		vm_object_page_clean(vp->v_object, 0, 0 ,0, FALSE);
	}
	error = VOP_FSYNC(vp, fp->f_cred,
		(vp->v_mount && (vp->v_mount->mnt_flag & MNT_ASYNC)) ? 
		MNT_NOWAIT : MNT_WAIT, p);
	VOP_UNLOCK(vp, 0, p);
	return (error);
}

/*
 * Rename files.  Source and destination must either both be directories,
 * or both not be directories.  If target is a directory, it must be empty.
 */
#ifndef _SYS_SYSPROTO_H_
struct rename_args {
	char	*from;
	char	*to;
};
#endif
/* ARGSUSED */
int
rename(p, uap)
	struct proc *p;
	register struct rename_args /* {
		syscallarg(char *) from;
		syscallarg(char *) to;
	} */ *uap;
{
	register struct vnode *tvp, *fvp, *tdvp;
	struct nameidata fromnd, tond;
	int error;

	NDINIT(&fromnd, DELETE, WANTPARENT | SAVESTART, UIO_USERSPACE,
	    SCARG(uap, from), p);
	if (error = namei(&fromnd))
		return (error);
	fvp = fromnd.ni_vp;
	NDINIT(&tond, RENAME, LOCKPARENT | LOCKLEAF | NOCACHE | SAVESTART,
	    UIO_USERSPACE, SCARG(uap, to), p);
	if (fromnd.ni_vp->v_type == VDIR)
		tond.ni_cnd.cn_flags |= WILLBEDIR;
	if (error = namei(&tond)) {
		/* Translate error code for rename("dir1", "dir2/."). */
		if (error == EISDIR && fvp->v_type == VDIR)
			error = EINVAL;
		VOP_ABORTOP(fromnd.ni_dvp, &fromnd.ni_cnd);
		vrele(fromnd.ni_dvp);
		vrele(fvp);
		goto out1;
	}
	tdvp = tond.ni_dvp;
	tvp = tond.ni_vp;
	if (tvp != NULL) {
		if (fvp->v_type == VDIR && tvp->v_type != VDIR) {
			error = ENOTDIR;
			goto out;
		} else if (fvp->v_type != VDIR && tvp->v_type == VDIR) {
			error = EISDIR;
			goto out;
		}
	}
	if (fvp == tdvp)
		error = EINVAL;
	/*
	 * If source is the same as the destination (that is the
	 * same inode number with the same name in the same directory),
	 * then there is nothing to do.
	 */
	if (fvp == tvp && fromnd.ni_dvp == tdvp &&
	    fromnd.ni_cnd.cn_namelen == tond.ni_cnd.cn_namelen &&
	    !bcmp(fromnd.ni_cnd.cn_nameptr, tond.ni_cnd.cn_nameptr,
	      fromnd.ni_cnd.cn_namelen))
		error = -1;
out:
	if (!error) {
		VOP_LEASE(tdvp, p, p->p_ucred, LEASE_WRITE);
		if (fromnd.ni_dvp != tdvp)
			VOP_LEASE(fromnd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
		if (tvp) {
			VOP_LEASE(tvp, p, p->p_ucred, LEASE_WRITE);
			(void) vnode_pager_uncache(tvp, p);
		}
		error = VOP_RENAME(fromnd.ni_dvp, fromnd.ni_vp, &fromnd.ni_cnd,
				   tond.ni_dvp, tond.ni_vp, &tond.ni_cnd);
	} else {
		VOP_ABORTOP(tond.ni_dvp, &tond.ni_cnd);
		if (tdvp == tvp)
			vrele(tdvp);
		else
			vput(tdvp);
		if (tvp)
			vput(tvp);
		VOP_ABORTOP(fromnd.ni_dvp, &fromnd.ni_cnd);
		vrele(fromnd.ni_dvp);
		vrele(fvp);
	}
	vrele(tond.ni_startdir);
	ASSERT_VOP_UNLOCKED(fromnd.ni_dvp, "rename");
	ASSERT_VOP_UNLOCKED(fromnd.ni_vp, "rename");
	ASSERT_VOP_UNLOCKED(tond.ni_dvp, "rename");
	ASSERT_VOP_UNLOCKED(tond.ni_vp, "rename");
	zfree(namei_zone, tond.ni_cnd.cn_pnbuf);
out1:
	if (fromnd.ni_startdir)
		vrele(fromnd.ni_startdir);
	zfree(namei_zone, fromnd.ni_cnd.cn_pnbuf);
	if (error == -1)
		return (0);
	return (error);
}

/*
 * Make a directory file.
 */
#ifndef _SYS_SYSPROTO_H_
struct mkdir_args {
	char	*path;
	int	mode;
};
#endif
/* ARGSUSED */
int
mkdir(p, uap)
	struct proc *p;
	register struct mkdir_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap;
{
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, CREATE, LOCKPARENT, UIO_USERSPACE, SCARG(uap, path), p);
	nd.ni_cnd.cn_flags |= WILLBEDIR;
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	if (vp != NULL) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(vp);
		return (EEXIST);
	}
	VATTR_NULL(&vattr);
	vattr.va_type = VDIR;
	vattr.va_mode = (SCARG(uap, mode) & ACCESSPERMS) &~ p->p_fd->fd_cmask;
	VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
	error = VOP_MKDIR(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr);
	if (!error)
		vput(nd.ni_vp);
	ASSERT_VOP_UNLOCKED(nd.ni_dvp, "mkdir");
	ASSERT_VOP_UNLOCKED(nd.ni_vp, "mkdir");
	return (error);
}

/*
 * Remove a directory file.
 */
#ifndef _SYS_SYSPROTO_H_
struct rmdir_args {
	char	*path;
};
#endif
/* ARGSUSED */
int
rmdir(p, uap)
	struct proc *p;
	struct rmdir_args /* {
		syscallarg(char *) path;
	} */ *uap;
{
	register struct vnode *vp;
	int error;
	struct nameidata nd;

	NDINIT(&nd, DELETE, LOCKPARENT | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	if (vp->v_type != VDIR) {
		error = ENOTDIR;
		goto out;
	}
	/*
	 * No rmdir "." please.
	 */
	if (nd.ni_dvp == vp) {
		error = EINVAL;
		goto out;
	}
	/*
	 * The root of a mounted filesystem cannot be deleted.
	 */
	if (vp->v_flag & VROOT)
		error = EBUSY;
out:
	if (!error) {
		VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
		VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
		error = VOP_RMDIR(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
	} else {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vput(vp);
	}
	ASSERT_VOP_UNLOCKED(nd.ni_dvp, "rmdir");
	ASSERT_VOP_UNLOCKED(nd.ni_vp, "rmdir");
	return (error);
}

#ifdef COMPAT_43
/*
 * Read a block of directory entries in a file system independent format.
 */
#ifndef _SYS_SYSPROTO_H_
struct ogetdirentries_args {
	int	fd;
	char	*buf;
	u_int	count;
	long	*basep;
};
#endif
int
ogetdirentries(p, uap)
	struct proc *p;
	register struct ogetdirentries_args /* {
		syscallarg(int) fd;
		syscallarg(char *) buf;
		syscallarg(u_int) count;
		syscallarg(long *) basep;
	} */ *uap;
{
	register struct vnode *vp;
	struct file *fp;
	struct uio auio, kuio;
	struct iovec aiov, kiov;
	struct dirent *dp, *edp;
	caddr_t dirbuf;
	int error, eofflag, readcnt;
	long loff;

	if (error = getvnode(p->p_fd, SCARG(uap, fd), &fp))
		return (error);
	if ((fp->f_flag & FREAD) == 0)
		return (EBADF);
	vp = (struct vnode *)fp->f_data;
unionread:
	if (vp->v_type != VDIR)
		return (EINVAL);
	aiov.iov_base = SCARG(uap, buf);
	aiov.iov_len = SCARG(uap, count);
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;
	auio.uio_resid = SCARG(uap, count);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	loff = auio.uio_offset = fp->f_offset;
#	if (BYTE_ORDER != LITTLE_ENDIAN)
		if (vp->v_mount->mnt_maxsymlinklen <= 0) {
			error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag,
			    NULL, NULL);
			fp->f_offset = auio.uio_offset;
		} else
#	endif
	{
		kuio = auio;
		kuio.uio_iov = &kiov;
		kuio.uio_segflg = UIO_SYSSPACE;
		kiov.iov_len = SCARG(uap, count);
		MALLOC(dirbuf, caddr_t, SCARG(uap, count), M_TEMP, M_WAITOK);
		kiov.iov_base = dirbuf;
		error = VOP_READDIR(vp, &kuio, fp->f_cred, &eofflag,
			    NULL, NULL);
		fp->f_offset = kuio.uio_offset;
		if (error == 0) {
			readcnt = SCARG(uap, count) - kuio.uio_resid;
			edp = (struct dirent *)&dirbuf[readcnt];
			for (dp = (struct dirent *)dirbuf; dp < edp; ) {
#				if (BYTE_ORDER == LITTLE_ENDIAN)
					/*
					 * The expected low byte of
					 * dp->d_namlen is our dp->d_type.
					 * The high MBZ byte of dp->d_namlen
					 * is our dp->d_namlen.
					 */
					dp->d_type = dp->d_namlen;
					dp->d_namlen = 0;
#				else
					/*
					 * The dp->d_type is the high byte
					 * of the expected dp->d_namlen,
					 * so must be zero'ed.
					 */
					dp->d_type = 0;
#				endif
				if (dp->d_reclen > 0) {
					dp = (struct dirent *)
					    ((char *)dp + dp->d_reclen);
				} else {
					error = EIO;
					break;
				}
			}
			if (dp >= edp)
				error = uiomove(dirbuf, readcnt, &auio);
		}
		FREE(dirbuf, M_TEMP);
	}
	VOP_UNLOCK(vp, 0, p);
	if (error)
		return (error);

#ifdef UNION
{
	if ((SCARG(uap, count) == auio.uio_resid) &&
	    (vp->v_op == union_vnodeop_p)) {
		struct vnode *lvp;

		lvp = union_dircache(vp, p);
		if (lvp != NULLVP) {
			struct vattr va;

			/*
			 * If the directory is opaque,
			 * then don't show lower entries
			 */
			error = VOP_GETATTR(vp, &va, fp->f_cred, p);
			if (va.va_flags & OPAQUE) {
				vput(lvp);
				lvp = NULL;
			}
		}
		
		if (lvp != NULLVP) {
			error = VOP_OPEN(lvp, FREAD, fp->f_cred, p);
			if (error) {
				vput(lvp);
				return (error);
			}
			VOP_UNLOCK(lvp, 0, p);
			fp->f_data = (caddr_t) lvp;
			fp->f_offset = 0;
			error = vn_close(vp, FREAD, fp->f_cred, p);
			if (error)
				return (error);
			vp = lvp;
			goto unionread;
		}
	}
}
#endif /* UNION */

	if ((SCARG(uap, count) == auio.uio_resid) &&
	    (vp->v_flag & VROOT) &&
	    (vp->v_mount->mnt_flag & MNT_UNION)) {
		struct vnode *tvp = vp;
		vp = vp->v_mount->mnt_vnodecovered;
		VREF(vp);
		fp->f_data = (caddr_t) vp;
		fp->f_offset = 0;
		vrele(tvp);
		goto unionread;
	}
	error = copyout((caddr_t)&loff, (caddr_t)SCARG(uap, basep),
	    sizeof(long));
	p->p_retval[0] = SCARG(uap, count) - auio.uio_resid;
	return (error);
}
#endif /* COMPAT_43 */

/*
 * Read a block of directory entries in a file system independent format.
 */
#ifndef _SYS_SYSPROTO_H_
struct getdirentries_args {
	int	fd;
	char	*buf;
	u_int	count;
	long	*basep;
};
#endif
int
getdirentries(p, uap)
	struct proc *p;
	register struct getdirentries_args /* {
		syscallarg(int) fd;
		syscallarg(char *) buf;
		syscallarg(u_int) count;
		syscallarg(long *) basep;
	} */ *uap;
{
	register struct vnode *vp;
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	long loff;
	int error, eofflag;

	if (error = getvnode(p->p_fd, SCARG(uap, fd), &fp))
		return (error);
	if ((fp->f_flag & FREAD) == 0)
		return (EBADF);
	vp = (struct vnode *)fp->f_data;
unionread:
	if (vp->v_type != VDIR)
		return (EINVAL);
	aiov.iov_base = SCARG(uap, buf);
	aiov.iov_len = SCARG(uap, count);
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;
	auio.uio_resid = SCARG(uap, count);
	/* vn_lock(vp, LK_SHARED | LK_RETRY, p); */
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	loff = auio.uio_offset = fp->f_offset;
	error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag, NULL, NULL);
	fp->f_offset = auio.uio_offset;
	VOP_UNLOCK(vp, 0, p);
	if (error)
		return (error);

#ifdef UNION
{
	if ((SCARG(uap, count) == auio.uio_resid) &&
	    (vp->v_op == union_vnodeop_p)) {
		struct vnode *lvp;

		lvp = union_dircache(vp, p);
		if (lvp != NULLVP) {
			struct vattr va;

			/*
			 * If the directory is opaque,
			 * then don't show lower entries
			 */
			error = VOP_GETATTR(vp, &va, fp->f_cred, p);
			if (va.va_flags & OPAQUE) {
				vput(lvp);
				lvp = NULL;
			}
		}

		if (lvp != NULLVP) {
			error = VOP_OPEN(lvp, FREAD, fp->f_cred, p);
			if (error) {
				vput(lvp);
				return (error);
			}
			VOP_UNLOCK(lvp, 0, p);
			fp->f_data = (caddr_t) lvp;
			fp->f_offset = 0;
			error = vn_close(vp, FREAD, fp->f_cred, p);
			if (error)
				return (error);
			vp = lvp;
			goto unionread;
		}
	}
}
#endif /* UNION */

	if ((SCARG(uap, count) == auio.uio_resid) &&
	    (vp->v_flag & VROOT) &&
	    (vp->v_mount->mnt_flag & MNT_UNION)) {
		struct vnode *tvp = vp;
		vp = vp->v_mount->mnt_vnodecovered;
		VREF(vp);
		fp->f_data = (caddr_t) vp;
		fp->f_offset = 0;
		vrele(tvp);
		goto unionread;
	}
	error = copyout((caddr_t)&loff, (caddr_t)SCARG(uap, basep),
	    sizeof(long));
	p->p_retval[0] = SCARG(uap, count) - auio.uio_resid;
	return (error);
}

/*
 * Set the mode mask for creation of filesystem nodes.
 */
#ifndef _SYS_SYSPROTO_H_
struct umask_args {
	int	newmask;
};
#endif
int
umask(p, uap)
	struct proc *p;
	struct umask_args /* {
		syscallarg(int) newmask;
	} */ *uap;
{
	register struct filedesc *fdp;

	fdp = p->p_fd;
	p->p_retval[0] = fdp->fd_cmask;
	fdp->fd_cmask = SCARG(uap, newmask) & ALLPERMS;
	return (0);
}

/*
 * Void all references to file by ripping underlying filesystem
 * away from vnode.
 */
#ifndef _SYS_SYSPROTO_H_
struct revoke_args {
	char	*path;
};
#endif
/* ARGSUSED */
int
revoke(p, uap)
	struct proc *p;
	register struct revoke_args /* {
		syscallarg(char *) path;
	} */ *uap;
{
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	vp = nd.ni_vp;
	if (error = VOP_GETATTR(vp, &vattr, p->p_ucred, p))
		goto out;
	if (p->p_ucred->cr_uid != vattr.va_uid &&
	    (error = suser(p->p_ucred, &p->p_acflag)))
		goto out;
	if (vp->v_usecount > 1 || (vp->v_flag & VALIASED))
		VOP_REVOKE(vp, REVOKEALL);
out:
	vrele(vp);
	return (error);
}

/*
 * Convert a user file descriptor to a kernel file entry.
 */
int
getvnode(fdp, fd, fpp)
	struct filedesc *fdp;
	int fd;
	struct file **fpp;
{
	struct file *fp;

	if ((u_int)fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_VNODE && fp->f_type != DTYPE_FIFO)
		return (EINVAL);
	*fpp = fp;
	return (0);
}
#ifndef _SYS_SYSPROTO_H_
struct  __getcwd_args {
	u_char	*buf;
	u_int	buflen;
};
#endif
#define STATNODE(mode, name, var) \
	SYSCTL_INT(_vfs_cache, OID_AUTO, name, mode, var, 0, "");

static int disablecwd;
SYSCTL_INT(_debug, OID_AUTO, disablecwd, CTLFLAG_RW, &disablecwd, 0, "");

static u_long numcwdcalls; STATNODE(CTLFLAG_RD, numcwdcalls, &numcwdcalls);
static u_long numcwdfail1; STATNODE(CTLFLAG_RD, numcwdfail1, &numcwdfail1);
static u_long numcwdfail2; STATNODE(CTLFLAG_RD, numcwdfail2, &numcwdfail2);
static u_long numcwdfail3; STATNODE(CTLFLAG_RD, numcwdfail3, &numcwdfail3);
static u_long numcwdfail4; STATNODE(CTLFLAG_RD, numcwdfail4, &numcwdfail4);
static u_long numcwdfound; STATNODE(CTLFLAG_RD, numcwdfound, &numcwdfound);
int
__getcwd(p, uap)
	struct proc *p;
	struct __getcwd_args *uap;
{
	struct filedesc *fdp;
	struct vnode *vp;
	struct namecache *ncp;
	char *buf, *bp;
	int i, j, error;

	if (disablecwd)
		return (ENODEV);
	fdp = p->p_fd;
	j = 0;
	error = 0;
	if (uap->buflen < 2)
		return (EINVAL);
	if (uap->buflen > MAXPATHLEN)
		uap->buflen = MAXPATHLEN;
	buf = bp = malloc(uap->buflen, M_TEMP, M_WAITOK);
	bp += uap->buflen - 1;
	*bp = '\0';
	numcwdcalls++;
	for (vp = fdp->fd_cdir; vp != fdp->fd_rdir && vp != rootvnode;) {
		if (vp->v_flag & VROOT) {
			vp = vp->v_mount->mnt_vnodecovered;
			continue;
		}
		if (vp->v_dd->v_id != vp->v_ddid) {
			numcwdfail1++;
			free(buf, M_TEMP);
			return (ENOTDIR);
		}
		ncp = TAILQ_FIRST(&vp->v_cache_dst);
		if (!ncp) {
			numcwdfail2++;
			free(buf, M_TEMP);
			return (ENOENT);
		}
		if (ncp->nc_dvp != vp->v_dd) {
			numcwdfail3++;
			free(buf, M_TEMP);
			return (EBADF);
		}
		for (i=ncp->nc_nlen - 1; i >= 0; i--) {
			if (bp == buf) {
				numcwdfail4++;
				free(buf, M_TEMP);
				return (ENOMEM);
			}
			*--bp = ncp->nc_name[i];
		}
		if (bp == buf) {
			numcwdfail4++;
			free(buf, M_TEMP);
			return (ENOMEM);
		}
		*--bp = '/';
		j++;
		vp = vp->v_dd;
	}
	if (!j) {
		if (bp == buf) {
			numcwdfail4++;
			free(buf, M_TEMP);
			return (ENOMEM);
		}
		*--bp = '/';
	}
	numcwdfound++;
	error = copyout(bp, uap->buf, strlen(bp) + 1);
	free(buf, M_TEMP);
	return (error);
}
