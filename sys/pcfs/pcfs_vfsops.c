/*
 *  Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 *  You can do anything you want with this software,
 *    just don't say you wrote it,
 *    and don't remove this notice.
 *
 *  This software is provided "as is".
 *
 *  The author supplies this software to be publicly
 *  redistributed on the understanding that the author
 *  is not responsible for the correct functioning of
 *  this software in any circumstances and is not liable
 *  for any damages caused by this software.
 *
 *  October 1992
 *
 *	$Id: pcfs_vfsops.c,v 1.5 1993/12/19 02:07:58 ache Exp $
 */

#include "param.h"
#include "systm.h"
#include "namei.h"
#include "proc.h"
#include "kernel.h"
#include "vnode.h"
#include "specdev.h"	/* defines v_rdev	*/
#include "mount.h"
#include "buf.h"
#include "file.h"
#include "malloc.h"

#include "bpb.h"
#include "bootsect.h"
#include "direntry.h"
#include "denode.h"
#include "pcfsmount.h"
#include "fat.h"

int pcfsdoforce = 0;	/* 1 = force unmount */

/*
 *  mp -
 *  path - addr in user space of mount point (ie /usr or whatever)
 *  data - addr in user space of mount params including the
 *         name of the block special file to treat as a filesystem.
 *  ndp  - 
 *  p    -
 */
int
pcfs_mount(mp, path, data, ndp, p)
	struct mount *mp;
	char *path;
	caddr_t data;
	struct nameidata *ndp;
	struct proc *p;
{
	struct vnode *devvp;	/* vnode for blk device to mount	*/
	struct pcfs_args args;	/* will hold data from mount request	*/
	struct pcfsmount *pmp = 0; /* pcfs specific mount control block	*/
	int error;
	u_int size;

/*
 *  Copy in the args for the mount request.
 */
	if (error = copyin(data, (caddr_t)&args, sizeof(struct pcfs_args)))
		return error;

/*
 *  Check to see if they want it to be an exportable
 *  filesystem via nfs.  And, if they do, should it
 *  be read only, and what uid is root to be mapped
 *  to.
 */
	if ((args.exflags & MNT_EXPORTED)  ||  (mp->mnt_flag & MNT_EXPORTED)) {
		if (args.exflags & MNT_EXPORTED)
			mp->mnt_flag |= MNT_EXPORTED;
		else
			mp->mnt_flag &= ~MNT_EXPORTED;
		if (args.exflags & MNT_EXRDONLY)
			mp->mnt_flag |= MNT_EXRDONLY;
		else
			mp->mnt_flag &= ~MNT_EXRDONLY;
		mp->mnt_exroot = args.exroot;
	}

/*
 *  If they just want to update then be sure we can
 *  do what is asked.  Can't change a filesystem from
 *  read/write to read only.  Why?
 *  And if they've supplied a new device file name then we
 *  continue, otherwise return.
 */
	if (mp->mnt_flag & MNT_UPDATE) {
		pmp = (struct pcfsmount *)mp->mnt_data;
		if (pmp->pm_ronly  &&  (mp->mnt_flag & MNT_RDONLY) == 0)
			pmp->pm_ronly = 0;
		if (args.fspec == 0)
			return 0;
	}

/*
 *  Now, lookup the name of the block device this
 *  mount or name update request is to apply to.
 */
	ndp->ni_nameiop = LOOKUP | FOLLOW;
	ndp->ni_segflg  = UIO_USERSPACE;
	ndp->ni_dirp    = args.fspec;
	if (error = namei(ndp, p))
		return error;

/*
 *  Be sure they've given us a block device to treat
 *  as a filesystem.  And, that its major number is
 *  within the bdevsw table.
 */
	devvp = ndp->ni_vp;
	if (devvp->v_type != VBLK) {
		vrele(devvp);		/* namei() acquires this?	*/
		return ENOTBLK;
	}
	if (major(devvp->v_rdev) >= nblkdev) {
		vrele(devvp);
		return ENXIO;
	}

/*
 *  If this is an update, then make sure the vnode
 *  for the block special device is the same as the
 *  one our filesystem is in.
 */
	if (mp->mnt_flag & MNT_UPDATE) {
		if (devvp != pmp->pm_devvp)
			error = EINVAL;
		else
			vrele(devvp);
	} else {

/*
 *  Well, it's not an update, it's a real mount request.
 *  Time to get dirty.
 */
		error = mountpcfs(devvp, mp, p);
	}
	if (error) {
		vrele(devvp);
		return error;
	}

/*
 *  Copy in the name of the directory the filesystem
 *  is to be mounted on.
 *  Then copy in the name of the block special file
 *  representing the filesystem being mounted.
 *  And we clear the remainder of the character strings
 *  to be tidy.
 *  Then, we try to fill in the filesystem stats structure
 *  as best we can with whatever applies from a dos file
 *  system.
 */
	pmp = (struct pcfsmount *)mp->mnt_data;
	copyinstr(path, (caddr_t)mp->mnt_stat.f_mntonname,
		sizeof(mp->mnt_stat.f_mntonname)-1, &size);
	bzero(mp->mnt_stat.f_mntonname + size,
		sizeof(mp->mnt_stat.f_mntonname) - size);
	copyinstr(args.fspec, mp->mnt_stat.f_mntfromname, MNAMELEN-1, &size);
	bzero(mp->mnt_stat.f_mntfromname + size,
		MNAMELEN - size);
	(void)pcfs_statfs(mp, &mp->mnt_stat, p);
#if defined(PCFSDEBUG)
printf("pcfs_mount(): mp %x, pmp %x, inusemap %x\n", mp, pmp, pmp->pm_inusemap);
#endif /* defined(PCFSDEBUG) */
	return 0;
}

