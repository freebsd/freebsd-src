/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1993
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

/*
 *	rs - reshape a data array
 *	Author:  John Kunze, Office of Comp. Affairs, UCB
 *		BEWARE: lots of unfinished edges
 */

#include <err.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vector>

static long	flags;
#define	TRANSPOSE	000001
#define	MTRANSPOSE	000002
#define	ONEPERLINE	000004
#define	ONEISEPONLY	000010
#define	ONEOSEPONLY	000020
#define	NOTRIMENDCOL	000040
#define	SQUEEZE		000100
#define	SHAPEONLY	000200
#define	DETAILSHAPE	000400
#define	RIGHTADJUST	001000
#define	NULLPAD		002000
#define	RECYCLE		004000
#define	SKIPPRINT	010000
#define	ICOLBOUNDS	020000
#define	OCOLBOUNDS	040000
#define ONEPERCHAR	0100000
#define NOARGS		0200000

static short	*colwidths;
static std::vector<char *> elem;
static char	*curline;
static size_t	curlen;
static size_t	irows, icols;
static size_t	orows = 0, ocols = 0;
static size_t	maxlen;
static int	skip;
static int	propgutter;
static char	isep = ' ', osep = ' ';
static char	blank[] = "";
static size_t	owidth = 80, gutter = 2;

static void	  getargs(int, char *[]);
static void	  getfile(void);
static int	  get_line(void);
static long	  getnum(const char *);
static void	  prepfile(void);
static void	  prints(char *, int);
static void	  putfile(void);
static void usage(void);

int
main(int argc, char *argv[])
{
	getargs(argc, argv);
	getfile();
	if (flags & SHAPEONLY) {
		printf("%zu %zu\n", irows, icols);
		exit(0);
	}
	prepfile();
	putfile();
	exit(0);
}

static void
getfile(void)
{
	char *p, *sp;
	char *endp;
	int c;
	int multisep = (flags & ONEISEPONLY ? 0 : 1);
	int nullpad = flags & NULLPAD;
	size_t len, padto;

	while (skip--) {
		c = get_line();
		if (flags & SKIPPRINT)
			puts(curline);
		if (c == EOF)
			return;
	}
	get_line();
	if (flags & NOARGS && curlen < owidth)
		flags |= ONEPERLINE;
	if (flags & ONEPERLINE)
		icols = 1;
	else				/* count cols on first line */
		for (p = curline, endp = curline + curlen; p < endp; p++) {
			if (*p == isep && multisep)
				continue;
			icols++;
			while (*p && *p != isep)
				p++;
		}
	do {
		if (flags & ONEPERLINE) {
			elem.push_back(curline);
			if (maxlen < curlen)
				maxlen = curlen;
			irows++;
			continue;
		}
		for (p = curline, endp = curline + curlen; p < endp; p++) {
			if (*p == isep && multisep)
				continue;	/* eat up column separators */
			if (*p == isep)		/* must be an empty column */
				elem.push_back(blank);
			else			/* store column entry */
				elem.push_back(p);
			sp = p;
			while (p < endp && *p != isep)
				p++;		/* find end of entry */
			*p = '\0';		/* mark end of entry */
			len = p - sp;
			if (maxlen < len)	/* update maxlen */
				maxlen = len;
		}
		irows++;			/* update row count */
		if (nullpad) {			/* pad missing entries */
			padto = irows * icols;
			elem.resize(padto, blank);
		}
	} while (get_line() != EOF);
}

static void
putfile(void)
{
	size_t i, j, k;

	if (flags & TRANSPOSE)
		for (i = 0; i < orows; i++) {
			for (j = i; j < elem.size(); j += orows)
				prints(elem[j], (j - i) / orows);
			putchar('\n');
		}
	else
		for (i = k = 0; i < orows; i++) {
			for (j = 0; j < ocols; j++, k++)
				if (k < elem.size())
					prints(elem[k], j);
			putchar('\n');
		}
}

