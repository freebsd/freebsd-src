/*
 * Copyright (c) 1989, 1991, 1993, 1994
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
 *	@(#)ffs_vfsops.c	8.31 (Berkeley) 5/20/95
 * $Id: ffs_vfsops.c,v 1.62 1997/11/12 05:42:25 julian Exp $
 */

#include "opt_quota.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/disklabel.h>
#include <sys/malloc.h>

#include <miscfs/specfs/specdev.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

#include <vm/vm.h>
#include <vm/vm_prot.h>
#include <vm/vm_page.h>
#include <vm/vm_extern.h>

static MALLOC_DEFINE(M_FFSNODE, "FFS node", "FFS vnode private part");

static int	ffs_sbupdate __P((struct ufsmount *, int));
static int	ffs_reload __P((struct mount *,struct ucred *,struct proc *));
static int	ffs_oldfscompat __P((struct fs *));
static int	ffs_mount __P((struct mount *, char *, caddr_t,
				struct nameidata *, struct proc *));
static int	ffs_init __P((struct vfsconf *));

struct vfsops ufs_vfsops = {
	ffs_mount,
	ufs_start,
	ffs_unmount,
	ufs_root,
	ufs_quotactl,
	ffs_statfs,
	ffs_sync,
	ffs_vget,
	ffs_fhtovp,
	ffs_vptofh,
	ffs_init,
};

VFS_SET(ufs_vfsops, ufs, MOUNT_UFS, 0);

/*
 * ffs_mount
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
static int
ffs_mount( mp, path, data, ndp, p)
        struct mount		*mp;	/* mount struct pointer*/
        char			*path;	/* path to mount point*/
        caddr_t			data;	/* arguments to FS specific mount*/
        struct nameidata	*ndp;	/* mount point credentials*/
        struct proc		*p;	/* process requesting mount*/
{
	u_int		size;
	int		err = 0;
	struct vnode	*devvp;

	struct ufs_args args;
	struct ufsmount *ump = 0;
	register struct fs *fs;
	int flags;

	/*
	 * Use NULL path to flag a root mount
	 */
	if( path == NULL) {
		/*
		 ***
		 * Mounting root file system
		 ***
		 */
	
		if ((err = bdevvp(rootdev, &rootvp))) {
			printf("ffs_mountroot: can't find rootvp");
			return (err);
		}

		if (bdevsw[major(rootdev)]->d_flags & D_NOCLUSTERR)
			mp->mnt_flag |= MNT_NOCLUSTERR;
		if (bdevsw[major(rootdev)]->d_flags & D_NOCLUSTERW)
			mp->mnt_flag |= MNT_NOCLUSTERW;
		if( ( err = ffs_mountfs(rootvp, mp, p, M_FFSNODE)) != 0) {
			/* fs specific cleanup (if any)*/
			goto error_1;
		}

		goto dostatfs;		/* success*/

	}

	/*
	 ***
	 * Mounting non-root file system or updating a file system
	 ***
	 */

