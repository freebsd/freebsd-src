/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Michael Fischbein.
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
"@(#) Copyright (c) 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)ls.c	5.48 (Berkeley) 4/3/91";
static char rcsid[] = "$Header: /a/cvs/386BSD/src/bin/ls/ls.c,v 1.2 1993/06/29 02:59:32 nate Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include "ls.h"

int (*sortfcn)(), (*printfcn)();
int lstat();
char *emalloc();

int termwidth = 80;		/* default terminal width */

/* flags */
int f_accesstime;		/* use time of last access */
int f_column;			/* columnated format */
int f_group;			/* show group ownership of a file */
int f_ignorelink;		/* indirect through symbolic link operands */
int f_inode;			/* print inode */
int f_kblocks;			/* print size in kilobytes */
int f_listalldot;		/* list . and .. as well */
int f_listdir;			/* list actual directory, not contents */
int f_listdot;			/* list files beginning with . */
int f_longform;			/* long listing format */
int f_needstat;			/* if need to stat files */
int f_newline;			/* if precede with newline */
int f_nonprint;			/* show unprintables as ? */
int f_nosort;			/* don't sort output */
int f_recursive;		/* ls subdirectories also */
int f_reversesort;		/* reverse whatever sort is used */
int f_sectime;			/* print the real time for all files */
int f_singlecol;		/* use single column output */
int f_size;			/* list size in short listing */
int f_statustime;		/* use time of last mode change */
int f_dirname;			/* if precede with directory name */
int f_timesort;			/* sort by time vice name */
int f_total;			/* if precede with "total" line */
int f_type;			/* add type character for non-regular files */

int (*statfcn)(), stat(), lstat();

