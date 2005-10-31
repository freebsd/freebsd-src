/*-
 * Copyright (c) 1992, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)fdesc_vfsops.c	8.4 (Berkeley) 1/21/94
 *
 * $FreeBSD$
 */

/*
 * /dev/fd Filesystem
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/vnode.h>

#include <fs/fdescfs/fdesc.h>

static MALLOC_DEFINE(M_FDESCMNT, "fdesc_mount", "FDESC mount structure");

static vfs_mount_t	fdesc_mount;
static vfs_unmount_t	fdesc_unmount;
static vfs_statfs_t	fdesc_statfs;
static vfs_root_t	fdesc_root;

/*
 * Mount the per-process file descriptors (/dev/fd)
 */
static int
fdesc_mount(struct mount *mp, struct thread *td)
{
	int error = 0;
	struct fdescmount *fmp;
	struct vnode *rvp;

	/*
	 * Update is a no-op
	 */
	if (mp->mnt_flag & (MNT_UPDATE | MNT_ROOTFS))
		return (EOPNOTSUPP);

	error = fdesc_allocvp(Froot, FD_ROOT, mp, &rvp, td);
	if (error)
		return (error);

	MALLOC(fmp, struct fdescmount *, sizeof(struct fdescmount),
				M_FDESCMNT, M_WAITOK);	/* XXX */
	rvp->v_type = VDIR;
	rvp->v_vflag |= VV_ROOT;
	fmp->f_root = rvp;
	/* XXX -- don't mark as local to work around fts() problems */
	/*mp->mnt_flag |= MNT_LOCAL;*/
	mp->mnt_data = (qaddr_t) fmp;
	vfs_getnewfsid(mp);

	vfs_mountedfrom(mp, "fdescfs");
	return (0);
}

static int
fdesc_unmount(mp, mntflags, td)
	struct mount *mp;
	int mntflags;
	struct thread *td;
{
	int error;
	int flags = 0;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	/*
	 * Clear out buffer cache.  I don't think we
	 * ever get anything cached at this level at the
	 * moment, but who knows...
	 *
	 * There is 1 extra root vnode reference corresponding
	 * to f_root.
	 */
	if ((error = vflush(mp, 1, flags, td)) != 0)
		return (error);

	/*
	 * Finally, throw away the fdescmount structure
	 */
	free(mp->mnt_data, M_FDESCMNT);	/* XXX */
	mp->mnt_data = 0;

	return (0);
}

static int
fdesc_root(mp, flags, vpp, td)
	struct mount *mp;
	int flags;
	struct vnode **vpp;
	struct thread *td;
{
	struct vnode *vp;

	/*
	 * Return locked reference to root.
	 */
	vp = VFSTOFDESC(mp)->f_root;
	VREF(vp);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	*vpp = vp;
	return (0);
}

static int
fdesc_statfs(mp, sbp, td)
	struct mount *mp;
	struct statfs *sbp;
	struct thread *td;
{
	struct filedesc *fdp;
	int lim;
	int i;
	int last;
	int freefd;

	/*
	 * Compute number of free file descriptors.
	 * [ Strange results will ensue if the open file
	 * limit is ever reduced below the current number
	 * of open files... ]
	 */
	PROC_LOCK(td->td_proc);
	lim = lim_cur(td->td_proc, RLIMIT_NOFILE);
	PROC_UNLOCK(td->td_proc);
	fdp = td->td_proc->p_fd;
	FILEDESC_LOCK_FAST(fdp);
	last = min(fdp->fd_nfiles, lim);
	freefd = 0;
	for (i = fdp->fd_freefile; i < last; i++)
		if (fdp->fd_ofiles[i] == NULL)
			freefd++;

	/*
	 * Adjust for the fact that the fdesc array may not
	 * have been fully allocated yet.
	 */
	if (fdp->fd_nfiles < lim)
		freefd += (lim - fdp->fd_nfiles);
	FILEDESC_UNLOCK_FAST(fdp);

	sbp->f_flags = 0;
	sbp->f_bsize = DEV_BSIZE;
	sbp->f_iosize = DEV_BSIZE;
	sbp->f_blocks = 2;		/* 1K to keep df happy */
	sbp->f_bfree = 0;
	sbp->f_bavail = 0;
	sbp->f_files = lim + 1;		/* Allow for "." */
	sbp->f_ffree = freefd;		/* See comments above */
	return (0);
}

static struct vfsops fdesc_vfsops = {
	.vfs_init =		fdesc_init,
	.vfs_mount =		fdesc_mount,
	.vfs_root =		fdesc_root,
	.vfs_statfs =		fdesc_statfs,
	.vfs_unmount =		fdesc_unmount,
};

VFS_SET(fdesc_vfsops, fdescfs, VFCF_SYNTHETIC);