	/* copy in user arguments*/
	err = copyin(data, (caddr_t)&args, sizeof (struct ufs_args));
	if (err)
		goto error_1;		/* can't get arguments*/

	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 * Disallow clearing MNT_NOCLUSTERR and MNT_NOCLUSTERW flags,
	 * if block device requests.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		ump = VFSTOUFS(mp);
		fs = ump->um_fs;
		err = 0;
		if (bdevsw[major(ump->um_dev)]->d_flags & D_NOCLUSTERR)
			mp->mnt_flag |= MNT_NOCLUSTERR;
		if (bdevsw[major(ump->um_dev)]->d_flags & D_NOCLUSTERW)
			mp->mnt_flag |= MNT_NOCLUSTERW;
		if (fs->fs_ronly == 0 && (mp->mnt_flag & MNT_RDONLY)) {
			flags = WRITECLOSE;
			if (mp->mnt_flag & MNT_FORCE)
				flags |= FORCECLOSE;
			err = ffs_flushfiles(mp, flags, p);
		}
		if (!err && (mp->mnt_flag & MNT_RELOAD))
			err = ffs_reload(mp, ndp->ni_cnd.cn_cred, p);
		if (err) {
			goto error_1;
		}
		if (fs->fs_ronly && (mp->mnt_kern_flag & MNTK_WANTRDWR)) {
			if (!fs->fs_clean) {
				if (mp->mnt_flag & MNT_FORCE) {
					printf("WARNING: %s was not properly dismounted.\n",fs->fs_fsmnt);
				} else {
					printf("WARNING: R/W mount of %s denied. Filesystem is not clean - run fsck.\n",
					    fs->fs_fsmnt);
					err = EPERM;
					goto error_1;
				}
			}
			fs->fs_ronly = 0;
		}
		if (fs->fs_ronly == 0) {
			fs->fs_clean = 0;
			ffs_sbupdate(ump, MNT_WAIT);
		}
		/* if not updating name...*/
		if (args.fspec == 0) {
			/*
			 * Process export requests.  Jumping to "success"
			 * will return the vfs_export() error code.
			 */
			err = vfs_export(mp, &ump->um_export, &args.export);
			goto success;
		}
	}

	/*
	 * Not an update, or updating the name: look up the name
	 * and verify that it refers to a sensible block device.
	 */
	NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, args.fspec, p);
	err = namei(ndp);
	if (err) {
		/* can't get devvp!*/
		goto error_1;
	}

	devvp = ndp->ni_vp;

	if (devvp->v_type != VBLK) {
		err = ENOTBLK;
		goto error_2;
	}
	if (major(devvp->v_rdev) >= nblkdev) {
		err = ENXIO;
		goto error_2;
	}
	if (mp->mnt_flag & MNT_UPDATE) {
		/*
		 ********************
		 * UPDATE
		 ********************
		 */

		if (devvp != ump->um_devvp)
			err = EINVAL;	/* needs translation */
		else
			vrele(devvp);
		/*
		 * Update device name only on success
		 */
		if( !err) {
			/* Save "mounted from" info for mount point (NULL pad)*/
			copyinstr(	args.fspec,
					mp->mnt_stat.f_mntfromname,
					MNAMELEN - 1,
					&size);
			bzero( mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
		}
	} else {
		/*
		 ********************
		 * NEW MOUNT
		 ********************
		 */

		if (bdevsw[major(devvp->v_rdev)]->d_flags & D_NOCLUSTERR)
			mp->mnt_flag |= MNT_NOCLUSTERR;
		if (bdevsw[major(devvp->v_rdev)]->d_flags & D_NOCLUSTERW)
			mp->mnt_flag |= MNT_NOCLUSTERW;

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

		err = ffs_mountfs(devvp, mp, p, M_FFSNODE);
	}
	if (err) {
		goto error_2;
	}

dostatfs:
	/*
	 * Initialize FS stat information in mount struct; uses both
	 * mp->mnt_stat.f_mntonname and mp->mnt_stat.f_mntfromname
	 *
	 * This code is common to root and non-root mounts
	 */
	(void)VFS_STATFS(mp, &mp->mnt_stat, p);

	goto success;


error_2:	/* error with devvp held*/

	/* release devvp before failing*/
	vrele(devvp);

error_1:	/* no state to back out*/

success:
	return( err);
}

/*
 * Reload all incore data for a filesystem (used after running fsck on
 * the root filesystem and finding things to fix). The filesystem must
 * be mounted read-only.
 *
 * Things to do to update the mount:
 *	1) invalidate all cached meta-data.
 *	2) re-read superblock from disk.
 *	3) re-read summary information from disk.
 *	4) invalidate all inactive vnodes.
 *	5) invalidate all cached file data.
 *	6) re-read inode data for all active vnodes.
 */
static int
ffs_reload(mp, cred, p)
	register struct mount *mp;
	struct ucred *cred;
	struct proc *p;
{
	register struct vnode *vp, *nvp, *devvp;
	struct inode *ip;
	struct csum *space;
	struct buf *bp;
	struct fs *fs, *newfs;
	struct partinfo dpart;
	int i, blks, size, error;
	int32_t *lp;

	if ((mp->mnt_flag & MNT_RDONLY) == 0)
		return (EINVAL);
	/*
	 * Step 1: invalidate all cached meta-data.
	 */
	devvp = VFSTOUFS(mp)->um_devvp;
	if (vinvalbuf(devvp, 0, cred, p, 0, 0))
		panic("ffs_reload: dirty1");
	/*
	 * Step 2: re-read superblock from disk.
	 */
	if (VOP_IOCTL(devvp, DIOCGPART, (caddr_t)&dpart, FREAD, NOCRED, p) != 0)
		size = DEV_BSIZE;
	else
		size = dpart.disklab->d_secsize;
	if (error = bread(devvp, (ufs_daddr_t)(SBOFF/size), SBSIZE, NOCRED,&bp))
		return (error);
	newfs = (struct fs *)bp->b_data;
	if (newfs->fs_magic != FS_MAGIC || newfs->fs_bsize > MAXBSIZE ||
		newfs->fs_bsize < sizeof(struct fs)) {
			brelse(bp);
			return (EIO);		/* XXX needs translation */
	}
	fs = VFSTOUFS(mp)->um_fs;
	/*
	 * Copy pointer fields back into superblock before copying in	XXX
	 * new superblock. These should really be in the ufsmount.	XXX
	 * Note that important parameters (eg fs_ncg) are unchanged.
	 */
	bcopy(&fs->fs_csp[0], &newfs->fs_csp[0], sizeof(fs->fs_csp));
	newfs->fs_maxcluster = fs->fs_maxcluster;
	bcopy(newfs, fs, (u_int)fs->fs_sbsize);
	if (fs->fs_sbsize < SBSIZE)
		bp->b_flags |= B_INVAL;
	brelse(bp);
	mp->mnt_maxsymlinklen = fs->fs_maxsymlinklen;
	ffs_oldfscompat(fs);

