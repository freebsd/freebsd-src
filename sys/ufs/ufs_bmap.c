/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)ufs_bmap.c	7.13 (Berkeley) 5/8/91
 *	$Id: ufs_bmap.c,v 1.3 1993/11/25 01:38:30 wollman Exp $
 */

#include "param.h"
#include "systm.h"
#include "buf.h"
#include "proc.h"
#include "file.h"
#include "vnode.h"

#include "quota.h"
#include "inode.h"
#include "fs.h"

/*
 * Bmap converts a the logical block number of a file
 * to its physical block number on the disk. The conversion
 * is done by using the logical block number to index into
 * the array of block pointers described by the dinode.
 */
int
bmap(ip, bn, bnp)
	register struct inode *ip;
	register daddr_t bn;
	daddr_t	*bnp;
{
	register struct fs *fs;
	register daddr_t nb;
	struct buf *bp;
	daddr_t *bap;
	int i, j, sh;
	int error;

	if (bn < 0)
		return (EFBIG);
	fs = ip->i_fs;

	/*
	 * The first NDADDR blocks are direct blocks
	 */
	if (bn < NDADDR) {
		nb = ip->i_db[bn];
		if (nb == 0) {
			*bnp = (daddr_t)-1;
			return (0);
		}
		*bnp = fsbtodb(fs, nb);
		return (0);
	}
	/*
	 * Determine the number of levels of indirection.
	 */
	sh = 1;
	bn -= NDADDR;
	for (j = NIADDR; j > 0; j--) {
		sh *= NINDIR(fs);
		if (bn < sh)
			break;
		bn -= sh;
	}
	if (j == 0)
		return (EFBIG);
	/*
	 * Fetch through the indirect blocks.
	 */
	nb = ip->i_ib[NIADDR - j];
	if (nb == 0) {
		*bnp = (daddr_t)-1;
		return (0);
	}
	for (; j <= NIADDR; j++) {
		if (error = bread(ip->i_devvp, fsbtodb(fs, nb),
		    (int)fs->fs_bsize, NOCRED, &bp)) {
			brelse(bp);
			return (error);
		}
		bap = bp->b_un.b_daddr;
		sh /= NINDIR(fs);
		i = (bn / sh) % NINDIR(fs);
		nb = bap[i];
		if (nb == 0) {
			*bnp = (daddr_t)-1;
			brelse(bp);
			return (0);
		}
		brelse(bp);
	}
	*bnp = fsbtodb(fs, nb);
	return (0);
}

/*
 * Balloc defines the structure of file system storage
 * by allocating the physical blocks on a device given
 * the inode and the logical block number in a file.
 */
