/*
 * Copyright (c) 1983, 1993
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
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)tunefs.c	8.2 (Berkeley) 4/19/94";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * tunefs: change layout parameters to an existing file system.
 */
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <ufs/ffs/fs.h>
#include <ufs/ufs/ufsmount.h>

#include <err.h>
#include <fcntl.h>
#include <fstab.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* the optimization warning string template */
#define	OPTWARN	"should optimize for %s with minfree %s %d%%"

union {
	struct	fs sb;
	char pad[MAXBSIZE];
} sbun;
#define	sblock sbun.sb

int fi;
long dev_bsize = 1;

void bwrite __P((daddr_t, const char *, int));
int bread __P((daddr_t, char *, int));
void getsb __P((struct fs *, const char *));
void putsb __P((struct fs *, const char *, int));
void usage __P((void));
void printfs __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	char *cp, *special;
	const char *name, *action;
	struct stat st;
	int i;
	int Aflag = 0, active = 0;
	struct fstab *fs;
	const char *chg[2];
	char device[MAXPATHLEN];
	struct ufs_args args;
	struct statfs stfs;

	argc--, argv++;
	if (argc < 2)
		usage();
	special = argv[argc - 1];
	fs = getfsfile(special);
	if (fs) {
		if (statfs(special, &stfs) == 0 &&
		    strcmp(special, stfs.f_mntonname) == 0) {
			active = 1;
		}
		special = fs->fs_spec;
	}
again:
	if (stat(special, &st) < 0) {
		if (*special != '/') {
			if (*special == 'r')
				special++;
			(void)snprintf(device, sizeof(device), "%s%s",
				       _PATH_DEV, special);
			special = device;
			goto again;
		}
		err(1, "%s", special);
	}
	if (fs == NULL && (st.st_mode & S_IFMT) == S_IFDIR)
		errx(10, "%s: unknown file system", special);
	getsb(&sblock, special);
	for (; argc > 0 && argv[0][0] == '-'; argc--, argv++) {
		for (cp = &argv[0][1]; *cp; cp++)
			switch (*cp) {

			case 'A':
				Aflag++;
				continue;

			case 'p':
				printfs();
				exit(0);

			case 'a':
				name = "maximum contiguous block count";
				if (argc < 1)
					errx(10, "-a: missing %s", name);
				argc--, argv++;
				i = atoi(*argv);
				if (i < 1)
					errx(10, "%s must be >= 1 (was %s)",
					    name, *argv);
				warnx("%s changes from %d to %d",
				    name, sblock.fs_maxcontig, i);
				sblock.fs_maxcontig = i;
				continue;

			case 'd':
				name =
				   "rotational delay between contiguous blocks";
				if (argc < 1)
					errx(10, "-d: missing %s", name);
				argc--, argv++;
				i = atoi(*argv);
				warnx("%s changes from %dms to %dms",
				    name, sblock.fs_rotdelay, i);
				sblock.fs_rotdelay = i;
				continue;

			case 'e':
				name =
				  "maximum blocks per file in a cylinder group";
				if (argc < 1)
					errx(10, "-e: missing %s", name);
				argc--, argv++;
				i = atoi(*argv);
				if (i < 1)
					errx(10, "%s must be >= 1 (was %s)",
					    name, *argv);
				warnx("%s changes from %d to %d",
				    name, sblock.fs_maxbpg, i);
				sblock.fs_maxbpg = i;
				continue;

			case 'f':
				name = "average file size";
				if (argc < 1)
					errx(10, "-a: missing %s", name);
				argc--, argv++;
				i = atoi(*argv);
				if (i < 1)
					errx(10, "%s must be >= 1 (was %s)", name, *argv);
				if (sblock.fs_avgfilesize == i) {
					warnx("%s remains unchanged as %d",
						name, i);
				} else {
					warnx("%s changes from %d to %d",
						name, sblock.fs_avgfilesize, i);
					sblock.fs_avgfilesize = i;
				}
				break;

			case 'm':
				name = "minimum percentage of free space";
				if (argc < 1)
					errx(10, "-m: missing %s", name);
				argc--, argv++;
				i = atoi(*argv);
				if (i < 0 || i > 99)
					errx(10, "bad %s (%s)", name, *argv);
				warnx("%s changes from %d%% to %d%%",
				    name, sblock.fs_minfree, i);
				sblock.fs_minfree = i;
				if (i >= MINFREE &&
				    sblock.fs_optim == FS_OPTSPACE)
					warnx(OPTWARN, "time", ">=", MINFREE);
				if (i < MINFREE &&
				    sblock.fs_optim == FS_OPTTIME)
					warnx(OPTWARN, "space", "<", MINFREE);
				continue;

			case 'n':
 				name = "soft updates";
 				if (argc < 1)
 					errx(10, "-n: missing %s", name);
 				argc--, argv++;
 				if (strcmp(*argv, "enable") == 0) {
 					sblock.fs_flags |= FS_DOSOFTDEP;
 					action = "set";
 				} else if (strcmp(*argv, "disable") == 0) {
 					sblock.fs_flags &= ~FS_DOSOFTDEP;
 					action = "cleared";
 				} else {
 					errx(10, "bad %s (options are %s)",
 					    name, "`enable' or `disable'");
 				}
 				warnx("%s %s", name, action);
 				continue;
 
			case 'o':
				name = "optimization preference";
				if (argc < 1)
					errx(10, "-o: missing %s", name);
				argc--, argv++;
				chg[FS_OPTSPACE] = "space";
				chg[FS_OPTTIME] = "time";
				if (strcmp(*argv, chg[FS_OPTSPACE]) == 0)
					i = FS_OPTSPACE;
				else if (strcmp(*argv, chg[FS_OPTTIME]) == 0)
					i = FS_OPTTIME;
				else
					errx(10, "bad %s (options are `space' or `time')",
					    name);
				if (sblock.fs_optim == i) {
					warnx("%s remains unchanged as %s",
					    name, chg[i]);
					continue;
				}
				warnx("%s changes from %s to %s",
				    name, chg[sblock.fs_optim], chg[i]);
				sblock.fs_optim = i;
				if (sblock.fs_minfree >= MINFREE &&
				    i == FS_OPTSPACE)
					warnx(OPTWARN, "time", ">=", MINFREE);
				if (sblock.fs_minfree < MINFREE &&
				    i == FS_OPTTIME)
					warnx(OPTWARN, "space", "<", MINFREE);
				continue;

			case 's':
				name = "expected number of files per directory";
				if (argc < 1)
					errx(10, "-a: missing %s", name);
				argc--, argv++;
				i = atoi(*argv);
				if (i < 1)
					errx(10, "%s must be >= 1 (was %s)", name, *argv);
				if (sblock.fs_avgfpdir == i) {
					warnx("%s remains unchanged as %d",
						name, i);
				} else {
					warnx("%s changes from %d to %d",
						name, sblock.fs_avgfpdir, i);
					sblock.fs_avgfpdir = i;
				}
				break;

			default:
				usage();
			}
	}
	if (argc != 1)
		usage();
	putsb(&sblock, special, Aflag);
	if (active) {
		bzero(&args, sizeof(args));
		if (mount("ufs", fs->fs_file,
		    stfs.f_flags | MNT_UPDATE | MNT_RELOAD, &args) < 0)
			err(9, "%s: reload", special);
		warnx("file system reloaded");
	}
	exit(0);
}

