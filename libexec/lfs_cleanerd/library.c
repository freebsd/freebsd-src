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
 *	$FreeBSD$
 */

#ifndef lint
static char sccsid[] = "@(#)library.c	8.1 (Berkeley) 6/4/93";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <ufs/ufs/dinode.h>
#include <ufs/lfs/lfs.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "clean.h"

void	 add_blocks __P((FS_INFO *, BLOCK_INFO *, int *, SEGSUM *, caddr_t,
	     daddr_t, daddr_t));
void	 add_inodes __P((FS_INFO *, BLOCK_INFO *, int *, SEGSUM *, caddr_t,
	     daddr_t));
int	 bi_compare __P((const void *, const void *));
int	 bi_toss __P((const void *, const void *, const void *));
void	 get_ifile __P((FS_INFO *, int));
int	 get_superblock __P((FS_INFO *, struct lfs *));
int	 pseg_valid __P((FS_INFO *, SEGSUM *));

/*
 * This function will get information on a a filesystem which matches
 * the name and type given.  If a "name" is in a filesystem of the given
 * type, then buf is filled with that filesystem's info, and the
 * a non-zero value is returned.
 */
int
fs_getmntinfo(buf, name, type)
	struct	statfs	**buf;
	char	*name;
	int	type;
{
	/* allocate space for the filesystem info */
	*buf = (struct statfs *)malloc(sizeof(struct statfs));
	if (*buf == NULL)
		return 0;

	/* grab the filesystem info */
	if (statfs(name, *buf) < 0) {
		free(*buf);
		return 0;
	}

	/* check to see if it's the one we want */
	if (((*buf)->f_type != type) ||
	    strncmp(name, (*buf)->f_mntonname, MNAMELEN)) {
		/* "this is not the filesystem you're looking for" */
		free(*buf);
		return 0;
	}

	return 1;
}

/*
 * Get all the information available on an LFS file system.
 * Returns an pointer to an FS_INFO structure, NULL on error.
 */
FS_INFO *
get_fs_info (lstatfsp, use_mmap)
	struct statfs *lstatfsp;	/* IN: pointer to statfs struct */
	int use_mmap;			/* IN: mmap or read */
{
	FS_INFO	*fsp;
	int	i;

	fsp = (FS_INFO *)malloc(sizeof(FS_INFO));
	if (fsp == NULL)
		return NULL;
	bzero(fsp, sizeof(FS_INFO));

	fsp->fi_statfsp = lstatfsp;
	if (get_superblock (fsp, &fsp->fi_lfs))
		err(1, "get_fs_info: get_superblock failed");
	fsp->fi_daddr_shift =
	     fsp->fi_lfs.lfs_bshift - fsp->fi_lfs.lfs_fsbtodb;
	get_ifile (fsp, use_mmap);
	return (fsp);
}

/*
 * If we are reading the ifile then we need to refresh it.  Even if
 * we are mmapping it, it might have grown.  Finally, we need to
 * refresh the file system information (statfs) info.
 */
void
reread_fs_info(fsp, use_mmap)
	FS_INFO *fsp;	/* IN: prointer fs_infos to reread */
	int use_mmap;
{
	int i;

	if (statfs(fsp->fi_statfsp->f_mntonname, fsp->fi_statfsp))
		err(1, "reread_fs_info: statfs failed");
	get_ifile (fsp, use_mmap);
}

/*
 * Gets the superblock from disk (possibly in face of errors)
 */
int
get_superblock (fsp, sbp)
	FS_INFO *fsp;		/* local file system info structure */
	struct lfs *sbp;
{
	char mntfromname[MNAMELEN+1];
        int fid;

	strcpy(mntfromname, "/dev/r");
	strcat(mntfromname, fsp->fi_statfsp->f_mntfromname+5);

	if ((fid = open(mntfromname, O_RDONLY, (mode_t)0)) < 0) {
		err(0, "get_superblock: bad open");
		return (-1);
	}

	get(fid, LFS_LABELPAD, sbp, sizeof(struct lfs));
	close (fid);

	return (0);
}

/*
 * This function will map the ifile into memory.  It causes a
 * fatal error on failure.
 */
void
get_ifile (fsp, use_mmap)
	FS_INFO	*fsp;
	int use_mmap;