	/*
	 * Step 3: re-read summary information from disk.
	 */
	blks = howmany(fs->fs_cssize, fs->fs_fsize);
	space = fs->fs_csp[0];
	for (i = 0; i < blks; i += fs->fs_frag) {
		size = fs->fs_bsize;
		if (i + fs->fs_frag > blks)
			size = (blks - i) * fs->fs_fsize;
		error = bread(devvp, fsbtodb(fs, fs->fs_csaddr + i), size,
		    NOCRED, &bp);
		if (error)
			return (error);
		bcopy(bp->b_data, fs->fs_csp[fragstoblks(fs, i)], (u_int)size);
		brelse(bp);
	}
	/*
	 * We no longer know anything about clusters per cylinder group.
	 */
	if (fs->fs_contigsumsize > 0) {
		lp = fs->fs_maxcluster;
		for (i = 0; i < fs->fs_ncg; i++)
			*lp++ = fs->fs_contigsumsize;
	}

loop:
	simple_lock(&mntvnode_slock);
	for (vp = mp->mnt_vnodelist.lh_first; vp != NULL; vp = nvp) {
		if (vp->v_mount != mp) {
			simple_unlock(&mntvnode_slock);
			goto loop;
		}
		nvp = vp->v_mntvnodes.le_next;
		/*
		 * Step 4: invalidate all inactive vnodes.
		 */
		if (vrecycle(vp, &mntvnode_slock, p))
			goto loop;
		/*
		 * Step 5: invalidate all cached file data.
		 */
		simple_lock(&vp->v_interlock);
		simple_unlock(&mntvnode_slock);
		if (vget(vp, LK_EXCLUSIVE | LK_INTERLOCK, p)) {
			goto loop;
		}
		if (vinvalbuf(vp, 0, cred, p, 0, 0))
			panic("ffs_reload: dirty2");
		/*
		 * Step 6: re-read inode data for all active vnodes.
		 */
		ip = VTOI(vp);
		error =
		    bread(devvp, fsbtodb(fs, ino_to_fsba(fs, ip->i_number)),
		    (int)fs->fs_bsize, NOCRED, &bp);
		if (error) {
			vput(vp);
			return (error);
		}
		ip->i_din = *((struct dinode *)bp->b_data +
		    ino_to_fsbo(fs, ip->i_number));
		brelse(bp);
		vput(vp);
		simple_lock(&mntvnode_slock);
	}
	simple_unlock(&mntvnode_slock);
	return (0);
}

/*
 * Common code for mount and mountroot
 */
int
ffs_mountfs(devvp, mp, p, malloctype)
	register struct vnode *devvp;
	struct mount *mp;
	struct proc *p;
	struct malloc_type *malloctype;
{
	register struct ufsmount *ump;
	struct buf *bp;
	register struct fs *fs;
	dev_t dev;
	struct partinfo dpart;
	caddr_t base, space;
	int error, i, blks, size, ronly;
	int32_t *lp;
	struct ucred *cred;
	u_int64_t maxfilesize;					/* XXX */
	u_int strsize;
	int ncount;

	dev = devvp->v_rdev;
	cred = p ? p->p_ucred : NOCRED;
	/*
	 * Disallow multiple mounts of the same device.
	 * Disallow mounting of a device that is currently in use
	 * (except for root, which might share swap device for miniroot).
	 * Flush out any old buffers remaining from a previous use.
	 */
	error = vfs_mountedon(devvp);
	if (error)
		return (error);
	ncount = vcount(devvp);
	if (devvp->v_object)
		ncount -= 1;
	if (ncount > 1 && devvp != rootvp)
		return (EBUSY);
	if (error = vinvalbuf(devvp, V_SAVE, cred, p, 0, 0))
		return (error);

	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	error = VOP_OPEN(devvp, ronly ? FREAD : FREAD|FWRITE, FSCRED, p);
	if (error)
		return (error);
	if (VOP_IOCTL(devvp, DIOCGPART, (caddr_t)&dpart, FREAD, cred, p) != 0)
		size = DEV_BSIZE;
	else
		size = dpart.disklab->d_secsize;

