/*
 * Copyright (c) 1998, 2003, 2013, 2018 Marshall Kirk McKusick.
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY MARSHALL KIRK MCKUSICK ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL MARSHALL KIRK MCKUSICK BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/time.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

union dinode {
	struct ufs1_dinode *dp1;
	struct ufs2_dinode *dp2;
};
struct fs *sbp;
char *fsname;
int fd;

void indirprt(int level, int blksperindir, int lbn, ufs2_daddr_t blkno,
	int lastlbn);
void printblk(int lbn, ufs2_daddr_t blkno, int numblks, int lastlbn);

/* 
 * Possible superblock locations ordered from most to least likely.
 */
static int sblock_try[] = SBLOCKSEARCH;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int i, len, lbn, frags, inonum, numblks, blksperindir;
	char sblock[SBLOCKSIZE], ibuf[MAXBSIZE];
	ufs2_daddr_t blkno;
	off_t size, offset;
	union dinode dp;

	if (argc < 3) {
		(void)fprintf(stderr,"usage: prtblknos filesystem inode ...\n");
		exit(1);
	}

	fsname = *++argv;

	/* get the superblock. */
	if ((fd = open(fsname, O_RDONLY, 0)) < 0)
		err(1, "%s", fsname);
	for (i = 0; sblock_try[i] != -1; i++) {
		if (lseek(fd, sblock_try[i], SEEK_SET) < 0)
			err(1, "lseek: %s", fsname);
		if (read(fd, sblock, (long)SBLOCKSIZE) != SBLOCKSIZE)
			err(1, "can't read superblock: %s", fsname);
		sbp = (struct fs *)sblock;
		if ((sbp->fs_magic == FS_UFS1_MAGIC ||
		     (sbp->fs_magic == FS_UFS2_MAGIC &&
		      sbp->fs_sblockloc == sblock_try[i])) &&
		    sbp->fs_bsize <= MAXBSIZE &&
		    sbp->fs_bsize >= sizeof(struct fs))
			break;
	}
	if (sblock_try[i] == -1)
		errx(1, "Cannot find file system superblock\n");

	/* remaining arguments are inode numbers. */
	while (*++argv) {
		/* get the inode number. */
		if ((inonum = atoi(*argv)) <= 0)
			errx(1, "%s is not a valid inode number", *argv);
		(void)printf("%d:", inonum);

		/* read in the appropriate block. */
		offset = ino_to_fsba(sbp, inonum);	/* inode to fs blk */
		offset = fsbtodb(sbp, offset);		/* fs blk disk blk */
		offset *= DEV_BSIZE;			/* disk blk to bytes */

		/* seek and read the block */
		if (lseek(fd, offset, SEEK_SET) < 0)
			err(1, "%s", fsname);
		if (read(fd, ibuf, sbp->fs_bsize) != sbp->fs_bsize)
			err(1, "%s", fsname);

		/* get the inode within the block. */
		if (sbp->fs_magic == FS_UFS1_MAGIC) {
			dp.dp1 = &((struct ufs1_dinode *)(ibuf))
			    [ino_to_fsbo(sbp, inonum)];
			size = dp.dp1->di_size;
		} else {
			dp.dp2 = &((struct ufs2_dinode *)(ibuf))
			    [ino_to_fsbo(sbp, inonum)];
			size = dp.dp2->di_size;
		}

		numblks = howmany(size, sbp->fs_bsize);
		if (numblks == 0) {
			printf(" empty file\n");
			continue;
		}
		len = numblks < UFS_NDADDR ? numblks : UFS_NDADDR;
		for (i = 0; i < len; i++) {
			if (i < numblks - 1)
				frags = sbp->fs_frag;
			else
				frags = howmany(size % sbp->fs_bsize,
						  sbp->fs_fsize);
			if (sbp->fs_magic == FS_UFS1_MAGIC)
				blkno = dp.dp1->di_db[i];
			else
				blkno = dp.dp2->di_db[i];
			printblk(i, blkno, frags, numblks);
		}

		blksperindir = 1;
		len = numblks - UFS_NDADDR;
		lbn = UFS_NDADDR;
		for (i = 0; len > 0 && i < UFS_NIADDR; i++) {
			if (sbp->fs_magic == FS_UFS1_MAGIC)
				blkno = dp.dp1->di_ib[i];
			else
				blkno = dp.dp2->di_ib[i];
			indirprt(i, blksperindir, lbn, blkno, numblks);
			blksperindir *= NINDIR(sbp);
			lbn += blksperindir;
			len -= blksperindir;
		}

		/* dummy print to flush out last extent */
		printblk(numblks, 0, frags, 0);
	}
	(void)close(fd);
	exit(0);
}

