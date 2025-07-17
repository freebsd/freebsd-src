/*	$OpenBSD: diffreg.c,v 1.93 2019/06/28 13:35:00 deraadt Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
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
 */

#include <sys/capsicum.h>
#include <sys/stat.h>

#include <capsicum_helpers.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <paths.h>
#include <regex.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pr.h"
#include "diff.h"
#include "xmalloc.h"

/*
 * diff - compare two files.
 */

/*
 *	Uses an algorithm due to Harold Stone, which finds a pair of longest
 *	identical subsequences in the two files.
 *
 *	The major goal is to generate the match vector J. J[i] is the index of
 *	the line in file1 corresponding to line i file0. J[i] = 0 if there is no
 *	such line in file1.
 *
 *	Lines are hashed so as to work in core. All potential matches are
 *	located by sorting the lines of each file on the hash (called
 *	``value''). In particular, this collects the equivalence classes in
 *	file1 together. Subroutine equiv replaces the value of each line in
 *	file0 by the index of the first element of its matching equivalence in
 *	(the reordered) file1. To save space equiv squeezes file1 into a single
 *	array member in which the equivalence classes are simply concatenated,
 *	except that their first members are flagged by changing sign.
 *
 *	Next the indices that point into member are unsorted into array class
 *	according to the original order of file0.
 *
 *	The cleverness lies in routine stone. This marches through the lines of
 *	file0, developing a vector klist of "k-candidates". At step i
 *	a k-candidate is a matched pair of lines x,y (x in file0 y in file1)
 *	such that there is a common subsequence of length k between the first
 *	i lines of file0 and the first y lines of file1, but there is no such
 *	subsequence for any smaller y. x is the earliest possible mate to y that
 *	occurs in such a subsequence.
 *
 *	Whenever any of the members of the equivalence class of lines in file1
 *	matable to a line in file0 has serial number less than the y of some
 *	k-candidate, that k-candidate with the smallest such y is replaced. The
 *	new k-candidate is chained (via pred) to the current k-1 candidate so
 *	that the actual subsequence can be recovered. When a member has serial
 *	number greater that the y of all k-candidates, the klist is extended. At
 *	the end, the longest subsequence is pulled out and placed in the array J
 *	by unravel.
 *
 *	With J in hand, the matches there recorded are check'ed against reality
 *	to assure that no spurious matches have crept in due to hashing. If they
 *	have, they are broken, and "jackpot" is recorded -- a harmless matter
 *	except that a true match for a spuriously mated line may now be
 *	unnecessarily reported as a change.
 *
 *	Much of the complexity of the program comes simply from trying to
 *	minimize core utilization and maximize the range of doable problems by
 *	dynamically allocating what is needed and reusing what is not. The core
 *	requirements for problems larger than somewhat are (in words)
 *	2*length(file0) + length(file1) + 3*(number of k-candidates installed),
 *	typically about 6n words for files of length n.
 */

struct cand {
	int	x;
	int	y;
	int	pred;
};

static struct line {
	int	serial;
	int	value;
} *file[2];

/*
 * The following struct is used to record change information when
 * doing a "context" or "unified" diff.  (see routine "change" to
 * understand the highly mnemonic field names)
 */
struct context_vec {
	int	a;		/* start line in old file */
	int	b;		/* end line in old file */
	int	c;		/* start line in new file */
	int	d;		/* end line in new file */
};

enum readhash { RH_BINARY, RH_OK, RH_EOF };

static int	 diffreg_stone(char *, char *, int, int);
static FILE	*opentemp(const char *);
static void	 output(char *, FILE *, char *, FILE *, int);
static void	 check(FILE *, FILE *, int);
static void	 range(int, int, const char *);
static void	 uni_range(int, int);
static void	 dump_context_vec(FILE *, FILE *, int);
static void	 dump_unified_vec(FILE *, FILE *, int);
static bool	 prepare(int, FILE *, size_t, int);
static void	 prune(void);
static void	 equiv(struct line *, int, struct line *, int, int *);
static void	 unravel(int);
static void	 unsort(struct line *, int, int *);
static void	 change(char *, FILE *, char *, FILE *, int, int, int, int, int *);
static void	 sort(struct line *, int);
static void	 print_header(const char *, const char *);
static void	 print_space(int, int, int);
static bool	 ignoreline_pattern(char *);
static bool	 ignoreline(char *, bool);
static int	 asciifile(FILE *);
static int	 fetch(long *, int, int, FILE *, int, int, int);
static int	 newcand(int, int, int);
static int	 search(int *, int, int);
static int	 skipline(FILE *);
static int	 stone(int *, int, int *, int *, int);
static enum readhash readhash(FILE *, int, unsigned *);
static int	 files_differ(FILE *, FILE *, int);
static char	*match_function(const long *, int, FILE *);
static char	*preadline(int, size_t, off_t);

static int	 *J;			/* will be overlaid on class */
static int	 *class;		/* will be overlaid on file[0] */
static int	 *klist;		/* will be overlaid on file[0] after class */
static int	 *member;		/* will be overlaid on file[1] */
static int	 clen;
static int	 inifdef;		/* whether or not we are in a #ifdef block */
static size_t	 len[2];		/* lengths of files in lines */
static size_t	 pref, suff;		/* lengths of prefix and suffix */
static size_t	 slen[2];		/* lengths of files minus pref / suff */
static int	 anychange;
static int	 hw, lpad,rpad;		/* half width and padding */
static int	 edoffset;
static long	*ixnew;		/* will be overlaid on file[1] */
static long	*ixold;		/* will be overlaid on klist */
static struct cand *clist;	/* merely a free storage pot for candidates */
static int	 clistlen;		/* the length of clist */
static struct line *sfile[2];	/* shortened by pruning common prefix/suffix */
static int	(*chrtran)(int);	/* translation table for case-folding */
static struct context_vec *context_vec_start;
static struct context_vec *context_vec_end;
static struct context_vec *context_vec_ptr;

#define FUNCTION_CONTEXT_SIZE	55
static char lastbuf[FUNCTION_CONTEXT_SIZE];
static int lastline;
static int lastmatchline;

