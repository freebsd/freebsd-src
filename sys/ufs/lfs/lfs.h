/*-
 * Copyright (c) 1991, 1993
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
 *	@(#)lfs.h	8.9 (Berkeley) 5/8/95
 */

#define	LFS_LABELPAD	8192		/* LFS label size */
#define	LFS_SBPAD	8192		/* LFS superblock size */

/*
 * XXX
 * This is a kluge and NEEDS to go away.
 *
 * Right now, ufs code handles most of the calls for directory operations
 * such as create, mkdir, link, etc.  As a result VOP_UPDATE is being
 * called with waitfor set (since ffs does these things synchronously).
 * Since LFS does not want to do these synchronously, we treat the last
 * argument to lfs_update as a set of flags.  If LFS_SYNC is set, then
 * the update should be synchronous, if not, do it asynchronously.
 * Unfortunately, this means that LFS won't work with NFS yet because
 * NFS goes through paths that will make normal calls to ufs which will
 * call lfs with a last argument of 1.
 */
#define	LFS_SYNC	0x02

/* On-disk and in-memory checkpoint segment usage structure. */
typedef struct segusage SEGUSE;
struct segusage {
	u_int32_t su_nbytes;		/* number of live bytes */
	u_int32_t su_lastmod;		/* SEGUSE last modified timestamp */
	u_int16_t su_nsums;		/* number of summaries in segment */
	u_int16_t su_ninos;		/* number of inode blocks in seg */

#define	SEGUSE_ACTIVE		0x01	/* segment is currently being written */
#define	SEGUSE_DIRTY		0x02	/* segment has data in it */
#define	SEGUSE_SUPERBLOCK	0x04	/* segment contains a superblock */
	u_int32_t su_flags;
};

#define	SEGUPB(fs)	(1 << (fs)->lfs_sushift)
#define	SEGTABSIZE_SU(fs)						\
	(((fs)->lfs_nseg + SEGUPB(fs) - 1) >> (fs)->lfs_sushift)

/* On-disk file information.  One per file with data blocks in the segment. */
typedef struct finfo FINFO;
struct finfo {
	u_int32_t fi_nblocks;		/* number of blocks */
	u_int32_t fi_version;		/* version number */
	u_int32_t fi_ino;		/* inode number */
	u_int32_t fi_lastlength;	/* length of last block in array */
	ufs_daddr_t	  fi_blocks[1];	/* array of logical block numbers */
};

/* On-disk and in-memory super block. */
struct lfs {
#define	LFS_MAGIC	0x070162
	u_int32_t lfs_magic;		/* magic number */
#define	LFS_VERSION	1
	u_int32_t lfs_version;		/* version number */

	u_int32_t lfs_size;		/* number of blocks in fs */
	u_int32_t lfs_ssize;		/* number of blocks per segment */
	u_int32_t lfs_dsize;		/* number of disk blocks in fs */
	u_int32_t lfs_bsize;		/* file system block size */
	u_int32_t lfs_fsize;		/* size of frag blocks in fs */
	u_int32_t lfs_frag;		/* number of frags in a block in fs */

/* Checkpoint region. */
	ino_t	  lfs_free;		/* start of the free list */
	u_int32_t lfs_bfree;		/* number of free disk blocks */
	u_int32_t lfs_nfiles;		/* number of allocated inodes */
	int32_t	  lfs_avail;		/* blocks available for writing */
	u_int32_t lfs_uinodes;		/* inodes in cache not yet on disk */
	ufs_daddr_t lfs_idaddr;		/* inode file disk address */
	ino_t	  lfs_ifile;		/* inode file inode number */
	ufs_daddr_t lfs_lastseg;	/* address of last segment written */
	ufs_daddr_t lfs_nextseg;	/* address of next segment to write */
	ufs_daddr_t lfs_curseg;		/* current segment being written */
	ufs_daddr_t lfs_offset;		/* offset in curseg for next partial */
	ufs_daddr_t lfs_lastpseg;	/* address of last partial written */
	u_int32_t lfs_tstamp;		/* time stamp */

/* These are configuration parameters. */
	u_int32_t lfs_minfree;		/* minimum percentage of free blocks */

/* These fields can be computed from the others. */
	u_int64_t lfs_maxfilesize;	/* maximum representable file size */
	u_int32_t lfs_dbpseg;		/* disk blocks per segment */
	u_int32_t lfs_inopb;		/* inodes per block */
	u_int32_t lfs_ifpb;		/* IFILE entries per block */
	u_int32_t lfs_sepb;		/* SEGUSE entries per block */
	u_int32_t lfs_nindir;		/* indirect pointers per block */
	u_int32_t lfs_nseg;		/* number of segments */
	u_int32_t lfs_nspf;		/* number of sectors per fragment */
	u_int32_t lfs_cleansz;		/* cleaner info size in blocks */
	u_int32_t lfs_segtabsz;		/* segment table size in blocks */

