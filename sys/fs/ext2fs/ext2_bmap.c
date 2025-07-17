/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/endian.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/racct.h>
#include <sys/resourcevar.h>
#include <sys/stat.h>

#include <fs/ext2fs/fs.h>
#include <fs/ext2fs/inode.h>
#include <fs/ext2fs/ext2fs.h>
#include <fs/ext2fs/ext2_dinode.h>
#include <fs/ext2fs/ext2_extern.h>
#include <fs/ext2fs/ext2_mount.h>

/*
 * Bmap converts the logical block number of a file to its physical block
 * number on the disk. The conversion is done by using the logical block
 * number to index into the array of block pointers described by the dinode.
 */
int
ext2_bmap(struct vop_bmap_args *ap)
{
	daddr_t blkno;
	int error;

	/*
	 * Check for underlying vnode requests and ensure that logical
	 * to physical mapping is requested.
	 */
	if (ap->a_bop != NULL)
		*ap->a_bop = &VTOI(ap->a_vp)->i_devvp->v_bufobj;
	if (ap->a_bnp == NULL)
		return (0);

	if (VTOI(ap->a_vp)->i_flag & IN_E4EXTENTS)
		error = ext4_bmapext(ap->a_vp, ap->a_bn, &blkno,
		    ap->a_runp, ap->a_runb);
	else
		error = ext2_bmaparray(ap->a_vp, ap->a_bn, &blkno,
		    ap->a_runp, ap->a_runb);
	*ap->a_bnp = blkno;
	return (error);
}

/*
 * Convert the logical block number of a file to its physical block number
 * on the disk within ext4 extents.
 */
int
ext4_bmapext(struct vnode *vp, int32_t bn, int64_t *bnp, int *runp, int *runb)
{
	struct inode *ip;
	struct m_ext2fs *fs;
	struct mount *mp;
	struct ext2mount *ump;
	struct ext4_extent_header *ehp;
	struct ext4_extent *ep;
	struct ext4_extent_path *path = NULL;
	daddr_t lbn;
	int error, depth, maxrun = 0, bsize;

	ip = VTOI(vp);
	fs = ip->i_e2fs;
	mp = vp->v_mount;
	ump = VFSTOEXT2(mp);
	lbn = bn;
	ehp = (struct ext4_extent_header *)ip->i_data;
	depth = le16toh(ehp->eh_depth);
	bsize = EXT2_BLOCK_SIZE(ump->um_e2fs);

	*bnp = -1;
	if (runp != NULL) {
		maxrun = mp->mnt_iosize_max / bsize - 1;
		*runp = 0;
	}
	if (runb != NULL)
		*runb = 0;

	error = ext4_ext_find_extent(ip, lbn, &path);
	if (error)
		return (error);

	ep = path[depth].ep_ext;
	if(ep) {
		if (lbn < le32toh(ep->e_blk)) {
			if (runp != NULL) {
				*runp = min(maxrun, le32toh(ep->e_blk) - lbn - 1);
			}
		} else if (le32toh(ep->e_blk) <= lbn &&
			    lbn < le32toh(ep->e_blk) + le16toh(ep->e_len)) {
			*bnp = fsbtodb(fs, lbn - le32toh(ep->e_blk) +
			    (le32toh(ep->e_start_lo) |
			    (daddr_t)le16toh(ep->e_start_hi) << 32));
			if (runp != NULL) {
				*runp = min(maxrun,
				    le16toh(ep->e_len) -
				    (lbn - le32toh(ep->e_blk)) - 1);
			}
			if (runb != NULL)
				*runb = min(maxrun, lbn - le32toh(ep->e_blk));
		} else {
			if (runb != NULL)
				*runb = min(maxrun, le32toh(ep->e_blk) + lbn -
				    le16toh(ep->e_len));
		}
	}

	ext4_ext_path_free(path);

	return (error);
}

static int
readindir(struct vnode *vp, e2fs_lbn_t lbn, e2fs_daddr_t daddr, struct buf **bpp)
{
	struct buf *bp;
	struct mount *mp;
	struct ext2mount *ump;
	int error;

	mp = vp->v_mount;
	ump = VFSTOEXT2(mp);

	bp = getblk(vp, lbn, mp->mnt_stat.f_iosize, 0, 0, 0);
	if ((bp->b_flags & B_CACHE) == 0) {
		KASSERT(daddr != 0,
		    ("readindir: indirect block not in cache"));

		bp->b_blkno = blkptrtodb(ump, daddr);
		bp->b_iocmd = BIO_READ;
		bp->b_flags &= ~B_INVAL;
		bp->b_ioflags &= ~BIO_ERROR;
		vfs_busy_pages(bp, 0);
		bp->b_iooffset = dbtob(bp->b_blkno);
		bstrategy(bp);
#ifdef RACCT
		if (racct_enable) {
			PROC_LOCK(curproc);
			racct_add_buf(curproc, bp, 0);
			PROC_UNLOCK(curproc);
		}
#endif
		curthread->td_ru.ru_inblock++;
		error = bufwait(bp);
		if (error != 0) {
			brelse(bp);
			return (error);
		}
	}
	*bpp = bp;
	return (0);
}

