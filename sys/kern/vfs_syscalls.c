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
#include "opt_ktrace.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/sysent.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/sysproto.h>
#include <sys/namei.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filio.h>
#include <sys/limits.h>
#include <sys/linker.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/dirent.h>
#include <sys/jail.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <machine/stdarg.h>

#include <security/audit/audit.h>
#include <security/mac/mac_framework.h>

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

/*
 * The module initialization routine for POSIX asynchronous I/O will
 * set this to the version of AIO that it implements.  (Zero means
 * that it is not implemented.)  This value is used here by pathconf()
 * and in kern_descrip.c by fpathconf().
 */
int async_io_version;

#ifdef DEBUG
static int syncprt = 0;
SYSCTL_INT(_debug, OID_AUTO, syncprt, CTLFLAG_RW, &syncprt, 0, "");
#endif

/*
 * Sync each mounted filesystem.
 */
#ifndef _SYS_SYSPROTO_H_
struct sync_args {
	int     dummy;
};
#endif
/* ARGSUSED */
int
sync(td, uap)
	struct thread *td;
	struct sync_args *uap;
{
	struct mount *mp, *nmp;
	int vfslocked;

	mtx_lock(&mountlist_mtx);
	for (mp = TAILQ_FIRST(&mountlist); mp != NULL; mp = nmp) {
		if (vfs_busy(mp, LK_NOWAIT, &mountlist_mtx, td)) {
			nmp = TAILQ_NEXT(mp, mnt_list);
			continue;
		}
		vfslocked = VFS_LOCK_GIANT(mp);
		if ((mp->mnt_flag & MNT_RDONLY) == 0 &&
		    vn_start_write(NULL, &mp, V_NOWAIT) == 0) {
			MNT_ILOCK(mp);
			mp->mnt_noasync++;
			mp->mnt_kern_flag &= ~MNTK_ASYNC;
			MNT_IUNLOCK(mp);
			vfs_msync(mp, MNT_NOWAIT);
			VFS_SYNC(mp, MNT_NOWAIT, td);
			MNT_ILOCK(mp);
			mp->mnt_noasync--;
			if ((mp->mnt_flag & MNT_ASYNC) != 0 &&
			    mp->mnt_noasync == 0)
				mp->mnt_kern_flag |= MNTK_ASYNC;
			MNT_IUNLOCK(mp);
			vn_finished_write(mp);
		}
		VFS_UNLOCK_GIANT(vfslocked);
		mtx_lock(&mountlist_mtx);
		nmp = TAILQ_NEXT(mp, mnt_list);
		vfs_unbusy(mp, td);
	}
	mtx_unlock(&mountlist_mtx);
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
	struct mount *mp;
	int vfslocked;
	int error;
	struct nameidata nd;

	AUDIT_ARG(cmd, uap->cmd);
	AUDIT_ARG(uid, uap->uid);
	if (jailed(td->td_ucred) && !prison_quotas)
		return (EPERM);
	NDINIT(&nd, LOOKUP, FOLLOW | MPSAFE | AUDITVNODE1,
	   UIO_USERSPACE, uap->path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	vfslocked = NDHASGIANT(&nd);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	mp = nd.ni_vp->v_mount;
	if ((error = vfs_busy(mp, 0, NULL, td))) {
		vrele(nd.ni_vp);
		VFS_UNLOCK_GIANT(vfslocked);
		return (error);
	}
	vrele(nd.ni_vp);
	error = VFS_QUOTACTL(mp, uap->cmd, uap->uid, uap->arg, td);
	vfs_unbusy(mp, td);
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

/*
 * Used by statfs conversion routines to scale the block size up if
 * necessary so that all of the block counts are <= 'max_size'.  Note
 * that 'max_size' should be a bitmask, i.e. 2^n - 1 for some non-zero
 * value of 'n'.
 */
void
statfs_scale_blocks(struct statfs *sf, long max_size)
{
	uint64_t count;
	int shift;

	KASSERT(powerof2(max_size + 1), ("%s: invalid max_size", __func__));

	/*
	 * Attempt to scale the block counts to give a more accurate
	 * overview to userland of the ratio of free space to used
	 * space.  To do this, find the largest block count and compute
	 * a divisor that lets it fit into a signed integer <= max_size.
	 */
	if (sf->f_bavail < 0)
		count = -sf->f_bavail;
	else
		count = sf->f_bavail;
	count = MAX(sf->f_blocks, MAX(sf->f_bfree, count));
	if (count <= max_size)
		return;

	count >>= flsl(max_size);
	shift = 0;
	while (count > 0) {
		shift++;
		count >>=1;
	}

	sf->f_bsize <<= shift;
	sf->f_blocks >>= shift;
	sf->f_bfree >>= shift;
	sf->f_bavail >>= shift;
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
	int vfslocked;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | MPSAFE | AUDITVNODE1,
	    pathseg, path, td);
	error = namei(&nd);
	if (error)
		return (error);
	vfslocked = NDHASGIANT(&nd);
	mp = nd.ni_vp->v_mount;
	vfs_ref(mp);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vput(nd.ni_vp);
#ifdef MAC
	error = mac_mount_check_stat(td->td_ucred, mp);
	if (error)
		goto out;
#endif
	/*
	 * Set these in case the underlying filesystem fails to do so.
	 */
	sp = &mp->mnt_stat;
	sp->f_version = STATFS_VERSION;
	sp->f_namemax = NAME_MAX;
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	error = VFS_STATFS(mp, sp, td);
	if (error)
		goto out;
	if (priv_check(td, PRIV_VFS_GENERATION)) {
		bcopy(sp, &sb, sizeof(sb));
		sb.f_fsid.val[0] = sb.f_fsid.val[1] = 0;
		prison_enforce_statfs(td->td_ucred, mp, &sb);
		sp = &sb;
	}
	*buf = *sp;
out:
	vfs_rel(mp);
	VFS_UNLOCK_GIANT(vfslocked);
	if (mtx_owned(&Giant))
		printf("statfs(%d): %s: %d\n", vfslocked, path, error);
	return (error);
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
	int vfslocked;
	struct vnode *vp;
	int error;

	AUDIT_ARG(fd, fd);
	error = getvnode(td->td_proc->p_fd, fd, &fp);
	if (error)
		return (error);
	vp = fp->f_vnode;
	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
#ifdef AUDIT
	AUDIT_ARG(vnode, vp, ARG_VNODE1);
#endif
	mp = vp->v_mount;
	if (mp)
		vfs_ref(mp);
	VOP_UNLOCK(vp, 0);
	fdrop(fp, td);
	if (vp->v_iflag & VI_DOOMED) {
		error = EBADF;
		goto out;
	}
#ifdef MAC
	error = mac_mount_check_stat(td->td_ucred, mp);
	if (error)
		goto out;
#endif
	/*
	 * Set these in case the underlying filesystem fails to do so.
	 */
	sp = &mp->mnt_stat;
	sp->f_version = STATFS_VERSION;
	sp->f_namemax = NAME_MAX;
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	error = VFS_STATFS(mp, sp, td);
	if (error)
		goto out;
	if (priv_check(td, PRIV_VFS_GENERATION)) {
		bcopy(sp, &sb, sizeof(sb));
		sb.f_fsid.val[0] = sb.f_fsid.val[1] = 0;
		prison_enforce_statfs(td->td_ucred, mp, &sb);
		sp = &sb;
	}
	*buf = *sp;
out:
	if (mp)
		vfs_rel(mp);
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
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

	return (kern_getfsstat(td, &uap->buf, uap->bufsize, UIO_USERSPACE,
	    uap->flags));
}

/*
 * If (bufsize > 0 && bufseg == UIO_SYSSPACE)
 * 	The caller is responsible for freeing memory which will be allocated
 *	in '*buf'.
 */
int
kern_getfsstat(struct thread *td, struct statfs **buf, size_t bufsize,
    enum uio_seg bufseg, int flags)
{
	struct mount *mp, *nmp;
	struct statfs *sfsp, *sp, sb;
	size_t count, maxcount;
	int vfslocked;
	int error;

	maxcount = bufsize / sizeof(struct statfs);
	if (bufsize == 0)
		sfsp = NULL;
	else if (bufseg == UIO_USERSPACE)
		sfsp = *buf;
	else /* if (bufseg == UIO_SYSSPACE) */ {
		count = 0;
		mtx_lock(&mountlist_mtx);
		TAILQ_FOREACH(mp, &mountlist, mnt_list) {
			count++;
		}
		mtx_unlock(&mountlist_mtx);
		if (maxcount > count)
			maxcount = count;
		sfsp = *buf = malloc(maxcount * sizeof(struct statfs), M_TEMP,
		    M_WAITOK);
	}
	count = 0;
	mtx_lock(&mountlist_mtx);
	for (mp = TAILQ_FIRST(&mountlist); mp != NULL; mp = nmp) {
		if (prison_canseemount(td->td_ucred, mp) != 0) {
			nmp = TAILQ_NEXT(mp, mnt_list);
			continue;
		}
#ifdef MAC
		if (mac_mount_check_stat(td->td_ucred, mp) != 0) {
			nmp = TAILQ_NEXT(mp, mnt_list);
			continue;
		}
#endif
		if (vfs_busy(mp, LK_NOWAIT, &mountlist_mtx, td)) {
			nmp = TAILQ_NEXT(mp, mnt_list);
			continue;
		}
		vfslocked = VFS_LOCK_GIANT(mp);
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
			if (((flags & (MNT_LAZY|MNT_NOWAIT)) == 0 ||
			    (flags & MNT_WAIT)) &&
			    (error = VFS_STATFS(mp, sp, td))) {
				VFS_UNLOCK_GIANT(vfslocked);
				mtx_lock(&mountlist_mtx);
				nmp = TAILQ_NEXT(mp, mnt_list);
				vfs_unbusy(mp, td);
				continue;
			}
			if (priv_check(td, PRIV_VFS_GENERATION)) {
				bcopy(sp, &sb, sizeof(sb));
				sb.f_fsid.val[0] = sb.f_fsid.val[1] = 0;
				prison_enforce_statfs(td->td_ucred, mp, &sb);
				sp = &sb;
			}
			if (bufseg == UIO_SYSSPACE)
				bcopy(sp, sfsp, sizeof(*sp));
			else /* if (bufseg == UIO_USERSPACE) */ {
				error = copyout(sp, sfsp, sizeof(*sp));
				if (error) {
					vfs_unbusy(mp, td);
					VFS_UNLOCK_GIANT(vfslocked);
					return (error);
				}
			}
			sfsp++;
		}
		VFS_UNLOCK_GIANT(vfslocked);
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
static void cvtstatfs(struct statfs *, struct ostatfs *);

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
	cvtstatfs(&sf, &osb);
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
	cvtstatfs(&sf, &osb);
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
	struct statfs *buf, *sp;
	struct ostatfs osb;
	size_t count, size;
	int error;

