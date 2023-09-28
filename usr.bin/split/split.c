/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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

#include <sys/cdefs.h>
#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1987, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif

#ifndef lint
static const char sccsid[] = "@(#)split.c	8.2 (Berkeley) 4/16/94";
#endif

#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libutil.h>
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>
#include <sysexits.h>

#define DEFLINE	1000			/* Default num lines per file. */

static off_t	 bytecnt;		/* Byte count to split on. */
static long	 chunks;		/* Chunks count to split into. */
static bool      clobber = true;        /* Whether to overwrite existing output files. */
static long	 numlines;		/* Line count to split on. */
static int	 file_open;		/* If a file open. */
static int	 ifd = -1, ofd = -1;	/* Input/output file descriptors. */
static char	 fname[MAXPATHLEN];	/* File name prefix. */
static regex_t	 rgx;
static int	 pflag;
static bool	 dflag;
static long	 sufflen = 2;		/* File name suffix length. */
static bool	 autosfx = true;	/* Whether to auto-extend the suffix length. */

static void newfile(void);
static void split1(void);
static void split2(void);
static void split3(void);
static void usage(void) __dead2;

int
main(int argc, char **argv)
{
	char errbuf[64];
	const char *p, *errstr;
	int ch, error;

	setlocale(LC_ALL, "");

	dflag = false;
	while ((ch = getopt(argc, argv, "0::1::2::3::4::5::6::7::8::9::a:b:cdl:n:p:")) != -1)
		switch (ch) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			/*
			 * Undocumented kludge: split was originally designed
			 * to take a number after a dash.
			 */
			if (numlines != 0)
				usage();
			numlines = ch - '0';
			p = optarg ? optarg : "";
			while (numlines >= 0 && *p >= '0' && *p <= '9')
				numlines = numlines * 10 + *p++ - '0';
			if (numlines <= 0 || *p != '\0')
				errx(EX_USAGE, "%c%s: line count is invalid",
				    ch, optarg ? optarg : "");
			break;
		case 'a':		/* Suffix length */
			sufflen = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL) {
				errx(EX_USAGE, "%s: suffix length is %s",
				    optarg, errstr);
			}
			if (sufflen == 0) {
				sufflen = 2;
				autosfx = true;
			} else {
				autosfx = false;
			}
			break;
		case 'b':		/* Byte count. */
			if (expand_number(optarg, &bytecnt) != 0) {
				errx(EX_USAGE, "%s: byte count is invalid",
				    optarg);
			}
			break;
		case 'c':               /* Continue, don't overwrite output files. */
			clobber = false;
			break;
		case 'd':		/* Decimal suffix */
			dflag = true;
			break;
		case 'l':		/* Line count. */
			if (numlines != 0)
				usage();
			numlines = strtonum(optarg, 1, LONG_MAX, &errstr);
			if (errstr != NULL) {
				errx(EX_USAGE, "%s: line count is %s",
				    optarg, errstr);
			}
			break;
		case 'n':		/* Chunks. */
			chunks = strtonum(optarg, 1, LONG_MAX, &errstr);
			if (errstr != NULL) {
				errx(EX_USAGE, "%s: number of chunks is %s",
				    optarg, errstr);
			}
			break;

		case 'p':		/* pattern matching. */
			error = regcomp(&rgx, optarg, REG_EXTENDED|REG_NOSUB);
			if (error != 0) {
				regerror(error, &rgx, errbuf, sizeof(errbuf));
				errx(EX_USAGE, "%s: regex is invalid: %s",
				    optarg, errbuf);
			}
			pflag = 1;
			break;
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	if (argc > 0) {			/* Input file. */
		if (strcmp(*argv, "-") == 0)
			ifd = STDIN_FILENO;
		else if ((ifd = open(*argv, O_RDONLY, 0)) < 0)
			err(EX_NOINPUT, "%s", *argv);
		++argv;
		--argc;
	}
	if (argc > 0) {			/* File name prefix. */
		if (strlcpy(fname, *argv, sizeof(fname)) >= sizeof(fname)) {
			errx(EX_USAGE, "%s: file name prefix is too long",
			    *argv);
		}
		++argv;
		--argc;
	}
	if (argc > 0)
		usage();

	if (strlen(fname) + (unsigned long)sufflen >= sizeof(fname))
		errx(EX_USAGE, "suffix is too long");
	if (pflag && (numlines != 0 || bytecnt != 0 || chunks != 0))
		usage();

	if (numlines == 0)
		numlines = DEFLINE;
	else if (bytecnt != 0 || chunks != 0)
		usage();

	if (bytecnt != 0 && chunks != 0)
		usage();

	if (ifd == -1)				/* Stdin by default. */
		ifd = 0;

	if (bytecnt != 0) {
		split1();
		exit (0);
	} else if (chunks != 0) {
		split3();
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
static void
split1(void)
{
	static char bfr[MAXBSIZE];
	off_t bcnt;
	char *C;
	ssize_t dist, len;
	int nfiles;

	nfiles = 0;

	for (bcnt = 0;;)
		switch ((len = read(ifd, bfr, sizeof(bfr)))) {
		case 0:
			exit(0);
		case -1:
			err(EX_IOERR, "read");
			/* NOTREACHED */
		default:
			if (!file_open) {
				if (chunks == 0 || nfiles < chunks) {
					newfile();
					nfiles++;
				}
			}
			if (bcnt + len >= bytecnt) {
				dist = bytecnt - bcnt;
				if (write(ofd, bfr, dist) != dist)
					err(EX_IOERR, "write");
				len -= dist;
				for (C = bfr + dist; len >= bytecnt;
				     len -= bytecnt, C += bytecnt) {
					if (chunks == 0 || nfiles < chunks) {
						newfile();
						nfiles++;
					}
					if (write(ofd, C, bytecnt) != bytecnt)
						err(EX_IOERR, "write");
				}
				if (len != 0) {
					if (chunks == 0 || nfiles < chunks) {
						newfile();
						nfiles++;
					}
					if (write(ofd, C, len) != len)
						err(EX_IOERR, "write");
				} else {
					file_open = 0;
				}
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
static void
split2(void)
{
	char *buf;
	size_t bufsize;
	ssize_t len;
	long lcnt = 0;
	FILE *infp;

	buf = NULL;
	bufsize = 0;

	/* Stick a stream on top of input file descriptor */
	if ((infp = fdopen(ifd, "r")) == NULL)
		err(EX_NOINPUT, "fdopen");

	/* Process input one line at a time */
	while ((errno = 0, len = getline(&buf, &bufsize, infp)) > 0) {
		/* Check if we need to start a new file */
		if (pflag) {
			regmatch_t pmatch;

			pmatch.rm_so = 0;
			pmatch.rm_eo = len - 1;
			if (regexec(&rgx, buf, 0, &pmatch, REG_STARTEND) == 0)
				newfile();
		} else if (lcnt++ == numlines) {
			newfile();
			lcnt = 1;
		}

		/* Open output file if needed */
		if (!file_open)
			newfile();

		/* Write out line */
		if (write(ofd, buf, len) != len)
			err(EX_IOERR, "write");
	}

	/* EOF or error? */
	if ((len == -1 && errno != 0) || ferror(infp))
		err(EX_IOERR, "read");
	else
		exit(0);
}

/*
 * split3 --
 *	Split the input into specified number of chunks
 */
static void
split3(void)
{
	struct stat sb;

	if (fstat(ifd, &sb) == -1) {
		err(1, "stat");
		/* NOTREACHED */
	}

	if (chunks > sb.st_size) {
		errx(1, "can't split into more than %d files",
		    (int)sb.st_size);
		/* NOTREACHED */
	}

	bytecnt = sb.st_size / chunks;
	split1();
}


/*
 * newfile --
 *	Open a new output file.
 */
static void
newfile(void)
{
	long i, maxfiles, tfnum;
	static long fnum;
	static char *fpnt;
	char beg, end;
	int pattlen;
	int flags = O_WRONLY | O_CREAT | O_TRUNC;

	if (!clobber)
		flags |= O_EXCL;

	if (ofd == -1) {
		if (fname[0] == '\0') {
			fname[0] = 'x';
			fpnt = fname + 1;
		} else {
			fpnt = fname + strlen(fname);
		}
	} else if (close(ofd) != 0)
		err(1, "%s", fname);

	again:
	if (dflag) {
		beg = '0';
		end = '9';
	}
	else {
		beg = 'a';
		end = 'z';
	}
	pattlen = end - beg + 1;

	/*
	 * If '-a' is not specified, then we automatically expand the
	 * suffix length to accomodate splitting all input.  We do this
	 * by moving the suffix pointer (fpnt) forward and incrementing
	 * sufflen by one, thereby yielding an additional two characters
	 * and allowing all output files to sort such that 'cat *' yields
	 * the input in order.  I.e., the order is '... xyy xyz xzaaa
	 * xzaab ... xzyzy, xzyzz, xzzaaaa, xzzaaab' and so on.
	 */
	if (!dflag && autosfx && (fpnt[0] == 'y') &&
			strspn(fpnt+1, "z") == strlen(fpnt+1)) {
		fpnt = fname + strlen(fname) - sufflen;
		fpnt[sufflen + 2] = '\0';
		fpnt[0] = end;
		fpnt[1] = beg;

		/*  Basename | Suffix
		 *  before:
		 *  x        | yz
		 *  after:
		 *  xz       | a.. */
		fpnt++;
		sufflen++;

		/* Reset so we start back at all 'a's in our extended suffix. */
		fnum = 0;
	}

	/* maxfiles = pattlen^sufflen, but don't use libm. */
	for (maxfiles = 1, i = 0; i < sufflen; i++)
		if (LONG_MAX / pattlen < maxfiles)
			errx(EX_USAGE, "suffix is too long (max %ld)", i);
		else
			maxfiles *= pattlen;

	if (fnum == maxfiles)
		errx(EX_DATAERR, "too many files");

	/* Generate suffix of sufflen letters */
	tfnum = fnum;
	i = sufflen - 1;
	do {
		fpnt[i] = tfnum % pattlen + beg;
		tfnum /= pattlen;
	} while (i-- > 0);
	fpnt[sufflen] = '\0';

	++fnum;
	if ((ofd = open(fname, flags, DEFFILEMODE)) < 0) {
		if (!clobber && errno == EEXIST)
			goto again;
		err(EX_IOERR, "%s", fname);
	}
	file_open = 1;
}

static void
usage(void)
{
	(void)fprintf(stderr,
"usage: split [-cd] [-l line_count] [-a suffix_length] [file [prefix]]\n"
"       split [-cd] -b byte_count[K|k|M|m|G|g] [-a suffix_length] [file [prefix]]\n"
"       split [-cd] -n chunk_count [-a suffix_length] [file [prefix]]\n"
"       split [-cd] -p pattern [-a suffix_length] [file [prefix]]\n");
	exit(EX_USAGE);
}