{
	struct stat file_stat;
	caddr_t ifp;
	char *ifile_name;
	int count, fid;

	ifp = NULL;
	ifile_name = malloc(strlen(fsp->fi_statfsp->f_mntonname) +
	    strlen(IFILE_NAME)+2);
	strcat(strcat(strcpy(ifile_name, fsp->fi_statfsp->f_mntonname), "/"),
	    IFILE_NAME);

	if ((fid = open(ifile_name, O_RDWR, (mode_t)0)) < 0)
		err(1, "get_ifile: bad open");

	if (fstat (fid, &file_stat))
		err(1, "get_ifile: fstat failed");

	if (use_mmap && file_stat.st_size == fsp->fi_ifile_length) {
		(void) close(fid);
		return;
	}

	/* get the ifile */
	if (use_mmap) {
		if (fsp->fi_cip)
			munmap((caddr_t)fsp->fi_cip, fsp->fi_ifile_length);
		ifp = mmap ((caddr_t)0, file_stat.st_size,
		    PROT_READ|PROT_WRITE, 0, fid, (off_t)0);
		if (ifp ==  (caddr_t)(-1))
			err(1, "get_ifile: mmap failed");
	} else {
		if (fsp->fi_cip)
			free(fsp->fi_cip);
		if (!(ifp = malloc (file_stat.st_size)))
			err (1, "get_ifile: malloc failed");
redo_read:
		count = read (fid, ifp, (size_t) file_stat.st_size);

		if (count < 0)
			err(1, "get_ifile: bad ifile read");
		else if (count < file_stat.st_size) {
			err(0, "get_ifile");
			if (lseek(fid, 0, SEEK_SET) < 0)
				err(1, "get_ifile: bad ifile lseek");
			goto redo_read;
		}
	}
	fsp->fi_ifile_length = file_stat.st_size;
	close (fid);

	fsp->fi_cip = (CLEANERINFO *)ifp;
	fsp->fi_segusep = (SEGUSE *)(ifp + CLEANSIZE(fsp));
	fsp->fi_ifilep  = (IFILE *)((caddr_t)fsp->fi_segusep + SEGTABSIZE(fsp));

	/*
	 * The number of ifile entries is equal to the number of blocks
	 * blocks in the ifile minus the ones allocated to cleaner info
	 * and segment usage table multiplied by the number of ifile
	 * entries per page.
	 */
	fsp->fi_ifile_count = (fsp->fi_ifile_length >> fsp->fi_lfs.lfs_bshift -
	    fsp->fi_lfs.lfs_cleansz - fsp->fi_lfs.lfs_segtabsz) *
	    fsp->fi_lfs.lfs_ifpb;

	free (ifile_name);
}

/*
 * This function will scan a segment and return a list of
 * <inode, blocknum> pairs which indicate which blocks were
 * contained as live data within the segment when the segment
 * summary was read (it may have "died" since then).  Any given
 * pair will be listed at most once.
 */