	count = uap->bufsize / sizeof(struct ostatfs);
	size = count * sizeof(struct statfs);
	error = kern_getfsstat(td, &buf, size, UIO_SYSSPACE, uap->flags);
	if (size > 0) {
		count = td->td_retval[0];
		sp = buf;
		while (count > 0 && error == 0) {
			cvtstatfs(sp, &osb);
			error = copyout(&osb, uap->buf, sizeof(osb));
			sp++;
			uap->buf++;
			count--;
		}
		free(buf, M_TEMP);
	}
	return (error);
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

	error = copyin(uap->u_fhp, &fh, sizeof(fhandle_t));
	if (error)
		return (error);
	error = kern_fhstatfs(td, fh, &sf);
	if (error)
		return (error);
	cvtstatfs(&sf, &osb);
	return (copyout(&osb, uap->buf, sizeof(osb)));
}

/*
 * Convert a new format statfs structure to an old format statfs structure.
 */
static void
cvtstatfs(nsp, osp)
	struct statfs *nsp;
	struct ostatfs *osp;
{

	statfs_scale_blocks(nsp, LONG_MAX);
	bzero(osp, sizeof(*osp));
	osp->f_bsize = nsp->f_bsize;
	osp->f_iosize = MIN(nsp->f_iosize, LONG_MAX);
	osp->f_blocks = nsp->f_blocks;
	osp->f_bfree = nsp->f_bfree;
	osp->f_bavail = nsp->f_bavail;
	osp->f_files = MIN(nsp->f_files, LONG_MAX);
	osp->f_ffree = MIN(nsp->f_ffree, LONG_MAX);
	osp->f_owner = nsp->f_owner;
	osp->f_type = nsp->f_type;
	osp->f_flags = nsp->f_flags;
	osp->f_syncwrites = MIN(nsp->f_syncwrites, LONG_MAX);
	osp->f_asyncwrites = MIN(nsp->f_asyncwrites, LONG_MAX);
	osp->f_syncreads = MIN(nsp->f_syncreads, LONG_MAX);
	osp->f_asyncreads = MIN(nsp->f_asyncreads, LONG_MAX);
	strlcpy(osp->f_fstypename, nsp->f_fstypename,
	    MIN(MFSNAMELEN, OMFSNAMELEN));
	strlcpy(osp->f_mntonname, nsp->f_mntonname,
	    MIN(MNAMELEN, OMNAMELEN));
	strlcpy(osp->f_mntfromname, nsp->f_mntfromname,
	    MIN(MNAMELEN, OMNAMELEN));
	osp->f_fsid = nsp->f_fsid;
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
	int vfslocked;
	int error;

	AUDIT_ARG(fd, uap->fd);
	if ((error = getvnode(fdp, uap->fd, &fp)) != 0)
		return (error);
	vp = fp->f_vnode;
	VREF(vp);
	fdrop(fp, td);
	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	AUDIT_ARG(vnode, vp, ARG_VNODE1);
	error = change_dir(vp, td);
	while (!error && (mp = vp->v_mountedhere) != NULL) {
		int tvfslocked;
		if (vfs_busy(mp, 0, 0, td))
			continue;
		tvfslocked = VFS_LOCK_GIANT(mp);
		error = VFS_ROOT(mp, LK_EXCLUSIVE, &tdp, td);
		vfs_unbusy(mp, td);
		if (error) {
			VFS_UNLOCK_GIANT(tvfslocked);
			break;
		}
		vput(vp);
		VFS_UNLOCK_GIANT(vfslocked);
		vp = tdp;
		vfslocked = tvfslocked;
	}
	if (error) {
		vput(vp);
		VFS_UNLOCK_GIANT(vfslocked);
		return (error);
	}
	VOP_UNLOCK(vp, 0);
	VFS_UNLOCK_GIANT(vfslocked);
	FILEDESC_XLOCK(fdp);
	vpold = fdp->fd_cdir;
	fdp->fd_cdir = vp;
	FILEDESC_XUNLOCK(fdp);
	vfslocked = VFS_LOCK_GIANT(vpold->v_mount);
	vrele(vpold);
	VFS_UNLOCK_GIANT(vfslocked);
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
	int vfslocked;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | AUDITVNODE1 | MPSAFE,
	    pathseg, path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	vfslocked = NDHASGIANT(&nd);
	if ((error = change_dir(nd.ni_vp, td)) != 0) {
		vput(nd.ni_vp);
		VFS_UNLOCK_GIANT(vfslocked);
		NDFREE(&nd, NDF_ONLY_PNBUF);
		return (error);
	}
	VOP_UNLOCK(nd.ni_vp, 0);
	VFS_UNLOCK_GIANT(vfslocked);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	FILEDESC_XLOCK(fdp);
	vp = fdp->fd_cdir;
	fdp->fd_cdir = nd.ni_vp;
	FILEDESC_XUNLOCK(fdp);
	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	vrele(vp);
	VFS_UNLOCK_GIANT(vfslocked);
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

	FILEDESC_LOCK_ASSERT(fdp);

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
	int vfslocked;

	error = priv_check(td, PRIV_VFS_CHROOT);
	if (error)
		return (error);
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | MPSAFE | AUDITVNODE1,
	    UIO_USERSPACE, uap->path, td);
	error = namei(&nd);
	if (error)
		goto error;
	vfslocked = NDHASGIANT(&nd);
	if ((error = change_dir(nd.ni_vp, td)) != 0)
		goto e_vunlock;
#ifdef MAC
	if ((error = mac_vnode_check_chroot(td->td_ucred, nd.ni_vp)))
		goto e_vunlock;
#endif
	VOP_UNLOCK(nd.ni_vp, 0);
	error = change_root(nd.ni_vp, td);
	vrele(nd.ni_vp);
	VFS_UNLOCK_GIANT(vfslocked);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	return (error);
e_vunlock:
	vput(nd.ni_vp);
	VFS_UNLOCK_GIANT(vfslocked);
error:
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
	error = mac_vnode_check_chdir(td->td_ucred, vp);
	if (error)
		return (error);
#endif
	error = VOP_ACCESS(vp, VEXEC, td->td_ucred, td);
	return (error);
}

/*
 * Common routine for kern_chroot() and jail_attach().  The caller is
 * responsible for invoking priv_check() and mac_vnode_check_chroot() to
 * authorize this operation.
 */
int
change_root(vp, td)
	struct vnode *vp;
	struct thread *td;
{
	struct filedesc *fdp;
	struct vnode *oldvp;
	int vfslocked;
	int error;

