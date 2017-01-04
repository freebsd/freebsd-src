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
 *	@(#)ffs_subr.c	8.2 (Berkeley) 9/21/93
 * $FreeBSD$
 */

#include <sys/param.h>

#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/lock.h>
#include <sys/ucred.h>
#include <sys/vnode.h>

#include <fs/ext2fs/inode.h>
#include <fs/ext2fs/ext2_extern.h>
#include <fs/ext2fs/ext2fs.h>
#include <fs/ext2fs/fs.h>
#include <fs/ext2fs/ext2_extents.h>
#include <fs/ext2fs/ext2_mount.h>
#include <fs/ext2fs/ext2_dinode.h>

#ifdef KDB
void	ext2_checkoverlap(struct buf *, struct inode *);
#endif

/*
 * Return buffer with the contents of block "offset" from the beginning of
 * directory "ip".  If "res" is non-zero, fill it in with a pointer to the
 * remaining space in the directory.
 */
int
ext2_blkatoff(struct vnode *vp, off_t offset, char **res, struct buf **bpp)
{
	struct inode *ip;
	struct m_ext2fs *fs;
	struct buf *bp;
	e2fs_lbn_t lbn;
	int bsize, error;
	daddr_t newblk;
	struct ext4_extent *ep;
	struct ext4_extent_path path;

	ip = VTOI(vp);
	fs = ip->i_e2fs;
	lbn = lblkno(fs, offset);
	bsize = blksize(fs, ip, lbn);
	*bpp = NULL;

	/*
	 * IN_E4EXTENTS requires special treatment as we can otherwise fall
	 * back to the normal path.
	 */
	if (!(ip->i_flag & IN_E4EXTENTS))
		goto normal;

	memset(&path, 0, sizeof(path));
	if (ext4_ext_find_extent(fs, ip, lbn, &path) == NULL)
		goto normal;
	ep = path.ep_ext;
	if (ep == NULL)
		goto normal;

	newblk = lbn - ep->e_blk +
	    (ep->e_start_lo | (daddr_t)ep->e_start_hi << 32);

	if (path.ep_bp != NULL) {
		brelse(path.ep_bp);
		path.ep_bp = NULL;
	}
	error = bread(ip->i_devvp, fsbtodb(fs, newblk), bsize, NOCRED, &bp);
	if (error != 0) {
		brelse(bp);
		return (error);
	}
	if (res)
		*res = (char *)bp->b_data + blkoff(fs, offset);
	/*
	 * If IN_E4EXTENTS is enabled we would get a wrong offset so
	 * reset b_offset here.
	 */
	bp->b_offset = lbn * bsize;
	*bpp = bp;
	return (0);

normal:
	if (*bpp == NULL) {
		if ((error = bread(vp, lbn, bsize, NOCRED, &bp)) != 0) {
			brelse(bp);
			return (error);
		}
		if (res)
			*res = (char *)bp->b_data + blkoff(fs, offset);
		*bpp = bp;
	}
	return (0);
}

#ifdef KDB
void
ext2_checkoverlap(struct buf *bp, struct inode *ip)
{
	struct buf *ebp, *ep;
	e4fs_daddr_t start, last;
	struct vnode *vp;

	ebp = &buf[nbuf];
	start = bp->b_blkno;
	last = start + btodb(bp->b_bcount) - 1;
	for (ep = buf; ep < ebp; ep++) {
		if (ep == bp || (ep->b_flags & B_INVAL))
			continue;
		vp = ip->i_ump->um_devvp;
		/* look for overlap */
		if (ep->b_bcount == 0 || ep->b_blkno > last ||
		    ep->b_blkno + btodb(ep->b_bcount) <= start)
			continue;
		vprint("Disk overlap", vp);
		printf("\tstart %jd, end %jd overlap start %jd, end %jd\n",
		    (intmax_t)start, (intmax_t)last, (intmax_t)ep->b_blkno,
		    (intmax_t)(ep->b_blkno + btodb(ep->b_bcount) - 1));
		panic("ext2_checkoverlap: Disk buffer overlap");
	}
}
#endif /* KDB */

/*
 * Update the cluster map because of an allocation of free like ffs.
 *
 * Cnt == 1 means free; cnt == -1 means allocating.
 */
void
ext2_clusteracct(struct m_ext2fs *fs, char *bbp, int cg, daddr_t bno, int cnt)
{
	int32_t *sump = fs->e2fs_clustersum[cg].cs_sum;
	int32_t *lp;
	int back, bit, end, forw, i, loc, start;

	/* Initialize the cluster summary array. */
	if (fs->e2fs_clustersum[cg].cs_init == 0) {
		int run = 0;

		bit = 1;
		loc = 0;

		for (i = 0; i < fs->e2fs->e2fs_fpg; i++) {
			if ((bbp[loc] & bit) == 0)
				run++;
			else if (run != 0) {
				if (run > fs->e2fs_contigsumsize)
					run = fs->e2fs_contigsumsize;
				sump[run]++;
				run = 0;
			}
			if ((i & (NBBY - 1)) != (NBBY - 1))
				bit <<= 1;
			else {
				loc++;
				bit = 1;
			}
		}
		if (run != 0) {
			if (run > fs->e2fs_contigsumsize)
				run = fs->e2fs_contigsumsize;
			sump[run]++;
		}
		fs->e2fs_clustersum[cg].cs_init = 1;
	}

	if (fs->e2fs_contigsumsize <= 0)
		return;

	/* Find the size of the cluster going forward. */
	start = bno + 1;
	end = start + fs->e2fs_contigsumsize;
	if (end > fs->e2fs->e2fs_fpg)
		end = fs->e2fs->e2fs_fpg;
	loc = start / NBBY;
	bit = 1 << (start % NBBY);
	for (i = start; i < end; i++) {
		if ((bbp[loc] & bit) != 0)
			break;
		if ((i & (NBBY - 1)) != (NBBY - 1))
			bit <<= 1;
		else {
			loc++;
			bit = 1;
		}
	}
	forw = i - start;

	/* Find the size of the cluster going backward. */
	start = bno - 1;
	end = start - fs->e2fs_contigsumsize;
	if (end < 0)
		end = -1;
	loc = start / NBBY;
	bit = 1 << (start % NBBY);
	for (i = start; i > end; i--) {
		if ((bbp[loc] & bit) != 0)
			break;
		if ((i & (NBBY - 1)) != 0)
			bit >>= 1;
		else {
			loc--;
			bit = 1 << (NBBY - 1);
		}
	}
	back = start - i;

	/*
	 * Account for old cluster and the possibly new forward and
	 * back clusters.
	 */
	i = back + forw + 1;
	if (i > fs->e2fs_contigsumsize)
		i = fs->e2fs_contigsumsize;
	sump[i] += cnt;
	if (back > 0)
		sump[back] -= cnt;
	if (forw > 0)
		sump[forw] -= cnt;

	/* Update cluster summary information. */
	lp = &sump[fs->e2fs_contigsumsize];
	for (i = fs->e2fs_contigsumsize; i > 0; i--)
		if (*lp-- > 0)
			break;
	fs->e2fs_maxcluster[cg] = i;
}
