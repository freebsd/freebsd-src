/*-
 *  modified for Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
#include <sys/endian.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mount.h>
#include <sys/sdt.h>
#include <sys/vnode.h>

#include <fs/ext2fs/fs.h>
#include <fs/ext2fs/inode.h>
#include <fs/ext2fs/ext2fs.h>
#include <fs/ext2fs/ext2_dinode.h>
#include <fs/ext2fs/ext2_extern.h>
#include <fs/ext2fs/ext2_mount.h>

SDT_PROVIDER_DECLARE(ext2fs);

/*
 * ext2_alloc.c defines this probe; here it is only used.  Without
 * KDTRACE_HOOKS every SDT macro expands to nothing, so the missing
 * declaration is invisible in an ordinary build and only breaks a kernel
 * built with tracing hooks, where the probe name is a real symbol.
 */
SDT_PROBE_DECLARE(ext2fs, , alloc, trace);

static int
ext2_ext_balloc(struct inode *ip, uint32_t lbn, int size,
    struct ucred *cred, struct buf **bpp, int flags)
{
	struct m_ext2fs *fs;
	struct buf *bp = NULL;
	struct vnode *vp = ITOV(ip);
	daddr_t newblk;
	int blks, error, allocated;

	fs = ip->i_e2fs;
	blks = howmany(size, fs->e2fs_bsize);

	error = ext4_ext_get_blocks(ip, lbn, blks, cred, NULL, &allocated, &newblk);
	if (error)
		return (error);

	if (allocated) {
		bp = getblk(vp, lbn, fs->e2fs_bsize, 0, 0, 0);
		if(!bp)
			return (EIO);
	} else {
		error = bread(vp, lbn, fs->e2fs_bsize, NOCRED, &bp);
		if (error) {
			return (error);
		}
	}

	bp->b_blkno = fsbtodb(fs, newblk);
	if (flags & BA_CLRBUF)
		vfs_bio_clrbuf(bp);

	*bpp = bp;

	return (error);
}

/*
 * Balloc defines the structure of filesystem storage
 * by allocating the physical blocks on a device given
 * the inode and the logical block number in a file.
 */
int
ext2_balloc(struct inode *ip, e2fs_lbn_t lbn, int size, struct ucred *cred,
    struct buf **bpp, int flags)
{
	struct m_ext2fs *fs;
	struct ext2mount *ump;
	struct buf *bp, *nbp;
	struct vnode *vp = ITOV(ip);
	struct indir indirs[EXT2_NIADDR + 2];
	struct ext2_alloc_context ctx;
	e2fs_daddr_t *bap;
	e4fs_daddr_t bpref, pref, newb;
	int num, i, error;

	*bpp = NULL;
	if (lbn < 0)
		return (EFBIG);
	fs = ip->i_e2fs;
	ump = ip->i_ump;

	/*
	 * check if this is a sequential block allocation.
	 * If so, increment i_next_alloc_block to allow ext2_blkpref
	 * to make a good guess.  i_next_alloc_goal is left to
	 * ext2_commit_allocated_block(), which points it past the run
	 * that was just allocated.
	 */
	if (lbn == ip->i_next_alloc_block + 1)
		ip->i_next_alloc_block++;

	if (ip->i_flag & IN_E4EXTENTS)
		return (ext2_ext_balloc(ip, lbn, size, cred, bpp, flags));

