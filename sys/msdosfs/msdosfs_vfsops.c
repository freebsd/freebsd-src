/*	$Id: msdosfs_vfsops.c,v 1.30 1998/03/08 09:57:48 julian Exp $ */
/*	$NetBSD: msdosfs_vfsops.c,v 1.51 1997/11/17 15:36:58 ws Exp $	*/

/*-
 * Copyright (C) 1994, 1995, 1997 Wolfgang Solfrank.
 * Copyright (C) 1994, 1995, 1997 TooLs GmbH.
 * All rights reserved.
 * Original code by Paul Popelka (paulp@uts.amdahl.com) (see below).
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 * You can do anything you want with this software, just don't say you wrote
 * it, and don't remove this notice.
 *
 * This software is provided "as is".
 *
 * The author supplies this software to be publicly redistributed on the
 * understanding that the author is not responsible for the correct
 * functioning of this software in any circumstances and is not liable for
 * any damages caused by this software.
 *
 * October 1992
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <miscfs/specfs/specdev.h> /* XXX */	/* defines v_rdev */
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/stat.h> 				/* defines ALLPERMS */

#include <msdosfs/bpb.h>
#include <msdosfs/bootsect.h>
#include <msdosfs/direntry.h>
#include <msdosfs/denode.h>
#include <msdosfs/msdosfsmount.h>
#include <msdosfs/fat.h>

MALLOC_DEFINE(M_MSDOSFSMNT, "MSDOSFS mount", "MSDOSFS mount structure");
static MALLOC_DEFINE(M_MSDOSFSFAT, "MSDOSFS FAT", "MSDOSFS file allocation table");

static int	update_mp __P((struct mount *mp, struct msdosfs_args *argp));
static int	mountmsdosfs __P((struct vnode *devvp, struct mount *mp,
				  struct proc *p, struct msdosfs_args *argp));
static int	msdosfs_fhtovp __P((struct mount *, struct fid *,
				    struct sockaddr *, struct vnode **, int *,
				    struct ucred **));
static int	msdosfs_mount __P((struct mount *, char *, caddr_t,
				   struct nameidata *, struct proc *));
static int	msdosfs_quotactl __P((struct mount *, int, uid_t, caddr_t,
				      struct proc *));
static int	msdosfs_root __P((struct mount *, struct vnode **));
static int	msdosfs_start __P((struct mount *, int, struct proc *));
static int	msdosfs_statfs __P((struct mount *, struct statfs *,
				    struct proc *));
static int	msdosfs_sync __P((struct mount *, int, struct ucred *,
				  struct proc *));
static int	msdosfs_unmount __P((struct mount *, int, struct proc *));
static int	msdosfs_vget __P((struct mount *mp, ino_t ino,
				  struct vnode **vpp));
static int	msdosfs_vptofh __P((struct vnode *, struct fid *));

static int
update_mp(mp, argp)
	struct mount *mp;
	struct msdosfs_args *argp;
{
	struct msdosfsmount *pmp = VFSTOMSDOSFS(mp);
	int error;

	pmp->pm_gid = argp->gid;
	pmp->pm_uid = argp->uid;
	pmp->pm_mask = argp->mask & ALLPERMS;
	pmp->pm_flags |= argp->flags & MSDOSFSMNT_MNTOPT;
	if (pmp->pm_flags & MSDOSFSMNT_U2WTABLE) {
		bcopy(argp->u2w, pmp->pm_u2w, sizeof(pmp->pm_u2w));
		bcopy(argp->d2u, pmp->pm_d2u, sizeof(pmp->pm_d2u));
		bcopy(argp->u2d, pmp->pm_u2d, sizeof(pmp->pm_u2d));
	}
	if (pmp->pm_flags & MSDOSFSMNT_ULTABLE) {
		bcopy(argp->ul, pmp->pm_ul, sizeof(pmp->pm_ul));
		bcopy(argp->lu, pmp->pm_lu, sizeof(pmp->pm_lu));
	}

#ifndef __FreeBSD__
	/*
	 * GEMDOS knows nothing (yet) about win95
	 */
	if (pmp->pm_flags & MSDOSFSMNT_GEMDOSFS)
		pmp->pm_flags |= MSDOSFSMNT_NOWIN95;
#endif

