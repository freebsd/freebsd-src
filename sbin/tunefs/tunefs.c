/*
 * Copyright (c) 1983 The Regents of the University of California.
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
char copyright[] =
"@(#) Copyright (c) 1983 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)tunefs.c	5.11 (Berkeley) 6/1/90";
#endif /* not lint */

/*
 * tunefs: change layout parameters to an existing file system.
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <ufs/fs.h>
#include <fstab.h>
#include <stdio.h>
#include <paths.h>

union {
	struct	fs sb;
	char pad[MAXBSIZE];
} sbun;
#define	sblock sbun.sb

int fi;
long dev_bsize = 1;

main(argc, argv)
	int argc;
	char *argv[];
{
	char *cp, *special, *name;
	struct stat st;
	int i;
	int Aflag = 0;
	struct fstab *fs;
	char *chg[2], device[MAXPATHLEN];

	argc--, argv++; 
	if (argc < 2)
		goto usage;
	special = argv[argc - 1];
	fs = getfsfile(special);
	if (fs)
		special = fs->fs_spec;
again:
	if (stat(special, &st) < 0) {
		if (*special != '/') {
			if (*special == 'r')
				special++;
			(void)sprintf(device, "%s/%s", _PATH_DEV, special);
			special = device;
			goto again;
		}
		fprintf(stderr, "tunefs: "); perror(special);
		exit(1);
	}
	if ((st.st_mode & S_IFMT) != S_IFBLK &&
	    (st.st_mode & S_IFMT) != S_IFCHR)
		fatal("%s: not a block or character device", special);
	getsb(&sblock, special);
	for (; argc > 0 && argv[0][0] == '-'; argc--, argv++) {
		for (cp = &argv[0][1]; *cp; cp++)
			switch (*cp) {

			case 'A':
				Aflag++;
				continue;

			case 'a':
				name = "maximum contiguous block count";
				if (argc < 1)
					fatal("-a: missing %s", name);
				argc--, argv++;
				i = atoi(*argv);
				if (i < 1)
					fatal("%s: %s must be >= 1",
						*argv, name);
				fprintf(stdout, "%s changes from %d to %d\n",
					name, sblock.fs_maxcontig, i);
				sblock.fs_maxcontig = i;
				continue;

			case 'd':
				name =
				   "rotational delay between contiguous blocks";
				if (argc < 1)
					fatal("-d: missing %s", name);
				argc--, argv++;
				i = atoi(*argv);
				fprintf(stdout,
					"%s changes from %dms to %dms\n",
					name, sblock.fs_rotdelay, i);
				sblock.fs_rotdelay = i;
				continue;

			case 'e':
				name =
				  "maximum blocks per file in a cylinder group";
				if (argc < 1)
					fatal("-e: missing %s", name);
				argc--, argv++;
				i = atoi(*argv);
				if (i < 1)
					fatal("%s: %s must be >= 1",
						*argv, name);
				fprintf(stdout, "%s changes from %d to %d\n",
					name, sblock.fs_maxbpg, i);
				sblock.fs_maxbpg = i;
				continue;

			case 'm':
				name = "minimum percentage of free space";
				if (argc < 1)
					fatal("-m: missing %s", name);
				argc--, argv++;
				i = atoi(*argv);
				if (i < 0 || i > 99)
					fatal("%s: bad %s", *argv, name);
				fprintf(stdout,
					"%s changes from %d%% to %d%%\n",
					name, sblock.fs_minfree, i);
				sblock.fs_minfree = i;
				if (i >= 10 && sblock.fs_optim == FS_OPTSPACE)
					fprintf(stdout, "should optimize %s",
					    "for time with minfree >= 10%\n");
				if (i < 10 && sblock.fs_optim == FS_OPTTIME)
					fprintf(stdout, "should optimize %s",
					    "for space with minfree < 10%\n");
				continue;

			case 'o':
				name = "optimization preference";
				if (argc < 1)
					fatal("-o: missing %s", name);
				argc--, argv++;
				chg[FS_OPTSPACE] = "space";
				chg[FS_OPTTIME] = "time";
				if (strcmp(*argv, chg[FS_OPTSPACE]) == 0)
					i = FS_OPTSPACE;
				else if (strcmp(*argv, chg[FS_OPTTIME]) == 0)
					i = FS_OPTTIME;
				else
					fatal("%s: bad %s (options are `space' or `time')",
						*argv, name);
				if (sblock.fs_optim == i) {
					fprintf(stdout,
						"%s remains unchanged as %s\n",
						name, chg[i]);
					continue;
				}
				fprintf(stdout,
					"%s changes from %s to %s\n",
					name, chg[sblock.fs_optim], chg[i]);
				sblock.fs_optim = i;
				if (sblock.fs_minfree >= 10 && i == FS_OPTSPACE)
					fprintf(stdout, "should optimize %s",
					    "for time with minfree >= 10%\n");
				if (sblock.fs_minfree < 10 && i == FS_OPTTIME)
					fprintf(stdout, "should optimize %s",
					    "for space with minfree < 10%\n");
				continue;

			default:
				fatal("-%c: unknown flag", *cp);
			}
	}
	if (argc != 1)
		goto usage;
	bwrite(SBOFF / dev_bsize, (char *)&sblock, SBSIZE);
	if (Aflag)
		for (i = 0; i < sblock.fs_ncg; i++)
			bwrite(fsbtodb(&sblock, cgsblock(&sblock, i)),
			    (char *)&sblock, SBSIZE);
	close(fi);
	exit(0);
usage:
	fprintf(stderr, "Usage: tunefs tuneup-options special-device\n");
	fprintf(stderr, "where tuneup-options are:\n");
	fprintf(stderr, "\t-a maximum contiguous blocks\n");
	fprintf(stderr, "\t-d rotational delay between contiguous blocks\n");
	fprintf(stderr, "\t-e maximum blocks per file in a cylinder group\n");
	fprintf(stderr, "\t-m minimum percentage of free space\n");
	fprintf(stderr, "\t-o optimization preference (`space' or `time')\n");
	exit(2);
}

getsb(fs, file)
	register struct fs *fs;
	char *file;
{

	fi = open(file, 2);
	if (fi < 0) {
		fprintf(stderr, "cannot open");
		perror(file);
		exit(3);
	}
	if (bread(SBOFF, (char *)fs, SBSIZE)) {
		fprintf(stderr, "bad super block");
		perror(file);
		exit(4);
	}
	if (fs->fs_magic != FS_MAGIC) {
		fprintf(stderr, "%s: bad magic number\n", file);
		exit(5);
	}
	dev_bsize = fs->fs_fsize / fsbtodb(fs, 1);
}

bwrite(blk, buf, size)
	char *buf;
	daddr_t blk;
	register size;
{
	if (lseek(fi, blk * dev_bsize, 0) < 0) {
		perror("FS SEEK");
		exit(6);
	}
	if (write(fi, buf, size) != size) {
		perror("FS WRITE");
		exit(7);
	}
}

bread(bno, buf, cnt)
	daddr_t bno;
	char *buf;
{
	register i;

	if (lseek(fi, bno * dev_bsize, 0) < 0)
		return(1);
	if ((i = read(fi, buf, cnt)) != cnt) {
		for(i=0; i<sblock.fs_bsize; i++)
			buf[i] = 0;
		return (1);
	}
	return (0);
}

/* VARARGS1 */
fatal(fmt, arg1, arg2)
	char *fmt, *arg1, *arg2;
{

	fprintf(stderr, "tunefs: ");
	fprintf(stderr, fmt, arg1, arg2);
	putc('\n', stderr);
	exit(10);
}