int
mountpcfs(devvp, mp, p)
	struct vnode *devvp;
	struct mount *mp;
	struct proc *p;
{
	int i;
	int bpc;
	int bit;
	int error = 0;
	int needclose;
	int ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	dev_t dev = devvp->v_rdev;
	union bootsector *bsp;
	struct pcfsmount *pmp = NULL;
	struct buf *bp0 = NULL;
	struct byte_bpb33 *b33;
	struct byte_bpb50 *b50;

/*
 *  Multiple mounts of the same block special file
 *  aren't allowed.  Make sure no one else has the
 *  special file open.  And flush any old buffers
 *  from this filesystem.  Presumably this prevents
 *  us from running into buffers that are the wrong
 *  blocksize.
 *  NOTE: mountedon() is a part of the ufs filesystem.
 *  If the ufs filesystem is not gen'ed into the system
 *  we will get an unresolved reference.
 */
	if (error = mountedon(devvp)) {
		return error;
	}
	if (vcount(devvp) > 1)
		return EBUSY;
	vinvalbuf(devvp, 1);

/*
 *  Now open the block special file.
 */
	if (error = VOP_OPEN(devvp, ronly ? FREAD : FREAD|FWRITE, NOCRED, p))
		return error;
	needclose = 1;
#if defined(HDSUPPORT)
/*
 *  Put this in when we support reading dos filesystems
 *  from partitioned harddisks.
 */
	if (VOP_IOCTL(devvp, DIOCGPART, &pcfspart, FREAD, NOCRED, p) == 0) {
	}
#endif /* defined(HDSUPPORT) */

/*
 *  Read the boot sector of the filesystem, and then
 *  check the boot signature.  If not a dos boot sector
 *  then error out.  We could also add some checking on
 *  the bsOemName field.  So far I've seen the following
 *  values:
 *    "IBM  3.3"
 *    "MSDOS3.3"
 *    "MSDOS5.0"
 */
	if (error = bread(devvp, 0, 512, NOCRED, &bp0))
		goto error_exit;
	bp0->b_flags |= B_AGE;
	bsp = (union bootsector *)bp0->b_un.b_addr;
	b33 = (struct byte_bpb33 *)bsp->bs33.bsBPB;
	b50 = (struct byte_bpb50 *)bsp->bs50.bsBPB;
	if (bsp->bs50.bsBootSectSig != BOOTSIG) {
		error = EINVAL;
		goto error_exit;
	}

