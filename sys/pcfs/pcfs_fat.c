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
 *	$Id: pcfs_fat.c,v 1.2 1993/10/16 19:29:34 rgrimes Exp $
 */

/*
 *  kernel include files.
 */
#include "param.h"
#include "systm.h"
#include "buf.h"
#include "file.h"
#include "namei.h"
#include "mount.h"	/* to define statfs structure */
#include "vnode.h"	/* to define vattr structure */
#include "errno.h"

/*
 *  pcfs include files.
 */
#include "bpb.h"
#include "pcfsmount.h"
#include "direntry.h"
#include "denode.h"
#include "fat.h"

/*
 *  Fat cache stats.
 */
int fc_fileextends;		/* # of file extends			*/
int fc_lfcempty;		/* # of time last file cluster cache entry
				 * was empty */
int fc_bmapcalls;		/* # of times pcbmap was called		*/
#define	LMMAX	20
int fc_lmdistance[LMMAX];	/* counters for how far off the last cluster
				 * mapped entry was. */
int fc_largedistance;		/* off by more than LMMAX		*/

/* Byte offset in FAT on filesystem pmp, cluster cn */
#define	FATOFS(pmp, cn)	(FAT12(pmp) ? (cn) * 3 / 2 : (cn) * 2)


static void fatblock (pmp, ofs, bnp, sizep, bop)
	struct pcfsmount *pmp;
	u_long ofs;
	u_long *bnp;
	u_long *sizep;
	u_long *bop;
{
	u_long bn, size;

	bn = ofs / pmp->pm_fatblocksize * pmp->pm_fatblocksec;
	size = min (pmp->pm_fatblocksec, pmp->pm_FATsecs - bn)
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
 *  Map the logical cluster number of a file into
 *  a physical disk sector that is filesystem relative.
 *  dep - address of denode representing the file of interest
 *  findcn - file relative cluster whose filesystem relative
 *    cluster number and/or block number are/is to be found
 *  bnp - address of where to place the file system relative
 *    block number.  If this pointer is null then don't return
 *    this quantity.
 *  cnp - address of where to place the file system relative
 *    cluster number.  If this pointer is null then don't return
 *    this quantity.
 *  NOTE:
 *    Either bnp or cnp must be non-null.
 *    This function has one side effect.  If the requested
 *    file relative cluster is beyond the end of file, then
 *    the actual number of clusters in the file is returned
 *    in *cnp.  This is useful for determining how long a
 *    directory is.  If cnp is null, nothing is returned.
 */
int
pcbmap(dep, findcn, bnp, cnp)
	struct denode *dep;
	u_long findcn;	/* file relative cluster to get		*/
	daddr_t *bnp;		/* returned filesys relative blk number	*/
	u_long *cnp;	/* returned cluster number		*/
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
	struct pcfsmount *pmp = dep->de_pmp;
	u_long bsize;
	int fat12 = FAT12(pmp);	/* 12 bit fat	*/

	fc_bmapcalls++;

/*
 *  If they don't give us someplace to return a value
 *  then don't bother doing anything.
 */
	if (bnp == NULL  &&  cnp == NULL)
		return 0;

	cn = dep->de_StartCluster;
/*
 *  The "file" that makes up the root directory is contiguous,
 *  permanently allocated, of fixed size, and is not made up
 *  of clusters.  If the cluster number is beyond the end of
 *  the root directory, then return the number of clusters in
 *  the file.
 */
	if (cn == PCFSROOT) {
		if (dep->de_Attributes & ATTR_DIRECTORY) {
			if (findcn * pmp->pm_SectPerClust > pmp->pm_rootdirsize) {
				if (cnp)
					*cnp = pmp->pm_rootdirsize / pmp->pm_SectPerClust;
				return E2BIG;
			}
			if (bnp)
				*bnp = pmp->pm_rootdirblk + (findcn * pmp->pm_SectPerClust);
			if (cnp)
				*cnp = PCFSROOT;
			return 0;
		}
		else {	/* just an empty file */
			if (cnp)
				*cnp = 0;
			return E2BIG;
		}
	}

/*
 *  Rummage around in the fat cache, maybe we can avoid
 *  tromping thru every fat entry for the file.
 *  And, keep track of how far off the cache was from
 *  where we wanted to be.
 */
	i = 0;
	fc_lookup(dep, findcn, &i, &cn);
	if ((bn = findcn - i) >= LMMAX)
		fc_largedistance++;
	else
		fc_lmdistance[bn]++;

/*
 *  Handle all other files or directories the normal way.
 */
	for (; i < findcn; i++) {
		if (PCFSEOF(cn))
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
		cn = getushort(&bp->b_un.b_addr[bo]);
		if (fat12) {
			if (prevcn & 1)
				cn >>= 4;
			cn &= 0x0fff;
/*
 *  Force the special cluster numbers in the range
 *  0x0ff0-0x0fff to be the same as for 16 bit cluster
 *  numbers to let the rest of pcfs think it is always
 *  dealing with 16 bit fats.
 */
			if ((cn & 0x0ff0) == 0x0ff0)
				cn |= 0xf000;
		}
	}

	if (!PCFSEOF(cn)) {
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
	fc_setcache(dep, FC_LASTFC, i-1, prevcn);
	return E2BIG;
}

/*
 *  Find the closest entry in the fat cache to the
 *  cluster we are looking for.
 */
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
		if (cn != FCE_EMPTY  &&  cn <= findcn) {
			if (closest == 0  ||  cn > closest->fc_frcn)
				closest = &dep->de_fc[i];
		}
	}
	if (closest) {
		*frcnp  = closest->fc_frcn;
		*fsrcnp = closest->fc_fsrcn;
	}
}

