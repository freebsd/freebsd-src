/*
 * Copyright (c) 1987, 1993, 1994
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
"@(#) Copyright (c) 1987, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)split.c	8.2 (Berkeley) 4/16/94";
#else
static const char rcsid[] =
  "$FreeBSD$";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>
#include <sysexits.h>

#define DEFLINE	1000			/* Default num lines per file. */

size_t	 bytecnt;			/* Byte count to split on. */
long	 numlines;			/* Line count to split on. */
int	 file_open;			/* If a file open. */
int	 ifd = -1, ofd = -1;		/* Input/output file descriptors. */
char	 bfr[MAXBSIZE];			/* I/O buffer. */
char	 fname[MAXPATHLEN];		/* File name prefix. */
regex_t	 rgx;
int	 pflag;

void newfile __P((void));
void split1 __P((void));
void split2 __P((void));
static void usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch;
	char *ep, *p;

	while ((ch = getopt(argc, argv, "-0123456789b:l:p:")) != -1)
		switch (ch) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			/*
			 * Undocumented kludge: split was originally designed
			 * to take a number after a dash.
			 */
			if (numlines == 0) {
				p = argv[optind - 1];
				if (p[0] == '-' && p[1] == ch && !p[2])
					numlines = strtol(++p, &ep, 10);
				else
					numlines =
					    strtol(argv[optind] + 1, &ep, 10);
				if (numlines <= 0 || *ep)
					errx(EX_USAGE,
					    "%s: illegal line count", optarg);
			}
			break;
		case '-':		/* Undocumented: historic stdin flag. */
			if (ifd != -1)
				usage();
			ifd = 0;
			break;
		case 'b':		/* Byte count. */
			if ((bytecnt = strtoq(optarg, &ep, 10)) <= 0 ||
			    (*ep != '\0' && *ep != 'k' && *ep != 'm'))
				errx(EX_USAGE,
				    "%s: illegal byte count", optarg);
			if (*ep == 'k')
				bytecnt *= 1024;
			else if (*ep == 'm')
				bytecnt *= 1048576;
			break;
		case 'p' :      /* pattern matching. */
			if (regcomp(&rgx, optarg, REG_EXTENDED|REG_NOSUB) != 0)
				errx(EX_USAGE, "%s: illegal regexp", optarg);
			pflag = 1;
			break;
		case 'l':		/* Line count. */
			if (numlines != 0)
				usage();
			if ((numlines = strtol(optarg, &ep, 10)) <= 0 || *ep)
				errx(EX_USAGE,
				    "%s: illegal line count", optarg);
			break;
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	if (*argv != NULL)
		if (ifd == -1) {		/* Input file. */
			if ((ifd = open(*argv, O_RDONLY, 0)) < 0)
				err(EX_NOINPUT, "%s", *argv);
			++argv;
		}
	if (*argv != NULL)			/* File name prefix. */
		(void)strcpy(fname, *argv++);
	if (*argv != NULL)
		usage();

	if (pflag && (numlines != 0 || bytecnt != 0))
		usage();

	if (numlines == 0)
		numlines = DEFLINE;
	else if (bytecnt != 0)
		usage();

	if (ifd == -1)				/* Stdin by default. */
		ifd = 0;

	if (bytecnt) {
		split1();
		exit (0);
	}
	split2();
	if (pflag)
		regfree(&rgx);
	exit(0);
}

/*
 * split1 --
 *	Split the input by bytes.
 */
void
split1()
{
	size_t bcnt, dist, len;
	char *C;

	for (bcnt = 0;;)
		switch ((len = read(ifd, bfr, MAXBSIZE))) {
		case 0:
			exit(0);
		case -1:
			err(EX_IOERR, "read");
			/* NOTREACHED */
		default:
			if (!file_open)
				newfile();
			if (bcnt + len >= bytecnt) {
				dist = bytecnt - bcnt;
				if (write(ofd, bfr, dist) != dist)
					err(EX_IOERR, "write");
				len -= dist;
				for (C = bfr + dist; len >= bytecnt;
				    len -= bytecnt, C += bytecnt) {
					newfile();
					if (write(ofd,
					    C, (int)bytecnt) != bytecnt)
						err(EX_IOERR, "write");
				}
				if (len != 0) {
					newfile();
					if (write(ofd, C, len) != len)
						err(EX_IOERR, "write");
				} else
					file_open = 0;
				bcnt = len;
			} else {
				bcnt += len;
				if (write(ofd, bfr, len) != len)
					err(EX_IOERR, "write");
			}
		}
}

/*
 * split2 --
 *	Split the input by lines.
 */
void
split2()
{
	long lcnt = 0;
	FILE *infp;

	/* Stick a stream on top of input file descriptor */
	if ((infp = fdopen(ifd, "r")) == NULL)
		err(EX_NOINPUT, "fdopen");

	/* Process input one line at a time */
	while (fgets(bfr, sizeof(bfr), infp) != NULL) {
		const int len = strlen(bfr);

		/* If line is too long to deal with, just write it out */
		if (bfr[len - 1] != '\n')
			goto writeit;

		/* Check if we need to start a new file */
		if (pflag) {
			regmatch_t pmatch;

			pmatch.rm_so = 0;
			pmatch.rm_eo = len - 1;
			if (regexec(&rgx, bfr, 0, &pmatch, REG_STARTEND) == 0)
				newfile();
		} else if (lcnt++ == numlines) {
			newfile();
			lcnt = 1;
		}

writeit:
		/* Open output file if needed */
		if (!file_open)
			newfile();

		/* Write out line */
		if (write(ofd, bfr, len) != len)
			err(EX_IOERR, "write");
	}

	/* EOF or error? */
	if (ferror(infp))
		err(EX_IOERR, "read");
	else
		exit(0);
}

/*
 * newfile --
 *	Open a new output file.
 */
void
newfile()
{
	static long fnum;
	static int defname;
	static char *fpnt;

	if (ofd == -1) {
		if (fname[0] == '\0') {
			fname[0] = 'x';
			fpnt = fname + 1;
			defname = 1;
		} else {
			fpnt = fname + strlen(fname);
			defname = 0;
		}
		ofd = fileno(stdout);
	}
	/*
	 * Hack to increase max files; original code wandered through
	 * magic characters.  Maximum files is 3 * 26 * 26 == 2028
	 */
#define MAXFILES	676
	if (fnum == MAXFILES) {
		if (!defname || fname[0] == 'z')
			errx(EX_DATAERR, "too many files");
		++fname[0];
		fnum = 0;
	}
	fpnt[0] = fnum / 26 + 'a';
	fpnt[1] = fnum % 26 + 'a';
	++fnum;
	if (!freopen(fname, "w", stdout))
		err(EX_IOERR, "%s", fname);
	file_open = 1;
}

static void
usage()
{
	(void)fprintf(stderr,
"usage: split [-b byte_count] [-l line_count] [-p pattern] [file [prefix]]\n");
	exit(EX_USAGE);
}
