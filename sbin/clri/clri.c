/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rich $alz of BBN Inc.
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
static const char copyright[] =
"@(#) Copyright (c) 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)clri.c	8.2 (Berkeley) 9/23/93";
#endif
#endif /* not lint */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/disklabel.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

/*
 * Possible superblock locations ordered from most to least likely.
 */
static int sblock_try[] = SBLOCKSEARCH;

static void
usage(void)
{
	(void)fprintf(stderr, "usage: clri special_device inode_number ...\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct fs *sbp;
	struct ufs1_dinode *dp1;
	struct ufs2_dinode *dp2;
	char *ibuf[MAXBSIZE];
	long generation, bsize;
	off_t offset;
	int i, fd, inonum;
	char *fs, sblock[SBLOCKSIZE];

	if (argc < 3)
		usage();

	fs = *++argv;

	/* get the superblock. */
	if ((fd = open(fs, O_RDWR, 0)) < 0)
		err(1, "%s", fs);
	for (i = 0; sblock_try[i] != -1; i++) {
		if (lseek(fd, (off_t)(sblock_try[i]), SEEK_SET) < 0)
			err(1, "%s", fs);
		if (read(fd, sblock, sizeof(sblock)) != sizeof(sblock))
			errx(1, "%s: can't read superblock", fs);
		sbp = (struct fs *)sblock;
		if ((sbp->fs_magic == FS_UFS1_MAGIC ||
		     (sbp->fs_magic == FS_UFS2_MAGIC &&
		      sbp->fs_sblockloc == numfrags(sbp, sblock_try[i]))) &&
		    sbp->fs_bsize <= MAXBSIZE &&
		    sbp->fs_bsize >= (int)sizeof(struct fs))
			break;
	}
	if (sblock_try[i] == -1) {
		fprintf(stderr, "Cannot find file system superblock\n");
		exit(2);
	}
	bsize = sbp->fs_bsize;

	/* remaining arguments are inode numbers. */
	while (*++argv) {
		/* get the inode number. */
		if ((inonum = atoi(*argv)) <= 0)
			errx(1, "%s is not a valid inode number", *argv);
		(void)printf("clearing %d\n", inonum);

		/* read in the appropriate block. */
		offset = ino_to_fsba(sbp, inonum);	/* inode to fs blk */
		offset = fsbtodb(sbp, offset);		/* fs blk disk blk */
		offset *= DEV_BSIZE;			/* disk blk to bytes */

		/* seek and read the block */
		if (lseek(fd, offset, SEEK_SET) < 0)
			err(1, "%s", fs);
		if (read(fd, ibuf, bsize) != bsize)
			err(1, "%s", fs);

		if (sbp->fs_magic == FS_UFS2_MAGIC) {
			/* get the inode within the block. */
			dp2 = &(((struct ufs2_dinode *)ibuf)
			    [ino_to_fsbo(sbp, inonum)]);

			/* clear the inode, and bump the generation count. */
			generation = dp2->di_gen + 1;
			memset(dp2, 0, sizeof(*dp2));
			dp2->di_gen = generation;
		} else {
			/* get the inode within the block. */
			dp1 = &(((struct ufs1_dinode *)ibuf)
			    [ino_to_fsbo(sbp, inonum)]);

			/* clear the inode, and bump the generation count. */
			generation = dp1->di_gen + 1;
			memset(dp1, 0, sizeof(*dp1));
			dp1->di_gen = generation;
		}

		/* backup and write the block */
		if (lseek(fd, (off_t)-bsize, SEEK_CUR) < 0)
			err(1, "%s", fs);
		if (write(fd, ibuf, bsize) != bsize)
			err(1, "%s", fs);
		(void)fsync(fd);
	}
	(void)close(fd);
	exit(0);
}