	pmp = malloc(sizeof *pmp, M_PCFSMNT, M_WAITOK);
	pmp->pm_inusemap = NULL;
	pmp->pm_mountp = mp;

/*
 *  Compute several useful quantities from the bpb in
 *  the bootsector.  Copy in the dos 5 variant of the
 *  bpb then fix up the fields that are different between
 *  dos 5 and dos 3.3.
 */
	pmp->pm_BytesPerSec  = getushort(b50->bpbBytesPerSec);
	pmp->pm_SectPerClust = b50->bpbSecPerClust;
	pmp->pm_ResSectors   = getushort(b50->bpbResSectors);
	pmp->pm_FATs         = b50->bpbFATs;
	pmp->pm_RootDirEnts  = getushort(b50->bpbRootDirEnts);
	pmp->pm_Sectors      = getushort(b50->bpbSectors);
	pmp->pm_Media        = b50->bpbMedia;
	pmp->pm_FATsecs      = getushort(b50->bpbFATsecs);
	pmp->pm_SecPerTrack  = getushort(b50->bpbSecPerTrack);
	pmp->pm_Heads        = getushort(b50->bpbHeads);
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
				pmp->pm_BytesPerSec; /* in sectors */
	pmp->pm_firstcluster = pmp->pm_rootdirblk + pmp->pm_rootdirsize;
	pmp->pm_nmbrofclusters = (pmp->pm_HugeSectors - pmp->pm_firstcluster) /
		pmp->pm_SectPerClust;
	pmp->pm_maxcluster = pmp->pm_nmbrofclusters + 1;
	pmp->pm_fatsize = pmp->pm_FATsecs * pmp->pm_BytesPerSec;
	if (FAT12(pmp))
		/* This will usually be a floppy disk.
		 * This size makes sure that one fat entry will not be split
		 * across multiple blocks. */
		pmp->pm_fatblocksize = 3 * pmp->pm_BytesPerSec;
	else
		/* This will usually be a hard disk.
		 * Reading or writing one block should be quite fast. */
		pmp->pm_fatblocksize = MAXBSIZE;
	pmp->pm_fatblocksec = pmp->pm_fatblocksize / pmp->pm_BytesPerSec;
		
#if defined(PCFSDEBUG)
	if ((pmp->pm_rootdirsize % pmp->pm_SectPerClust) != 0)
		printf("mountpcfs(): root directory is not a multiple of the clustersize in length\n");
#endif
/*
 *  Compute mask and shift value for isolating cluster relative
 *  byte offsets and cluster numbers from a file offset.
 */
	bpc = pmp->pm_SectPerClust * pmp->pm_BytesPerSec;
	pmp->pm_bpcluster = bpc;
	pmp->pm_depclust  = bpc/sizeof(struct direntry);
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
	pmp->pm_bnshift = 9;		/* shift right 9 bits to get bn */

/*
 *  Release the bootsector buffer.
 */
	brelse(bp0);
	bp0 = NULL;

/*
 *  Allocate memory for the bitmap of allocated clusters,
 *  and then fill it in.
 */
	pmp->pm_inusemap = malloc((pmp->pm_maxcluster >> 3) + 1,
		M_PCFSFAT, M_WAITOK);

/*
 *  fillinusemap() needs pm_devvp.
 */
	pmp->pm_dev = dev;
	pmp->pm_devvp = devvp;

/*
 *  Have the inuse map filled in.
 */
	error = fillinusemap(pmp);
	if (error)
		goto error_exit;

/*
 *  If they want fat updates to be synchronous then let
 *  them suffer the performance degradation in exchange
 *  for the on disk copy of the fat being correct just
 *  about all the time.  I suppose this would be a good
 *  thing to turn on if the kernel is still flakey.
 */
	pmp->pm_waitonfat = mp->mnt_flag & MNT_SYNCHRONOUS;

/*
 *  Finish up.
 */
	pmp->pm_ronly = ronly;
	if (ronly == 0)
		pmp->pm_fmod = 1;
	mp->mnt_data = (qaddr_t)pmp;
	mp->mnt_stat.f_fsid.val[0] = (long)dev;
	mp->mnt_stat.f_fsid.val[1] = MOUNT_MSDOS;
	mp->mnt_flag |= MNT_LOCAL;
#if defined(QUOTA)
/*
 *  If we ever do quotas for DOS filesystems this would
 *  be a place to fill in the info in the pcfsmount
 *  structure.
 *  You dolt, quotas on dos filesystems make no sense
 *  because files have no owners on dos filesystems.
 *  of course there is some empty space in the directory
 *  entry where we could put uid's and gid's.
 */
#endif /* defined(QUOTA) */
	devvp->v_specflags |= SI_MOUNTEDON;