	bp = NULL;
	ump = NULL;
	if (error = bread(devvp, SBLOCK, SBSIZE, cred, &bp))
		goto out;
	fs = (struct fs *)bp->b_data;
	if (fs->fs_magic != FS_MAGIC || fs->fs_bsize > MAXBSIZE ||
	    fs->fs_bsize < sizeof(struct fs)) {
		error = EINVAL;		/* XXX needs translation */
		goto out;
	}
	fs->fs_fmod = 0;
	if (!fs->fs_clean) {
		if (ronly || (mp->mnt_flag & MNT_FORCE)) {
			printf("WARNING: %s was not properly dismounted.\n",fs->fs_fsmnt);
		} else {
			printf("WARNING: R/W mount of %s denied. Filesystem is not clean - run fsck.\n",fs->fs_fsmnt);
			error = EPERM;
			goto out;
		}
	}
	/* XXX updating 4.2 FFS superblocks trashes rotational layout tables */
	if (fs->fs_postblformat == FS_42POSTBLFMT && !ronly) {
		error = EROFS;          /* needs translation */
		goto out;
	}
	ump = malloc(sizeof *ump, M_UFSMNT, M_WAITOK);
	bzero((caddr_t)ump, sizeof *ump);
	ump->um_malloctype = malloctype;
	ump->um_fs = malloc((u_long)fs->fs_sbsize, M_UFSMNT,
	    M_WAITOK);
	ump->um_blkatoff = ffs_blkatoff;
	ump->um_truncate = ffs_truncate;
	ump->um_update = ffs_update;
	ump->um_valloc = ffs_valloc;
	ump->um_vfree = ffs_vfree;
	bcopy(bp->b_data, ump->um_fs, (u_int)fs->fs_sbsize);
	if (fs->fs_sbsize < SBSIZE)
		bp->b_flags |= B_INVAL;
	brelse(bp);
	bp = NULL;
	fs = ump->um_fs;
	fs->fs_ronly = ronly;
	if (ronly == 0) {
		fs->fs_fmod = 1;
		fs->fs_clean = 0;
	}
	size = fs->fs_cssize;
	blks = howmany(size, fs->fs_fsize);
	if (fs->fs_contigsumsize > 0)
		size += fs->fs_ncg * sizeof(int32_t);
	base = space = malloc((u_long)size, M_UFSMNT, M_WAITOK);
	for (i = 0; i < blks; i += fs->fs_frag) {
		size = fs->fs_bsize;
		if (i + fs->fs_frag > blks)
			size = (blks - i) * fs->fs_fsize;
		if (error = bread(devvp, fsbtodb(fs, fs->fs_csaddr + i), size,
		    cred, &bp)) {
			free(base, M_UFSMNT);
			goto out;
		}
		bcopy(bp->b_data, space, (u_int)size);
		fs->fs_csp[fragstoblks(fs, i)] = (struct csum *)space;
		space += size;
		brelse(bp);
		bp = NULL;
	}
	if (fs->fs_contigsumsize > 0) {
		fs->fs_maxcluster = lp = (int32_t *)space;
		for (i = 0; i < fs->fs_ncg; i++)
			*lp++ = fs->fs_contigsumsize;
	}
	mp->mnt_data = (qaddr_t)ump;
	mp->mnt_stat.f_fsid.val[0] = (long)dev;
	if (fs->fs_id[0] != 0 && fs->fs_id[1] != 0)
		mp->mnt_stat.f_fsid.val[1] = fs->fs_id[1];
	else
		mp->mnt_stat.f_fsid.val[1] = mp->mnt_vfc->vfc_typenum;
	mp->mnt_maxsymlinklen = fs->fs_maxsymlinklen;
	mp->mnt_flag |= MNT_LOCAL;
	ump->um_mountp = mp;
	ump->um_dev = dev;
	ump->um_devvp = devvp;
	ump->um_nindir = fs->fs_nindir;
	ump->um_bptrtodb = fs->fs_fsbtodb;
	ump->um_seqinc = fs->fs_frag;
	for (i = 0; i < MAXQUOTAS; i++)
		ump->um_quotas[i] = NULLVP;
	devvp->v_specflags |= SI_MOUNTEDON;
	ffs_oldfscompat(fs);

	/*
	 * Set FS local "last mounted on" information (NULL pad)
	 */
	copystr(	mp->mnt_stat.f_mntonname,	/* mount point*/
			fs->fs_fsmnt,			/* copy area*/
			sizeof(fs->fs_fsmnt) - 1,	/* max size*/
			&strsize);			/* real size*/
	bzero( fs->fs_fsmnt + strsize, sizeof(fs->fs_fsmnt) - strsize);

