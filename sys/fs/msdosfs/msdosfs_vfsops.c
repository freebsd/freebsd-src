/*	$Id: msdosfs_vfsops.c,v 1.9 1995/11/07 14:10:19 phk Exp $ */
/*	$NetBSD: msdosfs_vfsops.c,v 1.19 1994/08/21 18:44:10 ws Exp $	*/

/*-
 * Copyright (C) 1994 Wolfgang Solfrank.
 * Copyright (C) 1994 TooLs GmbH.
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
#include <sys/file.h>
#include <sys/malloc.h>

#include <msdosfs/bpb.h>
#include <msdosfs/bootsect.h>
#include <msdosfs/direntry.h>
#include <msdosfs/denode.h>
#include <msdosfs/msdosfsmount.h>
#include <msdosfs/fat.h>

static int msdosfsdoforce = 1;		/* 1 = force unmount */

static int	mountmsdosfs __P((struct vnode *devvp, struct mount *mp,
				  struct proc *p));
static int	msdosfs_fhtovp __P((struct mount *, struct fid *,
				    struct mbuf *, struct vnode **, int *,
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
	struct msdosfsmount *pmp; /* msdosfs specific mount control block */
	int error, flags;
	u_int size;
	struct ucred *cred, *scred;
	struct vattr va;

	/*
	 * Copy in the args for the mount request.
	 */
	error = copyin(data, (caddr_t) & args, sizeof(struct msdosfs_args));
	if (error)
		return error;

	/*
	 * If they just want to update then be sure we can do what is
	 * asked.  Can't change a filesystem from read/write to read only.
	 * Why? And if they've supplied a new device file name then we
	 * continue, otherwise return.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		pmp = (struct msdosfsmount *) mp->mnt_data;
		error = 0;
		if (pmp->pm_ronly == 0 && (mp->mnt_flag & MNT_RDONLY)) {
			flags = WRITECLOSE;
			if (mp->mnt_flag & MNT_FORCE)
				flags |= FORCECLOSE;
			if (vfs_busy(mp))
				return EBUSY;
			error = vflush(mp, NULLVP, flags);
			vfs_unbusy(mp);
		}
		if (!error && (mp->mnt_flag & MNT_RELOAD))
			/* not yet implemented */
			error = EINVAL;
		if (error)
			return error;
		if (pmp->pm_ronly && (mp->mnt_flag & MNT_RDONLY) == 0)
			pmp->pm_ronly = 0;
		if (args.fspec == 0) {
			/*
			 * Process export requests.
			 */
			return vfs_export(mp, &pmp->pm_export, &args.export);
		}
	} else
		pmp = NULL;

	/*
	 * check to see that the user in owns the target directory.
	 * Note the very XXX trick to make sure we're checking as the
	 * real user -- were mount() executable by anyone, this wouldn't
	 * be a problem.
	 *
	 * XXX there should be one consistent error out.
	 */
	cred = crdup(p->p_ucred);			/* XXX */
	cred->cr_uid = p->p_cred->p_ruid;		/* XXX */
	error = VOP_GETATTR(mp->mnt_vnodecovered, &va, cred, p);
	if (error) {
		crfree(cred);				/* XXX */
		return error;
	}
	if (cred->cr_uid != 0) {
		if (va.va_uid != cred->cr_uid) {
			error = EACCES;
			crfree(cred);			/* XXX */
			return error;
		}

		/* a user mounted it; we'll verify permissions when unmounting */
		mp->mnt_flag |= MNT_USER;
	}

	/*
	 * Now, lookup the name of the block device this mount or name
	 * update request is to apply to.
	 */
	NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, args.fspec, p);
	scred = p->p_ucred;				/* XXX */
	p->p_ucred = cred;				/* XXX */
	error = namei(ndp);
	p->p_ucred = scred;				/* XXX */
	crfree(cred);					/* XXX */
	if (error != 0)
		return error;

	/*
	 * Be sure they've given us a block device to treat as a
	 * filesystem.  And, that its major number is within the bdevsw
	 * table.
	 */
	devvp = ndp->ni_vp;
	if (devvp->v_type != VBLK) {
		vrele(devvp);
		return ENOTBLK;
	}
	if (major(devvp->v_rdev) >= nblkdev) {
		vrele(devvp);
		return ENXIO;
	}

	/*
	 * If this is an update, then make sure the vnode for the block
	 * special device is the same as the one our filesystem is in.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		if (devvp != pmp->pm_devvp)
			error = EINVAL;
		else
			vrele(devvp);
	} else {

		/*
		 * Well, it's not an update, it's a real mount request.
		 * Time to get dirty.
		 */
		error = mountmsdosfs(devvp, mp, p);
	}
	if (error) {
		vrele(devvp);
		return error;
	}

	/*
	 * Copy in the name of the directory the filesystem is to be
	 * mounted on. Then copy in the name of the block special file
	 * representing the filesystem being mounted. And we clear the
	 * remainder of the character strings to be tidy. Set up the
	 * user id/group id/mask as specified by the user. Then, we try to
	 * fill in the filesystem stats structure as best we can with
	 * whatever applies from a dos file system.
	 */
	pmp = (struct msdosfsmount *) mp->mnt_data;
	copyinstr(path, (caddr_t) mp->mnt_stat.f_mntonname,
	    sizeof(mp->mnt_stat.f_mntonname) - 1, &size);
	bzero(mp->mnt_stat.f_mntonname + size,
	    sizeof(mp->mnt_stat.f_mntonname) - size);
	copyinstr(args.fspec, mp->mnt_stat.f_mntfromname, MNAMELEN - 1, &size);
	bzero(mp->mnt_stat.f_mntfromname + size,
	    MNAMELEN - size);
	pmp->pm_mounter = p->p_cred->p_ruid;
	pmp->pm_gid = args.gid;
	pmp->pm_uid = args.uid;
	pmp->pm_mask = args.mask;
	(void) msdosfs_statfs(mp, &mp->mnt_stat, p);
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_mount(): mp %p, pmp %p, inusemap %p\n", mp, pmp, pmp->pm_inusemap);
#endif
	return 0;
}