int
diffreg(char *file1, char *file2, int flags, int capsicum)
{
	/*
	 * If we have set the algorithm with -A or --algorithm use that if we
	 * can and if not print an error.
	 */
	if (diff_algorithm_set) {
		if (diff_algorithm == D_DIFFMYERS ||
		    diff_algorithm == D_DIFFPATIENCE) {
			if (can_libdiff(flags))
				return diffreg_new(file1, file2, flags, capsicum);
			else
				errx(2, "cannot use Myers algorithm with selected options");
		} else {
			/* Fallback to using stone. */
			return diffreg_stone(file1, file2, flags, capsicum);
		}
	} else {
		if (can_libdiff(flags))
			return diffreg_new(file1, file2, flags, capsicum);
		else
			return diffreg_stone(file1, file2, flags, capsicum);
	}
}

static int
clow2low(int c)
{

	return (c);
}

static int
cup2low(int c)
{

	return (tolower(c));
}

int
diffreg_stone(char *file1, char *file2, int flags, int capsicum)
{
	FILE *f1, *f2;
	int i, rval;
	struct pr *pr = NULL;
	cap_rights_t rights_ro;

	f1 = f2 = NULL;
	rval = D_SAME;
	anychange = 0;
	lastline = 0;
	lastmatchline = 0;

	/*
	 * In side-by-side mode, we need to print the left column, a
	 * change marker surrounded by padding, and the right column.
	 *
	 * If expanding tabs, we don't care about alignment, so we simply
	 * subtract 3 from the width and divide by two.
	 *
	 * If not expanding tabs, we need to ensure that the right column
	 * is aligned to a tab stop.  We start with the same formula, then
	 * decrement until we reach a size that lets us tab-align the
	 * right column.  We then adjust the width down if necessary for
	 * the padding calculation to work.
	 *
	 * Left padding is half the space left over, rounded down; right
	 * padding is whatever is needed to match the width.
	 */
	if (diff_format == D_SIDEBYSIDE) {
		if (flags & D_EXPANDTABS) {
			if (width > 3) {
				hw = (width - 3) / 2;
			} else {
				/* not enough space */
				hw = 0;
			}
		} else if (width <= 3 || width <= tabsize) {
			/* not enough space */
			hw = 0;
		} else {
			hw = (width - 3) / 2;
			while (hw > 0 && roundup(hw + 3, tabsize) + hw > width)
				hw--;
			if (width - (roundup(hw + 3, tabsize) + hw) < tabsize)
				width = roundup(hw + 3, tabsize) + hw;
		}
		lpad = (width - hw * 2 - 1) / 2;
		rpad = (width - hw * 2 - 1) - lpad;
	}

	if (flags & D_IGNORECASE)
		chrtran = cup2low;
	else
		chrtran = clow2low;
	if (S_ISDIR(stb1.st_mode) != S_ISDIR(stb2.st_mode))
		return (S_ISDIR(stb1.st_mode) ? D_MISMATCH1 : D_MISMATCH2);
	if (strcmp(file1, "-") == 0 && strcmp(file2, "-") == 0)
		goto closem;

	if (flags & D_EMPTY1)
		f1 = fopen(_PATH_DEVNULL, "r");
	else {
		if (!S_ISREG(stb1.st_mode)) {
			if ((f1 = opentemp(file1)) == NULL ||
			    fstat(fileno(f1), &stb1) == -1) {
				warn("%s", file1);
				rval = D_ERROR;
				status |= 2;
				goto closem;
			}
		} else if (strcmp(file1, "-") == 0)
			f1 = stdin;
		else
			f1 = fopen(file1, "r");
	}
	if (f1 == NULL) {
		warn("%s", file1);
		rval = D_ERROR;
		status |= 2;
		goto closem;
	}

	if (flags & D_EMPTY2)
		f2 = fopen(_PATH_DEVNULL, "r");
	else {
		if (!S_ISREG(stb2.st_mode)) {
			if ((f2 = opentemp(file2)) == NULL ||
			    fstat(fileno(f2), &stb2) == -1) {
				warn("%s", file2);
				rval = D_ERROR;
				status |= 2;
				goto closem;
			}
		} else if (strcmp(file2, "-") == 0)
			f2 = stdin;
		else
			f2 = fopen(file2, "r");
	}
	if (f2 == NULL) {
		warn("%s", file2);
		rval = D_ERROR;
		status |= 2;
		goto closem;
	}

	if (lflag)
		pr = start_pr(file1, file2);

	if (capsicum) {
		cap_rights_init(&rights_ro, CAP_READ, CAP_FSTAT, CAP_SEEK);
		if (caph_rights_limit(fileno(f1), &rights_ro) < 0)
			err(2, "unable to limit rights on: %s", file1);
		if (caph_rights_limit(fileno(f2), &rights_ro) < 0)
			err(2, "unable to limit rights on: %s", file2);
		if (fileno(f1) == STDIN_FILENO || fileno(f2) == STDIN_FILENO) {
			/* stdin has already been limited */
			if (caph_limit_stderr() == -1)
				err(2, "unable to limit stderr");
			if (caph_limit_stdout() == -1)
				err(2, "unable to limit stdout");
		} else if (caph_limit_stdio() == -1)
				err(2, "unable to limit stdio");

		caph_cache_catpages();
		caph_cache_tzdata();
		if (caph_enter() < 0)
			err(2, "unable to enter capability mode");
	}

	switch (files_differ(f1, f2, flags)) {
	case 0:
		goto closem;
	case 1:
		break;
	default:
		/* error */
		rval = D_ERROR;
		status |= 2;
		goto closem;
	}

	if (diff_format == D_BRIEF && ignore_pats == NULL &&
	    (flags & (D_FOLDBLANKS|D_IGNOREBLANKS|D_IGNORECASE|
	    D_SKIPBLANKLINES|D_STRIPCR)) == 0)
	{
		rval = D_DIFFER;
		status |= 1;
		goto closem;
	}
	if ((flags & D_FORCEASCII) != 0) {
		(void)prepare(0, f1, stb1.st_size, flags);
		(void)prepare(1, f2, stb2.st_size, flags);
	} else if (!asciifile(f1) || !asciifile(f2) ||
		    !prepare(0, f1, stb1.st_size, flags) ||
		    !prepare(1, f2, stb2.st_size, flags)) {
		rval = D_BINARY;
		status |= 1;
		goto closem;
	}
	if (len[0] > INT_MAX - 2)
		errc(1, EFBIG, "%s", file1);
	if (len[1] > INT_MAX - 2)
		errc(1, EFBIG, "%s", file2);

	prune();
	sort(sfile[0], slen[0]);
	sort(sfile[1], slen[1]);

	member = (int *)file[1];
	equiv(sfile[0], slen[0], sfile[1], slen[1], member);
	member = xreallocarray(member, slen[1] + 2, sizeof(*member));

	class = (int *)file[0];
	unsort(sfile[0], slen[0], class);
	class = xreallocarray(class, slen[0] + 2, sizeof(*class));

	klist = xcalloc(slen[0] + 2, sizeof(*klist));
	clen = 0;
	clistlen = 100;
	clist = xcalloc(clistlen, sizeof(*clist));
	i = stone(class, slen[0], member, klist, flags);
	free(member);
	free(class);

	J = xreallocarray(J, len[0] + 2, sizeof(*J));
	unravel(klist[i]);
	free(clist);
	free(klist);

	ixold = xreallocarray(ixold, len[0] + 2, sizeof(*ixold));
	ixnew = xreallocarray(ixnew, len[1] + 2, sizeof(*ixnew));
	check(f1, f2, flags);
	output(file1, f1, file2, f2, flags);

closem:
	if (pr != NULL)
		stop_pr(pr);
	if (anychange) {
		status |= 1;
		if (rval == D_SAME)
			rval = D_DIFFER;
	}
	if (f1 != NULL)
		fclose(f1);
	if (f2 != NULL)
		fclose(f2);

	return (rval);
}

