/*
 *  Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 *  You can do anything you want with this software,
 *    just don't say you wrote it,
 *    and don't reoove this notice.
 *
 *  This software is provided "as is".
 *
 *  The authop supplies this software to be publicly
 *  redistributed on the understanding that the author
 *  is not responsible for the correct functioning of
 *  this software in any circumstances and is not liable
 *  for any damages caused by this software.
 *
 *  October 1992
 *
 *	$Id: pcfs_denode.c,v 1.5 1993/12/19 00:54:30 wollman Exp $
 */

#include "param.h"
#include "systm.h"
#include "mount.h"
#include "proc.h"
#include "buf.h"
#include "vnode.h"
#include "kernel.h"	/* defines "time"			*/

#include "bpb.h"
#include "pcfsmount.h"
#include "direntry.h"
#include "denode.h"
#include "fat.h"

#define	DEHSZ	512
#if ((DEHSZ & (DEHSZ-1)) == 0)
#define	DEHASH(dev, deno)	(((dev)+(deno)+((deno)>>16))&(DEHSZ-1))
#else
#define	DEHASH(dev, deno)	(((dev)+(deno)+((deno)>>16))%DEHSZ)
#endif /* ((DEHSZ & (DEHSZ-1)) == 0) */

union dehead {
	union dehead *deh_head[2];
	struct denode *deh_chain[2];
} dehead[DEHSZ];

void
pcfs_init(void)
{
	int i;
	union dehead *deh;

	if (VN_MAXPRIVATE < sizeof(struct denode))
		panic("pcfs_init: vnode too small");

	for (i = DEHSZ, deh = dehead; --i >= 0; deh++) {
		deh->deh_head[0] = deh;
		deh->deh_head[1] = deh;
	}
}

/*
 *  If deget() succeeds it returns with the gotten denode
 *  locked().
 *  pmp - address of pcfsmount structure of the filesystem
 *    containing the denode of interest.  The pm_dev field
 *    and the address of the pcfsmount structure are used. 
 *  dirclust - which cluster bp contains, if dirclust is 0
 *    (root directory) diroffset is relative to the beginning
 *    of the root directory, otherwise it is cluster relative.
 *  diroffset - offset past begin of cluster of denode we
 *    want
 *  direntptr - address of the direntry structure of interest.
 *    direntptr is NULL, the block is read if necessary.
 *  depp - returns the address of the gotten denode.
 */
int
deget (pmp, dirclust, diroffset, direntptr, depp)
	struct pcfsmount *pmp;	/* so we know the maj/min number	*/
	u_long dirclust;	/* cluster this dir entry came from	*/
	u_long diroffset;	/* index of entry within the cluster	*/
	struct direntry *direntptr;
	struct denode **depp;	/* returns the addr of the gotten denode*/
{
	int error;
	dev_t dev = pmp->pm_dev;
	union dehead *deh;
	struct mount *mntp = pmp->pm_mountp;
	extern struct vnodeops pcfs_vnodeops;
	struct denode *ldep;
	struct vnode *nvp;
	struct buf *bp;
#if defined(PCFSDEBUG)
printf("deget(pmp %08x, dirclust %d, diroffset %x, direntptr %x, depp %08x)\n",
	pmp, dirclust, diroffset, direntptr, depp);
#endif /* defined(PCFSDEBUG) */

