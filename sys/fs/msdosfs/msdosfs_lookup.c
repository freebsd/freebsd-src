/*	$Id: msdosfs_lookup.c,v 1.13 1997/09/02 20:06:17 bde Exp $ */
/*	$NetBSD: msdosfs_lookup.c,v 1.14 1994/08/21 18:44:07 ws Exp $	*/

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
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/systm.h>

#include <msdosfs/bpb.h>
#include <msdosfs/direntry.h>
#include <msdosfs/denode.h>
#include <msdosfs/msdosfsmount.h>
#include <msdosfs/fat.h>

static int	markdeleted __P((struct msdosfsmount *pmp, u_long dirclust,
				 u_long diroffset));

/*
 * When we search a directory the blocks containing directory entries are
 * read and examined.  The directory entries contain information that would
 * normally be in the inode of a unix filesystem.  This means that some of
 * a directory's contents may also be in memory resident denodes (sort of
 * an inode).  This can cause problems if we are searching while some other
 * process is modifying a directory.  To prevent one process from accessing
 * incompletely modified directory information we depend upon being the
 * sole owner of a directory block.  bread/brelse provide this service.
 * This being the case, when a process modifies a directory it must first
 * acquire the disk block that contains the directory entry to be modified.
 * Then update the disk block and the denode, and then write the disk block
 * out to disk.  This way disk blocks containing directory entries and in
 * memory denode's will be in synch.
 */
int
msdosfs_lookup(ap)
	struct vop_cachedlookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct vnode *vdp = ap->a_dvp;
	struct vnode **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	daddr_t bn;
	int error;
	int lockparent;
	int wantparent;
	int slotstatus;

#define	NONE	0
#define	FOUND	1
	int slotoffset = -1;
	int slotcluster = -1;
	int frcn;
	u_long cluster;
	int rootreloff;
	int diroff;
	int isadir;		/* ~0 if found direntry is a directory	 */
	u_long scn;		/* starting cluster number		 */
	struct vnode *pdp;
	struct denode *dp;
	struct denode *tdp;
	struct msdosfsmount *pmp;
	struct buf *bp = 0;
	struct direntry *dep = NULL;
	struct ucred *cred = cnp->cn_cred;
	u_char dosfilename[12];
	int flags = cnp->cn_flags;
	int nameiop = cnp->cn_nameiop;
	struct proc *p = cnp->cn_proc;

#ifdef MSDOSFS_DEBUG
	printf("msdosfs_lookup(): looking for %s\n", cnp->cn_nameptr);
#endif
	dp = VTODE(vdp);
	pmp = dp->de_pmp;
	*vpp = NULL;
	lockparent = flags & LOCKPARENT;
	wantparent = flags & (LOCKPARENT | WANTPARENT);
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_lookup(): vdp %08x, dp %08x, Attr %02x\n",
	       vdp, dp, dp->de_Attributes);
#endif

	/*
	 * If they are going after the . or .. entry in the root directory,
	 * they won't find it.  DOS filesystems don't have them in the root
	 * directory.  So, we fake it. deget() is in on this scam too.
	 */
	if ((vdp->v_flag & VROOT) && cnp->cn_nameptr[0] == '.' &&
	    (cnp->cn_namelen == 1 ||
		(cnp->cn_namelen == 2 && cnp->cn_nameptr[1] == '.'))) {
		isadir = ATTR_DIRECTORY;
		scn = MSDOSFSROOT;
#ifdef MSDOSFS_DEBUG
		printf("msdosfs_lookup(): looking for . or .. in root directory\n");
#endif
		cluster = MSDOSFSROOT;
		diroff = MSDOSFSROOT_OFS;
		goto foundroot;
	}

	/*
	 * Don't search for free slots unless we are creating a filename
	 * and we are at the end of the pathname.
	 */
	slotstatus = FOUND;
	if ((nameiop == CREATE || nameiop == RENAME) && (flags & ISLASTCN)) {
		slotstatus = NONE;
		slotoffset = -1;
	}

	unix2dosfn((u_char *) cnp->cn_nameptr, dosfilename, cnp->cn_namelen);
	dosfilename[11] = 0;
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_lookup(): dos version of filename %s, length %d\n",
	       dosfilename, cnp->cn_namelen);