int
balloc(ip, bn, size, bpp, flags)
	register struct inode *ip;
	register daddr_t bn;
	int size;
	struct buf **bpp;
	int flags;
{
	register struct fs *fs;
	register daddr_t nb;
	struct buf *bp, *nbp;
	struct vnode *vp = ITOV(ip);
	int osize, nsize, i, j, sh, error;
	daddr_t newb, lbn, *bap, pref, blkpref();

	*bpp = (struct buf *)0;
	if (bn < 0)
		return (EFBIG);
	fs = ip->i_fs;

	/*
	 * If the next write will extend the file into a new block,
	 * and the file is currently composed of a fragment
	 * this fragment has to be extended to be a full block.
	 */
	nb = lblkno(fs, ip->i_size);
	if (nb < NDADDR && nb < bn) {
		osize = blksize(fs, ip, nb);
		if (osize < fs->fs_bsize && osize > 0) {
			error = realloccg(ip, nb,
				blkpref(ip, nb, (int)nb, &ip->i_db[0]),
				osize, (int)fs->fs_bsize, &bp);
			if (error)
				return (error);
			ip->i_size = (nb + 1) * fs->fs_bsize;
			vnode_pager_setsize(ITOV(ip), (u_long)ip->i_size);
			ip->i_db[nb] = dbtofsb(fs, bp->b_blkno);
			ip->i_flag |= IUPD|ICHG;
			if (flags & B_SYNC)
				bwrite(bp);
			else
				bawrite(bp);
		}
	}
	/*
	 * The first NDADDR blocks are direct blocks
	 */
	if (bn < NDADDR) {
		nb = ip->i_db[bn];
		if (nb != 0 && ip->i_size >= (bn + 1) * fs->fs_bsize) {
			error = bread(vp, bn, fs->fs_bsize, NOCRED, &bp);
			if (error) {
				brelse(bp);
				return (error);
			}
			*bpp = bp;
			return (0);
		}
		if (nb != 0) {
			/*
			 * Consider need to reallocate a fragment.
			 */
			osize = fragroundup(fs, blkoff(fs, ip->i_size));
			nsize = fragroundup(fs, size);
			if (nsize <= osize) {
				error = bread(vp, bn, osize, NOCRED, &bp);
				if (error) {
					brelse(bp);
					return (error);
				}
			} else {
				error = realloccg(ip, bn,
					blkpref(ip, bn, (int)bn, &ip->i_db[0]),
					osize, nsize, &bp);
				if (error)
					return (error);
			}
		} else {
			if (ip->i_size < (bn + 1) * fs->fs_bsize)
				nsize = fragroundup(fs, size);
			else
				nsize = fs->fs_bsize;
			error = alloc(ip, bn,
				blkpref(ip, bn, (int)bn, &ip->i_db[0]),
				nsize, &newb);
			if (error)
				return (error);
			bp = getblk(vp, bn, nsize);
			bp->b_blkno = fsbtodb(fs, newb);
			if (flags & B_CLRBUF)
				clrbuf(bp);
		}
		ip->i_db[bn] = dbtofsb(fs, bp->b_blkno);
		ip->i_flag |= IUPD|ICHG;
		*bpp = bp;
		return (0);
	}
	/*
	 * Determine the number of levels of indirection.
	 */
	pref = 0;
	sh = 1;
	lbn = bn;
	bn -= NDADDR;
	for (j = NIADDR; j > 0; j--) {
		sh *= NINDIR(fs);
		if (bn < sh)
			break;
		bn -= sh;
	}
	if (j == 0)
		return (EFBIG);
	/*
	 * Fetch the first indirect block allocating if necessary.
	 */
	nb = ip->i_ib[NIADDR - j];
	if (nb == 0) {
		pref = blkpref(ip, lbn, 0, (daddr_t *)0);
	        if (error = alloc(ip, lbn, pref, (int)fs->fs_bsize, &newb))
			return (error);
		nb = newb;
		bp = getblk(ip->i_devvp, fsbtodb(fs, nb), fs->fs_bsize);
		clrbuf(bp);
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
		if (error = bwrite(bp)) {
			blkfree(ip, nb, fs->fs_bsize);
			return (error);
		}
		ip->i_ib[NIADDR - j] = nb;
		ip->i_flag |= IUPD|ICHG;
	}
	/*
	 * Fetch through the indirect blocks, allocating as necessary.
	 */
	for (; ; j++) {
		error = bread(ip->i_devvp, fsbtodb(fs, nb),
		    (int)fs->fs_bsize, NOCRED, &bp);
		if (error) {
			brelse(bp);
			return (error);
		}
		bap = bp->b_un.b_daddr;
		sh /= NINDIR(fs);
		i = (bn / sh) % NINDIR(fs);
		nb = bap[i];
		if (j == NIADDR)
			break;
		if (nb != 0) {
			brelse(bp);
			continue;
		}
		if (pref == 0)
			pref = blkpref(ip, lbn, 0, (daddr_t *)0);
		if (error = alloc(ip, lbn, pref, (int)fs->fs_bsize, &newb)) {
			brelse(bp);
			return (error);
		}
		nb = newb;
		nbp = getblk(ip->i_devvp, fsbtodb(fs, nb), fs->fs_bsize);
		clrbuf(nbp);
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
		if (error = bwrite(nbp)) {
			blkfree(ip, nb, fs->fs_bsize);
			brelse(bp);
			return (error);
		}
		bap[i] = nb;
		/*
		 * If required, write synchronously, otherwise use
		 * delayed write. If this is the first instance of
		 * the delayed write, reassociate the buffer with the
		 * file so it will be written if the file is sync'ed.
		 */
		if (flags & B_SYNC) {
			bwrite(bp);
		} else if (bp->b_flags & B_DELWRI) {
			bdwrite(bp);
		} else {
			bdwrite(bp);
			reassignbuf(bp, vp);
		}
	}
	/*
	 * Get the data block, allocating if necessary.
	 */
	if (nb == 0) {
		pref = blkpref(ip, lbn, i, &bap[0]);
		if (error = alloc(ip, lbn, pref, (int)fs->fs_bsize, &newb)) {
			brelse(bp);
			return (error);
		}
		nb = newb;
		nbp = getblk(vp, lbn, fs->fs_bsize);
		nbp->b_blkno = fsbtodb(fs, nb);
		if (flags & B_CLRBUF)
			clrbuf(nbp);
		bap[i] = nb;
		/*
		 * If required, write synchronously, otherwise use
		 * delayed write. If this is the first instance of
		 * the delayed write, reassociate the buffer with the
		 * file so it will be written if the file is sync'ed.
		 */
		if (flags & B_SYNC) {
			bwrite(bp);
		} else if (bp->b_flags & B_DELWRI) {
			bdwrite(bp);
		} else {
			bdwrite(bp);
			reassignbuf(bp, vp);
		}
		*bpp = nbp;
		return (0);
	}
	brelse(bp);
	if (flags & B_CLRBUF) {
		error = bread(vp, lbn, (int)fs->fs_bsize, NOCRED, &nbp);
		if (error) {
			brelse(nbp);
			return (error);
		}
	} else {
		nbp = getblk(vp, lbn, fs->fs_bsize);
		nbp->b_blkno = fsbtodb(fs, nb);
	}
	*bpp = nbp;
	return (0);
}