	if (pmp->pm_flags & MSDOSFSMNT_NOWIN95)
		pmp->pm_flags |= MSDOSFSMNT_SHORTNAME;
	else if (!(pmp->pm_flags &
	    (MSDOSFSMNT_SHORTNAME | MSDOSFSMNT_LONGNAME))) {
		struct vnode *rootvp;

		/*
		 * Try to divine whether to support Win'95 long filenames
		 */
		if (FAT32(pmp))
			pmp->pm_flags |= MSDOSFSMNT_LONGNAME;
		else {
			if ((error = msdosfs_root(mp, &rootvp)) != 0)
				return error;
			pmp->pm_flags |= findwin95(VTODE(rootvp))
				? MSDOSFSMNT_LONGNAME
					: MSDOSFSMNT_SHORTNAME;
			vput(rootvp);
		}
	}
	return 0;
}

#ifndef __FreeBSD__
int
msdosfs_mountroot()
{
	register struct mount *mp;
	struct proc *p = curproc;	/* XXX */
	size_t size;
	int error;
	struct msdosfs_args args;

	if (root_device->dv_class != DV_DISK)
		return (ENODEV);

	/*
	 * Get vnodes for swapdev and rootdev.
	 */
	if (bdevvp(rootdev, &rootvp))
		panic("msdosfs_mountroot: can't setup rootvp");

	mp = malloc((u_long)sizeof(struct mount), M_MOUNT, M_WAITOK);
	bzero((char *)mp, (u_long)sizeof(struct mount));
	mp->mnt_op = &msdosfs_vfsops;
	mp->mnt_flag = 0;
	LIST_INIT(&mp->mnt_vnodelist);

	args.flags = 0;
	args.uid = 0;
	args.gid = 0;
	args.mask = 0777;

	if ((error = mountmsdosfs(rootvp, mp, p, &args)) != 0) {
		free(mp, M_MOUNT);
		return (error);
	}

	if ((error = update_mp(mp, &args)) != 0) {
		(void)msdosfs_unmount(mp, 0, p);
		free(mp, M_MOUNT);
		return (error);
	}

	if ((error = vfs_lock(mp)) != 0) {
		(void)msdosfs_unmount(mp, 0, p);
		free(mp, M_MOUNT);
		return (error);
	}

	CIRCLEQ_INSERT_TAIL(&mountlist, mp, mnt_list);
	mp->mnt_vnodecovered = NULLVP;
	(void) copystr("/", mp->mnt_stat.f_mntonname, MNAMELEN - 1,
	    &size);
	bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);
	(void) copystr(ROOTNAME, mp->mnt_stat.f_mntfromname, MNAMELEN - 1,
	    &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
	(void)msdosfs_statfs(mp, &mp->mnt_stat, p);
	vfs_unlock(mp);
	return (0);
}
#endif

/*
 * mp - path - addr in user space of mount point (ie /usr or whatever)
 * data - addr in user space of mount params including the name of the block
 * special file to treat as a filesystem.
 */
static int
msdosfs_mount(mp, path, data, ndp, p)
	struct mount *mp;
	char *path;
	caddr_t data;
	struct nameidata *ndp;
	struct proc *p;
{
	struct vnode *devvp;	  /* vnode for blk device to mount */
	struct msdosfs_args args; /* will hold data from mount request */
	/* msdosfs specific mount control block */
	struct msdosfsmount *pmp = NULL;
	size_t size;
	int error, flags;
	mode_t accessmode;

