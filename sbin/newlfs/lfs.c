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
 */

#ifndef lint
static char sccsid[] = "@(#)lfs.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */

#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/time.h>
#include <sys/mount.h>

#include <ufs/ufs/dir.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/dinode.h>
#include <ufs/lfs/lfs.h>

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "extern.h"

/*
 * This table is indexed by the log base 2 of the block size.
 * It returns the maximum file size allowed in a file system
 * with the specified block size.  For block sizes smaller than
 * 8K, the size is limited by tha maximum number of blocks that
 * can be reached by triply indirect blocks:
 *	NDADDR + INOPB(bsize) + INOPB(bsize)^2 + INOPB(bsize)^3
 * For block size of 8K or larger, the file size is limited by the
 * number of blocks that can be represented in the file system.  Since
 * we use negative block numbers to represent indirect blocks, we can
 * have a maximum of 2^31 blocks.
 */

u_quad_t maxtable[] = {
	/*    1 */ -1,
	/*    2 */ -1,
	/*    4 */ -1,
	/*    8 */ -1,
	/*   16 */ -1,
	/*   32 */ -1,
	/*   64 */ -1,
	/*  128 */ -1,
	/*  256 */ -1,
	/*  512 */ NDADDR + 128 + 128 * 128 + 128 * 128 * 128,
	/* 1024 */ NDADDR + 256 + 256 * 256 + 256 * 256 * 256,
	/* 2048 */ NDADDR + 512 + 512 * 512 + 512 * 512 * 512,
	/* 4096 */ NDADDR + 1024 + 1024 * 1024 + 1024 * 1024 * 1024,
	/* 8192 */ 1 << 31,
	/* 16 K */ 1 << 31,
	/* 32 K */ 1 << 31,
};

static struct lfs lfs_default =  {
	/* lfs_magic */		LFS_MAGIC,
	/* lfs_version */	LFS_VERSION,
	/* lfs_size */		0,
	/* lfs_ssize */		DFL_LFSSEG/DFL_LFSBLOCK,
	/* lfs_dsize */		0,
	/* lfs_bsize */		DFL_LFSBLOCK,
	/* lfs_fsize */		DFL_LFSBLOCK,
	/* lfs_frag */		1,
	/* lfs_free */		LFS_FIRST_INUM,
	/* lfs_bfree */		0,
	/* lfs_nfiles */	0,
	/* lfs_avail */		0,
	/* lfs_uinodes */	0,
	/* lfs_idaddr */	0,
	/* lfs_ifile */		LFS_IFILE_INUM,
	/* lfs_lastseg */	0,
	/* lfs_nextseg */	0,
	/* lfs_curseg */	0,
	/* lfs_offset */	0,
	/* lfs_lastpseg */	0,
	/* lfs_tstamp */	0,
	/* lfs_minfree */	MINFREE,
	/* lfs_maxfilesize */	0,
	/* lfs_dbpseg */	DFL_LFSSEG/DEV_BSIZE,
	/* lfs_inopb */		DFL_LFSBLOCK/sizeof(struct dinode),
	/* lfs_ifpb */		DFL_LFSBLOCK/sizeof(IFILE),
	/* lfs_sepb */		DFL_LFSBLOCK/sizeof(SEGUSE),
	/* lfs_nindir */	DFL_LFSBLOCK/sizeof(daddr_t),
	/* lfs_nseg */		0,
	/* lfs_nspf */		0,
	/* lfs_cleansz */	0,
	/* lfs_segtabsz */	0,
	/* lfs_segmask */	DFL_LFSSEG_MASK,
	/* lfs_segshift */	DFL_LFSSEG_SHIFT,
	/* lfs_bmask */		DFL_LFSBLOCK_MASK,
	/* lfs_bshift */	DFL_LFSBLOCK_SHIFT,
	/* lfs_ffmask */	0,
	/* lfs_ffshift */	0,
	/* lfs_fbmask */	0,
	/* lfs_fbshift */	0,
	/* lfs_fsbtodb */	0,
	/* lfs_sushift */	0,
	/* lfs_sboffs */	{ 0 },
	/* lfs_sp */		NULL,
	/* lfs_ivnode */	NULL,
	/* lfs_seglock */	0,
	/* lfs_lockpid */	0,
	/* lfs_iocount */	0,
	/* lfs_writer */	0,
	/* lfs_dirops */	0,
	/* lfs_doifile */	0,
	/* lfs_nactive */	0,
	/* lfs_fmod */		0,
	/* lfs_clean */		0,
	/* lfs_ronly */		0,
	/* lfs_flags */		0,
	/* lfs_fsmnt */		{ 0 },
	/* lfs_pad */		{ 0 },
	/* lfs_cksum */		0
};