	return 0;

error_exit:;
	if (bp0)
		brelse(bp0);
	if (needclose)
		(void)VOP_CLOSE(devvp, ronly ? FREAD : FREAD|FWRITE,
			NOCRED, p);
	if (pmp) {
		if (pmp->pm_inusemap)
			free((caddr_t)pmp->pm_inusemap, M_PCFSFAT);
		free((caddr_t)pmp, M_PCFSMNT);
		mp->mnt_data = (qaddr_t)0;
	}
	return error;
}

int
pcfs_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{
	return 0;
}

/*
 *  Unmount the filesystem described by mp.
 */
int
pcfs_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	int flags = 0;
	int error;
	struct pcfsmount *pmp = (struct pcfsmount *)mp->mnt_data;
	struct vnode *vp = pmp->pm_devvp;

	if (mntflags & MNT_FORCE) {
		if (!pcfsdoforce)
			return EINVAL;
		flags |= FORCECLOSE;
	}
	mntflushbuf(mp, 0);
	if (mntinvalbuf(mp))
		return EBUSY;
#if defined(QUOTA)
#endif /* defined(QUOTA) */
	if (error = vflush(mp, NULLVP, flags))
		return error;
	pmp->pm_devvp->v_specflags &= ~SI_MOUNTEDON;
#if defined(PCFSDEBUG)
printf("pcfs_umount(): just before calling VOP_CLOSE()\n");
printf("flag %08x, usecount %d, writecount %d, holdcnt %d\n",
	vp->v_flag, vp->v_usecount, vp->v_writecount, vp->v_holdcnt);
printf("lastr %d, id %d, mount %08x, op %08x\n",
	vp->v_lastr, vp->v_id, vp->v_mount, vp->v_op);
printf("freef %08x, freeb %08x, mountf %08x, mountb %08x\n",
	vp->v_freef, vp->v_freeb, vp->v_mountf, vp->v_mountb);
printf("cleanblkhd %08x, dirtyblkhd %08x, numoutput %d, type %d\n",
	vp->v_cleanblkhd, vp->v_dirtyblkhd, vp->v_numoutput, vp->v_type);
printf("union %08x, tag %d, data[0] %08x, data[1] %08x\n",
	vp->v_socket, vp->v_tag, vp->v_data[0], vp->v_data[1]);
#endif /* defined(PCFSDEBUG) */
	error = VOP_CLOSE(pmp->pm_devvp, pmp->pm_ronly ? FREAD : FREAD|FWRITE,
		NOCRED, p);
	vrele(pmp->pm_devvp);
	free((caddr_t)pmp->pm_inusemap, M_PCFSFAT);
	free((caddr_t)pmp, M_PCFSMNT);
	mp->mnt_data = (qaddr_t)0;
	mp->mnt_flag &= ~MNT_LOCAL;
	return error;
}

int
pcfs_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	struct denode *ndep;
	struct pcfsmount *pmp = (struct pcfsmount *)(mp->mnt_data);
	int error;

	error = deget(pmp, PCFSROOT, PCFSROOT_OFS, NULL, &ndep);
#if defined(PCFSDEBUG)
printf("pcfs_root(); mp %08x, pmp %08x, ndep %08x, vp %08x\n",
 mp, pmp, ndep, DETOV(ndep));
#endif /* defined(PCFSDEBUG) */
	if (error == 0)
		*vpp = DETOV(ndep);
	return error;
}

int
pcfs_quotactl(mp, cmds, uid, arg, p)
	struct mount *mp;
	int cmds;
	uid_t uid;
	caddr_t arg;
	struct proc *p;
{
	return EOPNOTSUPP;
}

