/*	$Id: msdosfs_denode.c,v 1.16 1996/01/19 03:58:42 dyson Exp $ */
/*	$NetBSD: msdosfs_denode.c,v 1.9 1994/08/21 18:44:00 ws Exp $	*/

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
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/types.h>
#include <sys/kernel.h>		/* defines "time" */

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_extern.h>

#include <msdosfs/bpb.h>
#include <msdosfs/msdosfsmount.h>
#include <msdosfs/direntry.h>
#include <msdosfs/denode.h>
#include <msdosfs/fat.h>

struct denode **dehashtbl;
u_long dehash;			/* size of hash table - 1 */
#define	DEHASH(dev, deno)	(((dev) + (deno)) & dehash)

union _qcvt {
	quad_t qcvt;
	long val[2];
};
#define SETHIGH(q, h) { \
	union _qcvt tmp; \
	tmp.qcvt = (q); \
	tmp.val[_QUAD_HIGHWORD] = (h); \
	(q) = tmp.qcvt; \
}
#define SETLOW(q, l) { \
	union _qcvt tmp; \
	tmp.qcvt = (q); \
	tmp.val[_QUAD_LOWWORD] = (l); \
	(q) = tmp.qcvt; \
}

static struct denode *
		msdosfs_hashget __P((dev_t dev, u_long dirclust,
				     u_long diroff));
static void	msdosfs_hashins __P((struct denode *dep));
static void	msdosfs_hashrem __P((struct denode *dep));

int msdosfs_init()
{
	dehashtbl = hashinit(desiredvnodes/2, M_MSDOSFSMNT, &dehash);
	return 0;
}

static struct denode *
msdosfs_hashget(dev, dirclust, diroff)
	dev_t dev;
	u_long dirclust;
	u_long diroff;
{
	struct denode *dep;

	for (;;)
		for (dep = dehashtbl[DEHASH(dev, dirclust + diroff)];;
		     dep = dep->de_next) {
			if (dep == NULL)
				return NULL;
			if (dirclust != dep->de_dirclust
			    || diroff != dep->de_diroffset
			    || dev != dep->de_dev
			    || dep->de_refcnt == 0)
				continue;
			if (dep->de_flag & DE_LOCKED) {
				dep->de_flag |= DE_WANTED;
				(void) tsleep((caddr_t)dep, PINOD, "msdhgt", 0);
				break;
			}
			if (!vget(DETOV(dep), 1))
				return dep;
			break;
		}
	/* NOTREACHED */
}

static void
msdosfs_hashins(dep)
	struct denode *dep;
{
	struct denode **depp, *deq;

	depp = &dehashtbl[DEHASH(dep->de_dev, dep->de_dirclust + dep->de_diroffset)];
	deq = *depp;
	if (deq)
		deq->de_prev = &dep->de_next;
	dep->de_next = deq;
	dep->de_prev = depp;
	*depp = dep;
}

static void
msdosfs_hashrem(dep)
	struct denode *dep;
{
	struct denode *deq;
	deq = dep->de_next;
	if (deq)
		deq->de_prev = dep->de_prev;
	*dep->de_prev = deq;
#ifdef DIAGNOSTIC
	dep->de_next = NULL;
	dep->de_prev = NULL;
#endif
}

/*
 * If deget() succeeds it returns with the gotten denode locked().
 *
 * pmp	     - address of msdosfsmount structure of the filesystem containing
 *	       the denode of interest.  The pm_dev field and the address of
 *	       the msdosfsmount structure are used.
 * dirclust  - which cluster bp contains, if dirclust is 0 (root directory)
 *	       diroffset is relative to the beginning of the root directory,
 *	       otherwise it is cluster relative.
 * diroffset - offset past begin of cluster of denode we want
 * direntptr - address of the direntry structure of interest. If direntptr is
 *	       NULL, the block is read if necessary.
 * depp	     - returns the address of the gotten denode.
 */
int
deget(pmp, dirclust, diroffset, direntptr, depp)
	struct msdosfsmount *pmp;	/* so we know the maj/min number */
	u_long dirclust;		/* cluster this dir entry came from */
	u_long diroffset;		/* index of entry within the cluster */
	struct direntry *direntptr;
	struct denode **depp;		/* returns the addr of the gotten denode */
{
	int error;
	dev_t dev = pmp->pm_dev;
	struct mount *mntp = pmp->pm_mountp;
	struct denode *ldep;
	struct vnode *nvp;
	struct buf *bp;

#ifdef MSDOSFS_DEBUG
	printf("deget(pmp %p, dirclust %ld, diroffset %x, direntptr %p, depp %p)\n",
	       pmp, dirclust, diroffset, direntptr, depp);
#endif

