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
 *	@(#)mfs_vfsops.c	8.11 (Berkeley) 6/19/95
 * $FreeBSD$
 */


#include "opt_mfs.h"
#include "opt_ufs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/linker.h>

#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

#include <ufs/mfs/mfsnode.h>
#include <ufs/mfs/mfs_extern.h>

MALLOC_DEFINE(M_MFSNODE, "MFS node", "MFS vnode private part");


static int mfs_minor;		/* used for building internal dev_t */

extern vop_t **mfs_vnodeop_p;

static int	mfs_mount __P((struct mount *mp,
			char *path, caddr_t data, struct nameidata *ndp, 
			struct proc *p));
static int	mfs_start __P((struct mount *mp, int flags, struct proc *p));
static int	mfs_statfs __P((struct mount *mp, struct statfs *sbp, 
			struct proc *p));
static int	mfs_init __P((struct vfsconf *));

static struct cdevsw mfs_cdevsw = {
	/* open */      noopen,
	/* close */     noclose,
	/* read */      physread,
	/* write */     physwrite,
	/* ioctl */     noioctl,
	/* poll */      nopoll,
	/* mmap */      nommap,
	/* strategy */  nostrategy,
	/* name */      "MFS",
	/* maj */       253,
	/* dump */      nodump,
	/* psize */     nopsize,
	/* flags */     D_DISK,
};

/*
 * mfs vfs operations.
 */
static struct vfsops mfs_vfsops = {
	mfs_mount,
	mfs_start,
	ffs_unmount,
	ufs_root,
	ufs_quotactl,
	mfs_statfs,
	ffs_sync,
	ffs_vget,
	ffs_fhtovp,
	vfs_stdcheckexp,
	ffs_vptofh,
	mfs_init,
	vfs_stduninit,
#ifdef UFS_EXTATTR
	ufs_extattrctl,
#else
	vfs_stdextattrctl,
#endif
};

VFS_SET(mfs_vfsops, mfs, 0);


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
static int
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
	struct fs *fs;
	struct mfsnode *mfsp;
	size_t size;
	int flags, err;
	dev_t dev;

	/*
	 * Use NULL path to flag a root mount
	 */
	if( path == NULL) {
		/*
		 ***
		 * Mounting root file system
		 ***
		 */

		/* you loose */
		panic("mfs_mount: mount MFS as root: not configured!");
	}

	/*
	 ***
	 * Mounting non-root file system or updating a file system
	 ***
	 */

	/* copy in user arguments*/
	if ((err = copyin(data, (caddr_t)&args, sizeof (struct mfs_args))) != 0)
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
			err = ffs_flushfiles(mp, flags, p);
			if (err)
				goto error_1;
		}
		if (fs->fs_ronly && (mp->mnt_kern_flag & MNTK_WANTRDWR))
			fs->fs_ronly = 0;
		/* if not updating name...*/
		if (args.fspec == 0) {
			/*
			 * Process export requests.  Jumping to "success"
			 * will return the vfs_export() error code. 
			 */
			err = vfs_export(mp, &args.export);
			goto success;
		}

		/* XXX MFS does not support name updating*/
		goto success;
	}
	/*
	 * Do the MALLOC before the getnewvnode since doing so afterward
	 * might cause a bogus v_data pointer to get dereferenced
	 * elsewhere if MALLOC should block.
	 */
	MALLOC(mfsp, struct mfsnode *, sizeof *mfsp, M_MFSNODE, M_WAITOK);

	err = getnewvnode(VT_MFS, (struct mount *)0, mfs_vnodeop_p, &devvp);
	if (err) {
		FREE(mfsp, M_MFSNODE);
		goto error_1;
	}
	devvp->v_type = VCHR;
	dev = makedev(mfs_cdevsw.d_maj, mfs_minor);
	dev->si_devsw = &mfs_cdevsw;
	/* It is not clear that these will get initialized otherwise */
	dev->si_bsize_phys = DEV_BSIZE;
	dev->si_iosize_max = DFLTPHYS;
	devvp = addaliasu(devvp, makeudev(253, mfs_minor++));
	devvp->v_data = mfsp;
	mfsp->mfs_baseoff = args.base;
	mfsp->mfs_size = args.size;
	mfsp->mfs_vnode = devvp;
	mfsp->mfs_pid = p->p_pid;
	mfsp->mfs_active = 1;
	bufq_init(&mfsp->buf_queue);

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

	if ((err = ffs_mountfs(devvp, mp, p, M_MFSNODE)) != 0) { 
		mfsp->mfs_active = 0;
		goto error_2;
	}

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


static int	mfs_pri = PWAIT | PCATCH;		/* XXX prob. temp */

/*
 * Used to grab the process and keep it in the kernel to service
 * memory filesystem I/O requests.
 *
 * Loop servicing I/O requests.
 * Copy the requested data into or out of the memory filesystem
 * address space.
 */
/* ARGSUSED */
static int
mfs_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{
	register struct vnode *vp = VFSTOUFS(mp)->um_devvp;
	register struct mfsnode *mfsp = VTOMFS(vp);
	register struct buf *bp;
	register int gotsig = 0, sig;

	/*
	 * We must prevent the system from trying to swap
	 * out or kill ( when swap space is low, see vm/pageout.c ) the
	 * process.  A deadlock can occur if the process is swapped out,
	 * and the system can loop trying to kill the unkillable ( while
	 * references exist ) MFS process when swap space is low.
	 */
	PHOLD(curproc);

	while (mfsp->mfs_active) {
		int s;

		s = splbio();

		while ((bp = bufq_first(&mfsp->buf_queue)) != NULL) {
			bufq_remove(&mfsp->buf_queue, bp);
			splx(s);
			mfs_doio(bp, mfsp);
			wakeup((caddr_t)bp);
			s = splbio();
		}

		splx(s);

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
			if (dounmount(mp, 0, p) != 0) {
				sig = CURSIG(p);
				if (sig) {
					PROC_LOCK(p);
					SIGDELSET(p->p_siglist, sig);
					PROC_UNLOCK(p);
				}
			}
		}
		else if (tsleep((caddr_t)vp, mfs_pri, "mfsidl", 0))
			gotsig++;	/* try to unmount in next pass */
	}
	return (0);
}

/*
 * Get file system statistics.
 */
static int
mfs_statfs(mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{
	int error;

	error = ffs_statfs(mp, sbp, p);
	sbp->f_type = mp->mnt_vfc->vfc_typenum;
	return (error);
}

/*
 * Memory based filesystem initialization.
 */
static int
mfs_init(vfsp)
	struct vfsconf *vfsp;
{

	cdevsw_add(&mfs_cdevsw);
	return (0);
}