#endif
	/*
	 * Search the directory pointed at by vdp for the name pointed at
	 * by cnp->cn_nameptr.
	 */
	tdp = NULL;
	/*
	 * The outer loop ranges over the clusters that make up the
	 * directory.  Note that the root directory is different from all
	 * other directories.  It has a fixed number of blocks that are not
	 * part of the pool of allocatable clusters.  So, we treat it a
	 * little differently. The root directory starts at "cluster" 0.
	 */
	rootreloff = 0;
	for (frcn = 0;; frcn++) {
		error = pcbmap(dp, frcn, &bn, &cluster);
		if (error) {
			if (error == E2BIG)
				break;
			return error;
		}
		error = bread(pmp->pm_devvp, bn, pmp->pm_bpcluster,NOCRED,&bp);
		if (error)
			return error;
		for (diroff = 0; diroff < pmp->pm_depclust; diroff++) {
			dep = (struct direntry *) bp->b_data + diroff;

			/*
			 * If the slot is empty and we are still looking
			 * for an empty then remember this one.  If the
			 * slot is not empty then check to see if it
			 * matches what we are looking for.  If the slot
			 * has never been filled with anything, then the
			 * remainder of the directory has never been used,
			 * so there is no point in searching it.
			 */
			if (dep->deName[0] == SLOT_EMPTY ||
			    dep->deName[0] == SLOT_DELETED) {
				if (slotstatus != FOUND) {
					slotstatus = FOUND;
					if (cluster == MSDOSFSROOT)
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
				/*
				 * Ignore volume labels (anywhere, not just
				 * the root directory).
				 */
				if ((dep->deAttributes & ATTR_VOLUME) == 0 &&
				    bcmp(dosfilename, dep->deName, 11) == 0) {
#ifdef MSDOSFS_DEBUG
					printf("msdosfs_lookup(): match diroff %d, rootreloff %d\n",
					       diroff, rootreloff);
#endif
					/*
					 * Remember where this directory
					 * entry came from for whoever did
					 * this lookup. If this is the root
					 * directory we are interested in
					 * the offset relative to the
					 * beginning of the directory (not
					 * the beginning of the cluster).
					 */
					if (cluster == MSDOSFSROOT)
						diroff = rootreloff;
					dp->de_fndoffset = diroff;
					dp->de_fndclust = cluster;
					goto found;
				}
			}
			rootreloff++;
		}		/* for (diroff = 0; .... */
		/*
		 * Release the buffer holding the directory cluster just
		 * searched.
		 */
		brelse(bp);
	}			/* for (frcn = 0; ; frcn++) */
notfound:;
	/*
	 * We hold no disk buffers at this point.
	 */

	/*
	 * If we get here we didn't find the entry we were looking for. But
	 * that's ok if we are creating or renaming and are at the end of
	 * the pathname and the directory hasn't been removed.
	 */
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_lookup(): op %d, refcnt %d, slotstatus %d\n",
	       nameiop, dp->de_refcnt, slotstatus);
	printf("               slotoffset %d, slotcluster %d\n",
	       slotoffset, slotcluster);
#endif
	if ((nameiop == CREATE || nameiop == RENAME) &&
	    (flags & ISLASTCN) && dp->de_refcnt != 0) {
		error = VOP_ACCESS(vdp, VWRITE, cred, cnp->cn_proc);
		if (error)
			return error;
		if (slotstatus == NONE) {
			dp->de_fndoffset = (u_long)-1;
			dp->de_fndclust = (u_long)-1;
		} else {
#ifdef MSDOSFS_DEBUG
			printf("msdosfs_lookup(): saving empty slot location\n");
#endif
			dp->de_fndoffset = slotoffset;
			dp->de_fndclust = slotcluster;
		}
		/* dp->de_flag |= DE_UPDATE;  never update dos directories */
		cnp->cn_flags |= SAVENAME;
		if (!lockparent)/* leave searched dir locked?	 */
			VOP_UNLOCK(vdp, 0, p);
		return EJUSTRETURN;
	}
	/*
	 * Insert name in cache as non-existant if not trying to create it.
	 */
	if ((cnp->cn_flags & MAKEENTRY) && nameiop != CREATE)
		cache_enter(vdp, *vpp, cnp);
	return ENOENT;

