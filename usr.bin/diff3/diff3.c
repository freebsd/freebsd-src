/*	$OpenBSD: diff3prog.c,v 1.11 2009/10/27 23:59:37 deraadt Exp $	*/

/*
 * Copyright (C) Caldera International Inc.  2001-2002.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code and documentation must retain the above
 *    copyright notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed or owned by Caldera
 *	International, Inc.
 * 4. Neither the name of Caldera International, Inc. nor the names of other
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE LIABLE FOR ANY DIRECT,
 * INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*-
 * Copyright (c) 1991, 1993
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
 *
 *	@(#)diff3.c	8.1 (Berkeley) 6/6/93
 */

#if 0
#ifndef lint
static char sccsid[] = "@(#)diff3.c	8.1 (Berkeley) 6/6/93";
#endif
#endif /* not lint */
#include <sys/cdefs.h>
#include <sys/capsicum.h>
#include <sys/procdesc.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/wait.h>

#include <capsicum_helpers.h>
#include <ctype.h>
#include <err.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>


/*
 * "from" is first in range of changed lines; "to" is last+1
 * from=to=line after point of insertion for added lines.
 */
struct range {
	int from;
	int to;
};

struct diff {
#define DIFF_TYPE2 2
#define DIFF_TYPE3 3
	int type;
#if DEBUG
	char *line;
#endif	/* DEBUG */

	/* Ranges as lines */
	struct range old;
	struct range new;
};

#define EFLAG_NONE 	0
#define EFLAG_OVERLAP 	1
#define EFLAG_NOOVERLAP	2
#define EFLAG_UNMERGED	3

static size_t szchanges;

static struct diff *d13;
static struct diff *d23;
/*
 * "de" is used to gather editing scripts.  These are later spewed out in
 * reverse order.  Its first element must be all zero, the "old" and "new"
 * components of "de" contain line positions. Array overlap indicates which
 * sections in "de" correspond to lines that are different in all three files.
 */
static struct diff *de;
static char *overlap;
static int  overlapcnt;
static FILE *fp[3];
static int cline[3];		/* # of the last-read line in each file (0-2) */
/*
 * The latest known correspondence between line numbers of the 3 files
 * is stored in last[1-3];
 */
static int last[4];
static int Aflag, eflag, iflag, mflag, Tflag;
static int oflag;		/* indicates whether to mark overlaps (-E or -X) */
static int strip_cr;
static char *f1mark, *f2mark, *f3mark;
static const char *oldmark = "<<<<<<<";
static const char *orgmark = "|||||||";
static const char *newmark = ">>>>>>>";
static const char *divider = "=======";

static bool duplicate(struct range *, struct range *);
static int edit(struct diff *, bool, int, int);
static char *getchange(FILE *);
static char *get_line(FILE *, size_t *);
static int readin(int fd, struct diff **);
static int skip(int, int, const char *);
static void change(int, struct range *, bool);
static void keep(int, struct range *);
static void merge(int, int);
static void prange(struct range *, bool);
static void repos(int);
static void edscript(int) __dead2;
static void Ascript(int) __dead2;
static void mergescript(int) __dead2;
static void increase(void);
static void usage(void);
static void printrange(FILE *, struct range *);

static const char diff3_version[] = "FreeBSD diff3 20220517";

enum {
	DIFFPROG_OPT,
	STRIPCR_OPT,
	HELP_OPT,
	VERSION_OPT
};

#define DIFF_PATH "/usr/bin/diff"

