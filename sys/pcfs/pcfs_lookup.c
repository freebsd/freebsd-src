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
 *	$Id: pcfs_lookup.c,v 1.4 1993/10/17 01:48:37 rgrimes Exp $
 */

#include "param.h"
#include "systm.h"
#include "namei.h"
#include "buf.h"
#include "vnode.h"
#include "mount.h"

#include "bpb.h"
#include "direntry.h"
#include "denode.h"
#include "pcfsmount.h"
#include "fat.h"

/*
 *  When we search a directory the blocks containing directory
 *  entries are read and examined.  The directory entries
 *  contain information that would normally be in the inode
 *  of a unix filesystem.  This means that some of a directory's
 *  contents may also be in memory resident denodes (sort of
 *  an inode).  This can cause problems if we are searching
 *  while some other process is modifying a directory.  To
 *  prevent one process from accessing incompletely modified
 *  directory information we depend upon being the soul owner
 *  of a directory block.  bread/brelse provide this service.
 *  This being the case, when a process modifies a directory
 *  it must first acquire the disk block that contains the
 *  directory entry to be modified.  Then update the disk
 *  block and the denode, and then write the disk block out
 *  to disk.  This way disk blocks containing directory
 *  entries and in memory denode's will be in synch.
 */
int
pcfs_lookup(vdp, ndp, p)
	struct vnode *vdp;	/* vnode of directory to search		*/
	struct nameidata *ndp;
	struct proc *p;
{
	daddr_t bn;
	int flag;
	int error;
	int lockparent;
	int wantparent;
	int slotstatus;
#define	NONE	0
#define	FOUND	1
	int slotoffset;
	int slotcluster;
	int frcn;
	u_long cluster;
	int rootreloff;
	int diroff;
	int isadir;		/* ~0 if found direntry is a directory	*/
	u_long scn;		/* starting cluster number		*/
	struct denode *dp;
	struct denode *pdp;
	struct denode *tdp;
	struct pcfsmount *pmp;
	struct buf *bp = 0;
	struct direntry *dep;
	u_char dosfilename[12];

#if defined(PCFSDEBUG)
printf("pcfs_lookup(): looking for %s\n", ndp->ni_ptr);
#endif /* defined(PCFSDEBUG) */
	ndp->ni_dvp = vdp;
	ndp->ni_vp  = NULL;
	dp = VTODE(vdp);
	pmp = dp->de_pmp;
	lockparent = ndp->ni_nameiop & LOCKPARENT;
	flag = ndp->ni_nameiop & OPMASK;
	wantparent = ndp->ni_nameiop & (LOCKPARENT | WANTPARENT);
#if defined(PCFSDEBUG)
printf("pcfs_lookup(): vdp %08x, dp %08x, Attr %02x\n",
	vdp, dp, dp->de_Attributes);
#endif /* defined(PCFSDEBUG) */

/*
 *  Be sure vdp is a directory.  Since dos filesystems
 *  don't have the concept of execute permission anybody
 *  can search a directory.
 */
	if ((dp->de_Attributes & ATTR_DIRECTORY) == 0)
		return ENOTDIR;

/*
 *  See if the component of the pathname we are looking for
 *  is in the directory cache.  If so then do a few things
 *  and return.
 */
	if (error = cache_lookup(ndp)) {
		int vpid;

		if (error == ENOENT)
			return error;
#ifdef PARANOID
		if (vdp == ndp->ni_rootdir  &&  ndp->ni_isdotdot)
			panic("pcfs_lookup: .. thru root");
#endif /* PARANOID */
		pdp = dp;
		vdp = ndp->ni_vp;
		dp  = VTODE(vdp);
		vpid = vdp->v_id;
		if (pdp == dp) {
			VREF(vdp);
			error = 0;
		} else if (ndp->ni_isdotdot) {
			DEUNLOCK(pdp);
			error = vget(vdp);
			if (!error && lockparent && *ndp->ni_next == '\0')
				DELOCK(pdp);
		} else {
			error = vget(vdp);
			if (!lockparent || error || *ndp->ni_next != '\0')
				DEUNLOCK(pdp);
		}

		if (!error) {
			if (vpid == vdp->v_id) {
#if defined(PCFSDEBUG)
printf("pcfs_lookup(): cache hit, vnode %08x, file %s\n", vdp, dp->de_Name);
#endif /* defined(PCFSDEBUG) */
				return 0;
			}
			deput(dp);
			if (lockparent && pdp != dp && *ndp->ni_next == '\0')
				DEUNLOCK(pdp);
		}
		DELOCK(pdp);
		dp = pdp;
		vdp = DETOV(dp);
		ndp->ni_vp = NULL;
	}

/*
 *  If they are going after the . or .. entry in the
 *  root directory, they won't find it.  DOS filesystems
 *  don't have them in the root directory.  So, we fake it.
 *  deget() is in on this scam too.
 */
	if ((vdp->v_flag & VROOT)  &&  ndp->ni_ptr[0] == '.'  &&
	    (ndp->ni_namelen == 1  ||
	     (ndp->ni_namelen == 2  &&  ndp->ni_ptr[1] == '.'))) {
		isadir = ATTR_DIRECTORY;
		scn = PCFSROOT;
#if defined(PCFSDEBUG)
printf("pcfs_lookup(): looking for . or .. in root directory\n");
#endif /* defined(PCFSDEBUG) */
		cluster == PCFSROOT;
		diroff = PCFSROOT_OFS;
		goto foundroot;
	}

/*
 *  Don't search for free slots unless we are creating
 *  a filename and we are at the end of the pathname.
 */
	slotstatus = FOUND;
	if ((flag == CREATE || flag == RENAME) && *ndp->ni_next == '\0') {
		slotstatus = NONE;
		slotoffset = -1;
	}