	error = copyin(data, (caddr_t)&args, sizeof(struct msdosfs_args));
	if (error)
		return (error);
	if (args.magic != MSDOSFS_ARGSMAGIC) {
		printf("Old mount_msdosfs, flags=%d\n", args.flags);
		args.flags = 0;
	}
	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		pmp = VFSTOMSDOSFS(mp);
		error = 0;
		if (!(pmp->pm_flags & MSDOSFSMNT_RONLY) && (mp->mnt_flag & MNT_RDONLY)) {
			flags = WRITECLOSE;
			if (mp->mnt_flag & MNT_FORCE)
				flags |= FORCECLOSE;
			error = vflush(mp, NULLVP, flags);
		}
		if (!error && (mp->mnt_flag & MNT_RELOAD))
			/* not yet implemented */
			error = EOPNOTSUPP;
		if (error)
			return (error);
		if ((pmp->pm_flags & MSDOSFSMNT_RONLY) && (mp->mnt_kern_flag & MNTK_WANTRDWR)) {
			/*
			 * If upgrade to read-write by non-root, then verify
			 * that user has necessary permissions on the device.
			 */
			if (p->p_ucred->cr_uid != 0) {
				devvp = pmp->pm_devvp;
				vn_lock(devvp, LK_EXCLUSIVE, p);
				error = VOP_ACCESS(devvp, VREAD | VWRITE,
						   p->p_ucred, p);
				if (error) {
					VOP_UNLOCK(devvp, 0, p);
					return (error);
				}
				VOP_UNLOCK(devvp, 0, p);
			}
			pmp->pm_flags &= ~MSDOSFSMNT_RONLY;
		}
		if (args.fspec == 0) {
#ifdef	__notyet__		/* doesn't work correctly with current mountd	XXX */
			if (args.flags & MSDOSFSMNT_MNTOPT) {
				pmp->pm_flags &= ~MSDOSFSMNT_MNTOPT;
				pmp->pm_flags |= args.flags & MSDOSFSMNT_MNTOPT;
				if (pmp->pm_flags & MSDOSFSMNT_NOWIN95)
					pmp->pm_flags |= MSDOSFSMNT_SHORTNAME;
			}
#endif
			/*
			 * Process export requests.
			 */
			return (vfs_export(mp, &pmp->pm_export, &args.export));
		}
	}
	/*
	 * Not an update, or updating the name: look up the name
	 * and verify that it refers to a sensible block device.
	 */
	NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, args.fspec, p);
	error = namei(ndp);
	if (error)
		return (error);
	devvp = ndp->ni_vp;

	if (devvp->v_type != VBLK) {
		vrele(devvp);
		return (ENOTBLK);
	}
	if (major(devvp->v_rdev) >= nblkdev) {
		vrele(devvp);
		return (ENXIO);
	}
	/*
	 * If mount by non-root, then verify that user has necessary
	 * permissions on the device.
	 */
	if (p->p_ucred->cr_uid != 0) {
		accessmode = VREAD;
		if ((mp->mnt_flag & MNT_RDONLY) == 0)
			accessmode |= VWRITE;
		vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, p);
		error = VOP_ACCESS(devvp, accessmode, p->p_ucred, p);
		if (error) {
			vput(devvp);
			return (error);
		}
		VOP_UNLOCK(devvp, 0, p);
	}
	if ((mp->mnt_flag & MNT_UPDATE) == 0) {
		error = mountmsdosfs(devvp, mp, p, &args);
#ifdef MSDOSFS_DEBUG		/* only needed for the printf below */
		pmp = VFSTOMSDOSFS(mp);
#endif
	} else {
		if (devvp != pmp->pm_devvp)
			error = EINVAL;	/* XXX needs translation */
		else
			vrele(devvp);
	}
	if (error) {
		vrele(devvp);
		return (error);
	}

	error = update_mp(mp, &args);
	if (error) {
		msdosfs_unmount(mp, MNT_FORCE, p);
		return error;
	}

	(void) copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size);
	bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);
	(void) copyinstr(args.fspec, mp->mnt_stat.f_mntfromname, MNAMELEN - 1,
	    &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
	(void) msdosfs_statfs(mp, &mp->mnt_stat, p);
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_mount(): mp %p, pmp %p, inusemap %p\n", mp, pmp, pmp->pm_inusemap);
#endif
	return (0);
}

static int
mountmsdosfs(devvp, mp, p, argp)
	struct vnode *devvp;
	struct mount *mp;
	struct proc *p;
	struct msdosfs_args *argp;
{
	struct msdosfsmount *pmp;
	struct buf *bp;
	dev_t dev = devvp->v_rdev;
#ifndef __FreeBSD__
	struct partinfo dpart;
#endif
	union bootsector *bsp;
	struct byte_bpb33 *b33;
	struct byte_bpb50 *b50;
	struct byte_bpb710 *b710;
	u_int8_t SecPerClust;
	int	ronly, error;
	int	bsize = 0, dtype = 0, tmp;

	/*
	 * Disallow multiple mounts of the same device.
	 * Disallow mounting of a device that is currently in use
	 * (except for root, which might share swap device for miniroot).
	 * Flush out any old buffers remaining from a previous use.
	 */
	error = vfs_mountedon(devvp);
	if (error)
		return (error);
	if (vcount(devvp) > 1 && devvp != rootvp)
		return (EBUSY);
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, p);
	error = vinvalbuf(devvp, V_SAVE, p->p_ucred, p, 0, 0);
	VOP_UNLOCK(devvp, 0, p);
	if (error)
		return (error);

	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	error = VOP_OPEN(devvp, ronly ? FREAD : FREAD|FWRITE, FSCRED, p);
	if (error)
		return (error);

	bp  = NULL; /* both used in error_exit */
	pmp = NULL;

