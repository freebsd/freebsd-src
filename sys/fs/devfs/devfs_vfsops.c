/*
 * Copyright (c) 1992, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2000
 *	Poul-Henning Kamp.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Neither the name of the University nor the names of its contributors
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
 *	@(#)kernfs_vfsops.c	8.10 (Berkeley) 5/14/95
 * From: FreeBSD: src/sys/miscfs/kernfs/kernfs_vfsops.c 1.36
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/eventhandler.h>

#define DEVFS_INTERN
#include <fs/devfs/devfs.h>

MALLOC_DEFINE(M_DEVFS, "DEVFS", "DEVFS data");

static int	devfs_mount __P((struct mount *mp, char *path, caddr_t data,
				  struct nameidata *ndp, struct proc *p));
static int	devfs_unmount __P((struct mount *mp, int mntflags,
				  struct proc *p));
static int	devfs_root __P((struct mount *mp, struct vnode **vpp));
static int	devfs_statfs __P((struct mount *mp, struct statfs *sbp,
				   struct proc *p));

/*
 * Mount the filesystem
 */
static int
devfs_mount(mp, path, data, ndp, p)
	struct mount *mp;
	char *path;
	caddr_t data;
	struct nameidata *ndp;
	struct proc *p;
{
	int error;
	u_int size;
	struct devfs_mount *fmp;
	struct vnode *rvp;

	error = 0;
	/*
	 * XXX: flag changes.
	 */
	if (mp->mnt_flag & MNT_UPDATE)
		return (EOPNOTSUPP);

	MALLOC(fmp, struct devfs_mount *, sizeof(struct devfs_mount),
	    M_DEVFS, M_WAITOK);
	bzero(fmp, sizeof(*fmp));

	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_data = (qaddr_t) fmp;
	vfs_getnewfsid(mp);

	fmp->dm_inode = NDEVINO;

	fmp->dm_rootdir = devfs_vmkdir("(root)", 6, NULL);
	fmp->dm_rootdir->de_inode = 2;
	fmp->dm_basedir = fmp->dm_rootdir;

	error = devfs_root(mp, &rvp);
	if (error) {
		FREE(fmp, M_DEVFS);
		return (error);
	}
	VOP_UNLOCK(rvp, 0, p);

	if (path != NULL) {
		(void) copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size);
	} else {
		strcpy(mp->mnt_stat.f_mntonname, "/");
		size = 1;
	}
	bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);
	bzero(mp->mnt_stat.f_mntfromname, MNAMELEN);
	bcopy("devfs", mp->mnt_stat.f_mntfromname, sizeof("devfs"));
	(void)devfs_statfs(mp, &mp->mnt_stat, p);
	devfs_populate(fmp);

	return (0);
}

static int
devfs_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	int error;
	int flags = 0;
	struct vnode *rootvp;
	struct devfs_mount *fmp;

	error = devfs_root(mp, &rootvp);
	if (error)
		return (error);
	fmp = VFSTODEVFS(mp);
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;
	if (rootvp->v_usecount > 2)
		return (EBUSY);
	devfs_purge(fmp->dm_rootdir);
	error = vflush(mp, rootvp, flags);
	if (error)
		return (error);
	vput(rootvp);
	vrele(rootvp);
	vgone(rootvp);
	mp->mnt_data = 0;
	free(fmp, M_DEVFS);
	return 0;
}

/* Return locked reference to root.  */

static int
devfs_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	int error;
	struct proc *p;
	struct vnode *vp;
	struct devfs_mount *dmp;

	p = curproc;					/* XXX */
	dmp = VFSTODEVFS(mp);
	error = devfs_allocv(dmp->dm_rootdir, mp, &vp, p);
	if (error)
		return (error);
	vp->v_flag |= VROOT;
	*vpp = vp;
	return (0);
}

static int
devfs_statfs(mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{

	sbp->f_flags = 0;
	sbp->f_bsize = DEV_BSIZE;
	sbp->f_iosize = DEV_BSIZE;
	sbp->f_blocks = 2;		/* 1K to keep df happy */
	sbp->f_bfree = 0;
	sbp->f_bavail = 0;
	sbp->f_files = 0;
	sbp->f_ffree = 0;
	if (sbp != &mp->mnt_stat) {
		sbp->f_type = mp->mnt_vfc->vfc_typenum;
		bcopy(&mp->mnt_stat.f_fsid, &sbp->f_fsid, sizeof(sbp->f_fsid));
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}
	return (0);
}

static struct vfsops devfs_vfsops = {
	devfs_mount,
	vfs_stdstart,
	devfs_unmount,
	devfs_root,
	vfs_stdquotactl,
	devfs_statfs,
	vfs_stdsync,
	vfs_stdvget,
	vfs_stdfhtovp,
	vfs_stdcheckexp,
	vfs_stdvptofh,
	vfs_stdinit,
	vfs_stduninit,
	vfs_stdextattrctl,
};

VFS_SET(devfs_vfsops, devfs, VFCF_SYNTHETIC);
