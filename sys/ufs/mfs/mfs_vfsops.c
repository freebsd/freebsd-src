/*
 * Copyright (c) 1989, 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)mfs_vfsops.c	8.4 (Berkeley) 4/16/94
 * $Id: mfs_vfsops.c,v 1.16 1995/11/28 03:15:58 peter Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/mbuf.h>
#include <sys/file.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/malloc.h>

#include <miscfs/specfs/specdev.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

#include <ufs/mfs/mfsnode.h>
#include <ufs/mfs/mfs_extern.h>

extern int	mfs_initminiroot __P((caddr_t base));

caddr_t	mfs_rootbase;	/* address of mini-root in kernel virtual memory */
u_long	mfs_rootsize;	/* size of mini-root in bytes */

static	int mfs_minor;	/* used for building internal dev_t */

extern vop_t **mfs_vnodeop_p;

/*
 * mfs vfs operations.
 */
struct vfsops mfs_vfsops = {
	mfs_mount,
	mfs_start,
	ffs_unmount,
	ufs_root,
	ufs_quotactl,
	mfs_statfs,
	ffs_sync,
	ffs_vget,
	ffs_fhtovp,
	ffs_vptofh,
	mfs_init,
};

VFS_SET(mfs_vfsops, mfs, MOUNT_MFS, 0);

#ifdef MFS_ROOT

u_char mfs_root[MFS_ROOT*1024] = "MFS Filesystem goes here";
u_char end_mfs_root[] = "MFS Filesystem had better STOP here";

#endif	/* MFS_ROOT */

/*
 * mfs_mount
 *
 * Called when mounting local physical media
 *
 * PARAMETERS:
 *		mountroot
 *			mp	mount point structure
 *			path	NULL (flag for root mount!!!)
 *			data	<unused>
 *			ndp	<unused>
 *			p	process (user credentials check [statfs])
 *
 *		mount
 *			mp	mount point structure
 *			path	path to mount point
 *			data	pointer to argument struct in user space
 *			ndp	mount point namei() return (used for
 *				credentials on reload), reused to look
 *				up block device.
 *			p	process (user credentials check)
 *
 * RETURNS:	0	Success
 *		!0	error number (errno.h)
 *
 * LOCK STATE:
 *
 *		ENTRY
 *			mount point is locked
 *		EXIT
 *			mount point is locked
 *
 * NOTES:
 *		A NULL path can be used for a flag since the mount
 *		system call will fail with EFAULT in copyinstr in
 *		namei() if it is a genuine NULL from the user.
 */
/* ARGSUSED */
int
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
	u_int size;
	int flags, err;

	/*
	 * Use NULL path to flag a root mount
	 */
	if( path == NULL) {
		/*
		 ***
		 * Mounting root file system
		 ***
		 */

#ifdef MFS_ROOT
		/* Location of MFS/FFS superblock */
		fs = (struct fs *)(mfs_root + SBOFF);

		/* recheck for valid super block */
		if (fs->fs_magic != FS_MAGIC || fs->fs_bsize > MAXBSIZE ||
		    fs->fs_bsize < sizeof(struct fs)) {
			panic("MFS image is invalid!!");
		}

		mfs_rootbase = mfs_root;
		mfs_rootsize = fs->fs_fsize * fs->fs_size;
		rootdev = makedev(255, mfs_minor++);
		printf("rootfs is %ld Kbyte compiled in MFS\n",
		       mfs_rootsize/1024);


		/* Get vnode for root device*/
		if( bdevvp( rootdev, &rootvp))
			panic("mfs_mountroot: can't setup bdevvp for rootdev");

		/*
		 * FS specific handling
		 */
		mfsp = malloc(sizeof *mfsp, M_MFSNODE, M_WAITOK);
		rootvp->v_data = mfsp;
		rootvp->v_op = mfs_vnodeop_p;
		rootvp->v_tag = VT_MFS;
		mfsp->mfs_baseoff = mfs_rootbase;
		mfsp->mfs_size = mfs_rootsize;
		mfsp->mfs_vnode = rootvp;
		mfsp->mfs_pid = p->p_pid;
		mfsp->mfs_buflist = (struct buf *)0;

		/*
		 * Attempt mount
		 */
		if( (err = ffs_mountfs(rootvp, mp, p)) != 0 ) {
			/* fs specific cleanup (if any)*/
			rootvp->v_data = NULL;
			free(mfsp, M_MFSNODE);
			goto error_1;
		}

		goto dostatfs;		/* success*/
#else
		/* you loose */
		panic("mfs_mount: mount MFS as root: not configured!");
#endif /* MFS_ROOT */
	}

	/*
	 ***
	 * Mounting non-root file system or updating a file system
	 ***
	 */

	/* copy in user arguments*/
	if (err = copyin(data, (caddr_t)&args, sizeof (struct mfs_args)))
		goto error_1;

	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		/*
		 ********************
		 * UPDATE
		 ********************
		 */
		ump = VFSTOUFS(mp);
		fs = ump->um_fs;
		if (fs->fs_ronly == 0 && (mp->mnt_flag & MNT_RDONLY)) {
			flags = WRITECLOSE;
			if (mp->mnt_flag & MNT_FORCE)
				flags |= FORCECLOSE;
			if (vfs_busy(mp)) {
				err = EBUSY;
				goto error_1;
			}
			err = ffs_flushfiles(mp, flags, p);
			vfs_unbusy(mp);
			if (err)
				goto error_1;
		}
		if (fs->fs_ronly && (mp->mnt_flag & MNT_WANTRDWR))
			fs->fs_ronly = 0;
