/*
 * Copyright (c) 1989 The Regents of the University of California.
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
"@(#) Copyright (c) 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)column.c	5.7 (Berkeley) 2/24/91";
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

int termwidth = 80;		/* default terminal width */

int entries;			/* number of records */
int eval;			/* exit value */
int maxlength;			/* longest record */
char **list;			/* array of pointers to records */
char *separator = "\t ";	/* field separator for table option */

main(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
	extern int errno, optind;
	struct winsize win;
	FILE *fp;
	int ch, tflag, xflag;
	char *p, *getenv();

	if (ioctl(1, TIOCGWINSZ, &win) == -1 || !win.ws_col) {
		if (p = getenv("COLUMNS"))
			termwidth = atoi(p);
	} else
		termwidth = win.ws_col;

	xflag = 0;
	while ((ch = getopt(argc, argv, "c:s:tx")) != EOF)
		switch(ch) {
		case 'c':
			termwidth = atoi(optarg);
			break;
		case 's':
			separator = optarg;
			break;
		case 't':
			tflag = 1;
			break;
		case 'x':
			xflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!*argv)
		input(stdin);
	else for (; *argv; ++argv)
		if (fp = fopen(*argv, "r")) {
			input(fp);
			(void)fclose(fp);
		} else {
			(void)fprintf(stderr, "column: %s: %s\n", *argv,
			    strerror(errno));
			eval = 1;
		}

	if (!entries)
		exit(eval);

	if (tflag)
		maketbl();
	else if (maxlength >= termwidth)
		print();
	else if (xflag)
		c_columnate();
	else
		r_columnate();
	exit(eval);
}

#define	TAB	8
c_columnate()
{
	register int chcnt, col, cnt, numcols;
	int endcol;
	char **lp;

	maxlength = (maxlength + TAB) & ~(TAB - 1);
	numcols = termwidth / maxlength;
	endcol = maxlength;
	for (chcnt = col = 0, lp = list;; ++lp) {
		chcnt += printf("%s", *lp);
		if (!--entries)
			break;
		if (++col == numcols) {
			chcnt = col = 0;
			endcol = maxlength;
			putchar('\n');
		} else {
			while ((cnt = (chcnt + TAB & ~(TAB - 1))) <= endcol) {
				(void)putchar('\t');
				chcnt = cnt;
			}
			endcol += maxlength;
		}
	}
	if (chcnt)
		putchar('\n');
}

r_columnate()
{
	register int base, chcnt, cnt, col;
	int endcol, numcols, numrows, row;

	maxlength = (maxlength + TAB) & ~(TAB - 1);
	numcols = termwidth / maxlength;
	numrows = entries / numcols;
	if (entries % numcols)
		++numrows;

	for (row = 0; row < numrows; ++row) {
		endcol = maxlength;
		for (base = row, chcnt = col = 0; col < numcols; ++col) {
			chcnt += printf("%s", list[base]);
			if ((base += numrows) >= entries)
				break;
			while ((cnt = (chcnt + TAB & ~(TAB - 1))) <= endcol) {
				(void)putchar('\t');
				chcnt = cnt;
			}
			endcol += maxlength;
		}
		putchar('\n');
	}
}

print()
{
	register int cnt;
	register char **lp;

	for (cnt = entries, lp = list; cnt--; ++lp)
		(void)printf("%s\n", *lp);
}

typedef struct _tbl {
	char **list;
	int cols, *len;
} TBL;
#define	DEFCOLS	25

maketbl()
{
	register TBL *t;
	register int coloff, cnt;
	register char *p, **lp;
	int *lens, maxcols;
	TBL *tbl;
	char **cols, *emalloc(), *realloc();

	t = tbl = (TBL *)emalloc(entries * sizeof(TBL));
	cols = (char **)emalloc((maxcols = DEFCOLS) * sizeof(char *));
	lens = (int *)emalloc(maxcols * sizeof(int));
	for (cnt = 0, lp = list; cnt < entries; ++cnt, ++lp, ++t) {
		for (coloff = 0, p = *lp; cols[coloff] = strtok(p, separator);
		    p = NULL)
			if (++coloff == maxcols) {
				if (!(cols = (char **)realloc((char *)cols,
				    (u_int)maxcols + DEFCOLS * sizeof(char *))) ||
				    !(lens = (int *)realloc((char *)lens,
				    (u_int)maxcols + DEFCOLS * sizeof(int))))
					nomem();
				bzero((char *)lens + maxcols * sizeof(int),
				    DEFCOLS * sizeof(int));
				maxcols += DEFCOLS;
			}
		t->list = (char **)emalloc(coloff * sizeof(char *));
		t->len = (int *)emalloc(coloff * sizeof(int));
		for (t->cols = coloff; --coloff >= 0;) {
			t->list[coloff] = cols[coloff];
			t->len[coloff] = strlen(cols[coloff]);
			if (t->len[coloff] > lens[coloff])
				lens[coloff] = t->len[coloff];
		}
	}
	for (cnt = 0, t = tbl; cnt < entries; ++cnt, ++t) {
		for (coloff = 0; coloff < t->cols  - 1; ++coloff)
			(void)printf("%s%*s", t->list[coloff],
			    lens[coloff] - t->len[coloff] + 2, " ");
		(void)printf("%s\n", t->list[coloff]);
	}
}

#define	DEFNUM		1000
#define	MAXLINELEN	(2048 + 1)

input(fp)
	register FILE *fp;
{
	static int maxentry;
	register int len;
	register char *p;
	char buf[MAXLINELEN], *emalloc(), *realloc();

	if (!list)
		list = (char **)emalloc((maxentry = DEFNUM) * sizeof(char *));
	while (fgets(buf, MAXLINELEN, fp)) {
		for (p = buf; *p && isspace(*p); ++p);
		if (!*p)
			continue;
		if (!(p = index(p, '\n'))) {
			(void)fprintf(stderr, "column: line too long.\n");
			eval = 1;
			continue;
		}
		*p = '\0';
		len = p - buf;
		if (maxlength < len)
			maxlength = len;
		if (entries == maxentry) {
			maxentry += DEFNUM;
			if (!(list =
			    (char **)realloc((char *)list,
			    (u_int)maxentry * sizeof(char *))))
				nomem();
		}
		list[entries++] = strdup(buf);
	}
}

char *
emalloc(size)
	int size;
{
	char *p, *malloc();

	/* NOSTRICT */
	if (!(p = malloc((u_int)size)))
		nomem();
	bzero(p, size);
	return(p);
}

nomem()
{
	(void)fprintf(stderr, "column: out of memory.\n");
	exit(1);
}

usage()
{
	(void)fprintf(stderr,
	    "usage: column [-tx] [-c columns] [file ...]\n");
	exit(1);
}