static void
prints(char *s, int col)
{
	int n;
	char *p = s;

	while (*p)
		p++;
	n = (flags & ONEOSEPONLY ? 1 : colwidths[col] - (p - s));
	if (flags & RIGHTADJUST)
		while (n-- > 0)
			putchar(osep);
	for (p = s; *p; p++)
		putchar(*p);
	while (n-- > 0)
		putchar(osep);
}

static void
usage(void)
{
	fprintf(stderr,
		"usage: rs [-[csCS][x][kKgGw][N]tTeEnyjhHmz] [rows [cols]]\n");
	exit(1);
}

static void
prepfile(void)
{
	size_t i, j;
	size_t colw, max, n, orig_size, padto;

	if (elem.empty())
		exit(0);
	gutter += maxlen * propgutter / 100.0;
	colw = maxlen + gutter;
	if (flags & MTRANSPOSE) {
		orows = icols;
		ocols = irows;
	}
	else if (orows == 0 && ocols == 0) {	/* decide rows and cols */
		ocols = owidth / colw;
		if (ocols == 0) {
			warnx("display width %zu is less than column width %zu",
					owidth, colw);
			ocols = 1;
		}
		if (ocols > elem.size())
			ocols = elem.size();
		orows = elem.size() / ocols + (elem.size() % ocols ? 1 : 0);
	}
	else if (orows == 0)			/* decide on rows */
		orows = elem.size() / ocols + (elem.size() % ocols ? 1 : 0);
	else if (ocols == 0)			/* decide on cols */
		ocols = elem.size() / orows + (elem.size() % orows ? 1 : 0);
	padto = orows * ocols;
	orig_size = elem.size();
	if (flags & RECYCLE) {
		for (i = 0; elem.size() < padto; i++)
			elem.push_back(elem[i % orig_size]);
	}
	if (!(colwidths = (short *) malloc(ocols * sizeof(short))))
		errx(1, "malloc");
	if (flags & SQUEEZE) {
		if (flags & TRANSPOSE) {
			auto it = elem.begin();
			for (i = 0; i < ocols; i++) {
				max = 0;
				for (j = 0; it != elem.end() && j < orows; j++)
					if ((n = strlen(*it++)) > max)
						max = n;
				colwidths[i] = max + gutter;
			}
		} else {
			for (i = 0; i < ocols; i++) {
				max = 0;
				for (j = i; j < elem.size(); j += ocols)
					if ((n = strlen(elem[j])) > max)
						max = n;
				colwidths[i] = max + gutter;
			}
		}
	}
	/*	for (i = 0; i < orows; i++) {
			for (j = i; j < elem.size(); j += orows)
				prints(elem[j], (j - i) / orows);
			putchar('\n');
		}
	else {
		auto it = elem.begin();
		for (i = 0; i < orows; i++) {
			for (j = 0; j < ocols; j++)
				prints(*it++, j);
			putchar('\n');
		}*/
	else
		for (i = 0; i < ocols; i++)
			colwidths[i] = colw;
	if (!(flags & NOTRIMENDCOL)) {
		if (flags & RIGHTADJUST)
			colwidths[0] -= gutter;
		else
			colwidths[ocols - 1] = 0;
	}
	/*for (i = 0; i < ocols; i++)
		warnx("%d is colwidths, nelem %zu", colwidths[i], elem.size());*/
}

#define	BSIZE	(LINE_MAX * 2)
static char	ibuf[BSIZE];