/*
 * Check to see if the given files differ.
 * Returns 0 if they are the same, 1 if different, and -1 on error.
 * XXX - could use code from cmp(1) [faster]
 */
static int
files_differ(FILE *f1, FILE *f2, int flags)
{
	char buf1[BUFSIZ], buf2[BUFSIZ];
	size_t i, j;

	if ((flags & (D_EMPTY1|D_EMPTY2)) || stb1.st_size != stb2.st_size ||
	    (stb1.st_mode & S_IFMT) != (stb2.st_mode & S_IFMT))
		return (1);

	if (stb1.st_dev == stb2.st_dev && stb1.st_ino == stb2.st_ino)
		return (0);

	for (;;) {
		i = fread(buf1, 1, sizeof(buf1), f1);
		j = fread(buf2, 1, sizeof(buf2), f2);
		if ((!i && ferror(f1)) || (!j && ferror(f2)))
			return (-1);
		if (i != j)
			return (1);
		if (i == 0)
			return (0);
		if (memcmp(buf1, buf2, i) != 0)
			return (1);
	}
}

static FILE *
opentemp(const char *f)
{
	char buf[BUFSIZ], tempfile[PATH_MAX];
	ssize_t nread;
	int ifd, ofd;

	if (strcmp(f, "-") == 0)
		ifd = STDIN_FILENO;
	else if ((ifd = open(f, O_RDONLY, 0644)) == -1)
		return (NULL);

	(void)strlcpy(tempfile, _PATH_TMP "/diff.XXXXXXXX", sizeof(tempfile));

	if ((ofd = mkstemp(tempfile)) == -1) {
		close(ifd);
		return (NULL);
	}
	unlink(tempfile);
	while ((nread = read(ifd, buf, BUFSIZ)) > 0) {
		if (write(ofd, buf, nread) != nread) {
			close(ifd);
			close(ofd);
			return (NULL);
		}
	}
	close(ifd);
	lseek(ofd, (off_t)0, SEEK_SET);
	return (fdopen(ofd, "r"));
}

static bool
prepare(int i, FILE *fd, size_t filesize, int flags)
{
	struct line *p;
	unsigned h;
	size_t sz, j = 0;
	enum readhash r;

	rewind(fd);

	sz = MIN(filesize, SIZE_MAX) / 25;
	if (sz < 100)
		sz = 100;

	p = xcalloc(sz + 3, sizeof(*p));
	while ((r = readhash(fd, flags, &h)) != RH_EOF) {
		if (r == RH_BINARY)
			return (false);
		if (j == SIZE_MAX)
			break;
		if (j == sz) {
			sz = sz * 3 / 2;
			p = xreallocarray(p, sz + 3, sizeof(*p));
		}
		p[++j].value = h;
	}

	len[i] = j;
	file[i] = p;

	return (true);
}

static void
prune(void)
{
	size_t i, j;

	for (pref = 0; pref < len[0] && pref < len[1] &&
	    file[0][pref + 1].value == file[1][pref + 1].value;
	    pref++)
		;
	for (suff = 0; suff < len[0] - pref && suff < len[1] - pref &&
	    file[0][len[0] - suff].value == file[1][len[1] - suff].value;
	    suff++)
		;
	for (j = 0; j < 2; j++) {
		sfile[j] = file[j] + pref;
		slen[j] = len[j] - pref - suff;
		for (i = 0; i <= slen[j]; i++)
			sfile[j][i].serial = i;
	}
}

static void
equiv(struct line *a, int n, struct line *b, int m, int *c)
{
	int i, j;

	i = j = 1;
	while (i <= n && j <= m) {
		if (a[i].value < b[j].value)
			a[i++].value = 0;
		else if (a[i].value == b[j].value)
			a[i++].value = j;
		else
			j++;
	}
	while (i <= n)
		a[i++].value = 0;
	b[m + 1].value = 0;
	j = 0;
	while (++j <= m) {
		c[j] = -b[j].serial;
		while (b[j + 1].value == b[j].value) {
			j++;
			c[j] = b[j].serial;
		}
	}
	c[j] = -1;
}

static int
stone(int *a, int n, int *b, int *c, int flags)
{
	int i, k, y, j, l;
	int oldc, tc, oldl, sq;
	unsigned numtries, bound;

	if (flags & D_MINIMAL)
		bound = UINT_MAX;
	else {
		sq = sqrt(n);
		bound = MAX(256, sq);
	}

	k = 0;
	c[0] = newcand(0, 0, 0);
	for (i = 1; i <= n; i++) {
		j = a[i];
		if (j == 0)
			continue;
		y = -b[j];
		oldl = 0;
		oldc = c[0];
		numtries = 0;
		do {
			if (y <= clist[oldc].y)
				continue;
			l = search(c, k, y);
			if (l != oldl + 1)
				oldc = c[l - 1];
			if (l <= k) {
				if (clist[c[l]].y <= y)
					continue;
				tc = c[l];
				c[l] = newcand(i, y, oldc);
				oldc = tc;
				oldl = l;
				numtries++;
			} else {
				c[l] = newcand(i, y, oldc);
				k++;
				break;
			}
		} while ((y = b[++j]) > 0 && numtries < bound);
	}
	return (k);
}

