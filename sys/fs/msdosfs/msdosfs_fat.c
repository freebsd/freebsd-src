/*	$Id: msdosfs_fat.c,v 1.8 1995/10/29 15:31:49 phk Exp $ */
/*	$NetBSD: msdosfs_fat.c,v 1.12 1994/08/21 18:44:04 ws Exp $	*/

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

/*
 * kernel include files.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/namei.h>
#include <sys/mount.h>		/* to define statfs structure */
#include <sys/vnode.h>		/* to define vattr structure */
#include <sys/errno.h>

/*
 * msdosfs include files.
 */
#include <msdosfs/bpb.h>
#include <msdosfs/msdosfsmount.h>
#include <msdosfs/direntry.h>
#include <msdosfs/denode.h>
#include <msdosfs/fat.h>

static void fc_lookup __P((struct denode *dep, u_long findcn, u_long *frcnp, u_long *fsrcnp));
/*
 * Fat cache stats.
 */
int fc_fileextends;		/* # of file extends			 */
int fc_lfcempty;		/* # of time last file cluster cache entry
				 * was empty */
int fc_bmapcalls;		/* # of times pcbmap was called		 */

#define	LMMAX	20
int fc_lmdistance[LMMAX];	/* counters for how far off the last
				 * cluster mapped entry was. */
int fc_largedistance;		/* off by more than LMMAX		 */

/* Byte offset in FAT on filesystem pmp, cluster cn */
#define	FATOFS(pmp, cn)	(FAT12(pmp) ? (cn) * 3 / 2 : (cn) * 2)

static void
fatblock(pmp, ofs, bnp, sizep, bop)
	struct msdosfsmount *pmp;
	u_long ofs;
	u_long *bnp;
	u_long *sizep;
	u_long *bop;
{
	u_long bn, size;

	bn = ofs / pmp->pm_fatblocksize * pmp->pm_fatblocksec;
	size = min(pmp->pm_fatblocksec, pmp->pm_FATsecs - bn)
	    * pmp->pm_BytesPerSec;
	bn += pmp->pm_fatblk;
	if (bnp)
		*bnp = bn;
	if (sizep)
		*sizep = size;
	if (bop)
		*bop = ofs % pmp->pm_fatblocksize;
}

/*
 * Map the logical cluster number of a file into a physical disk sector
 * that is filesystem relative.
 *
 * dep	  - address of denode representing the file of interest
 * findcn - file relative cluster whose filesystem relative cluster number
 *	    and/or block number are/is to be found
 * bnp	  - address of where to place the file system relative block number.
 *	    If this pointer is null then don't return this quantity.
 * cnp	  - address of where to place the file system relative cluster number.
 *	    If this pointer is null then don't return this quantity.
 *
 * NOTE: Either bnp or cnp must be non-null.
 * This function has one side effect.  If the requested file relative cluster
 * is beyond the end of file, then the actual number of clusters in the file
 * is returned in *cnp.  This is useful for determining how long a directory is.
 *  If cnp is null, nothing is returned.
 */