	unix2dosfn((u_char *)ndp->ni_ptr, dosfilename, ndp->ni_namelen);
	dosfilename[11] = 0;
#if defined(PCFSDEBUG)
printf("pcfs_lookup(): dos version of filename %s, length %d\n",
	dosfilename, ndp->ni_namelen);
#endif /* defined(PCFSDEBUG) */
/*
 *  Search the directory pointed at by vdp for the
 *  name pointed at by ndp->ni_ptr.
 */
	tdp = NULL;
/*
 *  The outer loop ranges over the clusters that make
 *  up the directory.  Note that the root directory is
 *  different from all other directories.  It has a
 *  fixed number of blocks that are not part of the
 *  pool of allocatable clusters.  So, we treat it a
 *  little differently.
 *  The root directory starts at "cluster" 0.
 */
	rootreloff = 0;
	for (frcn = 0; ; frcn++) {
		if (error = pcbmap(dp, frcn, &bn, &cluster)) {
			if (error == E2BIG)
				break;
			return error;
		}
		if (error = bread(pmp->pm_devvp, bn, pmp->pm_bpcluster, NOCRED, &bp))
			return error;
		for (diroff = 0; diroff < pmp->pm_depclust; diroff++) {
			dep = (struct direntry *)bp->b_un.b_addr + diroff;

/*
 *  If the slot is empty and we are still looking for
 *  an empty then remember this one.  If the slot is
 *  not empty then check to see if it matches what we
 *  are looking for.  If the slot has never been filled
 *  with anything, then the remainder of the directory
 *  has never been used, so there is no point in searching
 *  it.
 */
			if (dep->deName[0] == SLOT_EMPTY   ||
			    dep->deName[0] == SLOT_DELETED) {
				if (slotstatus != FOUND) {
					slotstatus  = FOUND;
					if (cluster == PCFSROOT)
						slotoffset = rootreloff;
					else
						slotoffset = diroff;
					slotcluster = cluster;
				}
				if (dep->deName[0] == SLOT_EMPTY) {
					brelse(bp);
					goto notfound;
				}
			} else {
				/* Ignore volume labels (anywhere, not just
				 * the root directory). */
				if ((dep->deAttributes & ATTR_VOLUME) == 0  &&
				    bcmp(dosfilename, dep->deName, 11) == 0) {
#if defined(PCFSDEBUG)
printf("pcfs_lookup(): match diroff %d, rootreloff %d\n", diroff, rootreloff);
#endif /* defined(PCFSDEBUG) */
/*
 *  Remember where this directory entry came from
 *  for whoever did this lookup.
 *  If this is the root directory we are interested
 *  in the offset relative to the beginning of the
 *  directory (not the beginning of the cluster).
 */
					if (cluster == PCFSROOT)
						diroff = rootreloff;
					ndp->ni_pcfs.pcfs_offset = diroff;
					ndp->ni_pcfs.pcfs_cluster = cluster;
					goto found;
				}
			}
			rootreloff++;
		}			/* for (diroff = 0; .... */
/*
 *  Release the buffer holding the directory cluster
 *  just searched.
 */
		brelse(bp);
	}			/* for (frcn = 0; ; frcn++) */
notfound:;
/*
 *  We hold no disk buffers at this point.
 */

/*
 *  If we get here we didn't find the entry we were looking
 *  for.  But that's ok if we are creating or renaming and
 *  are at the end of the pathname and the directory hasn't
 *  been removed.
 */
#if defined(PCFSDEBUG)
printf("pcfs_lookup(): flag %d, refcnt %d, slotstatus %d\n",
	flag, dp->de_refcnt, slotstatus);
printf("               slotoffset %d, slotcluster %d\n",
	slotoffset, slotcluster);
#endif /* defined(PCFSDEBUG) */
	if ((flag == CREATE || flag == RENAME)  &&
	    *ndp->ni_next == '\0' && dp->de_refcnt != 0) {
		if (slotstatus == NONE) {
			ndp->ni_pcfs.pcfs_offset  = 0;
			ndp->ni_pcfs.pcfs_cluster = 0;
			ndp->ni_pcfs.pcfs_count   = 0;
		} else {
#if defined(PCFSDEBUG)
printf("pcfs_lookup(): saving empty slot location\n");
#endif /* defined(PCFSDEBUG) */
			ndp->ni_pcfs.pcfs_offset  = slotoffset;
			ndp->ni_pcfs.pcfs_cluster = slotcluster;
			ndp->ni_pcfs.pcfs_count   = 1;
		}
/*		dp->de_flag |= DEUPD; /* never update dos directories */
		ndp->ni_nameiop |= SAVENAME;
		if (!lockparent)	/* leave searched dir locked?	*/
			DEUNLOCK(dp);
	}
/*
 *  Insert name in cache as non-existant if not
 *  trying to create it.
 */
	if (ndp->ni_makeentry && flag != CREATE)
		cache_enter(ndp);
	return ENOENT;

found:;
/*
 *  NOTE:  We still have the buffer with matched
 *  directory entry at this point.
 */
	isadir = dep->deAttributes & ATTR_DIRECTORY;
	scn = dep->deStartCluster;

foundroot:;
/*
 *  If we entered at foundroot, then we are looking
 *  for the . or .. entry of the filesystems root
 *  directory.  isadir and scn were setup before
 *  jumping here.  And, bp is null.  There is no buf header.
 */

/*
 *  If deleting and at the end of the path, then
 *  if we matched on "." then don't deget() we would
 *  probably panic().  Otherwise deget() the directory
 *  entry.
 */
	if (flag == DELETE && ndp->ni_next == '\0') {
		if (dp->de_StartCluster == scn  &&
		    isadir) { /* "." */
			VREF(vdp);
			ndp->ni_vp = vdp;
			if (bp) brelse(bp);
			return 0;
		}
		error = deget(pmp, cluster, diroff, dep, &tdp);
		if (error) {
			if (bp) brelse(bp);
			return error;
		}
		ndp->ni_vp = DETOV(tdp);
		if (!lockparent)
			DEUNLOCK(dp);
		if (bp) brelse(bp);
		return 0;
	}

/*
 *  If renaming.
 */
	if (flag == RENAME && wantparent && *ndp->ni_next == '\0') {
		if (dp->de_StartCluster == scn  &&
		    isadir) {
			if (bp) brelse(bp);
			return EISDIR;
		}
		error = deget(pmp, cluster, diroff, dep, &tdp);
		if (error) {
			if (bp) brelse(bp);
			return error;
		}
		ndp->ni_vp = DETOV(tdp);
		ndp->ni_nameiop |= SAVENAME;
		if (!lockparent)
			DEUNLOCK(dp);
		if (bp) brelse(bp);
		return 0;
	}

/*
 *  ?
 */
	pdp = dp;
	if (ndp->ni_isdotdot) {
		DEUNLOCK(pdp);
		error = deget(pmp, cluster, diroff, dep, &tdp);
		if (error) {
			DELOCK(pdp);
			if (bp) brelse(bp);
			return error;
		}
		if (lockparent && *ndp->ni_next == '\0')
			DELOCK(pdp);
		ndp->ni_vp = DETOV(tdp);
	} else if (dp->de_StartCluster == scn  &&
		   isadir) { /* "." */
		VREF(vdp);
		ndp->ni_vp = vdp;
	} else {
		error = deget(pmp, cluster, diroff, dep, &tdp);
		if (error) {
			if (bp) brelse(bp);
			return error;
		}
		if (!lockparent || *ndp->ni_next != '\0')
			DEUNLOCK(pdp);
		ndp->ni_vp = DETOV(tdp);
	}
	if (bp) brelse(bp);

/*
 *  Insert name in cache if wanted.
 */
	if (ndp->ni_makeentry)
		cache_enter(ndp);
	return 0;
}