	/* If dir entry is given and refers to a directory, convert to 
	 * canonical form
	 */
	if (direntptr && (direntptr->deAttributes & ATTR_DIRECTORY)) {
		dirclust = direntptr->deStartCluster;
		if (dirclust == PCFSROOT)
			diroffset = PCFSROOT_OFS;
		else
			diroffset = 0;
	}

/*
 *  See if the denode is in the denode cache. Use the location of
 *  the directory entry to compute the hash value.
 *  For subdir use address of "." entry.
 *  for root dir use cluster PCFSROOT, offset PCFSROOT_OFS
 *  
 *  NOTE: The check for de_refcnt > 0 below insures the denode
 *  being examined does not represent an unlinked but
 *  still open file.  These files are not to be accessible
 *  even when the directory entry that represented the
 *  file happens to be reused while the deleted file is still
 *  open.
 */
	deh = &dehead[DEHASH(dev, dirclust + diroffset)];
loop:
	for (ldep = deh->deh_chain[0]; ldep != (struct denode *)deh;
		ldep = ldep->de_forw) {
		if (dev != ldep->de_dev || ldep->de_refcnt == 0)
			continue;
		if (dirclust  != ldep->de_dirclust
		    || diroffset != ldep->de_diroffset)
			continue;
		if (ldep->de_flag & DELOCKED) {
			/* should we brelse() the passed buf hdr to
			 *  avoid some potential deadlock? */
			ldep->de_flag |= DEWANT;
			tsleep((caddr_t)ldep, PINOD, "deget", 0);
			goto loop;
		}
		if (vget(DETOV(ldep)))
			goto loop;
		*depp = ldep;
		return 0;
	}


/*
 *  Directory entry was not in cache, have to create
 *  a vnode and copy it from the passed disk buffer.
 */
	/* getnewvnode() does a VREF() on the vnode */
	if (error = getnewvnode(VT_PCFS, mntp, &pcfs_vnodeops, &nvp)) {
		*depp = 0;
		return error;
	}
	ldep = VTODE(nvp);
	ldep->de_vnode = nvp;
	ldep->de_flag = 0;
	ldep->de_devvp = 0;
	ldep->de_lockf = 0;
	ldep->de_dev   = dev;
	fc_purge(ldep, 0);	/* init the fat cache for this denode */

/*
 *  Insert the denode into the hash queue and lock the
 *  denode so it can't be accessed until we've read it
 *  in and have done what we need to it.
 */
	insque(ldep, deh);
	DELOCK(ldep);

	/*
	 *  Copy the directory entry into the denode area of the
	 *  vnode.
	 */
	if (dirclust == PCFSROOT && diroffset == PCFSROOT_OFS) {
		/*  Directory entry for the root directory.
		 *  There isn't one, so we manufacture one.
		 *  We should probably rummage through the root directory and
		 *  find a label entry (if it exists), and then use the time
		 *  and date from that entry as the time and date for the
		 *  root denode.
		 */
		ldep->de_Attributes = ATTR_DIRECTORY;
		ldep->de_StartCluster = PCFSROOT;
		ldep->de_FileSize = pmp->pm_rootdirsize * 512; /* Jim Jegers*/
		/* fill in time and date so that dos2unixtime() doesn't
		 * spit up when called from pcfs_getattr() with root denode */
		ldep->de_Time = 0x0000;		/* 00:00:00	*/
		ldep->de_Date = (0 << 9) | (1 << 5) | (1 << 0);
						/* Jan 1, 1980	*/
		/* leave the other fields as garbage */
	}
	else {
		bp = NULL;
		if (!direntptr) {
			error = readep(pmp, dirclust, diroffset, &bp,
				       &direntptr);
			if (error)
				return error;
		}
		ldep->de_de = *direntptr;
		if (bp)
			brelse (bp);
	}

/*
 *  Fill in a few fields of the vnode and finish filling
 *  in the denode.  Then return the address of the found
 *  denode.
 */
	ldep->de_pmp = pmp;
	ldep->de_devvp = pmp->pm_devvp;
	ldep->de_refcnt = 1;
	ldep->de_dirclust = dirclust;
	ldep->de_diroffset = diroffset;
	if (ldep->de_Attributes & ATTR_DIRECTORY) {
		/*
		 *  Since DOS directory entries that describe directories
		 *  have 0 in the filesize field, we take this opportunity
		 *  to find out the length of the directory and plug it
		 *  into the denode structure.
		 */
		u_long size;

		nvp->v_type = VDIR;
		if (ldep->de_StartCluster == PCFSROOT)
			nvp->v_flag |= VROOT;
		else {
			error = pcbmap(ldep, 0xffff, 0, &size);
			if (error == E2BIG) {
				ldep->de_FileSize = size << pmp->pm_cnshift;
				error = 0;
			}
			else
				printf("deget(): pcbmap returned %d\n", error);
		}
	}
	else
		nvp->v_type = VREG;
	VREF(ldep->de_devvp);
	*depp = ldep;
	return 0;
}