static int
newcand(int x, int y, int pred)
{
	struct cand *q;

	if (clen == clistlen) {
		clistlen = clistlen * 11 / 10;
		clist = xreallocarray(clist, clistlen, sizeof(*clist));
	}
	q = clist + clen;
	q->x = x;
	q->y = y;
	q->pred = pred;
	return (clen++);
}

static int
search(int *c, int k, int y)
{
	int i, j, l, t;

	if (clist[c[k]].y < y)	/* quick look for typical case */
		return (k + 1);
	i = 0;
	j = k + 1;
	for (;;) {
		l = (i + j) / 2;
		if (l <= i)
			break;
		t = clist[c[l]].y;
		if (t > y)
			j = l;
		else if (t < y)
			i = l;
		else
			return (l);
	}
	return (l + 1);
}

static void
unravel(int p)
{
	struct cand *q;
	size_t i;

	for (i = 0; i <= len[0]; i++)
		J[i] = i <= pref ? i :
		    i > len[0] - suff ? i + len[1] - len[0] : 0;
	for (q = clist + p; q->y != 0; q = clist + q->pred)
		J[q->x + pref] = q->y + pref;
}

/*
 * Check does double duty:
 *  1. ferret out any fortuitous correspondences due to confounding by
 *     hashing (which result in "jackpot")
 *  2. collect random access indexes to the two files
 */
static void
check(FILE *f1, FILE *f2, int flags)
{
	int i, j, /* jackpot, */ c, d;
	long ctold, ctnew;

	rewind(f1);
	rewind(f2);
	j = 1;
	ixold[0] = ixnew[0] = 0;
	/* jackpot = 0; */
	ctold = ctnew = 0;
	for (i = 1; i <= (int)len[0]; i++) {
		if (J[i] == 0) {
			ixold[i] = ctold += skipline(f1);
			continue;
		}
		while (j < J[i]) {
			ixnew[j] = ctnew += skipline(f2);
			j++;
		}
		if (flags & (D_FOLDBLANKS | D_IGNOREBLANKS | D_IGNORECASE | D_STRIPCR)) {
			for (;;) {
				c = getc(f1);
				d = getc(f2);
				/*
				 * GNU diff ignores a missing newline
				 * in one file for -b or -w.
				 */
				if (flags & (D_FOLDBLANKS | D_IGNOREBLANKS)) {
					if (c == EOF && isspace(d)) {
						ctnew++;
						break;
					} else if (isspace(c) && d == EOF) {
						ctold++;
						break;
					}
				}
				ctold++;
				ctnew++;
				if (flags & D_STRIPCR && (c == '\r' || d == '\r')) {
					if (c == '\r') {
						if ((c = getc(f1)) == '\n') {
							ctold++;
						} else {
							ungetc(c, f1);
						}
					}
					if (d == '\r') {
						if ((d = getc(f2)) == '\n') {
							ctnew++;
						} else {
							ungetc(d, f2);
						}
					}
					break;
				}
				if ((flags & D_FOLDBLANKS) && isspace(c) &&
				    isspace(d)) {
					do {
						if (c == '\n')
							break;
						ctold++;
					} while (isspace(c = getc(f1)));
					do {
						if (d == '\n')
							break;
						ctnew++;
					} while (isspace(d = getc(f2)));
				} else if (flags & D_IGNOREBLANKS) {
					while (isspace(c) && c != '\n') {
						c = getc(f1);
						ctold++;
					}
					while (isspace(d) && d != '\n') {
						d = getc(f2);
						ctnew++;
					}
				}
				if (chrtran(c) != chrtran(d)) {
					/* jackpot++; */
					J[i] = 0;
					if (c != '\n' && c != EOF)
						ctold += skipline(f1);
					if (d != '\n' && c != EOF)
						ctnew += skipline(f2);
					break;
				}
				if (c == '\n' || c == EOF)
					break;
			}
		} else {
			for (;;) {
				ctold++;
				ctnew++;
				if ((c = getc(f1)) != (d = getc(f2))) {
					/* jackpot++; */
					J[i] = 0;
					if (c != '\n' && c != EOF)
						ctold += skipline(f1);
					if (d != '\n' && c != EOF)
						ctnew += skipline(f2);
					break;
				}
				if (c == '\n' || c == EOF)
					break;
			}
		}
		ixold[i] = ctold;
		ixnew[j] = ctnew;
		j++;
	}
	for (; j <= (int)len[1]; j++) {
		ixnew[j] = ctnew += skipline(f2);
	}
	/*
	 * if (jackpot)
	 *	fprintf(stderr, "jackpot\n");
	 */
}

/* shellsort CACM #201 */
static void
sort(struct line *a, int n)
{
	struct line *ai, *aim, w;
	int j, m = 0, k;

	if (n == 0)
		return;
	for (j = 1; j <= n; j *= 2)
		m = 2 * j - 1;
	for (m /= 2; m != 0; m /= 2) {
		k = n - m;
		for (j = 1; j <= k; j++) {
			for (ai = &a[j]; ai > a; ai -= m) {
				aim = &ai[m];
				if (aim < ai)
					break;	/* wraparound */
				if (aim->value > ai[0].value ||
				    (aim->value == ai[0].value &&
					aim->serial > ai[0].serial))
					break;
				w.value = ai[0].value;
				ai[0].value = aim->value;
				aim->value = w.value;
				w.serial = ai[0].serial;
				ai[0].serial = aim->serial;
				aim->serial = w.serial;
			}
		}
	}
}

static void
unsort(struct line *f, int l, int *b)
{
	int *a, i;

	a = xcalloc(l + 1, sizeof(*a));
	for (i = 1; i <= l; i++)
		a[f[i].serial] = f[i].value;
	for (i = 1; i <= l; i++)
		b[i] = a[i];
	free(a);
}

static int
skipline(FILE *f)
{
	int i, c;

	for (i = 1; (c = getc(f)) != '\n' && c != EOF; i++)
		continue;
	return (i);
}

