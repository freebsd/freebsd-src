/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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

#if 0
#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)clri.c	8.2 (Berkeley) 9/23/93";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/disklabel.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libufs.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

static void
usage(void)
{
	(void)fprintf(stderr, "usage: clri special_device inode_number ...\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct fs *fs;
	struct ufs1_dinode *dp1;
	struct ufs2_dinode *dp2;
	char *ibuf[MAXBSIZE];
	long generation, bsize;
	off_t offset;
	int fd, ret, inonum;
	char *fsname;
	void *v = ibuf;

	if (argc < 3)
		usage();

	fsname = *++argv;

	/* get the superblock. */
	if ((fd = open(fsname, O_RDWR, 0)) < 0)
		err(1, "%s", fsname);
	if ((ret = sbget(fd, &fs, -1)) != 0) {
		switch (ret) {
		case ENOENT:
			warn("Cannot find file system superblock");
			return (1);
		default:
			warn("Unable to read file system superblock");
			return (1);
		}
	}
	bsize = fs->fs_bsize;

	/* remaining arguments are inode numbers. */
	while (*++argv) {
		/* get the inode number. */
		if ((inonum = atoi(*argv)) <= 0)
			errx(1, "%s is not a valid inode number", *argv);
		(void)printf("clearing %d\n", inonum);

		/* read in the appropriate block. */
		offset = ino_to_fsba(fs, inonum);	/* inode to fs blk */
		offset = fsbtodb(fs, offset);		/* fs blk disk blk */
		offset *= DEV_BSIZE;			/* disk blk to bytes */

		/* seek and read the block */
		if (lseek(fd, offset, SEEK_SET) < 0)
			err(1, "%s", fsname);
		if (read(fd, ibuf, bsize) != bsize)
			err(1, "%s", fsname);

		if (fs->fs_magic == FS_UFS2_MAGIC) {
			/* get the inode within the block. */
			dp2 = &(((struct ufs2_dinode *)v)
			    [ino_to_fsbo(fs, inonum)]);

			/* clear the inode, and bump the generation count. */
			generation = dp2->di_gen + 1;
			memset(dp2, 0, sizeof(*dp2));
			dp2->di_gen = generation;
		} else {
			/* get the inode within the block. */
			dp1 = &(((struct ufs1_dinode *)v)
			    [ino_to_fsbo(fs, inonum)]);

			/* clear the inode, and bump the generation count. */
			generation = dp1->di_gen + 1;
			memset(dp1, 0, sizeof(*dp1));
			dp1->di_gen = generation;
		}

		/* backup and write the block */
		if (lseek(fd, (off_t)-bsize, SEEK_CUR) < 0)
			err(1, "%s", fsname);
		if (write(fd, ibuf, bsize) != bsize)
			err(1, "%s", fsname);
		(void)fsync(fd);
	}
	(void)close(fd);
	exit(0);
}