found:	;
	/*
	 * NOTE:  We still have the buffer with matched directory entry at
	 * this point.
	 */
	isadir = dep->deAttributes & ATTR_DIRECTORY;
	scn = getushort(dep->deStartCluster);

foundroot:;
	/*
	 * If we entered at foundroot, then we are looking for the . or ..
	 * entry of the filesystems root directory.  isadir and scn were
	 * setup before jumping here.  And, bp is null.  There is no buf
	 * header.
	 */

	/*
	 * If deleting and at the end of the path, then if we matched on
	 * "." then don't deget() we would probably panic().  Otherwise
	 * deget() the directory entry.
	 */
	if (nameiop == DELETE && (flags & ISLASTCN)) {
		error = VOP_ACCESS(vdp, VWRITE, cred, cnp->cn_proc);
		if (error) {
			if (bp)
				brelse(bp);
			return error;
		}
		if (dp->de_StartCluster == scn && isadir) {	/* "." */
			VREF(vdp);
			*vpp = vdp;
			if (bp)
				brelse(bp);
			return 0;
		}
		error = deget(pmp, cluster, diroff, dep, &tdp);
		if (error) {
			if (bp)
				brelse(bp);
			return error;
		}
		*vpp = DETOV(tdp);
		if (!lockparent)
			VOP_UNLOCK(vdp, 0, p);
		if (bp)
			brelse(bp);
		return 0;
	}

	/*
	 * If renaming.
	 */
	if (nameiop == RENAME && wantparent && (flags & ISLASTCN)) {
		error = VOP_ACCESS(vdp, VWRITE, cred, cnp->cn_proc);
		if (error) {
			if (bp)
				brelse(bp);
			return error;
		}
		if (dp->de_StartCluster == scn && isadir) {
			if (bp)
				brelse(bp);
			return EISDIR;
		}
		error = deget(pmp, cluster, diroff, dep, &tdp);
		if (error) {
			if (bp)
				brelse(bp);
			return error;
		}
		*vpp = DETOV(tdp);
		cnp->cn_flags |= SAVENAME;
		if (!lockparent)
			VOP_UNLOCK(vdp, 0, p);
		if (bp)
			brelse(bp);
		return 0;
	}

	/*
	 * ?
	 */
	pdp = vdp;
	if (flags & ISDOTDOT) {
		VOP_UNLOCK(pdp, 0, p);
		error = deget(pmp, cluster, diroff, dep, &tdp);
		if (error) {
			vn_lock(pdp, LK_EXCLUSIVE | LK_RETRY, p);
			if (bp)
				brelse(bp);
			return error;
		}
		if (lockparent && (flags & ISLASTCN)
		    && (error = vn_lock(pdp, LK_EXCLUSIVE, p))) {
			vput(DETOV(tdp));
			return error;
		}
		*vpp = DETOV(tdp);
	} else if (dp->de_StartCluster == scn && isadir) {		/* "." */
		VREF(vdp);
		*vpp = vdp;
	} else {
		error = deget(pmp, cluster, diroff, dep, &tdp);
		if (error) {
			if (bp)
				brelse(bp);
			return error;
		}
		if (!lockparent || !(flags & ISLASTCN))
			VOP_UNLOCK(pdp, 0, p);
		*vpp = DETOV(tdp);
	}
	if (bp)
		brelse(bp);

	/*
	 * Insert name in cache if wanted.
	 */
	if (cnp->cn_flags & MAKEENTRY)
		cache_enter(vdp, *vpp, cnp);
	return 0;
}

/*
 * dep  - directory entry to copy into the directory
 * ddep - directory to add to
 * depp - return the address of the denode for the created directory entry
 *	  if depp != 0
 */