#define OPTIONS "3aAeEiL:mTxX"
static struct option longopts[] = {
	{ "ed",			no_argument,		NULL,	'e' },
	{ "show-overlap",	no_argument,		NULL,	'E' },
	{ "overlap-only",	no_argument,		NULL,	'x' },
	{ "initial-tab",	no_argument,		NULL,	'T' },
	{ "text",		no_argument,		NULL,	'a' },
	{ "strip-trailing-cr",	no_argument,		NULL,	STRIPCR_OPT },
	{ "show-all",		no_argument,		NULL,	'A' },
	{ "easy-only",		no_argument,		NULL,	'3' },
	{ "merge",		no_argument,		NULL,	'm' },
	{ "label",		required_argument,	NULL,	'L' },
	{ "diff-program",	required_argument,	NULL,	DIFFPROG_OPT },
	{ "help",		no_argument,		NULL,	HELP_OPT},
	{ "version",		no_argument,		NULL,	VERSION_OPT}
};

static void
usage(void)
{
	fprintf(stderr, "usage: diff3 [-3aAeEimTxX] [-L label1] [-L label2] "
	    "[-L label3] file1 file2 file3\n");
}

static int
readin(int fd, struct diff **dd)
{
	int a, b, c, d;
	size_t i;
	char kind, *p;
	FILE *f;

	f = fdopen(fd, "r");
	if (f == NULL)
		err(2, "fdopen");
	for (i = 0; (p = getchange(f)); i++) {
#if DEBUG
		(*dd)[i].line = strdup(p);
#endif	/* DEBUG */

		if (i >= szchanges - 1)
			increase();
		a = b = (int)strtoimax(p, &p, 10);
		if (*p == ',') {
			p++;
			b = (int)strtoimax(p, &p, 10);
		}
		kind = *p++;
		c = d = (int)strtoimax(p, &p, 10);
		if (*p == ',') {
			p++;
			d = (int)strtoimax(p, &p, 10);
		}
		if (kind == 'a')
			a++;
		if (kind == 'd')
			c++;
		b++;
		d++;
		(*dd)[i].old.from = a;
		(*dd)[i].old.to = b;
		(*dd)[i].new.from = c;
		(*dd)[i].new.to = d;
	}
	if (i) {
		(*dd)[i].old.from = (*dd)[i - 1].old.to;
		(*dd)[i].new.from = (*dd)[i - 1].new.to;
	}
	fclose(f);
	return (i);
}

static int
diffexec(const char *diffprog, char **diffargv, int fd[])
{
	int pd;

	switch (pdfork(&pd, PD_CLOEXEC)) {
	case 0:
		close(fd[0]);
		if (dup2(fd[1], STDOUT_FILENO) == -1)
			err(2, "child could not duplicate descriptor");
		close(fd[1]);
		execvp(diffprog, diffargv);
		err(2, "could not execute diff: %s", diffprog);
		break;
	case -1:
		err(2, "could not fork");
		break;
	}
	close(fd[1]);
	return (pd);
}

static char *
getchange(FILE *b)
{
	char *line;

	while ((line = get_line(b, NULL))) {
		if (isdigit((unsigned char)line[0]))
			return (line);
	}
	return (NULL);
}


static char *
get_line(FILE *b, size_t *n)
{
	ssize_t len;
	static char *buf = NULL;
	static size_t bufsize = 0;

	if ((len = getline(&buf, &bufsize, b)) < 0)
		return (NULL);

	if (strip_cr && len >= 2 && strcmp("\r\n", &(buf[len - 2])) == 0) {
		buf[len - 2] = '\n';
		buf[len - 1] = '\0';
		len--;
	}

	if (n != NULL)
		*n = len;

	return (buf);
}

