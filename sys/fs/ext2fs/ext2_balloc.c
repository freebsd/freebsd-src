/*-
 *  modified for Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
/*-
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *	@(#)ffs_balloc.c	8.4 (Berkeley) 9/23/93
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/lock.h>
#include <sys/ucred.h>
#include <sys/vnode.h>

#include <fs/ext2fs/inode.h>
#include <fs/ext2fs/ext2fs.h>
#include <fs/ext2fs/fs.h>
#include <fs/ext2fs/ext2_extern.h>
#include <fs/ext2fs/ext2_mount.h>
/*
 * Balloc defines the structure of file system storage
 * by allocating the physical blocks on a device given
 * the inode and the logical block number in a file.
 */
int
ext2_balloc(ip, lbn, size, cred, bpp, flags)
	struct inode *ip;
	int32_t lbn;
	int size;
	struct ucred *cred;
	struct buf **bpp;
	int flags;
{
	struct m_ext2fs *fs;
	struct ext2mount *ump;
	int32_t nb;
	struct buf *bp, *nbp;
	struct vnode *vp = ITOV(ip);
	struct indir indirs[NIADDR + 2];
	int32_t newb, *bap, pref;
	int osize, nsize, num, i, error;

	*bpp = NULL;
	if (lbn < 0)
		return (EFBIG);
	fs = ip->i_e2fs;
	ump = ip->i_ump;

	/*
	 * check if this is a sequential block allocation. 
	 * If so, increment next_alloc fields to allow ext2_blkpref 
	 * to make a good guess
	 */
        if (lbn == ip->i_next_alloc_block + 1) {
		ip->i_next_alloc_block++;
		ip->i_next_alloc_goal++;
	}

	/*
	 * The first NDADDR blocks are direct blocks
	 */
	if (lbn < NDADDR) {
		nb = ip->i_db[lbn];
		/* no new block is to be allocated, and no need to expand
		   the file */
		if (nb != 0 && ip->i_size >= (lbn + 1) * fs->e2fs_bsize) {
			error = bread(vp, lbn, fs->e2fs_bsize, NOCRED, &bp);
			if (error) {
				brelse(bp);
				return (error);
			}
			bp->b_blkno = fsbtodb(fs, nb);
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
				error = bread(vp, lbn, osize, NOCRED, &bp);
				if (error) {
					brelse(bp);
					return (error);
				}
				bp->b_blkno = fsbtodb(fs, nb);
			} else {
			/* Godmar thinks: this shouldn't happen w/o fragments */
				printf("nsize %d(%d) > osize %d(%d) nb %d\n", 
					(int)nsize, (int)size, (int)osize, 
					(int)ip->i_size, (int)nb);
				panic(
				    "ext2_balloc: Something is terribly wrong");
/*
 * please note there haven't been any changes from here on -
 * FFS seems to work.
 */
			}
		} else {
			if (ip->i_size < (lbn + 1) * fs->e2fs_bsize)
				nsize = fragroundup(fs, size);
			else
				nsize = fs->e2fs_bsize;
			EXT2_LOCK(ump);
			error = ext2_alloc(ip, lbn,
			    ext2_blkpref(ip, lbn, (int)lbn, &ip->i_db[0], 0),
			    nsize, cred, &newb);
			if (error)
				return (error);
			bp = getblk(vp, lbn, nsize, 0, 0, 0);
			bp->b_blkno = fsbtodb(fs, newb);
			if (flags & B_CLRBUF)
				vfs_bio_clrbuf(bp);
		}
		ip->i_db[lbn] = dbtofsb(fs, bp->b_blkno);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		*bpp = bp;
		return (0);
	}
	/*
	 * Determine the number of levels of indirection.
	 */
	pref = 0;
	if ((error = ext2_getlbns(vp, lbn, indirs, &num)) != 0)
		return(error);
#ifdef DIAGNOSTIC
	if (num < 1)
		panic ("ext2_balloc: ext2_getlbns returned indirect block");