	if( mp->mnt_flag & MNT_ROOTFS) {
		/*
		 * Root mount; update timestamp in mount structure.
		 * this will be used by the common root mount code
		 * to update the system clock.
		 */
		mp->mnt_time = fs->fs_time;
	}

	ump->um_savedmaxfilesize = fs->fs_maxfilesize;		/* XXX */
	maxfilesize = (u_int64_t)0x40000000 * fs->fs_bsize - 1;	/* XXX */
	if (fs->fs_maxfilesize > maxfilesize)			/* XXX */
		fs->fs_maxfilesize = maxfilesize;		/* XXX */
	if (ronly == 0) {
		fs->fs_clean = 0;
		(void) ffs_sbupdate(ump, MNT_WAIT);
	}
	/*
	 * Only VMIO the backing device if the backing device is a real
	 * block device.  This excludes the original MFS implementation.
	 * Note that it is optional that the backing device be VMIOed.  This
	 * increases the opportunity for metadata caching.
	 */
	if ((devvp->v_type == VBLK) && (major(devvp->v_rdev) < nblkdev)) {
		vfs_object_create(devvp, p, p->p_ucred, 0);
	}
	return (0);
out:
	if (bp)
		brelse(bp);
	(void)VOP_CLOSE(devvp, ronly ? FREAD : FREAD|FWRITE, cred, p);
	if (ump) {
		free(ump->um_fs, M_UFSMNT);
		free(ump, M_UFSMNT);
		mp->mnt_data = (qaddr_t)0;
	}
	return (error);
}

/*
 * Sanity checks for old file systems.
 *
 * XXX - goes away some day.
 */
static int
ffs_oldfscompat(fs)
	struct fs *fs;
{

	fs->fs_npsect = max(fs->fs_npsect, fs->fs_nsect);	/* XXX */
	fs->fs_interleave = max(fs->fs_interleave, 1);		/* XXX */
	if (fs->fs_postblformat == FS_42POSTBLFMT)		/* XXX */
		fs->fs_nrpos = 8;				/* XXX */
	if (fs->fs_inodefmt < FS_44INODEFMT) {			/* XXX */
#if 0
		int i;						/* XXX */
		u_int64_t sizepb = fs->fs_bsize;		/* XXX */
								/* XXX */
		fs->fs_maxfilesize = fs->fs_bsize * NDADDR - 1;	/* XXX */
		for (i = 0; i < NIADDR; i++) {			/* XXX */
			sizepb *= NINDIR(fs);			/* XXX */
			fs->fs_maxfilesize += sizepb;		/* XXX */
		}						/* XXX */
#endif
		fs->fs_maxfilesize = (u_quad_t) 1LL << 39;
		fs->fs_qbmask = ~fs->fs_bmask;			/* XXX */
		fs->fs_qfmask = ~fs->fs_fmask;			/* XXX */
	}							/* XXX */
	return (0);
}

/*
 * unmount system call
 */
int
ffs_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	register struct ufsmount *ump;
	register struct fs *fs;
	int error, flags;

	flags = 0;
	if (mntflags & MNT_FORCE) {
		flags |= FORCECLOSE;
	}
	error = ffs_flushfiles(mp, flags, p);
	if (error)
		return (error);
	ump = VFSTOUFS(mp);
	fs = ump->um_fs;
	if (fs->fs_ronly == 0) {
		fs->fs_clean = 1;
		error = ffs_sbupdate(ump, MNT_WAIT);
		if (error) {
			fs->fs_clean = 0;
			return (error);
		}
	}
	ump->um_devvp->v_specflags &= ~SI_MOUNTEDON;

	error = VOP_CLOSE(ump->um_devvp, fs->fs_ronly ? FREAD : FREAD|FWRITE,
		NOCRED, p);

	vrele(ump->um_devvp);

	free(fs->fs_csp[0], M_UFSMNT);
	free(fs, M_UFSMNT);
	free(ump, M_UFSMNT);
	mp->mnt_data = (qaddr_t)0;
	mp->mnt_flag &= ~MNT_LOCAL;
	return (error);
}

/*
 * Flush out all the files in a filesystem.
 */
int
ffs_flushfiles(mp, flags, p)
	register struct mount *mp;
	int flags;
	struct proc *p;
{
	register struct ufsmount *ump;
	int error;

