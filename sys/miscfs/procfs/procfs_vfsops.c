/*
 * Copyright (c) 1993 Jan-Simon Pendry
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
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
 *	@(#)procfs_vfsops.c	8.7 (Berkeley) 5/10/95
 *
 * $FreeBSD: src/sys/miscfs/procfs/procfs_vfsops.c,v 1.32 1999/12/19 06:07:50 rwatson Exp $
 */

/*
 * procfs VFS interface
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <miscfs/procfs/procfs.h>

static int	procfs_mount __P((struct mount *mp, char *path, caddr_t data,
				  struct nameidata *ndp, struct proc *p));
static int	procfs_statfs __P((struct mount *mp, struct statfs *sbp,
				   struct proc *p));
static int	procfs_unmount __P((struct mount *mp, int mntflags,
				    struct proc *p));

/*
 * VFS Operations.
 *
 * mount system call
 */
/* ARGSUSED */
static int
procfs_mount(mp, path, data, ndp, p)
	struct mount *mp;
	char *path;
	caddr_t data;
	struct nameidata *ndp;
	struct proc *p;
{
	size_t size;
	int error;

	if (mp->mnt_flag & MNT_UPDATE)
		return (EOPNOTSUPP);

	if (mp->mnt_vfc->vfc_refcount == 1 && (error = at_exit(procfs_exit))) {
		printf("procfs:  cannot register procfs_exit with at_exit\n");
		return(error);
	}

	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_data = 0;
	vfs_getnewfsid(mp);

	(void) copyinstr(path, (caddr_t)mp->mnt_stat.f_mntonname, MNAMELEN, &size);
	bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);

	size = sizeof("procfs") - 1;
	bcopy("procfs", mp->mnt_stat.f_mntfromname, size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
	(void)procfs_statfs(mp, &mp->mnt_stat, p);

	return (0);
}

/*
 * unmount system call
 */
static int
procfs_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	int error;
	int flags = 0;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	error = vflush(mp, 0, flags);
	if (error)
		return (error);

	if (mp->mnt_vfc->vfc_refcount == 1)
		rm_at_exit(procfs_exit);

	return (0);
}

int
procfs_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{

	return (procfs_allocvp(mp, vpp, 0, Proot));
}

/*
 * Get file system statistics.
 */
static int
procfs_statfs(mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{
	sbp->f_bsize = PAGE_SIZE;
	sbp->f_iosize = PAGE_SIZE;
	sbp->f_blocks = 1;	/* avoid divide by zero in some df's */
	sbp->f_bfree = 0;
	sbp->f_bavail = 0;
	sbp->f_files = maxproc;			/* approx */
	sbp->f_ffree = maxproc - nprocs;	/* approx */

	if (sbp != &mp->mnt_stat) {
		sbp->f_type = mp->mnt_vfc->vfc_typenum;
		bcopy(&mp->mnt_stat.f_fsid, &sbp->f_fsid, sizeof(sbp->f_fsid));
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}

	return (0);
}

static struct vfsops procfs_vfsops = {
	procfs_mount,
	vfs_stdstart,
	procfs_unmount,
	procfs_root,
	vfs_stdquotactl,
	procfs_statfs,
	vfs_stdsync,
	vfs_stdvget,
	vfs_stdfhtovp,
	vfs_stdcheckexp,
	vfs_stdvptofh,
	vfs_stdinit,
	vfs_stduninit,
	vfs_stdextattrctl,
};

VFS_SET(procfs_vfsops, procfs, VFCF_SYNTHETIC);