/*
 *  dep - directory to copy into the directory
 *  ndp - nameidata structure containing info on
 *    where to put the directory entry in the directory.
 *  depp - return the address of the denode for the
 *    created directory entry if depp != 0
 */
int
createde(dep, ndp, depp)
	struct denode *dep;
	struct nameidata *ndp;
	struct denode **depp;
{
	int bn;
	int error;
	u_long dirclust, diroffset;
	struct direntry *ndep;
	struct denode *ddep = VTODE(ndp->ni_dvp);	/* directory to add to */
	struct pcfsmount *pmp = dep->de_pmp;
	struct buf *bp;
#if defined(PCFSDEBUG)
printf("createde(dep %08x, ndp %08x, depp %08x)\n", dep, ndp, depp);
#endif /* defined(PCFSDEBUG) */

/*
 *  If no space left in the directory then allocate
 *  another cluster and chain it onto the end of the
 *  file.  There is one exception to this.  That is,
 *  if the root directory has no more space it can NOT
 *  be expanded.  extendfile() checks for and fails attempts to
 *  extend the root directory.  We just return an error
 *  in that case.
 */
	if (ndp->ni_pcfs.pcfs_count == 0) {
		if (error = extendfile(ddep, &bp, &dirclust))
			return error;
		ndep = (struct direntry *)bp->b_un.b_addr;
/*
 *  Let caller know where we put the directory entry.
 */
		ndp->ni_pcfs.pcfs_cluster = dirclust;
		ndp->ni_pcfs.pcfs_offset  = diroffset = 0;
	}

