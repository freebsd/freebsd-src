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
 * $Id: vfs_syscalls.c,v 1.25.4.3 1995/10/26 09:17:19 davidg Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/dirent.h>

#ifdef UNION
#include <miscfs/union/union.h>
#endif

#include <vm/vm.h>
#include <sys/sysctl.h>

static int change_dir	__P((struct nameidata *ndp, struct proc *p));
int getvnode __P((struct filedesc *, int, struct file **));

/*
 * Virtual File System System Calls
 */

/*
 * Mount a file system.
 */
struct mount_args {
	int	type;
	char	*path;
	int	flags;
	caddr_t	data;
};
/* ARGSUSED */
int
mount(p, uap, retval)
	struct proc *p;
	register struct mount_args *uap;
	int *retval;
{
	register struct vnode *vp;
	register struct mount *mp;
	int error, flag = 0;
	struct nameidata nd;

	/*
	 * Must be super user
	 */
	error = suser(p->p_ucred, &p->p_acflag);
	if (error)
		return (error);
	/*
	 * Get vnode to be covered
	 */
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE, uap->path, p);
	error = namei(&nd);
	if (error)
		return (error);
	vp = nd.ni_vp;
	if (uap->flags & MNT_UPDATE) {
		if ((vp->v_flag & VROOT) == 0) {
			vput(vp);
			return (EINVAL);
		}
		mp = vp->v_mount;
		flag = mp->mnt_flag;
		/*
		 * We only allow the filesystem to be reloaded if it
		 * is currently mounted read-only.
		 */
		if ((uap->flags & MNT_RELOAD) &&
		    ((mp->mnt_flag & MNT_RDONLY) == 0)) {
			vput(vp);
			return (EOPNOTSUPP);	/* Needs translation */
		}
		mp->mnt_flag |=
		    uap->flags & (MNT_RELOAD | MNT_FORCE | MNT_UPDATE);
		VOP_UNLOCK(vp);
		goto update;
	}
	error = vinvalbuf(vp, V_SAVE, p->p_ucred, p, 0, 0);
	if (error)
		return (error);
	if (vp->v_type != VDIR) {
		vput(vp);
		return (ENOTDIR);
	}
	if ((u_long)uap->type > MOUNT_MAXTYPE || vfssw[uap->type] == NULL) {
		vput(vp);
		return (ENODEV);
	}

	/*
	 * Allocate and initialize the file system.
	 */
	mp = (struct mount *)malloc((u_long)sizeof(struct mount),
		M_MOUNT, M_WAITOK);
	bzero((char *)mp, (u_long)sizeof(struct mount));
	mp->mnt_op = vfssw[uap->type];
	mp->mnt_vfc = vfsconf[uap->type];
	error = vfs_lock(mp);
	if (error) {
		free((caddr_t)mp, M_MOUNT);
		vput(vp);
		return (error);
	}
	if (vp->v_mountedhere != NULL) {
		vfs_unlock(mp);
		free((caddr_t)mp, M_MOUNT);
		vput(vp);
		return (EBUSY);
	}
	vp->v_mountedhere = mp;
	mp->mnt_vnodecovered = vp;
	vfsconf[uap->type]->vfc_refcount++;

update:
	/*
	 * Set the mount level flags.
	 */
	if (uap->flags & MNT_RDONLY)
		mp->mnt_flag |= MNT_RDONLY;
	else if (mp->mnt_flag & MNT_RDONLY)
		mp->mnt_flag |= MNT_WANTRDWR;
	mp->mnt_flag &=~ (MNT_NOSUID | MNT_NOEXEC | MNT_NODEV |
	    MNT_SYNCHRONOUS | MNT_UNION | MNT_ASYNC);
	mp->mnt_flag |= uap->flags & (MNT_NOSUID | MNT_NOEXEC | MNT_NODEV |
	    MNT_SYNCHRONOUS | MNT_UNION | MNT_ASYNC | MNT_FORCE);
	/*
	 * Mount the filesystem.
	 */
	error = VFS_MOUNT(mp, uap->path, uap->data, &nd, p);
	if (mp->mnt_flag & MNT_UPDATE) {
		vrele(vp);
		if (mp->mnt_flag & MNT_WANTRDWR)
			mp->mnt_flag &= ~MNT_RDONLY;
		mp->mnt_flag &=~
		    (MNT_UPDATE | MNT_RELOAD | MNT_FORCE | MNT_WANTRDWR);
		if (error)
			mp->mnt_flag = flag;
		return (error);
	}
	/*
	 * Put the new filesystem on the mount list after root.
	 */
	cache_purge(vp);
	if (!error) {
		TAILQ_INSERT_TAIL(&mountlist, mp, mnt_list);
		VOP_UNLOCK(vp);
		vfs_unlock(mp);
		error = VFS_START(mp, 0, p);
	} else {
		mp->mnt_vnodecovered->v_mountedhere = (struct mount *)0;
		vfs_unlock(mp);
		free((caddr_t)mp, M_MOUNT);
		vput(vp);
		vfsconf[uap->type]->vfc_refcount--;
	}
	return (error);
}

/*
 * Unmount a file system.
 *
 * Note: unmount takes a path to the vnode mounted on as argument,
 * not special file (as before).
 */