void
indirprt(level, blksperindir, lbn, blkno, lastlbn)
	int level;
	int blksperindir;
	int lbn;
	ufs2_daddr_t blkno;
	int lastlbn;
{
	char indir[MAXBSIZE];
	off_t offset;
	int i, last;

	printblk(lbn, blkno, sbp->fs_frag, -level);
	/* read in the indirect block. */
	offset = fsbtodb(sbp, blkno);		/* fs blk disk blk */
	offset *= DEV_BSIZE;			/* disk blk to bytes */
	if (lseek(fd, offset, SEEK_SET) < 0)
		err(1, "%s", fsname);
	if (read(fd, indir, sbp->fs_bsize) != sbp->fs_bsize)
		err(1, "%s", fsname);
	last = howmany(lastlbn - lbn, blksperindir) < NINDIR(sbp) ?
	    howmany(lastlbn - lbn, blksperindir) : NINDIR(sbp);
	if (blksperindir == 1) {
		for (i = 0; i < last; i++) {
			if (sbp->fs_magic == FS_UFS1_MAGIC)
				blkno = ((ufs1_daddr_t *)indir)[i];
			else
				blkno = ((ufs2_daddr_t *)indir)[i];
			printblk(lbn + i, blkno, sbp->fs_frag, lastlbn);
		}
		return;
	}
	for (i = 0; i < last; i++) {
		if (sbp->fs_magic == FS_UFS1_MAGIC)
			blkno = ((ufs1_daddr_t *)indir)[i];
		else
			blkno = ((ufs2_daddr_t *)indir)[i];
		indirprt(level - 1, blksperindir / NINDIR(sbp),
		    lbn + blksperindir * i, blkno, lastlbn);
	}
}

char *
distance(lastblk, firstblk)
	daddr_t lastblk;
	daddr_t firstblk;
{
	daddr_t delta;
	int firstcg, lastcg;
	static char buf[100];

	if (lastblk == 0)
		return ("");
	delta = firstblk - lastblk - 1;
	firstcg = dtog(sbp, firstblk);
	lastcg = dtog(sbp, lastblk);
	if (firstcg == lastcg) {
		snprintf(buf, 100, " distance %jd", (intmax_t)delta);
		return (&buf[0]);
	}
	snprintf(buf, 100, " cg %d blk %jd to cg %d blk %jd",
	    lastcg, dtogd(sbp, lastblk), firstcg, dtogd(sbp, firstblk));
	return (&buf[0]);
}
	

char *indirname[UFS_NIADDR] = { "First", "Second", "Third" };

void
printblk(lbn, blkno, numblks, lastlbn)
	int lbn;
	ufs2_daddr_t blkno;
	int numblks;
	int lastlbn;
{
	static int seq;
	static daddr_t lastindirblk, lastblk, firstblk;

	if (lastlbn <= 0)
		goto flush;
	if (seq == 0) {
		seq = 1;
		firstblk = blkno;
		return;
	}
	if (lbn == 0) {
		seq = 1;
		lastblk = 0;
		firstblk = blkno;
		lastindirblk = 0;
		return;
	}
	if (lbn < lastlbn && ((firstblk == 0 && blkno == 0) ||
	    (firstblk == BLK_NOCOPY && blkno == BLK_NOCOPY) ||
	    (firstblk == BLK_SNAP && blkno == BLK_SNAP) ||
	    blkno == firstblk + seq * numblks)) {
		seq++;
		return;
	}
flush:
	if (seq == 0)
		goto prtindir;
	if (firstblk <= BLK_SNAP) {
		if (seq == 1)
			printf("\tlbn %d %s\n", lbn - seq,
			    firstblk == 0 ? "hole" :
			    firstblk == BLK_NOCOPY ? "nocopy" :
			    "snapblk");
		else
			printf("\tlbn %d-%d %s\n",
			    lbn - seq, lbn - 1,
			    firstblk == 0 ? "hole" :
			    firstblk == BLK_NOCOPY ? "nocopy" :
			    "snapblk");
	} else if (seq == 1) {
		if (numblks == 1)
			printf("\tlbn %d blkno %jd%s\n", lbn - seq,
			    (intmax_t)firstblk, distance(lastblk, firstblk));
		else
			printf("\tlbn %d blkno %jd-%jd%s\n", lbn - seq,
			    (intmax_t)firstblk,
			    (intmax_t)(firstblk + numblks - 1),
			    distance(lastblk, firstblk));
		lastblk = firstblk + numblks - 1;
	} else {
		printf("\tlbn %d-%d blkno %jd-%jd%s\n", lbn - seq, lbn - 1,
		    (intmax_t)firstblk, (intmax_t)(firstblk +
		    (seq - 1) * sbp->fs_frag + numblks - 1),
		    distance(lastblk, firstblk));
		lastblk = firstblk + (seq - 1) * sbp->fs_frag + numblks - 1;
	}
	if (lastlbn > 0 || blkno == 0) {
		seq = 1;
		firstblk = blkno;
		return;
	}
prtindir:
	if (seq != 0 && (sbp->fs_metaspace == 0 || lastindirblk == 0))
		lastindirblk = lastblk;
	printf("%s-level indirect, blkno %jd-%jd%s\n", indirname[-lastlbn],
	    (intmax_t)blkno, (intmax_t)(blkno + numblks - 1),
	    distance(lastindirblk, blkno));
	lastindirblk = blkno + numblks - 1;
	if (sbp->fs_metaspace == 0)
		lastblk = lastindirblk;
	seq = 0;
}