/*
 * Indirect blocks are now on the vnode for the file.  They are given negative
 * logical block numbers.  Indirect blocks are addressed by the negative
 * address of the first data block to which they point.  Double indirect blocks
 * are addressed by one less than the address of the first indirect block to
 * which they point.  Triple indirect blocks are addressed by one less than
 * the address of the first double indirect block to which they point.
 *
 * ext2_bmaparray does the bmap conversion, and if requested returns the
 * array of logical blocks which must be traversed to get to a block.
 * Each entry contains the offset into that block that gets you to the
 * next block and the disk address of the block (if it is assigned).
 */

int
ext2_bmaparray(struct vnode *vp, daddr_t bn, daddr_t *bnp, int *runp, int *runb)
{
	struct inode *ip;
	struct buf *bp;
	struct ext2mount *ump;
	struct mount *mp;
	struct indir a[EXT2_NIADDR + 1], *ap;
	daddr_t daddr;
	e2fs_lbn_t metalbn;
	int error, num, maxrun = 0, bsize;
	int *nump;

	ap = NULL;
	ip = VTOI(vp);
	mp = vp->v_mount;
	ump = VFSTOEXT2(mp);

	bsize = EXT2_BLOCK_SIZE(ump->um_e2fs);

	if (runp) {
		maxrun = mp->mnt_iosize_max / bsize - 1;
		*runp = 0;
	}
	if (runb)
		*runb = 0;

	ap = a;
	nump = &num;
	error = ext2_getlbns(vp, bn, ap, nump);
	if (error)
		return (error);

	num = *nump;
	if (num == 0) {
		*bnp = blkptrtodb(ump, ip->i_db[bn]);
		if (*bnp == 0) {
			*bnp = -1;
		} else if (runp) {
			daddr_t bnb = bn;

			for (++bn; bn < EXT2_NDADDR && *runp < maxrun &&
			    is_sequential(ump, ip->i_db[bn - 1], ip->i_db[bn]);
			    ++bn, ++*runp);
			bn = bnb;
			if (runb && (bn > 0)) {
				for (--bn; (bn >= 0) && (*runb < maxrun) &&
					is_sequential(ump, ip->i_db[bn],
						ip->i_db[bn + 1]);
						--bn, ++*runb);
			}
		}
		return (0);
	}

	/* Get disk address out of indirect block array */
	daddr = ip->i_ib[ap->in_off];

	for (bp = NULL, ++ap; --num; ++ap) {
		/*
		 * Exit the loop if there is no disk address assigned yet and
		 * the indirect block isn't in the cache, or if we were
		 * looking for an indirect block and we've found it.
		 */

		metalbn = ap->in_lbn;
		if ((daddr == 0 && !incore(&vp->v_bufobj, metalbn)) || metalbn == bn)
			break;
		/*
		 * If we get here, we've either got the block in the cache
		 * or we have a disk address for it, go fetch it.
		 */
		if (bp)
			bqrelse(bp);
		error = readindir(vp, metalbn, daddr, &bp);
		if (error != 0)
			return (error);

		daddr = le32toh(((e2fs_daddr_t *)bp->b_data)[ap->in_off]);
		if (num == 1 && daddr && runp) {
			for (bn = ap->in_off + 1;
			    bn < MNINDIR(ump) && *runp < maxrun &&
			    is_sequential(ump,
			    ((e2fs_daddr_t *)bp->b_data)[bn - 1],
			    ((e2fs_daddr_t *)bp->b_data)[bn]);
			    ++bn, ++*runp);
			bn = ap->in_off;
			if (runb && bn) {
				for (--bn; bn >= 0 && *runb < maxrun &&
					is_sequential(ump,
					((e2fs_daddr_t *)bp->b_data)[bn],
					((e2fs_daddr_t *)bp->b_data)[bn + 1]);
					--bn, ++*runb);
			}
		}
	}
	if (bp)
		bqrelse(bp);

	*bnp = blkptrtodb(ump, daddr);
	if (*bnp == 0) {
		*bnp = -1;
	}
	return (0);
}

static e2fs_lbn_t
lbn_count(struct ext2mount *ump, int level)

{
	e2fs_lbn_t blockcnt;

	for (blockcnt = 1; level > 0; level--)
		blockcnt *= MNINDIR(ump);
	return (blockcnt);
}