	/*
	 * If dir entry is given and refers to a directory, convert to
	 * canonical form
	 */
	if (direntptr && (direntptr->deAttributes & ATTR_DIRECTORY)) {
		dirclust = getushort(direntptr->deStartCluster);
		if (dirclust == MSDOSFSROOT)
			diroffset = MSDOSFSROOT_OFS;
		else
			diroffset = 0;
	}

	/*
	 * See if the denode is in the denode cache. Use the location of
	 * the directory entry to compute the hash value. For subdir use
	 * address of "." entry. for root dir use cluster MSDOSFSROOT,
	 * offset MSDOSFSROOT_OFS
	 *
	 * NOTE: The check for de_refcnt > 0 below insures the denode being
	 * examined does not represent an unlinked but still open file.
	 * These files are not to be accessible even when the directory
	 * entry that represented the file happens to be reused while the
	 * deleted file is still open.
	 */
	ldep = msdosfs_hashget(dev, dirclust, diroffset);
	if (ldep) {
		*depp = ldep;
		return 0;
	}

	/*
	 * Do the MALLOC before the getnewvnode since doing so afterward
	 * might cause a bogus v_data pointer to get dereferenced
	 * elsewhere if MALLOC should block.
	 */
	MALLOC(ldep, struct denode *, sizeof(struct denode), M_MSDOSFSNODE, M_WAITOK);

	/*
	 * Directory entry was not in cache, have to create a vnode and
	 * copy it from the passed disk buffer.
	 */
	/* getnewvnode() does a VREF() on the vnode */
	error = getnewvnode(VT_MSDOSFS, mntp, msdosfs_vnodeop_p, &nvp);
	if (error) {
		*depp = NULL;
		FREE(ldep, M_MSDOSFSNODE);
		return error;
	}
	bzero((caddr_t)ldep, sizeof *ldep);
	nvp->v_data = ldep;
	ldep->de_vnode = nvp;
	ldep->de_flag = 0;
	ldep->de_devvp = 0;
	ldep->de_lockf = 0;
	ldep->de_dev = dev;
	ldep->de_dirclust = dirclust;
	ldep->de_diroffset = diroffset;
	fc_purge(ldep, 0);	/* init the fat cache for this denode */

	/*
	 * Insert the denode into the hash queue and lock the denode so it
	 * can't be accessed until we've read it in and have done what we
	 * need to it.
	 */
	VOP_LOCK(nvp);
	msdosfs_hashins(ldep);

	/*
	 * Copy the directory entry into the denode area of the vnode.
	 */
	if (dirclust == MSDOSFSROOT && diroffset == MSDOSFSROOT_OFS) {
		/*
		 * Directory entry for the root directory. There isn't one,
		 * so we manufacture one. We should probably rummage
		 * through the root directory and find a label entry (if it
		 * exists), and then use the time and date from that entry
		 * as the time and date for the root denode.
		 */
		ldep->de_Attributes = ATTR_DIRECTORY;
		ldep->de_StartCluster = MSDOSFSROOT;
		ldep->de_FileSize = pmp->pm_rootdirsize * pmp->pm_BytesPerSec;
		/*
		 * fill in time and date so that dos2unixtime() doesn't
		 * spit up when called from msdosfs_getattr() with root
		 * denode
		 */
		ldep->de_Time = 0x0000;	/* 00:00:00	 */
		ldep->de_Date = (0 << DD_YEAR_SHIFT) | (1 << DD_MONTH_SHIFT)
		    | (1 << DD_DAY_SHIFT);
		/* Jan 1, 1980	 */
		/* leave the other fields as garbage */
	} else {
		bp = NULL;
		if (!direntptr) {
			error = readep(pmp, dirclust, diroffset, &bp,
				       &direntptr);
			if (error)
				return error;
		}
		DE_INTERNALIZE(ldep, direntptr);
		if (bp)
			brelse(bp);
	}