	else {
/*
 *  There is space in the existing directory.  So,
 *  we just read in the cluster with space.  Copy
 *  the new directory entry in.  Then write it to
 *  disk.
 *  NOTE:  DOS directories do not get smaller as
 *  clusters are emptied.
 */
		dirclust = ndp->ni_pcfs.pcfs_cluster;
		diroffset = ndp->ni_pcfs.pcfs_offset;

		error = readep(pmp, dirclust, diroffset, &bp, &ndep);
		if (error)
			return error;
	}
	*ndep = dep->de_de;
/*
 *  If they want us to return with the denode gotten.
 */
	if (depp) {
		error = deget(pmp, dirclust, diroffset, ndep, depp);
		if (error)
			return error;
	}
	if (error = bwrite(bp))
/*deput()?*/
		return error;
	return 0;
}

/*
 *  Read in a directory entry and mark it as being deleted.
 */
int
markdeleted(pmp, dirclust, diroffset)
	struct pcfsmount *pmp;
	u_long dirclust;
	u_long diroffset;
{
	int error;
	struct direntry *ep;
	struct buf *bp;

	error = readep(pmp, dirclust, diroffset, &bp, &ep);
	if (error)
		return error;
	ep->deName[0] = SLOT_DELETED;
	return bwrite(bp);
}