void
usage()
{
	fprintf(stderr, "%s\n%s\n%s\n",
"usage: tunefs [-A] [-a maxcontig] [-d rotdelay] [-e maxbpg] [-f avgfilesize]",
"              [-m minfree] [-p] [-n enable | disable] [-o space | time]",
"              [-s filesperdir] [special | filesystem]");
	exit(2);
}

void
getsb(fs, file)
	struct fs *fs;
	const char *file;
{

	fi = open(file, O_RDONLY);
	if (fi < 0)
		err(3, "cannot open %s", file);
	if (bread((daddr_t)SBOFF, (char *)fs, SBSIZE))
		err(4, "%s: bad super block", file);
	if (fs->fs_magic != FS_MAGIC)
		errx(5, "%s: bad magic number", file);
	dev_bsize = fs->fs_fsize / fsbtodb(fs, 1);
}

void
putsb(fs, file, all)
	struct fs *fs;
	const char *file;
	int all;
{
	int i;

	/*
	 * Re-open the device read-write. Use the read-only file
	 * descriptor as an interlock to prevent the device from
	 * being mounted while we are switching mode.
	 */
	i = fi;
	fi = open(file, O_RDWR);
	close(i);
	if (fi < 0)
		err(3, "cannot open %s", file);
	bwrite((daddr_t)SBOFF / dev_bsize, (const char *)fs, SBSIZE);
	if (all)
		for (i = 0; i < fs->fs_ncg; i++)
			bwrite(fsbtodb(fs, cgsblock(fs, i)),
			    (const char *)fs, SBSIZE);
	close(fi);
}

void
printfs()
{
	warnx("soft updates:  (-n)                                %s", 
		(sblock.fs_flags & FS_DOSOFTDEP)? "enabled" : "disabled");
	warnx("maximum contiguous block count: (-a)               %d",
	      sblock.fs_maxcontig);
	warnx("rotational delay between contiguous blocks: (-d)   %d ms",
	      sblock.fs_rotdelay);
	warnx("maximum blocks per file in a cylinder group: (-e)  %d",
	      sblock.fs_maxbpg);
	warnx("average file size: (-f)                            %d",
	      sblock.fs_avgfilesize);
	warnx("average number of files in a directory: (-s)       %d",
	      sblock.fs_avgfpdir);
	warnx("minimum percentage of free space: (-m)             %d%%",
	      sblock.fs_minfree);
	warnx("optimization preference: (-o)                      %s",
	      sblock.fs_optim == FS_OPTSPACE ? "space" : "time");
	if (sblock.fs_minfree >= MINFREE &&
	    sblock.fs_optim == FS_OPTSPACE)
		warnx(OPTWARN, "time", ">=", MINFREE);
	if (sblock.fs_minfree < MINFREE &&
	    sblock.fs_optim == FS_OPTTIME)
		warnx(OPTWARN, "space", "<", MINFREE);
}

void
bwrite(blk, buf, size)
	daddr_t blk;
	const char *buf;
	int size;
{

	if (lseek(fi, (off_t)blk * dev_bsize, SEEK_SET) < 0)
		err(6, "FS SEEK");
	if (write(fi, buf, size) != size)
		err(7, "FS WRITE");
}

int
bread(bno, buf, cnt)
	daddr_t bno;
	char *buf;
	int cnt;
{
	int i;

	if (lseek(fi, (off_t)bno * dev_bsize, SEEK_SET) < 0)
		return(1);
	if ((i = read(fi, buf, cnt)) != cnt) {
		for(i=0; i<sblock.fs_bsize; i++)
			buf[i] = 0;
		return (1);
	}
	return (0);
}