	/*
	 * Fill in a few fields of the vnode and finish filling in the
	 * denode.  Then return the address of the found denode.
	 */
	ldep->de_pmp = pmp;
	ldep->de_devvp = pmp->pm_devvp;
	ldep->de_refcnt = 1;
	if (ldep->de_Attributes & ATTR_DIRECTORY) {
		/*
		 * Since DOS directory entries that describe directories
		 * have 0 in the filesize field, we take this opportunity
		 * to find out the length of the directory and plug it into
		 * the denode structure.
		 */
		u_long size;

		nvp->v_type = VDIR;
		if (ldep->de_StartCluster == MSDOSFSROOT)
			nvp->v_flag |= VROOT;
		else {
			error = pcbmap(ldep, 0xffff, 0, &size);
			if (error == E2BIG) {
				ldep->de_FileSize = size << pmp->pm_cnshift;
				error = 0;
			} else
				printf("deget(): pcbmap returned %d\n", error);
		}
	} else
		nvp->v_type = VREG;
	SETHIGH(ldep->de_modrev, mono_time.tv_sec);
	SETLOW(ldep->de_modrev, mono_time.tv_usec * 4294);
	VREF(ldep->de_devvp);
	*depp = ldep;
	return 0;
}

int
deupdat(dep, tp, waitfor)
	struct denode *dep;
	struct timespec *tp;
	int waitfor;
{
	int error;
	struct buf *bp;
	struct direntry *dirp;
	struct vnode *vp = DETOV(dep);

#ifdef MSDOSFS_DEBUG
	printf("deupdat(): dep %p\n", dep);
#endif

	/*
	 * If the denode-modified and update-mtime bits are off,
	 * or this denode is from a readonly filesystem,
	 * or this denode is for a directory,
	 * or the denode represents an open but unlinked file,
	 * then don't do anything.  DOS directory
	 * entries that describe a directory do not ever get
	 * updated.  This is the way DOS treats them.
	 */
	if ((dep->de_flag & (DE_MODIFIED | DE_UPDATE)) == 0 ||
	    vp->v_mount->mnt_flag & MNT_RDONLY ||
	    dep->de_Attributes & ATTR_DIRECTORY ||
	    dep->de_refcnt <= 0)
		return 0;

	/*
	 * Read in the cluster containing the directory entry we want to
	 * update.
	 */
	error = readde(dep, &bp, &dirp);
	if (error)
		return error;

	/*
	 * If the mtime is to be updated, put the passed in time into the
	 * directory entry.
	 */
	if (dep->de_flag & DE_UPDATE) {
		dep->de_Attributes |= ATTR_ARCHIVE;
		unix2dostime(tp, &dep->de_Date, &dep->de_Time);
	}

	/*
	 * The mtime is now up to date.  The denode will be unmodifed soon.
	 */
	dep->de_flag &= ~(DE_MODIFIED | DE_UPDATE);

	/*
	 * Copy the directory entry out of the denode into the cluster it
	 * came from.
	 */
	DE_EXTERNALIZE(dirp, dep);

	/*
	 * Write the cluster back to disk.  If they asked for us to wait
	 * for the write to complete, then use bwrite() otherwise use
	 * bdwrite().
	 */
	error = 0;		/* note that error is 0 from above, but ... */
	if (waitfor)
		error = bwrite(bp);
	else
		bdwrite(bp);
	return error;
}

/*
 * Truncate the file described by dep to the length specified by length.
 */
int
detrunc(dep, length, flags, cred, p)
	struct denode *dep;
	u_long length;
	int flags;
	struct ucred *cred;
	struct proc *p;
{
	int error;
	int allerror;
	int vflags;
	u_long eofentry;
	u_long chaintofree;
	daddr_t bn;
	int boff;
	int isadir = dep->de_Attributes & ATTR_DIRECTORY;
	struct buf *bp;
	struct msdosfsmount *pmp = dep->de_pmp;
	struct timespec ts;

#ifdef MSDOSFS_DEBUG
	printf("detrunc(): file %s, length %d, flags %d\n", dep->de_Name, length, flags);
#endif

	/*
	 * Disallow attempts to truncate the root directory since it is of
	 * fixed size.  That's just the way dos filesystems are.  We use
	 * the VROOT bit in the vnode because checking for the directory
	 * bit and a startcluster of 0 in the denode is not adequate to
	 * recognize the root directory at this point in a file or
	 * directory's life.
	 */
	if (DETOV(dep)->v_flag & VROOT) {
		printf(
    "detrunc(): can't truncate root directory, clust %ld, offset %ld\n",
		    dep->de_dirclust, dep->de_diroffset);
		return EINVAL;
	}