static int
mountmsdosfs(devvp, mp, p)
	struct vnode *devvp;
	struct mount *mp;
	struct proc *p;
{
	int i;
	int bpc;
	int bit;
	int error;
	int needclose;
	int ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	dev_t dev = devvp->v_rdev;
	union bootsector *bsp;
	struct msdosfsmount *pmp = NULL;
	struct buf *bp0 = NULL;
	struct byte_bpb33 *b33;
	struct byte_bpb50 *b50;

	/*
	 * Multiple mounts of the same block special file aren't allowed.
	 * Make sure no one else has the special file open.  And flush any
	 * old buffers from this filesystem.  Presumably this prevents us
	 * from running into buffers that are the wrong blocksize.
	 */
	error = vfs_mountedon(devvp);
	if (error)
		return error;
	if (vcount(devvp) > 1)
		return EBUSY;
	error = vinvalbuf(devvp, V_SAVE, p->p_ucred, p, 0, 0);
	if (error)
		return error;

	/*
	 * Now open the block special file.
	 */
	error = VOP_OPEN(devvp, ronly ? FREAD : FREAD | FWRITE, FSCRED, p);
	if (error)
		return error;
	needclose = 1;
#ifdef HDSUPPORT
	/*
	 * Put this in when we support reading dos filesystems from
	 * partitioned harddisks.
	 */
	if (VOP_IOCTL(devvp, DIOCGPART, &msdosfspart, FREAD, NOCRED, p) == 0) {
	}
#endif

	/*
	 * Read the boot sector of the filesystem, and then check the boot
	 * signature.  If not a dos boot sector then error out.  We could
	 * also add some checking on the bsOemName field.  So far I've seen
	 * the following values: "IBM  3.3" "MSDOS3.3" "MSDOS5.0"
	 */
	error = bread(devvp, 0, 512, NOCRED, &bp0);
	if (error)
		goto error_exit;
	bp0->b_flags |= B_AGE;
	bsp = (union bootsector *) bp0->b_data;
	b33 = (struct byte_bpb33 *) bsp->bs33.bsBPB;
	b50 = (struct byte_bpb50 *) bsp->bs50.bsBPB;
#ifdef MSDOSFS_CHECKSIG
	if (bsp->bs50.bsBootSectSig != BOOTSIG) {
		error = EINVAL;
		goto error_exit;
	}
#endif
	if ( bsp->bs50.bsJump[0] != 0xe9 &&
	    (bsp->bs50.bsJump[0] != 0xeb || bsp->bs50.bsJump[2] != 0x90)) {
		error = EINVAL;
		goto error_exit;
	}