int
ext2_bmap_seekdata(struct vnode *vp, off_t *offp)
{
	struct buf *bp;
	struct indir a[EXT2_NIADDR + 1], *ap;
	struct inode *ip;
	struct mount *mp;
	struct ext2mount *ump;
	e2fs_daddr_t bn, daddr, nextbn;
	uint64_t bsize;
	off_t numblks;
	int error, num, num1, off;

	bp = NULL;
	error = 0;
	ip = VTOI(vp);
	mp = vp->v_mount;
	ump = VFSTOEXT2(mp);

	if (vp->v_type != VREG)
		return (EINVAL);
	if (*offp < 0 || *offp >= ip->i_size)
		return (ENXIO);

	bsize = mp->mnt_stat.f_iosize;
	for (bn = *offp / bsize, numblks = howmany(ip->i_size, bsize);
	    bn < numblks; bn = nextbn) {
		if (bn < EXT2_NDADDR) {
			daddr = ip->i_db[bn];
			if (daddr != 0)
				break;
			nextbn = bn + 1;
			continue;
		}

		ap = a;
		error = ext2_getlbns(vp, bn, ap, &num);
		if (error != 0)
			break;
		MPASS(num >= 2);
		daddr = ip->i_ib[ap->in_off];
		ap++, num--;
		for (nextbn = EXT2_NDADDR, num1 = num - 1; num1 > 0; num1--)
			nextbn += lbn_count(ump, num1);
		if (daddr == 0) {
			nextbn += lbn_count(ump, num);
			continue;
		}

		for (; daddr != 0 && num > 0; ap++, num--) {
			if (bp != NULL)
				bqrelse(bp);
			error = readindir(vp, ap->in_lbn, daddr, &bp);
			if (error != 0)
				return (error);

			/*
			 * Scan the indirect block until we find a non-zero
			 * pointer.
			 */
			off = ap->in_off;
			do {
				daddr = le32toh(((e2fs_daddr_t *)bp->b_data)[off]);
			} while (daddr == 0 && ++off < MNINDIR(ump));
			nextbn += off * lbn_count(ump, num - 1);

			/*
			 * We need to recompute the LBNs of indirect
			 * blocks, so restart with the updated block offset.
			 */
			if (off != ap->in_off)
				break;
		}
		if (num == 0) {
			/*
			 * We found a data block.
			 */
			bn = nextbn;
			break;
		}
	}
	if (bp != NULL)
		bqrelse(bp);
	if (bn >= numblks)
		error = ENXIO;
	if (error == 0 && *offp < bn * bsize)
		*offp = bn * bsize;
	return (error);
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
ext2_getlbns(struct vnode *vp, daddr_t bn, struct indir *ap, int *nump)
{
	long blockcnt;
	e2fs_lbn_t metalbn, realbn;
	struct ext2mount *ump;
	int i, numlevels, off;
	int64_t qblockcnt;

	ump = VFSTOEXT2(vp->v_mount);
	if (nump)
		*nump = 0;
	numlevels = 0;
	realbn = bn;
	if ((long)bn < 0)
		bn = -(long)bn;

	/* The first EXT2_NDADDR blocks are direct blocks. */
	if (bn < EXT2_NDADDR)
		return (0);

	/*
	 * Determine the number of levels of indirection.  After this loop
	 * is done, blockcnt indicates the number of data blocks possible
	 * at the previous level of indirection, and EXT2_NIADDR - i is the
	 * number of levels of indirection needed to locate the requested block.
	 */
	for (blockcnt = 1, i = EXT2_NIADDR, bn -= EXT2_NDADDR; ;
	    i--, bn -= blockcnt) {
		if (i == 0)
			return (EFBIG);
		/*
		 * Use int64_t's here to avoid overflow for triple indirect
		 * blocks when longs have 32 bits and the block size is more
		 * than 4K.
		 */
		qblockcnt = (int64_t)blockcnt * MNINDIR(ump);
		if (bn < qblockcnt)
			break;
		blockcnt = qblockcnt;
	}

	/* Calculate the address of the first meta-block. */
	if (realbn >= 0)
		metalbn = -(realbn - bn + EXT2_NIADDR - i);
	else
		metalbn = -(-realbn - bn + EXT2_NIADDR - i);

	/*
	 * At each iteration, off is the offset into the bap array which is
	 * an array of disk addresses at the current level of indirection.
	 * The logical block number and the offset in that block are stored
	 * into the argument array.
	 */
	ap->in_lbn = metalbn;
	ap->in_off = off = EXT2_NIADDR - i;
	ap++;
	for (++numlevels; i <= EXT2_NIADDR; i++) {
		/* If searching for a meta-data block, quit when found. */
		if (metalbn == realbn)
			break;

		off = (bn / blockcnt) % MNINDIR(ump);

		++numlevels;
		ap->in_lbn = metalbn;
		ap->in_off = off;
		++ap;

		metalbn -= -1 + off * blockcnt;
		blockcnt /= MNINDIR(ump);
	}
	if (nump)
		*nump = numlevels;
	return (0);
}