static void
output(char *file1, FILE *f1, char *file2, FILE *f2, int flags)
{
	int i, j, m, i0, i1, j0, j1, nc;

	rewind(f1);
	rewind(f2);
	m = len[0];
	J[0] = 0;
	J[m + 1] = len[1] + 1;
	if (diff_format != D_EDIT) {
		for (i0 = 1; i0 <= m; i0 = i1 + 1) {
			while (i0 <= m && J[i0] == J[i0 - 1] + 1) {
				if (diff_format == D_SIDEBYSIDE && suppress_common != 1) {
					nc = fetch(ixold, i0, i0, f1, '\0', 1, flags);
					print_space(nc, hw - nc + lpad + 1 + rpad, flags);
					fetch(ixnew, J[i0], J[i0], f2, '\0', 0, flags);
					printf("\n");
				}
				i0++;
			}
			j0 = J[i0 - 1] + 1;
			i1 = i0 - 1;
			while (i1 < m && J[i1 + 1] == 0)
				i1++;
			j1 = J[i1 + 1] - 1;
			J[i1] = j1;

			/*
			 * When using side-by-side, lines from both of the files are
			 * printed. The algorithm used by diff(1) identifies the ranges
			 * in which two files differ.
			 * See the change() function below.
			 * The for loop below consumes the shorter range, whereas one of
			 * the while loops deals with the longer one.
			 */
			if (diff_format == D_SIDEBYSIDE) {
				for (i = i0, j = j0; i <= i1 && j <= j1; i++, j++)
					change(file1, f1, file2, f2, i, i, j, j, &flags);

				while (i <= i1) {
					change(file1, f1, file2, f2, i, i, j + 1, j, &flags);
					i++;
				}

				while (j <= j1) {
					change(file1, f1, file2, f2, i + 1, i, j, j, &flags);
					j++;
				}
			} else
				change(file1, f1, file2, f2, i0, i1, j0, j1, &flags);
		}
	} else {
		for (i0 = m; i0 >= 1; i0 = i1 - 1) {
			while (i0 >= 1 && J[i0] == J[i0 + 1] - 1 && J[i0] != 0)
				i0--;
			j0 = J[i0 + 1] - 1;
			i1 = i0 + 1;
			while (i1 > 1 && J[i1 - 1] == 0)
				i1--;
			j1 = J[i1 - 1] + 1;
			J[i1] = j1;
			change(file1, f1, file2, f2, i1, i0, j1, j0, &flags);
		}
	}
	if (m == 0)
		change(file1, f1, file2, f2, 1, 0, 1, len[1], &flags);
	if (diff_format == D_IFDEF || diff_format == D_GFORMAT) {
		for (;;) {
#define	c i0
			if ((c = getc(f1)) == EOF)
				return;
			printf("%c", c);
		}
#undef c
	}
	if (anychange != 0) {
		if (diff_format == D_CONTEXT)
			dump_context_vec(f1, f2, flags);
		else if (diff_format == D_UNIFIED)
			dump_unified_vec(f1, f2, flags);
	}
}

static void
range(int a, int b, const char *separator)
{
	printf("%d", a > b ? b : a);
	if (a < b)
		printf("%s%d", separator, b);
}

static void
uni_range(int a, int b)
{
	if (a < b)
		printf("%d,%d", a, b - a + 1);
	else if (a == b)
		printf("%d", b);
	else
		printf("%d,0", b);
}

static char *
preadline(int fd, size_t rlen, off_t off)
{
	char *line;
	ssize_t nr;

	line = xmalloc(rlen + 1);
	if ((nr = pread(fd, line, rlen, off)) == -1)
		err(2, "preadline");
	if (nr > 0 && line[nr-1] == '\n')
		nr--;
	line[nr] = '\0';
	return (line);
}

static bool
ignoreline_pattern(char *line)
{
	int ret;

	ret = regexec(&ignore_re, line, 0, NULL, 0);
	return (ret == 0);	/* if it matched, it should be ignored. */
}

static bool
ignoreline(char *line, bool skip_blanks)
{

	if (skip_blanks && *line == '\0')
		return (true);
	if (ignore_pats != NULL && ignoreline_pattern(line))
		return (true);
	return (false);
}

/*
 * Indicate that there is a difference between lines a and b of the from file
 * to get to lines c to d of the to file.  If a is greater then b then there
 * are no lines in the from file involved and this means that there were
 * lines appended (beginning at b).  If c is greater than d then there are
 * lines missing from the to file.
 */