static void
merge(int m1, int m2)
{
	struct diff *d1, *d2, *d3;
	int j, t1, t2;
	bool dup = false;

	d1 = d13;
	d2 = d23;
	j = 0;

	while (t1 = d1 < d13 + m1, t2 = d2 < d23 + m2, t1 || t2) {
		/* first file is different from the others */
		if (!t2 || (t1 && d1->new.to < d2->new.from)) {
			/* stuff peculiar to 1st file */
			if (eflag == EFLAG_NONE) {
				printf("====1\n");
				change(1, &d1->old, false);
				keep(2, &d1->new);
				change(3, &d1->new, false);
			}
			d1++;
			continue;
		}
		/* second file is different from others */
		if (!t1 || (t2 && d2->new.to < d1->new.from)) {
			if (eflag == EFLAG_NONE) {
				printf("====2\n");
				keep(1, &d2->new);
				change(3, &d2->new, false);
				change(2, &d2->old, false);
			} else if (Aflag || mflag) {
				// XXX-THJ: What does it mean for the second file to differ?
				j = edit(d2, dup, j, DIFF_TYPE2);
			}
			d2++;
			continue;
		}
		/*
		 * Merge overlapping changes in first file
		 * this happens after extension (see below).
		 */
		if (d1 + 1 < d13 + m1 && d1->new.to >= d1[1].new.from) {
			d1[1].old.from = d1->old.from;
			d1[1].new.from = d1->new.from;
			d1++;
			continue;
		}

		/* merge overlapping changes in second */
		if (d2 + 1 < d23 + m2 && d2->new.to >= d2[1].new.from) {
			d2[1].old.from = d2->old.from;
			d2[1].new.from = d2->new.from;
			d2++;
			continue;
		}
		/* stuff peculiar to third file or different in all */
		if (d1->new.from == d2->new.from && d1->new.to == d2->new.to) {
			dup = duplicate(&d1->old, &d2->old);
			/*
			 * dup = 0 means all files differ
			 * dup = 1 means files 1 and 2 identical
			 */
			if (eflag == EFLAG_NONE) {
				printf("====%s\n", dup ? "3" : "");
				change(1, &d1->old, dup);
				change(2, &d2->old, false);
				d3 = d1->old.to > d1->old.from ? d1 : d2;
				change(3, &d3->new, false);
			} else {
				j = edit(d1, dup, j, DIFF_TYPE3);
			}
			dup = false;
			d1++;
			d2++;
			continue;
		}
		/*
		 * Overlapping changes from file 1 and 2; extend changes
		 * appropriately to make them coincide.
		 */
		if (d1->new.from < d2->new.from) {
			d2->old.from -= d2->new.from - d1->new.from;
			d2->new.from = d1->new.from;
		} else if (d2->new.from < d1->new.from) {
			d1->old.from -= d1->new.from - d2->new.from;
			d1->new.from = d2->new.from;
		}
		if (d1->new.to > d2->new.to) {
			d2->old.to += d1->new.to - d2->new.to;
			d2->new.to = d1->new.to;
		} else if (d2->new.to > d1->new.to) {
			d1->old.to += d2->new.to - d1->new.to;
			d1->new.to = d2->new.to;
		}
	}

	if (mflag)
		mergescript(j);
	else if (Aflag)
		Ascript(j);
	else if (eflag)
		edscript(j);
}

/*
 * The range of lines rold.from thru rold.to in file i is to be changed.
 * It is to be printed only if it does not duplicate something to be
 * printed later.
 */
static void
change(int i, struct range *rold, bool dup)
{

	printf("%d:", i);
	last[i] = rold->to;
	prange(rold, false);
	if (dup)
		return;
	i--;
	skip(i, rold->from, NULL);
	skip(i, rold->to, "  ");
}

/*
 * Print the range of line numbers, rold.from thru rold.to, as n1,n2 or 
 * n1.
 */
static void
prange(struct range *rold, bool delete)
{

	if (rold->to <= rold->from)
		printf("%da\n", rold->from - 1);
	else {
		printf("%d", rold->from);
		if (rold->to > rold->from + 1)
			printf(",%d", rold->to - 1);
		if (delete)
			printf("d\n");
		else
			printf("c\n");
	}
}

/*
 * No difference was reported by diff between file 1 (or 2) and file 3,
 * and an artificial dummy difference (trange) must be ginned up to
 * correspond to the change reported in the other file.
 */