struct direct lfs_root_dir[] = {
	{ ROOTINO, sizeof(struct direct), DT_DIR, 1, "."},
	{ ROOTINO, sizeof(struct direct), DT_DIR, 2, ".."},
	{ LFS_IFILE_INUM, sizeof(struct direct), DT_REG, 5, "ifile"},
	{ LOSTFOUNDINO, sizeof(struct direct), DT_DIR, 10, "lost+found"},
};

struct direct lfs_lf_dir[] = {
        { LOSTFOUNDINO, sizeof(struct direct), DT_DIR, 1, "." },
        { ROOTINO, sizeof(struct direct), DT_DIR, 2, ".." },
};

static daddr_t make_dinode 
	__P((ino_t, struct dinode *, int, daddr_t, struct lfs *));
static void make_dir __P(( void *, struct direct *, int));
static void put __P((int, off_t, void *, size_t));

int
make_lfs(fd, lp, partp, minfree, block_size, seg_size)
	int fd;
	struct disklabel *lp;
	struct partition *partp;
	int minfree;
	int block_size;
	int seg_size;
{
	struct dinode *dip;	/* Pointer to a disk inode */
	struct dinode *dpagep;	/* Pointer to page of disk inodes */
	CLEANERINFO *cleaninfo;	/* Segment cleaner information table */
	FINFO file_info;	/* File info structure in summary blocks */
	IFILE *ifile;		/* Pointer to array of ifile structures */
	IFILE *ip;		/* Pointer to array of ifile structures */
	struct lfs *lfsp;	/* Superblock */
	SEGUSE *segp;		/* Segment usage table */
	SEGUSE *segtable;	/* Segment usage table */
	SEGSUM summary;		/* Segment summary structure */
	SEGSUM *sp;		/* Segment summary pointer */
	daddr_t	last_sb_addr;	/* Address of superblocks */
	daddr_t last_addr;	/* Previous segment address */
	daddr_t	sb_addr;	/* Address of superblocks */
	daddr_t	seg_addr;	/* Address of current segment */
	void *ipagep;		/* Pointer to the page we use to write stuff */
	void *sump;		/* Used to copy stuff into segment buffer */
	u_long *block_array;	/* Array of logical block nos to put in sum */
	u_long blocks_used;	/* Number of blocks in first segment */
	u_long *dp;		/* Used to computed checksum on data */
	u_long *datasump;	/* Used to computed checksum on data */
	int block_array_size;	/* How many entries in block array */
	int bsize;		/* Block size */
	int db_per_fb;		/* Disk blocks per file block */
	int i, j;
	int off;		/* Offset at which to write */
	int sb_interval;	/* number of segs between super blocks */
	int seg_seek;		/* Seek offset for a segment */
	int ssize;		/* Segment size */
	int sum_size;		/* Size of the summary block */

	lfsp = &lfs_default;

	if (!(bsize = block_size))
		bsize = DFL_LFSBLOCK;
	if (!(ssize = seg_size))
		ssize = DFL_LFSSEG;

	/* Modify parts of superblock overridden by command line arguments */
	if (bsize != DFL_LFSBLOCK) {
		lfsp->lfs_bshift = log2(bsize);
		if (1 << lfsp->lfs_bshift != bsize)
			fatal("%d: block size not a power of 2", bsize);
		lfsp->lfs_bsize = bsize;
		lfsp->lfs_fsize = bsize;
		lfsp->lfs_bmask = bsize - 1;
		lfsp->lfs_inopb = bsize / sizeof(struct dinode);
/* MIS -- should I round to power of 2 */
		lfsp->lfs_ifpb = bsize / sizeof(IFILE);
		lfsp->lfs_sepb = bsize / sizeof(SEGUSE);
		lfsp->lfs_nindir = bsize / sizeof(daddr_t);
	}

	if (ssize != DFL_LFSSEG) {
		lfsp->lfs_segshift = log2(ssize);
		if (1 << lfsp->lfs_segshift != ssize)
			fatal("%d: segment size not power of 2", ssize);
		lfsp->lfs_ssize = ssize;
		lfsp->lfs_segmask = ssize - 1;
		lfsp->lfs_dbpseg = ssize / DEV_BSIZE;
	}
	lfsp->lfs_ssize = ssize >> lfsp->lfs_bshift;

	if (minfree)
		lfsp->lfs_minfree = minfree;

	/*
	 * Fill in parts of superblock that can be computed from file system
	 * size, disk geometry and current time.
	 */
	db_per_fb = bsize/lp->d_secsize;
	lfsp->lfs_fsbtodb = log2(db_per_fb);
	lfsp->lfs_sushift = log2(lfsp->lfs_sepb);
	lfsp->lfs_size = partp->p_size >> lfsp->lfs_fsbtodb;
	lfsp->lfs_dsize = lfsp->lfs_size - (LFS_LABELPAD >> lfsp->lfs_bshift);
	lfsp->lfs_nseg = lfsp->lfs_dsize / lfsp->lfs_ssize;
	lfsp->lfs_maxfilesize = maxtable[lfsp->lfs_bshift] << lfsp->lfs_bshift;

	/* 
	 * The number of free blocks is set from the number of segments times
	 * the segment size - 2 (that we never write because we need to make
	 * sure the cleaner can run).  Then we'll subtract off the room for the
	 * superblocks ifile entries and segment usage table.
	 */
	lfsp->lfs_dsize = fsbtodb(lfsp, (lfsp->lfs_nseg - 2) * lfsp->lfs_ssize);
	lfsp->lfs_bfree = lfsp->lfs_dsize;
	lfsp->lfs_segtabsz = SEGTABSIZE_SU(lfsp);
	lfsp->lfs_cleansz = CLEANSIZE_SU(lfsp);
	if ((lfsp->lfs_tstamp = time(NULL)) == -1)
		fatal("time: %s", strerror(errno));
	if ((sb_interval = lfsp->lfs_nseg / LFS_MAXNUMSB) < LFS_MIN_SBINTERVAL)
		sb_interval = LFS_MIN_SBINTERVAL;

	/*
	 * Now, lay out the file system.  We need to figure out where
	 * the superblocks go, initialize the checkpoint information
	 * for the first two superblocks, initialize the segment usage
	 * information, put the segusage information in the ifile, create
	 * the first block of IFILE structures, and link all the IFILE
	 * structures into a free list.
	 */

	/* Figure out where the superblocks are going to live */
	lfsp->lfs_sboffs[0] = LFS_LABELPAD/lp->d_secsize;
	for (i = 1; i < LFS_MAXNUMSB; i++) {
		sb_addr = ((i * sb_interval) << 
		    (lfsp->lfs_segshift - lfsp->lfs_bshift + lfsp->lfs_fsbtodb))
		    + lfsp->lfs_sboffs[0];
		if (sb_addr > partp->p_size)
			break;
		lfsp->lfs_sboffs[i] = sb_addr;
	}
	last_sb_addr = lfsp->lfs_sboffs[i - 1];
	lfsp->lfs_lastseg = lfsp->lfs_sboffs[0];
	lfsp->lfs_nextseg = 
	    lfsp->lfs_sboffs[1] ? lfsp->lfs_sboffs[1] : lfsp->lfs_sboffs[0];
	lfsp->lfs_curseg = lfsp->lfs_lastseg;

	/*
	 * Initialize the segment usage table.  The first segment will
	 * contain the superblock, the cleanerinfo (cleansz), the segusage 
	 * table * (segtabsz), 1 block's worth of IFILE entries, the root 
	 * directory, the lost+found directory and one block's worth of 
	 * inodes (containing the ifile, root, and l+f inodes).
	 */
	if (!(cleaninfo = malloc(lfsp->lfs_cleansz << lfsp->lfs_bshift)))
		fatal("%s", strerror(errno));
	cleaninfo->clean = lfsp->lfs_nseg - 1;
	cleaninfo->dirty = 1;

	if (!(segtable = malloc(lfsp->lfs_segtabsz << lfsp->lfs_bshift)))
		fatal("%s", strerror(errno));
	segp = segtable;
	blocks_used = lfsp->lfs_segtabsz + lfsp->lfs_cleansz + 4;
	segp->su_nbytes = ((blocks_used - 1) << lfsp->lfs_bshift) +
	    3 * sizeof(struct dinode) + LFS_SUMMARY_SIZE;
	segp->su_lastmod = lfsp->lfs_tstamp;
	segp->su_nsums = 1;	/* 1 summary blocks */
	segp->su_ninos = 1;	/* 1 inode block */
	segp->su_flags = SEGUSE_SUPERBLOCK | SEGUSE_DIRTY;
	lfsp->lfs_bfree -= LFS_SUMMARY_SIZE / lp->d_secsize;
	lfsp->lfs_bfree -=
	     fsbtodb(lfsp, lfsp->lfs_cleansz + lfsp->lfs_segtabsz + 4);

	/* 
	 * Now figure out the address of the ifile inode. The inode block
	 * appears immediately after the segment summary.
	 */
	lfsp->lfs_idaddr = (LFS_LABELPAD + LFS_SBPAD + LFS_SUMMARY_SIZE) /
	    lp->d_secsize;

	for (segp = segtable + 1, i = 1; i < lfsp->lfs_nseg; i++, segp++) {
		if ((i % sb_interval) == 0) {
			segp->su_flags = SEGUSE_SUPERBLOCK;
			lfsp->lfs_bfree -= (LFS_SBPAD / lp->d_secsize);
		} else
			segp->su_flags = 0;
		segp->su_lastmod = 0;
		segp->su_nbytes = 0;
		segp->su_ninos = 0;
		segp->su_nsums = 0;
	}

	/* 
	 * Initialize dynamic accounting.  The blocks available for
	 * writing are the bfree blocks minus 1 segment summary for
	 * each segment since you can't write any new data without
	 * creating a segment summary - 2 segments that the cleaner
	 * needs.
	 */
	lfsp->lfs_avail = lfsp->lfs_bfree - lfsp->lfs_nseg - 
		fsbtodb(lfsp, 2 * lfsp->lfs_ssize);
	lfsp->lfs_uinodes = 0;
	/*
	 * Ready to start writing segments.  The first segment is different
	 * because it contains the segment usage table and the ifile inode
	 * as well as a superblock.  For the rest of the segments, set the 
	 * time stamp to be 0 so that the first segment is the most recent.
	 * For each segment that is supposed to contain a copy of the super
	 * block, initialize its first few blocks and its segment summary 
	 * to indicate this.
	 */
	lfsp->lfs_nfiles = LFS_FIRST_INUM - 1;
	lfsp->lfs_cksum = 
	    cksum(lfsp, sizeof(struct lfs) - sizeof(lfsp->lfs_cksum));

	/* Now create a block of disk inodes */
	if (!(dpagep = malloc(lfsp->lfs_bsize)))
		fatal("%s", strerror(errno));
	dip = (struct dinode *)dpagep;
	bzero(dip, lfsp->lfs_bsize);

	/* Create a block of IFILE structures. */
	if (!(ipagep = malloc(lfsp->lfs_bsize)))
		fatal("%s", strerror(errno));
	ifile = (IFILE *)ipagep;

	/* 
	 * Initialize IFILE.  It is the next block following the
	 * block of inodes (whose address has been calculated in
	 * lfsp->lfs_idaddr;
	 */
	sb_addr = lfsp->lfs_idaddr + lfsp->lfs_bsize / lp->d_secsize;
	sb_addr = make_dinode(LFS_IFILE_INUM, dip, 
	    lfsp->lfs_cleansz + lfsp->lfs_segtabsz+1, sb_addr, lfsp);
	dip->di_mode = IFREG|IREAD|IWRITE;
	ip = &ifile[LFS_IFILE_INUM];
	ip->if_version = 1;
	ip->if_daddr = lfsp->lfs_idaddr;

	/* Initialize the ROOT Directory */
	sb_addr = make_dinode(ROOTINO, ++dip, 1, sb_addr, lfsp);
	dip->di_mode = IFDIR|IREAD|IWRITE|IEXEC;
	dip->di_size = DIRBLKSIZ;
	dip->di_nlink = 3;
	ip = &ifile[ROOTINO];
	ip->if_version = 1;
	ip->if_daddr = lfsp->lfs_idaddr;

	/* Initialize the lost+found Directory */
	sb_addr = make_dinode(LOSTFOUNDINO, ++dip, 1, sb_addr, lfsp);
	dip->di_mode = IFDIR|IREAD|IWRITE|IEXEC;
	dip->di_size = DIRBLKSIZ;
	dip->di_nlink = 2;
	ip = &ifile[LOSTFOUNDINO];
	ip->if_version = 1;
	ip->if_daddr = lfsp->lfs_idaddr;

	/* Make all the other dinodes invalid */
	for (i = INOPB(lfsp)-3, dip++; i; i--, dip++)
		dip->di_inumber = LFS_UNUSED_INUM;
	

	/* Link remaining IFILE entries in free list */
	for (ip = &ifile[LFS_FIRST_INUM], i = LFS_FIRST_INUM; 
	    i < lfsp->lfs_ifpb; ++ip) {
		ip->if_version = 1;
		ip->if_daddr = LFS_UNUSED_DADDR;
		ip->if_nextfree = ++i;
	}
	ifile[lfsp->lfs_ifpb - 1].if_nextfree = LFS_UNUSED_INUM;

	/* Now, write the segment */

	/* Compute a checksum across all the data you're writing */
	dp = datasump = malloc (blocks_used * sizeof(u_long));
	*dp++ = ((u_long *)dpagep)[0];		/* inode block */
	for (i = 0; i < lfsp->lfs_cleansz; i++)
		*dp++ = ((u_long *)cleaninfo)[(i << lfsp->lfs_bshift) / 
		    sizeof(u_long)];		/* Cleaner info */
	for (i = 0; i < lfsp->lfs_segtabsz; i++)
		*dp++ = ((u_long *)segtable)[(i << lfsp->lfs_bshift) / 
		    sizeof(u_long)];		/* Segusage table */
	*dp++ = ((u_long *)ifile)[0];		/* Ifile */

	/* Still need the root and l+f bytes; get them later */

	/* Write out the inode block */
	off = LFS_LABELPAD + LFS_SBPAD + LFS_SUMMARY_SIZE;
	put(fd, off, dpagep, lfsp->lfs_bsize);
	free(dpagep);
	off += lfsp->lfs_bsize;

	/* Write out the ifile */

	put(fd, off, cleaninfo, lfsp->lfs_cleansz << lfsp->lfs_bshift);
	off += (lfsp->lfs_cleansz << lfsp->lfs_bshift);
	(void)free(cleaninfo);

	put(fd, off, segtable, lfsp->lfs_segtabsz << lfsp->lfs_bshift);
	off += (lfsp->lfs_segtabsz << lfsp->lfs_bshift);
	(void)free(segtable);

	put(fd, off, ifile, lfsp->lfs_bsize);
	off += lfsp->lfs_bsize;

	/*
	 * use ipagep for space for writing out other stuff.  It used to 
	 * contain the ifile, but we're done with it.
	 */

	/* Write out the root and lost and found directories */
	bzero(ipagep, lfsp->lfs_bsize);
	make_dir(ipagep, lfs_root_dir, 
	    sizeof(lfs_root_dir) / sizeof(struct direct));
	*dp++ = ((u_long *)ipagep)[0];
	put(fd, off, ipagep, lfsp->lfs_bsize);
	off += lfsp->lfs_bsize;

	bzero(ipagep, lfsp->lfs_bsize);
	make_dir(ipagep, lfs_lf_dir, 
		sizeof(lfs_lf_dir) / sizeof(struct direct));
	*dp++ = ((u_long *)ipagep)[0];
	put(fd, off, ipagep, lfsp->lfs_bsize);

	/* Write Supberblock */
	lfsp->lfs_offset = (off + lfsp->lfs_bsize) / lp->d_secsize;
	put(fd, LFS_LABELPAD, lfsp, sizeof(struct lfs));

	/* 
	 * Finally, calculate all the fields for the summary structure
	 * and write it.
	 */

	summary.ss_next = lfsp->lfs_nextseg;
	summary.ss_create = lfsp->lfs_tstamp;
	summary.ss_nfinfo = 3;
	summary.ss_ninos = 3;
	summary.ss_datasum = cksum(datasump, sizeof(u_long) * blocks_used);

	/*
	 * Make sure that we don't overflow a summary block. We have to
	 * record: FINFO structures for ifile, root, and l+f.  The number
	 * of blocks recorded for the ifile is determined by the size of
	 * the cleaner info and the segments usage table.  There is room
	 * for one block included in sizeof(FINFO) so we don't need to add
	 * any extra space for the ROOT and L+F, and one block of the ifile
	 * is already counted.  Finally, we leave room for 1 inode block
	 * address.
	 */
	sum_size = 3*sizeof(FINFO) + sizeof(SEGSUM) + sizeof(daddr_t) +
	    (lfsp->lfs_cleansz + lfsp->lfs_segtabsz) * sizeof(u_long);
#define	SUMERR \
"Multiple summary blocks in segment 1 not yet implemented\nsummary is %d bytes."
	if (sum_size > LFS_SUMMARY_SIZE)
		fatal(SUMERR, sum_size);

		block_array_size = lfsp->lfs_cleansz + lfsp->lfs_segtabsz + 1;

	if (!(block_array = malloc(block_array_size *sizeof(int))))
		fatal("%s: %s", special, strerror(errno));

	/* fill in the array */
	for (i = 0; i < block_array_size; i++)
		block_array[i] = i;

	/* copy into segment */
	sump = ipagep;
	bcopy(&summary, sump, sizeof(SEGSUM));
	sump += sizeof(SEGSUM);

	/* Now, add the ifile */
	file_info.fi_nblocks = block_array_size;
	file_info.fi_version = 1;
	file_info.fi_ino = LFS_IFILE_INUM;

	bcopy(&file_info, sump, sizeof(FINFO) - sizeof(u_long));
	sump += sizeof(FINFO) - sizeof(u_long);
	bcopy(block_array, sump, sizeof(u_long) * file_info.fi_nblocks);
	sump += sizeof(u_long) * file_info.fi_nblocks;

	/* Now, add the root directory */
	file_info.fi_nblocks = 1;
	file_info.fi_version = 1;
	file_info.fi_ino = ROOTINO;
	file_info.fi_blocks[0] = 0;
	bcopy(&file_info, sump, sizeof(FINFO));
	sump += sizeof(FINFO);

	/* Now, add the lost and found */
	file_info.fi_ino = LOSTFOUNDINO;
	bcopy(&file_info, sump, sizeof(FINFO));

	((daddr_t *)ipagep)[LFS_SUMMARY_SIZE / sizeof(daddr_t) - 1] = 
	    lfsp->lfs_idaddr;
	((SEGSUM *)ipagep)->ss_sumsum = cksum(ipagep+sizeof(summary.ss_sumsum), 
	    LFS_SUMMARY_SIZE - sizeof(summary.ss_sumsum));
	put(fd, LFS_LABELPAD + LFS_SBPAD, ipagep, LFS_SUMMARY_SIZE);

	sp = (SEGSUM *)ipagep;
	sp->ss_create = 0;
	sp->ss_nfinfo = 0;
	sp->ss_ninos = 0;
	sp->ss_datasum = 0;

	/* Now write the summary block for the next partial so it's invalid */
	lfsp->lfs_tstamp = 0;
	off += lfsp->lfs_bsize;
	sp->ss_sumsum =
	    cksum(&sp->ss_datasum, LFS_SUMMARY_SIZE - sizeof(sp->ss_sumsum));
	put(fd, off, sp, LFS_SUMMARY_SIZE);

	/* Now, write rest of segments containing superblocks */
	lfsp->lfs_cksum = 
	    cksum(lfsp, sizeof(struct lfs) - sizeof(lfsp->lfs_cksum));
	for (seg_addr = last_addr = lfsp->lfs_sboffs[0], j = 1, i = 1; 
	    i < lfsp->lfs_nseg; i++) {

		seg_addr += lfsp->lfs_ssize << lfsp->lfs_fsbtodb;
		sp->ss_next = last_addr;
		last_addr = seg_addr;
		seg_seek = seg_addr * lp->d_secsize;

		if (seg_addr == lfsp->lfs_sboffs[j]) {
			if (j < (LFS_MAXNUMSB - 2))
				j++;
			put(fd, seg_seek, lfsp, sizeof(struct lfs));
			seg_seek += LFS_SBPAD;
		} 

		/* Summary */
		sp->ss_sumsum = cksum(&sp->ss_datasum, 
		    LFS_SUMMARY_SIZE - sizeof(sp->ss_sumsum));
		put(fd, seg_seek, sp, LFS_SUMMARY_SIZE);
	}
	free(ipagep);
	close(fd);
	return (0);
}