	pmp = malloc(sizeof *pmp, M_MSDOSFSMNT, M_WAITOK);
	bzero((caddr_t)pmp, sizeof *pmp);
	pmp->pm_mountp = mp;

	/*
	 * Compute several useful quantities from the bpb in the
	 * bootsector.  Copy in the dos 5 variant of the bpb then fix up
	 * the fields that are different between dos 5 and dos 3.3.
	 */
	pmp->pm_BytesPerSec = getushort(b50->bpbBytesPerSec);
	pmp->pm_SectPerClust = b50->bpbSecPerClust;
	pmp->pm_ResSectors = getushort(b50->bpbResSectors);
	pmp->pm_FATs = b50->bpbFATs;
	pmp->pm_RootDirEnts = getushort(b50->bpbRootDirEnts);
	pmp->pm_Sectors = getushort(b50->bpbSectors);
	pmp->pm_Media = b50->bpbMedia;
	pmp->pm_FATsecs = getushort(b50->bpbFATsecs);
	pmp->pm_SecPerTrack = getushort(b50->bpbSecPerTrack);
	pmp->pm_Heads = getushort(b50->bpbHeads);

	/* XXX - We should probably check more values here */
    	if (!pmp->pm_BytesPerSec || !pmp->pm_SectPerClust ||
	    !pmp->pm_Heads || pmp->pm_Heads > 255 ||
	    !pmp->pm_SecPerTrack || pmp->pm_SecPerTrack > 63) {
		error = EINVAL;
		goto error_exit;
	}

	if (pmp->pm_Sectors == 0) {
		pmp->pm_HiddenSects = getulong(b50->bpbHiddenSecs);
		pmp->pm_HugeSectors = getulong(b50->bpbHugeSectors);
	} else {
		pmp->pm_HiddenSects = getushort(b33->bpbHiddenSecs);
		pmp->pm_HugeSectors = pmp->pm_Sectors;
	}
	pmp->pm_fatblk = pmp->pm_ResSectors;
	pmp->pm_rootdirblk = pmp->pm_fatblk +
	    (pmp->pm_FATs * pmp->pm_FATsecs);
	pmp->pm_rootdirsize = (pmp->pm_RootDirEnts * sizeof(struct direntry))
	    /
	    pmp->pm_BytesPerSec;/* in sectors */
	pmp->pm_firstcluster = pmp->pm_rootdirblk + pmp->pm_rootdirsize;
	pmp->pm_nmbrofclusters = (pmp->pm_HugeSectors - pmp->pm_firstcluster) /
	    pmp->pm_SectPerClust;
	pmp->pm_maxcluster = pmp->pm_nmbrofclusters + 1;
	pmp->pm_fatsize = pmp->pm_FATsecs * pmp->pm_BytesPerSec;
	if (FAT12(pmp))
		/*
		 * This will usually be a floppy disk. This size makes sure
		 * that one fat entry will not be split across multiple
		 * blocks.
		 */
		pmp->pm_fatblocksize = 3 * pmp->pm_BytesPerSec;
	else
		/*
		 * This will usually be a hard disk. Reading or writing one
		 * block should be quite fast.
		 */
		pmp->pm_fatblocksize = MAXBSIZE;
	pmp->pm_fatblocksec = pmp->pm_fatblocksize / pmp->pm_BytesPerSec;


	if ((pmp->pm_rootdirsize % pmp->pm_SectPerClust) != 0)
		printf("mountmsdosfs(): root directory is not a multiple of the clustersize in length\n");

	/*
	 * Compute mask and shift value for isolating cluster relative byte
	 * offsets and cluster numbers from a file offset.
	 */
	bpc = pmp->pm_SectPerClust * pmp->pm_BytesPerSec;
	pmp->pm_bpcluster = bpc;
	pmp->pm_depclust = bpc / sizeof(struct direntry);
	pmp->pm_crbomask = bpc - 1;
	if (bpc == 0) {
		error = EINVAL;
		goto error_exit;
	}
	bit = 1;
	for (i = 0; i < 32; i++) {
		if (bit & bpc) {
			if (bit ^ bpc) {
				error = EINVAL;
				goto error_exit;
			}
			pmp->pm_cnshift = i;
			break;
		}
		bit <<= 1;
	}

	pmp->pm_brbomask = 0x01ff;	/* 512 byte blocks only (so far) */
	pmp->pm_bnshift = 9;	/* shift right 9 bits to get bn */