int
pcbmap(dep, findcn, bnp, cnp)
	struct denode *dep;
	u_long findcn;		/* file relative cluster to get		 */
	daddr_t *bnp;		/* returned filesys relative blk number	 */
	u_long *cnp;		/* returned cluster number		 */
{
	int error;
	u_long i;
	u_long cn;
	u_long prevcn;
	u_long byteoffset;
	u_long bn;
	u_long bo;
	struct buf *bp = NULL;
	u_long bp_bn = -1;
	struct msdosfsmount *pmp = dep->de_pmp;
	u_long bsize;
	int fat12 = FAT12(pmp);	/* 12 bit fat	 */

	fc_bmapcalls++;

	/*
	 * If they don't give us someplace to return a value then don't
	 * bother doing anything.
	 */
	if (bnp == NULL && cnp == NULL)
		return 0;

	cn = dep->de_StartCluster;
	/*
	 * The "file" that makes up the root directory is contiguous,
	 * permanently allocated, of fixed size, and is not made up of
	 * clusters.  If the cluster number is beyond the end of the root
	 * directory, then return the number of clusters in the file.
	 */
	if (cn == MSDOSFSROOT) {
		if (dep->de_Attributes & ATTR_DIRECTORY) {
			if (findcn * pmp->pm_SectPerClust >= pmp->pm_rootdirsize) {
				if (cnp)
					*cnp = pmp->pm_rootdirsize / pmp->pm_SectPerClust;
				return E2BIG;
			}
			if (bnp)
				*bnp = pmp->pm_rootdirblk + (findcn * pmp->pm_SectPerClust);
			if (cnp)
				*cnp = MSDOSFSROOT;
			return 0;
		} else {		/* just an empty file */
			if (cnp)
				*cnp = 0;
			return E2BIG;
		}
	}

	/*
	 * Rummage around in the fat cache, maybe we can avoid tromping
	 * thru every fat entry for the file. And, keep track of how far
	 * off the cache was from where we wanted to be.
	 */
	i = 0;
	fc_lookup(dep, findcn, &i, &cn);
	if ((bn = findcn - i) >= LMMAX)
		fc_largedistance++;
	else
		fc_lmdistance[bn]++;

	/*
	 * Handle all other files or directories the normal way.
	 */
	prevcn = 0;
	for (; i < findcn; i++) {
		if (MSDOSFSEOF(cn))
			goto hiteof;
		byteoffset = FATOFS(pmp, cn);
		fatblock(pmp, byteoffset, &bn, &bsize, &bo);
		if (bn != bp_bn) {
			if (bp)
				brelse(bp);
			error = bread(pmp->pm_devvp, bn, bsize, NOCRED, &bp);
			if (error)
				return error;
			bp_bn = bn;
		}
		prevcn = cn;
		cn = getushort(&bp->b_data[bo]);
		if (fat12) {
			if (prevcn & 1)
				cn >>= 4;
			cn &= 0x0fff;
			/*
			 * Force the special cluster numbers in the range
			 * 0x0ff0-0x0fff to be the same as for 16 bit
			 * cluster numbers to let the rest of msdosfs think
			 * it is always dealing with 16 bit fats.
			 */
			if ((cn & 0x0ff0) == 0x0ff0)
				cn |= 0xf000;
		}
	}

	if (!MSDOSFSEOF(cn)) {
		if (bp)
			brelse(bp);
		if (bnp)
			*bnp = cntobn(pmp, cn);
		if (cnp)
			*cnp = cn;
		fc_setcache(dep, FC_LASTMAP, i, cn);
		return 0;
	}

hiteof:;
	if (cnp)
		*cnp = i;
	if (bp)
		brelse(bp);
	/* update last file cluster entry in the fat cache */
	fc_setcache(dep, FC_LASTFC, i - 1, prevcn);
	return E2BIG;
}

/*
 * Find the closest entry in the fat cache to the cluster we are looking
 * for.
 */
static void
fc_lookup(dep, findcn, frcnp, fsrcnp)
	struct denode *dep;
	u_long findcn;
	u_long *frcnp;
	u_long *fsrcnp;
{
	int i;
	u_long cn;
	struct fatcache *closest = 0;

	for (i = 0; i < FC_SIZE; i++) {
		cn = dep->de_fc[i].fc_frcn;
		if (cn != FCE_EMPTY && cn <= findcn) {
			if (closest == 0 || cn > closest->fc_frcn)
				closest = &dep->de_fc[i];
		}
	}
	if (closest) {
		*frcnp = closest->fc_frcn;
		*fsrcnp = closest->fc_fsrcn;
	}
}

/*
 * Purge the fat cache in denode dep of all entries relating to file
 * relative cluster frcn and beyond.
 */
void fc_purge(dep, frcn)
	struct denode *dep;
	u_int frcn;
{
	int i;
	struct fatcache *fcp;

	fcp = dep->de_fc;
	for (i = 0; i < FC_SIZE; i++, fcp++) {
		if (fcp->fc_frcn >= frcn)
			fcp->fc_frcn = FCE_EMPTY;
	}
}

/*
 * Update all copies of the fat. The first copy is updated last.
 *
 * pmp	 - msdosfsmount structure for filesystem to update
 * bp	 - addr of modified fat block
 * fatbn - block number relative to begin of filesystem of the modified fat block.
 */