int
lfs_segmapv(fsp, seg, seg_buf, blocks, bcount)
	FS_INFO *fsp;		/* pointer to local file system information */
	int seg;		/* the segment number */
	caddr_t seg_buf;	/* the buffer containing the segment's data */
	BLOCK_INFO **blocks;	/* OUT: array of block_info for live blocks */
	int *bcount;		/* OUT: number of active blocks in segment */
{
	BLOCK_INFO *bip;
	SEGSUM *sp;
	SEGUSE *sup;
	FINFO *fip;
	struct lfs *lfsp;
	caddr_t s, segend;
	daddr_t pseg_addr, seg_addr;
	int i, nelem, nblocks, sumsize;
	time_t timestamp;

	lfsp = &fsp->fi_lfs;
	nelem = 2 * lfsp->lfs_ssize;
	if (!(bip = malloc(nelem * sizeof(BLOCK_INFO))))
		goto err0;

	sup = SEGUSE_ENTRY(lfsp, fsp->fi_segusep, seg);
	s = seg_buf + (sup->su_flags & SEGUSE_SUPERBLOCK ? LFS_SBPAD : 0);
	seg_addr = sntoda(lfsp, seg);
	pseg_addr = seg_addr + (sup->su_flags & SEGUSE_SUPERBLOCK ? btodb(LFS_SBPAD) : 0);
#ifdef VERBOSE
		printf("\tsegment buffer at: 0x%x\tseg_addr 0x%x\n", s, seg_addr);
#endif /* VERBOSE */

	*bcount = 0;
	for (segend = seg_buf + seg_size(lfsp), timestamp = 0; s < segend; ) {
		sp = (SEGSUM *)s;

#ifdef VERBOSE
		printf("\tpartial at: 0x%x\n", pseg_addr);
		print_SEGSUM(lfsp, sp);
		fflush(stdout);
#endif /* VERBOSE */

		nblocks = pseg_valid(fsp, sp);
		if (nblocks <= 0)
			break;

		/* Check if we have hit old data */
		if (timestamp > ((SEGSUM*)s)->ss_create)
			break;
		timestamp = ((SEGSUM*)s)->ss_create;

#ifdef DIAGNOSTIC
		/* Verify size of summary block */
		sumsize = sizeof(SEGSUM) +
		    (sp->ss_ninos + INOPB(lfsp) - 1) / INOPB(lfsp);
		for (fip = (FINFO *)(sp + 1); i < sp->ss_nfinfo; ++i) {
			sumsize += sizeof(FINFO) +
			    (fip->fi_nblocks - 1) * sizeof(daddr_t);
			fip = (FINFO *)(&fip->fi_blocks[fip->fi_nblocks]);
		}
		if (sumsize > LFS_SUMMARY_SIZE) {
			fprintf(stderr,
			    "Segment %d summary block too big: %d\n",
			    seg, sumsize);
			exit(1);
		}
#endif

		if (*bcount + nblocks + sp->ss_ninos > nelem) {
			nelem = *bcount + nblocks + sp->ss_ninos;
			bip = realloc (bip, nelem * sizeof(BLOCK_INFO));
			if (!bip)
				goto err0;
		}
		add_blocks(fsp, bip, bcount, sp, seg_buf, seg_addr, pseg_addr);
		add_inodes(fsp, bip, bcount, sp, seg_buf, seg_addr);
		pseg_addr += fsbtodb(lfsp, nblocks) +
		    bytetoda(fsp, LFS_SUMMARY_SIZE);
		s += (nblocks << lfsp->lfs_bshift) + LFS_SUMMARY_SIZE;
	}
	qsort(bip, *bcount, sizeof(BLOCK_INFO), bi_compare);
	toss(bip, bcount, sizeof(BLOCK_INFO), bi_toss, NULL);
#ifdef VERBOSE
	{
		BLOCK_INFO *_bip;
		int i;

		printf("BLOCK INFOS\n");
		for (_bip = bip, i=0; i < *bcount; ++_bip, ++i)
			PRINT_BINFO(_bip);
	}
#endif
	*blocks = bip;
	return (0);

err0:	*bcount = 0;
	return (-1);

}

/*
 * This will parse a partial segment and fill in BLOCK_INFO structures
 * for each block described in the segment summary.  It will not include
 * blocks or inodes from files with new version numbers.
 */
void
add_blocks (fsp, bip, countp, sp, seg_buf, segaddr, psegaddr)
	FS_INFO *fsp;		/* pointer to super block */
	BLOCK_INFO *bip;	/* Block info array */
	int *countp;		/* IN/OUT: number of blocks in array */
	SEGSUM	*sp;		/* segment summmary pointer */
	caddr_t seg_buf;	/* buffer containing segment */
	daddr_t segaddr;	/* address of this segment */
	daddr_t psegaddr;	/* address of this partial segment */
{
	IFILE	*ifp;
	FINFO	*fip;
	caddr_t	bp;
	daddr_t	*dp, *iaddrp;
	int db_per_block, i, j;
	u_long page_size;

#ifdef VERBOSE
	printf("FILE INFOS\n");
#endif
	db_per_block = fsbtodb(&fsp->fi_lfs, 1);
	page_size = fsp->fi_lfs.lfs_bsize;
	bp = seg_buf + datobyte(fsp, psegaddr - segaddr) + LFS_SUMMARY_SIZE;
	bip += *countp;
	psegaddr += bytetoda(fsp, LFS_SUMMARY_SIZE);
	iaddrp = (daddr_t *)((caddr_t)sp + LFS_SUMMARY_SIZE);
	--iaddrp;
	for (fip = (FINFO *)(sp + 1), i = 0; i < sp->ss_nfinfo;
	    ++i, fip = (FINFO *)(&fip->fi_blocks[fip->fi_nblocks])) {

		ifp = IFILE_ENTRY(&fsp->fi_lfs, fsp->fi_ifilep, fip->fi_ino);
		PRINT_FINFO(fip, ifp);
		if (ifp->if_version > fip->fi_version)
			continue;
		dp = &(fip->fi_blocks[0]);
		for (j = 0; j < fip->fi_nblocks; j++, dp++) {
			while (psegaddr == *iaddrp) {
				psegaddr += db_per_block;
				bp += page_size;
				--iaddrp;
			}
			bip->bi_inode = fip->fi_ino;
			bip->bi_lbn = *dp;
			bip->bi_daddr = psegaddr;
			bip->bi_segcreate = (time_t)(sp->ss_create);
			bip->bi_bp = bp;
			bip->bi_version = ifp->if_version;
			psegaddr += db_per_block;
			bp += page_size;
			++bip;
			++(*countp);
		}
	}
}