static void
keep(int i, struct range *rnew)
{
	int delta;
	struct range trange;

	delta = last[3] - last[i];
	trange.from = rnew->from - delta;
	trange.to = rnew->to - delta;
	change(i, &trange, true);
}

/*
 * skip to just before line number from in file "i".  If "pr" is non-NULL,
 * print all skipped stuff with string pr as a prefix.
 */
static int
skip(int i, int from, const char *pr)
{
	size_t j, n;
	char *line;

	for (n = 0; cline[i] < from - 1; n += j) {
		if ((line = get_line(fp[i], &j)) == NULL)
			errx(EXIT_FAILURE, "logic error");
		if (pr != NULL)
			printf("%s%s", Tflag == 1 ? "\t" : pr, line);
		cline[i]++;
	}
	return ((int) n);
}

/*
 * Return 1 or 0 according as the old range (in file 1) contains exactly
 * the same data as the new range (in file 2).
 */
static bool
duplicate(struct range *r1, struct range *r2)
{
	int c, d;
	int nchar;
	int nline;

	if (r1->to-r1->from != r2->to-r2->from)
		return (0);
	skip(0, r1->from, NULL);
	skip(1, r2->from, NULL);
	nchar = 0;
	for (nline = 0; nline < r1->to - r1->from; nline++) {
		do {
			c = getc(fp[0]);
			d = getc(fp[1]);
			if (c == -1 && d == -1)
				break;
			if (c == -1 || d == -1)
				errx(EXIT_FAILURE, "logic error");
			nchar++;
			if (c != d) {
				repos(nchar);
				return (0);
			}
		} while (c != '\n');
	}
	repos(nchar);
	return (1);
}

static void
repos(int nchar)
{
	int i;

	for (i = 0; i < 2; i++)
		(void)fseek(fp[i], (long)-nchar, SEEK_CUR);
}

/*
 * collect an editing script for later regurgitation
 */
static int
edit(struct diff *diff, bool dup, int j, int difftype)
{
	if (!(eflag == EFLAG_UNMERGED ||
		(!dup && eflag == EFLAG_OVERLAP ) ||
		(dup && eflag == EFLAG_NOOVERLAP))) {
		return (j);
	}
	j++;
	overlap[j] = !dup;
	if (!dup)
		overlapcnt++;

	de[j].type = difftype;
#if DEBUG
	de[j].line = strdup(diff->line);
#endif	/* DEBUG */

	de[j].old.from = diff->old.from;
	de[j].old.to = diff->old.to;
	de[j].new.from = diff->new.from;
	de[j].new.to = diff->new.to;
	return (j);
}

static void
printrange(FILE *p, struct range *r)
{
	char *line = NULL;
	size_t len = 0;
	int i = 1;
	ssize_t rlen = 0;

	/* We haven't been asked to print anything */
	if (r->from == r->to)
		return;

	if (r->from > r->to)
		errx(EXIT_FAILURE, "invalid print range");

	/*
	 * XXX-THJ: We read through all of the file for each range printed.
	 * This duplicates work and will probably impact performance on large
	 * files with lots of ranges.
	 */
	fseek(p, 0L, SEEK_SET);
	while ((rlen = getline(&line, &len, p)) > 0) {
		if (i >= r->from)
			printf("%s", line);
		if (++i > r->to - 1)
			break;
	}
	free(line);
}

/* regurgitate */
static void
edscript(int n)
{
	bool delete;
	struct range *new, *old;

	for (; n > 0; n--) {
		new = &de[n].new;
		old = &de[n].old;

		delete = (new->from == new->to);
		if (!oflag || !overlap[n]) {
			prange(old, delete);
		} else {
			printf("%da\n", old->to - 1);
			printf("%s\n", divider);
		}
		printrange(fp[2], new);
		if (!oflag || !overlap[n]) {
			if (!delete)
				printf(".\n");
		} else {
			printf("%s %s\n.\n", newmark, f3mark);
			printf("%da\n%s %s\n.\n", old->from - 1,
				oldmark, f1mark);
		}
	}
	if (iflag)
		printf("w\nq\n");

	exit(eflag == EFLAG_NONE ? overlapcnt : 0);
}