static void
updatefats(pmp, bp, fatbn)
	struct msdosfsmount *pmp;
	struct buf *bp;
	u_long fatbn;
{
	int i;
	struct buf *bpn;

#ifdef MSDOSFS_DEBUG
	printf("updatefats(pmp %p, bp %p, fatbn %ld)\n", pmp, bp, fatbn);
#endif

	/*
	 * Now copy the block(s) of the modified fat to the other copies of
	 * the fat and write them out.  This is faster than reading in the
	 * other fats and then writing them back out.  This could tie up
	 * the fat for quite a while. Preventing others from accessing it.
	 * To prevent us from going after the fat quite so much we use
	 * delayed writes, unless they specfied "synchronous" when the
	 * filesystem was mounted.  If synch is asked for then use
	 * bwrite()'s and really slow things down.
	 */
	for (i = 1; i < pmp->pm_FATs; i++) {
		fatbn += pmp->pm_FATsecs;
		/* getblk() never fails */
		bpn = getblk(pmp->pm_devvp, fatbn, bp->b_bcount, 0, 0);
		bcopy(bp->b_data, bpn->b_data, bp->b_bcount);
		if (pmp->pm_waitonfat)
			bwrite(bpn);
		else
			bdwrite(bpn);
	}
	/*
	 * Write out the first fat last.
	 */
	if (pmp->pm_waitonfat)
		bwrite(bp);
	else
		bdwrite(bp);
}

/*
 * Updating entries in 12 bit fats is a pain in the butt.
 *
 * The following picture shows where nibbles go when moving from a 12 bit
 * cluster number into the appropriate bytes in the FAT.
 *
 *	byte m        byte m+1      byte m+2
 *	+----+----+   +----+----+   +----+----+
 *	|  0    1 |   |  2    3 |   |  4    5 |   FAT bytes
 *	+----+----+   +----+----+   +----+----+
 *
 *	+----+----+----+   +----+----+----+
 *	|  3    0    1 |   |  4    5    2 |
 *	+----+----+----+   +----+----+----+
 *	cluster n  	   cluster n+1
 *
 * Where n is even. m = n + (n >> 2)
 *
 */
static inline void
usemap_alloc(pmp, cn)
	struct msdosfsmount *pmp;
	u_long cn;
{
	pmp->pm_inusemap[cn / N_INUSEBITS]
			 |= 1 << (cn % N_INUSEBITS);
	pmp->pm_freeclustercount--;
}

static inline void
usemap_free(pmp, cn)
	struct msdosfsmount *pmp;
	u_long cn;
{
	pmp->pm_freeclustercount++;
	pmp->pm_inusemap[cn / N_INUSEBITS] &= ~(1 << (cn % N_INUSEBITS));
}

int
clusterfree(pmp, cluster, oldcnp)
	struct msdosfsmount *pmp;
	u_long cluster;
	u_long *oldcnp;
{
	int error;
	u_long oldcn;

	error = fatentry(FAT_GET_AND_SET, pmp, cluster, &oldcn, MSDOSFSFREE);
	if (error == 0) {
		/*
		 * If the cluster was successfully marked free, then update
		 * the count of free clusters, and turn off the "allocated"
		 * bit in the "in use" cluster bit map.
		 */
		usemap_free(pmp, cluster);
		if (oldcnp)
			*oldcnp = oldcn;
	}
	return error;
}

/*
 * Get or Set or 'Get and Set' the cluster'th entry in the fat.
 *
 * function	- whether to get or set a fat entry
 * pmp		- address of the msdosfsmount structure for the filesystem
 *		  whose fat is to be manipulated.
 * cn		- which cluster is of interest
 * oldcontents	- address of a word that is to receive the contents of the
 *		  cluster'th entry if this is a get function
 * newcontents	- the new value to be written into the cluster'th element of
 *		  the fat if this is a set function.
 *
 * This function can also be used to free a cluster by setting the fat entry
 * for a cluster to 0.
 *
 * All copies of the fat are updated if this is a set function. NOTE: If
 * fatentry() marks a cluster as free it does not update the inusemap in
 * the msdosfsmount structure. This is left to the caller.
 */