void
deput(dep)
	struct denode *dep;
{
	if ((dep->de_flag & DELOCKED) == 0)
		panic("deput: denode not locked");
	DEUNLOCK(dep);
	vrele(DETOV(dep));
}

int
deupdat(dep, tp, waitfor)
	struct denode *dep;
	struct timeval *tp;
	int waitfor;
{
	int error;
	daddr_t bn;
	int diro;
	struct buf *bp;
	struct direntry *dirp;
	struct pcfsmount *pmp = dep->de_pmp;
	struct vnode *vp = DETOV(dep);
#if defined(PCFSDEBUG)
printf("deupdat(): dep %08x\n", dep);
#endif /* defined(PCFSDEBUG) */

/*
 *  If the update bit is off, or this denode is from
 *  a readonly filesystem, or this denode is for a
 *  directory, or the denode represents an open but
 *  unlinked file then don't do anything.  DOS directory
 *  entries that describe a directory do not ever
 *  get updated.  This is the way dos treats them.
 */
	if ((dep->de_flag & DEUPD) == 0  ||
	    vp->v_mount->mnt_flag & MNT_RDONLY  ||
	    dep->de_Attributes & ATTR_DIRECTORY  ||
	    dep->de_refcnt <= 0)
		return 0;

/*
 *  Read in the cluster containing the directory entry
 *  we want to update.
 */
	if (error = readde(dep, &bp, &dirp))
		return error;

/*
 *  Put the passed in time into the directory entry.
 */
	unix2dostime(&time, (union dosdate *)&dep->de_Date,
		(union dostime *)&dep->de_Time);
	dep->de_flag &= ~DEUPD;

/*
 *  Copy the directory entry out of the denode into
 *  the cluster it came from.
 */
	*dirp = dep->de_de;	/* structure copy */

/*
 *  Write the cluster back to disk.  If they asked
 *  for us to wait for the write to complete, then
 *  use bwrite() otherwise use bdwrite().
 */
	error = 0;	/* note that error is 0 from above, but ... */
	if (waitfor)
		error = bwrite(bp);
	else
		bdwrite(bp);
	return error;
}

/*
 *  Truncate the file described by dep to the length
 *  specified by length.
 */