#ifndef __FreeBSD__
	if (argp->flags & MSDOSFSMNT_GEMDOSFS) {
		/*
	 	 * We need the disklabel to calculate the size of a FAT entry
		 * later on. Also make sure the partition contains a filesystem
		 * of type FS_MSDOS. This doesn't work for floppies, so we have
		 * to check for them too.
	 	 *
	 	 * At least some parts of the msdos fs driver seem to assume
		 * that the size of a disk block will always be 512 bytes.
		 * Let's check it...
		 */
		error = VOP_IOCTL(devvp, DIOCGPART, (caddr_t)&dpart,
				  FREAD, NOCRED, p);
		if (error)
			goto error_exit;
		tmp   = dpart.part->p_fstype;
		dtype = dpart.disklab->d_type;
		bsize = dpart.disklab->d_secsize;
		if (bsize != 512 || (dtype!=DTYPE_FLOPPY && tmp!=FS_MSDOS)) {
			error = EINVAL;
			goto error_exit;
		}
	}
#endif

	/*
	 * Read the boot sector of the filesystem, and then check the
	 * boot signature.  If not a dos boot sector then error out.
	 */
#ifdef	PC98
	error = bread(devvp, 0, 1024, NOCRED, &bp);
#else
	error = bread(devvp, 0, 512, NOCRED, &bp);
#endif
	if (error)
		goto error_exit;
	bp->b_flags |= B_AGE;
	bsp = (union bootsector *)bp->b_data;
	b33 = (struct byte_bpb33 *)bsp->bs33.bsBPB;
	b50 = (struct byte_bpb50 *)bsp->bs50.bsBPB;
	b710 = (struct byte_bpb710 *)bsp->bs710.bsPBP;