int
fatentry(function, pmp, cn, oldcontents, newcontents)
	int function;
	struct msdosfsmount *pmp;
	u_long cn;
	u_long *oldcontents;
	u_long newcontents;
{
	int error;
	u_long readcn;
	u_long bn, bo, bsize, byteoffset;
	struct buf *bp;

	/*
	 * printf("fatentry(func %d, pmp %08x, clust %d, oldcon %08x, newcon %d)\n",
	 *	  function, pmp, cluster, oldcontents, newcontents);
	 */

#ifdef DIAGNOSTIC
	/*
	 * Be sure they asked us to do something.
	 */
	if ((function & (FAT_SET | FAT_GET)) == 0) {
		printf("fatentry(): function code doesn't specify get or set\n");
		return EINVAL;
	}

	/*
	 * If they asked us to return a cluster number but didn't tell us
	 * where to put it, give them an error.
	 */
	if ((function & FAT_GET) && oldcontents == NULL) {
		printf("fatentry(): get function with no place to put result\n");
		return EINVAL;
	}
#endif

	/*
	 * Be sure the requested cluster is in the filesystem.
	 */
	if (cn < CLUST_FIRST || cn > pmp->pm_maxcluster)
		return EINVAL;

	byteoffset = FATOFS(pmp, cn);
	fatblock(pmp, byteoffset, &bn, &bsize, &bo);
	error = bread(pmp->pm_devvp, bn, bsize, NOCRED, &bp);
	if (error)
		return error;

	if (function & FAT_GET) {
		readcn = getushort(&bp->b_data[bo]);
		if (FAT12(pmp)) {
			if (cn & 1)
				readcn >>= 4;
			readcn &= 0x0fff;
			/* map certain 12 bit fat entries to 16 bit */
			if ((readcn & 0x0ff0) == 0x0ff0)
				readcn |= 0xf000;
		}
		*oldcontents = readcn;
	}
	if (function & FAT_SET) {
		if (FAT12(pmp)) {
			readcn = getushort(&bp->b_data[bo]);
			if (cn & 1) {
				readcn &= 0x000f;
				readcn |= newcontents << 4;
			} else {
				readcn &= 0xf000;
				readcn |= newcontents & 0xfff;
			}
			putushort(&bp->b_data[bo], readcn);
		} else
			putushort(&bp->b_data[bo], newcontents);
		updatefats(pmp, bp, bn);
		bp = NULL;
		pmp->pm_fmod = 1;
	}
	if (bp)
		brelse(bp);
	return 0;
}

/*
 * Update a contiguous cluster chain
 *
 * pmp	    - mount point
 * start    - first cluster of chain
 * count    - number of clusters in chain
 * fillwith - what to write into fat entry of last cluster
 */
static int
fatchain(pmp, start, count, fillwith)
	struct msdosfsmount *pmp;
	u_long start;
	u_long count;
	u_long fillwith;
{
	int error;
	u_long bn, bo, bsize, byteoffset, readcn, newc;
	struct buf *bp;

#ifdef MSDOSFS_DEBUG
	printf("fatchain(pmp %p, start %ld, count %ld, fillwith %ld)\n",
	       pmp, start, count, fillwith);
#endif
	/*
	 * Be sure the clusters are in the filesystem.
	 */
	if (start < CLUST_FIRST || start + count - 1 > pmp->pm_maxcluster)
		return EINVAL;

	while (count > 0) {
		byteoffset = FATOFS(pmp, start);
		fatblock(pmp, byteoffset, &bn, &bsize, &bo);
		error = bread(pmp->pm_devvp, bn, bsize, NOCRED, &bp);
		if (error)
			return error;
		while (count > 0) {
			start++;
			newc = --count > 0 ? start : fillwith;
			if (FAT12(pmp)) {
				readcn = getushort(&bp->b_data[bo]);
				if (start & 1) {
					readcn &= 0xf000;
					readcn |= newc & 0xfff;
				} else {
					readcn &= 0x000f;
					readcn |= newc << 4;
				}
				putushort(&bp->b_data[bo], readcn);
				bo++;
				if (!(start & 1))
					bo++;
			} else {
				putushort(&bp->b_data[bo], newc);
				bo += 2;
			}
			if (bo >= bsize)
				break;
		}
		updatefats(pmp, bp, bn);
	}
	pmp->pm_fmod = 1;
	return 0;
}