int
detrunc(dep, length, flags)
	struct denode *dep;
	u_long length;
	int flags;
{
	int error;
	int allerror;
	u_long eofentry;
	u_long chaintofree;
	daddr_t bn;
	int boff;
	int isadir = dep->de_Attributes & ATTR_DIRECTORY;
	struct buf *bp;
	struct pcfsmount *pmp = dep->de_pmp;
#if defined(PCFSDEBUG)
printf("detrunc(): file %s, length %d, flags %d\n", dep->de_Name, length, flags);
#endif /* defined(PCFSDEBUG) */

/*
 *  Disallow attempts to truncate the root directory
 *  since it is of fixed size.  That's just the way
 *  dos filesystems are.  We use the VROOT bit in the
 *  vnode because checking for the directory bit and
 *  a startcluster of 0 in the denode is not adequate
 *  to recognize the root directory at this point in
 *  a file or directory's life.
 */
	if (DETOV(dep)->v_flag & VROOT) {
		printf("detrunc(): can't truncate root directory, clust %d, offset %d\n",
			dep->de_dirclust, dep->de_diroffset);
		return EINVAL;
	}

	vnode_pager_setsize(DETOV(dep), length);

	if (dep->de_FileSize <= length) {
		dep->de_flag |= DEUPD;
		error = deupdat(dep, &time, 1);
#if defined(PCFSDEBUG)
printf("detrunc(): file is shorter return point, errno %d\n", error);
#endif /* defined(PCFSDEBUG) */
		return error;
	}

/*
 *  If the desired length is 0 then remember the starting
 *  cluster of the file and set the StartCluster field in
 *  the directory entry to 0.  If the desired length is
 *  not zero, then get the number of the last cluster in
 *  the shortened file.  Then get the number of the first
 *  cluster in the part of the file that is to be freed.
 *  Then set the next cluster pointer in the last cluster
 *  of the file to CLUST_EOFE.
 */
	if (length == 0) {
		chaintofree = dep->de_StartCluster;
		dep->de_StartCluster = 0;
		eofentry = ~0;
	} else {
		error = pcbmap(dep, (length-1) >> pmp->pm_cnshift,
				0, &eofentry);
		if (error) {
#if defined(PCFSDEBUG)
printf("detrunc(): pcbmap fails %d\n", error);
#endif /* defined(PCFSDEBUG) */
			return error;
		}
	}

	fc_purge(dep, (length + pmp->pm_crbomask) >> pmp->pm_cnshift);

/*
 *  If the new length is not a multiple of the cluster size
 *  then we must zero the tail end of the new last cluster in case
 *  it becomes part of the file again because of a seek.
 */
	if ((boff = length & pmp->pm_crbomask) != 0) {
		/* should read from file vnode or
		 * filesystem vnode depending on if file or dir */
		if (isadir) {
			bn = cntobn(pmp, eofentry);
			error = bread(pmp->pm_devvp, bn, pmp->pm_bpcluster,
				NOCRED, &bp);
		} else {
			bn = (length-1) >> pmp->pm_cnshift;
			error = bread(DETOV(dep), bn, pmp->pm_bpcluster,
				NOCRED, &bp);
		}
		if (error) {
#if defined(PCFSDEBUG)
printf("detrunc(): bread fails %d\n", error);
#endif /* defined(PCFSDEBUG) */
			return error;
		}
		vnode_pager_uncache(DETOV(dep));	/* what's this for? */
							/* is this the right
							 *  place for it? */
		bzero(bp->b_un.b_addr + boff, pmp->pm_bpcluster - boff);
		if (flags & IO_SYNC)
			bwrite(bp);
		else
			bdwrite(bp);
	}

/*
 *  Write out the updated directory entry.  Even
 *  if the update fails we free the trailing clusters.
 */
	dep->de_FileSize = length;
	dep->de_flag |= DEUPD;
	vinvalbuf(DETOV(dep), length > 0);
	allerror = deupdat(dep, &time, MNT_WAIT);
#if defined(PCFSDEBUG)
printf("detrunc(): allerror %d, eofentry %d\n",
	allerror, eofentry);
#endif /* defined(PCFSDEBUG) */

/*
 *  If we need to break the cluster chain for the file
 *  then do it now.
 */
	if (eofentry != ~0) {
		error = fatentry(FAT_GET_AND_SET, pmp, eofentry,
			&chaintofree, CLUST_EOFE);
		if (error) {
#if defined(PCFSDEBUG)
printf("detrunc(): fatentry errors %d\n", error);
#endif /* defined(PCFSDEBUG) */
			return error;
		}
		fc_setcache(dep, FC_LASTFC, (length - 1) >> pmp->pm_cnshift,
			eofentry);
	}

/*
 *  Now free the clusters removed from the file because
 *  of the truncation.
 */
	if (chaintofree != 0  &&  !PCFSEOF(chaintofree))
		freeclusterchain(pmp, chaintofree);

	return allerror;
}

/*
 *  Move a denode to its correct hash queue after
 *  the file it represents has been moved to a new
 *  directory.
 */
void
reinsert(dep)
	struct denode *dep;
{
	struct pcfsmount *pmp = dep->de_pmp;
	union dehead *deh;

/*
 *  Fix up the denode cache.  If the denode is
 *  for a directory, there is nothing to do since the
 *  hash is based on the starting cluster of the directory
 *  file and that hasn't changed.  If for a file the hash
 *  is based on the location
 *  of the directory entry, so we must remove it from the
 *  cache and re-enter it with the hash based on the new
 *  location of the directory entry.
 */
	if ((dep->de_Attributes & ATTR_DIRECTORY) == 0) {
		remque(dep);
		deh = &dehead[DEHASH(pmp->pm_dev,
			dep->de_dirclust + dep->de_diroffset)];
		insque(dep, deh);
	}
}

