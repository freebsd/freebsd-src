/*
 * Copyright (c) 1987 Regents of the University of California.
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
"@(#) Copyright (c) 1987 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)split.c	4.8 (Berkeley) 6/1/90";
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <stdio.h>
#include <ctype.h>

#define DEFLINE	1000			/* default num lines per file */
#define ERR	-1			/* general error */
#define NO	0			/* no/false */
#define OK	0			/* okay exit */
#define YES	1			/* yes/true */

static long	bytecnt,		/* byte count to split on */
		numlines;		/* lines in each file */
static int	ifd = ERR,		/* input file descriptor */
		ofd = ERR;		/* output file descriptor */
static short	file_open;		/* if a file open */
static char	bfr[MAXBSIZE],		/* I/O buffer */
		fname[MAXPATHLEN];	/* file name */

main(argc, argv)
	int argc;
	char **argv;
{
	register int cnt;
	long atol();
	char *strcpy();

	for (cnt = 1; cnt < argc; ++cnt) {
		if (argv[cnt][0] == '-')
			switch(argv[cnt][1]) {
			case 0:		/* stdin by request */
				if (ifd != ERR)
					usage();
				ifd = 0;
				break;
			case 'b':	/* byte count split */
				if (numlines || ((argc-1) <= cnt))
					usage();
				if (!argv[cnt][2])
					bytecnt = atol(argv[++cnt]);
				else
					bytecnt = atol(argv[cnt] + 2);
				if (bytecnt <= 0) {
					fputs("split: byte count must be greater than zero.\n", stderr);
					usage();
				}
				break;
			default:
				if (!isdigit(argv[cnt][1]) || bytecnt)
					usage();
				if ((numlines = atol(argv[cnt] + 1)) <= 0) {
					fputs("split: line count must be greater than zero.\n", stderr);
					usage();
				}
				break;
			}
		else if (ifd == ERR) {		/* input file */
			if ((ifd = open(argv[cnt], O_RDONLY, 0)) < 0) {
				perror(argv[cnt]);
				exit(1);
			}
		}
		else if (!*fname)		/* output file prefix */
			strcpy(fname, argv[cnt]);
		else
			usage();
	}
	if (ifd == ERR)				/* stdin by default */
		ifd = 0;
	if (bytecnt)
		split1();
	if (!numlines)
		numlines = DEFLINE;
	split2();
	exit(0);
}

/*
 * split1 --
 *	split by bytes
 */
split1()
{
	register long bcnt;
	register int dist, len;
	register char *C;

	for (bcnt = 0;;)
		switch(len = read(ifd, bfr, MAXBSIZE)) {
		case 0:
			exit(OK);
		case ERR:
			perror("read");
			exit(1);
		default:
			if (!file_open) {
				newfile();
				file_open = YES;
			}
			if (bcnt + len >= bytecnt) {
				dist = bytecnt - bcnt;
				if (write(ofd, bfr, dist) != dist)
					wrerror();
				len -= dist;
				for (C = bfr + dist; len >= bytecnt; len -= bytecnt, C += bytecnt) {
					newfile();
					if (write(ofd, C, (int)bytecnt) != bytecnt)
						wrerror();
				}
				if (len) {
					newfile();
					if (write(ofd, C, len) != len)
						wrerror();
				}
				else
					file_open = NO;
				bcnt = len;
			}
			else {
				bcnt += len;
				if (write(ofd, bfr, len) != len)
					wrerror();
			}
		}
}

/*
 * split2 --
 *	split by lines
 */
split2()
{
	register char *Ce, *Cs;
	register long lcnt;
	register int len, bcnt;

	for (lcnt = 0;;)
		switch(len = read(ifd, bfr, MAXBSIZE)) {
		case 0:
			exit(0);
		case ERR:
			perror("read");
			exit(1);
		default:
			if (!file_open) {
				newfile();
				file_open = YES;
			}
			for (Cs = Ce = bfr; len--; Ce++)
				if (*Ce == '\n' && ++lcnt == numlines) {
					bcnt = Ce - Cs + 1;
					if (write(ofd, Cs, bcnt) != bcnt)
						wrerror();
					lcnt = 0;
					Cs = Ce + 1;
					if (len)
						newfile();
					else
						file_open = NO;
				}
			if (Cs < Ce) {
				bcnt = Ce - Cs;
				if (write(ofd, Cs, bcnt) != bcnt)
					wrerror();
			}
		}
}

/*
 * newfile --
 *	open a new file
 */
newfile()
{
	static long fnum;
	static short defname;
	static char *fpnt;

	if (ofd == ERR) {
		if (fname[0]) {
			fpnt = fname + strlen(fname);
			defname = NO;
		}
		else {
			fname[0] = 'x';
			fpnt = fname + 1;
			defname = YES;
		}
		ofd = fileno(stdout);
	}
	/*
	 * hack to increase max files; original code just wandered through
	 * magic characters.  Maximum files is 3 * 26 * 26 == 2028
	 */
#define MAXFILES	676
	if (fnum == MAXFILES) {
		if (!defname || fname[0] == 'z') {
			fputs("split: too many files.\n", stderr);
			exit(1);
		}
		++fname[0];
		fnum = 0;
	}
	fpnt[0] = fnum / 26 + 'a';
	fpnt[1] = fnum % 26 + 'a';
	++fnum;
	if (!freopen(fname, "w", stdout)) {
		fprintf(stderr, "split: unable to write to %s.\n", fname);
		exit(ERR);
	}
}

/*
 * usage --
 *	print usage message and die
 */
usage()
{
	fputs("usage: split [-] [-#] [-b byte_count] [file [prefix]]\n", stderr);
	exit(1);
}

/*
 * wrerror --
 *	write error
 */
wrerror()
{
	perror("split: write");
	exit(1);
}