/*
 *  Purge the fat cache in denode dep of all entries
 *  relating to file relative cluster frcn and beyond.
 */
fc_purge(dep, frcn)
	struct denode *dep;
	u_int frcn;
{
	int i;
	struct fatcache *fcp;

	fcp = dep->de_fc;
	for (i = 0; i < FC_SIZE; i++, fcp++) {
		if (fcp->fc_frcn != FCE_EMPTY  &&  fcp->fc_frcn >= frcn)
			fcp->fc_frcn = FCE_EMPTY;
	}
}

/*
 *  Once the first fat is updated the other copies of
 *  the fat must also be updated.  This function does
 *  this.
 *  pmp - pcfsmount structure for filesystem to update
 *  bp - addr of modified fat block
 *  fatbn - block number relative to begin of filesystem
 *    of the modified fat block.
 */
void
updateotherfats(pmp, bp, fatbn)
	struct pcfsmount *pmp;
	struct buf *bp;
	u_long fatbn;
{
	int i;
	struct buf *bpn;

#if defined(PCFSDEBUG)
printf("updateotherfats(pmp %08x, bp %08x, fatbn %d)\n",
	pmp, bp, fatbn);
#endif /* defined(PCFSDEBUG) */

/*
 *  Now copy the block(s) of the modified fat to the other
 *  copies of the fat and write them out.  This is faster
 *  than reading in the other fats and then writing them
 *  back out.  This could tie up the fat for quite a while.
 *  Preventing others from accessing it.  To prevent us
 *  from going after the fat quite so much we use delayed
 *  writes, unless they specfied "synchronous" when the
 *  filesystem was mounted.  If synch is asked for then
 *  use bwrite()'s and really slow things down.
 */
	for (i = 1; i < pmp->pm_FATs; i++) {
		fatbn += pmp->pm_FATsecs;
		/* getblk() never fails */
		bpn = getblk(pmp->pm_devvp, fatbn, bp->b_bcount);
		bcopy(bp->b_un.b_addr, bpn->b_un.b_addr,
			bp->b_bcount);
		if (pmp->pm_waitonfat)
			bwrite(bpn);
		else
			bdwrite(bpn);
	}
}

/*
 *  Updating entries in 12 bit fats is a pain in the butt.
 *
 *  The following picture shows where nibbles go when
 *  moving from a 12 bit cluster number into the appropriate
 *  bytes in the FAT.
 *
 *      byte m        byte m+1      byte m+2
 *    +----+----+   +----+----+   +----+----+
 *    |  0    1 |   |  2    3 |   |  4    5 |   FAT bytes
 *    +----+----+   +----+----+   +----+----+
 *
 *       +----+----+----+ +----+----+----+
 *       |  3    0    1 | |  4    5    2 |
 *       +----+----+----+ +----+----+----+
 *         cluster n        cluster n+1
 *
 *    Where n is even.
 *    m = n + (n >> 2)
 *
 *	(Function no longer used)
 */


extern inline void
usemap_alloc (struct pcfsmount *pmp, u_long cn)
{
	pmp->pm_inusemap[cn / 8] |= 1 << (cn % 8);
	pmp->pm_freeclustercount--;
	/* This assumes that the lowest available cluster was allocated */
	pmp->pm_lookhere = cn + 1;
}

extern inline void
usemap_free (struct pcfsmount *pmp, u_long cn)
{
	pmp->pm_freeclustercount++;
	pmp->pm_inusemap[cn / 8] &= ~(1 << (cn % 8));
	if (pmp->pm_lookhere > cn)
		pmp->pm_lookhere = cn;
}

