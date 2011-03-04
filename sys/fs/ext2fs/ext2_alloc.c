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
 *	@(#)ffs_alloc.c	8.8 (Berkeley) 2/21/94
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/syslog.h>
#include <sys/buf.h>

#include <fs/ext2fs/inode.h>
#include <fs/ext2fs/ext2_mount.h>
#include <fs/ext2fs/ext2fs.h>
#include <fs/ext2fs/fs.h>
#include <fs/ext2fs/ext2_extern.h>

static daddr_t	ext2_alloccg(struct inode *, int, daddr_t, int);
static u_long	ext2_dirpref(struct inode *);
static void	ext2_fserr(struct m_ext2fs *, uid_t, char *);
static u_long	ext2_hashalloc(struct inode *, int, long, int,
				daddr_t (*)(struct inode *, int, daddr_t, 
						int));
static daddr_t	ext2_nodealloccg(struct inode *, int, daddr_t, int);
static daddr_t  ext2_mapsearch(struct m_ext2fs *, char *, daddr_t);
#ifdef FANCY_REALLOC
static int	ext2_reallocblks(struct vop_reallocblks_args *);
#endif

/*
 * Allocate a block in the file system.
 *
 * A preference may be optionally specified. If a preference is given
 * the following hierarchy is used to allocate a block:
 *   1) allocate the requested block.
 *   2) allocate a rotationally optimal block in the same cylinder.
 *   3) allocate a block in the same cylinder group.
 *   4) quadradically rehash into other cylinder groups, until an
 *        available block is located.
 * If no block preference is given the following hierarchy is used
 * to allocate a block:
 *   1) allocate a block in the cylinder group that contains the
 *        inode for the file.
 *   2) quadradically rehash into other cylinder groups, until an
 *        available block is located.
 */
int
ext2_alloc(ip, lbn, bpref, size, cred, bnp)
	struct inode *ip;
	int32_t lbn, bpref;
	int size;
	struct ucred *cred;
	int32_t *bnp;
{
	struct m_ext2fs *fs;
	struct ext2mount *ump;
	int32_t bno;
	int cg;	
	*bnp = 0;
	fs = ip->i_e2fs;
	ump = ip->i_ump;
	mtx_assert(EXT2_MTX(ump), MA_OWNED);
#ifdef DIAGNOSTIC
	if ((u_int)size > fs->e2fs_bsize || blkoff(fs, size) != 0) {
		vn_printf(ip->i_devvp, "bsize = %lu, size = %d, fs = %s\n",
		    (long unsigned int)fs->e2fs_bsize, size, fs->e2fs_fsmnt);
		panic("ext2_alloc: bad size");
	}
	if (cred == NOCRED)
		panic("ext2_alloc: missing credential");
#endif /* DIAGNOSTIC */
	if (size == fs->e2fs_bsize && fs->e2fs->e2fs_fbcount == 0)
		goto nospace;
	if (cred->cr_uid != 0 && 
		fs->e2fs->e2fs_fbcount < fs->e2fs->e2fs_rbcount)
		goto nospace;
	if (bpref >= fs->e2fs->e2fs_bcount)
		bpref = 0;
	if (bpref == 0)
                cg = ino_to_cg(fs, ip->i_number);
        else
                cg = dtog(fs, bpref);
        bno = (daddr_t)ext2_hashalloc(ip, cg, bpref, fs->e2fs_bsize,
                                                 ext2_alloccg);
        if (bno > 0) {
		/* set next_alloc fields as done in block_getblk */
		ip->i_next_alloc_block = lbn;
		ip->i_next_alloc_goal = bno;

                ip->i_blocks += btodb(fs->e2fs_bsize);
                ip->i_flag |= IN_CHANGE | IN_UPDATE;
                *bnp = bno;
                return (0);
        }
nospace:
	EXT2_UNLOCK(ump);
	ext2_fserr(fs, cred->cr_uid, "file system full");
	uprintf("\n%s: write failed, file system is full\n", fs->e2fs_fsmnt);
	return (ENOSPC);
}

/*
 * Reallocate a sequence of blocks into a contiguous sequence of blocks.
 *
 * The vnode and an array of buffer pointers for a range of sequential
 * logical blocks to be made contiguous is given. The allocator attempts
 * to find a range of sequential blocks starting as close as possible to
 * an fs_rotdelay offset from the end of the allocation for the logical
 * block immediately preceding the current range. If successful, the
 * physical block numbers in the buffer pointers and in the inode are
 * changed to reflect the new allocation. If unsuccessful, the allocation
 * is left unchanged. The success in doing the reallocation is returned.
 * Note that the error return is not reflected back to the user. Rather
 * the previous block allocation will be used.
 */

#ifdef FANCY_REALLOC
SYSCTL_NODE(_vfs, OID_AUTO, ext2fs, CTLFLAG_RW, 0, "EXT2FS filesystem");

static int doasyncfree = 1;
SYSCTL_INT(_vfs_ext2fs, OID_AUTO, doasyncfree, CTLFLAG_RW, &doasyncfree, 0,
    "Use asychronous writes to update block pointers when freeing blocks");

static int doreallocblks = 1;
SYSCTL_INT(_vfs_ext2fs, OID_AUTO, doreallocblks, CTLFLAG_RW, &doreallocblks, 0, "");
#endif

int
ext2_reallocblks(ap)
	struct vop_reallocblks_args /* {
		struct vnode *a_vp;
		struct cluster_save *a_buflist;
	} */ *ap;
{
#ifndef FANCY_REALLOC
/* printf("ext2_reallocblks not implemented\n"); */
return ENOSPC;
#else

	struct m_ext2fs *fs;
	struct inode *ip;
	struct vnode *vp;
	struct buf *sbp, *ebp;
	int32_t *bap, *sbap, *ebap = 0;
	struct ext2mount *ump;
	struct cluster_save *buflist;
	struct indir start_ap[NIADDR + 1], end_ap[NIADDR + 1], *idp;
	int32_t start_lbn, end_lbn, soff, newblk, blkno =0;
	int i, len, start_lvl, end_lvl, pref, ssize;

	vp = ap->a_vp;
	ip = VTOI(vp);
	fs = ip->i_e2fs;
	ump = ip->i_ump;
#ifdef UNKLAR
	if (fs->fs_contigsumsize <= 0)
		return (ENOSPC);
#endif
	buflist = ap->a_buflist;
	len = buflist->bs_nchildren;
	start_lbn = buflist->bs_children[0]->b_lblkno;
	end_lbn = start_lbn + len - 1;
#ifdef DIAGNOSTIC
	for (i = 1; i < len; i++)
		if (buflist->bs_children[i]->b_lblkno != start_lbn + i)
			panic("ext2_reallocblks: non-cluster");
#endif
	/*
	 * If the latest allocation is in a new cylinder group, assume that
	 * the filesystem has decided to move and do not force it back to
	 * the previous cylinder group.
	 */
	if (dtog(fs, dbtofsb(fs, buflist->bs_children[0]->b_blkno)) !=
	    dtog(fs, dbtofsb(fs, buflist->bs_children[len - 1]->b_blkno)))
		return (ENOSPC);
	if (ext2_getlbns(vp, start_lbn, start_ap, &start_lvl) ||
	    ext2_getlbns(vp, end_lbn, end_ap, &end_lvl))
		return (ENOSPC);
	/*
	 * Get the starting offset and block map for the first block.
	 */
	if (start_lvl == 0) {
		sbap = &ip->i_db[0];
		soff = start_lbn;
	} else {
		idp = &start_ap[start_lvl - 1];
		if (bread(vp, idp->in_lbn, (int)fs->e2fs_bsize, NOCRED, &sbp)) {
			brelse(sbp);
			return (ENOSPC);
		}
		sbap = (int32_t *)sbp->b_data;
		soff = idp->in_off;
	}
	/*
	 * Find the preferred location for the cluster.
	 */
	EXT2_LOCK(ump);
	pref = ext2_blkpref(ip, start_lbn, soff, sbap, blkno);
	/*
	 * If the block range spans two block maps, get the second map.
	 */
	if (end_lvl == 0 || (idp = &end_ap[end_lvl - 1])->in_off + 1 >= len) {
		ssize = len;
	} else {
#ifdef DIAGNOSTIC
		if (start_ap[start_lvl-1].in_lbn == idp->in_lbn)
			panic("ext2_reallocblk: start == end");
#endif
		ssize = len - (idp->in_off + 1);
		if (bread(vp, idp->in_lbn, (int)fs->e2fs_bsize, NOCRED, &ebp)){
			EXT2_UNLOCK(ump);	
			goto fail;
		}
		ebap = (int32_t *)ebp->b_data;
	}
	/*
	 * Search the block map looking for an allocation of the desired size.
	 */
	if ((newblk = (int32_t)ext2_hashalloc(ip, dtog(fs, pref), pref,
	    len, ext2_clusteralloc)) == 0){
		EXT2_UNLOCK(ump);
		goto fail;
	}	
	/*
	 * We have found a new contiguous block.
	 *
	 * First we have to replace the old block pointers with the new
	 * block pointers in the inode and indirect blocks associated
	 * with the file.
	 */
	blkno = newblk;
	for (bap = &sbap[soff], i = 0; i < len; i++, blkno += fs->e2fs_fpb) {
		if (i == ssize)
			bap = ebap;
			soff = -i;
#ifdef DIAGNOSTIC
		if (buflist->bs_children[i]->b_blkno != fsbtodb(fs, *bap))
			panic("ext2_reallocblks: alloc mismatch");
#endif
		*bap++ = blkno;
	}
	/*
	 * Next we must write out the modified inode and indirect blocks.
	 * For strict correctness, the writes should be synchronous since
	 * the old block values may have been written to disk. In practise
	 * they are almost never written, but if we are concerned about 
	 * strict correctness, the `doasyncfree' flag should be set to zero.
	 *
	 * The test on `doasyncfree' should be changed to test a flag
	 * that shows whether the associated buffers and inodes have
	 * been written. The flag should be set when the cluster is
	 * started and cleared whenever the buffer or inode is flushed.
	 * We can then check below to see if it is set, and do the
	 * synchronous write only when it has been cleared.
	 */
	if (sbap != &ip->i_db[0]) {
		if (doasyncfree)
			bdwrite(sbp);
		else
			bwrite(sbp);
	} else {
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		if (!doasyncfree)
			ext2_update(vp, 1);
	}
	if (ssize < len) {
		if (doasyncfree)
			bdwrite(ebp);
		else
			bwrite(ebp);
	}
	/*
	 * Last, free the old blocks and assign the new blocks to the buffers.
	 */
	for (blkno = newblk, i = 0; i < len; i++, blkno += fs->e2fs_fpb) {
		ext2_blkfree(ip, dbtofsb(fs, buflist->bs_children[i]->b_blkno),
		    fs->e2fs_bsize);
		buflist->bs_children[i]->b_blkno = fsbtodb(fs, blkno);
	}
	return (0);

fail:
	if (ssize < len)
		brelse(ebp);
	if (sbap != &ip->i_db[0])
		brelse(sbp);
	return (ENOSPC);

#endif /* FANCY_REALLOC */
}

/*
 * Allocate an inode in the file system.
 * 
 */
int
ext2_valloc(pvp, mode, cred, vpp)
	struct vnode *pvp;
	int mode;
	struct ucred *cred;
	struct vnode **vpp;
{
	struct inode *pip;
	struct m_ext2fs *fs;
	struct inode *ip;
	struct ext2mount *ump;
	ino_t ino, ipref;
	int i, error, cg;
	
	*vpp = NULL;
	pip = VTOI(pvp);
	fs = pip->i_e2fs;
	ump = pip->i_ump;

	EXT2_LOCK(ump);
	if (fs->e2fs->e2fs_ficount == 0)
		goto noinodes;
	/*
	 * If it is a directory then obtain a cylinder group based on
	 * ext2_dirpref else obtain it using ino_to_cg. The preferred inode is
	 * always the next inode.
	 */
	if((mode & IFMT) == IFDIR) {
		cg = ext2_dirpref(pip);
		if (fs->e2fs_contigdirs[cg] < 255)
			fs->e2fs_contigdirs[cg]++;
	} else {
		cg = ino_to_cg(fs, pip->i_number);
		if (fs->e2fs_contigdirs[cg] > 0)
			fs->e2fs_contigdirs[cg]--;
	}
	ipref = cg * fs->e2fs->e2fs_ipg + 1;
	ino = (ino_t)ext2_hashalloc(pip, cg, (long)ipref, mode, ext2_nodealloccg);

	if (ino == 0) 
		goto noinodes;
	error = VFS_VGET(pvp->v_mount, ino, LK_EXCLUSIVE, vpp);
	if (error) {
		ext2_vfree(pvp, ino, mode);
		return (error);
	}
	ip = VTOI(*vpp);

	/* 
	  the question is whether using VGET was such good idea at all -
	  Linux doesn't read the old inode in when it's allocating a
	  new one. I will set at least i_size & i_blocks the zero. 
	*/ 
	ip->i_mode = 0;
	ip->i_size = 0;
	ip->i_blocks = 0;
	ip->i_flags = 0;
        /* now we want to make sure that the block pointers are zeroed out */
        for (i = 0; i < NDADDR; i++)
                ip->i_db[i] = 0;
        for (i = 0; i < NIADDR; i++)
                ip->i_ib[i] = 0;

	/*
	 * Set up a new generation number for this inode.
	 * XXX check if this makes sense in ext2
	 */
	if (ip->i_gen == 0 || ++ip->i_gen == 0)
		ip->i_gen = random() / 2 + 1;
/*
printf("ext2_valloc: allocated inode %d\n", ino);
*/
	return (0);
noinodes:
	EXT2_UNLOCK(ump);
	ext2_fserr(fs, cred->cr_uid, "out of inodes");
	uprintf("\n%s: create/symlink failed, no inodes free\n", fs->e2fs_fsmnt);
	return (ENOSPC);
}

/*
 * Find a cylinder to place a directory.
 *
 * The policy implemented by this algorithm is to allocate a
 * directory inode in the same cylinder group as its parent
 * directory, but also to reserve space for its files inodes
 * and data. Restrict the number of directories which may be
 * allocated one after another in the same cylinder group
 * without intervening allocation of files.
 *
 * If we allocate a first level directory then force allocation
 * in another cylinder group.
 *
 */
static u_long
ext2_dirpref(struct inode *pip)
{
	struct m_ext2fs *fs;
        int cg, prefcg, dirsize, cgsize;
	int avgifree, avgbfree, avgndir, curdirsize;
	int minifree, minbfree, maxndir;
	int mincg, minndir;
	int maxcontigdirs;

	mtx_assert(EXT2_MTX(pip->i_ump), MA_OWNED);
	fs = pip->i_e2fs;

 	avgifree = fs->e2fs->e2fs_ficount / fs->e2fs_gcount;
	avgbfree = fs->e2fs->e2fs_fbcount / fs->e2fs_gcount;
	avgndir  = fs->e2fs_total_dir / fs->e2fs_gcount;

	/*
	 * Force allocation in another cg if creating a first level dir.
	 */
	ASSERT_VOP_LOCKED(ITOV(pip), "ext2fs_dirpref");
	if (ITOV(pip)->v_vflag & VV_ROOT) {
		prefcg = arc4random() % fs->e2fs_gcount;
		mincg = prefcg;
		minndir = fs->e2fs_ipg;
		for (cg = prefcg; cg < fs->e2fs_gcount; cg++)
			if (fs->e2fs_gd[cg].ext2bgd_ndirs < minndir &&
			    fs->e2fs_gd[cg].ext2bgd_nifree >= avgifree &&
			    fs->e2fs_gd[cg].ext2bgd_nbfree >= avgbfree) {
				mincg = cg;
				minndir = fs->e2fs_gd[cg].ext2bgd_ndirs;
			}
		for (cg = 0; cg < prefcg; cg++)
			if (fs->e2fs_gd[cg].ext2bgd_ndirs < minndir &&
                            fs->e2fs_gd[cg].ext2bgd_nifree >= avgifree &&
                            fs->e2fs_gd[cg].ext2bgd_nbfree >= avgbfree) {
                                mincg = cg;
                                minndir = fs->e2fs_gd[cg].ext2bgd_ndirs;
                        }

		return (mincg);
	}

	/*
	 * Count various limits which used for
	 * optimal allocation of a directory inode.
	 */
	maxndir = min(avgndir + fs->e2fs_ipg / 16, fs->e2fs_ipg);
	minifree = avgifree - avgifree / 4;
	if (minifree < 1)
		minifree = 1;
	minbfree = avgbfree - avgbfree / 4;
	if (minbfree < 1)
		minbfree = 1;
	cgsize = fs->e2fs_fsize * fs->e2fs_fpg;
	dirsize = AVGDIRSIZE;
	curdirsize = avgndir ? (cgsize - avgbfree * fs->e2fs_bsize) / avgndir : 0;
	if (dirsize < curdirsize)
		dirsize = curdirsize;
	if (dirsize <= 0)
		maxcontigdirs = 0;		/* dirsize overflowed */
	else
		maxcontigdirs = min((avgbfree * fs->e2fs_bsize) / dirsize, 255);
	maxcontigdirs = min(maxcontigdirs, fs->e2fs_ipg / AFPDIR);
	if (maxcontigdirs == 0)
		maxcontigdirs = 1;

	/*
	 * Limit number of dirs in one cg and reserve space for 
	 * regular files, but only if we have no deficit in
	 * inodes or space.
	 */
	prefcg = ino_to_cg(fs, pip->i_number);
	for (cg = prefcg; cg < fs->e2fs_gcount; cg++)
		if (fs->e2fs_gd[cg].ext2bgd_ndirs < maxndir &&
		    fs->e2fs_gd[cg].ext2bgd_nifree >= minifree &&
	    	    fs->e2fs_gd[cg].ext2bgd_nbfree >= minbfree) {
			if (fs->e2fs_contigdirs[cg] < maxcontigdirs)
				return (cg);
		}
	for (cg = 0; cg < prefcg; cg++)
		if (fs->e2fs_gd[cg].ext2bgd_ndirs < maxndir &&
		    fs->e2fs_gd[cg].ext2bgd_nifree >= minifree &&
	    	    fs->e2fs_gd[cg].ext2bgd_nbfree >= minbfree) {
			if (fs->e2fs_contigdirs[cg] < maxcontigdirs)
				return (cg);
		}
	/*
	 * This is a backstop when we have deficit in space.
	 */
	for (cg = prefcg; cg < fs->e2fs_gcount; cg++)
		if (fs->e2fs_gd[cg].ext2bgd_nifree >= avgifree)
			return (cg);
	for (cg = 0; cg < prefcg; cg++)
		if (fs->e2fs_gd[cg].ext2bgd_nifree >= avgifree)
			break;
	return (cg);
}

/*
 * Select the desired position for the next block in a file.  
 *
 * we try to mimic what Remy does in inode_getblk/block_getblk
 *
 * we note: blocknr == 0 means that we're about to allocate either
 * a direct block or a pointer block at the first level of indirection
 * (In other words, stuff that will go in i_db[] or i_ib[])
 *
 * blocknr != 0 means that we're allocating a block that is none
 * of the above. Then, blocknr tells us the number of the block
 * that will hold the pointer
 */
int32_t
ext2_blkpref(ip, lbn, indx, bap, blocknr)
	struct inode *ip;
	int32_t lbn;
	int indx;
	int32_t *bap;
	int32_t blocknr;
{
	int	tmp;
	mtx_assert(EXT2_MTX(ip->i_ump), MA_OWNED);

	/* if the next block is actually what we thought it is,
	   then set the goal to what we thought it should be
	*/
	if(ip->i_next_alloc_block == lbn && ip->i_next_alloc_goal != 0)
		return ip->i_next_alloc_goal;

	/* now check whether we were provided with an array that basically
	   tells us previous blocks to which we want to stay closeby
	*/
	if(bap) 
                for (tmp = indx - 1; tmp >= 0; tmp--) 
			if (bap[tmp]) 
				return bap[tmp];

	/* else let's fall back to the blocknr, or, if there is none,
	   follow the rule that a block should be allocated near its inode
	*/
	return blocknr ? blocknr :
			(int32_t)(ip->i_block_group * 
			EXT2_BLOCKS_PER_GROUP(ip->i_e2fs)) + 
			ip->i_e2fs->e2fs->e2fs_first_dblock;
}

/*
 * Implement the cylinder overflow algorithm.
 *
 * The policy implemented by this algorithm is:
 *   1) allocate the block in its requested cylinder group.
 *   2) quadradically rehash on the cylinder group number.
 *   3) brute force search for a free block.
 */
static u_long
ext2_hashalloc(struct inode *ip, int cg, long pref, int size,
                daddr_t (*allocator)(struct inode *, int, daddr_t, int))
{
	struct m_ext2fs *fs;
	ino_t result;
	int i, icg = cg;

	mtx_assert(EXT2_MTX(ip->i_ump), MA_OWNED);
	fs = ip->i_e2fs;
	/*
	 * 1: preferred cylinder group
	 */
	result = (*allocator)(ip, cg, pref, size);
	if (result)
		return (result);
	/*
	 * 2: quadratic rehash
	 */
	for (i = 1; i < fs->e2fs_gcount; i *= 2) {
		cg += i;
		if (cg >= fs->e2fs_gcount)
			cg -= fs->e2fs_gcount;
		result = (*allocator)(ip, cg, 0, size);
		if (result)
			return (result);
	}
	/*
	 * 3: brute force search
	 * Note that we start at i == 2, since 0 was checked initially,
	 * and 1 is always checked in the quadratic rehash.
	 */
	cg = (icg + 2) % fs->e2fs_gcount;
	for (i = 2; i < fs->e2fs_gcount; i++) {
		result = (*allocator)(ip, cg, 0, size);
		if (result)
			return (result);
		cg++;
		if (cg == fs->e2fs_gcount)
			cg = 0;
	}
	return (0);
}

/*
 * Determine whether a block can be allocated.
 *
 * Check to see if a block of the appropriate size is available,
 * and if it is, allocate it.
 */
static daddr_t
ext2_alloccg(struct inode *ip, int cg, daddr_t bpref, int size)
{
	struct m_ext2fs *fs;
	struct buf *bp;
	struct ext2mount *ump;
	daddr_t bno, runstart, runlen;
	int bit, loc, end, error, start;
	char *bbp;
	/* XXX ondisk32 */
	fs = ip->i_e2fs;
	ump = ip->i_ump;
	if (fs->e2fs_gd[cg].ext2bgd_nbfree == 0)
		return (0);
	EXT2_UNLOCK(ump);
	error = bread(ip->i_devvp, fsbtodb(fs,
		fs->e2fs_gd[cg].ext2bgd_b_bitmap),
		(int)fs->e2fs_bsize, NOCRED, &bp);
	if (error) {
		brelse(bp);
		EXT2_LOCK(ump);
		return (0);
	}
	if (fs->e2fs_gd[cg].ext2bgd_nbfree == 0) {
		/*
		 * Another thread allocated the last block in this
		 * group while we were waiting for the buffer.
		 */
		brelse(bp);
		EXT2_LOCK(ump);
		return (0);
	}
	bbp = (char *)bp->b_data;

	if (dtog(fs, bpref) != cg)
		bpref = 0;
	if (bpref != 0) {
		bpref = dtogd(fs, bpref);
		/*
		 * if the requested block is available, use it
		 */
		if (isclr(bbp, bpref)) {
			bno = bpref;
			goto gotit;
		}
	}
	/*
	 * no blocks in the requested cylinder, so take next
	 * available one in this cylinder group.
	 * first try to get 8 contigous blocks, then fall back to a single
	 * block.
	 */
	if (bpref)
		start = dtogd(fs, bpref) / NBBY;
	else
		start = 0;
	end = howmany(fs->e2fs->e2fs_fpg, NBBY) - start;
retry:
	runlen = 0;
	runstart = 0;
	for (loc = start; loc < end; loc++) {
		if (bbp[loc] == (char)0xff) {
			runlen = 0;
			continue;
		}

		/* Start of a run, find the number of high clear bits. */
		if (runlen == 0) {
			bit = fls(bbp[loc]);
			runlen = NBBY - bit;
			runstart = loc * NBBY + bit;
		} else if (bbp[loc] == 0) {
			/* Continue a run. */
			runlen += NBBY;
		} else {
			/*
			 * Finish the current run.  If it isn't long
			 * enough, start a new one.
			 */
			bit = ffs(bbp[loc]) - 1;
			runlen += bit;
			if (runlen >= 8) {
				bno = runstart;
				goto gotit;
			}

			/* Run was too short, start a new one. */
			bit = fls(bbp[loc]);
			runlen = NBBY - bit;
			runstart = loc * NBBY + bit;
		}

		/* If the current run is long enough, use it. */
		if (runlen >= 8) {
			bno = runstart;
			goto gotit;
		}
	}
	if (start != 0) {
		end = start;
		start = 0;
		goto retry;
	}

	bno = ext2_mapsearch(fs, bbp, bpref);
	if (bno < 0){
		brelse(bp);
		EXT2_LOCK(ump);
		return (0);
	}
gotit:
#ifdef DIAGNOSTIC
	if (isset(bbp, bno)) {
		printf("ext2fs_alloccgblk: cg=%d bno=%jd fs=%s\n",
			cg, (intmax_t)bno, fs->e2fs_fsmnt);
		panic("ext2fs_alloccg: dup alloc");
	}
#endif
	setbit(bbp, bno);
	EXT2_LOCK(ump);
	fs->e2fs->e2fs_fbcount--;
	fs->e2fs_gd[cg].ext2bgd_nbfree--;
	fs->e2fs_fmod = 1;
	EXT2_UNLOCK(ump);
	bdwrite(bp);
	return (cg * fs->e2fs->e2fs_fpg + fs->e2fs->e2fs_first_dblock + bno);
}

/*
 * Determine whether an inode can be allocated.
 *
 * Check to see if an inode is available, and if it is,
 * allocate it using tode in the specified cylinder group.
 */
static daddr_t
ext2_nodealloccg(struct inode *ip, int cg, daddr_t ipref, int mode)
{
	struct m_ext2fs *fs;
	struct buf *bp;
	struct ext2mount *ump;
	int error, start, len, loc, map, i;
	char *ibp;
	ipref--; /* to avoid a lot of (ipref -1) */
	if (ipref == -1)
		ipref = 0;
	fs = ip->i_e2fs;
	ump = ip->i_ump;
	if (fs->e2fs_gd[cg].ext2bgd_nifree == 0)
		return (0);
	EXT2_UNLOCK(ump);	
	error = bread(ip->i_devvp, fsbtodb(fs,
		fs->e2fs_gd[cg].ext2bgd_i_bitmap),
		(int)fs->e2fs_bsize, NOCRED, &bp);
	if (error) {
		brelse(bp);
		EXT2_LOCK(ump);
		return (0);
	}
	if (fs->e2fs_gd[cg].ext2bgd_nifree == 0) {
		/*
		 * Another thread allocated the last i-node in this
		 * group while we were waiting for the buffer.
		 */
		brelse(bp);
		EXT2_LOCK(ump);
		return (0);
	}
	ibp = (char *)bp->b_data;
	if (ipref) {
		ipref %= fs->e2fs->e2fs_ipg;
		if (isclr(ibp, ipref))
			goto gotit;
	}
	start = ipref / NBBY;
	len = howmany(fs->e2fs->e2fs_ipg - ipref, NBBY);
	loc = skpc(0xff, len, &ibp[start]);
	if (loc == 0) {
		len = start + 1;
		start = 0;
		loc = skpc(0xff, len, &ibp[0]);
		if (loc == 0) {
			printf("cg = %d, ipref = %lld, fs = %s\n",
				cg, (long long)ipref, fs->e2fs_fsmnt);
			panic("ext2fs_nodealloccg: map corrupted");
			/* NOTREACHED */
		}
	} 
	i = start + len - loc;
	map = ibp[i] ^ 0xff;
	if (map == 0) {
		printf("fs = %s\n", fs->e2fs_fsmnt);
		panic("ext2fs_nodealloccg: block not in map");
	}
	ipref = i * NBBY + ffs(map) - 1;
gotit:
	setbit(ibp, ipref);
	EXT2_LOCK(ump);
	fs->e2fs_gd[cg].ext2bgd_nifree--;
	fs->e2fs->e2fs_ficount--;
	fs->e2fs_fmod = 1;
	if ((mode & IFMT) == IFDIR) {
		fs->e2fs_gd[cg].ext2bgd_ndirs++;
		fs->e2fs_total_dir++;
	}
	EXT2_UNLOCK(ump);
	bdwrite(bp);
	return (cg * fs->e2fs->e2fs_ipg + ipref +1);
}

/*
 * Free a block or fragment.
 *
 */
void
ext2_blkfree(ip, bno, size)
	struct inode *ip;
	int32_t bno;
	long size;
{
	struct m_ext2fs *fs;
	struct buf *bp;
	struct ext2mount *ump;
	int cg, error;
	char *bbp;

	fs = ip->i_e2fs;
	ump = ip->i_ump;
	cg = dtog(fs, bno);
	if ((u_int)bno >= fs->e2fs->e2fs_bcount) {
                printf("bad block %lld, ino %llu\n", (long long)bno,
                    (unsigned long long)ip->i_number);
                ext2_fserr(fs, ip->i_uid, "bad block");
                return;
        }
        error = bread(ip->i_devvp,
                fsbtodb(fs, fs->e2fs_gd[cg].ext2bgd_b_bitmap),
                (int)fs->e2fs_bsize, NOCRED, &bp);
        if (error) {
                brelse(bp);
                return;
        }
        bbp = (char *)bp->b_data;
        bno = dtogd(fs, bno);
        if (isclr(bbp, bno)) {
                printf("block = %lld, fs = %s\n",
                     (long long)bno, fs->e2fs_fsmnt);
                panic("blkfree: freeing free block");
        }
        clrbit(bbp, bno);
	EXT2_LOCK(ump);
        fs->e2fs->e2fs_fbcount++;
        fs->e2fs_gd[cg].ext2bgd_nbfree++;
        fs->e2fs_fmod = 1;
	EXT2_UNLOCK(ump);
        bdwrite(bp);
}

/*
 * Free an inode.
 *
 */
int
ext2_vfree(pvp, ino, mode)
	struct vnode *pvp;
	ino_t ino;
	int mode;
{
	struct m_ext2fs *fs;
	struct inode *pip;
	struct buf *bp;
	struct ext2mount *ump;
	int error, cg;
	char * ibp;
/*	mode_t save_i_mode; */

	pip = VTOI(pvp);
	fs = pip->i_e2fs;
	ump = pip->i_ump;
	if ((u_int)ino > fs->e2fs_ipg * fs->e2fs_gcount)
		panic("ext2_vfree: range: devvp = %p, ino = %d, fs = %s",
		    pip->i_devvp, ino, fs->e2fs_fsmnt);

	cg = ino_to_cg(fs, ino);
	error = bread(pip->i_devvp,
		fsbtodb(fs, fs->e2fs_gd[cg].ext2bgd_i_bitmap),
		(int)fs->e2fs_bsize, NOCRED, &bp);
	if (error) {
		brelse(bp);
		return (0);
	}
	ibp = (char *)bp->b_data;
	ino = (ino - 1) % fs->e2fs->e2fs_ipg;
	if (isclr(ibp, ino)) {
		printf("ino = %llu, fs = %s\n",
			 (unsigned long long)ino, fs->e2fs_fsmnt);
		if (fs->e2fs_ronly == 0)
			panic("ifree: freeing free inode");
	}
	clrbit(ibp, ino);
	EXT2_LOCK(ump);
	fs->e2fs->e2fs_ficount++;
	fs->e2fs_gd[cg].ext2bgd_nifree++;
	if ((mode & IFMT) == IFDIR) {
		fs->e2fs_gd[cg].ext2bgd_ndirs--;
		fs->e2fs_total_dir--;
	}
	fs->e2fs_fmod = 1;
	EXT2_UNLOCK(ump);
	bdwrite(bp);
	return (0);
}

/*
 * Find a block in the specified cylinder group.
 *
 * It is a panic if a request is made to find a block if none are
 * available.
 */
static daddr_t
ext2_mapsearch(struct m_ext2fs *fs, char *bbp, daddr_t bpref)
{
	int start, len, loc, i, map;

	/*
	 * find the fragment by searching through the free block
	 * map for an appropriate bit pattern
	 */
	if (bpref)
		start = dtogd(fs, bpref) / NBBY;
	else
		start = 0;
	len = howmany(fs->e2fs->e2fs_fpg, NBBY) - start;
	loc = skpc(0xff, len, &bbp[start]);
	if (loc == 0) {
		len = start + 1;
		start = 0;
		loc = skpc(0xff, len, &bbp[start]);
		if (loc == 0) {
			printf("start = %d, len = %d, fs = %s\n",
				start, len, fs->e2fs_fsmnt);
			panic("ext2fs_alloccg: map corrupted");
			/* NOTREACHED */
		}
	}
	i = start + len - loc;
	map = bbp[i] ^ 0xff;
	if (map == 0) {
		printf("fs = %s\n", fs->e2fs_fsmnt);
		panic("ext2fs_mapsearch: block not in map");
	}
	return (i * NBBY + ffs(map) - 1);
}

/*
 * Fserr prints the name of a file system with an error diagnostic.
 * 
 * The form of the error message is:
 *	fs: error message
 */
static void
ext2_fserr(fs, uid, cp)
	struct m_ext2fs *fs;
	uid_t uid;
	char *cp;
{

	log(LOG_ERR, "uid %u on %s: %s\n", uid, fs->e2fs_fsmnt, cp);
}

int
cg_has_sb(int i)
{
        int a3, a5, a7;

        if (i == 0 || i == 1)
                return 1;
        for (a3 = 3, a5 = 5, a7 = 7;
            a3 <= i || a5 <= i || a7 <= i;
            a3 *= 3, a5 *= 5, a7 *= 7)
                if (i == a3 || i == a5 || i == a7)
                        return 1;
        return 0;
}