/*
 * Output an edit script to turn mine into yours, when there is a conflict
 * between the 3 files bracket the changes. Regurgitate the diffs in reverse
 * order to allow the ed script to track down where the lines are as changes
 * are made.
 */
static void
Ascript(int n)
{
	int startmark;
	bool deletenew;
	bool deleteold;

	struct range *new, *old;

	for (; n > 0; n--) {
		new = &de[n].new;
		old = &de[n].old;
		deletenew = (new->from == new->to);
		deleteold = (old->from == old->to);

		if (de[n].type == DIFF_TYPE2) {
			if (!oflag || !overlap[n]) {
				prange(old, deletenew);
				printrange(fp[2], new);
			} else {
				startmark = new->to;

				if (!deletenew)
					startmark--;

				printf("%da\n", startmark);
				printf("%s %s\n", newmark, f3mark);

				printf(".\n");

				printf("%da\n", startmark -
					(new->to - new->from));
				printf("%s %s\n", oldmark, f2mark);
				if (!deleteold)
					printrange(fp[1], old);
				printf("%s\n.\n", divider);
			}

		} else if (de[n].type == DIFF_TYPE3) {
			startmark = old->to - 1;

			if (!oflag || !overlap[n]) {
				prange(old, deletenew);
				printrange(fp[2], new);
			} else {
				printf("%da\n", startmark);
				printf("%s %s\n", orgmark, f2mark);

				if (deleteold) {
					struct range r;
					r.from = old->from-1;
					r.to = new->to;
					printrange(fp[1], &r);
				} else
					printrange(fp[1], old);

				printf("%s\n", divider);
				printrange(fp[2], new);
			}

			if (!oflag || !overlap[n]) {
				if (!deletenew)
					printf(".\n");
			} else {
				printf("%s %s\n.\n", newmark, f3mark);

				/*
				 * Go to the start of the conflict in original
				 * file and append lines
				 */
				printf("%da\n%s %s\n.\n",
					startmark - (old->to - old->from),
					oldmark, f1mark);
			}
		}
	}
	if (iflag)
		printf("w\nq\n");

	exit(overlapcnt > 0);
}

/*
 * Output the merged file directly (don't generate an ed script). When
 * regurgitating diffs we need to walk forward through the file and print any
 * inbetween lines.
 */
static void
mergescript(int i)
{
	struct range r, *new, *old;
	int n;

	r.from = 1;
	r.to = 1;

	for (n = 1; n < i+1; n++) {
		new = &de[n].new;
		old = &de[n].old;

		/* print any lines leading up to here */
		r.to = old->from;
		printrange(fp[0], &r);

		if (de[n].type == DIFF_TYPE2) {
			printf("%s %s\n", oldmark, f2mark);
			printrange(fp[1], old);
			printf("%s\n", divider);
			printrange(fp[2], new);
			printf("%s %s\n", newmark, f3mark);
		} else if (de[n].type == DIFF_TYPE3) {
			if (!oflag || !overlap[n]) {
				printrange(fp[2], new);
			} else {

				printf("%s %s\n", oldmark, f1mark);
				printrange(fp[0], old);

				printf("%s %s\n", orgmark, f2mark);
				if (old->from == old->to) {
					struct range or;
					or.from = old->from - 1;
					or.to = new->to;
					printrange(fp[1], &or);
				} else
					printrange(fp[1], old);

				printf("%s\n", divider);

				printrange(fp[2], new);
				printf("%s %s\n", newmark, f3mark);
			}
		}

		if (old->from == old->to)
			r.from = new->to;
		else
			r.from = old->to;
	}
	/*
	 * Print from the final range to the end of 'myfile'. Any deletions or
	 * additions to this file should have been handled by now.
	 *
	 * If the ranges are the same we need to rewind a line.
	 * If the new range is 0 length (from == to), we need to use the old
	 * range.
	 */
	new = &de[n-1].new;
	old = &de[n-1].old;
	if ((old->from == new->from) &&
		(old->to == new->to))
		r.from--;
	else if (new->from == new->to)
		r.from = old->from;

	/*
	 * If the range is a 3 way merge then we need to skip a line in the
	 * trailing output.
	 */
	if (de[n-1].type == DIFF_TYPE3)
		r.from++;

	r.to = INT_MAX;
	printrange(fp[0], &r);
	exit(overlapcnt > 0);
}