static void
put(fd, off, p, len)
	int fd;
	off_t off;
	void *p;
	size_t len;
{
	int wbytes;

	if (lseek(fd, off, SEEK_SET) < 0)
		fatal("%s: %s", special, strerror(errno));
	if ((wbytes = write(fd, p, len)) < 0)
		fatal("%s: %s", special, strerror(errno));
	if (wbytes != len)
		fatal("%s: short write (%d, not %d)", special, wbytes, len);
}

/*
 * Create the root directory for this file system and the lost+found
 * directory.
 */

	u_long	d_ino;			/* inode number of entry */
	u_short	d_reclen;		/* length of this record */
	u_short	d_namlen;		/* length of string in d_name */
	char	d_name[MAXNAMLEN + 1];	/* name with length <= MAXNAMLEN */
void
lfsinit()
{}

static daddr_t
make_dinode(ino, dip, nblocks, saddr, lfsp)
	ino_t ino;				/* inode we're creating */
	struct dinode *dip;			/* disk inode */
	int nblocks;				/* number of blocks in file */
	daddr_t saddr;				/* starting block address */
	struct lfs *lfsp;			/* superblock */
{
	int db_per_fb, i;

	dip->di_nlink = 1;
	dip->di_blocks = nblocks << lfsp->lfs_fsbtodb;

