/*
 * Copyright (c) 1981, 1983, 1993
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
static const char copyright[] =
"@(#) Copyright (c) 1981, 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static const char sccsid[] = "@(#)badsect.c	8.1 (Berkeley) 6/5/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * badsect
 *
 * Badsect takes a list of file-system relative sector numbers
 * and makes files containing the blocks of which these sectors are a part.
 * It can be used to contain sectors which have problems if these sectors
 * are not part of the bad file for the pack (see bad144).  For instance,
 * this program can be used if the driver for the file system in question
 * does not support bad block forwarding.
 */
#include <sys/param.h>
#include <sys/stat.h>

#include <ufs/ffs/fs.h>
#include <ufs/ufs/dinode.h>

#include <err.h>
#include <dirent.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

union {
	struct	fs fs;
	char	fsx[SBSIZE];
} ufs;
#define sblock	ufs.fs
union {
	struct	cg cg;
	char	cgx[MAXBSIZE];
} ucg;
#define	acg	ucg.cg
struct	fs *fs;
int	fso, fsi;
int	errs;
long	dev_bsize = 1;

char buf[MAXBSIZE];

void	rdfs __P((daddr_t, int, char *));
int	chkuse __P((daddr_t, int));

static void
usage(void)
{
	fprintf(stderr, "usage: badsect bbdir blkno ...\n");
	exit(1);
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	daddr_t diskbn;
	daddr_t number;
	struct stat stbuf, devstat;
	register struct dirent *dp;
	DIR *dirp;
	char name[2 * MAXPATHLEN];
	char *name_dir_end;

	if (argc < 3)
		usage();
	if (chdir(argv[1]) < 0 || stat(".", &stbuf) < 0)
		err(2, "%s", argv[1]);
	strcpy(name, _PATH_DEV);
	if ((dirp = opendir(name)) == NULL)
		err(3, "%s", name);
	name_dir_end = name + strlen(name);
	while ((dp = readdir(dirp)) != NULL) {
		strcpy(name_dir_end, dp->d_name);
		if (lstat(name, &devstat) < 0)
			err(4, "%s", name);
		if (stbuf.st_dev == devstat.st_rdev &&
		    (devstat.st_mode & IFMT) == IFCHR)
			break;
	}
	closedir(dirp);
	if (dp == NULL) {
		printf("Cannot find dev 0%lo corresponding to %s\n",
		    (u_long)stbuf.st_rdev, argv[1]);
		exit(5);
	}
	if ((fsi = open(name, O_RDONLY)) < 0)
		err(6, "%s", name);
	fs = &sblock;
	rdfs(SBOFF, SBSIZE, (char *)fs);
	dev_bsize = fs->fs_fsize / fsbtodb(fs, 1);
	for (argc -= 2, argv += 2; argc > 0; argc--, argv++) {
		number = atol(*argv);
		if (chkuse(number, 1))
			continue;
		/*
		 * Print a warning if converting the block number to a dev_t
		 * will truncate it.  badsect was not very useful in versions
		 * of BSD before 4.4 because dev_t was 16 bits and another
		 * bit was lost by bogus sign extensions.
		 */
		diskbn = dbtofsb(fs, number);
		if ((dev_t)diskbn != diskbn) {
			printf("sector %ld cannot be represented as a dev_t\n",
			    (long)number);
			errs++;
		}
		else if (mknod(*argv, IFMT|0600, (dev_t)diskbn) < 0) {
			warn("%s", *argv);
			errs++;
		}
	}
	printf("Don't forget to run ``fsck %s''\n", name);
	exit(errs);
}

int
chkuse(blkno, cnt)
	daddr_t blkno;
	int cnt;
{
	int cg;
	daddr_t fsbn, bn;

	fsbn = dbtofsb(fs, blkno);
	if ((unsigned)(fsbn+cnt) > fs->fs_size) {
		printf("block %ld out of range of file system\n", (long)blkno);
		return (1);
	}
	cg = dtog(fs, fsbn);
	if (fsbn < cgdmin(fs, cg)) {
		if (cg == 0 || (fsbn+cnt) > cgsblock(fs, cg)) {
			printf("block %ld in non-data area: cannot attach\n",
			    (long)blkno);
			return (1);
		}
	} else {
		if ((fsbn+cnt) > cgbase(fs, cg+1)) {
			printf("block %ld in non-data area: cannot attach\n",
			    (long)blkno);
			return (1);
		}
	}
	rdfs(fsbtodb(fs, cgtod(fs, cg)), (int)sblock.fs_cgsize,
	    (char *)&acg);
	if (!cg_chkmagic(&acg)) {
		fprintf(stderr, "cg %d: bad magic number\n", cg);
		errs++;
		return (1);
	}
	bn = dtogd(fs, fsbn);
	if (isclr(cg_blksfree(&acg), bn))
		printf("Warning: sector %ld is in use\n", (long)blkno);
	return (0);
}

/*
 * read a block from the file system
 */
void
rdfs(bno, size, bf)
	daddr_t bno;
	int size;
	char *bf;
{
	int n;

	if (lseek(fsi, (off_t)bno * dev_bsize, SEEK_SET) < 0) {
		printf("seek error: %ld\n", (long)bno);
		err(1, "rdfs");
	}
	n = read(fsi, bf, size);
	if (n != size) {
		printf("read error: %ld\n", (long)bno);
		err(1, "rdfs");
	}
}