/*
 *  Remove a directory entry.
 *  At this point the file represented by the directory
 *  entry to be removed is still full length until no
 *  one has it open.  When the file no longer being
 *  used pcfs_inactive() is called and will truncate
 *  the file to 0 length.  When the vnode containing
 *  the denode is needed for some other purpose by 
 *  VFS it will call pcfs_reclaim() which will remove
 *  the denode from the denode cache.
 */
int
removede(ndp)
	struct nameidata *ndp;
{
	struct denode *dep = VTODE(ndp->ni_vp);	/* the file being removed */
	struct pcfsmount *pmp = dep->de_pmp;
	int error;

#if defined(PCFSDEBUG)
/*printf("removede(): filename %s\n", dep->de_Name);
printf("rmde(): dep %08x, ndpcluster %d, ndpoffset %d\n",
	dep, ndp->ni_pcfs.pcfs_cluster, ndp->ni_pcfs.pcfs_offset);*/
#endif /* defined(PCFSDEBUG) */

/*
 *  Read the directory block containing the directory
 *  entry we are to make free.  The nameidata structure
 *  holds the cluster number and directory entry index
 *  number of the entry to free.
 */
	error = markdeleted(pmp, ndp->ni_pcfs.pcfs_cluster,
		ndp->ni_pcfs.pcfs_offset);

	dep->de_refcnt--;
	return error;
}

/*
 *  Be sure a directory is empty except for "." and "..".
 *  Return 1 if empty, return 0 if not empty or error.
 */
int
dosdirempty(dep)
	struct denode *dep;
{
	int dei;
	int error;
	u_long cn;
	daddr_t bn;
	struct buf *bp;
	struct pcfsmount *pmp = dep->de_pmp;
	struct direntry *dentp;

/*
 *  Since the filesize field in directory entries for a directory
 *  is zero, we just have to feel our way through the directory
 *  until we hit end of file.
 */
	for (cn = 0;; cn++) {
		error = pcbmap(dep, cn, &bn, 0);
		if (error == E2BIG)
			return 1;	/* it's empty */
		error = bread(pmp->pm_devvp, bn, pmp->pm_bpcluster, NOCRED,
			&bp);
		if (error)
			return error;
		dentp = (struct direntry *)bp->b_un.b_addr;
		for (dei = 0; dei < pmp->pm_depclust; dei++) {
			if (dentp->deName[0] != SLOT_DELETED) {
/*
 *  In dos directories an entry whose name starts with SLOT_EMPTY (0)
 *  starts the beginning of the unused part of the directory, so we
 *  can just return that it is empty.
 */
				if (dentp->deName[0] == SLOT_EMPTY) {
					brelse(bp);
					return 1;
				}
/*
 *  Any names other than "." and ".." in a directory mean
 *  it is not empty.
 */
				if (bcmp(dentp->deName, ".          ", 11)  &&
				    bcmp(dentp->deName, "..         ", 11)) {
					brelse(bp);
#if defined(PCFSDEBUG)
printf("dosdirempty(): entry %d found %02x, %02x\n", dei, dentp->deName[0],
	dentp->deName[1]);
#endif /* defined(PCFSDEBUG) */
					return 0;	/* not empty */
				}
			}
			dentp++;
		}
		brelse(bp);
	}
	/*NOTREACHED*/
}