#ifndef __FreeBSD__
	if (!(argp->flags & MSDOSFSMNT_GEMDOSFS)) {
#endif
#ifdef PC98
		if ((bsp->bs50.bsBootSectSig0 != BOOTSIG0
		    || bsp->bs50.bsBootSectSig1 != BOOTSIG1)
		    && (bsp->bs50.bsBootSectSig0 != 0       /* PC98 DOS 3.3x */
		    || bsp->bs50.bsBootSectSig1 != 0)
		    && (bsp->bs50.bsBootSectSig0 != 0x90    /* PC98 DOS 5.0  */
		    || bsp->bs50.bsBootSectSig1 != 0x3d)
		    && (bsp->bs50.bsBootSectSig0 != 0x46    /* PC98 DOS 3.3B */
		    || bsp->bs50.bsBootSectSig1 != 0xfa)) {
#else
		if (bsp->bs50.bsBootSectSig0 != BOOTSIG0
		    || bsp->bs50.bsBootSectSig1 != BOOTSIG1) {
#endif
			error = EINVAL;
			goto error_exit;
		}
#ifndef __FreeBSD__
	}
#endif

	pmp = malloc(sizeof *pmp, M_MSDOSFSMNT, M_WAITOK);
	bzero((caddr_t)pmp, sizeof *pmp);
	pmp->pm_mountp = mp;

	/*
	 * Compute several useful quantities from the bpb in the
	 * bootsector.  Copy in the dos 5 variant of the bpb then fix up
	 * the fields that are different between dos 5 and dos 3.3.
	 */
	SecPerClust = b50->bpbSecPerClust;
	pmp->pm_BytesPerSec = getushort(b50->bpbBytesPerSec);
	pmp->pm_ResSectors = getushort(b50->bpbResSectors);
	pmp->pm_FATs = b50->bpbFATs;
	pmp->pm_RootDirEnts = getushort(b50->bpbRootDirEnts);
	pmp->pm_Sectors = getushort(b50->bpbSectors);
	pmp->pm_FATsecs = getushort(b50->bpbFATsecs);
	pmp->pm_SecPerTrack = getushort(b50->bpbSecPerTrack);
	pmp->pm_Heads = getushort(b50->bpbHeads);
	pmp->pm_Media = b50->bpbMedia;

#ifndef __FreeBSD__
	if (!(argp->flags & MSDOSFSMNT_GEMDOSFS)) {
#endif
		/* XXX - We should probably check more values here */
		if (!pmp->pm_BytesPerSec || !SecPerClust
			|| !pmp->pm_Heads || pmp->pm_Heads > 255
#ifdef PC98
	    		|| !pmp->pm_SecPerTrack || pmp->pm_SecPerTrack > 255) {
#else
			|| !pmp->pm_SecPerTrack || pmp->pm_SecPerTrack > 63) {
#endif
			error = EINVAL;
			goto error_exit;
		}
#ifndef __FreeBSD__
	}
#endif

	if (pmp->pm_Sectors == 0) {
		pmp->pm_HiddenSects = getulong(b50->bpbHiddenSecs);
		pmp->pm_HugeSectors = getulong(b50->bpbHugeSectors);
	} else {
		pmp->pm_HiddenSects = getushort(b33->bpbHiddenSecs);
		pmp->pm_HugeSectors = pmp->pm_Sectors;
	}
	if (pmp->pm_HugeSectors > 0xffffffff / pmp->pm_BytesPerSec + 1) {
		/*
		 * We cannot deal currently with this size of disk
		 * due to fileid limitations (see msdosfs_getattr and
		 * msdosfs_readdir)
		 */
		error = EINVAL;
		goto error_exit;
	}

	if (pmp->pm_RootDirEnts == 0) {
		if (bsp->bs710.bsBootSectSig2 != BOOTSIG2
		    || bsp->bs710.bsBootSectSig3 != BOOTSIG3
		    || pmp->pm_Sectors
		    || pmp->pm_FATsecs
		    || getushort(b710->bpbFSVers)) {
			error = EINVAL;
			goto error_exit;
		}
		pmp->pm_fatmask = FAT32_MASK;
		pmp->pm_fatmult = 4;
		pmp->pm_fatdiv = 1;
		pmp->pm_FATsecs = getulong(b710->bpbBigFATsecs);
		if (getushort(b710->bpbExtFlags) & FATMIRROR)
			pmp->pm_curfat = getushort(b710->bpbExtFlags) & FATNUM;
		else
			pmp->pm_flags |= MSDOSFS_FATMIRROR;
	} else
		pmp->pm_flags |= MSDOSFS_FATMIRROR;

#ifndef __FreeBSD__
	if (argp->flags & MSDOSFSMNT_GEMDOSFS) {
		if (FAT32(pmp)) {
			/*
			 * GEMDOS doesn't know fat32.
			 */
			error = EINVAL;
			goto error_exit;
		}

		/*
		 * Check a few values (could do some more):
		 * - logical sector size: power of 2, >= block size
		 * - sectors per cluster: power of 2, >= 1
		 * - number of sectors:   >= 1, <= size of partition
		 */
		if ( (SecPerClust == 0)
		  || (SecPerClust & (SecPerClust - 1))
		  || (pmp->pm_BytesPerSec < bsize)
		  || (pmp->pm_BytesPerSec & (pmp->pm_BytesPerSec - 1))
		  || (pmp->pm_HugeSectors == 0)
		  || (pmp->pm_HugeSectors * (pmp->pm_BytesPerSec / bsize)
							> dpart.part->p_size)
		   ) {
			error = EINVAL;
			goto error_exit;
		}
		/*
		 * XXX - Many parts of the msdos fs driver seem to assume that
		 * the number of bytes per logical sector (BytesPerSec) will
		 * always be the same as the number of bytes per disk block
		 * Let's pretend it is.
		 */
		tmp = pmp->pm_BytesPerSec / bsize;
		pmp->pm_BytesPerSec  = bsize;
		pmp->pm_HugeSectors *= tmp;
		pmp->pm_HiddenSects *= tmp;
		pmp->pm_ResSectors  *= tmp;
		pmp->pm_Sectors     *= tmp;
		pmp->pm_FATsecs     *= tmp;
		SecPerClust         *= tmp;
	}
#endif
	pmp->pm_fatblk = pmp->pm_ResSectors;
	if (FAT32(pmp)) {
		pmp->pm_rootdirblk = getulong(b710->bpbRootClust);
		pmp->pm_firstcluster = pmp->pm_fatblk
			+ (pmp->pm_FATs * pmp->pm_FATsecs);
		pmp->pm_fsinfo = getushort(b710->bpbFSInfo);
	} else {
		pmp->pm_rootdirblk = pmp->pm_fatblk +
			(pmp->pm_FATs * pmp->pm_FATsecs);
		pmp->pm_rootdirsize = (pmp->pm_RootDirEnts * sizeof(struct direntry)
				       + pmp->pm_BytesPerSec - 1)
			/ pmp->pm_BytesPerSec;/* in sectors */
		pmp->pm_firstcluster = pmp->pm_rootdirblk + pmp->pm_rootdirsize;
	}

	pmp->pm_nmbrofclusters = (pmp->pm_HugeSectors - pmp->pm_firstcluster) /
	    SecPerClust;
	pmp->pm_maxcluster = pmp->pm_nmbrofclusters + 1;
	pmp->pm_fatsize = pmp->pm_FATsecs * pmp->pm_BytesPerSec;

#ifndef __FreeBSD__
	if (argp->flags & MSDOSFSMNT_GEMDOSFS) {
		if ((pmp->pm_nmbrofclusters <= (0xff0 - 2))
		      && ((dtype == DTYPE_FLOPPY) || ((dtype == DTYPE_VNODE)
		      && ((pmp->pm_Heads == 1) || (pmp->pm_Heads == 2))))
		    ) {
			pmp->pm_fatmask = FAT12_MASK;
			pmp->pm_fatmult = 3;
			pmp->pm_fatdiv = 2;
		} else {
			pmp->pm_fatmask = FAT16_MASK;
			pmp->pm_fatmult = 2;
			pmp->pm_fatdiv = 1;
		}
	} else 
#endif
	if (pmp->pm_fatmask == 0) {
		if (pmp->pm_maxcluster
		    <= ((CLUST_RSRVD - CLUST_FIRST) & FAT12_MASK)) {
			/*
			 * This will usually be a floppy disk. This size makes
			 * sure that one fat entry will not be split across
			 * multiple blocks.
			 */
			pmp->pm_fatmask = FAT12_MASK;
			pmp->pm_fatmult = 3;
			pmp->pm_fatdiv = 2;
		} else {
			pmp->pm_fatmask = FAT16_MASK;
			pmp->pm_fatmult = 2;
			pmp->pm_fatdiv = 1;
		}
	}
	if (FAT12(pmp))
		pmp->pm_fatblocksize = 3 * pmp->pm_BytesPerSec;
	else
		pmp->pm_fatblocksize = MAXBSIZE;

	pmp->pm_fatblocksec = pmp->pm_fatblocksize / pmp->pm_BytesPerSec;
	pmp->pm_bnshift = ffs(pmp->pm_BytesPerSec) - 1;

	/*
	 * Compute mask and shift value for isolating cluster relative byte
	 * offsets and cluster numbers from a file offset.
	 */
	pmp->pm_bpcluster = SecPerClust * pmp->pm_BytesPerSec;
	pmp->pm_crbomask = pmp->pm_bpcluster - 1;
	pmp->pm_cnshift = ffs(pmp->pm_bpcluster) - 1;

	/*
	 * Check for valid cluster size
	 * must be a power of 2
	 */
	if (pmp->pm_bpcluster ^ (1 << pmp->pm_cnshift)) {
		error = EINVAL;
		goto error_exit;
	}

	/*
	 * Release the bootsector buffer.
	 */
	brelse(bp);
	bp = NULL;

	/*
	 * Check FSInfo.
	 */
	if (pmp->pm_fsinfo) {
		struct fsinfo *fp;

		if ((error = bread(devvp, pmp->pm_fsinfo, 1024, NOCRED, &bp)) != 0)
			goto error_exit;
		fp = (struct fsinfo *)bp->b_data;
		if (!bcmp(fp->fsisig1, "RRaA", 4)
		    && !bcmp(fp->fsisig2, "rrAa", 4)
		    && !bcmp(fp->fsisig3, "\0\0\125\252", 4)
		    && !bcmp(fp->fsisig4, "\0\0\125\252", 4))
			pmp->pm_nxtfree = getulong(fp->fsinxtfree);
		else
			pmp->pm_fsinfo = 0;
		brelse(bp);
		bp = NULL;
	}

	/*
	 * Check and validate (or perhaps invalidate?) the fsinfo structure?		XXX
	 */

	/*
	 * Allocate memory for the bitmap of allocated clusters, and then
	 * fill it in.
	 */
	pmp->pm_inusemap = malloc(((pmp->pm_maxcluster + N_INUSEBITS - 1)
				   / N_INUSEBITS)
				  * sizeof(*pmp->pm_inusemap),
				  M_MSDOSFSFAT, M_WAITOK);

	/*
	 * fillinusemap() needs pm_devvp.
	 */
	pmp->pm_dev = dev;
	pmp->pm_devvp = devvp;

	/*
	 * Have the inuse map filled in.
	 */
	if ((error = fillinusemap(pmp)) != 0)
		goto error_exit;

	/*
	 * If they want fat updates to be synchronous then let them suffer
	 * the performance degradation in exchange for the on disk copy of
	 * the fat being correct just about all the time.  I suppose this
	 * would be a good thing to turn on if the kernel is still flakey.
	 */
	if (mp->mnt_flag & MNT_SYNCHRONOUS)
		pmp->pm_flags |= MSDOSFSMNT_WAITONFAT;

	/*
	 * Finish up.
	 */
	if (ronly)
		pmp->pm_flags |= MSDOSFSMNT_RONLY;
	else
		pmp->pm_fmod = 1;
	mp->mnt_data = (qaddr_t) pmp;
	mp->mnt_stat.f_fsid.val[0] = (long)dev;
	mp->mnt_stat.f_fsid.val[1] = mp->mnt_vfc->vfc_typenum;
	mp->mnt_flag |= MNT_LOCAL;
	devvp->v_specmountpoint = mp;

	return 0;

error_exit:
	if (bp)
		brelse(bp);
	(void) VOP_CLOSE(devvp, ronly ? FREAD : FREAD | FWRITE, NOCRED, p);
	if (pmp) {
		if (pmp->pm_inusemap)
			free(pmp->pm_inusemap, M_MSDOSFSFAT);
		free(pmp, M_MSDOSFSMNT);
		mp->mnt_data = (qaddr_t)0;
	}
	return (error);
}