#endif
	/*
	 * Fetch the first indirect block allocating if necessary.
	 */
	--num;
	nb = ip->i_ib[indirs[0].in_off];
	if (nb == 0) {
		EXT2_LOCK(ump);
		pref = ext2_blkpref(ip, lbn, indirs[0].in_off + 
					     EXT2_NDIR_BLOCKS, &ip->i_db[0], 0);
	        if ((error = ext2_alloc(ip, lbn, pref, 
			(int)fs->e2fs_bsize, cred, &newb)))
			return (error);
		nb = newb;
		bp = getblk(vp, indirs[1].in_lbn, fs->e2fs_bsize, 0, 0, 0);
		bp->b_blkno = fsbtodb(fs, newb);
		vfs_bio_clrbuf(bp);
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
		if ((error = bwrite(bp)) != 0) {
			ext2_blkfree(ip, nb, fs->e2fs_bsize);
			return (error);
		}
		ip->i_ib[indirs[0].in_off] = newb;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}
	/*
	 * Fetch through the indirect blocks, allocating as necessary.
	 */
	for (i = 1;;) {
		error = bread(vp,
		    indirs[i].in_lbn, (int)fs->e2fs_bsize, NOCRED, &bp);
		if (error) {
			brelse(bp);
			return (error);
		}
		bap = (int32_t *)bp->b_data;
		nb = bap[indirs[i].in_off];
		if (i == num)
			break;
		i += 1;
		if (nb != 0) {
			bqrelse(bp);
			continue;
		}
		EXT2_LOCK(ump);
		if (pref == 0)
			pref = ext2_blkpref(ip, lbn, indirs[i].in_off, bap,
						bp->b_lblkno);
		error =  ext2_alloc(ip, lbn, pref, (int)fs->e2fs_bsize, cred, &newb);
		if (error) {
			brelse(bp);
			return (error);
		}
		nb = newb;
		nbp = getblk(vp, indirs[i].in_lbn, fs->e2fs_bsize, 0, 0, 0);
		nbp->b_blkno = fsbtodb(fs, nb);
		vfs_bio_clrbuf(nbp);
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
		if ((error = bwrite(nbp)) != 0) {
			ext2_blkfree(ip, nb, fs->e2fs_bsize);
			EXT2_UNLOCK(ump);
			brelse(bp);
			return (error);
		}
		bap[indirs[i - 1].in_off] = nb;
		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */
		if (flags & B_SYNC) {
			bwrite(bp);
		} else {
			if (bp->b_bufsize == fs->e2fs_bsize)
				bp->b_flags |= B_CLUSTEROK;
			bdwrite(bp);
		}
	}
	/*
	 * Get the data block, allocating if necessary.
	 */
	if (nb == 0) {
		EXT2_LOCK(ump);
		pref = ext2_blkpref(ip, lbn, indirs[i].in_off, &bap[0], 
				bp->b_lblkno);
		if ((error = ext2_alloc(ip,
		    lbn, pref, (int)fs->e2fs_bsize, cred, &newb)) != 0) {
			brelse(bp);
			return (error);
		}
		nb = newb;
		nbp = getblk(vp, lbn, fs->e2fs_bsize, 0, 0, 0);
		nbp->b_blkno = fsbtodb(fs, nb);
		if (flags & B_CLRBUF)
			vfs_bio_clrbuf(nbp);
		bap[indirs[i].in_off] = nb;
		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */
		if (flags & B_SYNC) {
			bwrite(bp);
		} else {
		if (bp->b_bufsize == fs->e2fs_bsize)
				bp->b_flags |= B_CLUSTEROK;
			bdwrite(bp);
		}
		*bpp = nbp;
		return (0);
	}
	brelse(bp);
	if (flags & B_CLRBUF) {
		error = bread(vp, lbn, (int)fs->e2fs_bsize, NOCRED, &nbp);
		if (error) {
			brelse(nbp);
			return (error);
		}
	} else {
		nbp = getblk(vp, lbn, fs->e2fs_bsize, 0, 0, 0);
		nbp->b_blkno = fsbtodb(fs, nb);
	}
	*bpp = nbp;
	return (0);
}