#ifdef EXPORTMFS
		/* if not updating name...*/
		if (args.fspec == 0) {
			/*
			 * Process export requests.  Jumping to "success"
			 * will return the vfs_export() error code. 
			 */
			err = vfs_export(mp, &ump->um_export, &args.export);
			goto success;
		}
#endif

		/* XXX MFS does not support name updating*/
		goto success;
	}
	err = getnewvnode(VT_MFS, (struct mount *)0, mfs_vnodeop_p, &devvp);
	if (err)
		goto error_1;
	devvp->v_type = VBLK;
	if (checkalias(devvp, makedev(255, mfs_minor++), (struct mount *)0))
		panic("mfs_mount: dup dev");
	mfsp = (struct mfsnode *)malloc(sizeof *mfsp, M_MFSNODE, M_WAITOK);
	devvp->v_data = mfsp;
	mfsp->mfs_baseoff = args.base;
	mfsp->mfs_size = args.size;
	mfsp->mfs_vnode = devvp;
	mfsp->mfs_pid = p->p_pid;
	mfsp->mfs_buflist = (struct buf *)0;

	/*
	 * Since this is a new mount, we want the names for
	 * the device and the mount point copied in.  If an
	 * error occurs,  the mountpoint is discarded by the
	 * upper level code.
	 */
	/* Save "last mounted on" info for mount point (NULL pad)*/
	copyinstr(	path,				/* mount point*/
			mp->mnt_stat.f_mntonname,	/* save area*/
			MNAMELEN - 1,			/* max size*/
			&size);				/* real size*/
	bzero( mp->mnt_stat.f_mntonname + size, MNAMELEN - size);

	/* Save "mounted from" info for mount point (NULL pad)*/
	copyinstr(	args.fspec,			/* device name*/
			mp->mnt_stat.f_mntfromname,	/* save area*/
			MNAMELEN - 1,			/* max size*/
			&size);				/* real size*/
	bzero( mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);

	if (err = ffs_mountfs(devvp, mp, p)) {
		mfsp->mfs_buflist = (struct buf *)-1;
		goto error_2;
	}

dostatfs:
	/*
	 * Initialize FS stat information in mount struct; uses both
	 * mp->mnt_stat.f_mntonname and mp->mnt_stat.f_mntfromname
	 *
	 * This code is common to root and non-root mounts
	 */
	(void) VFS_STATFS(mp, &mp->mnt_stat, p);

	goto success;

error_2:	/* error with devvp held*/

	/* release devvp before failing*/
	vrele(devvp);

error_1:	/* no state to back out*/

success:
	return( err);
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
int
mfs_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{
	register struct vnode *vp = VFSTOUFS(mp)->um_devvp;
	register struct mfsnode *mfsp = VTOMFS(vp);
	register struct buf *bp;
	register caddr_t base;
	register int gotsig = 0;

	base = mfsp->mfs_baseoff;
	while (mfsp->mfs_buflist != (struct buf *)(-1)) {
		while (bp = mfsp->mfs_buflist) {
			mfsp->mfs_buflist = bp->b_actf;
			mfs_doio(bp, base);
			wakeup((caddr_t)bp);
		}
		/*
		 * If a non-ignored signal is received, try to unmount.
		 * If that fails, clear the signal (it has been "processed"),
		 * otherwise we will loop here, as tsleep will always return
		 * EINTR/ERESTART.
		 */
		/*
		 * Note that dounmount() may fail if work was queued after
		 * we slept. We have to jump hoops here to make sure that we
		 * process any buffers after the sleep, before we dounmount()
		 */
		if (gotsig) {
			gotsig = 0;
			if (dounmount(mp, 0, p) != 0)
				CLRSIG(p, CURSIG(p));	/* try sleep again.. */
		}
		else if (tsleep((caddr_t)vp, mfs_pri, "mfsidl", 0))
			gotsig++;	/* try to unmount in next pass */
	}
	return (0);
}

/*
 * Get file system statistics.
 */
int
mfs_statfs(mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{
	int error;

	error = ffs_statfs(mp, sbp, p);
	sbp->f_type = MOUNT_MFS;
	return (error);
}
