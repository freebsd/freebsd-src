/*-
 * Copyright (c) 1992, 1993
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
 *	@(#)clean.h	8.1 (Berkeley) 6/4/93
 */

/*
 * The LFS user-level library will be used when writing cleaners and
 * checkers for LFS file systems.  It will have facilities for finding
 * and parsing LFS segments.
 */

#define DUMP_SUM_HEADER		0x0001
#define DUMP_INODE_ADDRS	0x0002
#define DUMP_FINFOS		0x0004
#define	DUMP_ALL		0xFFFF

#define IFILE_NAME "ifile"

/*
 * Cleaner parameters
 *	BUSY_LIM: lower bound of the number of segments currently available
 *		as a percentage of the total number of free segments possibly
 *		available.
 *	IDLE_LIM: Same as BUSY_LIM but used when the system is idle.
 *	MIN_SEGS: Minimum number of segments you should always have.
 *		I have no idea what this should be, but it should probably
 *		be a function of lfsp.
 *	NUM_TO_CLEAN: Number of segments to clean at once.  Again, this
 *		should probably be based on the file system size and how
 *		full or empty the segments being cleaned are.
 */

#define	BUSY_LIM	0.50
#define	IDLE_LIM	0.90

#define	MIN_SEGS(lfsp)		(3)
#define	NUM_TO_CLEAN(fsp)	(1)

#define MAXLOADS	3
#define	ONE_MIN		0
#define	FIVE_MIN	1
#define	FIFTEEN_MIN	2

typedef struct fs_info {
	struct	statfs	*fi_statfsp;	/* fsstat info from getfsstat */
	struct	lfs	fi_lfs;		/* superblock */
	CLEANERINFO	*fi_cip;	/* Cleaner info from ifile */
	SEGUSE	*fi_segusep;		/* segment usage table (from ifile) */
	IFILE	*fi_ifilep;		/* ifile table (from ifile) */
	u_long	fi_daddr_shift;		/* shift to get byte offset of daddr */
	u_long	fi_ifile_count;		/* # entries in the ifile table */
	off_t	fi_ifile_length;	/* length of the ifile */
} FS_INFO;

/* 
 * XXX: size (in bytes) of a segment
 *	should lfs_bsize be fsbtodb(fs,1), blksize(fs), or lfs_dsize? 
 */
#define seg_size(fs) ((fs)->lfs_ssize << (fs)->lfs_bshift)

/* daddr -> byte offset */
#define datobyte(fs, da) ((da) << (fs)->fi_daddr_shift)
#define bytetoda(fs, byte) ((byte) >> (fs)->fi_daddr_shift)

#define CLEANSIZE(fsp)	(fsp->fi_lfs.lfs_cleansz << fsp->fi_lfs.lfs_bshift)
#define SEGTABSIZE(fsp)	(fsp->fi_lfs.lfs_segtabsz << fsp->fi_lfs.lfs_bshift)

#define IFILE_ENTRY(fs, if, i) \
	((IFILE *)((caddr_t)(if) + ((i) / (fs)->lfs_ifpb << (fs)->lfs_bshift)) \
	+ (i) % (fs)->lfs_ifpb)

#define SEGUSE_ENTRY(fs, su, i) \
	((SEGUSE *)((caddr_t)(su) + (fs)->lfs_bsize * ((i) / (fs)->lfs_sepb)) +\
	(i) % (fs)->lfs_sepb)

__BEGIN_DECLS
int	 dump_summary __P((struct lfs *, SEGSUM *, u_long, daddr_t **));
void	 err __P((const int, const char *, ...));
int	 fs_getmntinfo __P((struct statfs **, char *, int));
int	 get __P((int, off_t, void *, size_t));
FS_INFO	*get_fs_info __P((struct statfs *, int));
int 	 lfs_segmapv __P((FS_INFO *, int, caddr_t, BLOCK_INFO **, int *));
int	 mmap_segment __P((FS_INFO *, int, caddr_t *, int));
void	 munmap_segment __P((FS_INFO *, caddr_t, int));
void	 reread_fs_info __P((FS_INFO *, int));
void	 toss __P((void *, int *, size_t,
	      int (*)(const void *, const void *, const void *), void *));

/*
 * USEFUL DEBUGGING FUNCTIONS:
 */
#ifdef VERBOSE
#define PRINT_FINFO(fp, ip) { \
	(void)printf("    %s %s%d version %d nblocks %d\n", \
	    (ip)->if_version > (fp)->fi_version ? "TOSSING" : "KEEPING", \
	    "FINFO for inode: ", (fp)->fi_ino, \
	    (fp)->fi_version, (fp)->fi_nblocks); \
	fflush(stdout); \
}

#define PRINT_INODE(b, bip) { \
	(void) printf("\t%s inode: %d daddr: 0x%lx create: %s\n", \
	    b ? "KEEPING" : "TOSSING", (bip)->bi_inode, (bip)->bi_daddr, \
	    ctime((time_t *)&(bip)->bi_segcreate)); \
	fflush(stdout); \
}

#define PRINT_BINFO(bip) { \
	(void)printf("\tinode: %d lbn: %d daddr: 0x%lx create: %s\n", \
	    (bip)->bi_inode, (bip)->bi_lbn, (bip)->bi_daddr, \
	    ctime((time_t *)&(bip)->bi_segcreate)); \
	fflush(stdout); \
}

#define PRINT_SEGUSE(sup, n) { \
	(void)printf("Segment %d nbytes=%lu\tflags=%c%c%c ninos=%d nsums=%d lastmod: %s\n", \
			n, (sup)->su_nbytes, \
			(sup)->su_flags & SEGUSE_DIRTY ? 'D' : 'C', \
			(sup)->su_flags & SEGUSE_ACTIVE ? 'A' : ' ', \
			(sup)->su_flags & SEGUSE_SUPERBLOCK ? 'S' : ' ', \
			(sup)->su_ninos, (sup)->su_nsums, \
			ctime((time_t *)&(sup)->su_lastmod)); \
	fflush(stdout); \
}

void	 dump_super __P((struct lfs *));
void	 dump_cleaner_info __P((void *));
void	 print_SEGSUM __P(( struct lfs *, SEGSUM *));
void	 print_CLEANERINFO __P((CLEANERINFO *));
#else
#define	PRINT_FINFO(fp, ip)
#define	PRINT_INODE(b, bip)
#define PRINT_BINFO(bip)
#define	PRINT_SEGUSE(sup, n)
#define	dump_cleaner_info(cip)
#define	dump_super(lfsp)
#endif
__END_DECLS