struct unmount_args {
	char	*path;
	int	flags;
};
/* ARGSUSED */
int
unmount(p, uap, retval)
	struct proc *p;
	register struct unmount_args *uap;
	int *retval;
{
	register struct vnode *vp;
	struct mount *mp;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE, uap->path, p);
	error = namei(&nd);
	if (error)
		return (error);
	vp = nd.ni_vp;

	/*
	 * Unless this is a user mount, then must
	 * have suser privilege.
	 */
	if (((vp->v_mount->mnt_flag & MNT_USER) == 0) &&
	    (error = suser(p->p_ucred, &p->p_acflag))) {
		vput(vp);
		return (error);
	}

	/*
	 * Must be the root of the filesystem
	 */
	if ((vp->v_flag & VROOT) == 0) {
		vput(vp);
		return (EINVAL);
	}
	mp = vp->v_mount;
	vput(vp);

	/*
	 * Don't allow unmount of the root filesystem
	 */
	if (mp->mnt_flag & MNT_ROOTFS)
		return (EINVAL);

	return (dounmount(mp, uap->flags, p));
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

	coveredvp = mp->mnt_vnodecovered;
	if (vfs_busy(mp))
		return (EBUSY);
	mp->mnt_flag |= MNT_UNMOUNT;
	error = vfs_lock(mp);
	if (error)
		return (error);

	mp->mnt_flag &=~ MNT_ASYNC;
	vfs_msync(mp, MNT_NOWAIT);
	vnode_pager_umount(mp);	/* release cached vnodes */
	cache_purgevfs(mp);	/* remove cache entries for this file sys */
	if ((error = VFS_SYNC(mp, MNT_WAIT, p->p_ucred, p)) == 0 ||
	    (flags & MNT_FORCE))
		error = VFS_UNMOUNT(mp, flags, p);
	mp->mnt_flag &= ~MNT_UNMOUNT;
	vfs_unbusy(mp);
	if (error) {
		vfs_unlock(mp);
	} else {
		vrele(coveredvp);
		TAILQ_REMOVE(&mountlist, mp, mnt_list);
		mp->mnt_vnodecovered->v_mountedhere = (struct mount *)0;
		vfs_unlock(mp);
		mp->mnt_vfc->vfc_refcount--;
		if (mp->mnt_vnodelist.lh_first != NULL)
			panic("unmount: dangling vnode");
		free((caddr_t)mp, M_MOUNT);
	}
	return (error);
}

/*
 * Sync each mounted filesystem.
 */
#ifdef DIAGNOSTIC
int syncprt = 0;
struct ctldebug debug0 = { "syncprt", &syncprt };
#endif

/* ARGSUSED */
int
sync(p, uap, retval)
	struct proc *p;
	struct sync_args *uap;
	int *retval;
{
	register struct mount *mp;
	int asyncflag;

	for (mp = mountlist.tqh_first; mp != NULL; mp = mp->mnt_list.tqe_next) {
		/*
		 * The lock check below is to avoid races with mount
		 * and unmount.
		 */
		if ((mp->mnt_flag & (MNT_MLOCK|MNT_RDONLY|MNT_MPBUSY)) == 0 &&
		    !vfs_busy(mp)) {
			asyncflag = mp->mnt_flag & MNT_ASYNC;
			mp->mnt_flag &= ~MNT_ASYNC;
			vfs_msync(mp, MNT_NOWAIT);
			VFS_SYNC(mp, MNT_NOWAIT, p != NULL ? p->p_ucred : NOCRED, p);
			if (asyncflag)
				mp->mnt_flag |= MNT_ASYNC;
			vfs_unbusy(mp);
		}
	}
	return (0);
}

/*
 * Change filesystem quotas.
 */
struct quotactl_args {
	char *path;
	int cmd;
	int uid;
	caddr_t arg;
};
/* ARGSUSED */
int
quotactl(p, uap, retval)
	struct proc *p;
	register struct quotactl_args *uap;
	int *retval;
{
	register struct mount *mp;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, uap->path, p);
	error = namei(&nd);
	if (error)
		return (error);
	mp = nd.ni_vp->v_mount;
	vrele(nd.ni_vp);
	return (VFS_QUOTACTL(mp, uap->cmd, uap->uid, uap->arg, p));
}

/*
 * Get filesystem statistics.
 */
struct statfs_args {
	char *path;
	struct statfs *buf;
};
/* ARGSUSED */
int
statfs(p, uap, retval)
	struct proc *p;
	register struct statfs_args *uap;
	int *retval;
{
	register struct mount *mp;
	register struct statfs *sp;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, uap->path, p);
	error = namei(&nd);
	if (error)
		return (error);
	mp = nd.ni_vp->v_mount;
	sp = &mp->mnt_stat;
	vrele(nd.ni_vp);
	error = VFS_STATFS(mp, sp, p);
	if (error)
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	return (copyout((caddr_t)sp, (caddr_t)uap->buf, sizeof(*sp)));
}

/*
 * Get filesystem statistics.
 */
struct fstatfs_args {
	int fd;
	struct statfs *buf;
};
/* ARGSUSED */
int
fstatfs(p, uap, retval)
	struct proc *p;
	register struct fstatfs_args *uap;
	int *retval;
{
	struct file *fp;
	struct mount *mp;
	register struct statfs *sp;
	int error;

	error = getvnode(p->p_fd, uap->fd, &fp);
	if (error)
		return (error);
	mp = ((struct vnode *)fp->f_data)->v_mount;
	sp = &mp->mnt_stat;
	error = VFS_STATFS(mp, sp, p);
	if (error)
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	return (copyout((caddr_t)sp, (caddr_t)uap->buf, sizeof(*sp)));
}

/*
 * Get statistics on all filesystems.
 */
struct getfsstat_args {
	struct statfs *buf;
	long bufsize;
	int flags;
};
int
getfsstat(p, uap, retval)
	struct proc *p;
	register struct getfsstat_args *uap;
	int *retval;
{
	register struct mount *mp, *nmp;
	register struct statfs *sp;
	caddr_t sfsp;
	long count, maxcount, error;

	maxcount = uap->bufsize / sizeof(struct statfs);
	sfsp = (caddr_t)uap->buf;
	for (count = 0, mp = mountlist.tqh_first; mp != NULL; mp = nmp) {
		if (vfs_busy(mp)) {
			nmp = mp->mnt_list.tqe_next;
			continue;
		}
		if (sfsp && count < maxcount &&
		    ((mp->mnt_flag & MNT_MLOCK) == 0)) {
			sp = &mp->mnt_stat;
			/*
			 * If MNT_NOWAIT is specified, do not refresh the
			 * fsstat cache. MNT_WAIT overrides MNT_NOWAIT.
			 */
			if (((uap->flags & MNT_NOWAIT) == 0 ||
			    (uap->flags & MNT_WAIT)) &&
			    (error = VFS_STATFS(mp, sp, p))) {
				nmp = mp->mnt_list.tqe_next;
				vfs_unbusy(mp);
				continue;
			}
			sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
			error = copyout((caddr_t)sp, sfsp, sizeof(*sp));
			if (error) {
				vfs_unbusy(mp);
				return (error);
			}
			sfsp += sizeof(*sp);
		}
		count++;
		nmp = mp->mnt_list.tqe_next;
		vfs_unbusy(mp);
	}
	if (sfsp && count > maxcount)
		*retval = maxcount;
	else
		*retval = count;
	return (0);
}