/*
 * For a particular segment summary, reads the inode blocks and adds
 * INODE_INFO structures to the array.  Returns the number of inodes
 * actually added.
 */
void
add_inodes (fsp, bip, countp, sp, seg_buf, seg_addr)
	FS_INFO *fsp;		/* pointer to super block */
	BLOCK_INFO *bip;	/* block info array */
	int *countp;		/* pointer to current number of inodes */
	SEGSUM *sp;		/* segsum pointer */
	caddr_t	seg_buf;	/* the buffer containing the segment's data */
	daddr_t	seg_addr;	/* disk address of seg_buf */
{
	struct dinode *di;
	struct lfs *lfsp;
	IFILE *ifp;
	BLOCK_INFO *bp;
	daddr_t	*daddrp;
	ino_t inum;
	int i;

	if (sp->ss_ninos <= 0)
		return;

	bp = bip + *countp;
	lfsp = &fsp->fi_lfs;
#ifdef VERBOSE
	(void) printf("INODES:\n");
#endif
	daddrp = (daddr_t *)((caddr_t)sp + LFS_SUMMARY_SIZE);
	for (i = 0; i < sp->ss_ninos; ++i) {
		if (i % INOPB(lfsp) == 0) {
			--daddrp;
			di = (struct dinode *)(seg_buf +
			    ((*daddrp - seg_addr) << fsp->fi_daddr_shift));
		} else
			++di;

		inum = di->di_inumber;
		bp->bi_lbn = LFS_UNUSED_LBN;
		bp->bi_inode = inum;
		bp->bi_daddr = *daddrp;
		bp->bi_bp = di;
		bp->bi_segcreate = sp->ss_create;

		if (inum == LFS_IFILE_INUM) {
			bp->bi_version = 1;	/* Ifile version should be 1 */
			bp++;
			++(*countp);
			PRINT_INODE(1, bp);
		} else {
			ifp = IFILE_ENTRY(lfsp, fsp->fi_ifilep, inum);
			PRINT_INODE(ifp->if_daddr == *daddrp, bp);
			bp->bi_version = ifp->if_version;
			if (ifp->if_daddr == *daddrp) {
				bp++;
				++(*countp);
			}
		}
	}
}

/*
 * Checks the summary checksum and the data checksum to determine if the
 * segment is valid or not.  Returns the size of the partial segment if it
 * is valid, * and 0 otherwise.  Use dump_summary to figure out size of the
 * the partial as well as whether or not the checksum is valid.
 */
int
pseg_valid (fsp, ssp)
	FS_INFO *fsp;   /* pointer to file system info */
	SEGSUM *ssp;	/* pointer to segment summary block */
{
	caddr_t	p;
	int i, nblocks;
	u_long *datap;

	if ((nblocks = dump_summary(&fsp->fi_lfs, ssp, 0, NULL)) <= 0 ||
	    nblocks > fsp->fi_lfs.lfs_ssize - 1)
		return(0);

	/* check data/inode block(s) checksum too */
	datap = (u_long *)malloc(nblocks * sizeof(u_long));
	p = (caddr_t)ssp + LFS_SUMMARY_SIZE;
	for (i = 0; i < nblocks; ++i) {
		datap[i] = *((u_long *)p);
		p += fsp->fi_lfs.lfs_bsize;
	}
	if (cksum ((void *)datap, nblocks * sizeof(u_long)) != ssp->ss_datasum)
		return (0);

	return (nblocks);
}


/* #define MMAP_SEGMENT */
/*
 * read a segment into a memory buffer
 */
