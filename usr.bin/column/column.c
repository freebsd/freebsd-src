/*
 * Copyright (c) 1989, 1993, 1994
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
 *
 * $FreeBSD$
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static const char sccsid[] = "@(#)column.c	8.4 (Berkeley) 5/4/95";
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>

#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	TAB	8

void  c_columnate __P((void));
void  input __P((FILE *));
void  maketbl __P((void));
void  print __P((void));
void  r_columnate __P((void));
void  usage __P((void));

int termwidth = 80;		/* default terminal width */

int entries;			/* number of records */
int eval;			/* exit value */
int maxlength;			/* longest record */
char **list;			/* array of pointers to records */
char *separator = "\t ";	/* field separator for table option */

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct winsize win;
	FILE *fp;
	int ch, tflag, xflag;
	char *p;

	if (ioctl(1, TIOCGWINSZ, &win) == -1 || !win.ws_col) {
		if ((p = getenv("COLUMNS")))
			termwidth = atoi(p);
	} else
		termwidth = win.ws_col;

	tflag = xflag = 0;
	while ((ch = getopt(argc, argv, "c:s:tx")) != -1)
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
		if ((fp = fopen(*argv, "r"))) {
			input(fp);
			(void)fclose(fp);
		} else {
			warn("%s", *argv);
			eval = 1;
		}

	if (!entries)
		exit(eval);

	maxlength = (maxlength + TAB) & ~(TAB - 1);
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

void
c_columnate()
{
	int chcnt, col, cnt, endcol, numcols;
	char **lp;

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
			while ((cnt = ((chcnt + TAB) & ~(TAB - 1))) <= endcol) {
				(void)putchar('\t');
				chcnt = cnt;
			}
			endcol += maxlength;
		}
	}
	if (chcnt)
		putchar('\n');
}

void
r_columnate()
{
	int base, chcnt, cnt, col, endcol, numcols, numrows, row;

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
			while ((cnt = ((chcnt + TAB) & ~(TAB - 1))) <= endcol) {
				(void)putchar('\t');
				chcnt = cnt;
			}
			endcol += maxlength;
		}
		putchar('\n');
	}
}

void
print()
{
	int cnt;
	char **lp;

	for (cnt = entries, lp = list; cnt--; ++lp)
		(void)printf("%s\n", *lp);
}

typedef struct _tbl {
	char **list;
	int cols, *len;
} TBL;
#define	DEFCOLS	25

void
maketbl()
{
	TBL *t;
	int coloff, cnt;
	char *p, **lp;
	int *lens, maxcols;
	TBL *tbl;
	char **cols;

	if ((t = tbl = calloc(entries, sizeof(TBL))) == NULL)
		err(1, (char *)NULL);
	if ((cols = calloc((maxcols = DEFCOLS), sizeof(char *))) == NULL)
		err(1, (char *)NULL);
	if ((lens = calloc(maxcols, sizeof(int))) == NULL)
		err(1, (char *)NULL);
	for (cnt = 0, lp = list; cnt < entries; ++cnt, ++lp, ++t) {
		for (coloff = 0, p = *lp; (cols[coloff] = strtok(p, separator));
		    p = NULL)
			if (++coloff == maxcols) {
				if (!(cols = realloc(cols, (u_int)maxcols +
				    DEFCOLS * sizeof(char *))) ||
				    !(lens = realloc(lens,
				    (u_int)maxcols + DEFCOLS * sizeof(int))))
					err(1, NULL);
				memset((char *)lens + maxcols * sizeof(int),
				    0, DEFCOLS * sizeof(int));
				maxcols += DEFCOLS;
			}
		if ((t->list = calloc(coloff, sizeof(char *))) == NULL)
			err(1, (char *)NULL);
		if ((t->len = calloc(coloff, sizeof(int))) == NULL)
			err(1, (char *)NULL);
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
#define	MAXLINELEN	(LINE_MAX + 1)

void
input(fp)
	FILE *fp;
{
	static int maxentry;
	int len;
	char *p, buf[MAXLINELEN];

	if (!list)
		if ((list = calloc((maxentry = DEFNUM), sizeof(char *))) ==
		    NULL)
			err(1, (char *)NULL);
	while (fgets(buf, MAXLINELEN, fp)) {
		for (p = buf; *p && isspace(*p); ++p);
		if (!*p)
			continue;
		if (!(p = strchr(p, '\n'))) {
			warnx("line too long");
			eval = 1;
			continue;
		}
		*p = '\0';
		len = p - buf;
		if (maxlength < len)
			maxlength = len;
		if (entries == maxentry) {
			maxentry += DEFNUM;
			if (!(list = realloc(list,
			    (u_int)maxentry * sizeof(char *))))
				err(1, NULL);
		}
		list[entries++] = strdup(buf);
	}
}

void
usage()
{

	(void)fprintf(stderr,
	    "usage: column [-tx] [-c columns] [-s sep] [file ...]\n");
	exit(1);
}