static void
increase(void)
{
	struct diff *p;
	char *q;
	size_t newsz, incr;

	/* are the memset(3) calls needed? */
	newsz = szchanges == 0 ? 64 : 2 * szchanges;
	incr = newsz - szchanges;

	p = reallocarray(d13, newsz, sizeof(struct diff));
	if (p == NULL)
		err(1, NULL);
	memset(p + szchanges, 0, incr * sizeof(struct diff));
	d13 = p;
	p = reallocarray(d23, newsz, sizeof(struct diff));
	if (p == NULL)
		err(1, NULL);
	memset(p + szchanges, 0, incr * sizeof(struct diff));
	d23 = p;
	p = reallocarray(de, newsz, sizeof(struct diff));
	if (p == NULL)
		err(1, NULL);
	memset(p + szchanges, 0, incr * sizeof(struct diff));
	de = p;
	q = reallocarray(overlap, newsz, sizeof(char));
	if (q == NULL)
		err(1, NULL);
	memset(q + szchanges, 0, incr * sizeof(char));
	overlap = q;
	szchanges = newsz;
}


int
main(int argc, char **argv)
{
	int ch, nblabels, status, m, n, kq, nke, nleft, i;
	char *labels[] = { NULL, NULL, NULL };
	const char *diffprog = DIFF_PATH;
	char *file1, *file2, *file3;
	char *diffargv[7];
	int diffargc = 0;
	int fd13[2], fd23[2];
	int pd13, pd23;
	cap_rights_t rights_ro;
	struct kevent *e;

	nblabels = 0;
	eflag = EFLAG_NONE;
	oflag = 0;
	diffargv[diffargc++] = __DECONST(char *, diffprog);
	while ((ch = getopt_long(argc, argv, OPTIONS, longopts, NULL)) != -1) {
		switch (ch) {
		case '3':
			eflag = EFLAG_NOOVERLAP;
			break;
		case 'a':
			diffargv[diffargc++] = __DECONST(char *, "-a");
			break;
		case 'A':
			Aflag = 1;
			break;
		case 'e':
			eflag = EFLAG_UNMERGED;
			break;
		case 'E':
			eflag = EFLAG_UNMERGED;
			oflag = 1;
			break;
		case 'i':
			iflag = 1;
			break;
		case 'L':
			oflag = 1;
			if (nblabels >= 3)
				errx(2, "too many file label options");
			labels[nblabels++] = optarg;
			break;
		case 'm':
			Aflag = 1;
			oflag = 1;
			mflag = 1;
			break;
		case 'T':
			Tflag = 1;
			break;
		case 'x':
			eflag = EFLAG_OVERLAP;
			break;
		case 'X':
			oflag = 1;
			eflag = EFLAG_OVERLAP;
			break;
		case DIFFPROG_OPT:
			diffprog = optarg;
			break;
		case STRIPCR_OPT:
			strip_cr = 1;
			diffargv[diffargc++] = __DECONST(char *, "--strip-trailing-cr");
			break;
		case HELP_OPT:
			usage();
			exit(0);
		case VERSION_OPT:
			printf("%s\n", diff3_version);
			exit(0);
		}
	}
	argc -= optind;
	argv += optind;

	if (Aflag) {
		eflag = EFLAG_UNMERGED;
		oflag = 1;
	}

	if (argc != 3) {
		usage();
		exit(2);
	}

	if (caph_limit_stdio() == -1)
		err(2, "unable to limit stdio");

	cap_rights_init(&rights_ro, CAP_READ, CAP_FSTAT, CAP_SEEK);

	kq = kqueue();
	if (kq == -1)
		err(2, "kqueue");

	e = malloc(2 * sizeof(struct kevent));
	if (e == NULL)
		err(2, "malloc");

	/* TODO stdio */
	file1 = argv[0];
	file2 = argv[1];
	file3 = argv[2];

	if (oflag) {
		asprintf(&f1mark, "%s",
		    labels[0] != NULL ? labels[0] : file1);
		if (f1mark == NULL)
			err(2, "asprintf");
		asprintf(&f2mark, "%s",
		    labels[1] != NULL ? labels[1] : file2);
		if (f2mark == NULL)
			err(2, "asprintf");
		asprintf(&f3mark, "%s",
		    labels[2] != NULL ? labels[2] : file3);
		if (f3mark == NULL)
			err(2, "asprintf");
	}
	fp[0] = fopen(file1, "r");
	if (fp[0] == NULL)
		err(2, "Can't open %s", file1);
	if (caph_rights_limit(fileno(fp[0]), &rights_ro) < 0)
		err(2, "unable to limit rights on: %s", file1);

	fp[1] = fopen(file2, "r");
	if (fp[1] == NULL)
		err(2, "Can't open %s", file2);
	if (caph_rights_limit(fileno(fp[1]), &rights_ro) < 0)
		err(2, "unable to limit rights on: %s", file2);

	fp[2] = fopen(file3, "r");
	if (fp[2] == NULL)
		err(2, "Can't open %s", file3);
	if (caph_rights_limit(fileno(fp[2]), &rights_ro) < 0)
		err(2, "unable to limit rights on: %s", file3);

	if (pipe(fd13))
		err(2, "pipe");
	if (pipe(fd23))
		err(2, "pipe");

	diffargv[diffargc] = file1;
	diffargv[diffargc + 1] = file3;
	diffargv[diffargc + 2] = NULL;

	nleft = 0;
	pd13 = diffexec(diffprog, diffargv, fd13);
	EV_SET(e + nleft , pd13, EVFILT_PROCDESC, EV_ADD, NOTE_EXIT, 0, NULL);
	if (kevent(kq, e + nleft, 1, NULL, 0, NULL) == -1)
		err(2, "kevent1");
	nleft++;

	diffargv[diffargc] = file2;
	pd23 = diffexec(diffprog, diffargv, fd23);
	EV_SET(e + nleft , pd23, EVFILT_PROCDESC, EV_ADD, NOTE_EXIT, 0, NULL);
	if (kevent(kq, e + nleft, 1, NULL, 0, NULL) == -1)
		err(2, "kevent2");
	nleft++;

	caph_cache_catpages();
	if (caph_enter() < 0)
		err(2, "unable to enter capability mode");

	/* parse diffs */
	increase();
	m = readin(fd13[0], &d13);
	n = readin(fd23[0], &d23);

	/* waitpid cooked over pdforks */
	while (nleft > 0) {
		nke = kevent(kq, NULL, 0, e, nleft, NULL);
		if (nke == -1)
			err(2, "kevent");
		for (i = 0; i < nke; i++) {
			status = e[i].data;
			if (WIFEXITED(status) && WEXITSTATUS(status) >= 2)
				errx(2, "diff exited abnormally");
			else if (WIFSIGNALED(status))
				errx(2, "diff killed by signal %d",
				    WTERMSIG(status));
		}
		nleft -= nke;
	}
	merge(m, n);

	return (EXIT_SUCCESS);
}