static int
msdosfs_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{

	return (0);
}

/*
 * Unmount the filesystem described by mp.
 */
static int
msdosfs_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	struct msdosfsmount *pmp;
	int error, flags;

	flags = 0;
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;
	error = vflush(mp, NULLVP, flags);
	if (error)
		return error;
	pmp = VFSTOMSDOSFS(mp);
	pmp->pm_devvp->v_specmountpoint = NULL;
#ifdef MSDOSFS_DEBUG
	{
		struct vnode *vp = pmp->pm_devvp;

		printf("msdosfs_umount(): just before calling VOP_CLOSE()\n");
		printf("flag %08lx, usecount %d, writecount %d, holdcnt %ld\n",
		    vp->v_flag, vp->v_usecount, vp->v_writecount, vp->v_holdcnt);
		printf("lastr %d, id %lu, mount %p, op %p\n",
		    vp->v_lastr, vp->v_id, vp->v_mount, vp->v_op);
		printf("freef %p, freeb %p, mount %p\n",
		    vp->v_freelist.tqe_next, vp->v_freelist.tqe_prev,
		    vp->v_mount);
		printf("cleanblkhd %p, dirtyblkhd %p, numoutput %ld, type %d\n",
		    vp->v_cleanblkhd.lh_first,
		    vp->v_dirtyblkhd.lh_first,
		    vp->v_numoutput, vp->v_type);
		printf("union %p, tag %d, data[0] %08x, data[1] %08x\n",
		    vp->v_socket, vp->v_tag,
		    ((u_int *)vp->v_data)[0],
		    ((u_int *)vp->v_data)[1]);
	}