/*
 * Change current working directory to a given file descriptor.
 */
struct fchdir_args {
	int	fd;
};
/* ARGSUSED */
int
fchdir(p, uap, retval)
	struct proc *p;
	struct fchdir_args *uap;
	int *retval;
{
	register struct filedesc *fdp = p->p_fd;
	register struct vnode *vp;
	struct file *fp;
	int error;

	error = getvnode(fdp, uap->fd, &fp);
	if (error)
		return (error);
	vp = (struct vnode *)fp->f_data;
	VOP_LOCK(vp);
	if (vp->v_type != VDIR)
		error = ENOTDIR;
	else
		error = VOP_ACCESS(vp, VEXEC, p->p_ucred, p);
	VOP_UNLOCK(vp);
	if (error)
		return (error);
	VREF(vp);
	vrele(fdp->fd_cdir);
	fdp->fd_cdir = vp;
	return (0);
}

/*
 * Change current working directory (``.'').
 */
struct chdir_args {
	char	*path;
};
/* ARGSUSED */
int
chdir(p, uap, retval)
	struct proc *p;
	struct chdir_args *uap;
	int *retval;
{
	register struct filedesc *fdp = p->p_fd;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE, uap->path, p);
	error = change_dir(&nd, p);
	if (error)
		return (error);
	vrele(fdp->fd_cdir);
	fdp->fd_cdir = nd.ni_vp;
	return (0);
}

/*
 * Change notion of root (``/'') directory.
 */
struct chroot_args {
	char	*path;
};
/* ARGSUSED */
int
chroot(p, uap, retval)
	struct proc *p;
	struct chroot_args *uap;
	int *retval;
{
	register struct filedesc *fdp = p->p_fd;
	int error;
	struct nameidata nd;

	error = suser(p->p_ucred, &p->p_acflag);
	if (error)
		return (error);
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE, uap->path, p);
	error = change_dir(&nd, p);
	if (error)
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
	VOP_UNLOCK(vp);
	if (error)
		vrele(vp);
	return (error);
}

/*
 * Check permissions, allocate an open file structure,
 * and call the device open routine if any.
 */
struct open_args {
	char	*path;
	int	flags;
	int	mode;
};
int
open(p, uap, retval)
	struct proc *p;
	register struct open_args *uap;
	int *retval;
{
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	register struct vnode *vp;
	int flags, cmode;
	struct file *nfp;
	int type, indx, error;
	struct flock lf;
	struct nameidata nd;

	error = falloc(p, &nfp, &indx);
	if (error)
		return (error);
	fp = nfp;
	flags = FFLAGS(uap->flags);
	cmode = ((uap->mode &~ fdp->fd_cmask) & ALLPERMS) &~ S_ISTXT;
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, uap->path, p);
	p->p_dupfd = -indx - 1;			/* XXX check for fdopen */
	error = vn_open(&nd, flags, cmode);
	if (error) {
		ffree(fp);
		if ((error == ENODEV || error == ENXIO) &&
		    p->p_dupfd >= 0 && 			/* XXX from fdopen */
		    (error =
		        dupfdopen(fdp, indx, p->p_dupfd, flags, error)) == 0) {
			*retval = indx;
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
	fp->f_type = DTYPE_VNODE;
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
		VOP_UNLOCK(vp);
		error = VOP_ADVLOCK(vp, (caddr_t)fp, F_SETLK, &lf, type);
		if (error) {
			(void) vn_close(vp, fp->f_flag, fp->f_cred, p);
			ffree(fp);
			fdp->fd_ofiles[indx] = NULL;
			return (error);
		}
		VOP_LOCK(vp);
		fp->f_flag |= FHASLOCK;
	}
	VOP_UNLOCK(vp);
	*retval = indx;
	return (0);
}

#ifdef COMPAT_43
/*
 * Create a file.
 */
struct ocreat_args {
	char	*path;
	int	mode;
};
int
ocreat(p, uap, retval)
	struct proc *p;
	register struct ocreat_args *uap;
	int *retval;
{
	struct open_args openuap;

	openuap.path = uap->path;
	openuap.mode = uap->mode;
	openuap.flags = O_WRONLY | O_CREAT | O_TRUNC;
	return (open(p, &openuap, retval));
}
#endif /* COMPAT_43 */

/*
 * Create a special file.
 */
