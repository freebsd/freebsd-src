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
 *	@(#)ffs_vfsops.c	8.8 (Berkeley) 4/18/94
 * $Id: ffs_vfsops.c,v 1.41 1996/09/07 17:34:57 dyson Exp $
 */

#include "opt_quota.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/mbuf.h>
#include <sys/file.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>

#include <miscfs/specfs/specdev.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>

static int	ffs_sbupdate __P((struct ufsmount *, int));
static int	ffs_reload __P((struct mount *,struct ucred *,struct proc *));
static int	ffs_oldfscompat __P((struct fs *));
static int	ffs_mount __P((struct mount *,
	    char *, caddr_t, struct nameidata *, struct proc *));

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

extern u_long nextgennumber;


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
        register struct mount	*mp;	/* mount struct pointer*/
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
	
		/* Get vnode for root device*/
		if( bdevvp( rootdev, &rootvp))
			panic("ffs_mountroot: can't setup bdevvp for root");

		/*
		 * FS specific handling
		 */
		mp->mnt_flag |= MNT_RDONLY;	/* XXX globally applicable?*/

		/*
		 * Attempt mount
		 */
		if( ( err = ffs_mountfs(rootvp, mp, p)) != 0) {
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
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		ump = VFSTOUFS(mp);
		fs = ump->um_fs;
		err = 0;
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
		}
		if (!err && (mp->mnt_flag & MNT_RELOAD))
			err = ffs_reload(mp, ndp->ni_cnd.cn_cred, p);
		if (err) {
			goto error_1;
		}
		if (fs->fs_ronly && (mp->mnt_flag & MNT_WANTRDWR)) {
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

		err = ffs_mountfs(devvp, mp, p);
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
	struct fs *fs;
	int i, blks, size, error;

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
	error = bread(devvp, SBLOCK, SBSIZE, NOCRED, &bp);
	if (error)
		return (error);
	fs = (struct fs *)bp->b_data;
	fs->fs_fmod = 0;
	if (fs->fs_magic != FS_MAGIC || fs->fs_bsize > MAXBSIZE ||
	    fs->fs_bsize < sizeof(struct fs)) {
		brelse(bp);
		return (EIO);		/* XXX needs translation */
	}
	fs = VFSTOUFS(mp)->um_fs;
	bcopy(&fs->fs_csp[0], &((struct fs *)bp->b_data)->fs_csp[0],
	    sizeof(fs->fs_csp));
	bcopy(bp->b_data, fs, (u_int)fs->fs_sbsize);
	if (fs->fs_sbsize < SBSIZE)
		bp->b_flags |= B_INVAL;
	brelse(bp);
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
loop:
	for (vp = mp->mnt_vnodelist.lh_first; vp != NULL; vp = nvp) {
		nvp = vp->v_mntvnodes.le_next;
		/*
		 * Step 4: invalidate all inactive vnodes.
		 */
		if (vp->v_usecount == 0) {
			vgone(vp);
			continue;
		}
		/*
		 * Step 5: invalidate all cached file data.
		 */
		if (vget(vp, 1))
			goto loop;
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
		if (vp->v_mount != mp)
			goto loop;
	}
	return (0);
}

/*
 * Common code for mount and mountroot
 */
int
ffs_mountfs(devvp, mp, p)
	register struct vnode *devvp;
	struct mount *mp;
	struct proc *p;
{
	register struct ufsmount *ump;
	struct buf *bp;
	register struct fs *fs;
	dev_t dev = devvp->v_rdev;
	struct partinfo dpart;
	caddr_t base, space;
	int havepart = 0, blks;
	int error, i, size;
	int ronly;
	u_int strsize;
	int ncount;

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
	error = vinvalbuf(devvp, V_SAVE, p->p_ucred, p, 0, 0);
	if (error)
		return (error);

	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	error = VOP_OPEN(devvp, ronly ? FREAD : FREAD|FWRITE, FSCRED, p);
	if (error)
		return (error);
	if (VOP_IOCTL(devvp, DIOCGPART, (caddr_t)&dpart, FREAD, NOCRED, p) != 0)
		size = DEV_BSIZE;
	else {
		havepart = 1;
		size = dpart.disklab->d_secsize;
	}

	bp = NULL;
	ump = NULL;
	error = bread(devvp, SBLOCK, SBSIZE, NOCRED, &bp);
	if (error)
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
	ump = malloc(sizeof *ump, M_UFSMNT, M_WAITOK);
	bzero((caddr_t)ump, sizeof *ump);
	ump->um_fs = malloc((u_long)fs->fs_sbsize, M_UFSMNT,
	    M_WAITOK);
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
	blks = howmany(fs->fs_cssize, fs->fs_fsize);
	base = space = malloc((u_long)fs->fs_cssize, M_UFSMNT,
	    M_WAITOK);
	for (i = 0; i < blks; i += fs->fs_frag) {
		size = fs->fs_bsize;
		if (i + fs->fs_frag > blks)
			size = (blks - i) * fs->fs_fsize;
		error = bread(devvp, fsbtodb(fs, fs->fs_csaddr + i), size,
			NOCRED, &bp);
		if (error) {
			free(base, M_UFSMNT);
			goto out;
		}
		bcopy(bp->b_data, space, (u_int)size);
		fs->fs_csp[fragstoblks(fs, i)] = (struct csum *)space;
		space += size;
		brelse(bp);
		bp = NULL;
	}
	mp->mnt_data = (qaddr_t)ump;
	mp->mnt_stat.f_fsid.val[0] = (long)dev;
	mp->mnt_stat.f_fsid.val[1] = MOUNT_UFS;
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
	if (ronly == 0)
		ffs_sbupdate(ump, MNT_WAIT);
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
	(void)VOP_CLOSE(devvp, ronly ? FREAD : FREAD|FWRITE, NOCRED, p);
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
		quad_t sizepb = fs->fs_bsize;			/* XXX */
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
	int error, flags, ronly;

	flags = 0;
	if (mntflags & MNT_FORCE) {
		flags |= FORCECLOSE;
	}
	error = ffs_flushfiles(mp, flags, p);
	if (error)
		return (error);
	ump = VFSTOUFS(mp);
	fs = ump->um_fs;
	ronly = fs->fs_ronly;
	if (!ronly) {
		fs->fs_clean = 1;
		ffs_sbupdate(ump, MNT_WAIT);
	}
	ump->um_devvp->v_specflags &= ~SI_MOUNTEDON;

	VOP_LOCK(ump->um_devvp);
	vnode_pager_uncache(ump->um_devvp);
	VOP_UNLOCK(ump->um_devvp);

	error = VOP_CLOSE(ump->um_devvp, ronly ? FREAD : FREAD|FWRITE,
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

	if (!doforce)
		flags &= ~FORCECLOSE;
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
	sbp->f_type = MOUNT_UFS;
	sbp->f_bsize = fs->fs_fsize;
	sbp->f_iosize = fs->fs_bsize;
	sbp->f_blocks = fs->fs_dsize;
	sbp->f_bfree = fs->fs_cstotal.cs_nbfree * fs->fs_frag +
		fs->fs_cstotal.cs_nffree;
	sbp->f_bavail = freespace(fs, fs->fs_minfree);
	sbp->f_files =  fs->fs_ncg * fs->fs_ipg - ROOTINO;
	sbp->f_ffree = fs->fs_cstotal.cs_nifree;
	if (sbp != &mp->mnt_stat) {
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
	register struct vnode *vp, *nvp;
	register struct inode *ip;
	register struct ufsmount *ump = VFSTOUFS(mp);
	register struct fs *fs;
	struct timeval tv;
	int error, allerror = 0;

	fs = ump->um_fs;
	/*
	 * Write back modified superblock.
	 * Consistency check that the superblock
	 * is still in the buffer cache.
	 */
	if (fs->fs_fmod != 0) {
		if (fs->fs_ronly != 0) {		/* XXX */
			printf("fs = %s\n", fs->fs_fsmnt);
			panic("update: rofs mod");
		}
		fs->fs_fmod = 0;
		fs->fs_time = time.tv_sec;
		allerror = ffs_sbupdate(ump, waitfor);
	}
	/*
	 * Write back each (modified) inode.
	 */
loop:
	for (vp = mp->mnt_vnodelist.lh_first; vp != NULL; vp = nvp) {
		/*
		 * If the vnode that we are about to sync is no longer
		 * associated with this mount point, start over.
		 */
		if (vp->v_mount != mp)
			goto loop;
		nvp = vp->v_mntvnodes.le_next;
		if (VOP_ISLOCKED(vp))
			continue;
		ip = VTOI(vp);
		if ((((ip->i_flag &
		    (IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE)) == 0)) &&
		    vp->v_dirtyblkhd.lh_first == NULL)
			continue;
		if (vp->v_type != VCHR) {
			if (vget(vp, 1))
				goto loop;
			error = VOP_FSYNC(vp, cred, waitfor, p);
			if (error)
				allerror = error;
			vput(vp);
		} else {
			tv = time;
			/* VOP_UPDATE(vp, &tv, &tv, waitfor == MNT_WAIT); */
			VOP_UPDATE(vp, &tv, &tv, 0);
		}
	}
	/*
	 * Force stale file system control information to be flushed.
	 */
	error = VOP_FSYNC(ump->um_devvp, cred, waitfor, p);
	if (error)
		allerror = error;
#ifdef QUOTA
	qsync(mp);
#endif
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
	register struct fs *fs;
	register struct inode *ip;
	struct ufsmount *ump;
	struct buf *bp;
	struct vnode *vp;
	dev_t dev;
	int type, error;

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
	type = ump->um_devvp->v_tag == VT_MFS ? M_MFSNODE : M_FFSNODE; /* XXX */
	MALLOC(ip, struct inode *, sizeof(struct inode), type, M_WAITOK);

	/* Allocate a new vnode/inode. */
	error = getnewvnode(VT_UFS, mp, ffs_vnodeop_p, &vp);
	if (error) {
		if (ffs_inode_hash_lock < 0)
			wakeup(&ffs_inode_hash_lock);
		ffs_inode_hash_lock = 0;
		*vpp = NULL;
		FREE(ip, type);
		return (error);
	}
	bzero((caddr_t)ip, sizeof(struct inode));
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
		if (++nextgennumber < (u_long)time.tv_sec)
			nextgennumber = time.tv_sec;
		ip->i_gen = nextgennumber;
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
	struct mbuf *nam;
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
 * Write a superblock and associated information back to disk.
 */
static int
ffs_sbupdate(mp, waitfor)
	struct ufsmount *mp;
	int waitfor;
{
	register struct fs *fs = mp->um_fs;
	register struct buf *bp;
	int blks;
	caddr_t space;
	int i, size, error = 0;

	bp = getblk(mp->um_devvp, SBLOCK, (int)fs->fs_sbsize, 0, 0);
	bcopy((caddr_t)fs, bp->b_data, (u_int)fs->fs_sbsize);
	/* Restore compatibility to old file systems.		   XXX */
	if (fs->fs_postblformat == FS_42POSTBLFMT)		/* XXX */
		((struct fs *)bp->b_data)->fs_nrpos = -1;	/* XXX */
	if (waitfor == MNT_WAIT)
		error = bwrite(bp);
	else
		bawrite(bp);
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
		if (waitfor == MNT_WAIT)
			error = bwrite(bp);
		else
			bawrite(bp);
	}
	return (error);
}