int
clusterfree(pmp, cluster, oldcnp)
	struct pcfsmount *pmp;
	u_long cluster;
	u_long *oldcnp;
{
	int error;
	u_long oldcn;

	error = fatentry(FAT_GET_AND_SET, pmp, cluster, &oldcn, PCFSFREE);
	if (error == 0) {
/*
 *  If the cluster was successfully marked free, then update the count of
 *  free clusters, and turn off the "allocated" bit in the
 *  "in use" cluster bit map.
 */
		usemap_free(pmp, cluster);
		if (oldcnp)
			*oldcnp = oldcn;
	}
	return error;
}

/*
 *  Get or Set or 'Get and Set' the cluster'th entry in the
 *  fat.
 *  function - whether to get or set a fat entry
 *  pmp - address of the pcfsmount structure for the
 *    filesystem whose fat is to be manipulated.
 *  cluster - which cluster is of interest
 *  oldcontents - address of a word that is to receive
 *    the contents of the cluster'th entry if this is
 *    a get function
 *  newcontents - the new value to be written into the
 *    cluster'th element of the fat if this is a set
 *    function.
 *
 *  This function can also be used to free a cluster
 *  by setting the fat entry for a cluster to 0.
 *
 *  All copies of the fat are updated if this is a set
 *  function.
 *  NOTE:
 *    If fatentry() marks a cluster as free it does not
 *    update the inusemap in the pcfsmount structure.
 *    This is left to the caller.
 */
int
fatentry(function, pmp, cn, oldcontents, newcontents)
	int function;
	struct pcfsmount *pmp;
	u_long cn;
	u_long *oldcontents;
	u_long newcontents;
{
	int error;
	u_long readcn;
	u_long bn, bo, bsize, byteoffset;
	struct buf *bp;
/*printf("fatentry(func %d, pmp %08x, clust %d, oldcon %08x, newcon %d)\n",
	function, pmp, cluster, oldcontents, newcontents);*/

#ifdef DIAGNOSTIC
/*
 *  Be sure they asked us to do something.
 */
	if ((function & (FAT_SET | FAT_GET)) == 0) {
		printf("fatentry(): function code doesn't specify get or set\n");
		return EINVAL;
	}

/*
 *  If they asked us to return a cluster number
 *  but didn't tell us where to put it, give them
 *  an error.
 */
	if ((function & FAT_GET)  &&  oldcontents == NULL) {
		printf("fatentry(): get function with no place to put result\n");
		return EINVAL;
	}
#endif

/*
 *  Be sure the requested cluster is in the filesystem.
 */
	if (cn < CLUST_FIRST || cn > pmp->pm_maxcluster)
		return EINVAL;

	byteoffset = FATOFS(pmp, cn);
	fatblock(pmp, byteoffset, &bn, &bsize, &bo);
	error = bread(pmp->pm_devvp, bn, bsize, NOCRED, &bp);
	if (function & FAT_GET) {
		readcn = getushort(&bp->b_un.b_addr[bo]);
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
			readcn = getushort(&bp->b_un.b_addr[bo]);
			if (cn & 1) {
				readcn &= 0x000f;
				readcn |= (newcontents << 4);
			}
			else {
				readcn &= 0xf000;
				readcn |= (newcontents << 0);
			}
			putushort(&bp->b_un.b_addr[bo], readcn);
		}
		else
			putushort(&bp->b_un.b_addr[bo], newcontents);
		updateotherfats(pmp, bp, bn);
/*
 *  Write out the first fat last.
 */
		if (pmp->pm_waitonfat)
			bwrite(bp);
		else
			bdwrite(bp);
		bp = NULL;
		pmp->pm_fmod++;
	}
	if (bp)
		brelse(bp);
	return 0;
}

/*
 *  Allocate a free cluster.
 *  pmp - 
 *  retcluster - put the allocated cluster's number here.
 *  fillwith - put this value into the fat entry for the
 *     allocated cluster.
 */
int
clusteralloc(pmp, retcluster, fillwith)
	struct pcfsmount *pmp;
	u_long *retcluster;
	u_long fillwith;
{
	int error;
	u_long cn;
	u_long idx, max_idx, bit, map;

	max_idx = pmp->pm_maxcluster / 8;
	for (idx = pmp->pm_lookhere / 8; idx <= max_idx; idx++) {
		map = pmp->pm_inusemap[idx];
		if (map != 0xff) {
			for (bit = 0; bit < 8; bit++) {
				if ((map & (1 << bit)) == 0) {
					cn = idx * 8 + bit;
					goto found_one;
				}
			}
		}
	}
	return ENOSPC;

found_one:;
	error = fatentry(FAT_SET, pmp, cn, 0, fillwith);
	if (error == 0) {
		usemap_alloc(pmp, cn);
		*retcluster = cn;
	}
#if defined(PCFSDEBUG)
printf("clusteralloc(): allocated cluster %d\n", cn);
#endif /* defined(PCFSDEBUG) */
	return error;
}