int
pcfs_statfs(mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{
	struct pcfsmount *pmp = (struct pcfsmount *)mp->mnt_data;

/*
 *  Fill in the stat block.
 */
	sbp->f_type   = MOUNT_MSDOS;
	sbp->f_fsize  = pmp->pm_bpcluster;
	sbp->f_bsize  = pmp->pm_bpcluster;
	sbp->f_blocks = pmp->pm_nmbrofclusters;
	sbp->f_bfree  = pmp->pm_freeclustercount;
	sbp->f_bavail = pmp->pm_freeclustercount;
	sbp->f_files  = pmp->pm_RootDirEnts;
	sbp->f_ffree  = 0;		/* what to put in here? */

/*
 *  Copy the mounted on and mounted from names into
 *  the passed in stat block, if it is not the one
 *  in the mount structure.
 */
	if (sbp != &mp->mnt_stat) {
		bcopy((caddr_t)mp->mnt_stat.f_mntonname,
			(caddr_t)&sbp->f_mntonname[0], MNAMELEN);
		bcopy((caddr_t)mp->mnt_stat.f_mntfromname,
			(caddr_t)&sbp->f_mntfromname[0], MNAMELEN);
	}
	return 0;
}

int
pcfs_sync(mp, waitfor)
	struct mount *mp;
	int waitfor;
{
	struct vnode *vp;
	struct denode *dep;
	struct pcfsmount *pmp;
	int error;
	int allerror = 0;

	pmp = (struct pcfsmount *)mp->mnt_data;

/*
 *  If we ever switch to not updating all of the fats
 *  all the time, this would be the place to update them
 *  from the first one.
 */
	if (pmp->pm_fmod) {
		if (pmp->pm_ronly) {
			printf("pcfs_sync(): writing to readonly filesystem\n");
			return EINVAL;
		} else {
			/* update fats here */
		}
	}

/*
 *  Go thru in memory denodes and write them out along
 *  with unwritten file blocks.
 */
loop:
	for (vp = mp->mnt_mounth; vp; vp = vp->v_mountf) {
		if (vp->v_mount != mp)	/* not ours anymore	*/
			goto loop;
		if (VOP_ISLOCKED(vp))	/* file is busy		*/
			continue;
		dep = VTODE(vp);
		if ((dep->de_flag & DEUPD) == 0  &&  vp->v_dirtyblkhd == NULL)
			continue;
		if (vget(vp))		/* not there anymore?	*/
			goto loop;
		if (vp->v_dirtyblkhd)	/* flush dirty file blocks */
			vflushbuf(vp, 0);
		if ((dep->de_flag & DEUPD)  &&
		    (error = deupdat(dep, &time, 0)))
			allerror = error;
		vput(vp);		/* done with this one	*/
	}

/*
 *  Flush filesystem control info.
 */
	vflushbuf(pmp->pm_devvp, waitfor == MNT_WAIT ? B_SYNC : 0);
	return allerror;
}

int
pcfs_fhtovp (mp, fhp, vpp)
	struct mount *mp;
	struct fid *fhp;
	struct vnode **vpp;
{
	struct pcfsmount *pmp = (struct pcfsmount *)mp->mnt_data;
	struct defid *defhp = (struct defid *)fhp;
	struct denode *dep;
	int error;

	error = deget (pmp, defhp->defid_dirclust, defhp->defid_dirofs,
			NULL, &dep);
	if (error)
		return (error);
	*vpp = DETOV (dep);
	return (0);
}


int
pcfs_vptofh (vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{
	struct denode *dep = VTODE(vp);
	struct defid *defhp = (struct defid *)fhp;

	defhp->defid_len = sizeof(struct defid);
	defhp->defid_dirclust = dep->de_dirclust;
	defhp->defid_dirofs = dep->de_diroffset;
/*	defhp->defid_gen = ip->i_gen; */
	return (0);
}

struct vfsops pcfs_vfsops = {
	pcfs_mount,
	pcfs_start,
	pcfs_unmount,
	pcfs_root,
	pcfs_quotactl,
	pcfs_statfs,
	pcfs_sync,
	pcfs_fhtovp,
	pcfs_vptofh,
	pcfs_init
};