static int
get_line(void)	/* get line; maintain curline, curlen; manage storage */
{
	static	int putlength;
	static	char *endblock = ibuf + BSIZE;
	char *p;
	int c, i;

	if (irows == 0) {
		curline = ibuf;
		putlength = flags & DETAILSHAPE;
	}
	else if (skip <= 0) {			/* don't waste storage */
		curline += curlen + 1;
		if (putlength) {	/* print length, recycle storage */
			printf(" %zu line %zu\n", curlen, irows);
			curline = ibuf;
		}
	}
	if (!putlength && endblock - curline < LINE_MAX + 1) { /* need storage */
		/*ww = endblock-curline; tt += ww;*/
		/*printf("#wasted %d total %d\n",ww,tt);*/
		if (!(curline = (char *) malloc(BSIZE)))
			errx(1, "file too large");
		endblock = curline + BSIZE;
		/*printf("#endb %d curline %d\n",endblock,curline);*/
	}
	for (p = curline, i = 0;; *p++ = c, i++) {
		if ((c = getchar()) == EOF)
			break;
		if (i >= LINE_MAX)
			errx(1, "maximum line length (%d) exceeded", LINE_MAX);
		if (c == '\n')
			break;
	}
	*p = '\0';
	curlen = i;
	return(c);
}

static void
getargs(int ac, char *av[])
{
	long val;
	int ch;

	if (ac == 1) {
		flags |= NOARGS | TRANSPOSE;
	}

	while ((ch = getopt(ac, av, "C::EG:HK:S::Tc::eg:hjk:mns::tw:yz")) != -1)
		switch (ch) {
		case 'T':
			flags |= MTRANSPOSE;
			/* FALLTHROUGH */
		case 't':
			flags |= TRANSPOSE;
			break;
		case 'c':		/* input col. separator */
			flags |= ONEISEPONLY;
			/* FALLTHROUGH */
		case 's':		/* one or more allowed */
			if (optarg != NULL)
				isep = *optarg;
			else
				isep = '\t';	/* default is ^I */
			break;
		case 'C':
			flags |= ONEOSEPONLY;
			/* FALLTHROUGH */
		case 'S':
			if (optarg != NULL)
				osep = *optarg;
			else
				osep = '\t';	/* default is ^I */
			break;
		case 'w':		/* window width, default 80 */
			val = getnum(optarg);
			if (val <= 0)
				errx(1, "width must be a positive integer");
			owidth = val;
			break;
		case 'K':			/* skip N lines */
			flags |= SKIPPRINT;
			/* FALLTHROUGH */
		case 'k':			/* skip, do not print */
			skip = getnum(optarg);
			if (skip < 1)
				skip = 1;
			break;
		case 'm':
			flags |= NOTRIMENDCOL;
			break;
		case 'g':		/* gutter space */
			gutter = getnum(optarg);
			break;
		case 'G':
			propgutter = getnum(optarg);
			break;
		case 'e':		/* each line is an entry */
			flags |= ONEPERLINE;
			break;
		case 'E':
			flags |= ONEPERCHAR;
			break;
		case 'j':			/* right adjust */
			flags |= RIGHTADJUST;
			break;
		case 'n':	/* null padding for missing values */
			flags |= NULLPAD;
			break;
		case 'y':
			flags |= RECYCLE;
			break;
		case 'H':			/* print shape only */
			flags |= DETAILSHAPE;
			/* FALLTHROUGH */
		case 'h':
			flags |= SHAPEONLY;
			break;
		case 'z':			/* squeeze col width */
			flags |= SQUEEZE;
			break;
		/*case 'p':
			ipagespace = atoi(optarg);	(default is 1)
			break;*/
		default:
			usage();
		}

	av += optind;
	ac -= optind;

	/*if (!osep)
		osep = isep;*/
	switch (ac) {
#if 0
	case 3:
		opages = atoi(av[2]);
		/* FALLTHROUGH */
#endif
	case 2:
		val = strtol(av[1], NULL, 10);
		if (val >= 0)
			ocols = val;
		/* FALLTHROUGH */
	case 1:
		val = strtol(av[0], NULL, 10);
		if (val >= 0)
			orows = val;
		/* FALLTHROUGH */
	case 0:
		break;
	default:
		errx(1, "too many arguments");
	}
}

static long
getnum(const char *p)
{
	char *ep;
	long val;

	val = strtol(p, &ep, 10);
	if (*ep != '\0')
		errx(1, "invalid integer %s", p);
	return (val);
}
