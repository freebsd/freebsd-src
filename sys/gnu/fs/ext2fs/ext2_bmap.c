/*
 * Copyright (c) 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)ufs_bmap.c	8.7 (Berkeley) 3/21/95
 * $Id: ufs_bmap.c,v 1.15 1997/03/09 06:10:31 mpp Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/resourcevar.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

/*
 * Bmap converts a the logical block number of a file to its physical block
 * number on the disk. The conversion is done by using the logical block
 * number to index into the array of block pointers described by the dinode.
 */
int
ufs_bmap(ap)
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		ufs_daddr_t a_bn;
		struct vnode **a_vpp;
		ufs_daddr_t *a_bnp;
		int *a_runp;
		int *a_runb;
	} */ *ap;
{
	/*
	 * Check for underlying vnode requests and ensure that logical
	 * to physical mapping is requested.
	 */
	if (ap->a_vpp != NULL)
		*ap->a_vpp = VTOI(ap->a_vp)->i_devvp;
	if (ap->a_bnp == NULL)
		return (0);

	return (ufs_bmaparray(ap->a_vp, ap->a_bn, ap->a_bnp, NULL, NULL,
	    ap->a_runp, ap->a_runb));
}

/*
 * Indirect blocks are now on the vnode for the file.  They are given negative
 * logical block numbers.  Indirect blocks are addressed by the negative
 * address of the first data block to which they point.  Double indirect blocks
 * are addressed by one less than the address of the first indirect block to
 * which they point.  Triple indirect blocks are addressed by one less than
 * the address of the first double indirect block to which they point.
 *
 * ufs_bmaparray does the bmap conversion, and if requested returns the
 * array of logical blocks which must be traversed to get to a block.
 * Each entry contains the offset into that block that gets you to the
 * next block and the disk address of the block (if it is assigned).
 */

int
ufs_bmaparray(vp, bn, bnp, ap, nump, runp, runb)
	struct vnode *vp;
	ufs_daddr_t bn;
	ufs_daddr_t *bnp;
	struct indir *ap;
	int *nump;
	int *runp;
	int *runb;
{
	register struct inode *ip;
	struct buf *bp;
	struct ufsmount *ump;
	struct mount *mp;
	struct vnode *devvp;
	struct indir a[NIADDR+1], *xap;
	ufs_daddr_t daddr;
	long metalbn;
	int error, maxrun = 0, num;

	ip = VTOI(vp);
	mp = vp->v_mount;
	ump = VFSTOUFS(mp);
#ifdef DIAGNOSTIC
	if (ap != NULL && nump == NULL || ap == NULL && nump != NULL)
		panic("ufs_bmaparray: invalid arguments");
#endif

	if (runp) {
		/*
		 * XXX
		 * If MAXPHYS is the largest transfer the disks can handle,
		 * we probably want maxrun to be 1 block less so that we
		 * don't create a block larger than the device can handle.
		 */
		*runp = 0;
		maxrun = MAXPHYS / mp->mnt_stat.f_iosize - 1;
	}

	if (runb) {
		*runb = 0;
	}

	xap = ap == NULL ? a : ap;
	if (!nump)
		nump = &num;
	error = ufs_getlbns(vp, bn, xap, nump);
	if (error)
		return (error);

	num = *nump;
	if (num == 0) {
		*bnp = blkptrtodb(ump, ip->i_db[bn]);
		if (*bnp == 0)
			*bnp = -1;
		else if (runp) {
			daddr_t bnb = bn;
			for (++bn; bn < NDADDR && *runp < maxrun &&
			    is_sequential(ump, ip->i_db[bn - 1], ip->i_db[bn]);
			    ++bn, ++*runp);
			bn = bnb;
			if (runb && (bn > 0)) {
				for (--bn; (bn >= 0) && (*runb < maxrun) &&
					is_sequential(ump, ip->i_db[bn],
						ip->i_db[bn+1]);
						--bn, ++*runb);
			}
		}
		return (0);
	}


	/* Get disk address out of indirect block array */
	daddr = ip->i_ib[xap->in_off];

	devvp = VFSTOUFS(vp->v_mount)->um_devvp;
	for (bp = NULL, ++xap; --num; ++xap) {
		/*
		 * Exit the loop if there is no disk address assigned yet and
		 * the indirect block isn't in the cache, or if we were
		 * looking for an indirect block and we've found it.
		 */

		metalbn = xap->in_lbn;
		if ((daddr == 0 && !incore(vp, metalbn)) || metalbn == bn)
			break;
		/*
		 * If we get here, we've either got the block in the cache
		 * or we have a disk address for it, go fetch it.
		 */
		if (bp)
			bqrelse(bp);

		xap->in_exists = 1;
		bp = getblk(vp, metalbn, mp->mnt_stat.f_iosize, 0, 0);
		if ((bp->b_flags & B_CACHE) == 0) {
#ifdef DIAGNOSTIC
			if (!daddr)
				panic("ufs_bmaparray: indirect block not in cache");
#endif
			bp->b_blkno = blkptrtodb(ump, daddr);
			bp->b_flags |= B_READ;
			vfs_busy_pages(bp, 0);
			VOP_STRATEGY(bp);
			curproc->p_stats->p_ru.ru_inblock++;	/* XXX */
			error = biowait(bp);
			if (error) {
				brelse(bp);
				return (error);
			}
		}

		daddr = ((ufs_daddr_t *)bp->b_data)[xap->in_off];
		if (num == 1 && daddr && runp) {
			for (bn = xap->in_off + 1;
			    bn < MNINDIR(ump) && *runp < maxrun &&
			    is_sequential(ump,
			    ((ufs_daddr_t *)bp->b_data)[bn - 1],
			    ((ufs_daddr_t *)bp->b_data)[bn]);
			    ++bn, ++*runp);
			bn = xap->in_off;
			if (runb && bn) {
				for(--bn; bn > 0 && *runb < maxrun &&
			    		is_sequential(ump, ((daddr_t *)bp->b_data)[bn],
					    ((daddr_t *)bp->b_data)[bn+1]);
			    		--bn, ++*runb);
			}
		}
	}
	if (bp)
		bqrelse(bp);

	daddr = blkptrtodb(ump, daddr);
	*bnp = daddr == 0 ? -1 : daddr;
	return (0);
}