	u_int32_t lfs_segmask;		/* calculate offset within a segment */
	u_int32_t lfs_segshift;		/* fast mult/div for segments */
	u_int64_t lfs_bmask;		/* calc block offset from file offset */
	u_int32_t lfs_bshift;		/* calc block number from file offset */
	u_int64_t lfs_ffmask;		/* calc frag offset from file offset */
	u_int32_t lfs_ffshift;		/* fast mult/div for frag from file */
	u_int64_t lfs_fbmask;		/* calc frag offset from block offset */
	u_int32_t lfs_fbshift;		/* fast mult/div for frag from block */
	u_int32_t lfs_fsbtodb;		/* fsbtodb and dbtofsb shift constant */
	u_int32_t lfs_sushift;		/* fast mult/div for segusage table */

	int32_t	  lfs_maxsymlinklen;	/* max length of an internal symlink */

#define	LFS_MIN_SBINTERVAL	5	/* minimum superblock segment spacing */
#define	LFS_MAXNUMSB		10	/* superblock disk offsets */
	ufs_daddr_t lfs_sboffs[LFS_MAXNUMSB];

/* Checksum -- last valid disk field. */
	u_int32_t lfs_cksum;		/* checksum for superblock checking */

/* These fields are set at mount time and are meaningless on disk. */
	struct segment *lfs_sp;		/* current segment being written */
	struct vnode *lfs_ivnode;	/* vnode for the ifile */
	u_long	  lfs_seglock;		/* single-thread the segment writer */
	pid_t	  lfs_lockpid;		/* pid of lock holder */
	u_long	  lfs_iocount;		/* number of ios pending */
	u_long	  lfs_writer;		/* don't allow any dirops to start */
	u_long	  lfs_dirops;		/* count of active directory ops */
	u_long	  lfs_doifile;		/* Write ifile blocks on next write */
	u_long	  lfs_nactive;		/* Number of segments since last ckp */
	int8_t	  lfs_fmod;		/* super block modified flag */
	int8_t	  lfs_clean;		/* file system is clean flag */
	int8_t	  lfs_ronly;		/* mounted read-only flag */
	int8_t	  lfs_flags;		/* currently unused flag */
	u_char	  lfs_fsmnt[MNAMELEN];	/* name mounted on */

	int32_t	  lfs_pad[40];		/* round to 512 bytes */
};

/*
 * Inode 0:	out-of-band inode number
 * Inode 1:	IFILE inode number
 * Inode 2:	root inode
 * Inode 3:	lost+found inode number
 */
#define	LFS_UNUSED_INUM	0		/* out of band inode number */
#define	LFS_IFILE_INUM	1		/* IFILE inode number */
#define	LOSTFOUNDINO	3		/* lost+found inode number */
#define	LFS_FIRST_INUM	4		/* first free inode number */

/* Address calculations for metadata located in the inode */
#define	S_INDIR(fs)	-NDADDR
#define	D_INDIR(fs)	(S_INDIR(fs) - NINDIR(fs) - 1)
#define	T_INDIR(fs)	(D_INDIR(fs) - NINDIR(fs) * NINDIR(fs) - 1)

/* Unassigned disk address. */
#define	UNASSIGNED	-1

/* Unused logical block number */
#define LFS_UNUSED_LBN	-1

typedef struct ifile IFILE;
struct ifile {
	u_int32_t if_version;		/* inode version number */
#define	LFS_UNUSED_DADDR	0	/* out-of-band daddr */
	ufs_daddr_t if_daddr;		/* inode disk address */
	ino_t	  if_nextfree;		/* next-unallocated inode */
};

/*
 * Cleaner information structure.  This resides in the ifile and is used
 * to pass information between the cleaner and the kernel.
 */
typedef struct _cleanerinfo {
	u_int32_t clean;		/* K: number of clean segments */
	u_int32_t dirty;		/* K: number of dirty segments */
} CLEANERINFO;

#define	CLEANSIZE_SU(fs)						\
	((sizeof(CLEANERINFO) + (fs)->lfs_bsize - 1) >> (fs)->lfs_bshift)

/*
 * All summary blocks are the same size, so we can always read a summary
 * block easily from a segment.
 */
#define	LFS_SUMMARY_SIZE	512