	/*
	 * Release the bootsector buffer.
	 */
	brelse(bp0);
	bp0 = NULL;

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
	error = fillinusemap(pmp);
	if (error)
		goto error_exit;

	/*
	 * If they want fat updates to be synchronous then let them suffer
	 * the performance degradation in exchange for the on disk copy of
	 * the fat being correct just about all the time.  I suppose this
	 * would be a good thing to turn on if the kernel is still flakey.
	 */
	pmp->pm_waitonfat = mp->mnt_flag & MNT_SYNCHRONOUS;

	/*
	 * Finish up.
	 */
	pmp->pm_ronly = ronly;
	if (ronly == 0)
		pmp->pm_fmod = 1;
	mp->mnt_data = (qaddr_t) pmp;
        mp->mnt_stat.f_fsid.val[0] = (long)dev;
        mp->mnt_stat.f_fsid.val[1] = MOUNT_MSDOS;
	mp->mnt_flag |= MNT_LOCAL;
#ifdef QUOTA
	/*
	 * If we ever do quotas for DOS filesystems this would be a place
	 * to fill in the info in the msdosfsmount structure. You dolt,
	 * quotas on dos filesystems make no sense because files have no
	 * owners on dos filesystems. of course there is some empty space
	 * in the directory entry where we could put uid's and gid's.
	 */
#endif
	devvp->v_specflags |= SI_MOUNTEDON;

	return 0;

error_exit:;
	if (bp0)
		brelse(bp0);
	if (needclose)
		(void) VOP_CLOSE(devvp, ronly ? FREAD : FREAD | FWRITE,
		    NOCRED, p);
	if (pmp) {
		if (pmp->pm_inusemap)
			free((caddr_t) pmp->pm_inusemap, M_MSDOSFSFAT);
		free((caddr_t) pmp, M_MSDOSFSMNT);
		mp->mnt_data = (qaddr_t) 0;
	}
	return error;
}

static int
msdosfs_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{
	return 0;
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
	int flags = 0;
	int error;
	struct msdosfsmount *pmp = (struct msdosfsmount *) mp->mnt_data;

	/* only the mounter, or superuser can unmount */
	if ((p->p_cred->p_ruid != pmp->pm_mounter) &&
	    (error = suser(p->p_ucred, &p->p_acflag)))
		return error;

	if (mntflags & MNT_FORCE) {
		if (!msdosfsdoforce)
			return EINVAL;
		flags |= FORCECLOSE;
	}
#ifdef QUOTA
#endif
	error = vflush(mp, NULLVP, flags);
	if (error)
		return error;
	pmp->pm_devvp->v_specflags &= ~SI_MOUNTEDON;
	error = VOP_CLOSE(pmp->pm_devvp, pmp->pm_ronly ? FREAD : FREAD | FWRITE,
	    NOCRED, p);
	vrele(pmp->pm_devvp);
	free((caddr_t) pmp->pm_inusemap, M_MSDOSFSFAT);
	free((caddr_t) pmp, M_MSDOSFSMNT);
	mp->mnt_data = (qaddr_t) 0;
	mp->mnt_flag &= ~MNT_LOCAL;
	return error;
}

static int
msdosfs_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	struct denode *ndep;
	struct msdosfsmount *pmp = (struct msdosfsmount *) (mp->mnt_data);
	int error;

	error = deget(pmp, MSDOSFSROOT, MSDOSFSROOT_OFS, NULL, &ndep);
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_root(); mp %p, pmp %p, ndep %p, vp %p\n",
	    mp, pmp, ndep, DETOV(ndep));
#endif
	if (error == 0)
		*vpp = DETOV(ndep);
	return error;
}

static int
msdosfs_quotactl(mp, cmds, uid, arg, p)
	struct mount *mp;
	int cmds;
	uid_t uid;
	caddr_t arg;
	struct proc *p;
{
#ifdef QUOTA
	return EOPNOTSUPP;
#else
	return EOPNOTSUPP;
#endif
}