/*
 * Create an array of logical block number/offset pairs which represent the
 * path of indirect blocks required to access a data block.  The first "pair"
 * contains the logical block number of the appropriate single, double or
 * triple indirect block and the offset into the inode indirect block array.
 * Note, the logical block number of the inode single/double/triple indirect
 * block appears twice in the array, once with the offset into the i_ib and
 * once with the offset into the page itself.
 */
int
ufs_getlbns(vp, bn, ap, nump)
	struct vnode *vp;
	ufs_daddr_t bn;
	struct indir *ap;
	int *nump;
{
	long metalbn, realbn;
	struct ufsmount *ump;
	int blockcnt, i, numlevels, off;

	ump = VFSTOUFS(vp->v_mount);
	if (nump)
		*nump = 0;
	numlevels = 0;
	realbn = bn;
	if ((long)bn < 0)
		bn = -(long)bn;

	/* The first NDADDR blocks are direct blocks. */
	if (bn < NDADDR)
		return (0);

	/*
	 * Determine the number of levels of indirection.  After this loop
	 * is done, blockcnt indicates the number of data blocks possible
	 * at the given level of indirection, and NIADDR - i is the number
	 * of levels of indirection needed to locate the requested block.
	 */
	for (blockcnt = 1, i = NIADDR, bn -= NDADDR;; i--, bn -= blockcnt) {
		if (i == 0)
			return (EFBIG);
		blockcnt *= MNINDIR(ump);
		if (bn < blockcnt)
			break;
	}

	/* Calculate the address of the first meta-block. */
	if (realbn >= 0)
		metalbn = -(realbn - bn + NIADDR - i);
	else
		metalbn = -(-realbn - bn + NIADDR - i);

	/*
	 * At each iteration, off is the offset into the bap array which is
	 * an array of disk addresses at the current level of indirection.
	 * The logical block number and the offset in that block are stored
	 * into the argument array.
	 */
	ap->in_lbn = metalbn;
	ap->in_off = off = NIADDR - i;
	ap->in_exists = 0;
	ap++;
	for (++numlevels; i <= NIADDR; i++) {
		/* If searching for a meta-data block, quit when found. */
		if (metalbn == realbn)
			break;

		blockcnt /= MNINDIR(ump);
		off = (bn / blockcnt) % MNINDIR(ump);

		++numlevels;
		ap->in_lbn = metalbn;
		ap->in_off = off;
		ap->in_exists = 0;
		++ap;

		metalbn -= -1 + off * blockcnt;
	}
	if (nump)
		*nump = numlevels;
	return (0);
}