int
mmap_segment (fsp, segment, segbuf, use_mmap)
	FS_INFO *fsp;		/* file system information */
	int segment;		/* segment number */
	caddr_t *segbuf;	/* pointer to buffer area */
	int use_mmap;		/* mmap instead of read */
{
	struct lfs *lfsp;
	int fid;		/* fildes for file system device */
	daddr_t seg_daddr;	/* base disk address of segment */
	off_t seg_byte;
	size_t ssize;
	char mntfromname[MNAMELEN+2];

	lfsp = &fsp->fi_lfs;

	/* get the disk address of the beginning of the segment */
	seg_daddr = sntoda(lfsp, segment);
	seg_byte = datobyte(fsp, seg_daddr);
	ssize = seg_size(lfsp);

	strcpy(mntfromname, "/dev/r");
	strcat(mntfromname, fsp->fi_statfsp->f_mntfromname+5);

	if ((fid = open(mntfromname, O_RDONLY, (mode_t)0)) < 0) {
		err(0, "mmap_segment: bad open");
		return (-1);
	}

	if (use_mmap) {
		*segbuf = mmap ((caddr_t)0, seg_size(lfsp), PROT_READ,
		    0, fid, seg_byte);
		if (*(long *)segbuf < 0) {
			err(0, "mmap_segment: mmap failed");
			return (NULL);
		}
	} else {
#ifdef VERBOSE
		printf("mmap_segment\tseg_daddr: %lu\tseg_size: %lu\tseg_offset: %qu\n",
		    seg_daddr, ssize, seg_byte);
#endif
		/* malloc the space for the buffer */
		*segbuf = malloc(ssize);
		if (!*segbuf) {
			err(0, "mmap_segment: malloc failed");
			return(NULL);
		}

		/* read the segment data into the buffer */
		if (lseek (fid, seg_byte, SEEK_SET) != seg_byte) {
			err (0, "mmap_segment: bad lseek");
			free(*segbuf);
			return (-1);
		}

		if (read (fid, *segbuf, ssize) != ssize) {
			err (0, "mmap_segment: bad read");
			free(*segbuf);
			return (-1);
		}
	}
	close (fid);

	return (0);
}

void
munmap_segment (fsp, seg_buf, use_mmap)
	FS_INFO *fsp;		/* file system information */
	caddr_t seg_buf;	/* pointer to buffer area */
	int use_mmap;		/* mmap instead of read/write */
{
	if (use_mmap)
		munmap (seg_buf, seg_size(&fsp->fi_lfs));
	else
		free (seg_buf);
}


/*
 * USEFUL DEBUGGING TOOLS:
 */
void
print_SEGSUM (lfsp, p)
	struct lfs *lfsp;
	SEGSUM	*p;
{
	if (p)
		(void) dump_summary(lfsp, p, DUMP_ALL, NULL);
	else printf("0x0");
	fflush(stdout);
}

int
bi_compare(a, b)
	const void *a;
	const void *b;
{
	const BLOCK_INFO *ba, *bb;
	int diff;

	ba = a;
	bb = b;

	if (diff = (int)(ba->bi_inode - bb->bi_inode))
		return (diff);
	if (diff = (int)(ba->bi_lbn - bb->bi_lbn)) {
		if (ba->bi_lbn == LFS_UNUSED_LBN)
			return(-1);
		else if (bb->bi_lbn == LFS_UNUSED_LBN)
			return(1);
		else if (ba->bi_lbn < 0 && bb->bi_lbn >= 0)
			return(1);
		else if (bb->bi_lbn < 0 && ba->bi_lbn >= 0)
			return(-1);
		else
			return (diff);
	}
	if (diff = (int)(ba->bi_segcreate - bb->bi_segcreate))
		return (diff);
	diff = (int)(ba->bi_daddr - bb->bi_daddr);
	return (diff);
}

int
bi_toss(dummy, a, b)
	const void *dummy;
	const void *a;
	const void *b;
{
	const BLOCK_INFO *ba, *bb;

	ba = a;
	bb = b;

	return(ba->bi_inode == bb->bi_inode && ba->bi_lbn == bb->bi_lbn);
}

void
toss(p, nump, size, dotoss, client)
	void *p;
	int *nump;
	size_t size;
	int (*dotoss) __P((const void *, const void *, const void *));
	void *client;
{
	int i;
	void *p1;

	if (*nump == 0)
		return;

	for (i = *nump; --i > 0;) {
		p1 = p + size;
		if (dotoss(client, p, p1)) {
			memmove(p, p1, i * size);
			--(*nump);
		} else
			p += size;
	}
}
