/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/sysent.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/sysproto.h>
#include <sys/namei.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/limits.h>
#include <sys/linker.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/dirent.h>
#include <sys/extattr.h>
#include <sys/jail.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>

#include <machine/stdarg.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/uma.h>

static int chroot_refuse_vdir_fds(struct filedesc *fdp);
static int getutimes(const struct timeval *, enum uio_seg, struct timespec *);
static int setfown(struct thread *td, struct vnode *, uid_t, gid_t);
static int setfmode(struct thread *td, struct vnode *, int);
static int setfflags(struct thread *td, struct vnode *, int);
static int setutimes(struct thread *td, struct vnode *,
    const struct timespec *, int, int);
static int vn_access(struct vnode *vp, int user_flags, struct ucred *cred,
    struct thread *td);

static int extattr_list_vp(struct vnode *vp, int attrnamespace, void *data,
    size_t nbytes, struct thread *td);

int (*union_dircheckp)(struct thread *td, struct vnode **, struct file *);
int (*softdep_fsync_hook)(struct vnode *);

/*
 * The module initialization routine for POSIX asynchronous I/O will
 * set this to the version of AIO that it implements.  (Zero means
 * that it is not implemented.)  This value is used here by pathconf()
 * and in kern_descrip.c by fpathconf().
 */
int async_io_version;

/*
 * Sync each mounted filesystem.
 */
#ifndef _SYS_SYSPROTO_H_
struct sync_args {
	int     dummy;
};
#endif

#ifdef DEBUG
static int syncprt = 0;
SYSCTL_INT(_debug, OID_AUTO, syncprt, CTLFLAG_RW, &syncprt, 0, "");
#endif

/* ARGSUSED */
int
sync(td, uap)
	struct thread *td;
	struct sync_args *uap;
{
	struct mount *mp, *nmp;
	int asyncflag;

