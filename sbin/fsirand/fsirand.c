/*	$OpenBSD: fsirand.c,v 1.9 1997/02/28 00:46:33 millert Exp $	*/

/*
 * Copyright (c) 1997 Todd C. Miller <Todd.Miller@courtesan.com>
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
 *	This product includes software developed by Todd C. Miller.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/disklabel.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(void) __dead2;
int fsirand(char *);

/*
 * Possible superblock locations ordered from most to least likely.
 */
static int sblock_try[] = SBLOCKSEARCH;

int printonly = 0, force = 0, ignorelabel = 0;

int
main(int argc, char *argv[])
{
	int n, ex = 0;
	struct rlimit rl;

	while ((n = getopt(argc, argv, "bfp")) != -1) {
		switch (n) {
		case 'b':
			ignorelabel++;
			break;
		case 'p':
			printonly++;
			break;
		case 'f':
			force++;
			break;
		default:
			usage();
		}
	}
	if (argc - optind < 1)
		usage();

	srandomdev();

	/* Increase our data size to the max */
	if (getrlimit(RLIMIT_DATA, &rl) == 0) {
		rl.rlim_cur = rl.rlim_max;
		if (setrlimit(RLIMIT_DATA, &rl) < 0)
			warn("can't get resource limit to max data size");
	} else
		warn("can't get resource limit for data size");

	for (n = optind; n < argc; n++) {
		if (argc - optind != 1)
			(void)puts(argv[n]);
		ex += fsirand(argv[n]);
		if (n < argc - 1)
			putchar('\n');
	}

	exit(ex);
}