/*
 * Check the length of a free cluster chain starting at start.
 *
 * pmp	 - mount point
 * start - start of chain
 * count - maximum interesting length
 */
static int
chainlength(pmp, start, count)
	struct msdosfsmount *pmp;
	u_long start;
	u_long count;
{
	u_long idx, max_idx;
	u_int map;
	u_long len;

	max_idx = pmp->pm_maxcluster / N_INUSEBITS;
	idx = start / N_INUSEBITS;
	start %= N_INUSEBITS;
	map = pmp->pm_inusemap[idx];
	map &= ~((1 << start) - 1);
	if (map) {
		len = ffs(map) - 1 - start;
		return len > count ? count : len;
	}
	len = N_INUSEBITS - start;
	if (len >= count)
		return count;
	while (++idx <= max_idx) {
		if (len >= count)
			break;
		map = pmp->pm_inusemap[idx];
		if (map) {
			len +=  ffs(map) - 1;
			break;
		}
		len += N_INUSEBITS;
	}
	return len > count ? count : len;
}

/*
 * Allocate contigous free clusters.
 *
 * pmp	      - mount point.
 * start      - start of cluster chain.
 * count      - number of clusters to allocate.
 * fillwith   - put this value into the fat entry for the
 *		last allocated cluster.
 * retcluster - put the first allocated cluster's number here.
 * got	      - how many clusters were actually allocated.
 */
static int
chainalloc(pmp, start, count, fillwith, retcluster, got)
	struct msdosfsmount *pmp;
	u_long start;
	u_long count;
	u_long fillwith;
	u_long *retcluster;
	u_long *got;
{
	int error;

	error = fatchain(pmp, start, count, fillwith);
	if (error == 0) {
#ifdef MSDOSFS_DEBUG
		printf("clusteralloc(): allocated cluster chain at %ld (%ld clusters)\n",
		       start, count);
#endif
		if (retcluster)
			*retcluster = start;
		if (got)
			*got = count;
		while (count-- > 0)
			usemap_alloc(pmp, start++);
	}
	return error;
}

/*
 * Allocate contiguous free clusters.
 *
 * pmp	      - mount point.
 * start      - preferred start of cluster chain.
 * count      - number of clusters requested.
 * fillwith   - put this value into the fat entry for the
 *		last allocated cluster.
 * retcluster - put the first allocated cluster's number here.
 * got	      - how many clusters were actually allocated.
 */
int
clusteralloc(pmp, start, count, fillwith, retcluster, got)
	struct msdosfsmount *pmp;
	u_long start;
	u_long count;
	u_long fillwith;
	u_long *retcluster;
	u_long *got;
{
	u_long idx;
	u_long len, newst, foundcn, foundl, cn, l;
	u_int map;

#ifdef MSDOSFS_DEBUG
	printf("clusteralloc(): find %d clusters\n",count);
#endif
	if (start) {
		if ((len = chainlength(pmp, start, count)) >= count)
			return chainalloc(pmp, start, count, fillwith, retcluster, got);
	} else {
		/*
		 * This is a new file, initialize start
		 */
		struct timeval tv;

		microtime(&tv);
		start = (tv.tv_usec >> 10)|tv.tv_usec;
		len = 0;
	}

	/*
	 * Start at a (pseudo) random place to maximize cluster runs
	 * under multiple writers.
	 */
	foundcn = newst = (start * 1103515245 + 12345) % (pmp->pm_maxcluster + 1);
	foundl = 0;

	for (cn = newst; cn <= pmp->pm_maxcluster;) {
		idx = cn / N_INUSEBITS;
		map = pmp->pm_inusemap[idx];
		map |= (1 << (cn % N_INUSEBITS)) - 1;
		if (map != (u_int)-1) {
			cn = idx * N_INUSEBITS + ffs(map^(u_int)-1) - 1;
			if ((l = chainlength(pmp, cn, count)) >= count)
				return chainalloc(pmp, cn, count, fillwith, retcluster, got);
			if (l > foundl) {
				foundcn = cn;
				foundl = l;
			}
			cn += l + 1;
			continue;
		}
		cn += N_INUSEBITS - cn % N_INUSEBITS;
	}
	for (cn = 0; cn < newst;) {
		idx = cn / N_INUSEBITS;
		map = pmp->pm_inusemap[idx];
		map |= (1 << (cn % N_INUSEBITS)) - 1;
		if (map != (u_int)-1) {
			cn = idx * N_INUSEBITS + ffs(map^(u_int)-1) - 1;
			if ((l = chainlength(pmp, cn, count)) >= count)
				return chainalloc(pmp, cn, count, fillwith, retcluster, got);
			if (l > foundl) {
				foundcn = cn;
				foundl = l;
			}
			cn += l + 1;
			continue;
		}
		cn += N_INUSEBITS - cn % N_INUSEBITS;
	}

	if (!foundl)
		return ENOSPC;

	if (len)
		return chainalloc(pmp, start, len, fillwith, retcluster, got);
	else
		return chainalloc(pmp, foundcn, foundl, fillwith, retcluster, got);
}