int
createde(dep, ddep, depp)
	struct denode *dep;
	struct denode *ddep;
	struct denode **depp;
{
	int error;
	u_long dirclust, diroffset;
	struct direntry *ndep;
	struct msdosfsmount *pmp = ddep->de_pmp;
	struct buf *bp;

#ifdef MSDOSFS_DEBUG
	printf("createde(dep %08x, ddep %08x, depp %08x)\n", dep, ddep, depp);
#endif

	/*
	 * If no space left in the directory then allocate another cluster
	 * and chain it onto the end of the file.  There is one exception
	 * to this.  That is, if the root directory has no more space it
	 * can NOT be expanded.  extendfile() checks for and fails attempts
	 * to extend the root directory.  We just return an error in that
	 * case.
	 */
	if (ddep->de_fndclust == (u_long)-1) {
		error = extendfile(ddep, 1, &bp, &dirclust, DE_CLEAR);
		if (error)
			return error;
		ndep = (struct direntry *) bp->b_data;
		/*
		 * Let caller know where we put the directory entry.
		 */
		ddep->de_fndclust = dirclust;
		ddep->de_fndoffset = diroffset = 0;
		/*
		 * Update the size of the directory
		 */
		ddep->de_FileSize += pmp->pm_bpcluster;
	} else {
		/*
		 * There is space in the existing directory.  So, we just
		 * read in the cluster with space.  Copy the new directory
		 * entry in.  Then write it to disk. NOTE:  DOS directories
		 * do not get smaller as clusters are emptied.
		 */
		dirclust = ddep->de_fndclust;
		diroffset = ddep->de_fndoffset;

		error = readep(pmp, dirclust, diroffset, &bp, &ndep);
		if (error)
			return error;
	}
	DE_EXTERNALIZE(ndep, dep);

	/*
	 * If they want us to return with the denode gotten.
	 */
	if (depp) {
		error = deget(pmp, dirclust, diroffset, ndep, depp);
		if (error)
			return error;
	}
	error = bwrite(bp);
	if (error) {
		vput(DETOV(*depp));	/* free the vnode we got on error */
		return error;
	}
	return 0;
}

/*
 * Read in a directory entry and mark it as being deleted.
 */
static int
markdeleted(pmp, dirclust, diroffset)
	struct msdosfsmount *pmp;
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
 * Remove a directory entry. At this point the file represented by the
 * directory entry to be removed is still full length until no one has it
 * open.  When the file no longer being used msdosfs_inactive() is called
 * and will truncate the file to 0 length.  When the vnode containing the
 * denode is needed for some other purpose by VFS it will call
 * msdosfs_reclaim() which will remove the denode from the denode cache.
 */
int
removede(pdep,dep)
	struct denode *pdep;	/* directory where the entry is removed */
	struct denode *dep;	/* file to be removed */
{
	struct msdosfsmount *pmp = pdep->de_pmp;
	int error;

#ifdef MSDOSFS_DEBUG
	printf("removede(): filename %s\n", dep->de_Name);
	printf("removede(): dep %08x, ndpcluster %d, ndpoffset %d\n",
	       dep, pdep->de_fndclust, pdep->de_fndoffset);
#endif

	/*
	 * Read the directory block containing the directory entry we are
	 * to make free.  The nameidata structure holds the cluster number
	 * and directory entry index number of the entry to free.
	 */
	error = markdeleted(pmp, pdep->de_fndclust, pdep->de_fndoffset);

	if (error == 0)
		dep->de_refcnt--;
	return error;
}