int
fsirand(char *device)
{
	struct ufs1_dinode *dp1;
	struct ufs2_dinode *dp2;
	caddr_t inodebuf;
	size_t ibufsize;
	struct fs *sblock;
	ino_t inumber, maxino;
	ufs2_daddr_t sblockloc, dblk;
	char sbuf[SBLOCKSIZE], sbuftmp[SBLOCKSIZE];
	int i, devfd, n, cg;
	u_int32_t bsize = DEV_BSIZE;
	struct disklabel label;

	if ((devfd = open(device, printonly ? O_RDONLY : O_RDWR)) < 0) {
		warn("can't open %s", device);
		return (1);
	}

	/* Get block size (usually 512) from disklabel if possible */
	if (!ignorelabel) {
		if (ioctl(devfd, DIOCGDINFO, &label) < 0)
			warn("can't read disklabel, using sector size of %d",
			    bsize);
		else
			bsize = label.d_secsize;
	}

	/* Read in master superblock */
	(void)memset(&sbuf, 0, sizeof(sbuf));
	sblock = (struct fs *)&sbuf;
	for (i = 0; sblock_try[i] != -1; i++) {
		sblockloc = sblock_try[i];
		if (lseek(devfd, sblockloc, SEEK_SET) == -1) {
			warn("can't seek to superblock (%qd) on %s",
			    sblockloc, device);
			return (1);
		}
		if ((n = read(devfd, (void *)sblock, SBLOCKSIZE))!=SBLOCKSIZE) {
			warnx("can't read superblock on %s: %s", device,
			    (n < SBLOCKSIZE) ? "short read" : strerror(errno));
			return (1);
		}
		if ((sblock->fs_magic == FS_UFS1_MAGIC ||
		     (sblock->fs_magic == FS_UFS2_MAGIC &&
		      sblock->fs_sblockloc == sblock_try[i])) &&
		    sblock->fs_bsize <= MAXBSIZE &&
		    sblock->fs_bsize >= sizeof(struct fs))
			break;
	}
	if (sblock_try[i] == -1) {
		fprintf(stderr, "Cannot find file system superblock\n");
		return (1);
	}
	maxino = sblock->fs_ncg * sblock->fs_ipg;

	if (sblock->fs_magic == FS_UFS1_MAGIC &&
	    sblock->fs_old_inodefmt < FS_44INODEFMT) {
		warnx("file system format is too old, sorry");
		return (1);
	}
	if (!force && !printonly && sblock->fs_clean != 1) {
		warnx("file system is not clean, fsck %s first", device);
		return (1);
	}

	/* Make sure backup superblocks are sane. */
	sblock = (struct fs *)&sbuftmp;
	for (cg = 0; cg < sblock->fs_ncg; cg++) {
		dblk = fsbtodb(sblock, cgsblock(sblock, cg));
		if (lseek(devfd, (off_t)dblk * bsize, SEEK_SET) < 0) {
			warn("can't seek to %qd", (off_t)dblk * bsize);
			return (1);
		} else if ((n = write(devfd, (void *)sblock, SBLOCKSIZE)) != SBLOCKSIZE) {
			warn("can't read backup superblock %d on %s: %s",
			    cg + 1, device, (n < SBLOCKSIZE) ? "short write"
			    : strerror(errno));
			return (1);
		}
		if (sblock->fs_magic != FS_UFS1_MAGIC &&
		    sblock->fs_magic != FS_UFS2_MAGIC) {
			warnx("bad magic number in backup superblock %d on %s",
			    cg + 1, device);
			return (1);
		}
		if (sblock->fs_sbsize > SBLOCKSIZE) {
			warnx("size of backup superblock %d on %s is preposterous",
			    cg + 1, device);
			return (1);
		}
	}
	sblock = (struct fs *)&sbuf;

	/* XXX - should really cap buffer at 512kb or so */
	if (sblock->fs_magic == FS_UFS1_MAGIC)
		ibufsize = sizeof(struct ufs1_dinode) * sblock->fs_ipg;
	else
		ibufsize = sizeof(struct ufs2_dinode) * sblock->fs_ipg;
	if ((inodebuf = malloc(ibufsize)) == NULL)
		errx(1, "can't allocate memory for inode buffer");

	if (printonly && (sblock->fs_id[0] || sblock->fs_id[1])) {
		if (sblock->fs_id[0])
			(void)printf("%s was randomized on %s", device,
			    ctime((const time_t *)&(sblock->fs_id[0])));
		(void)printf("fsid: %x %x\n", sblock->fs_id[0],
			    sblock->fs_id[1]);
	}

	/* Randomize fs_id unless old 4.2BSD file system */
	if (!printonly) {
		/* Randomize fs_id and write out new sblock and backups */
		sblock->fs_id[0] = (u_int32_t)time(NULL);
		sblock->fs_id[1] = random();

		if (lseek(devfd, sblockloc, SEEK_SET) == -1) {
			warn("can't seek to superblock (%qd) on %s", sblockloc,
			    device);
			return (1);
		}
		if ((n = write(devfd, (void *)sblock, SBLOCKSIZE)) !=
		    SBLOCKSIZE) {
			warn("can't write superblock on %s: %s", device,
			    (n < SBLOCKSIZE) ? "short write" : strerror(errno));
			return (1);
		}
	}

	/* For each cylinder group, randomize inodes and update backup sblock */
	for (cg = 0, inumber = 0; cg < sblock->fs_ncg; cg++) {
		/* Update superblock if appropriate */
		if (!printonly) {
			dblk = fsbtodb(sblock, cgsblock(sblock, cg));
			if (lseek(devfd, (off_t)dblk * bsize, SEEK_SET) < 0) {
				warn("can't seek to %qd", (off_t)dblk * bsize);
				return (1);
			} else if ((n = write(devfd, (void *)sblock,
			    SBLOCKSIZE)) != SBLOCKSIZE) {
			      warn("can't write backup superblock %d on %s: %s",
				    cg + 1, device, (n < SBLOCKSIZE) ?
				    "short write" : strerror(errno));
				return (1);
			}
		}

		/* Read in inodes, then print or randomize generation nums */
		dblk = fsbtodb(sblock, ino_to_fsba(sblock, inumber));
		if (lseek(devfd, (off_t)dblk * bsize, SEEK_SET) < 0) {
			warn("can't seek to %qd", (off_t)dblk * bsize);
			return (1);
		} else if ((n = read(devfd, inodebuf, ibufsize)) != ibufsize) {
			warnx("can't read inodes: %s",
			     (n < ibufsize) ? "short read" : strerror(errno));
			return (1);
		}

		for (n = 0; n < sblock->fs_ipg; n++, inumber++) {
			if (sblock->fs_magic == FS_UFS1_MAGIC)
				dp1 = &((struct ufs1_dinode *)inodebuf)[n];
			else
				dp2 = &((struct ufs2_dinode *)inodebuf)[n];
			if (inumber >= ROOTINO) {
				if (printonly)
					(void)printf("ino %d gen %qx\n",
					    inumber,
					    sblock->fs_magic == FS_UFS1_MAGIC ?
					    (quad_t)dp1->di_gen : dp2->di_gen);
				else if (sblock->fs_magic == FS_UFS1_MAGIC) 
					dp1->di_gen = random(); 
				else
					dp2->di_gen = random();
			}
		}

		/* Write out modified inodes */
		if (!printonly) {
			if (lseek(devfd, (off_t)dblk * bsize, SEEK_SET) < 0) {
				warn("can't seek to %qd",
				    (off_t)dblk * bsize);
				return (1);
			} else if ((n = write(devfd, inodebuf, ibufsize)) !=
				 ibufsize) {
				warnx("can't write inodes: %s",
				     (n != ibufsize) ? "short write" :
				     strerror(errno));
				return (1);
			}
		}
	}
	(void)close(devfd);

	return(0);
}

static void
usage(void)
{
	(void)fprintf(stderr, 
		"usage: fsirand [-b] [-f] [-p] special [special ...]\n");
	exit(1);
}