/*
 * Free a chain of clusters.
 *
 * pmp		- address of the msdosfs mount structure for the filesystem
 *		  containing the cluster chain to be freed.
 * startcluster - number of the 1st cluster in the chain of clusters to be
 *		  freed.
 */
int
freeclusterchain(pmp, cluster)
	struct msdosfsmount *pmp;
	u_long cluster;
{
	int error = 0;
	struct buf *bp = NULL;
	u_long bn, bo, bsize, byteoffset;
	u_long readcn, lbn = -1;

	while (cluster >= CLUST_FIRST && cluster <= pmp->pm_maxcluster) {
		byteoffset = FATOFS(pmp, cluster);
		fatblock(pmp, byteoffset, &bn, &bsize, &bo);
		if (lbn != bn) {
			if (bp)
				updatefats(pmp, bp, lbn);
			error = bread(pmp->pm_devvp, bn, bsize, NOCRED, &bp);
			if (error)
				return error;
			lbn = bn;
		}
		usemap_free(pmp, cluster);
		readcn = getushort(&bp->b_data[bo]);
		if (FAT12(pmp)) {
			if (cluster & 1) {
				cluster = readcn >> 4;
				readcn &= 0x000f;
				readcn |= MSDOSFSFREE << 4;
			} else {
				cluster = readcn;
				readcn &= 0xf000;
				readcn |= MSDOSFSFREE & 0xfff;
			}
			putushort(&bp->b_data[bo], readcn);
			cluster &= 0x0fff;
			if ((cluster&0x0ff0) == 0x0ff0)
				cluster |= 0xf000;
		} else {
			cluster = readcn;
			putushort(&bp->b_data[bo], MSDOSFSFREE);
		}
	}
	if (bp)
		updatefats(pmp, bp, bn);
	return error;
}

/*
 * Read in fat blocks looking for free clusters. For every free cluster
 * found turn off its corresponding bit in the pm_inusemap.
 */
int
fillinusemap(pmp)
	struct msdosfsmount *pmp;
{
	struct buf *bp = NULL;
	u_long cn, readcn;
	int error;
	int fat12 = FAT12(pmp);
	u_long bn, bo, bsize, byteoffset;

	/*
	 * Mark all clusters in use, we mark the free ones in the fat scan
	 * loop further down.
	 */
	for (cn = 0; cn < (pmp->pm_maxcluster + N_INUSEBITS) / N_INUSEBITS; cn++)
		pmp->pm_inusemap[cn] = (u_int)-1;

	/*
	 * Figure how many free clusters are in the filesystem by ripping
	 * through the fat counting the number of entries whose content is
	 * zero.  These represent free clusters.
	 */
	pmp->pm_freeclustercount = 0;
	for (cn = CLUST_FIRST; cn <= pmp->pm_maxcluster; cn++) {
		byteoffset = FATOFS(pmp, cn);
		bo = byteoffset % pmp->pm_fatblocksize;
		if (!bo || !bp) {
			/* Read new FAT block */
			if (bp)
				brelse(bp);
			fatblock(pmp, byteoffset, &bn, &bsize, NULL);
			error = bread(pmp->pm_devvp, bn, bsize, NOCRED, &bp);
			if (error)
				return error;
		}
		readcn = getushort(&bp->b_data[bo]);
		if (fat12) {
			if (cn & 1)
				readcn >>= 4;
			readcn &= 0x0fff;
		}

		if (readcn == 0)
			usemap_free(pmp, cn);
	}
	brelse(bp);
	return 0;
}