/* On-disk segment summary information */
typedef struct segsum SEGSUM;
struct segsum {
	u_int32_t ss_sumsum;		/* check sum of summary block */
	u_int32_t ss_datasum;		/* check sum of data */
	u_int32_t ss_magic;		/* segment summary magic number */
#define SS_MAGIC	0x061561
	ufs_daddr_t ss_next;		/* next segment */
	u_int32_t ss_create;		/* creation time stamp */
	u_int16_t ss_nfinfo;		/* number of file info structures */
	u_int16_t ss_ninos;		/* number of inodes in summary */

#define	SS_DIROP	0x01		/* segment begins a dirop */
#define	SS_CONT		0x02		/* more partials to finish this write*/
	u_int16_t ss_flags;		/* used for directory operations */
	u_int16_t ss_pad;		/* extra space */
	/* FINFO's and inode daddr's... */
};

/* NINDIR is the number of indirects in a file system block. */
#define	NINDIR(fs)	((fs)->lfs_nindir)

/* INOPB is the number of inodes in a secondary storage block. */
#define	INOPB(fs)	((fs)->lfs_inopb)

#define blksize(fs, ip, lbn) \
	(((lbn) >= NDADDR || (ip)->i_size >= ((lbn) + 1) << (fs)->lfs_bshift) \
	    ? (fs)->lfs_bsize \
	    : (fragroundup(fs, blkoff(fs, (ip)->i_size))))
#define	blkoff(fs, loc)		((int)((loc) & (fs)->lfs_bmask))
#define fragoff(fs, loc)	/* calculates (loc % fs->lfs_fsize) */ \
	((int)((loc) & (fs)->lfs_ffmask))
#define	fsbtodb(fs, b)		((b) << (fs)->lfs_fsbtodb)
#define	dbtofsb(fs, b)		((b) >> (fs)->lfs_fsbtodb)
#define	fragstodb(fs, b)	((b) << (fs)->lfs_fsbtodb - (fs)->lfs_fbshift)
#define	dbtofrags(fs, b)	((b) >> (fs)->lfs_fsbtodb - (fs)->lfs_fbshift)
#define	lblkno(fs, loc)		((loc) >> (fs)->lfs_bshift)
#define	lblktosize(fs, blk)	((blk) << (fs)->lfs_bshift)
#define numfrags(fs, loc)	/* calculates (loc / fs->lfs_fsize) */ \
	((loc) >> (fs)->lfs_ffshift)
#define blkroundup(fs, size)	/* calculates roundup(size, fs->lfs_bsize) */ \
	((int)(((size) + (fs)->lfs_bmask) & (~(fs)->lfs_bmask)))
#define fragroundup(fs, size)	/* calculates roundup(size, fs->lfs_fsize) */ \
	((int)(((size) + (fs)->lfs_ffmask) & (~(fs)->lfs_ffmask)))
#define fragstoblks(fs, frags)	/* calculates (frags / fs->lfs_frag) */ \
	((frags) >> (fs)->lfs_fbshift)
#define blkstofrags(fs, blks)	/* calculates (blks * fs->lfs_frag) */ \
	((blks) << (fs)->lfs_fbshift)
#define fragnum(fs, fsb)	/* calculates (fsb % fs->lfs_frag) */ \
	((fsb) & ((fs)->lfs_frag - 1))
#define blknum(fs, fsb)		/* calculates rounddown(fsb, fs->lfs_frag) */ \
	((fsb) &~ ((fs)->lfs_frag - 1))
#define dblksize(fs, dip, lbn) \
	(((lbn) >= NDADDR || (dip)->di_size >= ((lbn) + 1) << (fs)->lfs_bshift)\
	    ? (fs)->lfs_bsize \
	    : (fragroundup(fs, blkoff(fs, (dip)->di_size))))
#define	datosn(fs, daddr)	/* disk address to segment number */	\
	(((daddr) - (fs)->lfs_sboffs[0]) / fsbtodb((fs), (fs)->lfs_ssize))
#define sntoda(fs, sn) 		/* segment number to disk address */	\
	((ufs_daddr_t)((sn) * ((fs)->lfs_ssize << (fs)->lfs_fsbtodb) +	\
	    (fs)->lfs_sboffs[0]))

/* Read in the block with the cleaner info from the ifile. */
#define LFS_CLEANERINFO(CP, F, BP) {					\
	VTOI((F)->lfs_ivnode)->i_flag |= IN_ACCESS;			\
	if (bread((F)->lfs_ivnode,					\
	    (ufs_daddr_t)0, (F)->lfs_bsize, NOCRED, &(BP)))		\
		panic("lfs: ifile read");				\
	(CP) = (CLEANERINFO *)(BP)->b_data;				\
}