static void
change(char *file1, FILE *f1, char *file2, FILE *f2, int a, int b, int c, int d,
    int *pflags)
{
	static size_t max_context = 64;
	long curpos;
	int i, nc;
	const char *walk;
	bool skip_blanks, ignore;

	skip_blanks = (*pflags & D_SKIPBLANKLINES);
restart:
	if ((diff_format != D_IFDEF || diff_format == D_GFORMAT) &&
	    a > b && c > d)
		return;
	if (ignore_pats != NULL || skip_blanks) {
		char *line;
		/*
		 * All lines in the change, insert, or delete must match an ignore
		 * pattern for the change to be ignored.
		 */
		if (a <= b) {		/* Changes and deletes. */
			for (i = a; i <= b; i++) {
				line = preadline(fileno(f1),
				    ixold[i] - ixold[i - 1], ixold[i - 1]);
				ignore = ignoreline(line, skip_blanks);
				free(line);
				if (!ignore)
					goto proceed;
			}
		}
		if (a > b || c <= d) {	/* Changes and inserts. */
			for (i = c; i <= d; i++) {
				line = preadline(fileno(f2),
				    ixnew[i] - ixnew[i - 1], ixnew[i - 1]);
				ignore = ignoreline(line, skip_blanks);
				free(line);
				if (!ignore)
					goto proceed;
			}
		}
		return;
	}
proceed:
	if (*pflags & D_HEADER && diff_format != D_BRIEF) {
		printf("%s %s %s\n", diffargs, file1, file2);
		*pflags &= ~D_HEADER;
	}
	if (diff_format == D_CONTEXT || diff_format == D_UNIFIED) {
		/*
		 * Allocate change records as needed.
		 */
		if (context_vec_start == NULL ||
		    context_vec_ptr == context_vec_end - 1) {
			ptrdiff_t offset = -1;

			if (context_vec_start != NULL)
				offset = context_vec_ptr - context_vec_start;
			max_context <<= 1;
			context_vec_start = xreallocarray(context_vec_start,
			    max_context, sizeof(*context_vec_start));
			context_vec_end = context_vec_start + max_context;
			context_vec_ptr = context_vec_start + offset;
		}
		if (anychange == 0) {
			/*
			 * Print the context/unidiff header first time through.
			 */
			print_header(file1, file2);
			anychange = 1;
		} else if (a > context_vec_ptr->b + (2 * diff_context) + 1 &&
		    c > context_vec_ptr->d + (2 * diff_context) + 1) {
			/*
			 * If this change is more than 'diff_context' lines from the
			 * previous change, dump the record and reset it.
			 */
			if (diff_format == D_CONTEXT)
				dump_context_vec(f1, f2, *pflags);
			else
				dump_unified_vec(f1, f2, *pflags);
		}
		context_vec_ptr++;
		context_vec_ptr->a = a;
		context_vec_ptr->b = b;
		context_vec_ptr->c = c;
		context_vec_ptr->d = d;
		return;
	}
	if (anychange == 0)
		anychange = 1;
	switch (diff_format) {
	case D_BRIEF:
		return;
	case D_NORMAL:
	case D_EDIT:
		range(a, b, ",");
		printf("%c", a > b ? 'a' : c > d ? 'd' : 'c');
		if (diff_format == D_NORMAL)
			range(c, d, ",");
		printf("\n");
		break;
	case D_REVERSE:
		printf("%c", a > b ? 'a' : c > d ? 'd' : 'c');
		range(a, b, " ");
		printf("\n");
		break;
	case D_NREVERSE:
		if (a > b)
			printf("a%d %d\n", b, d - c + 1);
		else {
			printf("d%d %d\n", a, b - a + 1);
			if (!(c > d))
				/* add changed lines */
				printf("a%d %d\n", b, d - c + 1);
		}
		break;
	}
	if (diff_format == D_GFORMAT) {
		curpos = ftell(f1);
		/* print through if append (a>b), else to (nb: 0 vs 1 orig) */
		nc = ixold[a > b ? b : a - 1] - curpos;
		for (i = 0; i < nc; i++)
			printf("%c", getc(f1));
		for (walk = group_format; *walk != '\0'; walk++) {
			if (*walk == '%') {
				walk++;
				switch (*walk) {
				case '<':
					fetch(ixold, a, b, f1, '<', 1, *pflags);
					break;
				case '>':
					fetch(ixnew, c, d, f2, '>', 0, *pflags);
					break;
				default:
					printf("%%%c", *walk);
					break;
				}
				continue;
			}
			printf("%c", *walk);
		}
	}
	if (diff_format == D_SIDEBYSIDE) {
		if (color && a > b)
			printf("\033[%sm", add_code);
		else if (color && c > d)
			printf("\033[%sm", del_code);
		if (a > b) {
			print_space(0, hw + lpad, *pflags);
		} else {
			nc = fetch(ixold, a, b, f1, '\0', 1, *pflags);
			print_space(nc, hw - nc + lpad, *pflags);
		}
		if (color && a > b)
			printf("\033[%sm", add_code);
		else if (color && c > d)
			printf("\033[%sm", del_code);
		printf("%c", (a > b) ? '>' : ((c > d) ? '<' : '|'));
		if (color && c > d)
			printf("\033[m");
		print_space(hw + lpad + 1, rpad, *pflags);
		fetch(ixnew, c, d, f2, '\0', 0, *pflags);
		printf("\n");
	}
	if (diff_format == D_NORMAL || diff_format == D_IFDEF) {
		fetch(ixold, a, b, f1, '<', 1, *pflags);
		if (a <= b && c <= d && diff_format == D_NORMAL)
			printf("---\n");
	}
	if (diff_format != D_GFORMAT && diff_format != D_SIDEBYSIDE)
		fetch(ixnew, c, d, f2, diff_format == D_NORMAL ? '>' : '\0', 0, *pflags);
	if (edoffset != 0 && diff_format == D_EDIT) {
		/*
		 * A non-zero edoffset value for D_EDIT indicates that the last line
		 * printed was a bare dot (".") that has been escaped as ".." to
		 * prevent ed(1) from misinterpreting it.  We have to add a
		 * substitute command to change this back and restart where we left
		 * off.
		 */
		printf(".\n");
		printf("%ds/.//\n", a + edoffset - 1);
		b = a + edoffset - 1;
		a = b + 1;
		c += edoffset;
		goto restart;
	}
	if ((diff_format == D_EDIT || diff_format == D_REVERSE) && c <= d)
		printf(".\n");
	if (inifdef) {
		printf("#endif /* %s */\n", ifdefname);
		inifdef = 0;
	}
}

static int
fetch(long *f, int a, int b, FILE *lb, int ch, int oldfile, int flags)
{
	int i, j, c, lastc, col, nc, newcol;

	edoffset = 0;
	nc = 0;
	col = 0;
	/*
	 * When doing #ifdef's, copy down to current line
	 * if this is the first file, so that stuff makes it to output.
	 */
	if ((diff_format == D_IFDEF) && oldfile) {
		long curpos = ftell(lb);
		/* print through if append (a>b), else to (nb: 0 vs 1 orig) */
		nc = f[a > b ? b : a - 1] - curpos;
		for (i = 0; i < nc; i++)
			printf("%c", getc(lb));
	}
	if (a > b)
		return (0);
	if (diff_format == D_IFDEF) {
		if (inifdef) {
			printf("#else /* %s%s */\n",
			    oldfile == 1 ? "!" : "", ifdefname);
		} else {
			if (oldfile)
				printf("#ifndef %s\n", ifdefname);
			else
				printf("#ifdef %s\n", ifdefname);
		}
		inifdef = 1 + oldfile;
	}
	for (i = a; i <= b; i++) {
		fseek(lb, f[i - 1], SEEK_SET);
		nc = f[i] - f[i - 1];
		if (diff_format == D_SIDEBYSIDE && hw < nc)
			nc = hw;
		if (diff_format != D_IFDEF && diff_format != D_GFORMAT &&
		    ch != '\0') {
			if (color && (ch == '>' || ch == '+'))
				printf("\033[%sm", add_code);
			else if (color && (ch == '<' || ch == '-'))
				printf("\033[%sm", del_code);
			printf("%c", ch);
			if (Tflag && (diff_format == D_NORMAL ||
			    diff_format == D_CONTEXT ||
			    diff_format == D_UNIFIED))
				printf("\t");
			else if (diff_format != D_UNIFIED)
				printf(" ");
		}
		col = j = 0;
		lastc = '\0';
		while (j < nc && (hw == 0 || col < hw)) {
			c = getc(lb);
			if (flags & D_STRIPCR && c == '\r') {
				if ((c = getc(lb)) == '\n')
					j++;
				else {
					ungetc(c, lb);
					c = '\r';
				}
			}
			if (c == EOF) {
				if (diff_format == D_EDIT ||
				    diff_format == D_REVERSE ||
				    diff_format == D_NREVERSE)
					warnx("No newline at end of file");
				else
					printf("\n\\ No newline at end of file\n");
				return (col);
			}
			/*
			 * when using --side-by-side, col needs to be increased
			 * in any case to keep the columns aligned
			 */
			if (c == '\t') {
				/*
				 * Calculate where the tab would bring us.
				 * If it would take us to the end of the
				 * column, either clip it (if expanding
				 * tabs) or return right away (if not).
				 */
				newcol = roundup(col + 1, tabsize);
				if ((flags & D_EXPANDTABS) == 0) {
					if (hw > 0 && newcol >= hw)
						return (col);
					printf("\t");
				} else {
					if (hw > 0 && newcol > hw)
						newcol = hw;
					printf("%*s", newcol - col, "");
				}
				col = newcol;
			} else {
				if (diff_format == D_EDIT && j == 1 && c == '\n' &&
				    lastc == '.') {
					/*
					 * Don't print a bare "." line since that will confuse
					 * ed(1). Print ".." instead and set the, global variable
					 * edoffset to an offset from which to restart. The
					 * caller must check the value of edoffset
					 */
					printf(".\n");
					edoffset = i - a + 1;
					return (edoffset);
				}
				/* when side-by-side, do not print a newline */
				if (diff_format != D_SIDEBYSIDE || c != '\n') {
					if (color && c == '\n')
						printf("\033[m%c", c);
					else
						printf("%c", c);
					col++;
				}
			}

			j++;
			lastc = c;
		}
	}
	if (color && diff_format == D_SIDEBYSIDE)
		printf("\033[m");
	return (col);
}