	ump = VFSTOUFS(mp);
#ifdef QUOTA
	if (mp->mnt_flag & MNT_QUOTA) {
		int i;
		error = vflush(mp, NULLVP, SKIPSYSTEM|flags);
		if (error)
			return (error);
		for (i = 0; i < MAXQUOTAS; i++) {
			if (ump->um_quotas[i] == NULLVP)
				continue;
			quotaoff(p, mp, i);
		}
		/*
		 * Here we fall through to vflush again to ensure
		 * that we have gotten rid of all the system vnodes.
		 */
	}
#endif
	error = vflush(mp, NULLVP, flags);
	return (error);
}

/*
 * Get file system statistics.
 */
int
ffs_statfs(mp, sbp, p)
	struct mount *mp;
	register struct statfs *sbp;
	struct proc *p;
{
	register struct ufsmount *ump;
	register struct fs *fs;

	ump = VFSTOUFS(mp);
	fs = ump->um_fs;
	if (fs->fs_magic != FS_MAGIC)
		panic("ffs_statfs");
	sbp->f_bsize = fs->fs_fsize;
	sbp->f_iosize = fs->fs_bsize;
	sbp->f_blocks = fs->fs_dsize;
	sbp->f_bfree = fs->fs_cstotal.cs_nbfree * fs->fs_frag +
		fs->fs_cstotal.cs_nffree;
	sbp->f_bavail = freespace(fs, fs->fs_minfree);
	sbp->f_files =  fs->fs_ncg * fs->fs_ipg - ROOTINO;
	sbp->f_ffree = fs->fs_cstotal.cs_nifree;
	if (sbp != &mp->mnt_stat) {
		sbp->f_type = mp->mnt_vfc->vfc_typenum;
		bcopy((caddr_t)mp->mnt_stat.f_mntonname,
			(caddr_t)&sbp->f_mntonname[0], MNAMELEN);
		bcopy((caddr_t)mp->mnt_stat.f_mntfromname,
			(caddr_t)&sbp->f_mntfromname[0], MNAMELEN);
	}
	return (0);
}

/*
 * Go through the disk queues to initiate sandbagged IO;
 * go through the inodes to write those that have been modified;
 * initiate the writing of the super block if it has been modified.
 *
 * Note: we are always called with the filesystem marked `MPBUSY'.
 */
int
ffs_sync(mp, waitfor, cred, p)
	struct mount *mp;
	int waitfor;
	struct ucred *cred;
	struct proc *p;
{
	struct vnode *nvp, *vp;
	struct inode *ip;
	struct ufsmount *ump = VFSTOUFS(mp);
	struct fs *fs;
	struct timeval tv;
	int error, allerror = 0;

	fs = ump->um_fs;
	if (fs->fs_fmod != 0 && fs->fs_ronly != 0) {		/* XXX */
		printf("fs = %s\n", fs->fs_fsmnt);
		panic("ffs_sync: rofs mod");
	}
	/*
	 * Write back each (modified) inode.
	 */
	simple_lock(&mntvnode_slock);
loop:
	for (vp = mp->mnt_vnodelist.lh_first; vp != NULL; vp = nvp) {
		/*
		 * If the vnode that we are about to sync is no longer
		 * associated with this mount point, start over.
		 */
		if (vp->v_mount != mp)
			goto loop;
		simple_lock(&vp->v_interlock);
		nvp = vp->v_mntvnodes.le_next;
		ip = VTOI(vp);
		if (((ip->i_flag &
		     (IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE)) == 0) &&
		    vp->v_dirtyblkhd.lh_first == NULL) {
			simple_unlock(&vp->v_interlock);
			continue;
		}
		if (vp->v_type != VCHR) {
			simple_unlock(&mntvnode_slock);
			error =
			  vget(vp, LK_EXCLUSIVE | LK_NOWAIT | LK_INTERLOCK, p);
			if (error) {
				simple_lock(&mntvnode_slock);
				if (error == ENOENT)
					goto loop;
				continue;
			}
			if (error = VOP_FSYNC(vp, cred, waitfor, p))
				allerror = error;
			VOP_UNLOCK(vp, 0, p);
			vrele(vp);
			simple_lock(&mntvnode_slock);
		} else {
			simple_unlock(&mntvnode_slock);
			simple_unlock(&vp->v_interlock);
			gettime(&tv);
			/* UFS_UPDATE(vp, &tv, &tv, waitfor == MNT_WAIT); */
			UFS_UPDATE(vp, &tv, &tv, 0);
			simple_lock(&mntvnode_slock);
		}
	}
	simple_unlock(&mntvnode_slock);
	/*
	 * Force stale file system control information to be flushed.
	 */
	error = VOP_FSYNC(ump->um_devvp, cred, waitfor, p);
	if (error)
		allerror = error;
#ifdef QUOTA
	qsync(mp);
#endif
	/*
	 * Write back modified superblock.
	 */
	if (fs->fs_fmod != 0) {
		fs->fs_fmod = 0;
		fs->fs_time = time.tv_sec;
		if (error = ffs_sbupdate(ump, waitfor))
			allerror = error;
	}
	return (allerror);
}