int pcfs_prtactive;	/* print reclaims of active vnodes */

int
pcfs_reclaim(vp)
	struct vnode *vp;
{
	struct denode *dep = VTODE(vp);
	int i;
#if defined(PCFSDEBUG)
printf("pcfs_reclaim(): dep %08x, file %s, refcnt %d\n",
	dep, dep->de_Name, dep->de_refcnt);
#endif /* defined(PCFSDEBUG) */

	if (pcfs_prtactive && vp->v_usecount != 0)
		vprint("pcfs_reclaim(): pushing active", vp);

/*
 *  Remove the denode from the denode hash chain we
 *  are in.
 */
	remque(dep);
	dep->de_forw = dep;
	dep->de_back = dep;

	cache_purge(vp);
/*
 *  Indicate that one less file on the filesystem is open.
 */
	if (dep->de_devvp) {
		vrele(dep->de_devvp);
		dep->de_devvp = 0;
	}

	dep->de_flag = 0;
	return 0;
}

int
pcfs_inactive(vp, p)
	struct vnode *vp;
	struct proc *p;
{
	struct denode *dep = VTODE(vp);
	int error = 0;
#if defined(PCFSDEBUG)
printf("pcfs_inactive(): dep %08x, de_Name[0] %x\n", dep, dep->de_Name[0]);
#endif /* defined(PCFSDEBUG) */

	if (pcfs_prtactive && vp->v_usecount != 0)
		vprint("pcfs_inactive(): pushing active", vp);

/*
 *  Get rid of denodes related to stale file handles.
 *  Hmmm, what does this really do?
 */
	if (dep->de_Name[0] == SLOT_DELETED) {
		if ((vp->v_flag & VXLOCK) == 0)
			vgone(vp);
		return 0;
	}

/*
 *  If the file has been deleted and it is on a read/write
 *  filesystem, then truncate the file, and mark the directory
 *  slot as empty.  (This may not be necessary for the dos
 *  filesystem.
 */
#if defined(PCFSDEBUG)
printf("pcfs_inactive(): dep %08x, refcnt %d, mntflag %x, MNT_RDONLY %x\n",
	dep, dep->de_refcnt, vp->v_mount->mnt_flag, MNT_RDONLY);
#endif /* defined(PCFSDEBUG) */
	DELOCK(dep);
	if (dep->de_refcnt <= 0  &&  (vp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
		error = detrunc(dep, (u_long)0, 0);
		dep->de_flag |= DEUPD;
		dep->de_Name[0] = SLOT_DELETED;
	}
	DEUPDAT(dep, &time, 0);
	DEUNLOCK(dep);
	dep->de_flag = 0;

/*
 *  If we are done with the denode, then reclaim
 *  it so that it can be reused now.
 */
#if defined(PCFSDEBUG)
printf("pcfs_inactive(): v_usecount %d, de_Name[0] %x\n", vp->v_usecount,
	dep->de_Name[0]);
#endif /* defined(PCFSDEBUG) */
	if (vp->v_usecount == 0  &&  dep->de_Name[0] == SLOT_DELETED)
		vgone(vp);
	return error;
}

void
delock(dep)
	struct denode *dep;
{
	while (dep->de_flag & DELOCKED) {
		dep->de_flag |= DEWANT;
		if (dep->de_spare0 == curproc->p_pid)
			panic("delock: locking against myself");
		dep->de_spare1 = curproc->p_pid;
		(void) tsleep((caddr_t)dep, PINOD, "delock", 0);
	}
	dep->de_spare1 = 0;
	dep->de_spare0 = curproc->p_pid;
	dep->de_flag |= DELOCKED;
}

void
deunlock(dep)
	struct denode *dep;
{
	if ((dep->de_flag & DELOCKED) == 0)
		vprint("deunlock: found unlocked denode", DETOV(dep));
	dep->de_spare0 = 0;
	dep->de_flag &= ~DELOCKED;
	if (dep->de_flag & DEWANT) {
		dep->de_flag &= ~DEWANT;
		wakeup((caddr_t)dep);
	}
}