struct mknod_args {
	char	*path;
	int	mode;
	int	dev;
};
/* ARGSUSED */
int
mknod(p, uap, retval)
	struct proc *p;
	register struct mknod_args *uap;
	int *retval;
{
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	error = suser(p->p_ucred, &p->p_acflag);
	if (error)
		return (error);
	NDINIT(&nd, CREATE, LOCKPARENT, UIO_USERSPACE, uap->path, p);
	error = namei(&nd);
	if (error)
		return (error);
	vp = nd.ni_vp;
	if (vp != NULL)
		error = EEXIST;
	else {
		VATTR_NULL(&vattr);
		vattr.va_mode = (uap->mode & ALLPERMS) &~ p->p_fd->fd_cmask;
		vattr.va_rdev = uap->dev;

		switch (uap->mode & S_IFMT) {
		case S_IFMT:	/* used by badsect to flag bad sectors */
			vattr.va_type = VBAD;
			break;
		case S_IFCHR:
			vattr.va_type = VCHR;
			break;
		case S_IFBLK:
			vattr.va_type = VBLK;
			break;
		default:
			error = EINVAL;
			break;
		}
	}
	if (!error) {
		LEASE_CHECK(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
		error = VOP_MKNOD(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr);
	} else {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (vp)
			vrele(vp);
	}
	return (error);
}

/*
 * Create named pipe.
 */
struct mkfifo_args {
	char	*path;
	int	mode;
};
/* ARGSUSED */
int
mkfifo(p, uap, retval)
	struct proc *p;
	register struct mkfifo_args *uap;
	int *retval;
{
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, CREATE, LOCKPARENT, UIO_USERSPACE, uap->path, p);
	error = namei(&nd);
	if (error)
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
	vattr.va_mode = (uap->mode & ALLPERMS) &~ p->p_fd->fd_cmask;
	LEASE_CHECK(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
	return (VOP_MKNOD(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr));
}

/*
 * Make a hard file link.
 */
struct link_args {
	char	*path;
	char	*link;
};
/* ARGSUSED */
int
link(p, uap, retval)
	struct proc *p;
	register struct link_args *uap;
	int *retval;
{
	register struct vnode *vp;
	struct nameidata nd;
	int error;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, uap->path, p);
	error = namei(&nd);
	if (error)
		return (error);
	vp = nd.ni_vp;
	if (vp->v_type != VDIR ||
	    (error = suser(p->p_ucred, &p->p_acflag)) == 0) {
		nd.ni_cnd.cn_nameiop = CREATE;
		nd.ni_cnd.cn_flags = LOCKPARENT;
		if (vp->v_type == VDIR)
			nd.ni_cnd.cn_flags |= WILLBEDIR;
		nd.ni_dirp = uap->link;
		error = namei(&nd);
		if (!error) {
			if (nd.ni_vp != NULL)
				error = EEXIST;
			if (!error) {
				LEASE_CHECK(nd.ni_dvp,
				    p, p->p_ucred, LEASE_WRITE);
				LEASE_CHECK(vp,
				    p, p->p_ucred, LEASE_WRITE);
				error = VOP_LINK(nd.ni_dvp, vp, &nd.ni_cnd);
			} else {
				VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
				if (nd.ni_dvp == nd.ni_vp)
					vrele(nd.ni_dvp);
				else
					vput(nd.ni_dvp);
				if (nd.ni_vp)
					vrele(nd.ni_vp);
			}
		}
	}
	vrele(vp);
	return (error);
}

/*
 * Make a symbolic link.
 */