/*
 * Look up a FFS dinode number to find its incore vnode, otherwise read it
 * in from disk.  If it is in core, wait for the lock bit to clear, then
 * return the inode locked.  Detection and handling of mount points must be
 * done by the calling routine.
 */
static int ffs_inode_hash_lock;

int
ffs_vget(mp, ino, vpp)
	struct mount *mp;
	ino_t ino;
	struct vnode **vpp;
{
	struct fs *fs;
	struct inode *ip;
	struct ufsmount *ump;
	struct buf *bp;
	struct vnode *vp;
	dev_t dev;
	int error;

	ump = VFSTOUFS(mp);
	dev = ump->um_dev;
restart:
	if ((*vpp = ufs_ihashget(dev, ino)) != NULL)
		return (0);

	/*
	 * Lock out the creation of new entries in the FFS hash table in
	 * case getnewvnode() or MALLOC() blocks, otherwise a duplicate
	 * may occur!
	 */
	if (ffs_inode_hash_lock) {
		while (ffs_inode_hash_lock) {
			ffs_inode_hash_lock = -1;
			tsleep(&ffs_inode_hash_lock, PVM, "ffsvgt", 0);
		}
		goto restart;
	}
	ffs_inode_hash_lock = 1;

	/*
	 * If this MALLOC() is performed after the getnewvnode()
	 * it might block, leaving a vnode with a NULL v_data to be
	 * found by ffs_sync() if a sync happens to fire right then,
	 * which will cause a panic because ffs_sync() blindly
	 * dereferences vp->v_data (as well it should).
	 */
	MALLOC(ip, struct inode *, sizeof(struct inode), 
	    ump->um_malloctype, M_WAITOK);

	/* Allocate a new vnode/inode. */
	error = getnewvnode(VT_UFS, mp, ffs_vnodeop_p, &vp);
	if (error) {
		if (ffs_inode_hash_lock < 0)
			wakeup(&ffs_inode_hash_lock);
		ffs_inode_hash_lock = 0;
		*vpp = NULL;
		FREE(ip, ump->um_malloctype);
		return (error);
	}
	bzero((caddr_t)ip, sizeof(struct inode));
	lockinit(&ip->i_lock, PINOD, "inode", 0, 0);
	vp->v_data = ip;
	ip->i_vnode = vp;
	ip->i_fs = fs = ump->um_fs;
	ip->i_dev = dev;
	ip->i_number = ino;
#ifdef QUOTA
	{
		int i;
		for (i = 0; i < MAXQUOTAS; i++)
			ip->i_dquot[i] = NODQUOT;
	}
#endif
	/*
	 * Put it onto its hash chain and lock it so that other requests for
	 * this inode will block if they arrive while we are sleeping waiting
	 * for old data structures to be purged or for the contents of the
	 * disk portion of this inode to be read.
	 */
	ufs_ihashins(ip);

	if (ffs_inode_hash_lock < 0)
		wakeup(&ffs_inode_hash_lock);
	ffs_inode_hash_lock = 0;

	/* Read in the disk contents for the inode, copy into the inode. */
	error = bread(ump->um_devvp, fsbtodb(fs, ino_to_fsba(fs, ino)),
	    (int)fs->fs_bsize, NOCRED, &bp);
	if (error) {
		/*
		 * The inode does not contain anything useful, so it would
		 * be misleading to leave it on its hash chain. With mode
		 * still zero, it will be unlinked and returned to the free
		 * list by vput().
		 */
		brelse(bp);
		vput(vp);
		*vpp = NULL;
		return (error);
	}
	ip->i_din = *((struct dinode *)bp->b_data + ino_to_fsbo(fs, ino));
	bqrelse(bp);

	/*
	 * Initialize the vnode from the inode, check for aliases.
	 * Note that the underlying vnode may have changed.
	 */
	error = ufs_vinit(mp, ffs_specop_p, ffs_fifoop_p, &vp);
	if (error) {
		vput(vp);
		*vpp = NULL;
		return (error);
	}
	/*
	 * Finish inode initialization now that aliasing has been resolved.
	 */
	ip->i_devvp = ump->um_devvp;
	VREF(ip->i_devvp);
	/*
	 * Set up a generation number for this inode if it does not
	 * already have one. This should only happen on old filesystems.
	 */
	if (ip->i_gen == 0) {
		ip->i_gen = random() / 2 + 1;
		if ((vp->v_mount->mnt_flag & MNT_RDONLY) == 0)
			ip->i_flag |= IN_MODIFIED;
	}
	/*
	 * Ensure that uid and gid are correct. This is a temporary
	 * fix until fsck has been changed to do the update.
	 */
	if (fs->fs_inodefmt < FS_44INODEFMT) {		/* XXX */
		ip->i_uid = ip->i_din.di_ouid;		/* XXX */
		ip->i_gid = ip->i_din.di_ogid;		/* XXX */
	}						/* XXX */

