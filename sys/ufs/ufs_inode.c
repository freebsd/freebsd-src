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
 *	from: @(#)ufs_inode.c	7.40 (Berkeley) 5/8/91
 *	$Id: ufs_inode.c,v 1.3 1993/10/16 18:17:52 rgrimes Exp $
 */

#include "param.h"
#include "systm.h"
#include "mount.h"
#include "proc.h"
#include "file.h"
#include "buf.h"
#include "vnode.h"
#include "kernel.h"
#include "malloc.h"

#include "quota.h"
#include "inode.h"
#include "fs.h"
#include "ufsmount.h"

#define	INOHSZ	512
#if	((INOHSZ&(INOHSZ-1)) == 0)
#define	INOHASH(dev,ino)	(((dev)+(ino))&(INOHSZ-1))
#else
#define	INOHASH(dev,ino)	(((unsigned)((dev)+(ino)))%INOHSZ)
#endif

union ihead {
	union  ihead *ih_head[2];
	struct inode *ih_chain[2];
} ihead[INOHSZ];

int prtactive;	/* 1 => print out reclaim of active vnodes */

/*
 * Initialize hash links for inodes.
 */
ufs_init()
{
	register int i;
	register union ihead *ih = ihead;

#ifndef lint
	if (VN_MAXPRIVATE < sizeof(struct inode))
		panic("ihinit: too small");
#endif /* not lint */
	for (i = INOHSZ; --i >= 0; ih++) {
		ih->ih_head[0] = ih;
		ih->ih_head[1] = ih;
	}
#ifdef QUOTA
	dqinit();
#endif /* QUOTA */
}

/*
 * Look up a UFS dinode number to find its incore vnode.
 * If it is not in core, read it in from the specified device.
 * If it is in core, wait for the lock bit to clear, then
 * return the inode locked. Detection and handling of mount
 * points must be done by the calling routine.
 */
iget(xp, ino, ipp)
	struct inode *xp;
	ino_t ino;
	struct inode **ipp;
{
	dev_t dev = xp->i_dev;
	struct mount *mntp = ITOV(xp)->v_mount;
	register struct fs *fs = VFSTOUFS(mntp)->um_fs;
	extern struct vnodeops ufs_vnodeops, spec_inodeops;
	register struct inode *ip, *iq;
	register struct vnode *vp;
	struct vnode *nvp;
	struct buf *bp;
	struct dinode *dp;
	union ihead *ih;
	int i, error;

	ih = &ihead[INOHASH(dev, ino)];
loop:
	for (ip = ih->ih_chain[0]; ip != (struct inode *)ih; ip = ip->i_forw) {
		if (ino != ip->i_number || dev != ip->i_dev)
			continue;
		if ((ip->i_flag&ILOCKED) != 0) {
			ip->i_flag |= IWANT;
			sleep((caddr_t)ip, PINOD);
			goto loop;
		}
		if (vget(ITOV(ip)))
			goto loop;
		*ipp = ip;
		return(0);
	}
	/*
	 * Allocate a new inode.
	 */
	if (error = getnewvnode(VT_UFS, mntp, &ufs_vnodeops, &nvp)) {
		*ipp = 0;
		return (error);
	}
	ip = VTOI(nvp);
	ip->i_vnode = nvp;
	ip->i_flag = 0;
	ip->i_devvp = 0;
	ip->i_mode = 0;
	ip->i_diroff = 0;
	ip->i_lockf = 0;
#ifdef QUOTA
	for (i = 0; i < MAXQUOTAS; i++)
		ip->i_dquot[i] = NODQUOT;
#endif
	for (i=0; i < DI_SPARE_SZ; i++)
		ip->i_di_spare[i] = (unsigned long)0L;
	/*
	 * Put it onto its hash chain and lock it so that other requests for
	 * this inode will block if they arrive while we are sleeping waiting
	 * for old data structures to be purged or for the contents of the
	 * disk portion of this inode to be read.
	 */
	ip->i_dev = dev;
	ip->i_number = ino;
	insque(ip, ih);
	ILOCK(ip);
	/*
	 * Read in the disk contents for the inode.
	 */
	if (error = bread(VFSTOUFS(mntp)->um_devvp, fsbtodb(fs, itod(fs, ino)),
	    (int)fs->fs_bsize, NOCRED, &bp)) {
		/*
		 * The inode does not contain anything useful, so it would
		 * be misleading to leave it on its hash chain.
		 * Iput() will take care of putting it back on the free list.
		 */
		remque(ip);
		ip->i_forw = ip;
		ip->i_back = ip;
		/*
		 * Unlock and discard unneeded inode.
		 */
		iput(ip);
		brelse(bp);
		*ipp = 0;
		return (error);
	}
	dp = bp->b_un.b_dino;
	dp += itoo(fs, ino);
	ip->i_din = *dp;
	brelse(bp);
	/*
	 * Initialize the associated vnode
	 */
	vp = ITOV(ip);
	vp->v_type = IFTOVT(ip->i_mode);
	if (vp->v_type == VFIFO) {
#ifdef FIFO
		extern struct vnodeops fifo_inodeops;
		vp->v_op = &fifo_inodeops;
#else
		iput(ip);
		*ipp = 0;
		return (EOPNOTSUPP);
#endif /* FIFO */
	}
	if (vp->v_type == VCHR || vp->v_type == VBLK) {
		vp->v_op = &spec_inodeops;
		if (nvp = checkalias(vp, ip->i_rdev, mntp)) {
			/*
			 * Reinitialize aliased inode.
			 */
			vp = nvp;
			iq = VTOI(vp);
			iq->i_vnode = vp;
			iq->i_flag = 0;
			ILOCK(iq);
			iq->i_din = ip->i_din;
			iq->i_dev = dev;
			iq->i_number = ino;
			insque(iq, ih);
			/*
			 * Discard unneeded vnode
			 */
			ip->i_mode = 0;
			iput(ip);
			ip = iq;
		}
	}
	if (ino == ROOTINO)
		vp->v_flag |= VROOT;
	/*
	 * Finish inode initialization.
	 */
	ip->i_fs = fs;
	ip->i_devvp = VFSTOUFS(mntp)->um_devvp;
	VREF(ip->i_devvp);
	/*
	 * Set up a generation number for this inode if it does not
	 * already have one. This should only happen on old filesystems.
	 */
	if (ip->i_gen == 0) {
		if (++nextgennumber < (u_long)time.tv_sec)
			nextgennumber = time.tv_sec;
		ip->i_gen = nextgennumber;
		if ((vp->v_mount->mnt_flag & MNT_RDONLY) == 0)
			ip->i_flag |= IMOD;
	}
	*ipp = ip;
	return (0);
}

/*
 * Unlock and decrement the reference count of an inode structure.
 */
iput(ip)
	register struct inode *ip;
{

	if ((ip->i_flag & ILOCKED) == 0)
		panic("iput");
	IUNLOCK(ip);
	vrele(ITOV(ip));
}

/*
 * Last reference to an inode, write the inode out and if necessary,
 * truncate and deallocate the file.
 */
ufs_inactive(vp, p)
	struct vnode *vp;
	struct proc *p;
{
	register struct inode *ip = VTOI(vp);
	int mode, error = 0;

	if (prtactive && vp->v_usecount != 0)
		vprint("ufs_inactive: pushing active", vp);
	/*
	 * Get rid of inodes related to stale file handles.
	 */
	if (ip->i_mode == 0) {
		if ((vp->v_flag & VXLOCK) == 0)
			vgone(vp);
		return (0);
	}
	ILOCK(ip);
	if (ip->i_nlink <= 0 && (vp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
#ifdef QUOTA
		if (!getinoquota(ip))
			(void) chkiq(ip, -1, NOCRED, 0);
#endif
		error = itrunc(ip, (u_long)0, 0);
		mode = ip->i_mode;
		ip->i_mode = 0;
		ip->i_rdev = 0;
		ip->i_flag |= IUPD|ICHG;
		ifree(ip, ip->i_number, mode);
	}
	IUPDAT(ip, &time, &time, 0);
	IUNLOCK(ip);
	ip->i_flag = 0;
	/*
	 * If we are done with the inode, reclaim it
	 * so that it can be reused immediately.
	 */
	if (vp->v_usecount == 0 && ip->i_mode == 0)
		vgone(vp);
	return (error);
}

/*
 * Reclaim an inode so that it can be used for other purposes.
 */
ufs_reclaim(vp)
	register struct vnode *vp;
{
	register struct inode *ip = VTOI(vp);
	int i;

	if (prtactive && vp->v_usecount != 0)
		vprint("ufs_reclaim: pushing active", vp);
	/*
	 * Remove the inode from its hash chain.
	 */
	remque(ip);
	ip->i_forw = ip;
	ip->i_back = ip;
	/*
	 * Purge old data structures associated with the inode.
	 */
	cache_purge(vp);
	if (ip->i_devvp) {
		vrele(ip->i_devvp);
		ip->i_devvp = 0;
	}
#ifdef QUOTA
	for (i = 0; i < MAXQUOTAS; i++) {
		if (ip->i_dquot[i] != NODQUOT) {
			dqrele(vp, ip->i_dquot[i]);
			ip->i_dquot[i] = NODQUOT;
		}
	}
#endif
	ip->i_flag = 0;
	return (0);
}

/*
 * Update the access, modified, and inode change times as specified
 * by the IACC, IMOD, and ICHG flags respectively. The IUPD flag
 * is used to specify that the inode needs to be updated but that
 * the times have already been set. The access and modified times
 * are taken from the second and third parameters; the inode change
 * time is always taken from the current time. If waitfor is set,
 * then wait for the disk write of the inode to complete.
 */
iupdat(ip, ta, tm, waitfor)
	register struct inode *ip;
	struct timeval *ta, *tm;
	int waitfor;
{
	struct buf *bp;
	struct vnode *vp = ITOV(ip);
	struct dinode *dp;
	register struct fs *fs;
	int error;

	fs = ip->i_fs;
	if ((ip->i_flag & (IUPD|IACC|ICHG|IMOD)) == 0)
		return (0);
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return (0);
	error = bread(ip->i_devvp, fsbtodb(fs, itod(fs, ip->i_number)),
		(int)fs->fs_bsize, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return (error);
	}
	if (ip->i_flag&IACC)
		ip->i_atime = ta->tv_sec;
	if (ip->i_flag&IUPD)
		ip->i_mtime = tm->tv_sec;
	if (ip->i_flag&ICHG)
		ip->i_ctime = time.tv_sec;
	ip->i_flag &= ~(IUPD|IACC|ICHG|IMOD);
	dp = bp->b_un.b_dino + itoo(fs, ip->i_number);
	*dp = ip->i_din;
	if (waitfor) {
		return (bwrite(bp));
	} else {
		bdwrite(bp);
		return (0);
	}
}

#define	SINGLE	0	/* index of single indirect block */
#define	DOUBLE	1	/* index of double indirect block */
#define	TRIPLE	2	/* index of triple indirect block */
/*
 * Truncate the inode ip to at most length size.  Free affected disk
 * blocks -- the blocks of the file are removed in reverse order.
 *
 * NB: triple indirect blocks are untested.
 */
itrunc(oip, length, flags)
	register struct inode *oip;
	u_long length;
	int flags;
{
	register daddr_t lastblock;
	daddr_t bn, lbn, lastiblock[NIADDR];
	register struct fs *fs;
	register struct inode *ip;
	struct buf *bp;
	int offset, osize, size, level;
	long count, nblocks, blocksreleased = 0;
	register int i;
	int aflags, error, allerror;
	struct inode tip;

	vnode_pager_setsize(ITOV(oip), length);
	if (FASTLINK(oip)) {
		if (length != 0)
			panic("itrunc fastlink to non-zero");
		bzero(oip->i_symlink, MAXFASTLINK);
		oip->i_size = 0;
		oip->i_din.di_spare[0] = 0;
	}
	if (oip->i_size <= length) {
		oip->i_flag |= ICHG|IUPD;
		error = iupdat(oip, &time, &time, 1);
		return (error);
	}
	/*
	 * Calculate index into inode's block list of
	 * last direct and indirect blocks (if any)
	 * which we want to keep.  Lastblock is -1 when
	 * the file is truncated to 0.
	 */
	fs = oip->i_fs;
	lastblock = lblkno(fs, length + fs->fs_bsize - 1) - 1;
	lastiblock[SINGLE] = lastblock - NDADDR;
	lastiblock[DOUBLE] = lastiblock[SINGLE] - NINDIR(fs);
	lastiblock[TRIPLE] = lastiblock[DOUBLE] - NINDIR(fs) * NINDIR(fs);
	nblocks = btodb(fs->fs_bsize);
	/*
	 * Update the size of the file. If the file is not being
	 * truncated to a block boundry, the contents of the
	 * partial block following the end of the file must be
	 * zero'ed in case it ever become accessable again because
	 * of subsequent file growth.
	 */
	osize = oip->i_size;
	offset = blkoff(fs, length);
	if (offset == 0) {
		oip->i_size = length;
	} else {
		lbn = lblkno(fs, length);
		aflags = B_CLRBUF;
		if (flags & IO_SYNC)
			aflags |= B_SYNC;
#ifdef QUOTA
		if (error = getinoquota(oip))
			return (error);
#endif
		if (error = balloc(oip, lbn, offset, &bp, aflags))
			return (error);
		oip->i_size = length;
		size = blksize(fs, oip, lbn);
		(void) vnode_pager_uncache(ITOV(oip));
		bzero(bp->b_un.b_addr + offset, (unsigned)(size - offset));
		allocbuf(bp, size);
		if (flags & IO_SYNC)
			bwrite(bp);
		else
			bdwrite(bp);
	}
	/*
	 * Update file and block pointers
	 * on disk before we start freeing blocks.
	 * If we crash before free'ing blocks below,
	 * the blocks will be returned to the free list.
	 * lastiblock values are also normalized to -1
	 * for calls to indirtrunc below.
	 */
	tip = *oip;
	tip.i_size = osize;
	for (level = TRIPLE; level >= SINGLE; level--)
		if (lastiblock[level] < 0) {
			oip->i_ib[level] = 0;
			lastiblock[level] = -1;
		}
	for (i = NDADDR - 1; i > lastblock; i--)
		oip->i_db[i] = 0;
	oip->i_flag |= ICHG|IUPD;
	vinvalbuf(ITOV(oip), (length > 0));
	allerror = iupdat(oip, &time, &time, MNT_WAIT);

	/*
	 * Indirect blocks first.
	 */
	ip = &tip;
	for (level = TRIPLE; level >= SINGLE; level--) {
		bn = ip->i_ib[level];
		if (bn != 0) {
			error = indirtrunc(ip, bn, lastiblock[level], level,
				&count);
			if (error)
				allerror = error;
			blocksreleased += count;
			if (lastiblock[level] < 0) {
				ip->i_ib[level] = 0;
				blkfree(ip, bn, (off_t)fs->fs_bsize);
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
		register off_t bsize;

		bn = ip->i_db[i];
		if (bn == 0)
			continue;
		ip->i_db[i] = 0;
		bsize = (off_t)blksize(fs, ip, i);
		blkfree(ip, bn, bsize);
		blocksreleased += btodb(bsize);
	}
	if (lastblock < 0)
		goto done;

	/*
	 * Finally, look for a change in size of the
	 * last direct block; release any frags.
	 */
	bn = ip->i_db[lastblock];
	if (bn != 0) {
		off_t oldspace, newspace;

		/*
		 * Calculate amount of space we're giving
		 * back as old block size minus new block size.
		 */
		oldspace = blksize(fs, ip, lastblock);
		ip->i_size = length;
		newspace = blksize(fs, ip, lastblock);
		if (newspace == 0)
			panic("itrunc: newspace");
		if (oldspace - newspace > 0) {
			/*
			 * Block number of space to be free'd is
			 * the old block # plus the number of frags
			 * required for the storage we're keeping.
			 */
			bn += numfrags(fs, newspace);
			blkfree(ip, bn, oldspace - newspace);
			blocksreleased += btodb(oldspace - newspace);
		}
	}
done:
/* BEGIN PARANOIA */
	for (level = SINGLE; level <= TRIPLE; level++)
		if (ip->i_ib[level] != oip->i_ib[level])
			panic("itrunc1");
	for (i = 0; i < NDADDR; i++)
		if (ip->i_db[i] != oip->i_db[i])
			panic("itrunc2");
/* END PARANOIA */
	oip->i_blocks -= blocksreleased;
	if (oip->i_blocks < 0)			/* sanity */
		oip->i_blocks = 0;
	oip->i_flag |= ICHG;
#ifdef QUOTA
	if (!getinoquota(oip))
		(void) chkdq(oip, -blocksreleased, NOCRED, 0);
#endif
	return (allerror);
}

/*
 * Release blocks associated with the inode ip and
 * stored in the indirect block bn.  Blocks are free'd
 * in LIFO order up to (but not including) lastbn.  If
 * level is greater than SINGLE, the block is an indirect
 * block and recursive calls to indirtrunc must be used to
 * cleanse other indirect blocks.
 *
 * NB: triple indirect blocks are untested.
 */
indirtrunc(ip, bn, lastbn, level, countp)
	register struct inode *ip;
	daddr_t bn, lastbn;
	int level;
	long *countp;
{
	register int i;
	struct buf *bp;
	register struct fs *fs = ip->i_fs;
	register daddr_t *bap;
	daddr_t *copy, nb, last;
	long blkcount, factor;
	int nblocks, blocksreleased = 0;
	int error, allerror = 0;

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
	 * Get buffer of block pointers, zero those 
	 * entries corresponding to blocks to be free'd,
	 * and update on disk copy first.
	 */
	error = bread(ip->i_devvp, fsbtodb(fs, bn), (int)fs->fs_bsize,
		NOCRED, &bp);
	if (error) {
		brelse(bp);
		*countp = 0;
		return (error);
	}
	bap = bp->b_un.b_daddr;
	MALLOC(copy, daddr_t *, fs->fs_bsize, M_TEMP, M_WAITOK);
	bcopy((caddr_t)bap, (caddr_t)copy, (u_int)fs->fs_bsize);
	bzero((caddr_t)&bap[last + 1],
	  (u_int)(NINDIR(fs) - (last + 1)) * sizeof (daddr_t));
	if (last == -1)
		bp->b_flags |= B_INVAL;
	error = bwrite(bp);
	if (error)
		allerror = error;
	bap = copy;

	/*
	 * Recursively free totally unused blocks.
	 */
	for (i = NINDIR(fs) - 1; i > last; i--) {
		nb = bap[i];
		if (nb == 0)
			continue;
		if (level > SINGLE) {
			error = indirtrunc(ip, nb, (daddr_t)-1, level - 1,
				&blkcount);
			if (error)
				allerror = error;
			blocksreleased += blkcount;
		}
		blkfree(ip, nb, (off_t)fs->fs_bsize);
		blocksreleased += nblocks;
	}

	/*
	 * Recursively free last partial block.
	 */
	if (level > SINGLE && lastbn >= 0) {
		last = lastbn % factor;
		nb = bap[i];
		if (nb != 0) {
			error = indirtrunc(ip, nb, last, level - 1, &blkcount);
			if (error)
				allerror = error;
			blocksreleased += blkcount;
		}
	}
	FREE(copy, M_TEMP);
	*countp = blocksreleased;
	return (allerror);
}

/*
 * Lock an inode. If its already locked, set the WANT bit and sleep.
 */
ilock(ip)
	register struct inode *ip;
{

	while (ip->i_flag & ILOCKED) {
		ip->i_flag |= IWANT;
		if (ip->i_spare0 == curproc->p_pid)
			panic("locking against myself");
		ip->i_spare1 = curproc->p_pid;
		(void) sleep((caddr_t)ip, PINOD);
	}
	ip->i_spare1 = 0;
	ip->i_spare0 = curproc->p_pid;
	ip->i_flag |= ILOCKED;
}

/*
 * Unlock an inode.  If WANT bit is on, wakeup.
 */
iunlock(ip)
	register struct inode *ip;
{

	if ((ip->i_flag & ILOCKED) == 0)
		vprint("iunlock: unlocked inode", ITOV(ip));
	ip->i_spare0 = 0;
	ip->i_flag &= ~ILOCKED;
	if (ip->i_flag&IWANT) {
		ip->i_flag &= ~IWANT;
		wakeup((caddr_t)ip);
	}
}