struct symlink_args {
	char	*path;
	char	*link;
};
/* ARGSUSED */
int
symlink(p, uap, retval)
	struct proc *p;
	register struct symlink_args *uap;
	int *retval;
{
	struct vattr vattr;
	char *path;
	int error;
	struct nameidata nd;

	MALLOC(path, char *, MAXPATHLEN, M_NAMEI, M_WAITOK);
	error = copyinstr(uap->path, path, MAXPATHLEN, NULL);
	if (error)
		goto out;
	NDINIT(&nd, CREATE, LOCKPARENT, UIO_USERSPACE, uap->link, p);
	error = namei(&nd);
	if (error)
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
	LEASE_CHECK(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
	error = VOP_SYMLINK(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr, path);
out:
	FREE(path, M_NAMEI);
	return (error);
}

/*
 * Delete a name from the filesystem.
 */
struct unlink_args {
	char	*path;
};
/* ARGSUSED */
int
unlink(p, uap, retval)
	struct proc *p;
	struct unlink_args *uap;
	int *retval;
{
	register struct vnode *vp;
	int error;
	struct nameidata nd;

	NDINIT(&nd, DELETE, LOCKPARENT, UIO_USERSPACE, uap->path, p);
	error = namei(&nd);
	if (error)
		return (error);
	vp = nd.ni_vp;
	LEASE_CHECK(vp, p, p->p_ucred, LEASE_WRITE);
	VOP_LOCK(vp);

	if (vp->v_type != VDIR ||
	    (error = suser(p->p_ucred, &p->p_acflag)) == 0) {
		/*
		 * The root of a mounted filesystem cannot be deleted.
		 */
		if (vp->v_flag & VROOT)
			error = EBUSY;
		else
			(void) vnode_pager_uncache(vp);
	}

	if (!error) {
		LEASE_CHECK(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
		error = VOP_REMOVE(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
	} else {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vput(vp);
	}
	return (error);
}

/*
 * Reposition read/write file offset.
 */
struct lseek_args {
	int	fd;
	int	pad;
	off_t	offset;
	int	whence;
};
int
lseek(p, uap, retval)
	struct proc *p;
	register struct lseek_args *uap;
	int *retval;
{
	struct ucred *cred = p->p_ucred;
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	struct vattr vattr;
	int error;

	if ((u_int)uap->fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[uap->fd]) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_VNODE)
		return (ESPIPE);
	switch (uap->whence) {
	case L_INCR:
		fp->f_offset += uap->offset;
		break;
	case L_XTND:
		error=VOP_GETATTR((struct vnode *)fp->f_data, &vattr, cred, p);
		if (error)
			return (error);
		fp->f_offset = uap->offset + vattr.va_size;
		break;
	case L_SET:
		fp->f_offset = uap->offset;
		break;
	default:
		return (EINVAL);
	}
	*(off_t *)retval = fp->f_offset;
	return (0);
}

#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
/*
 * Reposition read/write file offset.
 */
struct olseek_args {
	int	fd;
	long	offset;
	int	whence;
};
int
olseek(p, uap, retval)
	struct proc *p;
	register struct olseek_args *uap;
	int *retval;
{
	struct lseek_args nuap;
	off_t qret;
	int error;

	nuap.fd = uap->fd;
	nuap.offset = uap->offset;
	nuap.whence = uap->whence;
	error = lseek(p, &nuap, &qret);
	*(long *)retval = qret;
	return (error);
}
#endif /* COMPAT_43 */

/*
 * Check access permissions.
 */
struct access_args {
	char	*path;
	int	flags;
};
int
access(p, uap, retval)
	struct proc *p;
	register struct access_args *uap;
	int *retval;
{
	register struct ucred *cred = p->p_ucred;
	register struct vnode *vp;
	int error, flags, t_gid, t_uid;
	struct nameidata nd;

	t_uid = cred->cr_uid;
	t_gid = cred->cr_groups[0];
	cred->cr_uid = p->p_cred->p_ruid;
	cred->cr_groups[0] = p->p_cred->p_rgid;
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE, uap->path, p);
	error = namei(&nd);
	if (error)
		goto out1;
	vp = nd.ni_vp;

	/* Flags == 0 means only check for existence. */
	if (uap->flags) {
		flags = 0;
		if (uap->flags & R_OK)
			flags |= VREAD;
		if (uap->flags & W_OK)
			flags |= VWRITE;
		if (uap->flags & X_OK)
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
struct ostat_args {
	char	*path;
	struct ostat *ub;
};
/* ARGSUSED */
int
ostat(p, uap, retval)
	struct proc *p;
	register struct ostat_args *uap;
	int *retval;
{
	struct stat sb;
	struct ostat osb;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE, uap->path, p);
	error = namei(&nd);
	if (error)
		return (error);
	error = vn_stat(nd.ni_vp, &sb, p);
	vput(nd.ni_vp);
	if (error)
		return (error);
	cvtstat(&sb, &osb);
	error = copyout((caddr_t)&osb, (caddr_t)uap->ub, sizeof (osb));
	return (error);
}

/*
 * Get file status; this version does not follow links.
 */
struct olstat_args {
	char	*path;
	struct ostat *ub;
};
/* ARGSUSED */
int
olstat(p, uap, retval)
	struct proc *p;
	register struct olstat_args *uap;
	int *retval;
{
	struct vnode *vp, *dvp;
	struct stat sb, sb1;
	struct ostat osb;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF | LOCKPARENT, UIO_USERSPACE,
	    uap->path, p);
	error = namei(&nd);
	if (error)
		return (error);
	/*
	 * For symbolic links, always return the attributes of its
	 * containing directory, except for mode, size, and links.
	 */
	vp = nd.ni_vp;
	dvp = nd.ni_dvp;
	if (vp->v_type != VLNK) {
		if (dvp == vp)
			vrele(dvp);
		else
			vput(dvp);
		error = vn_stat(vp, &sb, p);
		vput(vp);
		if (error)
			return (error);
	} else {
		error = vn_stat(dvp, &sb, p);
		vput(dvp);
		if (error) {
			vput(vp);
			return (error);
		}
		error = vn_stat(vp, &sb1, p);
		vput(vp);
		if (error)
			return (error);
		sb.st_mode &= ~S_IFDIR;
		sb.st_mode |= S_IFLNK;
		sb.st_nlink = sb1.st_nlink;
		sb.st_size = sb1.st_size;
		sb.st_blocks = sb1.st_blocks;
	}
	cvtstat(&sb, &osb);
	error = copyout((caddr_t)&osb, (caddr_t)uap->ub, sizeof (osb));
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
struct stat_args {
	char	*path;
	struct stat *ub;
};
/* ARGSUSED */
int
stat(p, uap, retval)
	struct proc *p;
	register struct stat_args *uap;
	int *retval;
{
	struct stat sb;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE, uap->path, p);
	error = namei(&nd);
	if (error)
		return (error);
	error = vn_stat(nd.ni_vp, &sb, p);
	vput(nd.ni_vp);
	if (error)
		return (error);
	error = copyout((caddr_t)&sb, (caddr_t)uap->ub, sizeof (sb));
	return (error);
}

/*
 * Get file status; this version does not follow links.
 */
struct lstat_args {
	char	*path;
	struct stat *ub;
};
/* ARGSUSED */
int
lstat(p, uap, retval)
	struct proc *p;
	register struct lstat_args *uap;
	int *retval;
{
	int error;
	struct vnode *vp, *dvp;
	struct stat sb, sb1;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF | LOCKPARENT, UIO_USERSPACE,
	    uap->path, p);
	error = namei(&nd);
	if (error)
		return (error);
	/*
	 * For symbolic links, always return the attributes of its
	 * containing directory, except for mode, size, and links.
	 */
	vp = nd.ni_vp;
	dvp = nd.ni_dvp;
	if (vp->v_type != VLNK) {
		if (dvp == vp)
			vrele(dvp);
		else
			vput(dvp);
		error = vn_stat(vp, &sb, p);
		vput(vp);
		if (error)
			return (error);
	} else {
		error = vn_stat(dvp, &sb, p);
		vput(dvp);
		if (error) {
			vput(vp);
			return (error);
		}
		error = vn_stat(vp, &sb1, p);
		vput(vp);
		if (error)
			return (error);
		sb.st_mode &= ~S_IFDIR;
		sb.st_mode |= S_IFLNK;
		sb.st_nlink = sb1.st_nlink;
		sb.st_size = sb1.st_size;
		sb.st_blocks = sb1.st_blocks;
	}
	error = copyout((caddr_t)&sb, (caddr_t)uap->ub, sizeof (sb));
	return (error);
}

/*
 * Get configurable pathname variables.
 */
struct pathconf_args {
	char	*path;
	int	name;
};
/* ARGSUSED */
int
pathconf(p, uap, retval)
	struct proc *p;
	register struct pathconf_args *uap;
	int *retval;
{
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE, uap->path, p);
	error = namei(&nd);
	if (error)
		return (error);
	error = VOP_PATHCONF(nd.ni_vp, uap->name, retval);
	vput(nd.ni_vp);
	return (error);
}

/*
 * Return target name of a symbolic link.
 */
struct readlink_args {
	char	*path;
	char	*buf;
	int	count;
};
/* ARGSUSED */
int
readlink(p, uap, retval)
	struct proc *p;
	register struct readlink_args *uap;
	int *retval;
{
	register struct vnode *vp;
	struct iovec aiov;
	struct uio auio;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF, UIO_USERSPACE, uap->path, p);
	error = namei(&nd);
	if (error)
		return (error);
	vp = nd.ni_vp;
	if (vp->v_type != VLNK)
		error = EINVAL;
	else {
		aiov.iov_base = uap->buf;
		aiov.iov_len = uap->count;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = 0;
		auio.uio_rw = UIO_READ;
		auio.uio_segflg = UIO_USERSPACE;
		auio.uio_procp = p;
		auio.uio_resid = uap->count;
		error = VOP_READLINK(vp, &auio, p->p_ucred);
	}
	vput(vp);
	*retval = uap->count - auio.uio_resid;
	return (error);
}

