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
 * $Id: mfs_vfsops.c,v 1.42 1998/05/06 05:29:41 msmith Exp $
 */


#include "opt_mfs.h"
#include "opt_devfs.h"	/* for SLICE */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/malloc.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

#include <ufs/mfs/mfsnode.h>
#include <ufs/mfs/mfs_extern.h>

#ifdef MFS_AUTOLOAD
# include <sys/stat.h>		/* S_IFCHR */
# include <i386/i386/cons.h>	/* console IO */
#endif

MALLOC_DEFINE(M_MFSNODE, "MFS node", "MFS vnode private part");

static int	mfs_imageload __P((dev_t dev, caddr_t addr, int size));
extern int	mfs_initminiroot __P((caddr_t base));

static caddr_t	mfs_rootbase;	/* address of mini-root in kernel virtual memory */
static u_long	mfs_rootsize;	/* size of mini-root in bytes */

static	int mfs_minor;	/* used for building internal dev_t */

extern vop_t **mfs_vnodeop_p;

static int	mfs_mount __P((struct mount *mp,
			char *path, caddr_t data, struct nameidata *ndp, 
			struct proc *p));
static int	mfs_start __P((struct mount *mp, int flags, struct proc *p));
static int	mfs_statfs __P((struct mount *mp, struct statfs *sbp, 
			struct proc *p));
static int	mfs_init __P((struct vfsconf *));

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
	ffs_vptofh,
	mfs_init,
};

VFS_SET(mfs_vfsops, mfs, MOUNT_MFS, 0);

#ifdef	SLICE
	extern struct vnode	*root_device_vnode;
#endif	/* SLICE */
#ifdef MFS_ROOT

static u_char mfs_root[MFS_ROOT*1024] = "MFS Filesystem goes here";
static u_char end_mfs_root[] = "MFS Filesystem had better STOP here";

#ifdef MFS_AUTOLOAD
/*
 * XXX: UGLY UGLY UGLY!!!!!
 * XXX: This whole MFS_ROOT thing would be loads cleaner if we used
 * XXX: a "proper" ramdisk with a FFS image.
 *
 * This is a crude hack to load a floppy image into the MFS image (above)
 * in case the boot kernel did not have the root image installed due to
 * kzip size problems in 4MB. -Peter
 */
#define IMAGE_BLOCKING (32 * 1024)

static int
mfs_imageload (dev, addr, size)
	dev_t dev;
	caddr_t	addr;
	int size;
{
	int error, offset, chunk;
	int maj = major(dev);
	int mindev = minor(dev);
	struct	iovec iovec;
	struct	uio uio;

	error = (*cdevsw[maj]->d_open)(dev, 0, S_IFCHR , (struct proc *)0);
	if (error) {
		printf("mfs_imageload: could not open load device c %d,%d\n", maj, mindev);
		goto out;
	}

	offset = 0;
	while (offset < size) {

		/* pick a size to transfer, preferably a whole cylinder */
		chunk = min(size - offset, IMAGE_BLOCKING);
			
		/* do I/O to kernel space. Is there a better way ? */
		iovec.iov_base = addr + offset;
		iovec.iov_len  = chunk;
		uio.uio_iov    = &iovec;
		uio.uio_iovcnt = 1;
		uio.uio_offset = offset;
		uio.uio_resid  = chunk;
		uio.uio_segflg = UIO_SYSSPACE;
		uio.uio_rw     = UIO_READ;
		uio.uio_procp  = (struct proc *)0;

		/* perform the read request */
		error = (*cdevsw[maj]->d_read)(dev, &uio, 0);
		if (error) {
			printf("mfs_imageload: read failed! (error %d)\n", error);
			break;
		}

		/* sanity check - check there's no leftover... */
		if (uio.uio_resid != 0) {
			printf("mfs_imageload: load failing...\n");
			break;
		}

		offset += chunk;
	}

	/* close the driver */
out:
	(void)(*cdevsw[maj]->d_close)(dev, 0, S_IFCHR, (struct proc *)0);
	return (error);
}
#endif	/* MFS_AUTOLOAD */
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

#ifdef MFS_AUTOLOAD
		/*
		 * check if image was installed during build
		 * if not, attempt to load it now
		 */

		/* check for valid super block */
		if (fs->fs_magic != FS_MAGIC || fs->fs_bsize > MAXBSIZE ||
		    fs->fs_bsize < sizeof(struct fs)) {

			dev_t dev = makedev(9, 0);	/* boot floppy */
			printf("MFS image not present in boot kernel!\n");
			printf("Please replace boot disk with MFS disk...\n");
			printf("Press any key when done.\n");
			cngetc();
			mfs_imageload(dev, mfs_root, end_mfs_root - mfs_root);
		}
#endif /* MFS_AUTOLOAD */

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
#ifdef SLICE
		rootvp=root_device_vnode;
#else	/* !SLICE */
		if ((err = bdevvp(rootdev, &rootvp))) {
			printf("mfs_mountroot: can't find rootvp");
			return (err);
		}
#endif	/* !SLICE */

		/*
		 * FS specific handling
		 */
		MALLOC(mfsp, struct mfsnode *, sizeof *mfsp, M_MFSNODE, M_WAITOK);
		rootvp->v_data = mfsp;
		rootvp->v_op = mfs_vnodeop_p;
		rootvp->v_tag = VT_MFS;
		mfsp->mfs_baseoff = mfs_rootbase;
		mfsp->mfs_size = mfs_rootsize;
		mfsp->mfs_vnode = rootvp;
		mfsp->mfs_pid = p->p_pid;
		mfsp->mfs_active = 1;
		bufq_init(&mfsp->buf_queue);

		/* MFS wants to be read/write */
		mp->mnt_flag &= ~MNT_RDONLY;

		/*
		 * Attempt mount
		 */
		if( (err = ffs_mountfs(rootvp, mp, p, M_MFSNODE)) != 0 ) {
			/* fs specific cleanup (if any)*/
			rootvp->v_data = NULL;
			FREE(mfsp, M_MFSNODE);
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
			err = ffs_flushfiles(mp, flags, p);
			if (err)
				goto error_1;
		}
		if (fs->fs_ronly && (mp->mnt_kern_flag & MNTK_WANTRDWR))
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
	devvp->v_type = VBLK;
	if (checkalias(devvp, makedev(255, mfs_minor++), (struct mount *)0))
		panic("mfs_mount: dup dev");
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

	if (err = ffs_mountfs(devvp, mp, p, M_MFSNODE)) {
		mfsp->mfs_active = 0;
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
	register caddr_t base;
	register int gotsig = 0;

	base = mfsp->mfs_baseoff;
	while (mfsp->mfs_active) {
		while (bp = bufq_first(&mfsp->buf_queue)) {
			bufq_remove(&mfsp->buf_queue, bp);
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
	return (0);
}
