/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Newcomb.
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
static char copyright[] =
"@(#) Copyright (c) 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)du.c	8.5 (Berkeley) 5/4/95";
#endif /* not lint */


#include <sys/param.h>
#include <sys/stat.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int		linkchk __P((FTSENT *));
static void	usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	FTS		*fts;
	FTSENT		*p;
	FTSENT		*savedp;
	long		blocksize;
	int		ftsoptions;
	int		listall;
	int		depth;
	int		Hflag, Lflag, Pflag, aflag, sflag, dflag, cflag, ch, notused, rval;
	char 		**save;

	Hflag = Lflag = Pflag = aflag = sflag = dflag = cflag = 0;
	
	save = argv;
	ftsoptions = 0;
	depth = INT_MAX;
	
	while ((ch = getopt(argc, argv, "HLPad:ksxc")) != -1)
		switch (ch) {
			case 'H':
				Hflag = 1;
				break;
			case 'L':
				if (Pflag)
					usage();
				Lflag = 1;
				break;
			case 'P':
				if (Lflag)
					usage();
				Pflag = 1;
				break;
			case 'a':
				aflag = 1;
				break;
			case 'k':
				putenv("BLOCKSIZE=1024");
				break;
			case 's':
				sflag = 1;
				break;
			case 'x':
				ftsoptions |= FTS_XDEV;
				break;
			case 'd':
				dflag = 1;
				errno = 0;
				depth = atoi(optarg);
				if (errno == ERANGE || depth < 0) {
					(void) fprintf(stderr, "Invalid argument to option d: %s", optarg);
					usage();
				}
				break;
			case 'c':
				cflag = 1;
				break;
			case '?':
			case 'h':
			default:
				usage();
		}

	argc -= optind;
	argv += optind;

	/*
	 * XXX
	 * Because of the way that fts(3) works, logical walks will not count
	 * the blocks actually used by symbolic links.  We rationalize this by
	 * noting that users computing logical sizes are likely to do logical
	 * copies, so not counting the links is correct.  The real reason is
	 * that we'd have to re-implement the kernel's symbolic link traversing
	 * algorithm to get this right.  If, for example, you have relative
	 * symbolic links referencing other relative symbolic links, it gets
	 * very nasty, very fast.  The bottom line is that it's documented in
	 * the man page, so it's a feature.
	 */

	if (Hflag + Lflag + Pflag > 1)
		usage();

	if (Hflag + Lflag + Pflag == 0)
		Pflag = 1;			/* -P (physical) is default */

	if (Hflag)
		ftsoptions |= FTS_COMFOLLOW;

	if (Lflag)
		ftsoptions |= FTS_LOGICAL;

	if (Pflag)
		ftsoptions |= FTS_PHYSICAL;

	listall = 0;

	if (aflag) {
		if (sflag || dflag)
			usage();
		listall = 1;
	} else if (sflag) {
		if (dflag)
			usage();
		depth = 0;
	}

	if (!*argv) {
		argv = save;
		argv[0] = ".";
		argv[1] = NULL;
	}

	(void) getbsize(&notused, &blocksize);
	blocksize /= 512;

	rval = 0;
	
	if ((fts = fts_open(argv, ftsoptions, NULL)) == NULL)
		err(1, "fts_open");

	while ((p = fts_read(fts)) != NULL) {
		savedp = p;
		switch (p->fts_info) {
			case FTS_D:			/* Ignore. */
				break;
			case FTS_DP:
				p->fts_parent->fts_number +=
				    p->fts_number += p->fts_statp->st_blocks;
				
				if (p->fts_level <= depth)
					(void) printf("%ld\t%s\n",
					    howmany(p->fts_number, blocksize),
					    p->fts_path);
				break;
			case FTS_DC:			/* Ignore. */
				break;
			case FTS_DNR:			/* Warn, continue. */
			case FTS_ERR:
			case FTS_NS:
				warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
				rval = 1;
				break;
			default:
				if (p->fts_statp->st_nlink > 1 && linkchk(p))
					break;
				
				if (listall || p->fts_level == 0)
					(void) printf("%qd\t%s\n",
					    howmany(p->fts_statp->st_blocks, blocksize),
					    p->fts_path);

				p->fts_parent->fts_number += p->fts_statp->st_blocks;
		}
	}

	if (errno)
		err(1, "fts_read");

	if (cflag) {
		p = savedp->fts_parent;
		(void) printf("%ld\ttotal\n", howmany(p->fts_number, blocksize));
	}

	exit(rval);
}


typedef struct _ID {
	dev_t	dev;
	ino_t	inode;
} ID;


int
linkchk(p)
	FTSENT *p;
{
	static ID *files;
	static int maxfiles, nfiles;
	ID *fp, *start;
	ino_t ino;
	dev_t dev;

	ino = p->fts_statp->st_ino;
	dev = p->fts_statp->st_dev;
	if ((start = files) != NULL)
		for (fp = start + nfiles - 1; fp >= start; --fp)
			if (ino == fp->inode && dev == fp->dev)
				return (1);

	if (nfiles == maxfiles && (files = realloc((char *)files,
	    (u_int)(sizeof(ID) * (maxfiles += 128)))) == NULL)
		err(1, "can't allocate memory");
	files[nfiles].inode = ino;
	files[nfiles].dev = dev;
	++nfiles;
	return (0);
}

static void
usage()
{
	(void)fprintf(stderr,
		"usage: du [-H | -L | -P] [-a | -s | -d depth] [-k] [-x] [file ...]\n");
	exit(1);
}