static int
msdosfs_statfs(mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{
	struct msdosfsmount *pmp = (struct msdosfsmount *) mp->mnt_data;

	/*
	 * Fill in the stat block.
	 */
	sbp->f_type = MOUNT_MSDOS;
	sbp->f_bsize = pmp->pm_bpcluster;
	sbp->f_iosize = pmp->pm_bpcluster;
	sbp->f_blocks = pmp->pm_nmbrofclusters;
	sbp->f_bfree = pmp->pm_freeclustercount;
	sbp->f_bavail = pmp->pm_freeclustercount;
	sbp->f_files = pmp->pm_RootDirEnts;			/* XXX */
	sbp->f_ffree = 0;	/* what to put in here? */

	/*
	 * Copy the mounted on and mounted from names into the passed in
	 * stat block, if it is not the one in the mount structure.
	 */
	if (sbp != &mp->mnt_stat) {
		bcopy((caddr_t) mp->mnt_stat.f_mntonname,
		    (caddr_t) & sbp->f_mntonname[0], MNAMELEN);
		bcopy((caddr_t) mp->mnt_stat.f_mntfromname,
		    (caddr_t) & sbp->f_mntfromname[0], MNAMELEN);
	}
#if 0
	strncpy(&sbp->f_fstypename[0], mp->mnt_op->vfs_name, MFSNAMELEN);
	sbp->f_fstypename[MFSNAMELEN] = '\0';
#endif
	return 0;
}

static int
msdosfs_sync(mp, waitfor, cred, p)
	struct mount *mp;
	int waitfor;
	struct ucred *cred;
	struct proc *p;
{
	struct vnode *vp;
	struct denode *dep;
	struct msdosfsmount *pmp;
	int error;
	int allerror = 0;

	pmp = (struct msdosfsmount *) mp->mnt_data;

	/*
	 * If we ever switch to not updating all of the fats all the time,
	 * this would be the place to update them from the first one.
	 */
	if (pmp->pm_fmod)
		if (pmp->pm_ronly)
			panic("msdosfs_sync: rofs mod");
		else {
			/* update fats here */
		}

	/*
	 * Go thru in memory denodes and write them out along with
	 * unwritten file blocks.
	 */
loop:
	for (vp = mp->mnt_vnodelist.lh_first; vp;
	    vp = vp->v_mntvnodes.le_next) {
		if (vp->v_mount != mp)	/* not ours anymore	 */
			goto loop;
		if (VOP_ISLOCKED(vp))	/* file is busy		 */
			continue;
		dep = VTODE(vp);
		if ((dep->de_flag & (DE_MODIFIED | DE_UPDATE)) == 0 &&
		    vp->v_dirtyblkhd.lh_first == NULL)
			continue;
		if (vget(vp, 1))	/* not there anymore?	 */
			goto loop;
		error = VOP_FSYNC(vp, cred, waitfor, p);
		if (error)
			allerror = error;
		vput(vp);	/* done with this one	 */
	}

	/*
	 * Flush filesystem control info.
	 */
	error = VOP_FSYNC(pmp->pm_devvp, cred, waitfor, p);
	if (error)
		allerror = error;
	return allerror;
}

static int
msdosfs_fhtovp(mp, fhp, nam, vpp, exflagsp, credanonp)
	struct mount *mp;
	struct fid *fhp;
	struct mbuf *nam;
	struct vnode **vpp;
	int *exflagsp;
	struct ucred **credanonp;
{
	struct msdosfsmount *pmp = (struct msdosfsmount *) mp->mnt_data;
	struct defid *defhp = (struct defid *) fhp;
	struct denode *dep;
	struct netcred *np;
	int error;

	np = vfs_export_lookup(mp, &pmp->pm_export, nam);
	if (np == NULL)
		return EACCES;
	error = deget(pmp, defhp->defid_dirclust, defhp->defid_dirofs,
	    NULL, &dep);
	if (error) {
		*vpp = NULLVP;
		return error;
	}
	*vpp = DETOV(dep);
	*exflagsp = np->netc_exflags;
	*credanonp = &np->netc_anon;
	return 0;
}


static int
msdosfs_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{
	struct denode *dep = VTODE(vp);
	struct defid *defhp = (struct defid *) fhp;

	defhp->defid_len = sizeof(struct defid);
	defhp->defid_dirclust = dep->de_dirclust;
	defhp->defid_dirofs = dep->de_diroffset;
	/* defhp->defid_gen = ip->i_gen; */
	return 0;
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
	msdosfs_fhtovp,
	msdosfs_vptofh,
	msdosfs_init
};

VFS_SET(msdosfs_vfsops, msdos, MOUNT_MSDOS, 0);