/* Read in the block with a specific inode from the ifile. */
#define	LFS_IENTRY(IP, F, IN, BP) {					\
	int _e;								\
	VTOI((F)->lfs_ivnode)->i_flag |= IN_ACCESS;			\
	if (_e = bread((F)->lfs_ivnode,					\
	    (IN) / (F)->lfs_ifpb + (F)->lfs_cleansz + (F)->lfs_segtabsz,\
	    (F)->lfs_bsize, NOCRED, &(BP)))				\
		panic("lfs: ifile read %d", _e);			\
	(IP) = (IFILE *)(BP)->b_data + (IN) % (F)->lfs_ifpb;		\
}

/* Read in the block with a specific segment usage entry from the ifile. */
#define	LFS_SEGENTRY(SP, F, IN, BP) {					\
	int _e;								\
	VTOI((F)->lfs_ivnode)->i_flag |= IN_ACCESS;			\
	if (_e = bread((F)->lfs_ivnode,					\
	    ((IN) >> (F)->lfs_sushift) + (F)->lfs_cleansz,		\
	    (F)->lfs_bsize, NOCRED, &(BP)))				\
		panic("lfs: ifile read: %d", _e);			\
	(SP) = (SEGUSE *)(BP)->b_data + ((IN) & (F)->lfs_sepb - 1);	\
}

/* 
 * Determine if there is enough room currently available to write db
 * disk blocks.  We need enough blocks for the new blocks, the current,
 * inode blocks, a summary block, plus potentially the ifile inode and
 * the segment usage table, plus an ifile page.
 */
#define LFS_FITS(fs, db)						\
	((int32_t)((db + ((fs)->lfs_uinodes + INOPB((fs))) / 		\
	INOPB((fs)) + fsbtodb(fs, 1) + LFS_SUMMARY_SIZE / DEV_BSIZE +	\
	(fs)->lfs_segtabsz)) < (fs)->lfs_avail)

/* Determine if a buffer belongs to the ifile */
#define IS_IFILE(bp)	(VTOI(bp->b_vp)->i_number == LFS_IFILE_INUM)

/*
 * Structures used by lfs_bmapv and lfs_markv to communicate information
 * about inodes and data blocks.
 */
typedef struct block_info {
	ino_t	bi_inode;		/* inode # */
	ufs_daddr_t bi_lbn;		/* logical block w/in file */
	ufs_daddr_t bi_daddr;		/* disk address of block */
	time_t	bi_segcreate;		/* origin segment create time */
	int	bi_version;		/* file version number */
	void	*bi_bp;			/* data buffer */
	int     bi_size;                /* size of the block (if fragment) */
} BLOCK_INFO;

/* In-memory description of a segment about to be written. */
struct segment {
	struct lfs	 *fs;		/* file system pointer */
	struct buf	**bpp;		/* pointer to buffer array */
	struct buf	**cbpp;		/* pointer to next available bp */
	struct buf	**start_bpp;	/* pointer to first bp in this set */
	struct buf	 *ibp;		/* buffer pointer to inode page */
	struct finfo	 *fip;		/* current fileinfo pointer */
	struct vnode	 *vp;		/* vnode being gathered */
	void	 *segsum;		/* segment summary info */
	u_int32_t ninodes;		/* number of inodes in this segment */
	u_int32_t seg_bytes_left;	/* bytes left in segment */
	u_int32_t sum_bytes_left;	/* bytes left in summary block */
	u_int32_t seg_number;		/* number of this segment */
	ufs_daddr_t *start_lbp;		/* beginning lbn for this set */

#define	SEGM_CKP	0x01		/* doing a checkpoint */
#define	SEGM_CLEAN	0x02		/* cleaner call; don't sort */
#define	SEGM_SYNC	0x04		/* wait for segment */
	u_int16_t seg_flags;		/* run-time flags for this segment */
};

#define ISSPACE(F, BB, C)						\
	(((C)->cr_uid == 0 && (F)->lfs_bfree >= (BB)) ||		\
	((C)->cr_uid != 0 && IS_FREESPACE(F, BB)))

#define IS_FREESPACE(F, BB)						\
	((F)->lfs_bfree > ((F)->lfs_dsize * (F)->lfs_minfree / 100 + (BB)))

#define ISSPACE_XXX(F, BB)						\
	((F)->lfs_bfree >= (BB))

#define DOSTATS
#ifdef DOSTATS
/* Statistics Counters */
struct lfs_stats {
	u_int	segsused;
	u_int	psegwrites;
	u_int	psyncwrites;
	u_int	pcleanwrites;
	u_int	blocktot;
	u_int	cleanblocks;
	u_int	ncheckpoints;
	u_int	nwrites;
	u_int	nsync_writes;
	u_int	wait_exceeded;
	u_int	write_exceeded;
	u_int	flush_invoked;
};
extern struct lfs_stats lfs_stats;
#endif