	dip->di_size = (nblocks << lfsp->lfs_bshift);
	dip->di_atime.ts_sec = dip->di_mtime.ts_sec =
	    dip->di_ctime.ts_sec = lfsp->lfs_tstamp;
	dip->di_atime.ts_nsec = dip->di_mtime.ts_nsec =
	    dip->di_ctime.ts_nsec = 0;
	dip->di_inumber = ino;

#define	SEGERR \
"File requires more than the number of direct blocks; increase block or segment size."
	if (NDADDR < nblocks)
		fatal("%s", SEGERR);

	/* Assign the block addresses for the ifile */
	db_per_fb = 1 << lfsp->lfs_fsbtodb;
	for (i = 0; i < nblocks; i++, saddr += db_per_fb)
		dip->di_db[i] = saddr;

	return (saddr);
}


/*
 * Construct a set of directory entries in "bufp".  We assume that all the
 * entries in protodir fir in the first DIRBLKSIZ.  
 */
static void
make_dir(bufp, protodir, entries)
	void *bufp;
	register struct direct *protodir;
	int entries;
{
	char *cp;
	int i, spcleft;

	spcleft = DIRBLKSIZ;
	for (cp = bufp, i = 0; i < entries - 1; i++) {
		protodir[i].d_reclen = DIRSIZ(NEWDIRFMT, &protodir[i]);
		bcopy(&protodir[i], cp, protodir[i].d_reclen);
		cp += protodir[i].d_reclen;
		if ((spcleft -= protodir[i].d_reclen) < 0)
			fatal("%s: %s", special, "directory too big");
	}
	protodir[i].d_reclen = spcleft;
	bcopy(&protodir[i], cp, DIRSIZ(NEWDIRFMT, &protodir[i]));
}