	mtx_lock(&mountlist_mtx);
	for (mp = TAILQ_FIRST(&mountlist); mp != NULL; mp = nmp) {
		if (vfs_busy(mp, LK_NOWAIT, &mountlist_mtx, td)) {
			nmp = TAILQ_NEXT(mp, mnt_list);
			continue;
		}
		if ((mp->mnt_flag & MNT_RDONLY) == 0 &&
		    vn_start_write(NULL, &mp, V_NOWAIT) == 0) {
			asyncflag = mp->mnt_flag & MNT_ASYNC;
			mp->mnt_flag &= ~MNT_ASYNC;
			vfs_msync(mp, MNT_NOWAIT);
			VFS_SYNC(mp, MNT_NOWAIT,
			    ((td != NULL) ? td->td_ucred : NOCRED), td);
			mp->mnt_flag |= asyncflag;
			vn_finished_write(mp);
		}
		mtx_lock(&mountlist_mtx);
		nmp = TAILQ_NEXT(mp, mnt_list);
		vfs_unbusy(mp, td);
	}
	mtx_unlock(&mountlist_mtx);
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

/* XXX PRISON: could be per prison flag */
static int prison_quotas;
#if 0
SYSCTL_INT(_kern_prison, OID_AUTO, quotas, CTLFLAG_RW, &prison_quotas, 0, "");
#endif

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
int
quotactl(td, uap)
	struct thread *td;
	register struct quotactl_args /* {
		char *path;
		int cmd;
		int uid;
		caddr_t arg;
	} */ *uap;
{
	struct mount *mp, *vmp;
	int error;
	struct nameidata nd;

	if (jailed(td->td_ucred) && !prison_quotas)
		return (EPERM);
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, uap->path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	error = vn_start_write(nd.ni_vp, &vmp, V_WAIT | PCATCH);
	mp = nd.ni_vp->v_mount;
	vrele(nd.ni_vp);
	if (error)
		return (error);
	error = VFS_QUOTACTL(mp, uap->cmd, uap->uid, uap->arg, td);
	vn_finished_write(vmp);
	return (error);
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
int
statfs(td, uap)
	struct thread *td;
	register struct statfs_args /* {
		char *path;
		struct statfs *buf;
	} */ *uap;
{
	struct statfs sf;
	int error;

	error = kern_statfs(td, uap->path, UIO_USERSPACE, &sf);
	if (error == 0)
		error = copyout(&sf, uap->buf, sizeof(sf));
	return (error);
}

int
kern_statfs(struct thread *td, char *path, enum uio_seg pathseg,
    struct statfs *buf)
{
	struct mount *mp;
	struct statfs *sp, sb;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, pathseg, path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	mp = nd.ni_vp->v_mount;
	sp = &mp->mnt_stat;
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vrele(nd.ni_vp);
#ifdef MAC
	error = mac_check_mount_stat(td->td_ucred, mp);
	if (error)
		return (error);
#endif
	/*
	 * Set these in case the underlying filesystem fails to do so.
	 */
	sp->f_version = STATFS_VERSION;
	sp->f_namemax = NAME_MAX;
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	error = VFS_STATFS(mp, sp, td);
	if (error)
		return (error);
	if (suser(td)) {
		bcopy(sp, &sb, sizeof(sb));
		sb.f_fsid.val[0] = sb.f_fsid.val[1] = 0;
		sp = &sb;
	}
	*buf = *sp;
	return (0);
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
int
fstatfs(td, uap)
	struct thread *td;
	register struct fstatfs_args /* {
		int fd;
		struct statfs *buf;
	} */ *uap;
{
	struct statfs sf;
	int error;

	error = kern_fstatfs(td, uap->fd, &sf);
	if (error == 0)
		error = copyout(&sf, uap->buf, sizeof(sf));
	return (error);
}

int
kern_fstatfs(struct thread *td, int fd, struct statfs *buf)
{
	struct file *fp;
	struct mount *mp;
	struct statfs *sp, sb;
	int error;

	if ((error = getvnode(td->td_proc->p_fd, fd, &fp)) != 0)
		return (error);
	mp = fp->f_vnode->v_mount;
	fdrop(fp, td);
	if (mp == NULL)
		return (EBADF);
#ifdef MAC
	error = mac_check_mount_stat(td->td_ucred, mp);
	if (error)
		return (error);
#endif
	sp = &mp->mnt_stat;
	/*
	 * Set these in case the underlying filesystem fails to do so.
	 */
	sp->f_version = STATFS_VERSION;
	sp->f_namemax = NAME_MAX;
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	error = VFS_STATFS(mp, sp, td);
	if (error)
		return (error);
	if (suser(td)) {
		bcopy(sp, &sb, sizeof(sb));
		sb.f_fsid.val[0] = sb.f_fsid.val[1] = 0;
		sp = &sb;
	}
	*buf = *sp;
	return (0);
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
getfsstat(td, uap)
	struct thread *td;
	register struct getfsstat_args /* {
		struct statfs *buf;
		long bufsize;
		int flags;
	} */ *uap;
{
	struct mount *mp, *nmp;
	struct statfs *sp, sb;
	caddr_t sfsp;
	long count, maxcount, error;

	maxcount = uap->bufsize / sizeof(struct statfs);
	sfsp = (caddr_t)uap->buf;
	count = 0;
	mtx_lock(&mountlist_mtx);
	for (mp = TAILQ_FIRST(&mountlist); mp != NULL; mp = nmp) {
		if (!prison_check_mount(td->td_ucred, mp)) {
			nmp = TAILQ_NEXT(mp, mnt_list);
			continue;
		}
#ifdef MAC
		if (mac_check_mount_stat(td->td_ucred, mp) != 0) {
			nmp = TAILQ_NEXT(mp, mnt_list);
			continue;
		}
#endif
		if (vfs_busy(mp, LK_NOWAIT, &mountlist_mtx, td)) {
			nmp = TAILQ_NEXT(mp, mnt_list);
			continue;
		}
		if (sfsp && count < maxcount) {
			sp = &mp->mnt_stat;
			/*
			 * Set these in case the underlying filesystem
			 * fails to do so.
			 */
			sp->f_version = STATFS_VERSION;
			sp->f_namemax = NAME_MAX;
			sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
			/*
			 * If MNT_NOWAIT or MNT_LAZY is specified, do not
			 * refresh the fsstat cache. MNT_NOWAIT or MNT_LAZY
			 * overrides MNT_WAIT.
			 */
			if (((uap->flags & (MNT_LAZY|MNT_NOWAIT)) == 0 ||
			    (uap->flags & MNT_WAIT)) &&
			    (error = VFS_STATFS(mp, sp, td))) {
				mtx_lock(&mountlist_mtx);
				nmp = TAILQ_NEXT(mp, mnt_list);
				vfs_unbusy(mp, td);
				continue;
			}
			if (suser(td)) {
				bcopy(sp, &sb, sizeof(sb));
				sb.f_fsid.val[0] = sb.f_fsid.val[1] = 0;
				sp = &sb;
			}
			error = copyout(sp, sfsp, sizeof(*sp));
			if (error) {
				vfs_unbusy(mp, td);
				return (error);
			}
			sfsp += sizeof(*sp);
		}
		count++;
		mtx_lock(&mountlist_mtx);
		nmp = TAILQ_NEXT(mp, mnt_list);
		vfs_unbusy(mp, td);
	}
	mtx_unlock(&mountlist_mtx);
	if (sfsp && count > maxcount)
		td->td_retval[0] = maxcount;
	else
		td->td_retval[0] = count;
	return (0);
}

#ifdef COMPAT_FREEBSD4
/*
 * Get old format filesystem statistics.
 */
static void cvtstatfs(struct thread *, struct statfs *, struct ostatfs *);

#ifndef _SYS_SYSPROTO_H_
struct freebsd4_statfs_args {
	char *path;
	struct ostatfs *buf;
};
#endif
int
freebsd4_statfs(td, uap)
	struct thread *td;
	struct freebsd4_statfs_args /* {
		char *path;
		struct ostatfs *buf;
	} */ *uap;
{
	struct ostatfs osb;
	struct statfs sf;
	int error;

	error = kern_statfs(td, uap->path, UIO_USERSPACE, &sf);
	if (error)
		return (error);
	cvtstatfs(td, &sf, &osb);
	return (copyout(&osb, uap->buf, sizeof(osb)));
}

/*
 * Get filesystem statistics.
 */
#ifndef _SYS_SYSPROTO_H_
struct freebsd4_fstatfs_args {
	int fd;
	struct ostatfs *buf;
};
#endif
int
freebsd4_fstatfs(td, uap)
	struct thread *td;
	struct freebsd4_fstatfs_args /* {
		int fd;
		struct ostatfs *buf;
	} */ *uap;
{
	struct ostatfs osb;
	struct statfs sf;
	int error;

	error = kern_fstatfs(td, uap->fd, &sf);
	if (error)
		return (error);
	cvtstatfs(td, &sf, &osb);
	return (copyout(&osb, uap->buf, sizeof(osb)));
}

/*
 * Get statistics on all filesystems.
 */
#ifndef _SYS_SYSPROTO_H_
struct freebsd4_getfsstat_args {
	struct ostatfs *buf;
	long bufsize;
	int flags;
};
#endif
int
freebsd4_getfsstat(td, uap)
	struct thread *td;
	register struct freebsd4_getfsstat_args /* {
		struct ostatfs *buf;
		long bufsize;
		int flags;
	} */ *uap;
{
	struct mount *mp, *nmp;
	struct statfs *sp;
	struct ostatfs osb;
	caddr_t sfsp;
	long count, maxcount, error;

	maxcount = uap->bufsize / sizeof(struct ostatfs);
	sfsp = (caddr_t)uap->buf;
	count = 0;
	mtx_lock(&mountlist_mtx);
	for (mp = TAILQ_FIRST(&mountlist); mp != NULL; mp = nmp) {
		if (!prison_check_mount(td->td_ucred, mp)) {
			nmp = TAILQ_NEXT(mp, mnt_list);
			continue;
		}
#ifdef MAC
		if (mac_check_mount_stat(td->td_ucred, mp) != 0) {
			nmp = TAILQ_NEXT(mp, mnt_list);
			continue;
		}
#endif
		if (vfs_busy(mp, LK_NOWAIT, &mountlist_mtx, td)) {
			nmp = TAILQ_NEXT(mp, mnt_list);
			continue;
		}
		if (sfsp && count < maxcount) {
			sp = &mp->mnt_stat;
			/*
			 * If MNT_NOWAIT or MNT_LAZY is specified, do not
			 * refresh the fsstat cache. MNT_NOWAIT or MNT_LAZY
			 * overrides MNT_WAIT.
			 */
			if (((uap->flags & (MNT_LAZY|MNT_NOWAIT)) == 0 ||
			    (uap->flags & MNT_WAIT)) &&
			    (error = VFS_STATFS(mp, sp, td))) {
				mtx_lock(&mountlist_mtx);
				nmp = TAILQ_NEXT(mp, mnt_list);
				vfs_unbusy(mp, td);
				continue;
			}
			sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
			cvtstatfs(td, sp, &osb);
			error = copyout(&osb, sfsp, sizeof(osb));
			if (error) {
				vfs_unbusy(mp, td);
				return (error);
			}
			sfsp += sizeof(osb);
		}
		count++;
		mtx_lock(&mountlist_mtx);
		nmp = TAILQ_NEXT(mp, mnt_list);
		vfs_unbusy(mp, td);
	}
	mtx_unlock(&mountlist_mtx);
	if (sfsp && count > maxcount)
		td->td_retval[0] = maxcount;
	else
		td->td_retval[0] = count;
	return (0);
}

/*
 * Implement fstatfs() for (NFS) file handles.
 */
#ifndef _SYS_SYSPROTO_H_
struct freebsd4_fhstatfs_args {
	struct fhandle *u_fhp;
	struct ostatfs *buf;
};
#endif
int
freebsd4_fhstatfs(td, uap)
	struct thread *td;
	struct freebsd4_fhstatfs_args /* {
		struct fhandle *u_fhp;
		struct ostatfs *buf;
	} */ *uap;
{
	struct ostatfs osb;
	struct statfs sf;
	fhandle_t fh;
	int error;

	if ((error = copyin(uap->u_fhp, &fh, sizeof(fhandle_t))) != 0)
		return (error);
	error = kern_fhstatfs(td, fh, &sf);
	if (error)
		return (error);
	cvtstatfs(td, &sf, &osb);
	return (copyout(&osb, uap->buf, sizeof(osb)));
}

/*
 * Convert a new format statfs structure to an old format statfs structure.
 */
static void
cvtstatfs(td, nsp, osp)
	struct thread *td;
	struct statfs *nsp;
	struct ostatfs *osp;
{

	bzero(osp, sizeof(*osp));
	osp->f_bsize = MIN(nsp->f_bsize, LONG_MAX);
	osp->f_iosize = MIN(nsp->f_iosize, LONG_MAX);
	osp->f_blocks = MIN(nsp->f_blocks, LONG_MAX);
	osp->f_bfree = MIN(nsp->f_bfree, LONG_MAX);
	osp->f_bavail = MIN(nsp->f_bavail, LONG_MAX);
	osp->f_files = MIN(nsp->f_files, LONG_MAX);
	osp->f_ffree = MIN(nsp->f_ffree, LONG_MAX);
	osp->f_owner = nsp->f_owner;
	osp->f_type = nsp->f_type;
	osp->f_flags = nsp->f_flags;
	osp->f_syncwrites = MIN(nsp->f_syncwrites, LONG_MAX);
	osp->f_asyncwrites = MIN(nsp->f_asyncwrites, LONG_MAX);
	osp->f_syncreads = MIN(nsp->f_syncreads, LONG_MAX);
	osp->f_asyncreads = MIN(nsp->f_asyncreads, LONG_MAX);
	bcopy(nsp->f_fstypename, osp->f_fstypename,
	    MIN(MFSNAMELEN, OMNAMELEN));
	bcopy(nsp->f_mntonname, osp->f_mntonname,
	    MIN(MFSNAMELEN, OMNAMELEN));
	bcopy(nsp->f_mntfromname, osp->f_mntfromname,
	    MIN(MFSNAMELEN, OMNAMELEN));
	if (suser(td)) {
		osp->f_fsid.val[0] = osp->f_fsid.val[1] = 0;
	} else {
		osp->f_fsid = nsp->f_fsid;
	}
}
#endif /* COMPAT_FREEBSD4 */

/*
 * Change current working directory to a given file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct fchdir_args {
	int	fd;
};
#endif
int
fchdir(td, uap)
	struct thread *td;
	struct fchdir_args /* {
		int fd;
	} */ *uap;
{
	register struct filedesc *fdp = td->td_proc->p_fd;
	struct vnode *vp, *tdp, *vpold;
	struct mount *mp;
	struct file *fp;
	int error;

	if ((error = getvnode(fdp, uap->fd, &fp)) != 0)
		return (error);
	vp = fp->f_vnode;
	VREF(vp);
	fdrop(fp, td);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	if (vp->v_type != VDIR)
		error = ENOTDIR;
#ifdef MAC
	else if ((error = mac_check_vnode_chdir(td->td_ucred, vp)) != 0) {
	}
#endif
	else
		error = VOP_ACCESS(vp, VEXEC, td->td_ucred, td);
	while (!error && (mp = vp->v_mountedhere) != NULL) {
		if (vfs_busy(mp, 0, 0, td))
			continue;
		error = VFS_ROOT(mp, &tdp, td);
		vfs_unbusy(mp, td);
		if (error)
			break;
		vput(vp);
		vp = tdp;
	}
	if (error) {
		vput(vp);
		return (error);
	}
	VOP_UNLOCK(vp, 0, td);
	FILEDESC_LOCK_FAST(fdp);
	vpold = fdp->fd_cdir;
	fdp->fd_cdir = vp;
	FILEDESC_UNLOCK_FAST(fdp);
	vrele(vpold);
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
int
chdir(td, uap)
	struct thread *td;
	struct chdir_args /* {
		char *path;
	} */ *uap;
{

	return (kern_chdir(td, uap->path, UIO_USERSPACE));
}

int
kern_chdir(struct thread *td, char *path, enum uio_seg pathseg)
{
	register struct filedesc *fdp = td->td_proc->p_fd;
	int error;
	struct nameidata nd;
	struct vnode *vp;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, pathseg, path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	if ((error = change_dir(nd.ni_vp, td)) != 0) {
		vput(nd.ni_vp);
		NDFREE(&nd, NDF_ONLY_PNBUF);
		return (error);
	}
	VOP_UNLOCK(nd.ni_vp, 0, td);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	FILEDESC_LOCK_FAST(fdp);
	vp = fdp->fd_cdir;
	fdp->fd_cdir = nd.ni_vp;
	FILEDESC_UNLOCK_FAST(fdp);
	vrele(vp);
	return (0);
}

/*
 * Helper function for raised chroot(2) security function:  Refuse if
 * any filedescriptors are open directories.
 */
static int
chroot_refuse_vdir_fds(fdp)
	struct filedesc *fdp;
{
	struct vnode *vp;
	struct file *fp;
	int fd;

	FILEDESC_LOCK_ASSERT(fdp, MA_OWNED);
	for (fd = 0; fd < fdp->fd_nfiles ; fd++) {
		fp = fget_locked(fdp, fd);
		if (fp == NULL)
			continue;
		if (fp->f_type == DTYPE_VNODE) {
			vp = fp->f_vnode;
			if (vp->v_type == VDIR)
				return (EPERM);
		}
	}
	return (0);
}

/*
 * This sysctl determines if we will allow a process to chroot(2) if it
 * has a directory open:
 *	0: disallowed for all processes.
 *	1: allowed for processes that were not already chroot(2)'ed.
 *	2: allowed for all processes.
 */

static int chroot_allow_open_directories = 1;

SYSCTL_INT(_kern, OID_AUTO, chroot_allow_open_directories, CTLFLAG_RW,
     &chroot_allow_open_directories, 0, "");

/*
 * Change notion of root (``/'') directory.
 */
#ifndef _SYS_SYSPROTO_H_
struct chroot_args {
	char	*path;
};
#endif
int
chroot(td, uap)
	struct thread *td;
	struct chroot_args /* {
		char *path;
	} */ *uap;
{
	int error;
	struct nameidata nd;

	error = suser_cred(td->td_ucred, SUSER_ALLOWJAIL);
	if (error)
		return (error);
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE, uap->path, td);
	mtx_lock(&Giant);
	error = namei(&nd);
	if (error)
		goto error;
	if ((error = change_dir(nd.ni_vp, td)) != 0)
		goto e_vunlock;
#ifdef MAC
	if ((error = mac_check_vnode_chroot(td->td_ucred, nd.ni_vp)))
		goto e_vunlock;
#endif
	VOP_UNLOCK(nd.ni_vp, 0, td);
	error = change_root(nd.ni_vp, td);
	vrele(nd.ni_vp);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	mtx_unlock(&Giant);
	return (error);
e_vunlock:
	vput(nd.ni_vp);
error:
	mtx_unlock(&Giant);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	return (error);
}

/*
 * Common routine for chroot and chdir.  Callers must provide a locked vnode
 * instance.
 */
int
change_dir(vp, td)
	struct vnode *vp;
	struct thread *td;
{
	int error;

	ASSERT_VOP_LOCKED(vp, "change_dir(): vp not locked");
	if (vp->v_type != VDIR)
		return (ENOTDIR);
#ifdef MAC
	error = mac_check_vnode_chdir(td->td_ucred, vp);
	if (error)
		return (error);
#endif
	error = VOP_ACCESS(vp, VEXEC, td->td_ucred, td);
	return (error);
}

/*
 * Common routine for kern_chroot() and jail_attach().  The caller is
 * responsible for invoking suser() and mac_check_chroot() to authorize this
 * operation.
 */
int
change_root(vp, td)
	struct vnode *vp;
	struct thread *td;
{
	struct filedesc *fdp;
	struct vnode *oldvp;
	int error;

	mtx_assert(&Giant, MA_OWNED);
	fdp = td->td_proc->p_fd;
	FILEDESC_LOCK(fdp);
	if (chroot_allow_open_directories == 0 ||
	    (chroot_allow_open_directories == 1 && fdp->fd_rdir != rootvnode)) {
		error = chroot_refuse_vdir_fds(fdp);
		if (error) {
			FILEDESC_UNLOCK(fdp);
			return (error);
		}
	}
	oldvp = fdp->fd_rdir;
	fdp->fd_rdir = vp;
	VREF(fdp->fd_rdir);
	if (!fdp->fd_jdir) {
		fdp->fd_jdir = vp;
		VREF(fdp->fd_jdir);
	}
	FILEDESC_UNLOCK(fdp);
	vrele(oldvp);
	return (0);
}

/*
 * Check permissions, allocate an open file structure,
 * and call the device open routine if any.
 *
 * MP SAFE
 */
#ifndef _SYS_SYSPROTO_H_
struct open_args {
	char	*path;
	int	flags;
	int	mode;
};
#endif
int
open(td, uap)
	struct thread *td;
	register struct open_args /* {
		char *path;
		int flags;
		int mode;
	} */ *uap;
{

	return (kern_open(td, uap->path, UIO_USERSPACE, uap->flags, uap->mode));
}

int
kern_open(struct thread *td, char *path, enum uio_seg pathseg, int flags,
    int mode)
{
	struct proc *p = td->td_proc;
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct vnode *vp;
	struct vattr vat;
	struct mount *mp;
	int cmode;
	struct file *nfp;
	int type, indx, error;
	struct flock lf;
	struct nameidata nd;

	if ((flags & O_ACCMODE) == O_ACCMODE)
		return (EINVAL);
	flags = FFLAGS(flags);
	error = falloc(td, &nfp, &indx);
	if (error)
		return (error);
	/* An extra reference on `nfp' has been held for us by falloc(). */
	fp = nfp;
	cmode = ((mode &~ fdp->fd_cmask) & ALLPERMS) &~ S_ISTXT;
	NDINIT(&nd, LOOKUP, FOLLOW, pathseg, path, td);
	td->td_dupfd = -1;		/* XXX check for fdopen */
	mtx_lock(&Giant);
	error = vn_open(&nd, &flags, cmode, indx);
	if (error) {
		mtx_unlock(&Giant);

		/*
		 * If the vn_open replaced the method vector, something
		 * wonderous happened deep below and we just pass it up
		 * pretending we know what we do.
		 */
		if (error == ENXIO && fp->f_ops != &badfileops) {
			fdrop(fp, td);
			td->td_retval[0] = indx;
			return (0);
		}

		/*
		 * release our own reference
		 */
		fdrop(fp, td);

		/*
		 * handle special fdopen() case.  bleh.  dupfdopen() is
		 * responsible for dropping the old contents of ofiles[indx]
		 * if it succeeds.
		 */
		if ((error == ENODEV || error == ENXIO) &&
		    td->td_dupfd >= 0 &&		/* XXX from fdopen */
		    (error =
			dupfdopen(td, fdp, indx, td->td_dupfd, flags, error)) == 0) {
			td->td_retval[0] = indx;
			return (0);
		}
		/*
		 * Clean up the descriptor, but only if another thread hadn't
		 * replaced or closed it.
		 */
		fdclose(fdp, fp, indx, td);

		if (error == ERESTART)
			error = EINTR;
		return (error);
	}
	td->td_dupfd = 0;
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;

	/*
	 * There should be 2 references on the file, one from the descriptor
	 * table, and one for us.
	 *
	 * Handle the case where someone closed the file (via its file
	 * descriptor) while we were blocked.  The end result should look
	 * like opening the file succeeded but it was immediately closed.
	 * We call vn_close() manually because we haven't yet hooked up
	 * the various 'struct file' fields.
	 */
	FILEDESC_LOCK(fdp);
	FILE_LOCK(fp);
	if (fp->f_count == 1) {
		KASSERT(fdp->fd_ofiles[indx] != fp,
		    ("Open file descriptor lost all refs"));
		FILE_UNLOCK(fp);
		FILEDESC_UNLOCK(fdp);
		VOP_UNLOCK(vp, 0, td);
		vn_close(vp, flags & FMASK, fp->f_cred, td);
		mtx_unlock(&Giant);
		fdrop(fp, td);
		td->td_retval[0] = indx;
		return (0);
	}
	fp->f_vnode = vp;
	if (fp->f_data == NULL)
		fp->f_data = vp;
	fp->f_flag = flags & FMASK;
	if (fp->f_ops == &badfileops)
		fp->f_ops = &vnops;
	fp->f_seqcount = 1;
	fp->f_type = (vp->v_type == VFIFO ? DTYPE_FIFO : DTYPE_VNODE);
	FILE_UNLOCK(fp);
	FILEDESC_UNLOCK(fdp);

	/* assert that vn_open created a backing object if one is needed */
	KASSERT(!vn_canvmio(vp) || VOP_GETVOBJECT(vp, NULL) == 0,
		("open: vmio vnode has no backing object after vn_open"));

	VOP_UNLOCK(vp, 0, td);
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
		if ((error = VOP_ADVLOCK(vp, (caddr_t)fp, F_SETLK, &lf,
			    type)) != 0)
			goto bad;
		fp->f_flag |= FHASLOCK;
	}
	if (flags & O_TRUNC) {
		if ((error = vn_start_write(vp, &mp, V_WAIT | PCATCH)) != 0)
			goto bad;
		VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
		VATTR_NULL(&vat);
		vat.va_size = 0;
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
#ifdef MAC
		error = mac_check_vnode_write(td->td_ucred, fp->f_cred, vp);
		if (error == 0)
#endif
			error = VOP_SETATTR(vp, &vat, td->td_ucred, td);
		VOP_UNLOCK(vp, 0, td);
		vn_finished_write(mp);
		if (error)
			goto bad;
	}
	mtx_unlock(&Giant);
	/*
	 * Release our private reference, leaving the one associated with
	 * the descriptor table intact.
	 */
	fdrop(fp, td);
	td->td_retval[0] = indx;
	return (0);
bad:
	mtx_unlock(&Giant);
	fdclose(fdp, fp, indx, td);
	fdrop(fp, td);
	return (error);
}

#ifdef COMPAT_43
/*
 * Create a file.
 *
 * MP SAFE
 */
#ifndef _SYS_SYSPROTO_H_
struct ocreat_args {
	char	*path;
	int	mode;
};
#endif
int
ocreat(td, uap)
	struct thread *td;
	register struct ocreat_args /* {
		char *path;
		int mode;
	} */ *uap;
{

	return (kern_open(td, uap->path, UIO_USERSPACE,
	    O_WRONLY | O_CREAT | O_TRUNC, uap->mode));
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
int
mknod(td, uap)
	struct thread *td;
	register struct mknod_args /* {
		char *path;
		int mode;
		int dev;
	} */ *uap;
{

	return (kern_mknod(td, uap->path, UIO_USERSPACE, uap->mode, uap->dev));
}

int
kern_mknod(struct thread *td, char *path, enum uio_seg pathseg, int mode,
    int dev)
{
	struct vnode *vp;
	struct mount *mp;
	struct vattr vattr;
	int error;
	int whiteout = 0;
	struct nameidata nd;

	switch (mode & S_IFMT) {
	case S_IFCHR:
	case S_IFBLK:
		error = suser(td);
		break;
	default:
		error = suser_cred(td->td_ucred, SUSER_ALLOWJAIL);
		break;
	}
	if (error)
		return (error);
restart:
	bwillwrite();
	NDINIT(&nd, CREATE, LOCKPARENT | SAVENAME, pathseg, path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	if (vp != NULL) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		vrele(vp);
		if (vp == nd.ni_dvp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		return (EEXIST);
	} else {
		VATTR_NULL(&vattr);
		FILEDESC_LOCK_FAST(td->td_proc->p_fd);
		vattr.va_mode = (mode & ALLPERMS) &
		    ~td->td_proc->p_fd->fd_cmask;
		FILEDESC_UNLOCK_FAST(td->td_proc->p_fd);
		vattr.va_rdev = dev;
		whiteout = 0;

		switch (mode & S_IFMT) {
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
	if (vn_start_write(nd.ni_dvp, &mp, V_NOWAIT) != 0) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		vput(nd.ni_dvp);
		if ((error = vn_start_write(NULL, &mp, V_XSLEEP | PCATCH)) != 0)
			return (error);
		goto restart;
	}
#ifdef MAC
	if (error == 0 && !whiteout)
		error = mac_check_vnode_create(td->td_ucred, nd.ni_dvp,
		    &nd.ni_cnd, &vattr);
#endif
	if (!error) {
		VOP_LEASE(nd.ni_dvp, td, td->td_ucred, LEASE_WRITE);
		if (whiteout)
			error = VOP_WHITEOUT(nd.ni_dvp, &nd.ni_cnd, CREATE);
		else {
			error = VOP_MKNOD(nd.ni_dvp, &nd.ni_vp,
						&nd.ni_cnd, &vattr);
			if (error == 0)
				vput(nd.ni_vp);
		}
	}
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vput(nd.ni_dvp);
	vn_finished_write(mp);
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
int
mkfifo(td, uap)
	struct thread *td;
	register struct mkfifo_args /* {
		char *path;
		int mode;
	} */ *uap;
{

	return (kern_mkfifo(td, uap->path, UIO_USERSPACE, uap->mode));
}

int
kern_mkfifo(struct thread *td, char *path, enum uio_seg pathseg, int mode)
{
	struct mount *mp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

restart:
	bwillwrite();
	NDINIT(&nd, CREATE, LOCKPARENT | SAVENAME, pathseg, path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	if (nd.ni_vp != NULL) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		vrele(nd.ni_vp);
		if (nd.ni_vp == nd.ni_dvp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		return (EEXIST);
	}
	if (vn_start_write(nd.ni_dvp, &mp, V_NOWAIT) != 0) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		vput(nd.ni_dvp);
		if ((error = vn_start_write(NULL, &mp, V_XSLEEP | PCATCH)) != 0)
			return (error);
		goto restart;
	}
	VATTR_NULL(&vattr);
	vattr.va_type = VFIFO;
	FILEDESC_LOCK_FAST(td->td_proc->p_fd);
	vattr.va_mode = (mode & ALLPERMS) & ~td->td_proc->p_fd->fd_cmask;
	FILEDESC_UNLOCK_FAST(td->td_proc->p_fd);
#ifdef MAC
	error = mac_check_vnode_create(td->td_ucred, nd.ni_dvp, &nd.ni_cnd,
	    &vattr);
	if (error)
		goto out;
#endif
	VOP_LEASE(nd.ni_dvp, td, td->td_ucred, LEASE_WRITE);
	error = VOP_MKNOD(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr);
	if (error == 0)
		vput(nd.ni_vp);
#ifdef MAC
out:
#endif
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vput(nd.ni_dvp);
	vn_finished_write(mp);
	return (error);
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
int
link(td, uap)
	struct thread *td;
	register struct link_args /* {
		char *path;
		char *link;
	} */ *uap;
{
	int error;

	mtx_lock(&Giant);
	error = kern_link(td, uap->path, uap->link, UIO_USERSPACE);
	mtx_unlock(&Giant);
	return (error);
}

SYSCTL_DECL(_security_bsd);

static int hardlink_check_uid = 0;
SYSCTL_INT(_security_bsd, OID_AUTO, hardlink_check_uid, CTLFLAG_RW,
    &hardlink_check_uid, 0,
    "Unprivileged processes cannot create hard links to files owned by other "
    "users");
static int hardlink_check_gid = 0;
SYSCTL_INT(_security_bsd, OID_AUTO, hardlink_check_gid, CTLFLAG_RW,
    &hardlink_check_gid, 0,
    "Unprivileged processes cannot create hard links to files owned by other "
    "groups");

static int
can_hardlink(struct vnode *vp, struct thread *td, struct ucred *cred)
{
	struct vattr va;
	int error;

	if (suser_cred(cred, SUSER_ALLOWJAIL) == 0)
		return (0);

	if (!hardlink_check_uid && !hardlink_check_gid)
		return (0);

	error = VOP_GETATTR(vp, &va, cred, td);
	if (error != 0)
		return (error);

	if (hardlink_check_uid) {
		if (cred->cr_uid != va.va_uid)
			return (EPERM);
	}

	if (hardlink_check_gid) {
		if (!groupmember(va.va_gid, cred))
			return (EPERM);
	}

	return (0);
}

int
kern_link(struct thread *td, char *path, char *link, enum uio_seg segflg)
{
	struct vnode *vp;
	struct mount *mp;
	struct nameidata nd;
	int error;

	bwillwrite();
	NDINIT(&nd, LOOKUP, FOLLOW|NOOBJ, segflg, path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;
	if (vp->v_type == VDIR) {
		vrele(vp);
		return (EPERM);		/* POSIX */
	}
	if ((error = vn_start_write(vp, &mp, V_WAIT | PCATCH)) != 0) {
		vrele(vp);
		return (error);
	}
	NDINIT(&nd, CREATE, LOCKPARENT | NOOBJ | SAVENAME, segflg, link, td);
	if ((error = namei(&nd)) == 0) {
		if (nd.ni_vp != NULL) {
			vrele(nd.ni_vp);
			if (nd.ni_dvp == nd.ni_vp)
				vrele(nd.ni_dvp);
			else
				vput(nd.ni_dvp);
			error = EEXIST;
		} else if ((error = vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td))
		    == 0) {
			VOP_LEASE(nd.ni_dvp, td, td->td_ucred, LEASE_WRITE);
			VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
			error = can_hardlink(vp, td, td->td_ucred);
			if (error == 0)
#ifdef MAC
				error = mac_check_vnode_link(td->td_ucred,
				    nd.ni_dvp, vp, &nd.ni_cnd);
			if (error == 0)
#endif
				error = VOP_LINK(nd.ni_dvp, vp, &nd.ni_cnd);
			VOP_UNLOCK(vp, 0, td);
			vput(nd.ni_dvp);
		}
		NDFREE(&nd, NDF_ONLY_PNBUF);
	}
	vrele(vp);
	vn_finished_write(mp);
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
int
symlink(td, uap)
	struct thread *td;
	register struct symlink_args /* {
		char *path;
		char *link;
	} */ *uap;
{

	return (kern_symlink(td, uap->path, uap->link, UIO_USERSPACE));
}

int
kern_symlink(struct thread *td, char *path, char *link, enum uio_seg segflg)
{
	struct mount *mp;
	struct vattr vattr;
	char *syspath;
	int error;
	struct nameidata nd;

	if (segflg == UIO_SYSSPACE) {
		syspath = path;
	} else {
		syspath = uma_zalloc(namei_zone, M_WAITOK);
		if ((error = copyinstr(path, syspath, MAXPATHLEN, NULL)) != 0)
			goto out;
	}
restart:
	bwillwrite();
	NDINIT(&nd, CREATE, LOCKPARENT | NOOBJ | SAVENAME, segflg, link, td);
	if ((error = namei(&nd)) != 0)
		goto out;
	if (nd.ni_vp) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		vrele(nd.ni_vp);
		if (nd.ni_vp == nd.ni_dvp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		error = EEXIST;
		goto out;
	}
	if (vn_start_write(nd.ni_dvp, &mp, V_NOWAIT) != 0) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		vput(nd.ni_dvp);
		if ((error = vn_start_write(NULL, &mp, V_XSLEEP | PCATCH)) != 0)
			goto out;
		goto restart;
	}
	VATTR_NULL(&vattr);
	FILEDESC_LOCK_FAST(td->td_proc->p_fd);
	vattr.va_mode = ACCESSPERMS &~ td->td_proc->p_fd->fd_cmask;
	FILEDESC_UNLOCK_FAST(td->td_proc->p_fd);
#ifdef MAC
	vattr.va_type = VLNK;
	error = mac_check_vnode_create(td->td_ucred, nd.ni_dvp, &nd.ni_cnd,
	    &vattr);
	if (error)
		goto out2;
#endif
	VOP_LEASE(nd.ni_dvp, td, td->td_ucred, LEASE_WRITE);
	error = VOP_SYMLINK(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr, syspath);
	if (error == 0)
		vput(nd.ni_vp);
#ifdef MAC
out2:
#endif
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vput(nd.ni_dvp);
	vn_finished_write(mp);
	ASSERT_VOP_UNLOCKED(nd.ni_dvp, "symlink");
	ASSERT_VOP_UNLOCKED(nd.ni_vp, "symlink");
out:
	if (segflg != UIO_SYSSPACE)
		uma_zfree(namei_zone, syspath);
	return (error);
}

/*
 * Delete a whiteout from the filesystem.
 */
int
undelete(td, uap)
	struct thread *td;
	register struct undelete_args /* {
		char *path;
	} */ *uap;
{
	int error;
	struct mount *mp;
	struct nameidata nd;

restart:
	bwillwrite();
	NDINIT(&nd, DELETE, LOCKPARENT|DOWHITEOUT, UIO_USERSPACE,
	    uap->path, td);
	error = namei(&nd);
	if (error)
		return (error);

	if (nd.ni_vp != NULLVP || !(nd.ni_cnd.cn_flags & ISWHITEOUT)) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		if (nd.ni_vp)
			vrele(nd.ni_vp);
		if (nd.ni_vp == nd.ni_dvp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		return (EEXIST);
	}
	if (vn_start_write(nd.ni_dvp, &mp, V_NOWAIT) != 0) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		vput(nd.ni_dvp);
		if ((error = vn_start_write(NULL, &mp, V_XSLEEP | PCATCH)) != 0)
			return (error);
		goto restart;
	}
	VOP_LEASE(nd.ni_dvp, td, td->td_ucred, LEASE_WRITE);
	error = VOP_WHITEOUT(nd.ni_dvp, &nd.ni_cnd, DELETE);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vput(nd.ni_dvp);
	vn_finished_write(mp);
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
int
unlink(td, uap)
	struct thread *td;
	struct unlink_args /* {
		char *path;
	} */ *uap;
{
	int error;

	mtx_lock(&Giant);
	error = kern_unlink(td, uap->path, UIO_USERSPACE);
	mtx_unlock(&Giant);
	return (error);
}

int
kern_unlink(struct thread *td, char *path, enum uio_seg pathseg)
{
	struct mount *mp;
	struct vnode *vp;
	int error;
	struct nameidata nd;

restart:
	bwillwrite();
	NDINIT(&nd, DELETE, LOCKPARENT|LOCKLEAF, pathseg, path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	if (vp->v_type == VDIR)
		error = EPERM;		/* POSIX */
	else {
		/*
		 * The root of a mounted filesystem cannot be deleted.
		 *
		 * XXX: can this only be a VDIR case?
		 */
		if (vp->v_vflag & VV_ROOT)
			error = EBUSY;
	}
	if (error == 0) {
		if (vn_start_write(nd.ni_dvp, &mp, V_NOWAIT) != 0) {
			NDFREE(&nd, NDF_ONLY_PNBUF);
			if (vp == nd.ni_dvp)
				vrele(vp);
			else
				vput(vp);
			vput(nd.ni_dvp);
			if ((error = vn_start_write(NULL, &mp,
			    V_XSLEEP | PCATCH)) != 0)
				return (error);
			goto restart;
		}
#ifdef MAC
		error = mac_check_vnode_delete(td->td_ucred, nd.ni_dvp, vp,
		    &nd.ni_cnd);
		if (error)
			goto out;
#endif
		VOP_LEASE(nd.ni_dvp, td, td->td_ucred, LEASE_WRITE);
		error = VOP_REMOVE(nd.ni_dvp, vp, &nd.ni_cnd);
#ifdef MAC
out:
#endif
		vn_finished_write(mp);
	}
	NDFREE(&nd, NDF_ONLY_PNBUF);
	if (vp == nd.ni_dvp)
		vrele(vp);
	else
		vput(vp);
	vput(nd.ni_dvp);
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
lseek(td, uap)
	struct thread *td;
	register struct lseek_args /* {
		int fd;
		int pad;
		off_t offset;
		int whence;
	} */ *uap;
{
	struct ucred *cred = td->td_ucred;
	struct file *fp;
	struct vnode *vp;
	struct vattr vattr;
	off_t offset;
	int error, noneg;

	if ((error = fget(td, uap->fd, &fp)) != 0)
		return (error);
	if (!(fp->f_ops->fo_flags & DFLAG_SEEKABLE)) {
		fdrop(fp, td);
		return (ESPIPE);
	}
	vp = fp->f_vnode;
	noneg = (vp->v_type != VCHR);
	offset = uap->offset;
	switch (uap->whence) {
	case L_INCR:
		if (noneg &&
		    (fp->f_offset < 0 ||
		    (offset > 0 && fp->f_offset > OFF_MAX - offset))) {
			error = EOVERFLOW;
			break;
		}
		offset += fp->f_offset;
		break;
	case L_XTND:
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
		error = VOP_GETATTR(vp, &vattr, cred, td);
		VOP_UNLOCK(vp, 0, td);
		if (error)
			break;
		if (noneg &&
		    (vattr.va_size > OFF_MAX ||
		    (offset > 0 && vattr.va_size > OFF_MAX - offset))) {
			error = EOVERFLOW;
			break;
		}
		offset += vattr.va_size;
		break;
	case L_SET:
		break;
	default:
		error = EINVAL;
	}
	if (error == 0 && noneg && offset < 0)
		error = EINVAL;
	if (error != 0) {
		fdrop(fp, td);
		return (error);
	}
	fp->f_offset = offset;
	*(off_t *)(td->td_retval) = fp->f_offset;
	fdrop(fp, td);
	return (0);
}

#if defined(COMPAT_43)
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
olseek(td, uap)
	struct thread *td;
	register struct olseek_args /* {
		int fd;
		long offset;
		int whence;
	} */ *uap;
{
	struct lseek_args /* {
		int fd;
		int pad;
		off_t offset;
		int whence;
	} */ nuap;
	int error;

	nuap.fd = uap->fd;
	nuap.offset = uap->offset;
	nuap.whence = uap->whence;
	error = lseek(td, &nuap);
	return (error);
}
#endif /* COMPAT_43 */

/*
 * Check access permissions using passed credentials.
 */
static int
vn_access(vp, user_flags, cred, td)
	struct vnode	*vp;
	int		user_flags;
	struct ucred	*cred;
	struct thread	*td;
{
	int error, flags;

	/* Flags == 0 means only check for existence. */
	error = 0;
	if (user_flags) {
		flags = 0;
		if (user_flags & R_OK)
			flags |= VREAD;
		if (user_flags & W_OK)
			flags |= VWRITE;
		if (user_flags & X_OK)
			flags |= VEXEC;
#ifdef MAC
		error = mac_check_vnode_access(cred, vp, flags);
		if (error)
			return (error);
#endif
		if ((flags & VWRITE) == 0 || (error = vn_writechk(vp)) == 0)
			error = VOP_ACCESS(vp, flags, cred, td);
	}
	return (error);
}

/*
 * Check access permissions using "real" credentials.
 */
#ifndef _SYS_SYSPROTO_H_
struct access_args {
	char	*path;
	int	flags;
};
#endif
int
access(td, uap)
	struct thread *td;
	register struct access_args /* {
		char *path;
		int flags;
	} */ *uap;
{

	return (kern_access(td, uap->path, UIO_USERSPACE, uap->flags));
}

int
kern_access(struct thread *td, char *path, enum uio_seg pathseg, int flags)
{
	struct ucred *cred, *tmpcred;
	register struct vnode *vp;
	int error;
	struct nameidata nd;

	/*
	 * Create and modify a temporary credential instead of one that
	 * is potentially shared.  This could also mess up socket
	 * buffer accounting which can run in an interrupt context.
	 */
	cred = td->td_ucred;
	tmpcred = crdup(cred);
	tmpcred->cr_uid = cred->cr_ruid;
	tmpcred->cr_groups[0] = cred->cr_rgid;
	td->td_ucred = tmpcred;
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | NOOBJ, pathseg, path, td);
	if ((error = namei(&nd)) != 0)
		goto out1;
	vp = nd.ni_vp;

	error = vn_access(vp, flags, tmpcred, td);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vput(vp);
out1:
	td->td_ucred = cred;
	crfree(tmpcred);
	return (error);
}

/*
 * Check access permissions using "effective" credentials.
 */
#ifndef _SYS_SYSPROTO_H_
struct eaccess_args {
	char	*path;
	int	flags;
};
#endif
int
eaccess(td, uap)
	struct thread *td;
	register struct eaccess_args /* {
		char *path;
		int flags;
	} */ *uap;
{
	struct nameidata nd;
	struct vnode *vp;
	int error;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | NOOBJ, UIO_USERSPACE,
	    uap->path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;

	error = vn_access(vp, uap->flags, td->td_ucred, td);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vput(vp);
	return (error);
}

#if defined(COMPAT_43)
/*
 * Get file status; this version follows links.
 */
#ifndef _SYS_SYSPROTO_H_
struct ostat_args {
	char	*path;
	struct ostat *ub;
};
#endif
int
ostat(td, uap)
	struct thread *td;
	register struct ostat_args /* {
		char *path;
		struct ostat *ub;
	} */ *uap;
{
	struct stat sb;
	struct ostat osb;
	int error;

	error = kern_stat(td, uap->path, UIO_USERSPACE, &sb);
	if (error)
		return (error);
	cvtstat(&sb, &osb);
	error = copyout(&osb, uap->ub, sizeof (osb));
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
int
olstat(td, uap)
	struct thread *td;
	register struct olstat_args /* {
		char *path;
		struct ostat *ub;
	} */ *uap;
{
	struct stat sb;
	struct ostat osb;
	int error;

	error = kern_lstat(td, uap->path, UIO_USERSPACE, &sb);
	if (error)
		return (error);
	cvtstat(&sb, &osb);
	error = copyout(&osb, uap->ub, sizeof (osb));
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
#endif /* COMPAT_43 */

/*
 * Get file status; this version follows links.
 */
#ifndef _SYS_SYSPROTO_H_
struct stat_args {
	char	*path;
	struct stat *ub;
};
#endif
int
stat(td, uap)
	struct thread *td;
	register struct stat_args /* {
		char *path;
		struct stat *ub;
	} */ *uap;
{
	struct stat sb;
	int error;

	error = kern_stat(td, uap->path, UIO_USERSPACE, &sb);
	if (error == 0)
		error = copyout(&sb, uap->ub, sizeof (sb));
	return (error);
}

int
kern_stat(struct thread *td, char *path, enum uio_seg pathseg, struct stat *sbp)
{
	struct nameidata nd;
	struct stat sb;
	int error;

#ifdef LOOKUP_SHARED
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKSHARED | LOCKLEAF | NOOBJ,
	    pathseg, path, td);
#else
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | NOOBJ, pathseg,
	    path, td);
#endif
	if ((error = namei(&nd)) != 0)
		return (error);
	error = vn_stat(nd.ni_vp, &sb, td->td_ucred, NOCRED, td);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vput(nd.ni_vp);
	if (error)
		return (error);
	*sbp = sb;
	return (0);
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
int
lstat(td, uap)
	struct thread *td;
	register struct lstat_args /* {
		char *path;
		struct stat *ub;
	} */ *uap;
{
	struct stat sb;
	int error;

	error = kern_lstat(td, uap->path, UIO_USERSPACE, &sb);
	if (error == 0)
		error = copyout(&sb, uap->ub, sizeof (sb));
	return (error);
}

int
kern_lstat(struct thread *td, char *path, enum uio_seg pathseg, struct stat *sbp)
{
	struct vnode *vp;
	struct stat sb;
	struct nameidata nd;
	int error;

	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF | NOOBJ, pathseg,
	    path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	error = vn_stat(vp, &sb, td->td_ucred, NOCRED, td);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vput(vp);
	if (error)
		return (error);
	*sbp = sb;
	return (0);
}

/*
 * Implementation of the NetBSD [l]stat() functions.
 */
void
cvtnstat(sb, nsb)
	struct stat *sb;
	struct nstat *nsb;
{
	bzero(nsb, sizeof *nsb);
	nsb->st_dev = sb->st_dev;
	nsb->st_ino = sb->st_ino;
	nsb->st_mode = sb->st_mode;
	nsb->st_nlink = sb->st_nlink;
	nsb->st_uid = sb->st_uid;
	nsb->st_gid = sb->st_gid;
	nsb->st_rdev = sb->st_rdev;
	nsb->st_atimespec = sb->st_atimespec;
	nsb->st_mtimespec = sb->st_mtimespec;
	nsb->st_ctimespec = sb->st_ctimespec;
	nsb->st_size = sb->st_size;
	nsb->st_blocks = sb->st_blocks;
	nsb->st_blksize = sb->st_blksize;
	nsb->st_flags = sb->st_flags;
	nsb->st_gen = sb->st_gen;
	nsb->st_birthtimespec = sb->st_birthtimespec;
}

#ifndef _SYS_SYSPROTO_H_
struct nstat_args {
	char	*path;
	struct nstat *ub;
};
#endif
int
nstat(td, uap)
	struct thread *td;
	register struct nstat_args /* {
		char *path;
		struct nstat *ub;
	} */ *uap;
{
	struct stat sb;
	struct nstat nsb;
	int error;

	error = kern_stat(td, uap->path, UIO_USERSPACE, &sb);
	if (error)
		return (error);
	cvtnstat(&sb, &nsb);
	error = copyout(&nsb, uap->ub, sizeof (nsb));
	return (error);
}

/*
 * NetBSD lstat.  Get file status; this version does not follow links.
 */
#ifndef _SYS_SYSPROTO_H_
struct lstat_args {
	char	*path;
	struct stat *ub;
};
#endif
int
nlstat(td, uap)
	struct thread *td;
	register struct nlstat_args /* {
		char *path;
		struct nstat *ub;
	} */ *uap;
{
	struct stat sb;
	struct nstat nsb;
	int error;

	error = kern_lstat(td, uap->path, UIO_USERSPACE, &sb);
	if (error)
		return (error);
	cvtnstat(&sb, &nsb);
	error = copyout(&nsb, uap->ub, sizeof (nsb));
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
int
pathconf(td, uap)
	struct thread *td;
	register struct pathconf_args /* {
		char *path;
		int name;
	} */ *uap;
{

	return (kern_pathconf(td, uap->path, UIO_USERSPACE, uap->name));
}

int
kern_pathconf(struct thread *td, char *path, enum uio_seg pathseg, int name)
{
	struct nameidata nd;
	int error;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | NOOBJ, pathseg,
	    path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	/* If asynchronous I/O is available, it works for all files. */
	if (name == _PC_ASYNC_IO)
		td->td_retval[0] = async_io_version;
	else
		error = VOP_PATHCONF(nd.ni_vp, name, td->td_retval);
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
int
readlink(td, uap)
	struct thread *td;
	register struct readlink_args /* {
		char *path;
		char *buf;
		int count;
	} */ *uap;
{

	return (kern_readlink(td, uap->path, UIO_USERSPACE, uap->buf,
	    UIO_USERSPACE, uap->count));
}

int
kern_readlink(struct thread *td, char *path, enum uio_seg pathseg, char *buf,
    enum uio_seg bufseg, int count)
{
	register struct vnode *vp;
	struct iovec aiov;
	struct uio auio;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF | NOOBJ, pathseg, path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;
#ifdef MAC
	error = mac_check_vnode_readlink(td->td_ucred, vp);
	if (error) {
		vput(vp);
		return (error);
	}
#endif
	if (vp->v_type != VLNK)
		error = EINVAL;
	else {
		aiov.iov_base = buf;
		aiov.iov_len = count;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = 0;
		auio.uio_rw = UIO_READ;
		auio.uio_segflg = bufseg;
		auio.uio_td = td;
		auio.uio_resid = count;
		error = VOP_READLINK(vp, &auio, td->td_ucred);
	}
	vput(vp);
	td->td_retval[0] = count - auio.uio_resid;
	return (error);
}

/*
 * Common implementation code for chflags() and fchflags().
 */
static int
setfflags(td, vp, flags)
	struct thread *td;
	struct vnode *vp;
	int flags;
{
	int error;
	struct mount *mp;
	struct vattr vattr;

	/*
	 * Prevent non-root users from setting flags on devices.  When
	 * a device is reused, users can retain ownership of the device
	 * if they are allowed to set flags and programs assume that
	 * chown can't fail when done as root.
	 */
	if (vp->v_type == VCHR || vp->v_type == VBLK) {
		error = suser_cred(td->td_ucred, SUSER_ALLOWJAIL);
		if (error)
			return (error);
	}

	if ((error = vn_start_write(vp, &mp, V_WAIT | PCATCH)) != 0)
		return (error);
	VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	VATTR_NULL(&vattr);
	vattr.va_flags = flags;
#ifdef MAC
	error = mac_check_vnode_setflags(td->td_ucred, vp, vattr.va_flags);
	if (error == 0)
#endif
		error = VOP_SETATTR(vp, &vattr, td->td_ucred, td);
	VOP_UNLOCK(vp, 0, td);
	vn_finished_write(mp);
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
int
chflags(td, uap)
	struct thread *td;
	register struct chflags_args /* {
		char *path;
		int flags;
	} */ *uap;
{
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, uap->path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	error = setfflags(td, nd.ni_vp, uap->flags);
	vrele(nd.ni_vp);
	return (error);
}

/*
 * Same as chflags() but doesn't follow symlinks.
 */
int
lchflags(td, uap)
	struct thread *td;
	register struct lchflags_args /* {
		char *path;
		int flags;
	} */ *uap;
{
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_USERSPACE, uap->path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	error = setfflags(td, nd.ni_vp, uap->flags);
	vrele(nd.ni_vp);
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
int
fchflags(td, uap)
	struct thread *td;
	register struct fchflags_args /* {
		int fd;
		int flags;
	} */ *uap;
{
	struct file *fp;
	int error;

	if ((error = getvnode(td->td_proc->p_fd, uap->fd, &fp)) != 0)
		return (error);
	error = setfflags(td, fp->f_vnode, uap->flags);
	fdrop(fp, td);
	return (error);
}

/*
 * Common implementation code for chmod(), lchmod() and fchmod().
 */
static int
setfmode(td, vp, mode)
	struct thread *td;
	struct vnode *vp;
	int mode;
{
	int error;
	struct mount *mp;
	struct vattr vattr;

	if ((error = vn_start_write(vp, &mp, V_WAIT | PCATCH)) != 0)
		return (error);
	VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	VATTR_NULL(&vattr);
	vattr.va_mode = mode & ALLPERMS;
#ifdef MAC
	error = mac_check_vnode_setmode(td->td_ucred, vp, vattr.va_mode);
	if (error == 0)
#endif
		error = VOP_SETATTR(vp, &vattr, td->td_ucred, td);
	VOP_UNLOCK(vp, 0, td);
	vn_finished_write(mp);
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
int
chmod(td, uap)
	struct thread *td;
	register struct chmod_args /* {
		char *path;
		int mode;
	} */ *uap;
{

	return (kern_chmod(td, uap->path, UIO_USERSPACE, uap->mode));
}

int
kern_chmod(struct thread *td, char *path, enum uio_seg pathseg, int mode)
{
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, pathseg, path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	error = setfmode(td, nd.ni_vp, mode);
	vrele(nd.ni_vp);
	return (error);
}

/*
 * Change mode of a file given path name (don't follow links.)
 */
#ifndef _SYS_SYSPROTO_H_
struct lchmod_args {
	char	*path;
	int	mode;
};
#endif
int
lchmod(td, uap)
	struct thread *td;
	register struct lchmod_args /* {
		char *path;
		int mode;
	} */ *uap;
{
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_USERSPACE, uap->path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	error = setfmode(td, nd.ni_vp, uap->mode);
	vrele(nd.ni_vp);
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
int
fchmod(td, uap)
	struct thread *td;
	register struct fchmod_args /* {
		int fd;
		int mode;
	} */ *uap;
{
	struct file *fp;
	int error;

	if ((error = getvnode(td->td_proc->p_fd, uap->fd, &fp)) != 0)
		return (error);
	error = setfmode(td, fp->f_vnode, uap->mode);
	fdrop(fp, td);
	return (error);
}

/*
 * Common implementation for chown(), lchown(), and fchown()
 */
static int
setfown(td, vp, uid, gid)
	struct thread *td;
	struct vnode *vp;
	uid_t uid;
	gid_t gid;
{
	int error;
	struct mount *mp;
	struct vattr vattr;

	if ((error = vn_start_write(vp, &mp, V_WAIT | PCATCH)) != 0)
		return (error);
	VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	VATTR_NULL(&vattr);
	vattr.va_uid = uid;
	vattr.va_gid = gid;
#ifdef MAC
	error = mac_check_vnode_setowner(td->td_ucred, vp, vattr.va_uid,
	    vattr.va_gid);
	if (error == 0)
#endif
		error = VOP_SETATTR(vp, &vattr, td->td_ucred, td);
	VOP_UNLOCK(vp, 0, td);
	vn_finished_write(mp);
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
int
chown(td, uap)
	struct thread *td;
	register struct chown_args /* {
		char *path;
		int uid;
		int gid;
	} */ *uap;
{

	return (kern_chown(td, uap->path, UIO_USERSPACE, uap->uid, uap->gid));
}

int
kern_chown(struct thread *td, char *path, enum uio_seg pathseg, int uid,
    int gid)
{
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, pathseg, path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	error = setfown(td, nd.ni_vp, uid, gid);
	vrele(nd.ni_vp);
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
int
lchown(td, uap)
	struct thread *td;
	register struct lchown_args /* {
		char *path;
		int uid;
		int gid;
	} */ *uap;
{

	return (kern_lchown(td, uap->path, UIO_USERSPACE, uap->uid, uap->gid));
}

int
kern_lchown(struct thread *td, char *path, enum uio_seg pathseg, int uid,
    int gid)
{
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, NOFOLLOW, pathseg, path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	error = setfown(td, nd.ni_vp, uid, gid);
	vrele(nd.ni_vp);
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
int
fchown(td, uap)
	struct thread *td;
	register struct fchown_args /* {
		int fd;
		int uid;
		int gid;
	} */ *uap;
{
	struct file *fp;
	int error;

	if ((error = getvnode(td->td_proc->p_fd, uap->fd, &fp)) != 0)
		return (error);
	error = setfown(td, fp->f_vnode, uap->uid, uap->gid);
	fdrop(fp, td);
	return (error);
}

/*
 * Common implementation code for utimes(), lutimes(), and futimes().
 */
static int
getutimes(usrtvp, tvpseg, tsp)
	const struct timeval *usrtvp;
	enum uio_seg tvpseg;
	struct timespec *tsp;
{
	struct timeval tv[2];
	const struct timeval *tvp;
	int error;

	if (usrtvp == NULL) {
		microtime(&tv[0]);
		TIMEVAL_TO_TIMESPEC(&tv[0], &tsp[0]);
		tsp[1] = tsp[0];
	} else {
		if (tvpseg == UIO_SYSSPACE) {
			tvp = usrtvp;
		} else {
			if ((error = copyin(usrtvp, tv, sizeof(tv))) != 0)
				return (error);
			tvp = tv;
		}

		TIMEVAL_TO_TIMESPEC(&tvp[0], &tsp[0]);
		TIMEVAL_TO_TIMESPEC(&tvp[1], &tsp[1]);
	}
	return (0);
}

/*
 * Common implementation code for utimes(), lutimes(), and futimes().
 */
static int
setutimes(td, vp, ts, numtimes, nullflag)
	struct thread *td;
	struct vnode *vp;
	const struct timespec *ts;
	int numtimes;
	int nullflag;
{
	int error, setbirthtime;
	struct mount *mp;
	struct vattr vattr;

	if ((error = vn_start_write(vp, &mp, V_WAIT | PCATCH)) != 0)
		return (error);
	VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	setbirthtime = 0;
	if (numtimes < 3 && VOP_GETATTR(vp, &vattr, td->td_ucred, td) == 0 &&
	    timespeccmp(&ts[1], &vattr.va_birthtime, < ))
		setbirthtime = 1;
	VATTR_NULL(&vattr);
	vattr.va_atime = ts[0];
	vattr.va_mtime = ts[1];
	if (setbirthtime)
		vattr.va_birthtime = ts[1];
	if (numtimes > 2)
		vattr.va_birthtime = ts[2];
	if (nullflag)
		vattr.va_vaflags |= VA_UTIMES_NULL;
#ifdef MAC
	error = mac_check_vnode_setutimes(td->td_ucred, vp, vattr.va_atime,
	    vattr.va_mtime);
#endif
	if (error == 0)
		error = VOP_SETATTR(vp, &vattr, td->td_ucred, td);
	VOP_UNLOCK(vp, 0, td);
	vn_finished_write(mp);
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
int
utimes(td, uap)
	struct thread *td;
	register struct utimes_args /* {
		char *path;
		struct timeval *tptr;
	} */ *uap;
{

	return (kern_utimes(td, uap->path, UIO_USERSPACE, uap->tptr,
	    UIO_USERSPACE));
}

int
kern_utimes(struct thread *td, char *path, enum uio_seg pathseg,
    struct timeval *tptr, enum uio_seg tptrseg)
{
	struct timespec ts[2];
	int error;
	struct nameidata nd;

	if ((error = getutimes(tptr, tptrseg, ts)) != 0)
		return (error);
	NDINIT(&nd, LOOKUP, FOLLOW, pathseg, path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	error = setutimes(td, nd.ni_vp, ts, 2, tptr == NULL);
	vrele(nd.ni_vp);
	return (error);
}

/*
 * Set the access and modification times of a file.
 */
#ifndef _SYS_SYSPROTO_H_
struct lutimes_args {
	char	*path;
	struct	timeval *tptr;
};
#endif
int
lutimes(td, uap)
	struct thread *td;
	register struct lutimes_args /* {
		char *path;
		struct timeval *tptr;
	} */ *uap;
{

	return (kern_lutimes(td, uap->path, UIO_USERSPACE, uap->tptr,
	    UIO_USERSPACE));
}

int
kern_lutimes(struct thread *td, char *path, enum uio_seg pathseg,
    struct timeval *tptr, enum uio_seg tptrseg)
{
	struct timespec ts[2];
	int error;
	struct nameidata nd;

	if ((error = getutimes(tptr, tptrseg, ts)) != 0)
		return (error);
	NDINIT(&nd, LOOKUP, NOFOLLOW, pathseg, path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	error = setutimes(td, nd.ni_vp, ts, 2, tptr == NULL);
	vrele(nd.ni_vp);
	return (error);
}

/*
 * Set the access and modification times of a file.
 */
#ifndef _SYS_SYSPROTO_H_
struct futimes_args {
	int	fd;
	struct	timeval *tptr;
};
#endif
int
futimes(td, uap)
	struct thread *td;
	register struct futimes_args /* {
		int  fd;
		struct timeval *tptr;
	} */ *uap;
{

	return (kern_futimes(td, uap->fd, uap->tptr, UIO_USERSPACE));
}

int
kern_futimes(struct thread *td, int fd, struct timeval *tptr,
    enum uio_seg tptrseg)
{
	struct timespec ts[2];
	struct file *fp;
	int error;

	if ((error = getutimes(tptr, tptrseg, ts)) != 0)
		return (error);
	if ((error = getvnode(td->td_proc->p_fd, fd, &fp)) != 0)
		return (error);
	error = setutimes(td, fp->f_vnode, ts, 2, tptr == NULL);
	fdrop(fp, td);
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
int
truncate(td, uap)
	struct thread *td;
	register struct truncate_args /* {
		char *path;
		int pad;
		off_t length;
	} */ *uap;
{

	return (kern_truncate(td, uap->path, UIO_USERSPACE, uap->length));
}

int
kern_truncate(struct thread *td, char *path, enum uio_seg pathseg, off_t length)
{
	struct mount *mp;
	struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	if (length < 0)
		return(EINVAL);
	NDINIT(&nd, LOOKUP, FOLLOW, pathseg, path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	if ((error = vn_start_write(vp, &mp, V_WAIT | PCATCH)) != 0) {
		vrele(vp);
		return (error);
	}
	NDFREE(&nd, NDF_ONLY_PNBUF);
	VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	if (vp->v_type == VDIR)
		error = EISDIR;
#ifdef MAC
	else if ((error = mac_check_vnode_write(td->td_ucred, NOCRED, vp))) {
	}
#endif
	else if ((error = vn_writechk(vp)) == 0 &&
	    (error = VOP_ACCESS(vp, VWRITE, td->td_ucred, td)) == 0) {
		VATTR_NULL(&vattr);
		vattr.va_size = length;
		error = VOP_SETATTR(vp, &vattr, td->td_ucred, td);
	}
	vput(vp);
	vn_finished_write(mp);
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
int
ftruncate(td, uap)
	struct thread *td;
	register struct ftruncate_args /* {
		int fd;
		int pad;
		off_t length;
	} */ *uap;
{
	struct mount *mp;
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	if (uap->length < 0)
		return(EINVAL);
	if ((error = getvnode(td->td_proc->p_fd, uap->fd, &fp)) != 0)
		return (error);
	if ((fp->f_flag & FWRITE) == 0) {
		fdrop(fp, td);
		return (EINVAL);
	}
	vp = fp->f_vnode;
	if ((error = vn_start_write(vp, &mp, V_WAIT | PCATCH)) != 0) {
		fdrop(fp, td);
		return (error);
	}
	VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	if (vp->v_type == VDIR)
		error = EISDIR;
#ifdef MAC
	else if ((error = mac_check_vnode_write(td->td_ucred, fp->f_cred,
	    vp))) {
	}
#endif
	else if ((error = vn_writechk(vp)) == 0) {
		VATTR_NULL(&vattr);
		vattr.va_size = uap->length;
		error = VOP_SETATTR(vp, &vattr, fp->f_cred, td);
	}
	VOP_UNLOCK(vp, 0, td);
	vn_finished_write(mp);
	fdrop(fp, td);
	return (error);
}

#if defined(COMPAT_43)
/*
 * Truncate a file given its path name.
 */
#ifndef _SYS_SYSPROTO_H_
struct otruncate_args {
	char	*path;
	long	length;
};
#endif
int
otruncate(td, uap)
	struct thread *td;
	register struct otruncate_args /* {
		char *path;
		long length;
	} */ *uap;
{
	struct truncate_args /* {
		char *path;
		int pad;
		off_t length;
	} */ nuap;

	nuap.path = uap->path;
	nuap.length = uap->length;
	return (truncate(td, &nuap));
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
int
oftruncate(td, uap)
	struct thread *td;
	register struct oftruncate_args /* {
		int fd;
		long length;
	} */ *uap;
{
	struct ftruncate_args /* {
		int fd;
		int pad;
		off_t length;
	} */ nuap;

	nuap.fd = uap->fd;
	nuap.length = uap->length;
	return (ftruncate(td, &nuap));
}
#endif /* COMPAT_43 */

/*
 * Sync an open file.
 */
#ifndef _SYS_SYSPROTO_H_
struct fsync_args {
	int	fd;
};
#endif
int
fsync(td, uap)
	struct thread *td;
	struct fsync_args /* {
		int fd;
	} */ *uap;
{
	struct vnode *vp;
	struct mount *mp;
	struct file *fp;
	vm_object_t obj;
	int error;

	GIANT_REQUIRED;

	if ((error = getvnode(td->td_proc->p_fd, uap->fd, &fp)) != 0)
		return (error);
	vp = fp->f_vnode;
	if ((error = vn_start_write(vp, &mp, V_WAIT | PCATCH)) != 0) {
		fdrop(fp, td);
		return (error);
	}
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	if (VOP_GETVOBJECT(vp, &obj) == 0) {
		VM_OBJECT_LOCK(obj);
		vm_object_page_clean(obj, 0, 0, 0);
		VM_OBJECT_UNLOCK(obj);
	}
	error = VOP_FSYNC(vp, fp->f_cred, MNT_WAIT, td);
	if (error == 0 && vp->v_mount && (vp->v_mount->mnt_flag & MNT_SOFTDEP)
	    && softdep_fsync_hook != NULL)
		error = (*softdep_fsync_hook)(vp);

	VOP_UNLOCK(vp, 0, td);
	vn_finished_write(mp);
	fdrop(fp, td);
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
int
rename(td, uap)
	struct thread *td;
	register struct rename_args /* {
		char *from;
		char *to;
	} */ *uap;
{

	return (kern_rename(td, uap->from, uap->to, UIO_USERSPACE));
}

int
kern_rename(struct thread *td, char *from, char *to, enum uio_seg pathseg)
{
	struct mount *mp = NULL;
	struct vnode *tvp, *fvp, *tdvp;
	struct nameidata fromnd, tond;
	int error;

	bwillwrite();
#ifdef MAC
	NDINIT(&fromnd, DELETE, LOCKPARENT | LOCKLEAF | SAVESTART, pathseg,
	    from, td);
#else
	NDINIT(&fromnd, DELETE, WANTPARENT | SAVESTART, pathseg, from, td);
#endif
	if ((error = namei(&fromnd)) != 0)
		return (error);
#ifdef MAC
	error = mac_check_vnode_rename_from(td->td_ucred, fromnd.ni_dvp,
	    fromnd.ni_vp, &fromnd.ni_cnd);
	VOP_UNLOCK(fromnd.ni_dvp, 0, td);
	VOP_UNLOCK(fromnd.ni_vp, 0, td);
#endif
	fvp = fromnd.ni_vp;
	if (error == 0)
		error = vn_start_write(fvp, &mp, V_WAIT | PCATCH);
	if (error != 0) {
		NDFREE(&fromnd, NDF_ONLY_PNBUF);
		vrele(fromnd.ni_dvp);
		vrele(fvp);
		goto out1;
	}
	NDINIT(&tond, RENAME, LOCKPARENT | LOCKLEAF | NOCACHE | SAVESTART |
	    NOOBJ, pathseg, to, td);
	if (fromnd.ni_vp->v_type == VDIR)
		tond.ni_cnd.cn_flags |= WILLBEDIR;
	if ((error = namei(&tond)) != 0) {
		/* Translate error code for rename("dir1", "dir2/."). */
		if (error == EISDIR && fvp->v_type == VDIR)
			error = EINVAL;
		NDFREE(&fromnd, NDF_ONLY_PNBUF);
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
	 * If the source is the same as the destination (that is, if they
	 * are links to the same vnode), then there is nothing to do.
	 */
	if (fvp == tvp)
		error = -1;
#ifdef MAC
	else
		error = mac_check_vnode_rename_to(td->td_ucred, tdvp,
		    tond.ni_vp, fromnd.ni_dvp == tdvp, &tond.ni_cnd);
#endif
out:
	if (!error) {
		VOP_LEASE(tdvp, td, td->td_ucred, LEASE_WRITE);
		if (fromnd.ni_dvp != tdvp) {
			VOP_LEASE(fromnd.ni_dvp, td, td->td_ucred, LEASE_WRITE);
		}
		if (tvp) {
			VOP_LEASE(tvp, td, td->td_ucred, LEASE_WRITE);
		}
		error = VOP_RENAME(fromnd.ni_dvp, fromnd.ni_vp, &fromnd.ni_cnd,
				   tond.ni_dvp, tond.ni_vp, &tond.ni_cnd);
		NDFREE(&fromnd, NDF_ONLY_PNBUF);
		NDFREE(&tond, NDF_ONLY_PNBUF);
	} else {
		NDFREE(&fromnd, NDF_ONLY_PNBUF);
		NDFREE(&tond, NDF_ONLY_PNBUF);
		if (tdvp == tvp)
			vrele(tdvp);
		else
			vput(tdvp);
		if (tvp)
			vput(tvp);
		vrele(fromnd.ni_dvp);
		vrele(fvp);
	}
	vrele(tond.ni_startdir);
	ASSERT_VOP_UNLOCKED(fromnd.ni_dvp, "rename");
	ASSERT_VOP_UNLOCKED(fromnd.ni_vp, "rename");
	ASSERT_VOP_UNLOCKED(tond.ni_dvp, "rename");
	ASSERT_VOP_UNLOCKED(tond.ni_vp, "rename");
out1:
	vn_finished_write(mp);
	if (fromnd.ni_startdir)
		vrele(fromnd.ni_startdir);
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
int
mkdir(td, uap)
	struct thread *td;
	register struct mkdir_args /* {
		char *path;
		int mode;
	} */ *uap;
{

	return (kern_mkdir(td, uap->path, UIO_USERSPACE, uap->mode));
}

int
kern_mkdir(struct thread *td, char *path, enum uio_seg segflg, int mode)
{
	struct mount *mp;
	struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

restart:
	bwillwrite();
	NDINIT(&nd, CREATE, LOCKPARENT | SAVENAME, segflg, path, td);
	nd.ni_cnd.cn_flags |= WILLBEDIR;
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	if (vp != NULL) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		vrele(vp);
		/*
		 * XXX namei called with LOCKPARENT but not LOCKLEAF has
		 * the strange behaviour of leaving the vnode unlocked
		 * if the target is the same vnode as the parent.
		 */
		if (vp == nd.ni_dvp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		return (EEXIST);
	}
	if (vn_start_write(nd.ni_dvp, &mp, V_NOWAIT) != 0) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		vput(nd.ni_dvp);
		if ((error = vn_start_write(NULL, &mp, V_XSLEEP | PCATCH)) != 0)
			return (error);
		goto restart;
	}
	VATTR_NULL(&vattr);
	vattr.va_type = VDIR;
	FILEDESC_LOCK_FAST(td->td_proc->p_fd);
	vattr.va_mode = (mode & ACCESSPERMS) &~ td->td_proc->p_fd->fd_cmask;
	FILEDESC_UNLOCK_FAST(td->td_proc->p_fd);
#ifdef MAC
	error = mac_check_vnode_create(td->td_ucred, nd.ni_dvp, &nd.ni_cnd,
	    &vattr);
	if (error)
		goto out;
#endif
	VOP_LEASE(nd.ni_dvp, td, td->td_ucred, LEASE_WRITE);
	error = VOP_MKDIR(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr);
#ifdef MAC
out:
#endif
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vput(nd.ni_dvp);
	if (!error)
		vput(nd.ni_vp);
	vn_finished_write(mp);
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
int
rmdir(td, uap)
	struct thread *td;
	struct rmdir_args /* {
		char *path;
	} */ *uap;
{

	return (kern_rmdir(td, uap->path, UIO_USERSPACE));
}

int
kern_rmdir(struct thread *td, char *path, enum uio_seg pathseg)
{
	struct mount *mp;
	struct vnode *vp;
	int error;
	struct nameidata nd;

restart:
	bwillwrite();
	NDINIT(&nd, DELETE, LOCKPARENT | LOCKLEAF, pathseg, path, td);
	if ((error = namei(&nd)) != 0)
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
	if (vp->v_vflag & VV_ROOT) {
		error = EBUSY;
		goto out;
	}
#ifdef MAC
	error = mac_check_vnode_delete(td->td_ucred, nd.ni_dvp, vp,
	    &nd.ni_cnd);
	if (error)
		goto out;
#endif
	if (vn_start_write(nd.ni_dvp, &mp, V_NOWAIT) != 0) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vput(vp);
		if ((error = vn_start_write(NULL, &mp, V_XSLEEP | PCATCH)) != 0)
			return (error);
		goto restart;
	}
	VOP_LEASE(nd.ni_dvp, td, td->td_ucred, LEASE_WRITE);
	VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
	error = VOP_RMDIR(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
	vn_finished_write(mp);
out:
	NDFREE(&nd, NDF_ONLY_PNBUF);
	if (nd.ni_dvp == vp)
		vrele(nd.ni_dvp);
	else
		vput(nd.ni_dvp);
	vput(vp);
	ASSERT_VOP_UNLOCKED(nd.ni_dvp, "rmdir");
	ASSERT_VOP_UNLOCKED(nd.ni_vp, "rmdir");
	return (error);
}

#ifdef COMPAT_43
/*
 * Read a block of directory entries in a filesystem independent format.
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
ogetdirentries(td, uap)
	struct thread *td;
	register struct ogetdirentries_args /* {
		int fd;
		char *buf;
		u_int count;
		long *basep;
	} */ *uap;
{
	struct vnode *vp;
	struct file *fp;
	struct uio auio, kuio;
	struct iovec aiov, kiov;
	struct dirent *dp, *edp;
	caddr_t dirbuf;
	int error, eofflag, readcnt;
	long loff;

	/* XXX arbitrary sanity limit on `count'. */
	if (uap->count > 64 * 1024)
		return (EINVAL);
	if ((error = getvnode(td->td_proc->p_fd, uap->fd, &fp)) != 0)
		return (error);
	if ((fp->f_flag & FREAD) == 0) {
		fdrop(fp, td);
		return (EBADF);
	}
	vp = fp->f_vnode;
unionread:
	if (vp->v_type != VDIR) {
		fdrop(fp, td);
		return (EINVAL);
	}
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_td = td;
	auio.uio_resid = uap->count;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	loff = auio.uio_offset = fp->f_offset;
#ifdef MAC
	error = mac_check_vnode_readdir(td->td_ucred, vp);
	if (error) {
		VOP_UNLOCK(vp, 0, td);
		fdrop(fp, td);
		return (error);
	}
#endif
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
		kiov.iov_len = uap->count;
		MALLOC(dirbuf, caddr_t, uap->count, M_TEMP, M_WAITOK);
		kiov.iov_base = dirbuf;
		error = VOP_READDIR(vp, &kuio, fp->f_cred, &eofflag,
			    NULL, NULL);
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
	VOP_UNLOCK(vp, 0, td);
	if (error) {
		fdrop(fp, td);
		return (error);
	}
	if (uap->count == auio.uio_resid) {
		if (union_dircheckp) {
			error = union_dircheckp(td, &vp, fp);
			if (error == -1)
				goto unionread;
			if (error) {
				fdrop(fp, td);
				return (error);
			}
		}
		/*
		 * XXX We could delay dropping the lock above but
		 * union_dircheckp complicates things.
		 */
		vn_lock(vp, LK_EXCLUSIVE|LK_RETRY, td);
		if ((vp->v_vflag & VV_ROOT) &&
		    (vp->v_mount->mnt_flag & MNT_UNION)) {
			struct vnode *tvp = vp;
			vp = vp->v_mount->mnt_vnodecovered;
			VREF(vp);
			fp->f_vnode = vp;
			fp->f_data = vp;
			fp->f_offset = 0;
			vput(tvp);
			goto unionread;
		}
		VOP_UNLOCK(vp, 0, td);
	}
	error = copyout(&loff, uap->basep, sizeof(long));
	fdrop(fp, td);
	td->td_retval[0] = uap->count - auio.uio_resid;
	return (error);
}
#endif /* COMPAT_43 */

/*
 * Read a block of directory entries in a filesystem independent format.
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
getdirentries(td, uap)
	struct thread *td;
	register struct getdirentries_args /* {
		int fd;
		char *buf;
		u_int count;
		long *basep;
	} */ *uap;
{
	struct vnode *vp;
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	long loff;
	int error, eofflag;

	if ((error = getvnode(td->td_proc->p_fd, uap->fd, &fp)) != 0)
		return (error);
	if ((fp->f_flag & FREAD) == 0) {
		fdrop(fp, td);
		return (EBADF);
	}
	vp = fp->f_vnode;
unionread:
	if (vp->v_type != VDIR) {
		fdrop(fp, td);
		return (EINVAL);
	}
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_td = td;
	auio.uio_resid = uap->count;
	/* vn_lock(vp, LK_SHARED | LK_RETRY, td); */
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	loff = auio.uio_offset = fp->f_offset;
#ifdef MAC
	error = mac_check_vnode_readdir(td->td_ucred, vp);
	if (error == 0)
#endif
		error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag, NULL,
		    NULL);
	fp->f_offset = auio.uio_offset;
	VOP_UNLOCK(vp, 0, td);
	if (error) {
		fdrop(fp, td);
		return (error);
	}
	if (uap->count == auio.uio_resid) {
		if (union_dircheckp) {
			error = union_dircheckp(td, &vp, fp);
			if (error == -1)
				goto unionread;
			if (error) {
				fdrop(fp, td);
				return (error);
			}
		}
		/*
		 * XXX We could delay dropping the lock above but
		 * union_dircheckp complicates things.
		 */
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
		if ((vp->v_vflag & VV_ROOT) &&
		    (vp->v_mount->mnt_flag & MNT_UNION)) {
			struct vnode *tvp = vp;
			vp = vp->v_mount->mnt_vnodecovered;
			VREF(vp);
			fp->f_vnode = vp;
			fp->f_data = vp;
			fp->f_offset = 0;
			vput(tvp);
			goto unionread;
		}
		VOP_UNLOCK(vp, 0, td);
	}
	if (uap->basep != NULL) {
		error = copyout(&loff, uap->basep, sizeof(long));
	}
	td->td_retval[0] = uap->count - auio.uio_resid;
	fdrop(fp, td);
	return (error);
}
#ifndef _SYS_SYSPROTO_H_
struct getdents_args {
	int fd;
	char *buf;
	size_t count;
};
#endif
int
getdents(td, uap)
	struct thread *td;
	register struct getdents_args /* {
		int fd;
		char *buf;
		u_int count;
	} */ *uap;
{
	struct getdirentries_args ap;
	ap.fd = uap->fd;
	ap.buf = uap->buf;
	ap.count = uap->count;
	ap.basep = NULL;
	return (getdirentries(td, &ap));
}

/*
 * Set the mode mask for creation of filesystem nodes.
 *
 * MP SAFE
 */
#ifndef _SYS_SYSPROTO_H_
struct umask_args {
	int	newmask;
};
#endif
int
umask(td, uap)
	struct thread *td;
	struct umask_args /* {
		int newmask;
	} */ *uap;
{
	register struct filedesc *fdp;

	FILEDESC_LOCK_FAST(td->td_proc->p_fd);
	fdp = td->td_proc->p_fd;
	td->td_retval[0] = fdp->fd_cmask;
	fdp->fd_cmask = uap->newmask & ALLPERMS;
	FILEDESC_UNLOCK_FAST(td->td_proc->p_fd);
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
int
revoke(td, uap)
	struct thread *td;
	register struct revoke_args /* {
		char *path;
	} */ *uap;
{
	struct mount *mp;
	struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE, uap->path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	NDFREE(&nd, NDF_ONLY_PNBUF);
	if (vp->v_type != VCHR) {
		vput(vp);
		return (EINVAL);
	}
#ifdef MAC
	error = mac_check_vnode_revoke(td->td_ucred, vp);
	if (error) {
		vput(vp);
		return (error);
	}
#endif
	error = VOP_GETATTR(vp, &vattr, td->td_ucred, td);
	if (error) {
		vput(vp);
		return (error);
	}
	VOP_UNLOCK(vp, 0, td);
	if (td->td_ucred->cr_uid != vattr.va_uid) {
		error = suser_cred(td->td_ucred, SUSER_ALLOWJAIL);
		if (error)
			goto out;
	}
	if ((error = vn_start_write(vp, &mp, V_WAIT | PCATCH)) != 0)
		goto out;
	if (vcount(vp) > 1)
		VOP_REVOKE(vp, REVOKEALL);
	vn_finished_write(mp);
out:
	vrele(vp);
	return (error);
}

/*
 * Convert a user file descriptor to a kernel file entry.
 * A reference on the file entry is held upon returning.
 */
int
getvnode(fdp, fd, fpp)
	struct filedesc *fdp;
	int fd;
	struct file **fpp;
{
	int error;
	struct file *fp;

	fp = NULL;
	if (fdp == NULL)
		error = EBADF;
	else {
		FILEDESC_LOCK(fdp);
		if ((u_int)fd >= fdp->fd_nfiles ||
		    (fp = fdp->fd_ofiles[fd]) == NULL)
			error = EBADF;
		else if (fp->f_vnode == NULL) {
			fp = NULL;
			error = EINVAL;
		} else {
			fhold(fp);
			error = 0;
		}
		FILEDESC_UNLOCK(fdp);
	}
	*fpp = fp;
	return (error);
}

/*
 * Get (NFS) file handle
 */
#ifndef _SYS_SYSPROTO_H_
struct lgetfh_args {
	char	*fname;
	fhandle_t *fhp;
};
#endif
int
lgetfh(td, uap)
	struct thread *td;
	register struct lgetfh_args *uap;
{
	struct nameidata nd;
	fhandle_t fh;
	register struct vnode *vp;
	int error;

	error = suser(td);
	if (error)
		return (error);
	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF, UIO_USERSPACE, uap->fname, td);
	error = namei(&nd);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;
	bzero(&fh, sizeof(fh));
	fh.fh_fsid = vp->v_mount->mnt_stat.f_fsid;
	error = VFS_VPTOFH(vp, &fh.fh_fid);
	vput(vp);
	if (error)
		return (error);
	error = copyout(&fh, uap->fhp, sizeof (fh));
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct getfh_args {
	char	*fname;
	fhandle_t *fhp;
};
#endif
int
getfh(td, uap)
	struct thread *td;
	register struct getfh_args *uap;
{
	struct nameidata nd;
	fhandle_t fh;
	register struct vnode *vp;
	int error;

	error = suser(td);
	if (error)
		return (error);
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE, uap->fname, td);
	error = namei(&nd);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;
	bzero(&fh, sizeof(fh));
	fh.fh_fsid = vp->v_mount->mnt_stat.f_fsid;
	error = VFS_VPTOFH(vp, &fh.fh_fid);
	vput(vp);
	if (error)
		return (error);
	error = copyout(&fh, uap->fhp, sizeof (fh));
	return (error);
}

/*
 * syscall for the rpc.lockd to use to translate a NFS file handle into
 * an open descriptor.
 *
 * warning: do not remove the suser() call or this becomes one giant
 * security hole.
 */
#ifndef _SYS_SYSPROTO_H_
struct fhopen_args {
	const struct fhandle *u_fhp;
	int flags;
};
#endif
int
fhopen(td, uap)
	struct thread *td;
	struct fhopen_args /* {
		const struct fhandle *u_fhp;
		int flags;
	} */ *uap;
{
	struct proc *p = td->td_proc;
	struct mount *mp;
	struct vnode *vp;
	struct fhandle fhp;
	struct vattr vat;
	struct vattr *vap = &vat;
	struct flock lf;
	struct file *fp;
	register struct filedesc *fdp = p->p_fd;
	int fmode, mode, error, type;
	struct file *nfp;
	int indx;

	error = suser(td);
	if (error)
		return (error);
	fmode = FFLAGS(uap->flags);
	/* why not allow a non-read/write open for our lockd? */
	if (((fmode & (FREAD | FWRITE)) == 0) || (fmode & O_CREAT))
		return (EINVAL);
	error = copyin(uap->u_fhp, &fhp, sizeof(fhp));
	if (error)
		return(error);
	/* find the mount point */
	mp = vfs_getvfs(&fhp.fh_fsid);
	if (mp == NULL)
		return (ESTALE);
	/* now give me my vnode, it gets returned to me locked */
	error = VFS_FHTOVP(mp, &fhp.fh_fid, &vp);
	if (error)
		return (error);
	/*
	 * from now on we have to make sure not
	 * to forget about the vnode
	 * any error that causes an abort must vput(vp)
	 * just set error = err and 'goto bad;'.
	 */

	/*
	 * from vn_open
	 */
	if (vp->v_type == VLNK) {
		error = EMLINK;
		goto bad;
	}
	if (vp->v_type == VSOCK) {
		error = EOPNOTSUPP;
		goto bad;
	}
	mode = 0;
	if (fmode & (FWRITE | O_TRUNC)) {
		if (vp->v_type == VDIR) {
			error = EISDIR;
			goto bad;
		}
		error = vn_writechk(vp);
		if (error)
			goto bad;
		mode |= VWRITE;
	}
	if (fmode & FREAD)
		mode |= VREAD;
	if (fmode & O_APPEND)
		mode |= VAPPEND;
#ifdef MAC
	error = mac_check_vnode_open(td->td_ucred, vp, mode);
	if (error)
		goto bad;
#endif
	if (mode) {
		error = VOP_ACCESS(vp, mode, td->td_ucred, td);
		if (error)
			goto bad;
	}
	if (fmode & O_TRUNC) {
		VOP_UNLOCK(vp, 0, td);				/* XXX */
		if ((error = vn_start_write(NULL, &mp, V_WAIT | PCATCH)) != 0) {
			vrele(vp);
			return (error);
		}
		VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);	/* XXX */
#ifdef MAC
		/*
		 * We don't yet have fp->f_cred, so use td->td_ucred, which
		 * should be right.
		 */
		error = mac_check_vnode_write(td->td_ucred, td->td_ucred, vp);
		if (error == 0) {
#endif
			VATTR_NULL(vap);
			vap->va_size = 0;
			error = VOP_SETATTR(vp, vap, td->td_ucred, td);
#ifdef MAC
		}
#endif
		vn_finished_write(mp);
		if (error)
			goto bad;
	}
	error = VOP_OPEN(vp, fmode, td->td_ucred, td, -1);
	if (error)
		goto bad;
	/*
	 * Make sure that a VM object is created for VMIO support.
	 */
	if (vn_canvmio(vp) == TRUE) {
		if ((error = vfs_object_create(vp, td, td->td_ucred)) != 0)
			goto bad;
	}
	if (fmode & FWRITE)
		vp->v_writecount++;

	/*
	 * end of vn_open code
	 */

	if ((error = falloc(td, &nfp, &indx)) != 0) {
		if (fmode & FWRITE)
			vp->v_writecount--;
		goto bad;
	}
	/* An extra reference on `nfp' has been held for us by falloc(). */
	fp = nfp;

	nfp->f_vnode = vp;
	nfp->f_data = vp;
	nfp->f_flag = fmode & FMASK;
	nfp->f_ops = &vnops;
	nfp->f_type = DTYPE_VNODE;
	if (fmode & (O_EXLOCK | O_SHLOCK)) {
		lf.l_whence = SEEK_SET;
		lf.l_start = 0;
		lf.l_len = 0;
		if (fmode & O_EXLOCK)
			lf.l_type = F_WRLCK;
		else
			lf.l_type = F_RDLCK;
		type = F_FLOCK;
		if ((fmode & FNONBLOCK) == 0)
			type |= F_WAIT;
		VOP_UNLOCK(vp, 0, td);
		if ((error = VOP_ADVLOCK(vp, (caddr_t)fp, F_SETLK, &lf,
			    type)) != 0) {
			/*
			 * The lock request failed.  Normally close the
			 * descriptor but handle the case where someone might
			 * have dup()d or close()d it when we weren't looking.
			 */
			fdclose(fdp, fp, indx, td);

			/*
			 * release our private reference
			 */
			fdrop(fp, td);
			return(error);
		}
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
		fp->f_flag |= FHASLOCK;
	}
	if ((vp->v_type == VREG) && (VOP_GETVOBJECT(vp, NULL) != 0))
		vfs_object_create(vp, td, td->td_ucred);

	VOP_UNLOCK(vp, 0, td);
	fdrop(fp, td);
	td->td_retval[0] = indx;
	return (0);

bad:
	vput(vp);
	return (error);
}

/*
 * Stat an (NFS) file handle.
 */
#ifndef _SYS_SYSPROTO_H_
struct fhstat_args {
	struct fhandle *u_fhp;
	struct stat *sb;
};
#endif
int
fhstat(td, uap)
	struct thread *td;
	register struct fhstat_args /* {
		struct fhandle *u_fhp;
		struct stat *sb;
	} */ *uap;
{
	struct stat sb;
	fhandle_t fh;
	struct mount *mp;
	struct vnode *vp;
	int error;

	error = suser(td);
	if (error)
		return (error);
	error = copyin(uap->u_fhp, &fh, sizeof(fhandle_t));
	if (error)
		return (error);
	if ((mp = vfs_getvfs(&fh.fh_fsid)) == NULL)
		return (ESTALE);
	if ((error = VFS_FHTOVP(mp, &fh.fh_fid, &vp)))
		return (error);
	error = vn_stat(vp, &sb, td->td_ucred, NOCRED, td);
	vput(vp);
	if (error)
		return (error);
	error = copyout(&sb, uap->sb, sizeof(sb));
	return (error);
}

/*
 * Implement fstatfs() for (NFS) file handles.
 */
#ifndef _SYS_SYSPROTO_H_
struct fhstatfs_args {
	struct fhandle *u_fhp;
	struct statfs *buf;
};
#endif
int
fhstatfs(td, uap)
	struct thread *td;
	struct fhstatfs_args /* {
		struct fhandle *u_fhp;
		struct statfs *buf;
	} */ *uap;
{
	struct statfs sf;
	fhandle_t fh;
	int error;

	if ((error = copyin(uap->u_fhp, &fh, sizeof(fhandle_t))) != 0)
		return (error);
	error = kern_fhstatfs(td, fh, &sf);
	if (error == 0)
		error = copyout(&sf, uap->buf, sizeof(sf));
	return (error);
}

int
kern_fhstatfs(struct thread *td, fhandle_t fh, struct statfs *buf)
{
	struct statfs *sp;
	struct mount *mp;
	struct vnode *vp;
	int error;

	error = suser(td);
	if (error)
		return (error);
	if ((mp = vfs_getvfs(&fh.fh_fsid)) == NULL)
		return (ESTALE);
	if ((error = VFS_FHTOVP(mp, &fh.fh_fid, &vp)))
		return (error);
	mp = vp->v_mount;
	sp = &mp->mnt_stat;
	vput(vp);
#ifdef MAC
	error = mac_check_mount_stat(td->td_ucred, mp);
	if (error)
		return (error);
#endif
	/*
	 * Set these in case the underlying filesystem fails to do so.
	 */
	sp->f_version = STATFS_VERSION;
	sp->f_namemax = NAME_MAX;
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	if ((error = VFS_STATFS(mp, sp, td)) != 0)
		return (error);
	*buf = *sp;
	return (0);
}

/*
 * Syscall to push extended attribute configuration information into the
 * VFS.  Accepts a path, which it converts to a mountpoint, as well as
 * a command (int cmd), and attribute name and misc data.  For now, the
 * attribute name is left in userspace for consumption by the VFS_op.
 * It will probably be changed to be copied into sysspace by the
 * syscall in the future, once issues with various consumers of the
 * attribute code have raised their hands.
 *
 * Currently this is used only by UFS Extended Attributes.
 */
int
extattrctl(td, uap)
	struct thread *td;
	struct extattrctl_args /* {
		const char *path;
		int cmd;
		const char *filename;
		int attrnamespace;
		const char *attrname;
	} */ *uap;
{
	struct vnode *filename_vp;
	struct nameidata nd;
	struct mount *mp, *mp_writable;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	/*
	 * uap->attrname is not always defined.  We check again later when we
	 * invoke the VFS call so as to pass in NULL there if needed.
	 */
	if (uap->attrname != NULL) {
		error = copyinstr(uap->attrname, attrname, EXTATTR_MAXNAMELEN,
		    NULL);
		if (error)
			return (error);
	}

	/*
	 * uap->filename is not always defined.  If it is, grab a vnode lock,
	 * which VFS_EXTATTRCTL() will later release.
	 */
	filename_vp = NULL;
	if (uap->filename != NULL) {
		NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
		    uap->filename, td);
		error = namei(&nd);
		if (error)
			return (error);
		filename_vp = nd.ni_vp;
		NDFREE(&nd, NDF_NO_VP_RELE | NDF_NO_VP_UNLOCK);
	}

	/* uap->path is always defined. */
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, uap->path, td);
	error = namei(&nd);
	if (error) {
		if (filename_vp != NULL)
			vput(filename_vp);
		return (error);
	}
	mp = nd.ni_vp->v_mount;
	error = vn_start_write(nd.ni_vp, &mp_writable, V_WAIT | PCATCH);
	NDFREE(&nd, 0);
	if (error) {
		if (filename_vp != NULL)
			vput(filename_vp);
		return (error);
	}

	error = VFS_EXTATTRCTL(mp, uap->cmd, filename_vp, uap->attrnamespace,
	    uap->attrname != NULL ? attrname : NULL, td);

	vn_finished_write(mp_writable);
	/*
	 * VFS_EXTATTRCTL will have unlocked, but not de-ref'd,
	 * filename_vp, so vrele it if it is defined.
	 */
	if (filename_vp != NULL)
		vrele(filename_vp);
	return (error);
}

/*-
 * Set a named extended attribute on a file or directory
 *
 * Arguments: unlocked vnode "vp", attribute namespace "attrnamespace",
 *            kernelspace string pointer "attrname", userspace buffer
 *            pointer "data", buffer length "nbytes", thread "td".
 * Returns: 0 on success, an error number otherwise
 * Locks: none
 * References: vp must be a valid reference for the duration of the call
 */
static int
extattr_set_vp(struct vnode *vp, int attrnamespace, const char *attrname,
    void *data, size_t nbytes, struct thread *td)
{
	struct mount *mp;
	struct uio auio;
	struct iovec aiov;
	ssize_t cnt;
	int error;

	error = vn_start_write(vp, &mp, V_WAIT | PCATCH);
	if (error)
		return (error);
	VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);

	aiov.iov_base = data;
	aiov.iov_len = nbytes;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = 0;
	if (nbytes > INT_MAX) {
		error = EINVAL;
		goto done;
	}
	auio.uio_resid = nbytes;
	auio.uio_rw = UIO_WRITE;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_td = td;
	cnt = nbytes;

#ifdef MAC
	error = mac_check_vnode_setextattr(td->td_ucred, vp, attrnamespace,
	    attrname, &auio);
	if (error)
		goto done;
#endif

	error = VOP_SETEXTATTR(vp, attrnamespace, attrname, &auio,
	    td->td_ucred, td);
	cnt -= auio.uio_resid;
	td->td_retval[0] = cnt;

done:
	VOP_UNLOCK(vp, 0, td);
	vn_finished_write(mp);
	return (error);
}

int
extattr_set_fd(td, uap)
	struct thread *td;
	struct extattr_set_fd_args /* {
		int fd;
		int attrnamespace;
		const char *attrname;
		void *data;
		size_t nbytes;
	} */ *uap;
{
	struct file *fp;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(uap->attrname, attrname, EXTATTR_MAXNAMELEN, NULL);
	if (error)
		return (error);

	error = getvnode(td->td_proc->p_fd, uap->fd, &fp);
	if (error)
		return (error);

	error = extattr_set_vp(fp->f_vnode, uap->attrnamespace,
	    attrname, uap->data, uap->nbytes, td);
	fdrop(fp, td);

	return (error);
}

int
extattr_set_file(td, uap)
	struct thread *td;
	struct extattr_set_file_args /* {
		const char *path;
		int attrnamespace;
		const char *attrname;
		void *data;
		size_t nbytes;
	} */ *uap;
{
	struct nameidata nd;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(uap->attrname, attrname, EXTATTR_MAXNAMELEN, NULL);
	if (error)
		return (error);

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, uap->path, td);
	error = namei(&nd);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	error = extattr_set_vp(nd.ni_vp, uap->attrnamespace, attrname,
	    uap->data, uap->nbytes, td);

	vrele(nd.ni_vp);
	return (error);
}

int
extattr_set_link(td, uap)
	struct thread *td;
	struct extattr_set_link_args /* {
		const char *path;
		int attrnamespace;
		const char *attrname;
		void *data;
		size_t nbytes;
	} */ *uap;
{
	struct nameidata nd;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(uap->attrname, attrname, EXTATTR_MAXNAMELEN, NULL);
	if (error)
		return (error);

	NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_USERSPACE, uap->path, td);
	error = namei(&nd);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	error = extattr_set_vp(nd.ni_vp, uap->attrnamespace, attrname,
	    uap->data, uap->nbytes, td);

	vrele(nd.ni_vp);
	return (error);
}

/*-
 * Get a named extended attribute on a file or directory
 *
 * Arguments: unlocked vnode "vp", attribute namespace "attrnamespace",
 *            kernelspace string pointer "attrname", userspace buffer
 *            pointer "data", buffer length "nbytes", thread "td".
 * Returns: 0 on success, an error number otherwise
 * Locks: none
 * References: vp must be a valid reference for the duration of the call
 */
static int
extattr_get_vp(struct vnode *vp, int attrnamespace, const char *attrname,
    void *data, size_t nbytes, struct thread *td)
{
	struct uio auio, *auiop;
	struct iovec aiov;
	ssize_t cnt;
	size_t size, *sizep;
	int error;

	VOP_LEASE(vp, td, td->td_ucred, LEASE_READ);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);

	/*
	 * Slightly unusual semantics: if the user provides a NULL data
	 * pointer, they don't want to receive the data, just the
	 * maximum read length.
	 */
	auiop = NULL;
	sizep = NULL;
	cnt = 0;
	if (data != NULL) {
		aiov.iov_base = data;
		aiov.iov_len = nbytes;
		auio.uio_iov = &aiov;
		auio.uio_offset = 0;
		if (nbytes > INT_MAX) {
			error = EINVAL;
			goto done;
		}
		auio.uio_resid = nbytes;
		auio.uio_rw = UIO_READ;
		auio.uio_segflg = UIO_USERSPACE;
		auio.uio_td = td;
		auiop = &auio;
		cnt = nbytes;
	} else
		sizep = &size;

#ifdef MAC
	error = mac_check_vnode_getextattr(td->td_ucred, vp, attrnamespace,
	    attrname, &auio);
	if (error)
		goto done;
#endif

	error = VOP_GETEXTATTR(vp, attrnamespace, attrname, auiop, sizep,
	    td->td_ucred, td);

	if (auiop != NULL) {
		cnt -= auio.uio_resid;
		td->td_retval[0] = cnt;
	} else
		td->td_retval[0] = size;

done:
	VOP_UNLOCK(vp, 0, td);
	return (error);
}

int
extattr_get_fd(td, uap)
	struct thread *td;
	struct extattr_get_fd_args /* {
		int fd;
		int attrnamespace;
		const char *attrname;
		void *data;
		size_t nbytes;
	} */ *uap;
{
	struct file *fp;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(uap->attrname, attrname, EXTATTR_MAXNAMELEN, NULL);
	if (error)
		return (error);

	error = getvnode(td->td_proc->p_fd, uap->fd, &fp);
	if (error)
		return (error);

	error = extattr_get_vp(fp->f_vnode, uap->attrnamespace,
	    attrname, uap->data, uap->nbytes, td);

	fdrop(fp, td);
	return (error);
}

int
extattr_get_file(td, uap)
	struct thread *td;
	struct extattr_get_file_args /* {
		const char *path;
		int attrnamespace;
		const char *attrname;
		void *data;
		size_t nbytes;
	} */ *uap;
{
	struct nameidata nd;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(uap->attrname, attrname, EXTATTR_MAXNAMELEN, NULL);
	if (error)
		return (error);

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, uap->path, td);
	error = namei(&nd);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	error = extattr_get_vp(nd.ni_vp, uap->attrnamespace, attrname,
	    uap->data, uap->nbytes, td);

	vrele(nd.ni_vp);
	return (error);
}

int
extattr_get_link(td, uap)
	struct thread *td;
	struct extattr_get_link_args /* {
		const char *path;
		int attrnamespace;
		const char *attrname;
		void *data;
		size_t nbytes;
	} */ *uap;
{
	struct nameidata nd;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(uap->attrname, attrname, EXTATTR_MAXNAMELEN, NULL);
	if (error)
		return (error);

	NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_USERSPACE, uap->path, td);
	error = namei(&nd);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	error = extattr_get_vp(nd.ni_vp, uap->attrnamespace, attrname,
	    uap->data, uap->nbytes, td);

	vrele(nd.ni_vp);
	return (error);
}

/*
 * extattr_delete_vp(): Delete a named extended attribute on a file or
 *                      directory
 *
 * Arguments: unlocked vnode "vp", attribute namespace "attrnamespace",
 *            kernelspace string pointer "attrname", proc "p"
 * Returns: 0 on success, an error number otherwise
 * Locks: none
 * References: vp must be a valid reference for the duration of the call
 */
static int
extattr_delete_vp(struct vnode *vp, int attrnamespace, const char *attrname,
    struct thread *td)
{
	struct mount *mp;
	int error;

	error = vn_start_write(vp, &mp, V_WAIT | PCATCH);
	if (error)
		return (error);
	VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);

#ifdef MAC
	error = mac_check_vnode_deleteextattr(td->td_ucred, vp, attrnamespace,
	    attrname);
	if (error)
		goto done;
#endif

	error = VOP_DELETEEXTATTR(vp, attrnamespace, attrname, td->td_ucred,
	    td);
	if (error == EOPNOTSUPP)
		error = VOP_SETEXTATTR(vp, attrnamespace, attrname, NULL,
		    td->td_ucred, td);
#ifdef MAC
done:
#endif
	VOP_UNLOCK(vp, 0, td);
	vn_finished_write(mp);
	return (error);
}

int
extattr_delete_fd(td, uap)
	struct thread *td;
	struct extattr_delete_fd_args /* {
		int fd;
		int attrnamespace;
		const char *attrname;
	} */ *uap;
{
	struct file *fp;
	struct vnode *vp;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(uap->attrname, attrname, EXTATTR_MAXNAMELEN, NULL);
	if (error)
		return (error);

	error = getvnode(td->td_proc->p_fd, uap->fd, &fp);
	if (error)
		return (error);
	vp = fp->f_vnode;

	error = extattr_delete_vp(vp, uap->attrnamespace, attrname, td);
	fdrop(fp, td);
	return (error);
}

int
extattr_delete_file(td, uap)
	struct thread *td;
	struct extattr_delete_file_args /* {
		const char *path;
		int attrnamespace;
		const char *attrname;
	} */ *uap;
{
	struct nameidata nd;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(uap->attrname, attrname, EXTATTR_MAXNAMELEN, NULL);
	if (error)
		return(error);

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, uap->path, td);
	error = namei(&nd);
	if (error)
		return(error);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	error = extattr_delete_vp(nd.ni_vp, uap->attrnamespace, attrname, td);
	vrele(nd.ni_vp);
	return(error);
}

int
extattr_delete_link(td, uap)
	struct thread *td;
	struct extattr_delete_link_args /* {
		const char *path;
		int attrnamespace;
		const char *attrname;
	} */ *uap;
{
	struct nameidata nd;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(uap->attrname, attrname, EXTATTR_MAXNAMELEN, NULL);
	if (error)
		return(error);

	NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_USERSPACE, uap->path, td);
	error = namei(&nd);
	if (error)
		return(error);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	error = extattr_delete_vp(nd.ni_vp, uap->attrnamespace, attrname, td);
	vrele(nd.ni_vp);
	return(error);
}

/*-
 * Retrieve a list of extended attributes on a file or directory.
 *
 * Arguments: unlocked vnode "vp", attribute namespace 'attrnamespace",
 *            userspace buffer pointer "data", buffer length "nbytes",
 *            thread "td".
 * Returns: 0 on success, an error number otherwise
 * Locks: none
 * References: vp must be a valid reference for the duration of the call
 */
static int
extattr_list_vp(struct vnode *vp, int attrnamespace, void *data,
    size_t nbytes, struct thread *td)
{
	struct uio auio, *auiop;
	size_t size, *sizep;
	struct iovec aiov;
	ssize_t cnt;
	int error;

	VOP_LEASE(vp, td, td->td_ucred, LEASE_READ);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);

	auiop = NULL;
	sizep = NULL;
	cnt = 0;
	if (data != NULL) {
		aiov.iov_base = data;
		aiov.iov_len = nbytes;
		auio.uio_iov = &aiov;
		auio.uio_offset = 0;
		if (nbytes > INT_MAX) {
			error = EINVAL;
			goto done;
		}
		auio.uio_resid = nbytes;
		auio.uio_rw = UIO_READ;
		auio.uio_segflg = UIO_USERSPACE;
		auio.uio_td = td;
		auiop = &auio;
		cnt = nbytes;
	} else
		sizep = &size;

#ifdef MAC
	error = mac_check_vnode_listextattr(td->td_ucred, vp, attrnamespace);
	if (error)
		goto done;
#endif

	error = VOP_LISTEXTATTR(vp, attrnamespace, auiop, sizep,
	    td->td_ucred, td);

	if (auiop != NULL) {
		cnt -= auio.uio_resid;
		td->td_retval[0] = cnt;
	} else
		td->td_retval[0] = size;

done:
	VOP_UNLOCK(vp, 0, td);
	return (error);
}


int
extattr_list_fd(td, uap)
	struct thread *td;
	struct extattr_list_fd_args /* {
		int fd;
		int attrnamespace;
		void *data;
		size_t nbytes;
	} */ *uap;
{
	struct file *fp;
	int error;

	error = getvnode(td->td_proc->p_fd, uap->fd, &fp);
	if (error)
		return (error);

	error = extattr_list_vp(fp->f_vnode, uap->attrnamespace, uap->data,
	    uap->nbytes, td);

	fdrop(fp, td);
	return (error);
}

int
extattr_list_file(td, uap)
	struct thread*td;
	struct extattr_list_file_args /* {
		const char *path;
		int attrnamespace;
		void *data;
		size_t nbytes;
	} */ *uap;
{
	struct nameidata nd;
	int error;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, uap->path, td);
	error = namei(&nd);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	error = extattr_list_vp(nd.ni_vp, uap->attrnamespace, uap->data,
	    uap->nbytes, td);

	vrele(nd.ni_vp);
	return (error);
}

int
extattr_list_link(td, uap)
	struct thread*td;
	struct extattr_list_link_args /* {
		const char *path;
		int attrnamespace;
		void *data;
		size_t nbytes;
	} */ *uap;
{
	struct nameidata nd;
	int error;

	NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_USERSPACE, uap->path, td);
	error = namei(&nd);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	error = extattr_list_vp(nd.ni_vp, uap->attrnamespace, uap->data,
	    uap->nbytes, td);

	vrele(nd.ni_vp);
	return (error);
}