	/*
	 * The first EXT2_NDADDR blocks are direct blocks
	 */
	if (lbn < EXT2_NDADDR) {
		pref = ip->i_db[lbn];
		if (pref != 0) {
			error = bread(vp, lbn, fs->e2fs_bsize, NOCRED, &bp);
			if (error) {
				return (error);
			}
			bp->b_blkno = fsbtodb(fs, pref);
			if (ip->i_size >= (lbn + 1) * fs->e2fs_bsize) {
				*bpp = bp;
				return (0);
			}
		} else {
			EXT2_LOCK(ump);
			error = ext2_alloc(ip, lbn,
			    ext2_blkpref(ip, lbn, (int)lbn, &ip->i_db[0], 0),
			    fs->e2fs_bsize, cred, &newb);
			if (error)
				return (error);
			/*
			 * If the newly allocated block exceeds 32-bit limit,
			 * we can not use it in file block maps.
			 */
			if (newb > UINT_MAX) {
				/* Not installed in i_db[] yet, so still free. */
				ext2_rollback_unpublished(ip, newb, 1);
				return (EFBIG);
			}
			bp = getblk(vp, lbn, fs->e2fs_bsize, 0, 0, 0);
			bp->b_blkno = fsbtodb(fs, newb);
			if (flags & BA_CLRBUF)
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
	bpref = 0;
	if ((error = ext2_getlbns(vp, lbn, indirs, &num)) != 0)
		return (error);
#ifdef INVARIANTS
	if (num < 1)
		panic("ext2_balloc: ext2_getlbns returned indirect block");
#endif
	/*
	 * Fetch the first indirect block allocating if necessary.
	 */
	--num;
	pref = ip->i_ib[indirs[0].in_off];
	if (pref == 0) {
		EXT2_LOCK(ump);
		bpref = ext2_blkpref(ip, lbn, indirs[0].in_off +
		    EXT2_NDIR_BLOCKS, &ip->i_db[0], 0);
		error = ext2_alloc(ip, lbn, bpref, fs->e2fs_bsize, cred,
		    &newb);
		if (error)
			return (error);
		if (newb > UINT_MAX) {
			/* Not installed in i_ib[] yet, so still free. */
			ext2_rollback_unpublished(ip, newb, 1);
			return (EFBIG);
		}
		pref = newb;
		bp = getblk(vp, indirs[1].in_lbn, fs->e2fs_bsize, 0, 0, 0);
		bp->b_blkno = fsbtodb(fs, pref);
		vfs_bio_clrbuf(bp);
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
		if ((error = bwrite(bp)) != 0) {
			/* bwrite() released bp; pref is still unreferenced. */
			ext2_rollback_unpublished(ip, pref, 1);
			return (error);
		}
		ip->i_ib[indirs[0].in_off] = pref;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}
	/*
	 * Fetch through the indirect blocks, allocating as necessary.
	 */
	for (i = 1;;) {
		error = bread(vp,
		    indirs[i].in_lbn, (int)fs->e2fs_bsize, NOCRED, &bp);
		if (error) {
			return (error);
		}
		bap = (e2fs_daddr_t *)bp->b_data;
		pref = le32toh(bap[indirs[i].in_off]);
		if (i == num)
			break;
		i += 1;
		if (pref != 0) {
			bqrelse(bp);
			continue;
		}
		EXT2_LOCK(ump);
		if (bpref == 0)
			bpref = ext2_blkpref(ip, lbn, indirs[i].in_off, bap,
			    bp->b_lblkno);
		error = ext2_alloc(ip, lbn, bpref, (int)fs->e2fs_bsize, cred, &newb);
		if (error) {
			brelse(bp);
			return (error);
		}
		if (newb > UINT_MAX) {
			/* Not installed in bap[] yet, so still free. */
			ext2_rollback_unpublished(ip, newb, 1);
			brelse(bp);
			return (EFBIG);
		}
		pref = newb;
		nbp = getblk(vp, indirs[i].in_lbn, fs->e2fs_bsize, 0, 0, 0);
		nbp->b_blkno = fsbtodb(fs, pref);
		vfs_bio_clrbuf(nbp);
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
		if ((error = bwrite(nbp)) != 0) {
			/* bwrite() released nbp; pref is still unreferenced. */
			ext2_rollback_unpublished(ip, pref, 1);
			brelse(bp);
			return (error);
		}
		bap[indirs[i - 1].in_off] = htole32(pref);
		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.  bwrite() and bdwrite() both release
		 * 'bp' internally; the loop re-assigns bp via bread()
		 * at the top of the next iteration.
		 */
		if (flags & IO_SYNC) {
			if ((error = bwrite(bp)) != 0) {
				/*
				 * The child indirect block is on disk but
				 * unreachable: the parent that names it is not.
				 */
				ext2_rollback_unpublished(ip, pref, 1);
				bp = NULL;
				return (error);
			}
		} else {
			if (bp->b_bufsize == fs->e2fs_bsize)
				bp->b_flags |= B_CLUSTEROK;
			bdwrite(bp);
			bp = NULL;
		}
	}
	/*
	 * Get the data block, allocating if necessary.
	 */
	if (pref == 0) {
		/*
		 * The traditional path maps one logical block per call, so
		 * it asks for a single block: a longer run would be blocks
		 * this call cannot install into the indirect array.
		 */
		EXT2_LOCK(ump);
		if (fs->e2fs_fbcount == 0)
			goto nospace;
		bpref = ext2_blkpref(ip, lbn, indirs[i].in_off, &bap[0],
		    bp->b_lblkno);
		/* Returns with EXT2_LOCK held, also on failure. */
		error = ext2_alloc_run(ip, lbn, 1, bpref,
		    EXT2_ALLOC_DATA_RAND, cred, &ctx);
		if (error)
			goto nospace;
		pref = ctx.run.par_physical_start;
		ext2_commit_allocated_block(ip, &ctx);

		/*
		 * Drop the lock before any buffer I/O: getblk(),
		 * vfs_bio_clrbuf(), bwrite() and bdwrite() can sleep, and
		 * the inode accounting above is already done.
		 */
		EXT2_UNLOCK(ump);

		if (pref > UINT_MAX) {
			/* Not installed in bap[] yet, so still free. */
			ext2_rollback_allocation(&ctx);
			brelse(bp);
			return (EFBIG);
		}
		nbp = getblk(vp, lbn, fs->e2fs_bsize, 0, 0, 0);
		nbp->b_blkno = fsbtodb(fs, pref);
		if (flags & BA_CLRBUF)
			vfs_bio_clrbuf(nbp);
		bap[indirs[i].in_off] = htole32(pref);
		ext2_alloc_transition(&ctx, EXT2_ALLOC_MAPPED);
		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */
		if (flags & IO_SYNC) {
			if ((error = bwrite(bp)) != 0) {
				/*
				 * The parent indirect block did not reach
				 * the disk, so the run is still unpublished
				 * and can be rolled back.
				 */
				ext2_rollback_allocation(&ctx);
				brelse(nbp);
				return (error);
			}
			ext2_alloc_transition(&ctx, EXT2_ALLOC_PUBLISHED);
		} else {
			if (bp->b_bufsize == fs->e2fs_bsize)
				bp->b_flags |= B_CLUSTEROK;
			bdwrite(bp);
		}
		*bpp = nbp;
		return (0);
	}
	brelse(bp);
	if (flags & BA_CLRBUF) {
		int seqcount = (flags & BA_SEQMASK) >> BA_SEQSHIFT;

		if (seqcount && (vp->v_mount->mnt_flag & MNT_NOCLUSTERR) == 0) {
			error = cluster_read(vp, ip->i_size, lbn,
			    (int)fs->e2fs_bsize, NOCRED,
			    MAXBSIZE, seqcount, 0, &nbp);
		} else {
			error = bread(vp, lbn, (int)fs->e2fs_bsize, NOCRED, &nbp);
		}
		if (error) {
			brelse(nbp);
			return (error);
		}
	} else {
		nbp = getblk(vp, lbn, fs->e2fs_bsize, 0, 0, 0);
		nbp->b_blkno = fsbtodb(fs, pref);
	}
	*bpp = nbp;
	return (0);
nospace:
	/* Both paths into this label leave EXT2_LOCK held. */
	brelse(bp);
	EXT2_UNLOCK(ump);
	SDT_PROBE2(ext2fs, , alloc, trace, 1, "cannot allocate data block");
	return (ENOSPC);
}