/*
 * Change flags of a file given a path name.
 */
struct chflags_args {
	char	*path;
	int	flags;
};
/* ARGSUSED */
int
chflags(p, uap, retval)
	struct proc *p;
	register struct chflags_args *uap;
	int *retval;
{
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, uap->path, p);
	error = namei(&nd);
	if (error)
		return (error);
	vp = nd.ni_vp;
	LEASE_CHECK(vp, p, p->p_ucred, LEASE_WRITE);
	VOP_LOCK(vp);
	VATTR_NULL(&vattr);
	vattr.va_flags = uap->flags;
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	vput(vp);
	return (error);
}

/*
 * Change flags of a file given a file descriptor.
 */
struct fchflags_args {
	int	fd;
	int	flags;
};
/* ARGSUSED */
int
fchflags(p, uap, retval)
	struct proc *p;
	register struct fchflags_args *uap;
	int *retval;
{
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	error = getvnode(p->p_fd, uap->fd, &fp);
	if (error)
		return (error);
	vp = (struct vnode *)fp->f_data;
	LEASE_CHECK(vp, p, p->p_ucred, LEASE_WRITE);
	VOP_LOCK(vp);
	VATTR_NULL(&vattr);
	vattr.va_flags = uap->flags;
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	VOP_UNLOCK(vp);
	return (error);
}

/*
 * Change mode of a file given path name.
 */
struct chmod_args {
	char	*path;
	int	mode;
};
/* ARGSUSED */
int
chmod(p, uap, retval)
	struct proc *p;
	register struct chmod_args *uap;
	int *retval;
{
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, uap->path, p);
	error = namei(&nd);
	if (error)
		return (error);
	vp = nd.ni_vp;
	LEASE_CHECK(vp, p, p->p_ucred, LEASE_WRITE);
	VOP_LOCK(vp);
	VATTR_NULL(&vattr);
	vattr.va_mode = uap->mode & ALLPERMS;
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	vput(vp);
	return (error);
}

/*
 * Change mode of a file given a file descriptor.
 */
struct fchmod_args {
	int	fd;
	int	mode;
};
/* ARGSUSED */
int
fchmod(p, uap, retval)
	struct proc *p;
	register struct fchmod_args *uap;
	int *retval;
{
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	error = getvnode(p->p_fd, uap->fd, &fp);
	if (error)
		return (error);
	vp = (struct vnode *)fp->f_data;
	LEASE_CHECK(vp, p, p->p_ucred, LEASE_WRITE);
	VOP_LOCK(vp);
	VATTR_NULL(&vattr);
	vattr.va_mode = uap->mode & ALLPERMS;
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	VOP_UNLOCK(vp);
	return (error);
}

/*
 * Set ownership given a path name.
 */
struct chown_args {
	char	*path;
	int	uid;
	int	gid;
};
/* ARGSUSED */
int
chown(p, uap, retval)
	struct proc *p;
	register struct chown_args *uap;
	int *retval;
{
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, uap->path, p);
	error = namei(&nd);
	if (error)
		return (error);
	vp = nd.ni_vp;
	LEASE_CHECK(vp, p, p->p_ucred, LEASE_WRITE);
	VOP_LOCK(vp);
	VATTR_NULL(&vattr);
	vattr.va_uid = uap->uid;
	vattr.va_gid = uap->gid;
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	vput(vp);
	return (error);
}

/*
 * Set ownership given a file descriptor.
 */
struct fchown_args {
	int	fd;
	int	uid;
	int	gid;
};
/* ARGSUSED */
int
fchown(p, uap, retval)
	struct proc *p;
	register struct fchown_args *uap;
	int *retval;
{
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	error = getvnode(p->p_fd, uap->fd, &fp);
	if (error)
		return (error);
	vp = (struct vnode *)fp->f_data;
	LEASE_CHECK(vp, p, p->p_ucred, LEASE_WRITE);
	VOP_LOCK(vp);
	VATTR_NULL(&vattr);
	vattr.va_uid = uap->uid;
	vattr.va_gid = uap->gid;
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	VOP_UNLOCK(vp);
	return (error);
}

/*
 * Set the access and modification times of a file.
 */
struct utimes_args {
	char	*path;
	struct	timeval *tptr;
};
/* ARGSUSED */
int
utimes(p, uap, retval)
	struct proc *p;
	register struct utimes_args *uap;
	int *retval;
{
	register struct vnode *vp;
	struct timeval tv[2];
	struct vattr vattr;
	int error;
	struct nameidata nd;

	VATTR_NULL(&vattr);
	if (uap->tptr == NULL) {
		microtime(&tv[0]);
		tv[1] = tv[0];
		vattr.va_vaflags |= VA_UTIMES_NULL;
	} else {
		error = copyin((caddr_t)uap->tptr, (caddr_t)tv, sizeof (tv));
		if (error)
			return (error);
	}
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, uap->path, p);
	error = namei(&nd);
	if (error)
		return (error);
	vp = nd.ni_vp;
	LEASE_CHECK(vp, p, p->p_ucred, LEASE_WRITE);
	VOP_LOCK(vp);
	vattr.va_atime.ts_sec = tv[0].tv_sec;
	vattr.va_atime.ts_nsec = tv[0].tv_usec * 1000;
	vattr.va_mtime.ts_sec = tv[1].tv_sec;
	vattr.va_mtime.ts_nsec = tv[1].tv_usec * 1000;
	error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	vput(vp);
	return (error);
}

/*
 * Truncate a file given its path name.
 */