	if (dep->de_FileSize < length) {
		vnode_pager_setsize(DETOV(dep), length);
		return deextend(dep, length, cred);
	}

	/*
	 * If the desired length is 0 then remember the starting cluster of
	 * the file and set the StartCluster field in the directory entry
	 * to 0.  If the desired length is not zero, then get the number of
	 * the last cluster in the shortened file.  Then get the number of
	 * the first cluster in the part of the file that is to be freed.
	 * Then set the next cluster pointer in the last cluster of the
	 * file to CLUST_EOFE.
	 */
	if (length == 0) {
		chaintofree = dep->de_StartCluster;
		dep->de_StartCluster = 0;
		eofentry = ~0;
	} else {
		error = pcbmap(dep, de_clcount(pmp, length) - 1, 0, &eofentry);
		if (error) {
#ifdef MSDOSFS_DEBUG
			printf("detrunc(): pcbmap fails %d\n", error);
#endif
			return error;
		}
	}

	fc_purge(dep, (length + pmp->pm_crbomask) >> pmp->pm_cnshift);

	/*
	 * If the new length is not a multiple of the cluster size then we
	 * must zero the tail end of the new last cluster in case it
	 * becomes part of the file again because of a seek.
	 */
	if ((boff = length & pmp->pm_crbomask) != 0) {
		/*
		 * should read from file vnode or filesystem vnode
		 * depending on if file or dir
		 */
		if (isadir) {
			bn = cntobn(pmp, eofentry);
			error = bread(pmp->pm_devvp, bn, pmp->pm_bpcluster,
			    NOCRED, &bp);
		} else {
			bn = de_blk(pmp, length);
			error = bread(DETOV(dep), bn, pmp->pm_bpcluster,
			    NOCRED, &bp);
		}
		if (error) {
#ifdef MSDOSFS_DEBUG
			printf("detrunc(): bread fails %d\n", error);
#endif
			return error;
		}
		/*
		 * is this the right place for it?
		 */
		bzero(bp->b_data + boff, pmp->pm_bpcluster - boff);
		if (flags & IO_SYNC)
			bwrite(bp);
		else
			bdwrite(bp);
	}

	/*
	 * Write out the updated directory entry.  Even if the update fails
	 * we free the trailing clusters.
	 */
	dep->de_FileSize = length;
	dep->de_flag |= DE_UPDATE;
	vflags = (length > 0 ? V_SAVE : 0) | V_SAVEMETA;
	vinvalbuf(DETOV(dep), vflags, cred, p, 0, 0);
	vnode_pager_setsize(DETOV(dep), length);
	TIMEVAL_TO_TIMESPEC(&time, &ts);
	allerror = deupdat(dep, &ts, 1);
#ifdef MSDOSFS_DEBUG
	printf("detrunc(): allerror %d, eofentry %d\n",
	       allerror, eofentry);
#endif

	/*
	 * If we need to break the cluster chain for the file then do it
	 * now.
	 */
	if (eofentry != ~0) {
		error = fatentry(FAT_GET_AND_SET, pmp, eofentry,
				 &chaintofree, CLUST_EOFE);
		if (error) {
#ifdef MSDOSFS_DEBUG
			printf("detrunc(): fatentry errors %d\n", error);
#endif
			return error;
		}
		fc_setcache(dep, FC_LASTFC, (length - 1) >> pmp->pm_cnshift,
			    eofentry);
	}

	/*
	 * Now free the clusters removed from the file because of the
	 * truncation.
	 */
	if (chaintofree != 0 && !MSDOSFSEOF(chaintofree))
		freeclusterchain(pmp, chaintofree);

	return allerror;
}

/*
 * Extend the file described by dep to length specified by length.
 */
int
deextend(dep, length, cred)
	struct denode *dep;
	off_t length;
	struct ucred *cred;
{
	struct msdosfsmount *pmp = dep->de_pmp;
	u_long count;
	int error;
	struct timespec ts;

	/*
	 * The root of a DOS filesystem cannot be extended.
	 */
	if (DETOV(dep)->v_flag & VROOT)
		return EINVAL;

	/*
	 * Directories can only be extended by the superuser.
	 * Is this really important?
	 */
	if (dep->de_Attributes & ATTR_DIRECTORY) {
		error = suser(cred, NULL);
		if (error)
			return error;
	}