/*
 * Allocate a new cluster and chain it onto the end of the file.
 *
 * dep	 - the file to extend
 * count - number of clusters to allocate
 * bpp	 - where to return the address of the buf header for the first new
 *	   file block
 * ncp	 - where to put cluster number of the first newly allocated cluster
 *	   If this pointer is 0, do not return the cluster number.
 * flags - see fat.h
 *
 * NOTE: This function is not responsible for turning on the DE_UPDATE bit of
 * the de_flag field of the denode and it does not change the de_FileSize
 * field.  This is left for the caller to do.
 */
int
extendfile(dep, count, bpp, ncp, flags)
	struct denode *dep;
	u_long count;
	struct buf **bpp;
	u_long *ncp;
	int flags;
{
	int error = 0;
	u_long frcn;
	u_long cn, got;
	struct msdosfsmount *pmp = dep->de_pmp;
	struct buf *bp;

	/*
	 * Don't try to extend the root directory
	 */
	if (DETOV(dep)->v_flag & VROOT) {
		printf("extendfile(): attempt to extend root directory\n");
		return ENOSPC;
	}

	/*
	 * If the "file's last cluster" cache entry is empty, and the file
	 * is not empty, then fill the cache entry by calling pcbmap().
	 */
	fc_fileextends++;
	if (dep->de_fc[FC_LASTFC].fc_frcn == FCE_EMPTY &&
	    dep->de_StartCluster != 0) {
		fc_lfcempty++;
		error = pcbmap(dep, 0xffff, 0, &cn);
		/* we expect it to return E2BIG */
		if (error != E2BIG)
			return error;
		error = 0;
	}

	while (count > 0) {
		/*
		 * Allocate a new cluster chain and cat onto the end of the file.
		 * If the file is empty we make de_StartCluster point to the new
		 * block.  Note that de_StartCluster being 0 is sufficient to be
		 * sure the file is empty since we exclude attempts to extend the
		 * root directory above, and the root dir is the only file with a
		 * startcluster of 0 that has blocks allocated (sort of).
		 */
		if (dep->de_StartCluster == 0)
			cn = 0;
		else
			cn = dep->de_fc[FC_LASTFC].fc_fsrcn + 1;
		error = clusteralloc(pmp, cn, count, CLUST_EOFE, &cn, &got);
		if (error)
			return error;

		count -= got;

		/*
		 * Give them the filesystem relative cluster number if they want
		 * it.
		 */
		if (ncp) {
			*ncp = cn;
			ncp = NULL;
		}

		if (dep->de_StartCluster == 0) {
			dep->de_StartCluster = cn;
			frcn = 0;
		} else {
			error = fatentry(FAT_SET, pmp, dep->de_fc[FC_LASTFC].fc_fsrcn,
					 0, cn);
			if (error) {
				clusterfree(pmp, cn, NULL);
				return error;
			}

			frcn = dep->de_fc[FC_LASTFC].fc_frcn + 1;
		}

		/*
		 * Update the "last cluster of the file" entry in the denode's fat
		 * cache.
		 */
		fc_setcache(dep, FC_LASTFC, frcn + got - 1, cn + got - 1);

		if (flags & DE_CLEAR) {
			while (got-- > 0) {
				/*
				 * Get the buf header for the new block of the file.
				 */
				if (dep->de_Attributes & ATTR_DIRECTORY)
					bp = getblk(pmp->pm_devvp, cntobn(pmp, cn++),
						    pmp->pm_bpcluster, 0, 0);
				else {
					bp = getblk(DETOV(dep), frcn++, pmp->pm_bpcluster, 0, 0);
					/*
					 * Do the bmap now, as in msdosfs_write
					 */
					if (pcbmap(dep, bp->b_lblkno, &bp->b_blkno, 0))
						bp->b_blkno = -1;
					if (bp->b_blkno == -1)
						panic("extendfile: pcbmap");
				}
				clrbuf(bp);
				if (bpp) {
					*bpp = bp;
					bpp = NULL;
				} else {
					bp->b_flags |= B_AGE;
					bawrite(bp);
				}
			}
		}
	}

	return 0;
}