/*
 * Hash function taken from Robert Sedgewick, Algorithms in C, 3d ed., p 578.
 */
static enum readhash
readhash(FILE *f, int flags, unsigned *hash)
{
	int i, t, space;
	unsigned sum;

	sum = 1;
	space = 0;
	for (i = 0;;) {
		switch (t = getc(f)) {
		case '\0':
			if ((flags & D_FORCEASCII) == 0)
				return (RH_BINARY);
			goto hashchar;
		case '\r':
			if (flags & D_STRIPCR) {
				t = getc(f);
				if (t == '\n')
					break;
				ungetc(t, f);
			}
			/* FALLTHROUGH */
		case '\t':
		case '\v':
		case '\f':
		case ' ':
			if ((flags & (D_FOLDBLANKS|D_IGNOREBLANKS)) != 0) {
				space++;
				continue;
			}
			/* FALLTHROUGH */
		default:
		hashchar:
			if (space && (flags & D_IGNOREBLANKS) == 0) {
				i++;
				space = 0;
			}
			sum = sum * 127 + chrtran(t);
			i++;
			continue;
		case EOF:
			if (i == 0)
				return (RH_EOF);
			/* FALLTHROUGH */
		case '\n':
			break;
		}
		break;
	}
	*hash = sum;
	return (RH_OK);
}

static int
asciifile(FILE *f)
{
	unsigned char buf[BUFSIZ];
	size_t cnt;

	if (f == NULL)
		return (1);

	rewind(f);
	cnt = fread(buf, 1, sizeof(buf), f);
	return (memchr(buf, '\0', cnt) == NULL);
}

#define begins_with(s, pre) (strncmp(s, pre, sizeof(pre) - 1) == 0)

static char *
match_function(const long *f, int pos, FILE *fp)
{
	unsigned char buf[FUNCTION_CONTEXT_SIZE];
	size_t nc;
	int last = lastline;
	const char *state = NULL;

	lastline = pos;
	for (; pos > last; pos--) {
		fseek(fp, f[pos - 1], SEEK_SET);
		nc = f[pos] - f[pos - 1];
		if (nc >= sizeof(buf))
			nc = sizeof(buf) - 1;
		nc = fread(buf, 1, nc, fp);
		if (nc == 0)
			continue;
		buf[nc] = '\0';
		buf[strcspn(buf, "\n")] = '\0';
		if (most_recent_pat != NULL) {
			int ret = regexec(&most_recent_re, buf, 0, NULL, 0);

			if (ret != 0)
				continue;
			strlcpy(lastbuf, buf, sizeof(lastbuf));
			lastmatchline = pos;
			return (lastbuf);
		} else if (isalpha(buf[0]) || buf[0] == '_' || buf[0] == '$'
			|| buf[0] == '-' || buf[0] == '+') {
			if (begins_with(buf, "private:")) {
				if (!state)
					state = " (private)";
			} else if (begins_with(buf, "protected:")) {
				if (!state)
					state = " (protected)";
			} else if (begins_with(buf, "public:")) {
				if (!state)
					state = " (public)";
			} else {
				strlcpy(lastbuf, buf, sizeof(lastbuf));
				if (state)
					strlcat(lastbuf, state, sizeof(lastbuf));
				lastmatchline = pos;
				return (lastbuf);
			}
		}
	}
	return (lastmatchline > 0 ? lastbuf : NULL);
}