	if (length <= dep->de_FileSize)
		panic("deextend: file too large");

	/*
	 * Compute the number of clusters to allocate.
	 */
	count = de_clcount(pmp, length) - de_clcount(pmp, dep->de_FileSize);
	if (count > 0) {
		if (count > pmp->pm_freeclustercount)
			return ENOSPC;
		error = extendfile(dep, count, NULL, NULL, DE_CLEAR);
		if (error) {
			/* truncate the added clusters away again */
			(void) detrunc(dep, dep->de_FileSize, 0, cred, NULL);
			return error;
		}
	}

	dep->de_flag |= DE_UPDATE;
	dep->de_FileSize = length;
	TIMEVAL_TO_TIMESPEC(&time, &ts);
	return deupdat(dep, &ts, 1);
}

/*
 * Move a denode to its correct hash queue after the file it represents has
 * been moved to a new directory.
 */
int reinsert(dep)
	struct denode *dep;
{
	/*
	 * Fix up the denode cache.  If the denode is for a directory,
	 * there is nothing to do since the hash is based on the starting
	 * cluster of the directory file and that hasn't changed.  If for a
	 * file the hash is based on the location of the directory entry,
	 * so we must remove it from the cache and re-enter it with the
	 * hash based on the new location of the directory entry.
	 */
	if ((dep->de_Attributes & ATTR_DIRECTORY) == 0) {
		msdosfs_hashrem(dep);
		msdosfs_hashins(dep);
	}
	return 0;
}

int
msdosfs_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct denode *dep = VTODE(vp);

#ifdef MSDOSFS_DEBUG
	printf("msdosfs_reclaim(): dep %p, file %s, refcnt %ld\n",
	    dep, dep->de_Name, dep->de_refcnt);
#endif

	if (prtactive && vp->v_usecount != 0)
		vprint("msdosfs_reclaim(): pushing active", vp);

	/*
	 * Remove the denode from the denode hash chain we are in.
	 */
	msdosfs_hashrem(dep);

	cache_purge(vp);
	/*
	 * Indicate that one less file on the filesystem is open.
	 */
	if (dep->de_devvp) {
		vrele(dep->de_devvp);
		dep->de_devvp = 0;
	}

	dep->de_flag = 0;

	FREE(dep, M_MSDOSFSNODE);
	vp->v_data = NULL;

	return 0;
}

int
msdosfs_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct denode *dep = VTODE(vp);
	int error = 0;
	struct timespec ts;

#ifdef MSDOSFS_DEBUG
	printf("msdosfs_inactive(): dep %p, de_Name[0] %x\n", dep, dep->de_Name[0]);
#endif

	if (prtactive && vp->v_usecount != 0)
		vprint("msdosfs_inactive(): pushing active", vp);

	/*
	 * Get rid of denodes related to stale file handles. Hmmm, what
	 * does this really do?
	 */
	if (dep->de_Name[0] == SLOT_DELETED) {
		if ((vp->v_flag & VXLOCK) == 0)
			vgone(vp);
		return 0;
	}

	/*
	 * If the file has been deleted and it is on a read/write
	 * filesystem, then truncate the file, and mark the directory slot
	 * as empty.  (This may not be necessary for the dos filesystem.)
	 */
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_inactive(): dep %p, refcnt %ld, mntflag %x, MNT_RDONLY %x\n",
	       dep, dep->de_refcnt, vp->v_mount->mnt_flag, MNT_RDONLY);
#endif
	VOP_LOCK(vp);
	if (dep->de_refcnt <= 0 && (vp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
		error = detrunc(dep, (u_long) 0, 0, NOCRED, NULL);
		dep->de_flag |= DE_UPDATE;
		dep->de_Name[0] = SLOT_DELETED;
	}
	if (dep->de_flag & (DE_MODIFIED | DE_UPDATE)) {
		TIMEVAL_TO_TIMESPEC(&time, &ts);
		deupdat(dep, &ts, 0);
	}
	VOP_UNLOCK(vp);
	dep->de_flag = 0;

	/*
	 * If we are done with the denode, then reclaim it so that it can
	 * be reused now.
	 */
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_inactive(): v_usecount %d, de_Name[0] %x\n", vp->v_usecount,
	       dep->de_Name[0]);
#endif
	if (vp->v_usecount == 0 && dep->de_Name[0] == SLOT_DELETED)
		vgone(vp);
	return error;
}