/*
 *  Check to see if the directory described by target is
 *  in some subdirectory of source.  This prevents something
 *  like the following from succeeding and leaving a bunch
 *  or files and directories orphaned.
 *	mv /a/b/c /a/b/c/d/e/f
 *  Where c and f are directories.
 *  source - the inode for /a/b/c
 *  target - the inode for /a/b/c/d/e/f
 *  Returns 0 if target is NOT a subdirectory of source.
 *  Otherwise returns a non-zero error number.
 *  The target inode is always unlocked on return.
 */
int
doscheckpath(source, target)
	struct denode *source;
	struct denode *target;
{
	daddr_t scn;
	struct denode dummy;
	struct pcfsmount *pmp;
	struct direntry *ep;
	struct denode *dep;
	struct buf *bp = NULL;
	int error = 0;

	dep = target;
	if ((target->de_Attributes & ATTR_DIRECTORY) == 0  ||
	    (source->de_Attributes & ATTR_DIRECTORY) == 0) {
		error = ENOTDIR;
		goto out;
	}
	if (dep->de_StartCluster == source->de_StartCluster) {
		error = EEXIST;
		goto out;
	}
	if (dep->de_StartCluster == PCFSROOT)
		goto out;
	for (;;) {
		if ((dep->de_Attributes & ATTR_DIRECTORY) == 0) {
			error = ENOTDIR;
			goto out;
		}
		pmp = dep->de_pmp;
		scn = dep->de_StartCluster;
		error = bread(pmp->pm_devvp, cntobn(pmp, scn),
			pmp->pm_bpcluster, NOCRED, &bp);
		if (error) {
			break;
		}
		ep = (struct direntry *)bp->b_un.b_addr + 1;
		if ((ep->deAttributes & ATTR_DIRECTORY) == 0  ||
		    bcmp(ep->deName, "..         ", 11) != 0) {
			error = ENOTDIR;
			break;
		}
		if (ep->deStartCluster == source->de_StartCluster) {
			error = EINVAL;
			break;
		}
		if (ep->deStartCluster == PCFSROOT)
			break;
		deput(dep);
		/* NOTE: deget() clears dep on error */
		error = deget(pmp, ep->deStartCluster, 0, ep, &dep);
		brelse(bp);
		bp = NULL;
		if (error)
			break;
	}
out:;
	if (bp)
		brelse(bp);
	if (error == ENOTDIR)
		printf("doscheckpath(): .. not a directory?\n");
	if (dep != NULL)
		deput(dep);
	return error;
}

/*
 *  Read in the disk block containing the directory entry
 *  (dirclu, dirofs) and return the address of the buf header,
 *  and the address of the directory entry within the block.
 */
int
readep(pmp, dirclu, dirofs, bpp, epp)
	struct pcfsmount *pmp;
	u_long dirclu, dirofs;
	struct buf **bpp;
	struct direntry **epp;
{
	int error;
	daddr_t bn;

	bn = detobn(pmp, dirclu, dirofs);
	if (error = bread(pmp->pm_devvp, bn, pmp->pm_bpcluster, NOCRED, bpp)) {
		*bpp = NULL;
		return error;
	}
	if (epp)
		*epp = bptoep(pmp, *bpp, dirofs);
	return 0;
}


/*
 *  Read in the disk block containing the directory entry
 *  dep came from and return the address of the buf header,
 *  and the address of the directory entry within the block.
 */
int
readde(dep, bpp, epp)
	struct denode *dep;
	struct buf **bpp;
	struct direntry **epp;
{
	return readep(dep->de_pmp, dep->de_dirclust, dep->de_diroffset,
		      bpp, epp);
}