struct truncate_args {
	char	*path;
	int	pad;
	off_t	length;
};
/* ARGSUSED */
int
truncate(p, uap, retval)
	struct proc *p;
	register struct truncate_args *uap;
	int *retval;
{
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	if (uap->length < 0)
		return(EINVAL);
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, uap->path, p);
	error = namei(&nd);
	if (error)
		return (error);
	vp = nd.ni_vp;
	LEASE_CHECK(vp, p, p->p_ucred, LEASE_WRITE);
	VOP_LOCK(vp);
	if (vp->v_type == VDIR)
		error = EISDIR;
	else if ((error = vn_writechk(vp)) == 0 &&
	    (error = VOP_ACCESS(vp, VWRITE, p->p_ucred, p)) == 0) {
		VATTR_NULL(&vattr);
		vattr.va_size = uap->length;
		error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	}
	vput(vp);
	return (error);
}

/*
 * Truncate a file given a file descriptor.
 */
struct ftruncate_args {
	int	fd;
	int	pad;
	off_t	length;
};
/* ARGSUSED */
int
ftruncate(p, uap, retval)
	struct proc *p;
	register struct ftruncate_args *uap;
	int *retval;
{
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	if (uap->length < 0)
		return(EINVAL);
	error = getvnode(p->p_fd, uap->fd, &fp);
	if (error)
		return (error);
	if ((fp->f_flag & FWRITE) == 0)
		return (EINVAL);
	vp = (struct vnode *)fp->f_data;
	LEASE_CHECK(vp, p, p->p_ucred, LEASE_WRITE);
	VOP_LOCK(vp);
	if (vp->v_type == VDIR)
		error = EISDIR;
	else if ((error = vn_writechk(vp)) == 0) {
		VATTR_NULL(&vattr);
		vattr.va_size = uap->length;
		error = VOP_SETATTR(vp, &vattr, fp->f_cred, p);
	}
	VOP_UNLOCK(vp);
	return (error);
}

#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
/*
 * Truncate a file given its path name.
 */
struct otruncate_args {
	char	*path;
	long	length;
};
/* ARGSUSED */
int
otruncate(p, uap, retval)
	struct proc *p;
	register struct otruncate_args *uap;
	int *retval;
{
	struct truncate_args nuap;

	nuap.path = uap->path;
	nuap.length = uap->length;
	return (truncate(p, &nuap, retval));
}

/*
 * Truncate a file given a file descriptor.
 */
struct oftruncate_args {
	int	fd;
	long	length;
};
/* ARGSUSED */
int
oftruncate(p, uap, retval)
	struct proc *p;
	register struct oftruncate_args *uap;
	int *retval;
{
	struct ftruncate_args nuap;

	nuap.fd = uap->fd;
	nuap.length = uap->length;
	return (ftruncate(p, &nuap, retval));
}
#endif /* COMPAT_43 || COMPAT_SUNOS */

/*
 * Sync an open file.
 */
struct fsync_args {
	int	fd;
};
/* ARGSUSED */
int
fsync(p, uap, retval)
	struct proc *p;
	struct fsync_args *uap;
	int *retval;
{
	register struct vnode *vp;
	struct file *fp;
	int error;

	error = getvnode(p->p_fd, uap->fd, &fp);
	if (error)
		return (error);
	vp = (struct vnode *)fp->f_data;
	VOP_LOCK(vp);
	if (vp->v_vmdata) {
		_vm_object_page_clean((vm_object_t) vp->v_vmdata, 0, 0 ,0);
	}
	error = VOP_FSYNC(vp, fp->f_cred, MNT_WAIT, p);
	VOP_UNLOCK(vp);
	return (error);
}

/*
 * Rename files.  Source and destination must either both be directories,
 * or both not be directories.  If target is a directory, it must be empty.
 */