/*
 * Be sure a directory is empty except for "." and "..". Return 1 if empty,
 * return 0 if not empty or error.
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
	struct msdosfsmount *pmp = dep->de_pmp;
	struct direntry *dentp;

	/*
	 * Since the filesize field in directory entries for a directory is
	 * zero, we just have to feel our way through the directory until
	 * we hit end of file.
	 */
	for (cn = 0;; cn++) {
		error = pcbmap(dep, cn, &bn, 0);
		if (error == E2BIG)
			return 1;	/* it's empty */
		error = bread(pmp->pm_devvp, bn, pmp->pm_bpcluster, NOCRED,
		    &bp);
		if (error)
			return error;
		dentp = (struct direntry *) bp->b_data;
		for (dei = 0; dei < pmp->pm_depclust; dei++) {
			if (dentp->deName[0] != SLOT_DELETED) {
				/*
				 * In dos directories an entry whose name
				 * starts with SLOT_EMPTY (0) starts the
				 * beginning of the unused part of the
				 * directory, so we can just return that it
				 * is empty.
				 */
				if (dentp->deName[0] == SLOT_EMPTY) {
					brelse(bp);
					return 1;
				}
				/*
				 * Any names other than "." and ".." in a
				 * directory mean it is not empty.
				 */
				if (bcmp(dentp->deName, ".          ", 11) &&
				    bcmp(dentp->deName, "..         ", 11)) {
					brelse(bp);
#ifdef MSDOSFS_DEBUG
					printf("dosdirempty(): entry %d found %02x, %02x\n",
					       dei, dentp->deName[0], dentp->deName[1]);
#endif
					return 0;	/* not empty */
				}
			}
			dentp++;
		}
		brelse(bp);
	}
	/* NOTREACHED */
}

/*
 * Check to see if the directory described by target is in some
 * subdirectory of source.  This prevents something like the following from
 * succeeding and leaving a bunch or files and directories orphaned. mv
 * /a/b/c /a/b/c/d/e/f Where c and f are directories.
 *
 * source - the inode for /a/b/c
 * target - the inode for /a/b/c/d/e/f
 *
 * Returns 0 if target is NOT a subdirectory of source.
 * Otherwise returns a non-zero error number.
 * The target inode is always unlocked on return.
 */
int
doscheckpath(source, target)
	struct denode *source;
	struct denode *target;
{
	daddr_t scn;
	struct msdosfsmount *pmp;
	struct direntry *ep;
	struct denode *dep;
	struct buf *bp = NULL;
	int error = 0;

	dep = target;
	if ((target->de_Attributes & ATTR_DIRECTORY) == 0 ||
	    (source->de_Attributes & ATTR_DIRECTORY) == 0) {
		error = ENOTDIR;
		goto out;
	}
	if (dep->de_StartCluster == source->de_StartCluster) {
		error = EEXIST;
		goto out;
	}
	if (dep->de_StartCluster == MSDOSFSROOT)
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
		ep = (struct direntry *) bp->b_data + 1;
		if ((ep->deAttributes & ATTR_DIRECTORY) == 0 ||
		    bcmp(ep->deName, "..         ", 11) != 0) {
			error = ENOTDIR;
			break;
		}
		scn = getushort(ep->deStartCluster);
		if (scn == source->de_StartCluster) {
			error = EINVAL;
			break;
		}
		if (scn == MSDOSFSROOT)
			break;
		vput(DETOV(dep));
		/* NOTE: deget() clears dep on error */
		error = deget(pmp, scn, 0, ep, &dep);
		brelse(bp);
		bp = NULL;
		if (error)
			break;
	}
out:	;
	if (bp)
		brelse(bp);
	if (error == ENOTDIR)
		printf("doscheckpath(): .. not a directory?\n");
	if (dep != NULL)
		vput(DETOV(dep));
	return error;
}

/*
 * Read in the disk block containing the directory entry (dirclu, dirofs)
 * and return the address of the buf header, and the address of the
 * directory entry within the block.
 */
int
readep(pmp, dirclu, dirofs, bpp, epp)
	struct msdosfsmount *pmp;
	u_long dirclu, dirofs;
	struct buf **bpp;
	struct direntry **epp;
{
	int error;
	daddr_t bn;

	bn = detobn(pmp, dirclu, dirofs);
	error = bread(pmp->pm_devvp, bn, pmp->pm_bpcluster, NOCRED, bpp);
	if (error) {
		*bpp = NULL;
		return error;
	}
	if (epp)
		*epp = bptoep(pmp, *bpp, dirofs);
	return 0;
}


/*
 * Read in the disk block containing the directory entry dep came from and
 * return the address of the buf header, and the address of the directory
 * entry within the block.
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