main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind, stat();
	struct winsize win;
	int ch;
	char *p, *getenv();
	int acccmp(), modcmp(), namecmp(), prcopy(), printcol();
	int printlong(), printscol(), revacccmp(), revmodcmp(), revnamecmp();
	int revstatcmp(), statcmp();

	/* terminal defaults to -Cq, non-terminal defaults to -1 */
	if (isatty(1)) {
		f_nonprint = 1;
		if (ioctl(1, TIOCGWINSZ, &win) == -1 || !win.ws_col) {
			if (p = getenv("COLUMNS"))
				termwidth = atoi(p);
		}
		else
			termwidth = win.ws_col;
		f_column = 1;
	} else
		f_singlecol = 1;

	/* root is -A automatically */
	if (!getuid())
		f_listdot = 1;

	while ((ch = getopt(argc, argv, "1ACFLRTacdfgiklqrstu")) != EOF) {
		switch (ch) {
		/*
		 * -1, -C and -l all override each other
		 * so shell aliasing works right
		 */
		case '1':
			f_singlecol = 1;
			f_column = f_longform = 0;
			break;
		case 'C':
			f_column = 1;
			f_longform = f_singlecol = 0;
			break;
		case 'l':
			f_longform = 1;
			f_column = f_singlecol = 0;
			break;
		/* -c and -u override each other */
		case 'c':
			f_statustime = 1;
			f_accesstime = 0;
			break;
		case 'u':
			f_accesstime = 1;
			f_statustime = 0;
			break;
		case 'F':
			f_type = 1;
			break;
		case 'L':
			f_ignorelink = 1;
			break;
		case 'R':
			f_recursive = 1;
			break;
		case 'a':
			f_listalldot = 1;
			/* FALLTHROUGH */
		case 'A':
			f_listdot = 1;
			break;
		case 'd':
			f_listdir = 1;
			break;
		case 'f':
			f_nosort = 1;
			break;
		case 'g':
			f_group = 1;
			break;
		case 'i':
			f_inode = 1;
			break;
		case 'k':
			f_kblocks = 1;
			break;
		case 'q':
			f_nonprint = 1;
			break;
		case 'r':
			f_reversesort = 1;
			break;
		case 's':
			f_size = 1;
			break;
		case 'T':
			f_sectime = 1;
			break;
		case 't':
			f_timesort = 1;
			break;
		default:
		case '?':
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* -d turns off -R */
	if (f_listdir)
		f_recursive = 0;

	/* if need to stat files */
	f_needstat = f_longform || f_recursive || f_timesort ||
	    f_size || f_type;

	/* select a sort function */
	if (f_reversesort) {
		if (!f_timesort)
			sortfcn = revnamecmp;
		else if (f_accesstime)
			sortfcn = revacccmp;
		else if (f_statustime)
			sortfcn = revstatcmp;
		else /* use modification time */
			sortfcn = revmodcmp;
	} else {
		if (!f_timesort)
			sortfcn = namecmp;
		else if (f_accesstime)
			sortfcn = acccmp;
		else if (f_statustime)
			sortfcn = statcmp;
		else /* use modification time */
			sortfcn = modcmp;
	}

	/* select a print function */
	if (f_singlecol)
		printfcn = printscol;
	else if (f_longform)
		printfcn = printlong;
	else
		printfcn = printcol;

	/* if -l, -d, -R, or -F, and not ignoring the link, use lstat() */
	statfcn =
	    (f_longform || f_listdir || f_type || f_recursive) && !f_ignorelink ? lstat : stat;

	if (!argc) {
		static char *nargv[2];
		char dot[2];

		strcpy(dot, ".");
		nargv[0] = dot;
		doargs(1, nargv);
	} else
		doargs(argc, argv);
	exit(0);
}

static char path[MAXPATHLEN + 1];
static char *endofpath = path;

doargs(argc, argv)
	int argc;
	char **argv;
{
	register LS *dstatp, *rstatp;
	register int cnt, dircnt, maxlen, regcnt;
	LS *dstats, *rstats;
	struct stat sb;
	char top[MAXPATHLEN + 1];
	u_long blocks;

	/*
	 * walk through the operands, building separate arrays of LS
	 * structures for directory and non-directory files.
	 */
	dstats = rstats = NULL;
	for (dircnt = regcnt = 0; *argv; ++argv) {
		if (statfcn(*argv, &sb) &&
		    (statfcn == lstat || lstat(*argv, &sb))) {
			(void)fprintf(stderr,
			    "ls: %s: %s\n", *argv, strerror(errno));
			if (errno == ENOENT)
				continue;
			exit(1);
		}
		if (S_ISDIR(sb.st_mode) && !f_listdir) {
			if (!dstats)
				dstatp = dstats = (LS *)emalloc((u_int)argc *
				    (sizeof(LS)));
			dstatp->name = *argv;
			dstatp->lstat = sb;
			++dstatp;
			++dircnt;
		}
		else {
			if (!rstats) {
				rstatp = rstats = (LS *)emalloc((u_int)argc *
				    (sizeof(LS)));
				blocks = 0;
				maxlen = -1;
			}
			rstatp->name = *argv;
			rstatp->lstat = sb;

			/* save name length for -C format */
			rstatp->len = strlen(*argv);

			if (f_nonprint)
				prcopy(*argv, *argv, rstatp->len);

			/* calculate number of blocks if -l/-s formats */
			if (f_longform || f_size)
				blocks += sb.st_blocks;

			/* save max length if -C format */
			if (f_column && maxlen < rstatp->len)
				maxlen = rstatp->len;

			++rstatp;
			++regcnt;
		}
	}
	/* display regular files */
	if (regcnt) {
		rstats[0].lstat.st_btotal = blocks;
		rstats[0].lstat.st_maxlen = maxlen;
		displaydir(rstats, regcnt);
		f_newline = f_dirname = 1;
	}
	/* display directories */
	if (dircnt) {
		register char *p;

		f_total = 1;
		if (dircnt > 1) {
			(void)getwd(top);
			qsort((char *)dstats, dircnt, sizeof(LS), sortfcn);
			f_dirname = 1;
		}
		for (cnt = 0; cnt < dircnt; ++dstats) {
			for (endofpath = path, p = dstats->name;
			    *endofpath = *p++; ++endofpath);
			subdir(dstats);
			f_newline = 1;
			if (++cnt < dircnt && chdir(top)) {
				(void)fprintf(stderr, "ls: %s: %s\n",
				    top, strerror(errno));
				exit(1);
			}
		}
	}
}

displaydir(stats, num)
	LS *stats;
	register int num;
{
	register char *p, *savedpath;
	LS *lp;

	if (num > 1 && !f_nosort) {
		u_long save1, save2;

		save1 = stats[0].lstat.st_btotal;
		save2 = stats[0].lstat.st_maxlen;
		qsort((char *)stats, num, sizeof(LS), sortfcn);
		stats[0].lstat.st_btotal = save1;
		stats[0].lstat.st_maxlen = save2;
	}

	printfcn(stats, num);

	if (f_recursive) {
		savedpath = endofpath;
		for (lp = stats; num--; ++lp) {
			if (!S_ISDIR(lp->lstat.st_mode))
				continue;
			p = lp->name;
			if (p[0] == '.' && (!p[1] || p[1] == '.' && !p[2]))
				continue;
			if (endofpath != path && endofpath[-1] != '/')
				*endofpath++ = '/';
			for (; *endofpath = *p++; ++endofpath);
			f_newline = f_dirname = f_total = 1;
			subdir(lp);
			*(endofpath = savedpath) = '\0';
		}
	}
}

subdir(lp)
	LS *lp;
{
	LS *stats;
	int num;
	char *names;

	if (f_newline)
		(void)putchar('\n');
	if (f_dirname)
		(void)printf("%s:\n", path);

	if (chdir(lp->name)) {
		(void)fprintf(stderr, "ls: %s: %s\n", lp->name,
		     strerror(errno));
		return;
	}
	if (num = tabdir(lp, &stats, &names)) {
		displaydir(stats, num);
		(void)free((char *)stats);
		(void)free((char *)names);
	}
	if (chdir("..")) {
		(void)fprintf(stderr, "ls: ..: %s\n", strerror(errno));
		exit(1);
	}
}

tabdir(lp, s_stats, s_names)
	LS *lp, **s_stats;
	char **s_names;
{
	register DIR *dirp;
	register int cnt, maxentry, maxlen;
	register char *p, *names;
	struct dirent *dp;
	u_long blocks;
	LS *stats;

	if (!(dirp = opendir("."))) {
		(void)fprintf(stderr, "ls: %s: %s\n", lp->name,
		    strerror(errno));
		return(0);
	}
	blocks = maxentry = maxlen = 0;
	stats = NULL;
	for (cnt = 0; dp = readdir(dirp);) {
		/* this does -A and -a */
		p = dp->d_name;
		if (p[0] == '.') {
			if (!f_listdot)
				continue;
			if (!f_listalldot && (!p[1] || p[1] == '.' && !p[2]))
				continue;
		}
		if (cnt == maxentry) {
			if (!maxentry)
				*s_names = names =
				    emalloc((u_int)lp->lstat.st_size);
#define	DEFNUM	256
			maxentry += DEFNUM;
			if (!(*s_stats = stats = (LS *)realloc((char *)stats,
			    (u_int)maxentry * sizeof(LS))))
				nomem();
		}
		if (f_needstat && statfcn(dp->d_name, &stats[cnt].lstat) &&
		    statfcn == stat && lstat(dp->d_name, &stats[cnt].lstat)) {
			/*
			 * don't exit -- this could be an NFS mount that has
			 * gone away.  Flush stdout so the messages line up.
			 */
			(void)fflush(stdout);
			(void)fprintf(stderr,
			    "ls: %s: %s\n", dp->d_name, strerror(errno));
			continue;
		}
		stats[cnt].name = names;

		if (f_nonprint)
			prcopy(dp->d_name, names, (int)dp->d_namlen);
		else
			bcopy(dp->d_name, names, (int)dp->d_namlen);
		names += dp->d_namlen;
		*names++ = '\0';

		/*
		 * get the inode from the directory, so the -f flag
		 * works right.
		 */
		stats[cnt].lstat.st_ino = dp->d_ino;

		/* save name length for -C format */
		stats[cnt].len = dp->d_namlen;

		/* calculate number of blocks if -l/-s formats */
		if (f_longform || f_size)
			blocks += stats[cnt].lstat.st_blocks;

		/* save max length if -C format */
		if (f_column && maxlen < (int)dp->d_namlen)
			maxlen = dp->d_namlen;
		++cnt;
	}
	(void)closedir(dirp);

	if (cnt) {
		stats[0].lstat.st_btotal = blocks;
		stats[0].lstat.st_maxlen = maxlen;
	} else if (stats) {
		(void)free((char *)stats);
		(void)free((char *)names);
	}
	return(cnt);
}