/* dump accumulated "context" diff changes */
static void
dump_context_vec(FILE *f1, FILE *f2, int flags)
{
	struct context_vec *cvp = context_vec_start;
	int lowa, upb, lowc, upd, do_output;
	int a, b, c, d;
	char ch, *f;

	if (context_vec_start > context_vec_ptr)
		return;

	b = d = 0;		/* gcc */
	lowa = MAX(1, cvp->a - diff_context);
	upb = MIN((int)len[0], context_vec_ptr->b + diff_context);
	lowc = MAX(1, cvp->c - diff_context);
	upd = MIN((int)len[1], context_vec_ptr->d + diff_context);

	printf("***************");
	if (flags & (D_PROTOTYPE | D_MATCHLAST)) {
		f = match_function(ixold, cvp->a - 1, f1);
		if (f != NULL)
			printf(" %s", f);
	}
	printf("\n*** ");
	range(lowa, upb, ",");
	printf(" ****\n");

	/*
	 * Output changes to the "old" file.  The first loop suppresses
	 * output if there were no changes to the "old" file (we'll see
	 * the "old" lines as context in the "new" list).
	 */
	do_output = 0;
	for (; cvp <= context_vec_ptr; cvp++)
		if (cvp->a <= cvp->b) {
			cvp = context_vec_start;
			do_output++;
			break;
		}
	if (do_output) {
		while (cvp <= context_vec_ptr) {
			a = cvp->a;
			b = cvp->b;
			c = cvp->c;
			d = cvp->d;

			if (a <= b && c <= d)
				ch = 'c';
			else
				ch = (a <= b) ? 'd' : 'a';

			if (ch == 'a')
				fetch(ixold, lowa, b, f1, ' ', 0, flags);
			else {
				fetch(ixold, lowa, a - 1, f1, ' ', 0, flags);
				fetch(ixold, a, b, f1,
				    ch == 'c' ? '!' : '-', 0, flags);
			}
			lowa = b + 1;
			cvp++;
		}
		fetch(ixold, b + 1, upb, f1, ' ', 0, flags);
	}
	/* output changes to the "new" file */
	printf("--- ");
	range(lowc, upd, ",");
	printf(" ----\n");

	do_output = 0;
	for (cvp = context_vec_start; cvp <= context_vec_ptr; cvp++)
		if (cvp->c <= cvp->d) {
			cvp = context_vec_start;
			do_output++;
			break;
		}
	if (do_output) {
		while (cvp <= context_vec_ptr) {
			a = cvp->a;
			b = cvp->b;
			c = cvp->c;
			d = cvp->d;

			if (a <= b && c <= d)
				ch = 'c';
			else
				ch = (a <= b) ? 'd' : 'a';

			if (ch == 'd')
				fetch(ixnew, lowc, d, f2, ' ', 0, flags);
			else {
				fetch(ixnew, lowc, c - 1, f2, ' ', 0, flags);
				fetch(ixnew, c, d, f2,
				    ch == 'c' ? '!' : '+', 0, flags);
			}
			lowc = d + 1;
			cvp++;
		}
		fetch(ixnew, d + 1, upd, f2, ' ', 0, flags);
	}
	context_vec_ptr = context_vec_start - 1;
}

/* dump accumulated "unified" diff changes */
static void
dump_unified_vec(FILE *f1, FILE *f2, int flags)
{
	struct context_vec *cvp = context_vec_start;
	int lowa, upb, lowc, upd;
	int a, b, c, d;
	char ch, *f;

	if (context_vec_start > context_vec_ptr)
		return;

	b = d = 0;		/* gcc */
	lowa = MAX(1, cvp->a - diff_context);
	upb = MIN((int)len[0], context_vec_ptr->b + diff_context);
	lowc = MAX(1, cvp->c - diff_context);
	upd = MIN((int)len[1], context_vec_ptr->d + diff_context);

	printf("@@ -");
	uni_range(lowa, upb);
	printf(" +");
	uni_range(lowc, upd);
	printf(" @@");
	if (flags & (D_PROTOTYPE | D_MATCHLAST)) {
		f = match_function(ixold, cvp->a - 1, f1);
		if (f != NULL)
			printf(" %s", f);
	}
	printf("\n");

	/*
	 * Output changes in "unified" diff format--the old and new lines
	 * are printed together.
	 */
	for (; cvp <= context_vec_ptr; cvp++) {
		a = cvp->a;
		b = cvp->b;
		c = cvp->c;
		d = cvp->d;

		/*
		 * c: both new and old changes
		 * d: only changes in the old file
		 * a: only changes in the new file
		 */
		if (a <= b && c <= d)
			ch = 'c';
		else
			ch = (a <= b) ? 'd' : 'a';

		switch (ch) {
		case 'c':
			fetch(ixold, lowa, a - 1, f1, ' ', 0, flags);
			fetch(ixold, a, b, f1, '-', 0, flags);
			fetch(ixnew, c, d, f2, '+', 0, flags);
			break;
		case 'd':
			fetch(ixold, lowa, a - 1, f1, ' ', 0, flags);
			fetch(ixold, a, b, f1, '-', 0, flags);
			break;
		case 'a':
			fetch(ixnew, lowc, c - 1, f2, ' ', 0, flags);
			fetch(ixnew, c, d, f2, '+', 0, flags);
			break;
		}
		lowa = b + 1;
		lowc = d + 1;
	}
	fetch(ixnew, d + 1, upd, f2, ' ', 0, flags);

	context_vec_ptr = context_vec_start - 1;
}

static void
print_header(const char *file1, const char *file2)
{
	const char *time_format;
	char buf[256];
	struct tm tm1, tm2, *tm_ptr1, *tm_ptr2;
	int nsec1 = stb1.st_mtim.tv_nsec;
	int nsec2 = stb2.st_mtim.tv_nsec;

	time_format = "%Y-%m-%d %H:%M:%S";

	if (cflag)
		time_format = "%c";
	tm_ptr1 = localtime_r(&stb1.st_mtime, &tm1);
	tm_ptr2 = localtime_r(&stb2.st_mtime, &tm2);
	if (label[0] != NULL)
		printf("%s %s\n", diff_format == D_CONTEXT ? "***" : "---",
		    label[0]);
	else {
		strftime(buf, sizeof(buf), time_format, tm_ptr1);
		printf("%s %s\t%s", diff_format == D_CONTEXT ? "***" : "---",
		    file1, buf);
		if (!cflag) {
			strftime(buf, sizeof(buf), "%z", tm_ptr1);
			printf(".%.9d %s", nsec1, buf);
		}
		printf("\n");
	}
	if (label[1] != NULL)
		printf("%s %s\n", diff_format == D_CONTEXT ? "---" : "+++",
		    label[1]);
	else {
		strftime(buf, sizeof(buf), time_format, tm_ptr2);
		printf("%s %s\t%s", diff_format == D_CONTEXT ? "---" : "+++",
		    file2, buf);
		if (!cflag) {
			strftime(buf, sizeof(buf), "%z", tm_ptr2);
			printf(".%.9d %s", nsec2, buf);
		}
		printf("\n");
	}
}

/*
 * Prints n number of space characters either by using tab
 * or single space characters.
 * nc is the preceding number of characters
 */
static void
print_space(int nc, int n, int flags)
{
	int col, newcol, tabstop;

	col = nc;
	newcol = nc + n;
	/* first, use tabs if allowed */
	if ((flags & D_EXPANDTABS) == 0) {
		while ((tabstop = roundup(col + 1, tabsize)) <= newcol) {
			printf("\t");
			col = tabstop;
		}
	}
	/* finish with spaces */
	printf("%*s", newcol - col, "");
}