	*vpp = vp;
	return (0);
}

/*
 * File handle to vnode
 *
 * Have to be really careful about stale file handles:
 * - check that the inode number is valid
 * - call ffs_vget() to get the locked inode
 * - check for an unallocated inode (i_mode == 0)
 * - check that the given client host has export rights and return
 *   those rights via. exflagsp and credanonp
 */
int
ffs_fhtovp(mp, fhp, nam, vpp, exflagsp, credanonp)
	register struct mount *mp;
	struct fid *fhp;
	struct sockaddr *nam;
	struct vnode **vpp;
	int *exflagsp;
	struct ucred **credanonp;
{
	register struct ufid *ufhp;
	struct fs *fs;

	ufhp = (struct ufid *)fhp;
	fs = VFSTOUFS(mp)->um_fs;
	if (ufhp->ufid_ino < ROOTINO ||
	    ufhp->ufid_ino >= fs->fs_ncg * fs->fs_ipg)
		return (ESTALE);
	return (ufs_check_export(mp, ufhp, nam, vpp, exflagsp, credanonp));
}

/*
 * Vnode pointer to File handle
 */
/* ARGSUSED */
int
ffs_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{
	register struct inode *ip;
	register struct ufid *ufhp;

	ip = VTOI(vp);
	ufhp = (struct ufid *)fhp;
	ufhp->ufid_len = sizeof(struct ufid);
	ufhp->ufid_ino = ip->i_number;
	ufhp->ufid_gen = ip->i_gen;
	return (0);
}

/*
 * Initialize the filesystem; just use ufs_init.
 */
static int
ffs_init(vfsp)
	struct vfsconf *vfsp;
{

	return (ufs_init(vfsp));
}

/*
 * Write a superblock and associated information back to disk.
 */
static int
ffs_sbupdate(mp, waitfor)
	struct ufsmount *mp;
	int waitfor;
{
	register struct fs *dfs, *fs = mp->um_fs;
	register struct buf *bp;
	int blks;
	caddr_t space;
	int i, size, error, allerror = 0;

	/*
	 * First write back the summary information.
	 */
	blks = howmany(fs->fs_cssize, fs->fs_fsize);
	space = (caddr_t)fs->fs_csp[0];
	for (i = 0; i < blks; i += fs->fs_frag) {
		size = fs->fs_bsize;
		if (i + fs->fs_frag > blks)
			size = (blks - i) * fs->fs_fsize;
		bp = getblk(mp->um_devvp, fsbtodb(fs, fs->fs_csaddr + i),
		    size, 0, 0);
		bcopy(space, bp->b_data, (u_int)size);
		space += size;
		if (waitfor != MNT_WAIT)
			bawrite(bp);
		else if (error = bwrite(bp))
			allerror = error;
	}
	/*
	 * Now write back the superblock itself. If any errors occurred
	 * up to this point, then fail so that the superblock avoids
	 * being written out as clean.
	 */
	if (allerror)
		return (allerror);
	bp = getblk(mp->um_devvp, SBLOCK, (int)fs->fs_sbsize, 0, 0);
	bcopy((caddr_t)fs, bp->b_data, (u_int)fs->fs_sbsize);
	/* Restore compatibility to old file systems.		   XXX */
	dfs = (struct fs *)bp->b_data;				/* XXX */
	if (fs->fs_postblformat == FS_42POSTBLFMT)		/* XXX */
		dfs->fs_nrpos = -1;				/* XXX */
	if (fs->fs_inodefmt < FS_44INODEFMT) {			/* XXX */
		int32_t *lp, tmp;				/* XXX */
								/* XXX */
		lp = (int32_t *)&dfs->fs_qbmask;		/* XXX */
		tmp = lp[4];					/* XXX */
		for (i = 4; i > 0; i--)				/* XXX */
			lp[i] = lp[i-1];			/* XXX */
		lp[0] = tmp;					/* XXX */
	}							/* XXX */
	dfs->fs_maxfilesize = mp->um_savedmaxfilesize;		/* XXX */
	if (waitfor != MNT_WAIT)
		bawrite(bp);
	else if (error = bwrite(bp))
		allerror = error;
	return (allerror);
}
