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
 *	@(#)ffs_inode.c	8.13 (Berkeley) 4/21/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_quota.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/resourcevar.h>
#include <sys/vmmeter.h>
#include <sys/stat.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

static int ffs_indirtrunc(struct inode *, ufs2_daddr_t, ufs2_daddr_t,
	    ufs2_daddr_t, int, ufs2_daddr_t *);

/*
 * Update the access, modified, and inode change times as specified by the
 * IN_ACCESS, IN_UPDATE, and IN_CHANGE flags respectively.  Write the inode
 * to disk if the IN_MODIFIED flag is set (it may be set initially, or by
 * the timestamp update).  The IN_LAZYMOD flag is set to force a write
 * later if not now.  If we write now, then clear both IN_MODIFIED and
 * IN_LAZYMOD to reflect the presumably successful write, and if waitfor is
 * set, then wait for the write to complete.
 */
int
ffs_update(vp, waitfor)
	struct vnode *vp;
	int waitfor;
{
	struct fs *fs;
	struct buf *bp;
	struct inode *ip;
	int error;

#ifdef DEBUG_VFS_LOCKS
	if ((vp->v_iflag & VI_XLOCK) == 0)
		ASSERT_VOP_LOCKED(vp, "ffs_update");
#endif
	ufs_itimes(vp);
	ip = VTOI(vp);
	if ((ip->i_flag & IN_MODIFIED) == 0 && waitfor == 0)
		return (0);
	ip->i_flag &= ~(IN_LAZYMOD | IN_MODIFIED);
	fs = ip->i_fs;
	if (fs->fs_ronly)
		return (0);
	/*
	 * Ensure that uid and gid are correct. This is a temporary
	 * fix until fsck has been changed to do the update.
	 */
	if (fs->fs_magic == FS_UFS1_MAGIC &&		/* XXX */
	    fs->fs_old_inodefmt < FS_44INODEFMT) {	/* XXX */
		ip->i_din1->di_ouid = ip->i_uid;	/* XXX */
		ip->i_din1->di_ogid = ip->i_gid;	/* XXX */
	}						/* XXX */
	error = bread(ip->i_devvp, fsbtodb(fs, ino_to_fsba(fs, ip->i_number)),
		(int)fs->fs_bsize, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return (error);
	}
	if (DOINGSOFTDEP(vp))
		softdep_update_inodeblock(ip, bp, waitfor);
	else if (ip->i_effnlink != ip->i_nlink)
		panic("ffs_update: bad link cnt");
	if (ip->i_ump->um_fstype == UFS1)
		*((struct ufs1_dinode *)bp->b_data +
		    ino_to_fsbo(fs, ip->i_number)) = *ip->i_din1;
	else
		*((struct ufs2_dinode *)bp->b_data +
		    ino_to_fsbo(fs, ip->i_number)) = *ip->i_din2;
	if (waitfor && !DOINGASYNC(vp)) {
		return (bwrite(bp));
	} else if (vm_page_count_severe() || buf_dirty_count_severe()) {
		return (bwrite(bp));
	} else {
		if (bp->b_bufsize == fs->fs_bsize)
			bp->b_flags |= B_CLUSTEROK;
		bdwrite(bp);
		return (0);
	}
}

#define	SINGLE	0	/* index of single indirect block */
#define	DOUBLE	1	/* index of double indirect block */
#define	TRIPLE	2	/* index of triple indirect block */
/*
 * Truncate the inode oip to at most length size, freeing the
 * disk blocks.
 */
int
ffs_truncate(vp, length, flags, cred, td)
	struct vnode *vp;
	off_t length;
	int flags;
	struct ucred *cred;
	struct thread *td;
{
	struct vnode *ovp = vp;
	struct inode *oip;
	ufs2_daddr_t bn, lbn, lastblock, lastiblock[NIADDR], indir_lbn[NIADDR];
	ufs2_daddr_t oldblks[NDADDR + NIADDR], newblks[NDADDR + NIADDR];
	ufs2_daddr_t count, blocksreleased = 0, datablocks;
	struct fs *fs;
	struct buf *bp;
	struct ufsmount *ump;
	int needextclean, softdepslowdown, extblocks;
	int offset, size, level, nblocks;
	int i, error, allerror;
	off_t osize;

	oip = VTOI(ovp);
	fs = oip->i_fs;
	ump = oip->i_ump;

	if (length < 0)
		return (EINVAL);
	/*
	 * Historically clients did not have to specify which data
	 * they were truncating. So, if not specified, we assume
	 * traditional behavior, e.g., just the normal data.
	 */
	if ((flags & (IO_EXT | IO_NORMAL)) == 0)
		flags |= IO_NORMAL;
	/*
	 * If we are truncating the extended-attributes, and cannot
	 * do it with soft updates, then do it slowly here. If we are
	 * truncating both the extended attributes and the file contents
	 * (e.g., the file is being unlinked), then pick it off with
	 * soft updates below.
	 */
	needextclean = 0;
	softdepslowdown = DOINGSOFTDEP(ovp) && softdep_slowdown(ovp);
	extblocks = 0;
	datablocks = DIP(oip, i_blocks);
	if (fs->fs_magic == FS_UFS2_MAGIC && oip->i_din2->di_extsize > 0) {
		extblocks = btodb(fragroundup(fs, oip->i_din2->di_extsize));
		datablocks -= extblocks;
	}
	if ((flags & IO_EXT) && extblocks > 0) {
		if (DOINGSOFTDEP(ovp) && softdepslowdown == 0 && length == 0) {
			if ((flags & IO_NORMAL) == 0) {
				softdep_setup_freeblocks(oip, length, IO_EXT);
				return (0);
			}
			needextclean = 1;
		} else {
			if (length != 0)
				panic("ffs_truncate: partial trunc of extdata");
			if ((error = ffs_syncvnode(ovp, MNT_WAIT)) != 0)
				return (error);
			osize = oip->i_din2->di_extsize;
			oip->i_din2->di_blocks -= extblocks;
#ifdef QUOTA
			(void) chkdq(oip, -extblocks, NOCRED, 0);
#endif
			vinvalbuf(ovp, V_ALT, td, 0, 0);
			oip->i_din2->di_extsize = 0;
			for (i = 0; i < NXADDR; i++) {
				oldblks[i] = oip->i_din2->di_extb[i];
				oip->i_din2->di_extb[i] = 0;
			}
			oip->i_flag |= IN_CHANGE | IN_UPDATE;
			if ((error = ffs_update(ovp, 1)))
				return (error);
			for (i = 0; i < NXADDR; i++) {
				if (oldblks[i] == 0)
					continue;
				ffs_blkfree(ump, fs, oip->i_devvp, oldblks[i],
				    sblksize(fs, osize, i), oip->i_number);
			}
		}
	}
	if ((flags & IO_NORMAL) == 0)
		return (0);
	if (length > fs->fs_maxfilesize)
		return (EFBIG);
	if (ovp->v_type == VLNK &&
	    (oip->i_size < ovp->v_mount->mnt_maxsymlinklen ||
	     datablocks == 0)) {
#ifdef DIAGNOSTIC
		if (length != 0)
			panic("ffs_truncate: partial truncate of symlink");
#endif
		bzero(SHORTLINK(oip), (u_int)oip->i_size);
		oip->i_size = 0;
		DIP_SET(oip, i_size, 0);
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		if (needextclean)
			softdep_setup_freeblocks(oip, length, IO_EXT);
		return (ffs_update(ovp, 1));
	}
	if (oip->i_size == length) {
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		if (needextclean)
			softdep_setup_freeblocks(oip, length, IO_EXT);
		return (ffs_update(ovp, 0));
	}
	if (fs->fs_ronly)
		panic("ffs_truncate: read-only filesystem");
#ifdef QUOTA
	error = getinoquota(oip);
	if (error)
		return (error);
#endif
	if ((oip->i_flags & SF_SNAPSHOT) != 0)
		ffs_snapremove(ovp);
	ovp->v_lasta = ovp->v_clen = ovp->v_cstart = ovp->v_lastw = 0;
	if (DOINGSOFTDEP(ovp)) {
		if (length > 0 || softdepslowdown) {
			/*
			 * If a file is only partially truncated, then
			 * we have to clean up the data structures
			 * describing the allocation past the truncation
			 * point. Finding and deallocating those structures
			 * is a lot of work. Since partial truncation occurs
			 * rarely, we solve the problem by syncing the file
			 * so that it will have no data structures left.
			 */
			if ((error = ffs_syncvnode(ovp, MNT_WAIT)) != 0)
				return (error);
			UFS_LOCK(ump);
			if (oip->i_flag & IN_SPACECOUNTED)
				fs->fs_pendingblocks -= datablocks;
			UFS_UNLOCK(ump);
		} else {
#ifdef QUOTA
			(void) chkdq(oip, -datablocks, NOCRED, 0);
#endif
			softdep_setup_freeblocks(oip, length, needextclean ?
			    IO_EXT | IO_NORMAL : IO_NORMAL);
			vinvalbuf(ovp, needextclean ? 0 : V_NORMAL, td, 0, 0);
			oip->i_flag |= IN_CHANGE | IN_UPDATE;
			return (ffs_update(ovp, 0));
		}
	}
	osize = oip->i_size;
	/*
	 * Lengthen the size of the file. We must ensure that the
	 * last byte of the file is allocated. Since the smallest
	 * value of osize is 0, length will be at least 1.
	 */
	if (osize < length) {
		vnode_pager_setsize(ovp, length);
		flags |= BA_CLRBUF;
		error = UFS_BALLOC(ovp, length - 1, 1, cred, flags, &bp);
		if (error)
			return (error);
		oip->i_size = length;
		DIP_SET(oip, i_size, length);
		if (bp->b_bufsize == fs->fs_bsize)
			bp->b_flags |= B_CLUSTEROK;
		if (flags & IO_SYNC)
			bwrite(bp);
		else
			bawrite(bp);
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		return (ffs_update(ovp, 1));
	}
	/*
	 * Shorten the size of the file. If the file is not being
	 * truncated to a block boundary, the contents of the
	 * partial block following the end of the file must be
	 * zero'ed in case it ever becomes accessible again because
	 * of subsequent file growth. Directories however are not
	 * zero'ed as they should grow back initialized to empty.
	 */
	offset = blkoff(fs, length);
	if (offset == 0) {
		oip->i_size = length;
		DIP_SET(oip, i_size, length);
	} else {
		lbn = lblkno(fs, length);
		flags |= BA_CLRBUF;
		error = UFS_BALLOC(ovp, length - 1, 1, cred, flags, &bp);
		if (error) {
			return (error);
		}
		/*
		 * When we are doing soft updates and the UFS_BALLOC
		 * above fills in a direct block hole with a full sized
		 * block that will be truncated down to a fragment below,
		 * we must flush out the block dependency with an FSYNC
		 * so that we do not get a soft updates inconsistency
		 * when we create the fragment below.
		 */
		if (DOINGSOFTDEP(ovp) && lbn < NDADDR &&
		    fragroundup(fs, blkoff(fs, length)) < fs->fs_bsize &&
		    (error = ffs_syncvnode(ovp, MNT_WAIT)) != 0)
			return (error);
		oip->i_size = length;
		DIP_SET(oip, i_size, length);
		size = blksize(fs, oip, lbn);
		if (ovp->v_type != VDIR)
			bzero((char *)bp->b_data + offset,
			    (u_int)(size - offset));
		/* Kirk's code has reallocbuf(bp, size, 1) here */
		allocbuf(bp, size);
		if (bp->b_bufsize == fs->fs_bsize)
			bp->b_flags |= B_CLUSTEROK;
		if (flags & IO_SYNC)
			bwrite(bp);
		else
			bawrite(bp);
	}
	/*
	 * Calculate index into inode's block list of
	 * last direct and indirect blocks (if any)
	 * which we want to keep.  Lastblock is -1 when
	 * the file is truncated to 0.
	 */
	lastblock = lblkno(fs, length + fs->fs_bsize - 1) - 1;
	lastiblock[SINGLE] = lastblock - NDADDR;
	lastiblock[DOUBLE] = lastiblock[SINGLE] - NINDIR(fs);
	lastiblock[TRIPLE] = lastiblock[DOUBLE] - NINDIR(fs) * NINDIR(fs);
	nblocks = btodb(fs->fs_bsize);
	/*
	 * Update file and block pointers on disk before we start freeing
	 * blocks.  If we crash before free'ing blocks below, the blocks
	 * will be returned to the free list.  lastiblock values are also
	 * normalized to -1 for calls to ffs_indirtrunc below.
	 */
	for (level = TRIPLE; level >= SINGLE; level--) {
		oldblks[NDADDR + level] = DIP(oip, i_ib[level]);
		if (lastiblock[level] < 0) {
			DIP_SET(oip, i_ib[level], 0);
			lastiblock[level] = -1;
		}
	}
	for (i = 0; i < NDADDR; i++) {
		oldblks[i] = DIP(oip, i_db[i]);
		if (i > lastblock)
			DIP_SET(oip, i_db[i], 0);
	}
	oip->i_flag |= IN_CHANGE | IN_UPDATE;
	allerror = ffs_update(ovp, 1);
	
	/*
	 * Having written the new inode to disk, save its new configuration
	 * and put back the old block pointers long enough to process them.
	 * Note that we save the new block configuration so we can check it
	 * when we are done.
	 */
	for (i = 0; i < NDADDR; i++) {
		newblks[i] = DIP(oip, i_db[i]);
		DIP_SET(oip, i_db[i], oldblks[i]);
	}
	for (i = 0; i < NIADDR; i++) {
		newblks[NDADDR + i] = DIP(oip, i_ib[i]);
		DIP_SET(oip, i_ib[i], oldblks[NDADDR + i]);
	}
	oip->i_size = osize;
	DIP_SET(oip, i_size, osize);

	error = vtruncbuf(ovp, cred, td, length, fs->fs_bsize);
	if (error && (allerror == 0))
		allerror = error;

	/*
	 * Indirect blocks first.
	 */
	indir_lbn[SINGLE] = -NDADDR;
	indir_lbn[DOUBLE] = indir_lbn[SINGLE] - NINDIR(fs) - 1;
	indir_lbn[TRIPLE] = indir_lbn[DOUBLE] - NINDIR(fs) * NINDIR(fs) - 1;
	for (level = TRIPLE; level >= SINGLE; level--) {
		bn = DIP(oip, i_ib[level]);
		if (bn != 0) {
			error = ffs_indirtrunc(oip, indir_lbn[level],
			    fsbtodb(fs, bn), lastiblock[level], level, &count);
			if (error)
				allerror = error;
			blocksreleased += count;
			if (lastiblock[level] < 0) {
				DIP_SET(oip, i_ib[level], 0);
				ffs_blkfree(ump, fs, oip->i_devvp, bn,
				    fs->fs_bsize, oip->i_number);
				blocksreleased += nblocks;
			}
		}
		if (lastiblock[level] >= 0)
			goto done;
	}

	/*
	 * All whole direct blocks or frags.
	 */
	for (i = NDADDR - 1; i > lastblock; i--) {
		long bsize;

		bn = DIP(oip, i_db[i]);
		if (bn == 0)
			continue;
		DIP_SET(oip, i_db[i], 0);
		bsize = blksize(fs, oip, i);
		ffs_blkfree(ump, fs, oip->i_devvp, bn, bsize, oip->i_number);
		blocksreleased += btodb(bsize);
	}
	if (lastblock < 0)
		goto done;

	/*
	 * Finally, look for a change in size of the
	 * last direct block; release any frags.
	 */
	bn = DIP(oip, i_db[lastblock]);
	if (bn != 0) {
		long oldspace, newspace;

		/*
		 * Calculate amount of space we're giving
		 * back as old block size minus new block size.
		 */
		oldspace = blksize(fs, oip, lastblock);
		oip->i_size = length;
		DIP_SET(oip, i_size, length);
		newspace = blksize(fs, oip, lastblock);
		if (newspace == 0)
			panic("ffs_truncate: newspace");
		if (oldspace - newspace > 0) {
			/*
			 * Block number of space to be free'd is
			 * the old block # plus the number of frags
			 * required for the storage we're keeping.
			 */
			bn += numfrags(fs, newspace);
			ffs_blkfree(ump, fs, oip->i_devvp, bn,
			    oldspace - newspace, oip->i_number);
			blocksreleased += btodb(oldspace - newspace);
		}
	}
done:
#ifdef DIAGNOSTIC
	for (level = SINGLE; level <= TRIPLE; level++)
		if (newblks[NDADDR + level] != DIP(oip, i_ib[level]))
			panic("ffs_truncate1");
	for (i = 0; i < NDADDR; i++)
		if (newblks[i] != DIP(oip, i_db[i]))
			panic("ffs_truncate2");
	VI_LOCK(ovp);
	if (length == 0 &&
	    (fs->fs_magic != FS_UFS2_MAGIC || oip->i_din2->di_extsize == 0) &&
	    (vp->v_bufobj.bo_dirty.bv_cnt > 0 ||
	     vp->v_bufobj.bo_clean.bv_cnt > 0))
		panic("ffs_truncate3");
	VI_UNLOCK(ovp);
#endif /* DIAGNOSTIC */
	/*
	 * Put back the real size.
	 */
	oip->i_size = length;
	DIP_SET(oip, i_size, length);
	DIP_SET(oip, i_blocks, DIP(oip, i_blocks) - blocksreleased);

	if (DIP(oip, i_blocks) < 0)			/* sanity */
		DIP_SET(oip, i_blocks, 0);
	oip->i_flag |= IN_CHANGE;
#ifdef QUOTA
	(void) chkdq(oip, -blocksreleased, NOCRED, 0);
#endif
	return (allerror);
}

/*
 * Release blocks associated with the inode ip and stored in the indirect
 * block bn.  Blocks are free'd in LIFO order up to (but not including)
 * lastbn.  If level is greater than SINGLE, the block is an indirect block
 * and recursive calls to indirtrunc must be used to cleanse other indirect
 * blocks.
 */
static int
ffs_indirtrunc(ip, lbn, dbn, lastbn, level, countp)
	struct inode *ip;
	ufs2_daddr_t lbn, lastbn;
	ufs2_daddr_t dbn;
	int level;
	ufs2_daddr_t *countp;
{
	struct buf *bp;
	struct fs *fs = ip->i_fs;
	struct vnode *vp;
	caddr_t copy = NULL;
	int i, nblocks, error = 0, allerror = 0;
	ufs2_daddr_t nb, nlbn, last;
	ufs2_daddr_t blkcount, factor, blocksreleased = 0;
	ufs1_daddr_t *bap1 = NULL;
	ufs2_daddr_t *bap2 = NULL;
#	define BAP(ip, i) (((ip)->i_ump->um_fstype == UFS1) ? bap1[i] : bap2[i])

	/*
	 * Calculate index in current block of last
	 * block to be kept.  -1 indicates the entire
	 * block so we need not calculate the index.
	 */
	factor = 1;
	for (i = SINGLE; i < level; i++)
		factor *= NINDIR(fs);
	last = lastbn;
	if (lastbn > 0)
		last /= factor;
	nblocks = btodb(fs->fs_bsize);
	/*
	 * Get buffer of block pointers, zero those entries corresponding
	 * to blocks to be free'd, and update on disk copy first.  Since
	 * double(triple) indirect before single(double) indirect, calls
	 * to bmap on these blocks will fail.  However, we already have
	 * the on disk address, so we have to set the b_blkno field
	 * explicitly instead of letting bread do everything for us.
	 */
	vp = ITOV(ip);
	bp = getblk(vp, lbn, (int)fs->fs_bsize, 0, 0, 0);
	if ((bp->b_flags & B_CACHE) == 0) {
		curproc->p_stats->p_ru.ru_inblock++;	/* pay for read */
		bp->b_iocmd = BIO_READ;
		bp->b_flags &= ~B_INVAL;
		bp->b_ioflags &= ~BIO_ERROR;
		if (bp->b_bcount > bp->b_bufsize)
			panic("ffs_indirtrunc: bad buffer size");
		bp->b_blkno = dbn;
		vfs_busy_pages(bp, 0);
		bp->b_iooffset = dbtob(bp->b_blkno);
		bstrategy(bp);
		error = bufwait(bp);
	}
	if (error) {
		brelse(bp);
		*countp = 0;
		return (error);
	}

	if (ip->i_ump->um_fstype == UFS1)
		bap1 = (ufs1_daddr_t *)bp->b_data;
	else
		bap2 = (ufs2_daddr_t *)bp->b_data;
	if (lastbn != -1) {
		MALLOC(copy, caddr_t, fs->fs_bsize, M_TEMP, M_WAITOK);
		bcopy((caddr_t)bp->b_data, copy, (u_int)fs->fs_bsize);
		for (i = last + 1; i < NINDIR(fs); i++)
			if (ip->i_ump->um_fstype == UFS1)
				bap1[i] = 0;
			else
				bap2[i] = 0;
		if (DOINGASYNC(vp)) {
			bawrite(bp);
		} else {
			error = bwrite(bp);
			if (error)
				allerror = error;
		}
		if (ip->i_ump->um_fstype == UFS1)
			bap1 = (ufs1_daddr_t *)copy;
		else
			bap2 = (ufs2_daddr_t *)copy;
	}

	/*
	 * Recursively free totally unused blocks.
	 */
	for (i = NINDIR(fs) - 1, nlbn = lbn + 1 - i * factor; i > last;
	    i--, nlbn += factor) {
		nb = BAP(ip, i);
		if (nb == 0)
			continue;
		if (level > SINGLE) {
			if ((error = ffs_indirtrunc(ip, nlbn, fsbtodb(fs, nb),
			    (ufs2_daddr_t)-1, level - 1, &blkcount)) != 0)
				allerror = error;
			blocksreleased += blkcount;
		}
		ffs_blkfree(ip->i_ump, fs, ip->i_devvp, nb, fs->fs_bsize,
		    ip->i_number);
		blocksreleased += nblocks;
	}

	/*
	 * Recursively free last partial block.
	 */
	if (level > SINGLE && lastbn >= 0) {
		last = lastbn % factor;
		nb = BAP(ip, i);
		if (nb != 0) {
			error = ffs_indirtrunc(ip, nlbn, fsbtodb(fs, nb),
			    last, level - 1, &blkcount);
			if (error)
				allerror = error;
			blocksreleased += blkcount;
		}
	}
	if (copy != NULL) {
		FREE(copy, M_TEMP);
	} else {
		bp->b_flags |= B_INVAL | B_NOCACHE;
		brelse(bp);
	}

	*countp = blocksreleased;
	return (allerror);
}