struct rename_args {
	char	*from;
	char	*to;
};
/* ARGSUSED */
int
rename(p, uap, retval)
	struct proc *p;
	register struct rename_args *uap;
	int *retval;
{
	register struct vnode *tvp, *fvp, *tdvp;
	struct nameidata fromnd, tond;
	int error;

	NDINIT(&fromnd, DELETE, WANTPARENT | SAVESTART, UIO_USERSPACE,
		uap->from, p);
	error = namei(&fromnd);
	if (error)
		return (error);
	fvp = fromnd.ni_vp;
	NDINIT(&tond, RENAME, LOCKPARENT | LOCKLEAF | NOCACHE | SAVESTART,
		UIO_USERSPACE, uap->to, p);
	if (fromnd.ni_vp->v_type == VDIR)
		tond.ni_cnd.cn_flags |= WILLBEDIR;
	error = namei(&tond);
	if (error) {
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
		LEASE_CHECK(tdvp, p, p->p_ucred, LEASE_WRITE);
		if (fromnd.ni_dvp != tdvp) {
			LEASE_CHECK(fromnd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
		}
		if (tvp) {
			LEASE_CHECK(tvp, p, p->p_ucred, LEASE_WRITE);
			(void) vnode_pager_uncache(tvp);
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
	FREE(tond.ni_cnd.cn_pnbuf, M_NAMEI);
out1:
	if (fromnd.ni_startdir)
		vrele(fromnd.ni_startdir);
	FREE(fromnd.ni_cnd.cn_pnbuf, M_NAMEI);
	if (error == -1)
		return (0);
	return (error);
}

/*
 * Make a directory file.
 */
struct mkdir_args {
	char	*path;
	int	mode;
};
/* ARGSUSED */
int
mkdir(p, uap, retval)
	struct proc *p;
	register struct mkdir_args *uap;
	int *retval;
{
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, CREATE, LOCKPARENT, UIO_USERSPACE, uap->path, p);
	nd.ni_cnd.cn_flags |= WILLBEDIR;
	error = namei(&nd);
	if (error)
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
	vattr.va_mode = (uap->mode & ACCESSPERMS) &~ p->p_fd->fd_cmask;
	LEASE_CHECK(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
	error = VOP_MKDIR(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr);
	if (!error)
		vput(nd.ni_vp);
	return (error);
}

/*
 * Remove a directory file.
 */
struct rmdir_args {
	char	*path;
};
/* ARGSUSED */
int
rmdir(p, uap, retval)
	struct proc *p;
	struct rmdir_args *uap;
	int *retval;
{
	register struct vnode *vp;
	int error;
	struct nameidata nd;

	NDINIT(&nd, DELETE, LOCKPARENT | LOCKLEAF, UIO_USERSPACE, uap->path, p);
	error = namei(&nd);
	if (error)
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
		LEASE_CHECK(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
		LEASE_CHECK(vp, p, p->p_ucred, LEASE_WRITE);
		error = VOP_RMDIR(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
	} else {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vput(vp);
	}
	return (error);
}

#ifdef COMPAT_43
/*
 * Read a block of directory entries in a file system independent format.
 */
struct ogetdirentries_args {
	int	fd;
	char	*buf;
	u_int	count;
	long	*basep;
};
int
ogetdirentries(p, uap, retval)
	struct proc *p;
	register struct ogetdirentries_args *uap;
	int *retval;
{
	register struct vnode *vp;
	struct file *fp;
	struct uio auio, kuio;
	struct iovec aiov, kiov;
	struct dirent *dp, *edp;
	caddr_t dirbuf;
	int error, readcnt;
	long loff;

	error = getvnode(p->p_fd, uap->fd, &fp);
	if (error)
		return (error);
	if ((fp->f_flag & FREAD) == 0)
		return (EBADF);
	vp = (struct vnode *)fp->f_data;
	if (vp->v_type != VDIR)
		return (EINVAL);
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;
	auio.uio_resid = uap->count;
	VOP_LOCK(vp);
	loff = auio.uio_offset = fp->f_offset;
#	if (BYTE_ORDER != LITTLE_ENDIAN)
		if (vp->v_mount->mnt_maxsymlinklen <= 0) {
			error = VOP_READDIR(vp, &auio, fp->f_cred, NULL, NULL, NULL);
			fp->f_offset = auio.uio_offset;
		} else
#	endif
	{
		kuio = auio;
		kuio.uio_iov = &kiov;
		kuio.uio_segflg = UIO_SYSSPACE;
		kiov.iov_len = uap->count;
		MALLOC(dirbuf, caddr_t, uap->count, M_TEMP, M_WAITOK);
		kiov.iov_base = dirbuf;
		error = VOP_READDIR(vp, &kuio, fp->f_cred, NULL, NULL, NULL);
		fp->f_offset = kuio.uio_offset;
		if (error == 0) {
			readcnt = uap->count - kuio.uio_resid;
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
	VOP_UNLOCK(vp);
	if (error)
		return (error);
	error = copyout((caddr_t)&loff, (caddr_t)uap->basep, sizeof(long));
	*retval = uap->count - auio.uio_resid;
	return (error);
}
#endif

/*
 * Read a block of directory entries in a file system independent format.
 */
struct getdirentries_args {
	int	fd;
	char	*buf;
	u_int	count;
	long	*basep;
};
int
getdirentries(p, uap, retval)
	struct proc *p;
	register struct getdirentries_args *uap;
	int *retval;
{
	register struct vnode *vp;
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	long loff;
	int error;

	error = getvnode(p->p_fd, uap->fd, &fp);
	if (error)
		return (error);
	if ((fp->f_flag & FREAD) == 0)
		return (EBADF);
	vp = (struct vnode *)fp->f_data;
unionread:
	if (vp->v_type != VDIR)
		return (EINVAL);
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;
	auio.uio_resid = uap->count;
	VOP_LOCK(vp);
	loff = auio.uio_offset = fp->f_offset;
	error = VOP_READDIR(vp, &auio, fp->f_cred, NULL, NULL, NULL);
	fp->f_offset = auio.uio_offset;
	VOP_UNLOCK(vp);
	if (error)
		return (error);

#ifdef UNION
{
	if ((uap->count == auio.uio_resid) &&
	    (vp->v_op == union_vnodeop_p)) {
		struct vnode *tvp = vp;

		vp = union_lowervp(vp);
		if (vp != NULLVP) {
			VOP_LOCK(vp);
			error = VOP_OPEN(vp, FREAD, fp->f_cred, p);
			VOP_UNLOCK(vp);

			if (error) {
				vrele(vp);
				return (error);
			}
			fp->f_data = (caddr_t) vp;
			fp->f_offset = 0;
			error = vn_close(tvp, FREAD, fp->f_cred, p);
			if (error)
				return (error);
			goto unionread;
		}
	}
}
#endif

	if ((uap->count == auio.uio_resid) &&
	    vp &&
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
	error = copyout((caddr_t)&loff, (caddr_t)uap->basep, sizeof(long));
	*retval = uap->count - auio.uio_resid;
	return (error);
}

/*
 * Set the mode mask for creation of filesystem nodes.
 */
struct umask_args {
	int	newmask;
};
mode_t				/* XXX */
umask(p, uap, retval)
	struct proc *p;
	struct umask_args *uap;
	int *retval;
{
	register struct filedesc *fdp;

	fdp = p->p_fd;
	*retval = fdp->fd_cmask;
	fdp->fd_cmask = uap->newmask & ALLPERMS;
	return (0);
}

/*
 * Void all references to file by ripping underlying filesystem
 * away from vnode.
 */
struct revoke_args {
	char	*path;
};
/* ARGSUSED */
int
revoke(p, uap, retval)
	struct proc *p;
	register struct revoke_args *uap;
	int *retval;
{
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, uap->path, p);
	error = namei(&nd);
	if (error)
		return (error);
	vp = nd.ni_vp;
	if (vp->v_type != VCHR && vp->v_type != VBLK) {
		error = EINVAL;
		goto out;
	}
	error = VOP_GETATTR(vp, &vattr, p->p_ucred, p);
	if (error)
		goto out;
	if (p->p_ucred->cr_uid != vattr.va_uid &&
	    (error = suser(p->p_ucred, &p->p_acflag)))
		goto out;
	if (vp->v_usecount > 1 || (vp->v_flag & VALIASED))
		vgoneall(vp);
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
	struct file **fpp;
	int fd;
{
	struct file *fp;

	if ((u_int)fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_VNODE)
		return (EINVAL);
	*fpp = fp;
	return (0);
}
