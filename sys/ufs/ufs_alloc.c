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
 *	from: @(#)ufs_alloc.c	7.26 (Berkeley) 5/2/91
 *	$Id: ufs_alloc.c,v 1.2 1993/10/16 18:17:49 rgrimes Exp $
 */

#include "param.h"
#include "systm.h"
#include "buf.h"
#include "proc.h"
#include "vnode.h"
#include "kernel.h"
#include "syslog.h"

#include "quota.h"
#include "inode.h"
#include "fs.h"

extern u_long		hashalloc();
extern ino_t		ialloccg();
extern daddr_t		alloccg();
extern daddr_t		alloccgblk();
extern daddr_t		fragextend();
extern daddr_t		blkpref();
extern daddr_t		mapsearch();
extern int		inside[], around[];
extern unsigned char	*fragtbl[];

/*
 * Allocate a block in the file system.
 * 
 * The size of the requested block is given, which must be some
 * multiple of fs_fsize and <= fs_bsize.
 * A preference may be optionally specified. If a preference is given
 * the following hierarchy is used to allocate a block:
 *   1) allocate the requested block.
 *   2) allocate a rotationally optimal block in the same cylinder.
 *   3) allocate a block in the same cylinder group.
 *   4) quadradically rehash into other cylinder groups, until an
 *      available block is located.
 * If no block preference is given the following heirarchy is used
 * to allocate a block:
 *   1) allocate a block in the cylinder group that contains the
 *      inode for the file.
 *   2) quadradically rehash into other cylinder groups, until an
 *      available block is located.
 */
alloc(ip, lbn, bpref, size, bnp)
	register struct inode *ip;
	daddr_t lbn, bpref;
	int size;
	daddr_t *bnp;
{
	daddr_t bno;
	register struct fs *fs;
	register struct buf *bp;
	int cg, error;
	struct ucred *cred = curproc->p_ucred;		/* XXX */
	
	*bnp = 0;
	fs = ip->i_fs;
	if ((unsigned)size > fs->fs_bsize || fragoff(fs, size) != 0) {
		printf("dev = 0x%x, bsize = %d, size = %d, fs = %s\n",
		    ip->i_dev, fs->fs_bsize, size, fs->fs_fsmnt);
		panic("alloc: bad size");
	}
	if (size == fs->fs_bsize && fs->fs_cstotal.cs_nbfree == 0)
		goto nospace;
	if (cred->cr_uid != 0 && freespace(fs, fs->fs_minfree) <= 0)
		goto nospace;
#ifdef QUOTA
	if (error = chkdq(ip, (long)btodb(size), cred, 0))
		return (error);
#endif
	if (bpref >= fs->fs_size)
		bpref = 0;
	if (bpref == 0)
		cg = itog(fs, ip->i_number);
	else
		cg = dtog(fs, bpref);
	bno = (daddr_t)hashalloc(ip, cg, (long)bpref, size,
		(u_long (*)())alloccg);
	if (bno > 0) {
		ip->i_blocks += btodb(size);
		ip->i_flag |= IUPD|ICHG;
		*bnp = bno;
		return (0);
	}
#ifdef QUOTA
	/*
	 * Restore user's disk quota because allocation failed.
	 */
	(void) chkdq(ip, (long)-btodb(size), cred, FORCE);
#endif
nospace:
	fserr(fs, cred->cr_uid, "file system full");
	uprintf("\n%s: write failed, file system is full\n", fs->fs_fsmnt);
	return (ENOSPC);
}

/*
 * Reallocate a fragment to a bigger size
 *
 * The number and size of the old block is given, and a preference
 * and new size is also specified. The allocator attempts to extend
 * the original block. Failing that, the regular block allocator is
 * invoked to get an appropriate block.
 */
realloccg(ip, lbprev, bpref, osize, nsize, bpp)
	register struct inode *ip;
	off_t lbprev;
	daddr_t bpref;
	int osize, nsize;
	struct buf **bpp;
{
	register struct fs *fs;
	struct buf *bp, *obp;
	int cg, request, error;
	daddr_t bprev, bno;
	struct ucred *cred = curproc->p_ucred;		/* XXX */
	
	*bpp = 0;
	fs = ip->i_fs;
	if ((unsigned)osize > fs->fs_bsize || fragoff(fs, osize) != 0 ||
	    (unsigned)nsize > fs->fs_bsize || fragoff(fs, nsize) != 0) {
		printf("dev = 0x%x, bsize = %d, osize = %d, nsize = %d, fs = %s\n",
		    ip->i_dev, fs->fs_bsize, osize, nsize, fs->fs_fsmnt);
		panic("realloccg: bad size");
	}
	if (cred->cr_uid != 0 && freespace(fs, fs->fs_minfree) <= 0)
		goto nospace;
	if ((bprev = ip->i_db[lbprev]) == 0) {
		printf("dev = 0x%x, bsize = %d, bprev = %d, fs = %s\n",
		    ip->i_dev, fs->fs_bsize, bprev, fs->fs_fsmnt);
		panic("realloccg: bad bprev");
	}
	/*
	 * Allocate the extra space in the buffer.
	 */
	if (error = bread(ITOV(ip), lbprev, osize, NOCRED, &bp)) {
		brelse(bp);
		return (error);
	}
#ifdef QUOTA
	if (error = chkdq(ip, (long)btodb(nsize - osize), cred, 0)) {
		brelse(bp);
		return (error);
	}
#endif
	/*
	 * Check for extension in the existing location.
	 */
	cg = dtog(fs, bprev);
	if (bno = fragextend(ip, cg, (long)bprev, osize, nsize)) {
		if (bp->b_blkno != fsbtodb(fs, bno))
			panic("bad blockno");
		ip->i_blocks += btodb(nsize - osize);
		ip->i_flag |= IUPD|ICHG;
		allocbuf(bp, nsize);
		bp->b_flags |= B_DONE;
		bzero(bp->b_un.b_addr + osize, (unsigned)nsize - osize);
		*bpp = bp;
		return (0);
	}
	/*
	 * Allocate a new disk location.
	 */
	if (bpref >= fs->fs_size)
		bpref = 0;
	switch ((int)fs->fs_optim) {
	case FS_OPTSPACE:
		/*
		 * Allocate an exact sized fragment. Although this makes 
		 * best use of space, we will waste time relocating it if 
		 * the file continues to grow. If the fragmentation is
		 * less than half of the minimum free reserve, we choose
		 * to begin optimizing for time.
		 */
		request = nsize;
		if (fs->fs_minfree < 5 ||
		    fs->fs_cstotal.cs_nffree >
		    fs->fs_dsize * fs->fs_minfree / (2 * 100))
			break;
		log(LOG_NOTICE, "%s: optimization changed from SPACE to TIME\n",
			fs->fs_fsmnt);
		fs->fs_optim = FS_OPTTIME;
		break;
	case FS_OPTTIME:
		/*
		 * At this point we have discovered a file that is trying
		 * to grow a small fragment to a larger fragment. To save
		 * time, we allocate a full sized block, then free the 
		 * unused portion. If the file continues to grow, the 
		 * `fragextend' call above will be able to grow it in place
		 * without further copying. If aberrant programs cause
		 * disk fragmentation to grow within 2% of the free reserve,
		 * we choose to begin optimizing for space.
		 */
		request = fs->fs_bsize;
		if (fs->fs_cstotal.cs_nffree <
		    fs->fs_dsize * (fs->fs_minfree - 2) / 100)
			break;
		log(LOG_NOTICE, "%s: optimization changed from TIME to SPACE\n",
			fs->fs_fsmnt);
		fs->fs_optim = FS_OPTSPACE;
		break;
	default:
		printf("dev = 0x%x, optim = %d, fs = %s\n",
		    ip->i_dev, fs->fs_optim, fs->fs_fsmnt);
		panic("realloccg: bad optim");
		/* NOTREACHED */
	}
	bno = (daddr_t)hashalloc(ip, cg, (long)bpref, request,
		(u_long (*)())alloccg);
	if (bno > 0) {
		bp->b_blkno = fsbtodb(fs, bno);
		(void) vnode_pager_uncache(ITOV(ip));
		blkfree(ip, bprev, (off_t)osize);
		if (nsize < request)
			blkfree(ip, bno + numfrags(fs, nsize),
				(off_t)(request - nsize));
		ip->i_blocks += btodb(nsize - osize);
		ip->i_flag |= IUPD|ICHG;
		allocbuf(bp, nsize);
		bp->b_flags |= B_DONE;
		bzero(bp->b_un.b_addr + osize, (unsigned)nsize - osize);
		*bpp = bp;
		return (0);
	}
#ifdef QUOTA
	/*
	 * Restore user's disk quota because allocation failed.
	 */
	(void) chkdq(ip, (long)-btodb(nsize - osize), cred, FORCE);
#endif
	brelse(bp);
nospace:
	/*
	 * no space available
	 */
	fserr(fs, cred->cr_uid, "file system full");
	uprintf("\n%s: write failed, file system is full\n", fs->fs_fsmnt);
	return (ENOSPC);
}

/*
 * Allocate an inode in the file system.
 * 
 * A preference may be optionally specified. If a preference is given
 * the following hierarchy is used to allocate an inode:
 *   1) allocate the requested inode.
 *   2) allocate an inode in the same cylinder group.
 *   3) quadradically rehash into other cylinder groups, until an
 *      available inode is located.
 * If no inode preference is given the following heirarchy is used
 * to allocate an inode:
 *   1) allocate an inode in cylinder group 0.
 *   2) quadradically rehash into other cylinder groups, until an
 *      available inode is located.
 */
ialloc(pip, ipref, mode, cred, ipp)
	register struct inode *pip;
	ino_t ipref;
	int mode;
	struct ucred *cred;
	struct inode **ipp;
{
	ino_t ino;
	register struct fs *fs;
	register struct inode *ip;
	int cg, error;
	
	*ipp = 0;
	fs = pip->i_fs;
	if (fs->fs_cstotal.cs_nifree == 0)
		goto noinodes;
	if (ipref >= fs->fs_ncg * fs->fs_ipg)
		ipref = 0;
	cg = itog(fs, ipref);
	ino = (ino_t)hashalloc(pip, cg, (long)ipref, mode, ialloccg);
	if (ino == 0)
		goto noinodes;
	error = iget(pip, ino, ipp);
	if (error) {
		ifree(pip, ino, mode);
		return (error);
	}
	ip = *ipp;
	if (ip->i_mode) {
		printf("mode = 0%o, inum = %d, fs = %s\n",
		    ip->i_mode, ip->i_number, fs->fs_fsmnt);
		panic("ialloc: dup alloc");
	}
	if (ip->i_blocks) {				/* XXX */
		printf("free inode %s/%d had %d blocks\n",
		    fs->fs_fsmnt, ino, ip->i_blocks);
		ip->i_blocks = 0;
	}
	ip->i_flags = 0;
	/*
	 * Set up a new generation number for this inode.
	 */
	if (++nextgennumber < (u_long)time.tv_sec)
		nextgennumber = time.tv_sec;
	ip->i_gen = nextgennumber;
	return (0);
noinodes:
	fserr(fs, cred->cr_uid, "out of inodes");
	uprintf("\n%s: create/symlink failed, no inodes free\n", fs->fs_fsmnt);
	return (ENOSPC);
}

/*
 * Find a cylinder to place a directory.
 *
 * The policy implemented by this algorithm is to select from
 * among those cylinder groups with above the average number of
 * free inodes, the one with the smallest number of directories.
 */
ino_t
dirpref(fs)
	register struct fs *fs;
{
	int cg, minndir, mincg, avgifree;

	avgifree = fs->fs_cstotal.cs_nifree / fs->fs_ncg;
	minndir = fs->fs_ipg;
	mincg = 0;
	for (cg = 0; cg < fs->fs_ncg; cg++)
		if (fs->fs_cs(fs, cg).cs_ndir < minndir &&
		    fs->fs_cs(fs, cg).cs_nifree >= avgifree) {
			mincg = cg;
			minndir = fs->fs_cs(fs, cg).cs_ndir;
		}
	return ((ino_t)(fs->fs_ipg * mincg));
}

/*
 * Select the desired position for the next block in a file.  The file is
 * logically divided into sections. The first section is composed of the
 * direct blocks. Each additional section contains fs_maxbpg blocks.
 * 
 * If no blocks have been allocated in the first section, the policy is to
 * request a block in the same cylinder group as the inode that describes
 * the file. If no blocks have been allocated in any other section, the
 * policy is to place the section in a cylinder group with a greater than
 * average number of free blocks.  An appropriate cylinder group is found
 * by using a rotor that sweeps the cylinder groups. When a new group of
 * blocks is needed, the sweep begins in the cylinder group following the
 * cylinder group from which the previous allocation was made. The sweep
 * continues until a cylinder group with greater than the average number
 * of free blocks is found. If the allocation is for the first block in an
 * indirect block, the information on the previous allocation is unavailable;
 * here a best guess is made based upon the logical block number being
 * allocated.
 * 
 * If a section is already partially allocated, the policy is to
 * contiguously allocate fs_maxcontig blocks.  The end of one of these
 * contiguous blocks and the beginning of the next is physically separated
 * so that the disk head will be in transit between them for at least
 * fs_rotdelay milliseconds.  This is to allow time for the processor to
 * schedule another I/O transfer.
 */
daddr_t
blkpref(ip, lbn, indx, bap)
	struct inode *ip;
	daddr_t lbn;
	int indx;
	daddr_t *bap;
{
	register struct fs *fs;
	register int cg;
	int avgbfree, startcg;
	daddr_t nextblk;

	fs = ip->i_fs;
	if (indx % fs->fs_maxbpg == 0 || bap[indx - 1] == 0) {
		if (lbn < NDADDR) {
			cg = itog(fs, ip->i_number);
			return (fs->fs_fpg * cg + fs->fs_frag);
		}
		/*
		 * Find a cylinder with greater than average number of
		 * unused data blocks.
		 */
		if (indx == 0 || bap[indx - 1] == 0)
			startcg = itog(fs, ip->i_number) + lbn / fs->fs_maxbpg;
		else
			startcg = dtog(fs, bap[indx - 1]) + 1;
		startcg %= fs->fs_ncg;
		avgbfree = fs->fs_cstotal.cs_nbfree / fs->fs_ncg;
		for (cg = startcg; cg < fs->fs_ncg; cg++)
			if (fs->fs_cs(fs, cg).cs_nbfree >= avgbfree) {
				fs->fs_cgrotor = cg;
				return (fs->fs_fpg * cg + fs->fs_frag);
			}
		for (cg = 0; cg <= startcg; cg++)
			if (fs->fs_cs(fs, cg).cs_nbfree >= avgbfree) {
				fs->fs_cgrotor = cg;
				return (fs->fs_fpg * cg + fs->fs_frag);
			}
		return (NULL);
	}
	/*
	 * One or more previous blocks have been laid out. If less
	 * than fs_maxcontig previous blocks are contiguous, the
	 * next block is requested contiguously, otherwise it is
	 * requested rotationally delayed by fs_rotdelay milliseconds.
	 */
	nextblk = bap[indx - 1] + fs->fs_frag;
	if (indx < fs->fs_maxcontig || bap[indx - fs->fs_maxcontig] +
	    blkstofrags(fs, fs->fs_maxcontig) != nextblk)
		return (nextblk);
	if (fs->fs_rotdelay != 0)
		/*
		 * Here we convert ms of delay to frags as:
		 * (frags) = (ms) * (rev/sec) * (sect/rev) /
		 *	((sect/frag) * (ms/sec))
		 * then round up to the next block.
		 */
		nextblk += roundup(fs->fs_rotdelay * fs->fs_rps * fs->fs_nsect /
		    (NSPF(fs) * 1000), fs->fs_frag);
	return (nextblk);
}

/*
 * Implement the cylinder overflow algorithm.
 *
 * The policy implemented by this algorithm is:
 *   1) allocate the block in its requested cylinder group.
 *   2) quadradically rehash on the cylinder group number.
 *   3) brute force search for a free block.
 */
/*VARARGS5*/
u_long
hashalloc(ip, cg, pref, size, allocator)
	struct inode *ip;
	int cg;
	long pref;
	int size;	/* size for data blocks, mode for inodes */
	u_long (*allocator)();
{
	register struct fs *fs;
	long result;
	int i, icg = cg;

	fs = ip->i_fs;
	/*
	 * 1: preferred cylinder group
	 */
	result = (*allocator)(ip, cg, pref, size);
	if (result)
		return (result);
	/*
	 * 2: quadratic rehash
	 */
	for (i = 1; i < fs->fs_ncg; i *= 2) {
		cg += i;
		if (cg >= fs->fs_ncg)
			cg -= fs->fs_ncg;
		result = (*allocator)(ip, cg, 0, size);
		if (result)
			return (result);
	}
	/*
	 * 3: brute force search
	 * Note that we start at i == 2, since 0 was checked initially,
	 * and 1 is always checked in the quadratic rehash.
	 */
	cg = (icg + 2) % fs->fs_ncg;
	for (i = 2; i < fs->fs_ncg; i++) {
		result = (*allocator)(ip, cg, 0, size);
		if (result)
			return (result);
		cg++;
		if (cg == fs->fs_ncg)
			cg = 0;
	}
	return (NULL);
}

/*
 * Determine whether a fragment can be extended.
 *
 * Check to see if the necessary fragments are available, and 
 * if they are, allocate them.
 */
daddr_t
fragextend(ip, cg, bprev, osize, nsize)
	struct inode *ip;
	int cg;
	long bprev;
	int osize, nsize;
{
	register struct fs *fs;
	register struct cg *cgp;
	struct buf *bp;
	long bno;
	int frags, bbase;
	int i, error;

	fs = ip->i_fs;
	if (fs->fs_cs(fs, cg).cs_nffree < numfrags(fs, nsize - osize))
		return (NULL);
	frags = numfrags(fs, nsize);
	bbase = fragnum(fs, bprev);
	if (bbase > fragnum(fs, (bprev + frags - 1))) {
		/* cannot extend across a block boundary */
		return (NULL);
	}
	error = bread(ip->i_devvp, fsbtodb(fs, cgtod(fs, cg)),
		(int)fs->fs_cgsize, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return (NULL);
	}
	cgp = bp->b_un.b_cg;
	if (!cg_chkmagic(cgp)) {
		brelse(bp);
		return (NULL);
	}
	cgp->cg_time = time.tv_sec;
	bno = dtogd(fs, bprev);
	for (i = numfrags(fs, osize); i < frags; i++)
		if (isclr(cg_blksfree(cgp), bno + i)) {
			brelse(bp);
			return (NULL);
		}
	/*
	 * the current fragment can be extended
	 * deduct the count on fragment being extended into
	 * increase the count on the remaining fragment (if any)
	 * allocate the extended piece
	 */
	for (i = frags; i < fs->fs_frag - bbase; i++)
		if (isclr(cg_blksfree(cgp), bno + i))
			break;
	cgp->cg_frsum[i - numfrags(fs, osize)]--;
	if (i != frags)
		cgp->cg_frsum[i - frags]++;
	for (i = numfrags(fs, osize); i < frags; i++) {
		clrbit(cg_blksfree(cgp), bno + i);
		cgp->cg_cs.cs_nffree--;
		fs->fs_cstotal.cs_nffree--;
		fs->fs_cs(fs, cg).cs_nffree--;
	}
	fs->fs_fmod++;
	bdwrite(bp);
	return (bprev);
}

/*
 * Determine whether a block can be allocated.
 *
 * Check to see if a block of the apprpriate size is available,
 * and if it is, allocate it.
 */
daddr_t
alloccg(ip, cg, bpref, size)
	struct inode *ip;
	int cg;
	daddr_t bpref;
	int size;
{
	register struct fs *fs;
	register struct cg *cgp;
	struct buf *bp;
	register int i;
	int error, bno, frags, allocsiz;

	fs = ip->i_fs;
	if (fs->fs_cs(fs, cg).cs_nbfree == 0 && size == fs->fs_bsize)
		return (NULL);
	error = bread(ip->i_devvp, fsbtodb(fs, cgtod(fs, cg)),
		(int)fs->fs_cgsize, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return (NULL);
	}
	cgp = bp->b_un.b_cg;
	if (!cg_chkmagic(cgp) ||
	    (cgp->cg_cs.cs_nbfree == 0 && size == fs->fs_bsize)) {
		brelse(bp);
		return (NULL);
	}
	cgp->cg_time = time.tv_sec;
	if (size == fs->fs_bsize) {
		bno = alloccgblk(fs, cgp, bpref);
		bdwrite(bp);
		return (bno);
	}
	/*
	 * check to see if any fragments are already available
	 * allocsiz is the size which will be allocated, hacking
	 * it down to a smaller size if necessary
	 */
	frags = numfrags(fs, size);
	for (allocsiz = frags; allocsiz < fs->fs_frag; allocsiz++)
		if (cgp->cg_frsum[allocsiz] != 0)
			break;
	if (allocsiz == fs->fs_frag) {
		/*
		 * no fragments were available, so a block will be 
		 * allocated, and hacked up
		 */
		if (cgp->cg_cs.cs_nbfree == 0) {
			brelse(bp);
			return (NULL);
		}
		bno = alloccgblk(fs, cgp, bpref);
		bpref = dtogd(fs, bno);
		for (i = frags; i < fs->fs_frag; i++)
			setbit(cg_blksfree(cgp), bpref + i);
		i = fs->fs_frag - frags;
		cgp->cg_cs.cs_nffree += i;
		fs->fs_cstotal.cs_nffree += i;
		fs->fs_cs(fs, cg).cs_nffree += i;
		fs->fs_fmod++;
		cgp->cg_frsum[i]++;
		bdwrite(bp);
		return (bno);
	}
	bno = mapsearch(fs, cgp, bpref, allocsiz);
	if (bno < 0) {
		brelse(bp);
		return (NULL);
	}
	for (i = 0; i < frags; i++)
		clrbit(cg_blksfree(cgp), bno + i);
	cgp->cg_cs.cs_nffree -= frags;
	fs->fs_cstotal.cs_nffree -= frags;
	fs->fs_cs(fs, cg).cs_nffree -= frags;
	fs->fs_fmod++;
	cgp->cg_frsum[allocsiz]--;
	if (frags != allocsiz)
		cgp->cg_frsum[allocsiz - frags]++;
	bdwrite(bp);
	return (cg * fs->fs_fpg + bno);
}

/*
 * Allocate a block in a cylinder group.
 *
 * This algorithm implements the following policy:
 *   1) allocate the requested block.
 *   2) allocate a rotationally optimal block in the same cylinder.
 *   3) allocate the next available block on the block rotor for the
 *      specified cylinder group.
 * Note that this routine only allocates fs_bsize blocks; these
 * blocks may be fragmented by the routine that allocates them.
 */
daddr_t
alloccgblk(fs, cgp, bpref)
	register struct fs *fs;
	register struct cg *cgp;
	daddr_t bpref;
{
	daddr_t bno;
	int cylno, pos, delta;
	short *cylbp;
	register int i;

	if (bpref == 0) {
		bpref = cgp->cg_rotor;
		goto norot;
	}
	bpref = blknum(fs, bpref);
	bpref = dtogd(fs, bpref);
	/*
	 * if the requested block is available, use it
	 */
	if (isblock(fs, cg_blksfree(cgp), fragstoblks(fs, bpref))) {
		bno = bpref;
		goto gotit;
	}
	/*
	 * check for a block available on the same cylinder
	 */
	cylno = cbtocylno(fs, bpref);
	if (cg_blktot(cgp)[cylno] == 0)
		goto norot;
	if (fs->fs_cpc == 0) {
		/*
		 * block layout info is not available, so just have
		 * to take any block in this cylinder.
		 */
		bpref = howmany(fs->fs_spc * cylno, NSPF(fs));
		goto norot;
	}
	/*
	 * check the summary information to see if a block is 
	 * available in the requested cylinder starting at the
	 * requested rotational position and proceeding around.
	 */
	cylbp = cg_blks(fs, cgp, cylno);
	pos = cbtorpos(fs, bpref);
	for (i = pos; i < fs->fs_nrpos; i++)
		if (cylbp[i] > 0)
			break;
	if (i == fs->fs_nrpos)
		for (i = 0; i < pos; i++)
			if (cylbp[i] > 0)
				break;
	if (cylbp[i] > 0) {
		/*
		 * found a rotational position, now find the actual
		 * block. A panic if none is actually there.
		 */
		pos = cylno % fs->fs_cpc;
		bno = (cylno - pos) * fs->fs_spc / NSPB(fs);
		if (fs_postbl(fs, pos)[i] == -1) {
			printf("pos = %d, i = %d, fs = %s\n",
			    pos, i, fs->fs_fsmnt);
			panic("alloccgblk: cyl groups corrupted");
		}
		for (i = fs_postbl(fs, pos)[i];; ) {
			if (isblock(fs, cg_blksfree(cgp), bno + i)) {
				bno = blkstofrags(fs, (bno + i));
				goto gotit;
			}
			delta = fs_rotbl(fs)[i];
			if (delta <= 0 ||
			    delta + i > fragstoblks(fs, fs->fs_fpg))
				break;
			i += delta;
		}
		printf("pos = %d, i = %d, fs = %s\n", pos, i, fs->fs_fsmnt);
		panic("alloccgblk: can't find blk in cyl");
	}
norot:
	/*
	 * no blocks in the requested cylinder, so take next
	 * available one in this cylinder group.
	 */
	bno = mapsearch(fs, cgp, bpref, (int)fs->fs_frag);
	if (bno < 0)
		return (NULL);
	cgp->cg_rotor = bno;
gotit:
	clrblock(fs, cg_blksfree(cgp), (long)fragstoblks(fs, bno));
	cgp->cg_cs.cs_nbfree--;
	fs->fs_cstotal.cs_nbfree--;
	fs->fs_cs(fs, cgp->cg_cgx).cs_nbfree--;
	cylno = cbtocylno(fs, bno);
	cg_blks(fs, cgp, cylno)[cbtorpos(fs, bno)]--;
	cg_blktot(cgp)[cylno]--;
	fs->fs_fmod++;
	return (cgp->cg_cgx * fs->fs_fpg + bno);
}

/*
 * Determine whether an inode can be allocated.
 *
 * Check to see if an inode is available, and if it is,
 * allocate it using the following policy:
 *   1) allocate the requested inode.
 *   2) allocate the next available inode after the requested
 *      inode in the specified cylinder group.
 */
ino_t
ialloccg(ip, cg, ipref, mode)
	struct inode *ip;
	int cg;
	daddr_t ipref;
	int mode;
{
	register struct fs *fs;
	register struct cg *cgp;
	struct buf *bp;
	int error, start, len, loc, map, i;

	fs = ip->i_fs;
	if (fs->fs_cs(fs, cg).cs_nifree == 0)
		return (NULL);
	error = bread(ip->i_devvp, fsbtodb(fs, cgtod(fs, cg)),
		(int)fs->fs_cgsize, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return (NULL);
	}
	cgp = bp->b_un.b_cg;
	if (!cg_chkmagic(cgp) || cgp->cg_cs.cs_nifree == 0) {
		brelse(bp);
		return (NULL);
	}
	cgp->cg_time = time.tv_sec;
	if (ipref) {
		ipref %= fs->fs_ipg;
		if (isclr(cg_inosused(cgp), ipref))
			goto gotit;
	}
	start = cgp->cg_irotor / NBBY;
	len = howmany(fs->fs_ipg - cgp->cg_irotor, NBBY);
	loc = skpc(0xff, len, &cg_inosused(cgp)[start]);
	if (loc == 0) {
		len = start + 1;
		start = 0;
		loc = skpc(0xff, len, &cg_inosused(cgp)[0]);
		if (loc == 0) {
			printf("cg = %s, irotor = %d, fs = %s\n",
			    cg, cgp->cg_irotor, fs->fs_fsmnt);
			panic("ialloccg: map corrupted");
			/* NOTREACHED */
		}
	}
	i = start + len - loc;
	map = cg_inosused(cgp)[i];
	ipref = i * NBBY;
	for (i = 1; i < (1 << NBBY); i <<= 1, ipref++) {
		if ((map & i) == 0) {
			cgp->cg_irotor = ipref;
			goto gotit;
		}
	}
	printf("fs = %s\n", fs->fs_fsmnt);
	panic("ialloccg: block not in map");
	/* NOTREACHED */
gotit:
	setbit(cg_inosused(cgp), ipref);
	cgp->cg_cs.cs_nifree--;
	fs->fs_cstotal.cs_nifree--;
	fs->fs_cs(fs, cg).cs_nifree--;
	fs->fs_fmod++;
	if ((mode & IFMT) == IFDIR) {
		cgp->cg_cs.cs_ndir++;
		fs->fs_cstotal.cs_ndir++;
		fs->fs_cs(fs, cg).cs_ndir++;
	}
	bdwrite(bp);
	return (cg * fs->fs_ipg + ipref);
}

/*
 * Free a block or fragment.
 *
 * The specified block or fragment is placed back in the
 * free map. If a fragment is deallocated, a possible 
 * block reassembly is checked.
 */
blkfree(ip, bno, size)
	register struct inode *ip;
	daddr_t bno;
	off_t size;
{
	register struct fs *fs;
	register struct cg *cgp;
	struct buf *bp;
	int error, cg, blk, frags, bbase;
	register int i;
	struct ucred *cred = curproc->p_ucred;	/* XXX */

	fs = ip->i_fs;
	if ((unsigned)size > fs->fs_bsize || fragoff(fs, size) != 0) {
		printf("dev = 0x%x, bsize = %d, size = %d, fs = %s\n",
		    ip->i_dev, fs->fs_bsize, size, fs->fs_fsmnt);
		panic("blkfree: bad size");
	}
	cg = dtog(fs, bno);
	if ((unsigned)bno >= fs->fs_size) {
		printf("bad block %d, ino %d\n", bno, ip->i_number);
		fserr(fs, cred->cr_uid, "bad block");
		return;
	}
	error = bread(ip->i_devvp, fsbtodb(fs, cgtod(fs, cg)),
		(int)fs->fs_cgsize, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return;
	}
	cgp = bp->b_un.b_cg;
	if (!cg_chkmagic(cgp)) {
		brelse(bp);
		return;
	}
	cgp->cg_time = time.tv_sec;
	bno = dtogd(fs, bno);
	if (size == fs->fs_bsize) {
		if (isblock(fs, cg_blksfree(cgp), fragstoblks(fs, bno))) {
			printf("dev = 0x%x, block = %d, fs = %s\n",
			    ip->i_dev, bno, fs->fs_fsmnt);
			panic("blkfree: freeing free block");
		}
		setblock(fs, cg_blksfree(cgp), fragstoblks(fs, bno));
		cgp->cg_cs.cs_nbfree++;
		fs->fs_cstotal.cs_nbfree++;
		fs->fs_cs(fs, cg).cs_nbfree++;
		i = cbtocylno(fs, bno);
		cg_blks(fs, cgp, i)[cbtorpos(fs, bno)]++;
		cg_blktot(cgp)[i]++;
	} else {
		bbase = bno - fragnum(fs, bno);
		/*
		 * decrement the counts associated with the old frags
		 */
		blk = blkmap(fs, cg_blksfree(cgp), bbase);
		fragacct(fs, blk, cgp->cg_frsum, -1);
		/*
		 * deallocate the fragment
		 */
		frags = numfrags(fs, size);
		for (i = 0; i < frags; i++) {
			if (isset(cg_blksfree(cgp), bno + i)) {
				printf("dev = 0x%x, block = %d, fs = %s\n",
				    ip->i_dev, bno + i, fs->fs_fsmnt);
				panic("blkfree: freeing free frag");
			}
			setbit(cg_blksfree(cgp), bno + i);
		}
		cgp->cg_cs.cs_nffree += i;
		fs->fs_cstotal.cs_nffree += i;
		fs->fs_cs(fs, cg).cs_nffree += i;
		/*
		 * add back in counts associated with the new frags
		 */
		blk = blkmap(fs, cg_blksfree(cgp), bbase);
		fragacct(fs, blk, cgp->cg_frsum, 1);
		/*
		 * if a complete block has been reassembled, account for it
		 */
		if (isblock(fs, cg_blksfree(cgp),
		    (daddr_t)fragstoblks(fs, bbase))) {
			cgp->cg_cs.cs_nffree -= fs->fs_frag;
			fs->fs_cstotal.cs_nffree -= fs->fs_frag;
			fs->fs_cs(fs, cg).cs_nffree -= fs->fs_frag;
			cgp->cg_cs.cs_nbfree++;
			fs->fs_cstotal.cs_nbfree++;
			fs->fs_cs(fs, cg).cs_nbfree++;
			i = cbtocylno(fs, bbase);
			cg_blks(fs, cgp, i)[cbtorpos(fs, bbase)]++;
			cg_blktot(cgp)[i]++;
		}
	}
	fs->fs_fmod++;
	bdwrite(bp);
}

/*
 * Free an inode.
 *
 * The specified inode is placed back in the free map.
 */
ifree(ip, ino, mode)
	struct inode *ip;
	ino_t ino;
	int mode;
{
	register struct fs *fs;
	register struct cg *cgp;
	struct buf *bp;
	int error, cg;

	fs = ip->i_fs;
	if ((unsigned)ino >= fs->fs_ipg*fs->fs_ncg) {
		printf("dev = 0x%x, ino = %d, fs = %s\n",
		    ip->i_dev, ino, fs->fs_fsmnt);
		panic("ifree: range");
	}
	cg = itog(fs, ino);
	error = bread(ip->i_devvp, fsbtodb(fs, cgtod(fs, cg)),
		(int)fs->fs_cgsize, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return;
	}
	cgp = bp->b_un.b_cg;
	if (!cg_chkmagic(cgp)) {
		brelse(bp);
		return;
	}
	cgp->cg_time = time.tv_sec;
	ino %= fs->fs_ipg;
	if (isclr(cg_inosused(cgp), ino)) {
		printf("dev = 0x%x, ino = %d, fs = %s\n",
		    ip->i_dev, ino, fs->fs_fsmnt);
		if (fs->fs_ronly == 0)
			panic("ifree: freeing free inode");
	}
	clrbit(cg_inosused(cgp), ino);
	if (ino < cgp->cg_irotor)
		cgp->cg_irotor = ino;
	cgp->cg_cs.cs_nifree++;
	fs->fs_cstotal.cs_nifree++;
	fs->fs_cs(fs, cg).cs_nifree++;
	if ((mode & IFMT) == IFDIR) {
		cgp->cg_cs.cs_ndir--;
		fs->fs_cstotal.cs_ndir--;
		fs->fs_cs(fs, cg).cs_ndir--;
	}
	fs->fs_fmod++;
	bdwrite(bp);
}

/*
 * Find a block of the specified size in the specified cylinder group.
 *
 * It is a panic if a request is made to find a block if none are
 * available.
 */
daddr_t
mapsearch(fs, cgp, bpref, allocsiz)
	register struct fs *fs;
	register struct cg *cgp;
	daddr_t bpref;
	int allocsiz;
{
	daddr_t bno;
	int start, len, loc, i;
	int blk, field, subfield, pos;

	/*
	 * find the fragment by searching through the free block
	 * map for an appropriate bit pattern
	 */
	if (bpref)
		start = dtogd(fs, bpref) / NBBY;
	else
		start = cgp->cg_frotor / NBBY;
	len = howmany(fs->fs_fpg, NBBY) - start;
	loc = scanc((unsigned)len, (u_char *)&cg_blksfree(cgp)[start],
		(u_char *)fragtbl[fs->fs_frag],
		(u_char)(1 << (allocsiz - 1 + (fs->fs_frag % NBBY))));
	if (loc == 0) {
		len = start + 1;
		start = 0;
		loc = scanc((unsigned)len, (u_char *)&cg_blksfree(cgp)[0],
			(u_char *)fragtbl[fs->fs_frag],
			(u_char)(1 << (allocsiz - 1 + (fs->fs_frag % NBBY))));
		if (loc == 0) {
			printf("start = %d, len = %d, fs = %s\n",
			    start, len, fs->fs_fsmnt);
			panic("alloccg: map corrupted");
			/* NOTREACHED */
		}
	}
	bno = (start + len - loc) * NBBY;
	cgp->cg_frotor = bno;
	/*
	 * found the byte in the map
	 * sift through the bits to find the selected frag
	 */
	for (i = bno + NBBY; bno < i; bno += fs->fs_frag) {
		blk = blkmap(fs, cg_blksfree(cgp), bno);
		blk <<= 1;
		field = around[allocsiz];
		subfield = inside[allocsiz];
		for (pos = 0; pos <= fs->fs_frag - allocsiz; pos++) {
			if ((blk & field) == subfield)
				return (bno + pos);
			field <<= 1;
			subfield <<= 1;
		}
	}
	printf("bno = %d, fs = %s\n", bno, fs->fs_fsmnt);
	panic("alloccg: block not in map");
	return (-1);
}

/*
 * Fserr prints the name of a file system with an error diagnostic.
 * 
 * The form of the error message is:
 *	fs: error message
 */
fserr(fs, uid, cp)
	struct fs *fs;
	uid_t uid;
	char *cp;
{

	log(LOG_ERR, "uid %d on %s: %s\n", uid, fs->fs_fsmnt, cp);
}
