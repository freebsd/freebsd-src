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
 *	$Id: procfs_vfsops.c,v 1.16 1997/08/02 14:32:19 bde Exp $
 */

/*
 * procfs VFS interface
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <miscfs/procfs/procfs.h>

static int	procfs_init __P((struct vfsconf *vfsp));
static int	procfs_mount __P((struct mount *mp, char *path, caddr_t data,
				  struct nameidata *ndp, struct proc *p));
static int	procfs_start __P((struct mount *mp, int flags, struct proc *p));
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
	u_int size;

	if (UIO_MX & (UIO_MX-1)) {
		log(LOG_ERR, "procfs: invalid directory entry size\n");
		return (EINVAL);
	}

	if (mp->mnt_flag & MNT_UPDATE)
		return (EOPNOTSUPP);

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

	return (0);
}

int
procfs_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{

	return (procfs_allocvp(mp, vpp, 0, Proot));
}

/* ARGSUSED */
static int
procfs_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{

	return (0);
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

static int
procfs_init(vfsp)
	struct vfsconf *vfsp;
{

	return (0);
}

#define procfs_fhtovp ((int (*) __P((struct mount *, struct fid *, \
	    struct sockaddr *, struct vnode **, int *, struct ucred **)))einval)
#define procfs_quotactl ((int (*) __P((struct mount *, int, uid_t, caddr_t, \
	    struct proc *)))eopnotsupp)
#define procfs_sync ((int (*) __P((struct mount *, int, struct ucred *, \
	    struct proc *)))nullop)
#define procfs_sysctl ((int (*) __P((int *, u_int, void *, size_t *, void *, \
	    size_t, struct proc *)))eopnotsupp)
#define procfs_vget ((int (*) __P((struct mount *, ino_t, struct vnode **))) \
	    eopnotsupp)
#define procfs_vptofh ((int (*) __P((struct vnode *, struct fid *)))einval)

static struct vfsops procfs_vfsops = {
	procfs_mount,
	procfs_start,
	procfs_unmount,
	procfs_root,
	procfs_quotactl,
	procfs_statfs,
	procfs_sync,
	procfs_vget,
	procfs_fhtovp,
	procfs_vptofh,
	procfs_init,
};

VFS_SET(procfs_vfsops, procfs, MOUNT_PROCFS, VFCF_SYNTHETIC);
