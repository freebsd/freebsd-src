/*
 * Copyright (c) 1989, 1990 The Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)mfs_vfsops.c	7.19 (Berkeley) 4/16/91
 *	$Id: mfs_vfsops.c,v 1.3 1993/10/16 18:17:42 rgrimes Exp $
 */

#include "param.h"
#include "time.h"
#include "kernel.h"
#include "proc.h"
#include "buf.h"
#include "mount.h"
#include "signalvar.h"
#include "vnode.h"

#include "quota.h"
#include "inode.h"
#include "ufsmount.h"
#include "mfsnode.h"
#include "fs.h"

extern struct vnodeops mfs_vnodeops;

/*
 * mfs vfs operations.
 */
int mfs_mount();
int mfs_start();
int ufs_unmount();
int ufs_root();
int ufs_quotactl();
int mfs_statfs();
int ufs_sync();
int ufs_fhtovp();
int ufs_vptofh();
int mfs_init();

struct vfsops mfs_vfsops = {
	mfs_mount,
	mfs_start,
	ufs_unmount,
	ufs_root,
	ufs_quotactl,
	mfs_statfs,
	ufs_sync,
	ufs_fhtovp,
	ufs_vptofh,
	mfs_init,
};

/*
 * VFS Operations.
 *
 * mount system call
 */
/* ARGSUSED */
mfs_mount(mp, path, data, ndp, p)
	register struct mount *mp;
	char *path;
	caddr_t data;
	struct nameidata *ndp;
	struct proc *p;
{
	struct vnode *devvp;
	struct mfs_args args;
	struct ufsmount *ump;
	register struct fs *fs;
	register struct mfsnode *mfsp;
	static int mfs_minor;
	u_int size;
	int error;

	if (mp->mnt_flag & MNT_UPDATE) {
		ump = VFSTOUFS(mp);
		fs = ump->um_fs;
		if (fs->fs_ronly && (mp->mnt_flag & MNT_RDONLY) == 0)
			fs->fs_ronly = 0;
		return (0);
	}
	if (error = copyin(data, (caddr_t)&args, sizeof (struct mfs_args)))
		return (error);
	error = getnewvnode(VT_MFS, (struct mount *)0, &mfs_vnodeops, &devvp);
	if (error)
		return (error);
	devvp->v_type = VBLK;
	if (checkalias(devvp, makedev(255, mfs_minor++), (struct mount *)0))
		panic("mfs_mount: dup dev");
	mfsp = VTOMFS(devvp);
	mfsp->mfs_baseoff = args.base;
	mfsp->mfs_size = args.size;
	mfsp->mfs_vnode = devvp;
	mfsp->mfs_pid = p->p_pid;
	mfsp->mfs_buflist = (struct buf *)0;
	if (error = mountfs(devvp, mp)) {
		mfsp->mfs_buflist = (struct buf *)-1;
		vrele(devvp);
		return (error);
	}
	ump = VFSTOUFS(mp);
	fs = ump->um_fs;
	(void) copyinstr(path, fs->fs_fsmnt, sizeof(fs->fs_fsmnt) - 1, &size);
	bzero(fs->fs_fsmnt + size, sizeof(fs->fs_fsmnt) - size);
	bcopy((caddr_t)fs->fs_fsmnt, (caddr_t)mp->mnt_stat.f_mntonname,
		MNAMELEN);
	mp->mnt_stat.f_mntonname[MNAMELEN-1] = '\0';
	(void) copyinstr(args.name, mp->mnt_stat.f_mntfromname, MNAMELEN - 1,
		&size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
	(void) mfs_statfs(mp, &mp->mnt_stat);
	return (0);
}

int	mfs_pri = PWAIT | PCATCH;		/* XXX prob. temp */

/*
 * Used to grab the process and keep it in the kernel to service
 * memory filesystem I/O requests.
 *
 * Loop servicing I/O requests.
 * Copy the requested data into or out of the memory filesystem
 * address space.
 */
/* ARGSUSED */
mfs_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{
	register struct vnode *vp = VFSTOUFS(mp)->um_devvp;
	register struct mfsnode *mfsp = VTOMFS(vp);
	register struct buf *bp;
	register caddr_t base;
	int error = 0;

	base = mfsp->mfs_baseoff;
	while (mfsp->mfs_buflist != (struct buf *)(-1)) {
		while (bp = mfsp->mfs_buflist) {
			mfsp->mfs_buflist = bp->av_forw;
			mfs_doio(bp, base);
			wakeup((caddr_t)bp);
		}
		/*
		 * If a non-ignored signal is received, try to unmount.
		 * If that fails, clear the signal (it has been "processed"),
		 * otherwise we will loop here, as tsleep will always return
		 * EINTR/ERESTART.
		 */
		if (error = tsleep((caddr_t)vp, mfs_pri, "mfsidl", 0))
			if (dounmount(mp, MNT_NOFORCE, p) != 0)
				CLRSIG(p, CURSIG(p));
	}
	return (error);
}

/*
 * Get file system statistics.
 */
mfs_statfs(mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{
	int error;

	error = ufs_statfs(mp, sbp, p);
	sbp->f_type = MOUNT_MFS;
	return (error);
}