	VFS_ASSERT_GIANT(vp->v_mount);
	fdp = td->td_proc->p_fd;
	FILEDESC_XLOCK(fdp);
	if (chroot_allow_open_directories == 0 ||
	    (chroot_allow_open_directories == 1 && fdp->fd_rdir != rootvnode)) {
		error = chroot_refuse_vdir_fds(fdp);
		if (error) {
			FILEDESC_XUNLOCK(fdp);
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
	FILEDESC_XUNLOCK(fdp);
	vfslocked = VFS_LOCK_GIANT(oldvp->v_mount);
	vrele(oldvp);
	VFS_UNLOCK_GIANT(vfslocked);
	return (0);
}

/*
 * Check permissions, allocate an open file structure, and call the device
 * open routine if any.
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

#ifndef _SYS_SYSPROTO_H_
struct openat_args {
	int	fd;
	char	*path;
	int	flag;
	int	mode;
};
#endif
int
openat(struct thread *td, struct openat_args *uap)
{

	return (kern_openat(td, uap->fd, uap->path, UIO_USERSPACE, uap->flag,
	    uap->mode));
}

int
kern_open(struct thread *td, char *path, enum uio_seg pathseg, int flags,
    int mode)
{

	return (kern_openat(td, AT_FDCWD, path, pathseg, flags, mode));
}

int
kern_openat(struct thread *td, int fd, char *path, enum uio_seg pathseg,
    int flags, int mode)
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
	int vfslocked;

	AUDIT_ARG(fflags, flags);
	AUDIT_ARG(mode, mode);
	/* XXX: audit dirfd */
	/*
	 * Only one of the O_EXEC, O_RDONLY, O_WRONLY and O_RDWR may
	 * be specified.
	 */
	if (flags & O_EXEC) {
		if (flags & O_ACCMODE)
			return (EINVAL);
	} else if ((flags & O_ACCMODE) == O_ACCMODE)
		return (EINVAL);
	else
		flags = FFLAGS(flags);

	error = falloc(td, &nfp, &indx);
	if (error)
		return (error);
	/* An extra reference on `nfp' has been held for us by falloc(). */
	fp = nfp;
	/* Set the flags early so the finit in devfs can pick them up. */
	fp->f_flag = flags & FMASK;
	cmode = ((mode &~ fdp->fd_cmask) & ALLPERMS) &~ S_ISTXT;
	NDINIT_AT(&nd, LOOKUP, FOLLOW | AUDITVNODE1 | MPSAFE, pathseg, path, fd,
	    td);
	td->td_dupfd = -1;		/* XXX check for fdopen */
	error = vn_open(&nd, &flags, cmode, fp);
	if (error) {
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
		 * handle special fdopen() case.  bleh.  dupfdopen() is
		 * responsible for dropping the old contents of ofiles[indx]
		 * if it succeeds.
		 */
		if ((error == ENODEV || error == ENXIO) &&
		    td->td_dupfd >= 0 &&		/* XXX from fdopen */
		    (error =
			dupfdopen(td, fdp, indx, td->td_dupfd, flags, error)) == 0) {
			td->td_retval[0] = indx;
			fdrop(fp, td);
			return (0);
		}
		/*
		 * Clean up the descriptor, but only if another thread hadn't
		 * replaced or closed it.
		 */
		fdclose(fdp, fp, indx, td);
		fdrop(fp, td);

		if (error == ERESTART)
			error = EINTR;
		return (error);
	}
	td->td_dupfd = 0;
	vfslocked = NDHASGIANT(&nd);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;

	fp->f_vnode = vp;	/* XXX Does devfs need this? */
	/*
	 * If the file wasn't claimed by devfs bind it to the normal
	 * vnode operations here.
	 */
	if (fp->f_ops == &badfileops) {
		KASSERT(vp->v_type != VFIFO, ("Unexpected fifo."));
		fp->f_seqcount = 1;
		finit(fp, flags & FMASK, DTYPE_VNODE, vp, &vnops);
	}

	VOP_UNLOCK(vp, 0);
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
		atomic_set_int(&fp->f_flag, FHASLOCK);
	}
	if (flags & O_TRUNC) {
		if ((error = vn_start_write(vp, &mp, V_WAIT | PCATCH)) != 0)
			goto bad;
		VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
		VATTR_NULL(&vat);
		vat.va_size = 0;
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
#ifdef MAC
		error = mac_vnode_check_write(td->td_ucred, fp->f_cred, vp);
		if (error == 0)
#endif
			error = VOP_SETATTR(vp, &vat, td->td_ucred);
		VOP_UNLOCK(vp, 0);
		vn_finished_write(mp);
		if (error)
			goto bad;
	}
	VFS_UNLOCK_GIANT(vfslocked);
	/*
	 * Release our private reference, leaving the one associated with
	 * the descriptor table intact.
	 */
	fdrop(fp, td);
	td->td_retval[0] = indx;
	return (0);
bad:
	VFS_UNLOCK_GIANT(vfslocked);
	fdclose(fdp, fp, indx, td);
	fdrop(fp, td);
	return (error);
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

#ifndef _SYS_SYSPROTO_H_
struct mknodat_args {
	int	fd;
	char	*path;
	mode_t	mode;
	dev_t	dev;
};
#endif
int
mknodat(struct thread *td, struct mknodat_args *uap)
{

	return (kern_mknodat(td, uap->fd, uap->path, UIO_USERSPACE, uap->mode,
	    uap->dev));
}

int
kern_mknod(struct thread *td, char *path, enum uio_seg pathseg, int mode,
    int dev)
{

	return (kern_mknodat(td, AT_FDCWD, path, pathseg, mode, dev));
}

int
kern_mknodat(struct thread *td, int fd, char *path, enum uio_seg pathseg,
    int mode, int dev)
{
	struct vnode *vp;
	struct mount *mp;
	struct vattr vattr;
	int error;
	int whiteout = 0;
	struct nameidata nd;
	int vfslocked;

	AUDIT_ARG(mode, mode);
	AUDIT_ARG(dev, dev);
	switch (mode & S_IFMT) {
	case S_IFCHR:
	case S_IFBLK:
		error = priv_check(td, PRIV_VFS_MKNOD_DEV);
		break;
	case S_IFMT:
		error = priv_check(td, PRIV_VFS_MKNOD_BAD);
		break;
	case S_IFWHT:
		error = priv_check(td, PRIV_VFS_MKNOD_WHT);
		break;
	case S_IFIFO:
		if (dev == 0)
			return (kern_mkfifoat(td, fd, path, pathseg, mode));
		/* FALLTHROUGH */
	default:
		error = EINVAL;
		break;
	}
	if (error)
		return (error);
restart:
	bwillwrite();
	NDINIT_AT(&nd, CREATE, LOCKPARENT | SAVENAME | MPSAFE | AUDITVNODE1,
	    pathseg, path, fd, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	vfslocked = NDHASGIANT(&nd);
	vp = nd.ni_vp;
	if (vp != NULL) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		if (vp == nd.ni_dvp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(vp);
		VFS_UNLOCK_GIANT(vfslocked);
		return (EEXIST);
	} else {
		VATTR_NULL(&vattr);
		FILEDESC_SLOCK(td->td_proc->p_fd);
		vattr.va_mode = (mode & ALLPERMS) &
		    ~td->td_proc->p_fd->fd_cmask;
		FILEDESC_SUNLOCK(td->td_proc->p_fd);
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
			panic("kern_mknod: invalid mode");
		}
	}
	if (vn_start_write(nd.ni_dvp, &mp, V_NOWAIT) != 0) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		vput(nd.ni_dvp);
		VFS_UNLOCK_GIANT(vfslocked);
		if ((error = vn_start_write(NULL, &mp, V_XSLEEP | PCATCH)) != 0)
			return (error);
		goto restart;
	}
#ifdef MAC
	if (error == 0 && !whiteout)
		error = mac_vnode_check_create(td->td_ucred, nd.ni_dvp,
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
	VFS_UNLOCK_GIANT(vfslocked);
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

#ifndef _SYS_SYSPROTO_H_
struct mkfifoat_args {
	int	fd;
	char	*path;
	mode_t	mode;
};
#endif
int
mkfifoat(struct thread *td, struct mkfifoat_args *uap)
{

	return (kern_mkfifoat(td, uap->fd, uap->path, UIO_USERSPACE,
	    uap->mode));
}

int
kern_mkfifo(struct thread *td, char *path, enum uio_seg pathseg, int mode)
{

	return (kern_mkfifoat(td, AT_FDCWD, path, pathseg, mode));
}

int
kern_mkfifoat(struct thread *td, int fd, char *path, enum uio_seg pathseg,
    int mode)
{
	struct mount *mp;
	struct vattr vattr;
	int error;
	struct nameidata nd;
	int vfslocked;

	AUDIT_ARG(mode, mode);
restart:
	bwillwrite();
	NDINIT_AT(&nd, CREATE, LOCKPARENT | SAVENAME | MPSAFE | AUDITVNODE1,
	    pathseg, path, fd, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	vfslocked = NDHASGIANT(&nd);
	if (nd.ni_vp != NULL) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		if (nd.ni_vp == nd.ni_dvp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(nd.ni_vp);
		VFS_UNLOCK_GIANT(vfslocked);
		return (EEXIST);
	}
	if (vn_start_write(nd.ni_dvp, &mp, V_NOWAIT) != 0) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		vput(nd.ni_dvp);
		VFS_UNLOCK_GIANT(vfslocked);
		if ((error = vn_start_write(NULL, &mp, V_XSLEEP | PCATCH)) != 0)
			return (error);
		goto restart;
	}
	VATTR_NULL(&vattr);
	vattr.va_type = VFIFO;
	FILEDESC_SLOCK(td->td_proc->p_fd);
	vattr.va_mode = (mode & ALLPERMS) & ~td->td_proc->p_fd->fd_cmask;
	FILEDESC_SUNLOCK(td->td_proc->p_fd);
#ifdef MAC
	error = mac_vnode_check_create(td->td_ucred, nd.ni_dvp, &nd.ni_cnd,
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
	vput(nd.ni_dvp);
	vn_finished_write(mp);
	VFS_UNLOCK_GIANT(vfslocked);
	NDFREE(&nd, NDF_ONLY_PNBUF);
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

	return (kern_link(td, uap->path, uap->link, UIO_USERSPACE));
}

#ifndef _SYS_SYSPROTO_H_
struct linkat_args {
	int	fd1;
	char	*path1;
	int	fd2;
	char	*path2;
	int	flag;
};
#endif
int
linkat(struct thread *td, struct linkat_args *uap)
{
	int flag;

	flag = uap->flag;
	if (flag & ~AT_SYMLINK_FOLLOW)
		return (EINVAL);

	return (kern_linkat(td, uap->fd1, uap->fd2, uap->path1, uap->path2,
	    UIO_USERSPACE, (flag & AT_SYMLINK_FOLLOW) ? FOLLOW : NOFOLLOW));
}

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
can_hardlink(struct vnode *vp, struct ucred *cred)
{
	struct vattr va;
	int error;

	if (!hardlink_check_uid && !hardlink_check_gid)
		return (0);

	error = VOP_GETATTR(vp, &va, cred);
	if (error != 0)
		return (error);

	if (hardlink_check_uid && cred->cr_uid != va.va_uid) {
		error = priv_check_cred(cred, PRIV_VFS_LINK, 0);
		if (error)
			return (error);
	}

	if (hardlink_check_gid && !groupmember(va.va_gid, cred)) {
		error = priv_check_cred(cred, PRIV_VFS_LINK, 0);
		if (error)
			return (error);
	}

	return (0);
}

int
kern_link(struct thread *td, char *path, char *link, enum uio_seg segflg)
{

	return (kern_linkat(td, AT_FDCWD, AT_FDCWD, path,link, segflg, FOLLOW));
}

int
kern_linkat(struct thread *td, int fd1, int fd2, char *path1, char *path2,
    enum uio_seg segflg, int follow)
{
	struct vnode *vp;
	struct mount *mp;
	struct nameidata nd;
	int vfslocked;
	int lvfslocked;
	int error;

	bwillwrite();
	NDINIT_AT(&nd, LOOKUP, follow | MPSAFE | AUDITVNODE1, segflg, path1,
	    fd1, td);

	if ((error = namei(&nd)) != 0)
		return (error);
	vfslocked = NDHASGIANT(&nd);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;
	if (vp->v_type == VDIR) {
		vrele(vp);
		VFS_UNLOCK_GIANT(vfslocked);
		return (EPERM);		/* POSIX */
	}
	if ((error = vn_start_write(vp, &mp, V_WAIT | PCATCH)) != 0) {
		vrele(vp);
		VFS_UNLOCK_GIANT(vfslocked);
		return (error);
	}
	NDINIT_AT(&nd, CREATE, LOCKPARENT | SAVENAME | MPSAFE | AUDITVNODE1,
	    segflg, path2, fd2, td);
	if ((error = namei(&nd)) == 0) {
		lvfslocked = NDHASGIANT(&nd);
		if (nd.ni_vp != NULL) {
			if (nd.ni_dvp == nd.ni_vp)
				vrele(nd.ni_dvp);
			else
				vput(nd.ni_dvp);
			vrele(nd.ni_vp);
			error = EEXIST;
		} else if ((error = vn_lock(vp, LK_EXCLUSIVE | LK_RETRY))
		    == 0) {
			VOP_LEASE(nd.ni_dvp, td, td->td_ucred, LEASE_WRITE);
			VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
			error = can_hardlink(vp, td->td_ucred);
			if (error == 0)
#ifdef MAC
				error = mac_vnode_check_link(td->td_ucred,
				    nd.ni_dvp, vp, &nd.ni_cnd);
			if (error == 0)
#endif
				error = VOP_LINK(nd.ni_dvp, vp, &nd.ni_cnd);
			VOP_UNLOCK(vp, 0);
			vput(nd.ni_dvp);
		}
		NDFREE(&nd, NDF_ONLY_PNBUF);
		VFS_UNLOCK_GIANT(lvfslocked);
	}
	vrele(vp);
	vn_finished_write(mp);
	VFS_UNLOCK_GIANT(vfslocked);
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

#ifndef _SYS_SYSPROTO_H_
struct symlinkat_args {
	char	*path;
	int	fd;
	char	*path2;
};
#endif
int
symlinkat(struct thread *td, struct symlinkat_args *uap)
{

	return (kern_symlinkat(td, uap->path1, uap->fd, uap->path2,
	    UIO_USERSPACE));
}

int
kern_symlink(struct thread *td, char *path, char *link, enum uio_seg segflg)
{

	return (kern_symlinkat(td, path, AT_FDCWD, link, segflg));
}

int
kern_symlinkat(struct thread *td, char *path1, int fd, char *path2,
    enum uio_seg segflg)
{
	struct mount *mp;
	struct vattr vattr;
	char *syspath;
	int error;
	struct nameidata nd;
	int vfslocked;

	if (segflg == UIO_SYSSPACE) {
		syspath = path1;
	} else {
		syspath = uma_zalloc(namei_zone, M_WAITOK);
		if ((error = copyinstr(path1, syspath, MAXPATHLEN, NULL)) != 0)
			goto out;
	}
	AUDIT_ARG(text, syspath);
restart:
	bwillwrite();
	NDINIT_AT(&nd, CREATE, LOCKPARENT | SAVENAME | MPSAFE | AUDITVNODE1,
	    segflg, path2, fd, td);
	if ((error = namei(&nd)) != 0)
		goto out;
	vfslocked = NDHASGIANT(&nd);
	if (nd.ni_vp) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		if (nd.ni_vp == nd.ni_dvp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(nd.ni_vp);
		VFS_UNLOCK_GIANT(vfslocked);
		error = EEXIST;
		goto out;
	}
	if (vn_start_write(nd.ni_dvp, &mp, V_NOWAIT) != 0) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		vput(nd.ni_dvp);
		VFS_UNLOCK_GIANT(vfslocked);
		if ((error = vn_start_write(NULL, &mp, V_XSLEEP | PCATCH)) != 0)
			goto out;
		goto restart;
	}
	VATTR_NULL(&vattr);
	FILEDESC_SLOCK(td->td_proc->p_fd);
	vattr.va_mode = ACCESSPERMS &~ td->td_proc->p_fd->fd_cmask;
	FILEDESC_SUNLOCK(td->td_proc->p_fd);
#ifdef MAC
	vattr.va_type = VLNK;
	error = mac_vnode_check_create(td->td_ucred, nd.ni_dvp, &nd.ni_cnd,
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
	VFS_UNLOCK_GIANT(vfslocked);
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
	int vfslocked;

restart:
	bwillwrite();
	NDINIT(&nd, DELETE, LOCKPARENT | DOWHITEOUT | MPSAFE | AUDITVNODE1,
	    UIO_USERSPACE, uap->path, td);
	error = namei(&nd);
	if (error)
		return (error);
	vfslocked = NDHASGIANT(&nd);

	if (nd.ni_vp != NULLVP || !(nd.ni_cnd.cn_flags & ISWHITEOUT)) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		if (nd.ni_vp == nd.ni_dvp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (nd.ni_vp)
			vrele(nd.ni_vp);
		VFS_UNLOCK_GIANT(vfslocked);
		return (EEXIST);
	}
	if (vn_start_write(nd.ni_dvp, &mp, V_NOWAIT) != 0) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		vput(nd.ni_dvp);
		VFS_UNLOCK_GIANT(vfslocked);
		if ((error = vn_start_write(NULL, &mp, V_XSLEEP | PCATCH)) != 0)
			return (error);
		goto restart;
	}
	VOP_LEASE(nd.ni_dvp, td, td->td_ucred, LEASE_WRITE);
	error = VOP_WHITEOUT(nd.ni_dvp, &nd.ni_cnd, DELETE);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vput(nd.ni_dvp);
	vn_finished_write(mp);
	VFS_UNLOCK_GIANT(vfslocked);
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

	return (kern_unlink(td, uap->path, UIO_USERSPACE));
}

#ifndef _SYS_SYSPROTO_H_
struct unlinkat_args {
	int	fd;
	char	*path;
	int	flag;
};
#endif
int
unlinkat(struct thread *td, struct unlinkat_args *uap)
{
	int flag = uap->flag;
	int fd = uap->fd;
	char *path = uap->path;

	if (flag & ~AT_REMOVEDIR)
		return (EINVAL);

	if (flag & AT_REMOVEDIR)
		return (kern_rmdirat(td, fd, path, UIO_USERSPACE));
	else
		return (kern_unlinkat(td, fd, path, UIO_USERSPACE));
}

int
kern_unlink(struct thread *td, char *path, enum uio_seg pathseg)
{

	return (kern_unlinkat(td, AT_FDCWD, path, pathseg));
}

int
kern_unlinkat(struct thread *td, int fd, char *path, enum uio_seg pathseg)
{
	struct mount *mp;
	struct vnode *vp;
	int error;
	struct nameidata nd;
	int vfslocked;

restart:
	bwillwrite();
	NDINIT_AT(&nd, DELETE, LOCKPARENT | LOCKLEAF | MPSAFE | AUDITVNODE1,
	    pathseg, path, fd, td);
	if ((error = namei(&nd)) != 0)
		return (error == EINVAL ? EPERM : error);
	vfslocked = NDHASGIANT(&nd);
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
			vput(nd.ni_dvp);
			if (vp == nd.ni_dvp)
				vrele(vp);
			else
				vput(vp);
			VFS_UNLOCK_GIANT(vfslocked);
			if ((error = vn_start_write(NULL, &mp,
			    V_XSLEEP | PCATCH)) != 0)
				return (error);
			goto restart;
		}
#ifdef MAC
		error = mac_vnode_check_unlink(td->td_ucred, nd.ni_dvp, vp,
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
	vput(nd.ni_dvp);
	if (vp == nd.ni_dvp)
		vrele(vp);
	else
		vput(vp);
	VFS_UNLOCK_GIANT(vfslocked);
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
	int vfslocked;

	if ((error = fget(td, uap->fd, &fp)) != 0)
		return (error);
	if (!(fp->f_ops->fo_flags & DFLAG_SEEKABLE)) {
		fdrop(fp, td);
		return (ESPIPE);
	}
	vp = fp->f_vnode;
	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
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
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		error = VOP_GETATTR(vp, &vattr, cred);
		VOP_UNLOCK(vp, 0);
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
	case SEEK_DATA:
		error = fo_ioctl(fp, FIOSEEKDATA, &offset, cred, td);
		break;
	case SEEK_HOLE:
		error = fo_ioctl(fp, FIOSEEKHOLE, &offset, cred, td);
		break;
	default:
		error = EINVAL;
	}
	if (error == 0 && noneg && offset < 0)
		error = EINVAL;
	if (error != 0)
		goto drop;
	fp->f_offset = offset;
	*(off_t *)(td->td_retval) = fp->f_offset;
drop:
	fdrop(fp, td);
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
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

	nuap.fd = uap->fd;
	nuap.offset = uap->offset;
	nuap.whence = uap->whence;
	return (lseek(td, &nuap));
}
#endif /* COMPAT_43 */

/* Version with the 'pad' argument */
int
freebsd6_lseek(td, uap)
	struct thread *td;
	register struct freebsd6_lseek_args *uap;
{
	struct lseek_args ouap;

	ouap.fd = uap->fd;
	ouap.offset = uap->offset;
	ouap.whence = uap->whence;
	return (lseek(td, &ouap));
}

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
		error = mac_vnode_check_access(cred, vp, flags);
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

#ifndef _SYS_SYSPROTO_H_
struct faccessat_args {
	int	dirfd;
	char	*path;
	int	mode;
	int	flag;
}
#endif
int faccessat(struct thread *td, struct faccessat_args *uap)
{

	if (uap->flag & ~AT_EACCESS)
		return (EINVAL);
	return (kern_accessat(td, uap->fd, uap->path, UIO_USERSPACE, uap->flag,
	    uap->mode));
}

int
kern_access(struct thread *td, char *path, enum uio_seg pathseg, int mode)
{

	return (kern_accessat(td, AT_FDCWD, path, pathseg, 0, mode));
}

int
kern_accessat(struct thread *td, int fd, char *path, enum uio_seg pathseg,
    int flags, int mode)
{
	struct ucred *cred, *tmpcred;
	struct vnode *vp;
	struct nameidata nd;
	int vfslocked;
	int error;

	/*
	 * Create and modify a temporary credential instead of one that
	 * is potentially shared.  This could also mess up socket
	 * buffer accounting which can run in an interrupt context.
	 */
	if (!(flags & AT_EACCESS)) {
		cred = td->td_ucred;
		tmpcred = crdup(cred);
		tmpcred->cr_uid = cred->cr_ruid;
		tmpcred->cr_groups[0] = cred->cr_rgid;
		td->td_ucred = tmpcred;
	} else
		cred = tmpcred = td->td_ucred;
	NDINIT_AT(&nd, LOOKUP, FOLLOW | LOCKLEAF | MPSAFE | AUDITVNODE1,
	    pathseg, path, fd, td);
	if ((error = namei(&nd)) != 0)
		goto out1;
	vfslocked = NDHASGIANT(&nd);
	vp = nd.ni_vp;

	error = vn_access(vp, mode, tmpcred, td);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vput(vp);
	VFS_UNLOCK_GIANT(vfslocked);
out1:
	if (!(flags & AT_EACCESS)) {
		td->td_ucred = cred;
		crfree(tmpcred);
	}
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

	return (kern_eaccess(td, uap->path, UIO_USERSPACE, uap->flags));
}

int
kern_eaccess(struct thread *td, char *path, enum uio_seg pathseg, int flags)
{

	return (kern_accessat(td, AT_FDCWD, path, pathseg, AT_EACCESS, flags));
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

#ifndef _SYS_SYSPROTO_H_
struct fstatat_args {
	int	fd;
	char	*path;
	struct stat	*buf;
	int	flag;
}
#endif
int
fstatat(struct thread *td, struct fstatat_args *uap)
{
	struct stat sb;
	int error;

	error = kern_statat(td, uap->flag, uap->fd, uap->path,
	    UIO_USERSPACE, &sb);
	if (error == 0)
		error = copyout(&sb, uap->buf, sizeof (sb));
	return (error);
}

int
kern_stat(struct thread *td, char *path, enum uio_seg pathseg, struct stat *sbp)
{

	return (kern_statat(td, 0, AT_FDCWD, path, pathseg, sbp));
}

int
kern_statat(struct thread *td, int flag, int fd, char *path,
    enum uio_seg pathseg, struct stat *sbp)
{
	struct nameidata nd;
	struct stat sb;
	int error, vfslocked;

	if (flag & ~AT_SYMLINK_NOFOLLOW)
		return (EINVAL);

	NDINIT_AT(&nd, LOOKUP, ((flag & AT_SYMLINK_NOFOLLOW) ? NOFOLLOW :
	    FOLLOW) | LOCKSHARED | LOCKLEAF | AUDITVNODE1 | MPSAFE, pathseg,
	    path, fd, td);

	if ((error = namei(&nd)) != 0)
		return (error);
	vfslocked = NDHASGIANT(&nd);
	error = vn_stat(nd.ni_vp, &sb, td->td_ucred, NOCRED, td);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vput(nd.ni_vp);
	VFS_UNLOCK_GIANT(vfslocked);
	if (mtx_owned(&Giant))
		printf("stat(%d): %s\n", vfslocked, path);
	if (error)
		return (error);
	*sbp = sb;
#ifdef KTRACE
	if (KTRPOINT(td, KTR_STRUCT))
		ktrstat(&sb);
#endif
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

	return (kern_statat(td, AT_SYMLINK_NOFOLLOW, AT_FDCWD, path, pathseg,
	    sbp));
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
	int error, vfslocked;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | MPSAFE | AUDITVNODE1,
	    pathseg, path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	vfslocked = NDHASGIANT(&nd);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	/* If asynchronous I/O is available, it works for all files. */
	if (name == _PC_ASYNC_IO)
		td->td_retval[0] = async_io_version;
	else
		error = VOP_PATHCONF(nd.ni_vp, name, td->td_retval);
	vput(nd.ni_vp);
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

/*
 * Return target name of a symbolic link.
 */
#ifndef _SYS_SYSPROTO_H_
struct readlink_args {
	char	*path;
	char	*buf;
	size_t	count;
};
#endif
int
readlink(td, uap)
	struct thread *td;
	register struct readlink_args /* {
		char *path;
		char *buf;
		size_t count;
	} */ *uap;
{

	return (kern_readlink(td, uap->path, UIO_USERSPACE, uap->buf,
	    UIO_USERSPACE, uap->count));
}
#ifndef _SYS_SYSPROTO_H_
struct readlinkat_args {
	int	fd;
	char	*path;
	char	*buf;
	size_t	bufsize;
};
#endif
int
readlinkat(struct thread *td, struct readlinkat_args *uap)
{

	return (kern_readlinkat(td, uap->fd, uap->path, UIO_USERSPACE,
	    uap->buf, UIO_USERSPACE, uap->bufsize));
}

int
kern_readlink(struct thread *td, char *path, enum uio_seg pathseg, char *buf,
    enum uio_seg bufseg, size_t count)
{

	return (kern_readlinkat(td, AT_FDCWD, path, pathseg, buf, bufseg,
	    count));
}

int
kern_readlinkat(struct thread *td, int fd, char *path, enum uio_seg pathseg,
    char *buf, enum uio_seg bufseg, size_t count)
{
	struct vnode *vp;
	struct iovec aiov;
	struct uio auio;
	int error;
	struct nameidata nd;
	int vfslocked;

	NDINIT_AT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF | MPSAFE | AUDITVNODE1,
	    pathseg, path, fd, td);

	if ((error = namei(&nd)) != 0)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vfslocked = NDHASGIANT(&nd);
	vp = nd.ni_vp;
#ifdef MAC
	error = mac_vnode_check_readlink(td->td_ucred, vp);
	if (error) {
		vput(vp);
		VFS_UNLOCK_GIANT(vfslocked);
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
	VFS_UNLOCK_GIANT(vfslocked);
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
		error = priv_check(td, PRIV_VFS_CHFLAGS_DEV);
		if (error)
			return (error);
	}

	if ((error = vn_start_write(vp, &mp, V_WAIT | PCATCH)) != 0)
		return (error);
	VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	VATTR_NULL(&vattr);
	vattr.va_flags = flags;
#ifdef MAC
	error = mac_vnode_check_setflags(td->td_ucred, vp, vattr.va_flags);
	if (error == 0)
#endif
		error = VOP_SETATTR(vp, &vattr, td->td_ucred);
	VOP_UNLOCK(vp, 0);
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
	int vfslocked;

	AUDIT_ARG(fflags, uap->flags);
	NDINIT(&nd, LOOKUP, FOLLOW | MPSAFE | AUDITVNODE1, UIO_USERSPACE,
	    uap->path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vfslocked = NDHASGIANT(&nd);
	error = setfflags(td, nd.ni_vp, uap->flags);
	vrele(nd.ni_vp);
	VFS_UNLOCK_GIANT(vfslocked);
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
	int vfslocked;

	AUDIT_ARG(fflags, uap->flags);
	NDINIT(&nd, LOOKUP, NOFOLLOW | MPSAFE | AUDITVNODE1, UIO_USERSPACE,
	    uap->path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	vfslocked = NDHASGIANT(&nd);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	error = setfflags(td, nd.ni_vp, uap->flags);
	vrele(nd.ni_vp);
	VFS_UNLOCK_GIANT(vfslocked);
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
	int vfslocked;
	int error;

	AUDIT_ARG(fd, uap->fd);
	AUDIT_ARG(fflags, uap->flags);
	if ((error = getvnode(td->td_proc->p_fd, uap->fd, &fp)) != 0)
		return (error);
	vfslocked = VFS_LOCK_GIANT(fp->f_vnode->v_mount);
#ifdef AUDIT
	vn_lock(fp->f_vnode, LK_EXCLUSIVE | LK_RETRY);
	AUDIT_ARG(vnode, fp->f_vnode, ARG_VNODE1);
	VOP_UNLOCK(fp->f_vnode, 0);
#endif
	error = setfflags(td, fp->f_vnode, uap->flags);
	VFS_UNLOCK_GIANT(vfslocked);
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
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	VATTR_NULL(&vattr);
	vattr.va_mode = mode & ALLPERMS;
#ifdef MAC
	error = mac_vnode_check_setmode(td->td_ucred, vp, vattr.va_mode);
	if (error == 0)
#endif
		error = VOP_SETATTR(vp, &vattr, td->td_ucred);
	VOP_UNLOCK(vp, 0);
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

#ifndef _SYS_SYSPROTO_H_
struct fchmodat_args {
	int	dirfd;
	char	*path;
	mode_t	mode;
	int	flag;
}
#endif
int
fchmodat(struct thread *td, struct fchmodat_args *uap)
{
	int flag = uap->flag;
	int fd = uap->fd;
	char *path = uap->path;
	mode_t mode = uap->mode;

	if (flag & ~AT_SYMLINK_NOFOLLOW)
		return (EINVAL);

	return (kern_fchmodat(td, fd, path, UIO_USERSPACE, mode, flag));
}

int
kern_chmod(struct thread *td, char *path, enum uio_seg pathseg, int mode)
{

	return (kern_fchmodat(td, AT_FDCWD, path, pathseg, mode, 0));
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

	return (kern_fchmodat(td, AT_FDCWD, uap->path, UIO_USERSPACE,
	    uap->mode, AT_SYMLINK_NOFOLLOW));
}


int
kern_fchmodat(struct thread *td, int fd, char *path, enum uio_seg pathseg,
    mode_t mode, int flag)
{
	int error;
	struct nameidata nd;
	int vfslocked;
	int follow;

	AUDIT_ARG(mode, mode);
	follow = (flag & AT_SYMLINK_NOFOLLOW) ? NOFOLLOW : FOLLOW;
	NDINIT_AT(&nd, LOOKUP,  follow | MPSAFE | AUDITVNODE1, pathseg, path,
	    fd, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	vfslocked = NDHASGIANT(&nd);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	error = setfmode(td, nd.ni_vp, mode);
	vrele(nd.ni_vp);
	VFS_UNLOCK_GIANT(vfslocked);
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
	int vfslocked;
	int error;

	AUDIT_ARG(fd, uap->fd);
	AUDIT_ARG(mode, uap->mode);
	if ((error = getvnode(td->td_proc->p_fd, uap->fd, &fp)) != 0)
		return (error);
	vfslocked = VFS_LOCK_GIANT(fp->f_vnode->v_mount);
#ifdef AUDIT
	vn_lock(fp->f_vnode, LK_EXCLUSIVE | LK_RETRY);
	AUDIT_ARG(vnode, fp->f_vnode, ARG_VNODE1);
	VOP_UNLOCK(fp->f_vnode, 0);
#endif
	error = setfmode(td, fp->f_vnode, uap->mode);
	VFS_UNLOCK_GIANT(vfslocked);
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
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	VATTR_NULL(&vattr);
	vattr.va_uid = uid;
	vattr.va_gid = gid;
#ifdef MAC
	error = mac_vnode_check_setowner(td->td_ucred, vp, vattr.va_uid,
	    vattr.va_gid);
	if (error == 0)
#endif
		error = VOP_SETATTR(vp, &vattr, td->td_ucred);
	VOP_UNLOCK(vp, 0);
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

#ifndef _SYS_SYSPROTO_H_
struct fchownat_args {
	int fd;
	const char * path;
	uid_t uid;
	gid_t gid;
	int flag;
};
#endif
int
fchownat(struct thread *td, struct fchownat_args *uap)
{
	int flag;

	flag = uap->flag;
	if (flag & ~AT_SYMLINK_NOFOLLOW)
		return (EINVAL);

	return (kern_fchownat(td, uap->fd, uap->path, UIO_USERSPACE, uap->uid,
	    uap->gid, uap->flag));
}

int
kern_chown(struct thread *td, char *path, enum uio_seg pathseg, int uid,
    int gid)
{

	return (kern_fchownat(td, AT_FDCWD, path, pathseg, uid, gid, 0));
}

int
kern_fchownat(struct thread *td, int fd, char *path, enum uio_seg pathseg,
    int uid, int gid, int flag)
{
	struct nameidata nd;
	int error, vfslocked, follow;

	AUDIT_ARG(owner, uid, gid);
	follow = (flag & AT_SYMLINK_NOFOLLOW) ? NOFOLLOW : FOLLOW;
	NDINIT_AT(&nd, LOOKUP, follow | MPSAFE | AUDITVNODE1, pathseg, path,
	    fd, td);

	if ((error = namei(&nd)) != 0)
		return (error);
	vfslocked = NDHASGIANT(&nd);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	error = setfown(td, nd.ni_vp, uid, gid);
	vrele(nd.ni_vp);
	VFS_UNLOCK_GIANT(vfslocked);
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

	return (kern_fchownat(td, AT_FDCWD, path, pathseg, uid, gid,
	    AT_SYMLINK_NOFOLLOW));
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
	int vfslocked;
	int error;

	AUDIT_ARG(fd, uap->fd);
	AUDIT_ARG(owner, uap->uid, uap->gid);
	if ((error = getvnode(td->td_proc->p_fd, uap->fd, &fp)) != 0)
		return (error);
	vfslocked = VFS_LOCK_GIANT(fp->f_vnode->v_mount);
#ifdef AUDIT
	vn_lock(fp->f_vnode, LK_EXCLUSIVE | LK_RETRY);
	AUDIT_ARG(vnode, fp->f_vnode, ARG_VNODE1);
	VOP_UNLOCK(fp->f_vnode, 0);
#endif
	error = setfown(td, fp->f_vnode, uap->uid, uap->gid);
	VFS_UNLOCK_GIANT(vfslocked);
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

		if (tvp[0].tv_usec < 0 || tvp[0].tv_usec >= 1000000 ||
		    tvp[1].tv_usec < 0 || tvp[1].tv_usec >= 1000000)
			return (EINVAL);
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
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	setbirthtime = 0;
	if (numtimes < 3 && !VOP_GETATTR(vp, &vattr, td->td_ucred) &&
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
	error = mac_vnode_check_setutimes(td->td_ucred, vp, vattr.va_atime,
	    vattr.va_mtime);
#endif
	if (error == 0)
		error = VOP_SETATTR(vp, &vattr, td->td_ucred);
	VOP_UNLOCK(vp, 0);
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

#ifndef _SYS_SYSPROTO_H_
struct futimesat_args {
	int fd;
	const char * path;
	const struct timeval * times;
};
#endif
int
futimesat(struct thread *td, struct futimesat_args *uap)
{

	return (kern_utimesat(td, uap->fd, uap->path, UIO_USERSPACE,
	    uap->times, UIO_USERSPACE));
}

int
kern_utimes(struct thread *td, char *path, enum uio_seg pathseg,
    struct timeval *tptr, enum uio_seg tptrseg)
{

	return (kern_utimesat(td, AT_FDCWD, path, pathseg, tptr, tptrseg));
}

int
kern_utimesat(struct thread *td, int fd, char *path, enum uio_seg pathseg,
    struct timeval *tptr, enum uio_seg tptrseg)
{
	struct nameidata nd;
	struct timespec ts[2];
	int error, vfslocked;

	if ((error = getutimes(tptr, tptrseg, ts)) != 0)
		return (error);
	NDINIT_AT(&nd, LOOKUP, FOLLOW | MPSAFE | AUDITVNODE1, pathseg, path,
	    fd, td);

	if ((error = namei(&nd)) != 0)
		return (error);
	vfslocked = NDHASGIANT(&nd);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	error = setutimes(td, nd.ni_vp, ts, 2, tptr == NULL);
	vrele(nd.ni_vp);
	VFS_UNLOCK_GIANT(vfslocked);
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
	int vfslocked;

	if ((error = getutimes(tptr, tptrseg, ts)) != 0)
		return (error);
	NDINIT(&nd, LOOKUP, NOFOLLOW | MPSAFE | AUDITVNODE1, pathseg, path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	vfslocked = NDHASGIANT(&nd);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	error = setutimes(td, nd.ni_vp, ts, 2, tptr == NULL);
	vrele(nd.ni_vp);
	VFS_UNLOCK_GIANT(vfslocked);
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
	int vfslocked;
	int error;

	AUDIT_ARG(fd, fd);
	if ((error = getutimes(tptr, tptrseg, ts)) != 0)
		return (error);
	if ((error = getvnode(td->td_proc->p_fd, fd, &fp)) != 0)
		return (error);
	vfslocked = VFS_LOCK_GIANT(fp->f_vnode->v_mount);
#ifdef AUDIT
	vn_lock(fp->f_vnode, LK_EXCLUSIVE | LK_RETRY);
	AUDIT_ARG(vnode, fp->f_vnode, ARG_VNODE1);
	VOP_UNLOCK(fp->f_vnode, 0);
#endif
	error = setutimes(td, fp->f_vnode, ts, 2, tptr == NULL);
	VFS_UNLOCK_GIANT(vfslocked);
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
	int vfslocked;

	if (length < 0)
		return(EINVAL);
	NDINIT(&nd, LOOKUP, FOLLOW | MPSAFE | AUDITVNODE1, pathseg, path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	vfslocked = NDHASGIANT(&nd);
	vp = nd.ni_vp;
	if ((error = vn_start_write(vp, &mp, V_WAIT | PCATCH)) != 0) {
		vrele(vp);
		VFS_UNLOCK_GIANT(vfslocked);
		return (error);
	}
	NDFREE(&nd, NDF_ONLY_PNBUF);
	VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	if (vp->v_type == VDIR)
		error = EISDIR;
#ifdef MAC
	else if ((error = mac_vnode_check_write(td->td_ucred, NOCRED, vp))) {
	}
#endif
	else if ((error = vn_writechk(vp)) == 0 &&
	    (error = VOP_ACCESS(vp, VWRITE, td->td_ucred, td)) == 0) {
		VATTR_NULL(&vattr);
		vattr.va_size = length;
		error = VOP_SETATTR(vp, &vattr, td->td_ucred);
	}
	vput(vp);
	vn_finished_write(mp);
	VFS_UNLOCK_GIANT(vfslocked);
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
#endif /* COMPAT_43 */

/* Versions with the pad argument */
int
freebsd6_truncate(struct thread *td, struct freebsd6_truncate_args *uap)
{
	struct truncate_args ouap;

	ouap.path = uap->path;
	ouap.length = uap->length;
	return (truncate(td, &ouap));
}

int
freebsd6_ftruncate(struct thread *td, struct freebsd6_ftruncate_args *uap)
{
	struct ftruncate_args ouap;

	ouap.fd = uap->fd;
	ouap.length = uap->length;
	return (ftruncate(td, &ouap));
}

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
	int vfslocked;
	int error;

	AUDIT_ARG(fd, uap->fd);
	if ((error = getvnode(td->td_proc->p_fd, uap->fd, &fp)) != 0)
		return (error);
	vp = fp->f_vnode;
	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	if ((error = vn_start_write(vp, &mp, V_WAIT | PCATCH)) != 0)
		goto drop;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	AUDIT_ARG(vnode, vp, ARG_VNODE1);
	if (vp->v_object != NULL) {
		VM_OBJECT_LOCK(vp->v_object);
		vm_object_page_clean(vp->v_object, 0, 0, 0);
		VM_OBJECT_UNLOCK(vp->v_object);
	}
	error = VOP_FSYNC(vp, MNT_WAIT, td);

	VOP_UNLOCK(vp, 0);
	vn_finished_write(mp);
drop:
	VFS_UNLOCK_GIANT(vfslocked);
	fdrop(fp, td);
	return (error);
}

/*
 * Rename files.  Source and destination must either both be directories, or
 * both not be directories.  If target is a directory, it must be empty.
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

#ifndef _SYS_SYSPROTO_H_
struct renameat_args {
	int	oldfd;
	char	*old;
	int	newfd;
	char	*new;
};
#endif
int
renameat(struct thread *td, struct renameat_args *uap)
{

	return (kern_renameat(td, uap->oldfd, uap->old, uap->newfd, uap->new,
	    UIO_USERSPACE));
}

int
kern_rename(struct thread *td, char *from, char *to, enum uio_seg pathseg)
{

	return (kern_renameat(td, AT_FDCWD, from, AT_FDCWD, to, pathseg));
}

int
kern_renameat(struct thread *td, int oldfd, char *old, int newfd, char *new,
    enum uio_seg pathseg)
{
	struct mount *mp = NULL;
	struct vnode *tvp, *fvp, *tdvp;
	struct nameidata fromnd, tond;
	int tvfslocked;
	int fvfslocked;
	int error;

	bwillwrite();
#ifdef MAC
	NDINIT_AT(&fromnd, DELETE, LOCKPARENT | LOCKLEAF | SAVESTART | MPSAFE |
	    AUDITVNODE1, pathseg, old, oldfd, td);
#else
	NDINIT_AT(&fromnd, DELETE, WANTPARENT | SAVESTART | MPSAFE |
	    AUDITVNODE1, pathseg, old, oldfd, td);
#endif

	if ((error = namei(&fromnd)) != 0)
		return (error);
	fvfslocked = NDHASGIANT(&fromnd);
	tvfslocked = 0;
#ifdef MAC
	error = mac_vnode_check_rename_from(td->td_ucred, fromnd.ni_dvp,
	    fromnd.ni_vp, &fromnd.ni_cnd);
	VOP_UNLOCK(fromnd.ni_dvp, 0);
	if (fromnd.ni_dvp != fromnd.ni_vp)
		VOP_UNLOCK(fromnd.ni_vp, 0);
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
	NDINIT_AT(&tond, RENAME, LOCKPARENT | LOCKLEAF | NOCACHE | SAVESTART |
	    MPSAFE | AUDITVNODE2, pathseg, new, newfd, td);
	if (fromnd.ni_vp->v_type == VDIR)
		tond.ni_cnd.cn_flags |= WILLBEDIR;
	if ((error = namei(&tond)) != 0) {
		/* Translate error code for rename("dir1", "dir2/."). */
		if (error == EISDIR && fvp->v_type == VDIR)
			error = EINVAL;
		NDFREE(&fromnd, NDF_ONLY_PNBUF);
		vrele(fromnd.ni_dvp);
		vrele(fvp);
		vn_finished_write(mp);
		goto out1;
	}
	tvfslocked = NDHASGIANT(&tond);
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
		error = mac_vnode_check_rename_to(td->td_ucred, tdvp,
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
		if (tvp)
			vput(tvp);
		if (tdvp == tvp)
			vrele(tdvp);
		else
			vput(tdvp);
		vrele(fromnd.ni_dvp);
		vrele(fvp);
	}
	vrele(tond.ni_startdir);
	vn_finished_write(mp);
out1:
	if (fromnd.ni_startdir)
		vrele(fromnd.ni_startdir);
	VFS_UNLOCK_GIANT(fvfslocked);
	VFS_UNLOCK_GIANT(tvfslocked);
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

#ifndef _SYS_SYSPROTO_H_
struct mkdirat_args {
	int	fd;
	char	*path;
	mode_t	mode;
};
#endif
int
mkdirat(struct thread *td, struct mkdirat_args *uap)
{

	return (kern_mkdirat(td, uap->fd, uap->path, UIO_USERSPACE, uap->mode));
}

int
kern_mkdir(struct thread *td, char *path, enum uio_seg segflg, int mode)
{

	return (kern_mkdirat(td, AT_FDCWD, path, segflg, mode));
}

int
kern_mkdirat(struct thread *td, int fd, char *path, enum uio_seg segflg,
    int mode)
{
	struct mount *mp;
	struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;
	int vfslocked;

	AUDIT_ARG(mode, mode);
restart:
	bwillwrite();
	NDINIT_AT(&nd, CREATE, LOCKPARENT | SAVENAME | MPSAFE | AUDITVNODE1,
	    segflg, path, fd, td);
	nd.ni_cnd.cn_flags |= WILLBEDIR;
	if ((error = namei(&nd)) != 0)
		return (error);
	vfslocked = NDHASGIANT(&nd);
	vp = nd.ni_vp;
	if (vp != NULL) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		/*
		 * XXX namei called with LOCKPARENT but not LOCKLEAF has
		 * the strange behaviour of leaving the vnode unlocked
		 * if the target is the same vnode as the parent.
		 */
		if (vp == nd.ni_dvp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(vp);
		VFS_UNLOCK_GIANT(vfslocked);
		return (EEXIST);
	}
	if (vn_start_write(nd.ni_dvp, &mp, V_NOWAIT) != 0) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		vput(nd.ni_dvp);
		VFS_UNLOCK_GIANT(vfslocked);
		if ((error = vn_start_write(NULL, &mp, V_XSLEEP | PCATCH)) != 0)
			return (error);
		goto restart;
	}
	VATTR_NULL(&vattr);
	vattr.va_type = VDIR;
	FILEDESC_SLOCK(td->td_proc->p_fd);
	vattr.va_mode = (mode & ACCESSPERMS) &~ td->td_proc->p_fd->fd_cmask;
	FILEDESC_SUNLOCK(td->td_proc->p_fd);
#ifdef MAC
	error = mac_vnode_check_create(td->td_ucred, nd.ni_dvp, &nd.ni_cnd,
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
	VFS_UNLOCK_GIANT(vfslocked);
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

	return (kern_rmdirat(td, AT_FDCWD, path, pathseg));
}

int
kern_rmdirat(struct thread *td, int fd, char *path, enum uio_seg pathseg)
{
	struct mount *mp;
	struct vnode *vp;
	int error;
	struct nameidata nd;
	int vfslocked;

restart:
	bwillwrite();
	NDINIT_AT(&nd, DELETE, LOCKPARENT | LOCKLEAF | MPSAFE | AUDITVNODE1,
	    pathseg, path, fd, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	vfslocked = NDHASGIANT(&nd);
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
	error = mac_vnode_check_unlink(td->td_ucred, nd.ni_dvp, vp,
	    &nd.ni_cnd);
	if (error)
		goto out;
#endif
	if (vn_start_write(nd.ni_dvp, &mp, V_NOWAIT) != 0) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		vput(vp);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		VFS_UNLOCK_GIANT(vfslocked);
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
	vput(vp);
	if (nd.ni_dvp == vp)
		vrele(nd.ni_dvp);
	else
		vput(nd.ni_dvp);
	VFS_UNLOCK_GIANT(vfslocked);
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
	int error, eofflag, readcnt, vfslocked;
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
	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	if (vp->v_type != VDIR) {
		VFS_UNLOCK_GIANT(vfslocked);
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
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	loff = auio.uio_offset = fp->f_offset;
#ifdef MAC
	error = mac_vnode_check_readdir(td->td_ucred, vp);
	if (error) {
		VOP_UNLOCK(vp, 0);
		VFS_UNLOCK_GIANT(vfslocked);
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
	if (error) {
		VOP_UNLOCK(vp, 0);
		VFS_UNLOCK_GIANT(vfslocked);
		fdrop(fp, td);
		return (error);
	}
	if (uap->count == auio.uio_resid &&
	    (vp->v_vflag & VV_ROOT) &&
	    (vp->v_mount->mnt_flag & MNT_UNION)) {
		struct vnode *tvp = vp;
		vp = vp->v_mount->mnt_vnodecovered;
		VREF(vp);
		fp->f_vnode = vp;
		fp->f_data = vp;
		fp->f_offset = 0;
		vput(tvp);
		VFS_UNLOCK_GIANT(vfslocked);
		goto unionread;
	}
	VOP_UNLOCK(vp, 0);
	VFS_UNLOCK_GIANT(vfslocked);
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
	int vfslocked;
	long loff;
	int error, eofflag;

	AUDIT_ARG(fd, uap->fd);
	if ((error = getvnode(td->td_proc->p_fd, uap->fd, &fp)) != 0)
		return (error);
	if ((fp->f_flag & FREAD) == 0) {
		fdrop(fp, td);
		return (EBADF);
	}
	vp = fp->f_vnode;
unionread:
	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	if (vp->v_type != VDIR) {
		VFS_UNLOCK_GIANT(vfslocked);
		error = EINVAL;
		goto fail;
	}
	aiov.iov_base = uap->buf;
	aiov.iov_len = uap->count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_td = td;
	auio.uio_resid = uap->count;
	/* vn_lock(vp, LK_SHARED | LK_RETRY); */
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	AUDIT_ARG(vnode, vp, ARG_VNODE1);
	loff = auio.uio_offset = fp->f_offset;
#ifdef MAC
	error = mac_vnode_check_readdir(td->td_ucred, vp);
	if (error == 0)
#endif
		error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag, NULL,
		    NULL);
	fp->f_offset = auio.uio_offset;
	if (error) {
		VOP_UNLOCK(vp, 0);
		VFS_UNLOCK_GIANT(vfslocked);
		goto fail;
	}
	if (uap->count == auio.uio_resid &&
	    (vp->v_vflag & VV_ROOT) &&
	    (vp->v_mount->mnt_flag & MNT_UNION)) {
		struct vnode *tvp = vp;
		vp = vp->v_mount->mnt_vnodecovered;
		VREF(vp);
		fp->f_vnode = vp;
		fp->f_data = vp;
		fp->f_offset = 0;
		vput(tvp);
		VFS_UNLOCK_GIANT(vfslocked);
		goto unionread;
	}
	VOP_UNLOCK(vp, 0);
	VFS_UNLOCK_GIANT(vfslocked);
	if (uap->basep != NULL) {
		error = copyout(&loff, uap->basep, sizeof(long));
	}
	td->td_retval[0] = uap->count - auio.uio_resid;
fail:
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

	FILEDESC_XLOCK(td->td_proc->p_fd);
	fdp = td->td_proc->p_fd;
	td->td_retval[0] = fdp->fd_cmask;
	fdp->fd_cmask = uap->newmask & ALLPERMS;
	FILEDESC_XUNLOCK(td->td_proc->p_fd);
	return (0);
}

/*
 * Void all references to file by ripping underlying filesystem away from
 * vnode.
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
	struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;
	int vfslocked;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | MPSAFE | AUDITVNODE1,
	    UIO_USERSPACE, uap->path, td);
	if ((error = namei(&nd)) != 0)
		return (error);
	vfslocked = NDHASGIANT(&nd);
	vp = nd.ni_vp;
	NDFREE(&nd, NDF_ONLY_PNBUF);
	if (vp->v_type != VCHR) {
		error = EINVAL;
		goto out;
	}
#ifdef MAC
	error = mac_vnode_check_revoke(td->td_ucred, vp);
	if (error)
		goto out;
#endif
	error = VOP_GETATTR(vp, &vattr, td->td_ucred);
	if (error)
		goto out;
	if (td->td_ucred->cr_uid != vattr.va_uid) {
		error = priv_check(td, PRIV_VFS_ADMIN);
		if (error)
			goto out;
	}
	if (vcount(vp) > 1)
		VOP_REVOKE(vp, REVOKEALL);
out:
	vput(vp);
	VFS_UNLOCK_GIANT(vfslocked);
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
		FILEDESC_SLOCK(fdp);
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
		FILEDESC_SUNLOCK(fdp);
	}
	*fpp = fp;
	return (error);
}

/*
 * Get an (NFS) file handle.
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
	int vfslocked;
	int error;

	error = priv_check(td, PRIV_VFS_GETFH);
	if (error)
		return (error);
	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF | MPSAFE | AUDITVNODE1,
	    UIO_USERSPACE, uap->fname, td);
	error = namei(&nd);
	if (error)
		return (error);
	vfslocked = NDHASGIANT(&nd);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;
	bzero(&fh, sizeof(fh));
	fh.fh_fsid = vp->v_mount->mnt_stat.f_fsid;
	error = VOP_VPTOFH(vp, &fh.fh_fid);
	vput(vp);
	VFS_UNLOCK_GIANT(vfslocked);
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
	int vfslocked;
	int error;

	error = priv_check(td, PRIV_VFS_GETFH);
	if (error)
		return (error);
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | MPSAFE | AUDITVNODE1,
	    UIO_USERSPACE, uap->fname, td);
	error = namei(&nd);
	if (error)
		return (error);
	vfslocked = NDHASGIANT(&nd);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;
	bzero(&fh, sizeof(fh));
	fh.fh_fsid = vp->v_mount->mnt_stat.f_fsid;
	error = VOP_VPTOFH(vp, &fh.fh_fid);
	vput(vp);
	VFS_UNLOCK_GIANT(vfslocked);
	if (error)
		return (error);
	error = copyout(&fh, uap->fhp, sizeof (fh));
	return (error);
}

/*
 * syscall for the rpc.lockd to use to translate a NFS file handle into an
 * open descriptor.
 *
 * warning: do not remove the priv_check() call or this becomes one giant
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
	int vfslocked;
	int indx;

	error = priv_check(td, PRIV_VFS_FHOPEN);
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
	vfslocked = VFS_LOCK_GIANT(mp);
	/* now give me my vnode, it gets returned to me locked */
	error = VFS_FHTOVP(mp, &fhp.fh_fid, &vp);
	if (error)
		goto out;
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
	error = mac_vnode_check_open(td->td_ucred, vp, mode);
	if (error)
		goto bad;
#endif
	if (mode) {
		error = VOP_ACCESS(vp, mode, td->td_ucred, td);
		if (error)
			goto bad;
	}
	if (fmode & O_TRUNC) {
		VOP_UNLOCK(vp, 0);				/* XXX */
		if ((error = vn_start_write(NULL, &mp, V_WAIT | PCATCH)) != 0) {
			vrele(vp);
			goto out;
		}
		VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);	/* XXX */
#ifdef MAC
		/*
		 * We don't yet have fp->f_cred, so use td->td_ucred, which
		 * should be right.
		 */
		error = mac_vnode_check_write(td->td_ucred, td->td_ucred, vp);
		if (error == 0) {
#endif
			VATTR_NULL(vap);
			vap->va_size = 0;
			error = VOP_SETATTR(vp, vap, td->td_ucred);
#ifdef MAC
		}
#endif
		vn_finished_write(mp);
		if (error)
			goto bad;
	}
	error = VOP_OPEN(vp, fmode, td->td_ucred, td, NULL);
	if (error)
		goto bad;

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
	finit(nfp, fmode & FMASK, DTYPE_VNODE, vp, &vnops);
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
		VOP_UNLOCK(vp, 0);
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
			goto out;
		}
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		atomic_set_int(&fp->f_flag, FHASLOCK);
	}

	VOP_UNLOCK(vp, 0);
	fdrop(fp, td);
	vfs_rel(mp);
	VFS_UNLOCK_GIANT(vfslocked);
	td->td_retval[0] = indx;
	return (0);

bad:
	vput(vp);
out:
	vfs_rel(mp);
	VFS_UNLOCK_GIANT(vfslocked);
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
	int vfslocked;
	int error;

	error = priv_check(td, PRIV_VFS_FHSTAT);
	if (error)
		return (error);
	error = copyin(uap->u_fhp, &fh, sizeof(fhandle_t));
	if (error)
		return (error);
	if ((mp = vfs_getvfs(&fh.fh_fsid)) == NULL)
		return (ESTALE);
	vfslocked = VFS_LOCK_GIANT(mp);
	if ((error = VFS_FHTOVP(mp, &fh.fh_fid, &vp))) {
		vfs_rel(mp);
		VFS_UNLOCK_GIANT(vfslocked);
		return (error);
	}
	error = vn_stat(vp, &sb, td->td_ucred, NOCRED, td);
	vput(vp);
	vfs_rel(mp);
	VFS_UNLOCK_GIANT(vfslocked);
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

	error = copyin(uap->u_fhp, &fh, sizeof(fhandle_t));
	if (error)
		return (error);
	error = kern_fhstatfs(td, fh, &sf);
	if (error)
		return (error);
	return (copyout(&sf, uap->buf, sizeof(sf)));
}

int
kern_fhstatfs(struct thread *td, fhandle_t fh, struct statfs *buf)
{
	struct statfs *sp;
	struct mount *mp;
	struct vnode *vp;
	int vfslocked;
	int error;

	error = priv_check(td, PRIV_VFS_FHSTATFS);
	if (error)
		return (error);
	if ((mp = vfs_getvfs(&fh.fh_fsid)) == NULL)
		return (ESTALE);
	vfslocked = VFS_LOCK_GIANT(mp);
	error = VFS_FHTOVP(mp, &fh.fh_fid, &vp);
	if (error) {
		VFS_UNLOCK_GIANT(vfslocked);
		vfs_rel(mp);
		return (error);
	}
	vput(vp);
	error = prison_canseemount(td->td_ucred, mp);
	if (error)
		goto out;
#ifdef MAC
	error = mac_mount_check_stat(td->td_ucred, mp);
	if (error)
		goto out;
#endif
	/*
	 * Set these in case the underlying filesystem fails to do so.
	 */
	sp = &mp->mnt_stat;
	sp->f_version = STATFS_VERSION;
	sp->f_namemax = NAME_MAX;
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	error = VFS_STATFS(mp, sp, td);
	if (error == 0)
		*buf = *sp;
out:
	vfs_rel(mp);
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}