#endif
	error = VOP_CLOSE(pmp->pm_devvp,
		    (pmp->pm_flags&MSDOSFSMNT_RONLY) ? FREAD : FREAD | FWRITE,
		    NOCRED, p);
	vrele(pmp->pm_devvp);
	free(pmp->pm_inusemap, M_MSDOSFSFAT);
	free(pmp, M_MSDOSFSMNT);
	mp->mnt_data = (qaddr_t)0;
	mp->mnt_flag &= ~MNT_LOCAL;
	return (error);
}

static int
msdosfs_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	struct msdosfsmount *pmp = VFSTOMSDOSFS(mp);
	struct denode *ndep;
	int error;

#ifdef MSDOSFS_DEBUG
	printf("msdosfs_root(); mp %p, pmp %p\n", mp, pmp);
#endif
	error = deget(pmp, MSDOSFSROOT, MSDOSFSROOT_OFS, &ndep);
	if (error)
		return (error);
	*vpp = DETOV(ndep);
	return (0);
}

static int
msdosfs_quotactl(mp, cmds, uid, arg, p)
	struct mount *mp;
	int cmds;
	uid_t uid;
	caddr_t arg;
	struct proc *p;
{
	return EOPNOTSUPP;
}

static int
msdosfs_statfs(mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{
	struct msdosfsmount *pmp;

	pmp = VFSTOMSDOSFS(mp);
	sbp->f_bsize = pmp->pm_bpcluster;
	sbp->f_iosize = pmp->pm_bpcluster;
	sbp->f_blocks = pmp->pm_nmbrofclusters;
	sbp->f_bfree = pmp->pm_freeclustercount;
	sbp->f_bavail = pmp->pm_freeclustercount;
	sbp->f_files = pmp->pm_RootDirEnts;			/* XXX */
	sbp->f_ffree = 0;	/* what to put in here? */
	if (sbp != &mp->mnt_stat) {
		sbp->f_type = mp->mnt_vfc->vfc_typenum;
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}
	strncpy(sbp->f_fstypename, mp->mnt_vfc->vfc_name, MFSNAMELEN);
	return (0);
}

static int
msdosfs_sync(mp, waitfor, cred, p)
	struct mount *mp;
	int waitfor;
	struct ucred *cred;
	struct proc *p;
{
	struct vnode *vp, *nvp;
	struct denode *dep;
	struct msdosfsmount *pmp = VFSTOMSDOSFS(mp);
	int error, allerror = 0;

	/*
	 * If we ever switch to not updating all of the fats all the time,
	 * this would be the place to update them from the first one.
	 */
	if (pmp->pm_fmod != 0)
		if (pmp->pm_flags & MSDOSFSMNT_RONLY)
			panic("msdosfs_sync: rofs mod");
		else {
			/* update fats here */
		}
	/*
	 * Write back each (modified) denode.
	 */
	simple_lock(&mntvnode_slock);
loop:
	for (vp = mp->mnt_vnodelist.lh_first;
	     vp != NULL;
	     vp = nvp) {
		/*
		 * If the vnode that we are about to sync is no longer
		 * assoicated with this mount point, start over.
		 */
		if (vp->v_mount != mp)
			goto loop;

		simple_lock(&vp->v_interlock);
		nvp = vp->v_mntvnodes.le_next;
		dep = VTODE(vp);
		if (vp->v_type == VNON
		|| (waitfor == MNT_LAZY) /* can this happen with msdosfs? */
		|| (((dep->de_flag &
		     (DE_ACCESS | DE_CREATE | DE_UPDATE | DE_MODIFIED)) == 0)
		  && (vp->v_dirtyblkhd.lh_first == NULL))) {
			simple_unlock(&vp->v_interlock);
			continue;
		}
		simple_unlock(&mntvnode_slock);
		error = vget(vp, LK_EXCLUSIVE | LK_NOWAIT | LK_INTERLOCK, p);
		if (error) {
			simple_lock(&mntvnode_slock);
			if (error == ENOENT)
				goto loop;
			continue;
		}
		error = VOP_FSYNC(vp, cred, waitfor, p);
		if (error)
			allerror = error;
		VOP_UNLOCK(vp, 0, p);
		vrele(vp);	/* done with this one	 */
		simple_lock(&mntvnode_slock);
	}
	simple_unlock(&mntvnode_slock);

	/*
	 * Flush filesystem control info.
	 */
	error = VOP_FSYNC(pmp->pm_devvp, cred, waitfor, p);
	if (error)
		allerror = error;
	return (allerror);
}

static int
msdosfs_fhtovp(mp, fhp, nam, vpp, exflagsp, credanonp)
	struct mount *mp;
	struct fid *fhp;
	struct sockaddr *nam;
	struct vnode **vpp;
	int *exflagsp;
	struct ucred **credanonp;
{
	struct msdosfsmount *pmp = VFSTOMSDOSFS(mp);
	struct defid *defhp = (struct defid *) fhp;
	struct denode *dep;
	struct netcred *np;
	int error;

	np = vfs_export_lookup(mp, &pmp->pm_export, nam);
	if (np == NULL)
		return (EACCES);
	error = deget(pmp, defhp->defid_dirclust, defhp->defid_dirofs, &dep);
	if (error) {
		*vpp = NULLVP;
		return (error);
	}
	*vpp = DETOV(dep);
	*exflagsp = np->netc_exflags;
	*credanonp = &np->netc_anon;
	return (0);
}

static int
msdosfs_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{
	struct denode *dep;
	struct defid *defhp;

	dep = VTODE(vp);
	defhp = (struct defid *)fhp;
	defhp->defid_len = sizeof(struct defid);
	defhp->defid_dirclust = dep->de_dirclust;
	defhp->defid_dirofs = dep->de_diroffset;
	/* defhp->defid_gen = dep->de_gen; */
	return (0);
}

static int
msdosfs_vget(mp, ino, vpp)
	struct mount *mp;
	ino_t ino;
	struct vnode **vpp;
{
	return EOPNOTSUPP;
}

static struct vfsops msdosfs_vfsops = {
	msdosfs_mount,
	msdosfs_start,
	msdosfs_unmount,
	msdosfs_root,
	msdosfs_quotactl,
	msdosfs_statfs,
	msdosfs_sync,
	msdosfs_vget,
	vfs_vrele,
	msdosfs_fhtovp,
	msdosfs_vptofh,
	msdosfs_init
};

VFS_SET(msdosfs_vfsops, msdos, MOUNT_MSDOS, 0);
