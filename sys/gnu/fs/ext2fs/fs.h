/*
 *  modified for EXT2FS support in Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
/*
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)fs.h	8.7 (Berkeley) 4/19/94
 */

/*
 * Each disk drive contains some number of file systems.
 * A file system consists of a number of cylinder groups.
 * Each cylinder group has inodes and data.
 *
 * A file system is described by its super-block, which in turn
 * describes the cylinder groups.  The super-block is critical
 * data and is replicated in each cylinder group to protect against
 * catastrophic loss.  This is done at `newfs' time and the critical
 * super-block data does not change, so the copies need not be
 * referenced further unless disaster strikes.
 *
 * The first boot and super blocks are given in absolute disk addresses.
 * The byte-offset forms are preferred, as they don't imply a sector size.
 */
#define BBSIZE		1024
#define SBSIZE		1024
#define	BBOFF		((off_t)(0))
#define	SBOFF		((off_t)(BBOFF + BBSIZE))
#define	BBLOCK		((daddr_t)(0))
#define	SBLOCK		((daddr_t)(BBLOCK + BBSIZE / DEV_BSIZE))

/*
 * The path name on which the file system is mounted is maintained
 * in fs_fsmnt. MAXMNTLEN defines the amount of space allocated in 
 * the super block for this name.
 */
#define MAXMNTLEN 512

/*
 * Macros for access to superblock array structures
 */

/*
 * Convert cylinder group to base address of its global summary info.
 */
#define fs_cs(fs, cgindx)      (((struct ext2_group_desc *) \
        (fs->s_group_desc[cgindx / EXT2_DESC_PER_BLOCK(fs)]->b_data)) \
		[cgindx % EXT2_DESC_PER_BLOCK(fs)])

/*
 * Turn file system block numbers into disk block addresses.
 * This maps file system blocks to device size blocks.
 */
#define fsbtodb(fs, b)	((b) << ((fs)->s_fsbtodb))
#define	dbtofsb(fs, b)	((b) >> ((fs)->s_fsbtodb))

/* get group containing inode */
#define ino_to_cg(fs, x)	(((x) - 1) / EXT2_INODES_PER_GROUP(fs))

/* get block containing inode from its number x */
#define	ino_to_fsba(fs, x)	fs_cs(fs, ino_to_cg(fs, x)).bg_inode_table + \
	(((x)-1) % EXT2_INODES_PER_GROUP(fs))/EXT2_INODES_PER_BLOCK(fs)

/* get offset for inode in block */
#define	ino_to_fsbo(fs, x)	((x-1) % EXT2_INODES_PER_BLOCK(fs))

/*
 * Give cylinder group number for a file system block.
 * Give cylinder group block number for a file system block.
 */
#define	dtog(fs, d)	(((d) - fs->s_es->s_first_data_block) / \
			EXT2_BLOCKS_PER_GROUP(fs))
#define	dtogd(fs, d)	(((d) - fs->s_es->s_first_data_block) % \
			EXT2_BLOCKS_PER_GROUP(fs))

/*
 * The following macros optimize certain frequently calculated
 * quantities by using shifts and masks in place of divisions
 * modulos and multiplications.
 */
#define blkoff(fs, loc)		/* calculates (loc % fs->fs_bsize) */ \
	((loc) & (fs)->s_qbmask)

#define lblktosize(fs, blk)	/* calculates (blk * fs->fs_bsize) */ \
	((blk) << (fs->s_bshift))

#define lblkno(fs, loc)		/* calculates (loc / fs->fs_bsize) */ \
	((loc) >> (fs->s_bshift))

/* no fragments -> logical block number equal # of frags */
#define numfrags(fs, loc)	/* calculates (loc / fs->fs_fsize) */ \
	((loc) >> (fs->s_bshift))

#define fragroundup(fs, size)	/* calculates roundup(size, fs->fs_fsize) */ \
	roundup(size, fs->s_frag_size)
	/* was (((size) + (fs)->fs_qfmask) & (fs)->fs_fmask) */

/*
 * Determining the size of a file block in the file system.
 * easy w/o fragments
 */
#define blksize(fs, ip, lbn) ((fs)->s_frag_size)

/*
 * INOPB is the number of inodes in a secondary storage block.
 */
#define	INOPB(fs)	EXT2_INODES_PER_BLOCK(fs)

/*
 * NINDIR is the number of indirects in a file system block.
 */
#define	NINDIR(fs)	(EXT2_ADDR_PER_BLOCK(fs))

extern int inside[], around[];
extern u_char *fragtbl[];

/* a few remarks about superblock locking/unlocking
 * Linux provides special routines for doing so
 * I haven't figured out yet what BSD does
 * I think I'll try a VOP_LOCK/VOP_UNLOCK on the device vnode
 */
#define  DEVVP(inode)		(VFSTOUFS(ITOV(inode)->v_mount)->um_devvp)
#define  lock_super(devvp)   	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, curproc)
#define  unlock_super(devvp) 	VOP_UNLOCK(devvp, 0, curproc)

/*
 * To lock a buffer, set the B_LOCKED flag and then brelse() it. To unlock,
 * reset the B_LOCKED flag and brelse() the buffer back on the LRU list
 */
#define LCK_BUF(bp) { \
	int s; \
	s = splbio(); \
	(bp)->b_flags |= B_LOCKED; \
	splx(s); \
	brelse(bp); \
}

#define ULCK_BUF(bp) { \
	int s; \
	s = splbio(); \
	(bp)->b_flags &= ~B_LOCKED; \
	splx(s); \
	bremfree(bp); \
	brelse(bp); \
}