/*
 *  Free a chain of clusters.
 *  pmp - address of the pcfs mount structure for the
 *    filesystem containing the cluster chain to be freed.
 *  startcluster - number of the 1st cluster in the chain
 *    of clusters to be freed.
 */
int
freeclusterchain(pmp, startcluster)
	struct pcfsmount *pmp;
	u_long startcluster;
{
	u_long nextcluster;
	int error = 0;

	while (startcluster >= CLUST_FIRST  &&  startcluster <= pmp->pm_maxcluster) {
		error = clusterfree(pmp, startcluster, &nextcluster);
		if (error) {
			printf("freeclusterchain(): free failed, cluster %d\n",
				startcluster);
			break;
		}
		startcluster = nextcluster;
	}
	return error;
}

/*
 *  Read in fat blocks looking for free clusters.
 *  For every free cluster found turn off its
 *  corresponding bit in the pm_inusemap.
 */
int
fillinusemap(pmp)
	struct pcfsmount *pmp;
{
	struct buf *bp = NULL;
	u_long cn, readcn;
	int error;
	int fat12 = FAT12(pmp);
	u_long bn, bo, bsize, byteoffset;

/*
 *  Mark all clusters in use, we mark the free ones in the
 *  fat scan loop further down.
 */
	for (cn = 0; cn < (pmp->pm_maxcluster >> 3) + 1; cn++)
		pmp->pm_inusemap[cn] = 0xff;

/*
 *  Figure how many free clusters are in the filesystem
 *  by ripping thougth the fat counting the number of
 *  entries whose content is zero.  These represent free
 *  clusters.
 */
	pmp->pm_freeclustercount = 0;
	pmp->pm_lookhere = pmp->pm_maxcluster + 1;
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
		readcn = getushort(&bp->b_un.b_addr[bo]);
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
 *  Allocate a new cluster and chain it onto the end of the
 *  file.
 *  dep - the file to extend
 *  bpp - where to return the address of the buf header for the
 *        new file block
 *  ncp - where to put cluster number of the newly allocated file block
 *        If this pointer is 0, do not return the cluster number.
 *
 *  NOTE:
 *   This function is not responsible for turning on the DEUPD
 *   bit if the de_flag field of the denode and it does not
 *   change the de_FileSize field.  This is left for the caller
 *   to do.
 */
int
extendfile(dep, bpp, ncp)
	struct denode *dep;
	struct buf **bpp;
	u_int *ncp;
{
	int error = 0;
	u_long frcn;
	u_long cn;
	struct pcfsmount *pmp = dep->de_pmp;

/*
 *  Don't try to extend the root directory
 */
	if (DETOV(dep)->v_flag & VROOT) {
		printf("extendfile(): attempt to extend root directory\n");
		return ENOSPC;
	}

/*
 *  If the "file's last cluster" cache entry is empty,
 *  and the file is not empty,
 *  then fill the cache entry by calling pcbmap().
 */
	fc_fileextends++;
	if (dep->de_fc[FC_LASTFC].fc_frcn == FCE_EMPTY  &&
	    dep->de_StartCluster != 0) {
		fc_lfcempty++;
		error = pcbmap(dep, 0xffff, 0, &cn);
		/* we expect it to return E2BIG */
		if (error != E2BIG)
			return error;
		error = 0;
	}

/*
 *  Allocate another cluster and chain onto the end of the file.
 *  If the file is empty we make de_StartCluster point to the
 *  new block.  Note that de_StartCluster being 0 is sufficient
 *  to be sure the file is empty since we exclude attempts to
 *  extend the root directory above, and the root dir is the
 *  only file with a startcluster of 0 that has blocks allocated
 *  (sort of).
 */
	if (error = clusteralloc(pmp, &cn, CLUST_EOFE))
		return error;
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
 *  Update the "last cluster of the file" entry in the denode's
 *  fat cache.
 */
	fc_setcache(dep, FC_LASTFC, frcn, cn);

/*
 *  Get the buf header for the new block of the file.
 */
	if (dep->de_Attributes & ATTR_DIRECTORY) {
		*bpp = getblk(pmp->pm_devvp, cntobn(pmp, cn),
			pmp->pm_bpcluster);
	} else {
		*bpp = getblk(DETOV(dep), frcn,
			pmp->pm_bpcluster);
	}
	clrbuf(*bpp);

/*
 *  Give them the filesystem relative cluster number
 *  if they want it.
 */
	if (ncp)
		*ncp = cn;
	return 0;
}
