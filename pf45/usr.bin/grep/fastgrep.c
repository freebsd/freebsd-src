/*	$OpenBSD: util.c,v 1.36 2007/10/02 17:59:18 otto Exp $	*/

/*-
 * Copyright (c) 1999 James Howard and Dag-Erling Coïdan Smørgrav
 * Copyright (C) 2008 Gabor Kovesdan <gabor@FreeBSD.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * XXX: This file is a speed up for grep to cover the defects of the
 * regex library.  These optimizations should practically be implemented
 * there keeping this code clean.  This is a future TODO, but for the
 * meantime, we need to use this workaround.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include "grep.h"

static inline int	grep_cmp(const unsigned char *, const unsigned char *, size_t);
static inline void	grep_revstr(unsigned char *, int);

void
fgrepcomp(fastgrep_t *fg, const char *pat)
{
	unsigned int i;

	/* Initialize. */
	fg->len = strlen(pat);
	fg->bol = false;
	fg->eol = false;
	fg->reversed = false;

	fg->pattern = grep_malloc(strlen(pat) + 1);
	strcpy(fg->pattern, pat);

	/* Preprocess pattern. */
	for (i = 0; i <= UCHAR_MAX; i++)
		fg->qsBc[i] = fg->len;
	for (i = 1; i < fg->len; i++)
		fg->qsBc[fg->pattern[i]] = fg->len - i;
}

/*
 * Returns: -1 on failure, 0 on success
 */
int
fastcomp(fastgrep_t *fg, const char *pat)
{
	unsigned int i;
	int firstHalfDot = -1;
	int firstLastHalfDot = -1;
	int hasDot = 0;
	int lastHalfDot = 0;
	int shiftPatternLen;
	bool bol = false;
	bool eol = false;

	/* Initialize. */
	fg->len = strlen(pat);
	fg->bol = false;
	fg->eol = false;
	fg->reversed = false;

	/* Remove end-of-line character ('$'). */
	if (fg->len > 0 && pat[fg->len - 1] == '$') {
		eol = true;
		fg->eol = true;
		fg->len--;
	}

	/* Remove beginning-of-line character ('^'). */
	if (pat[0] == '^') {
		bol = true;
		fg->bol = true;
		fg->len--;
	}

	if (fg->len >= 14 &&
	    strncmp(pat + (fg->bol ? 1 : 0), "[[:<:]]", 7) == 0 &&
	    strncmp(pat + (fg->bol ? 1 : 0) + fg->len - 7, "[[:>:]]", 7) == 0) {
		fg->len -= 14;
		/* Word boundary is handled separately in util.c */
		wflag = true;
	}

	/*
	 * Copy pattern minus '^' and '$' characters as well as word
	 * match character classes at the beginning and ending of the
	 * string respectively.
	 */
	fg->pattern = grep_malloc(fg->len + 1);
	strlcpy(fg->pattern, pat + (bol ? 1 : 0) + wflag, fg->len + 1);

	/* Look for ways to cheat...er...avoid the full regex engine. */
	for (i = 0; i < fg->len; i++) {
		/* Can still cheat? */
		if (fg->pattern[i] == '.') {
			hasDot = i;
			if (i < fg->len / 2) {
				if (firstHalfDot < 0)
					/* Closest dot to the beginning */
					firstHalfDot = i;
			} else {
				/* Closest dot to the end of the pattern. */
				lastHalfDot = i;
				if (firstLastHalfDot < 0)
					firstLastHalfDot = i;
			}
		} else {
			/* Free memory and let others know this is empty. */
			free(fg->pattern);
			fg->pattern = NULL;
			return (-1);
		}
	}

	/*
	 * Determine if a reverse search would be faster based on the placement
	 * of the dots.
	 */
	if ((!(lflag || cflag)) && ((!(bol || eol)) &&
	    ((lastHalfDot) && ((firstHalfDot < 0) ||
	    ((fg->len - (lastHalfDot + 1)) < (size_t)firstHalfDot)))) &&
	    !oflag && !color) {
		fg->reversed = true;
		hasDot = fg->len - (firstHalfDot < 0 ?
		    firstLastHalfDot : firstHalfDot) - 1;
		grep_revstr(fg->pattern, fg->len);
	}

	/*
	 * Normal Quick Search would require a shift based on the position the
	 * next character after the comparison is within the pattern.  With
	 * wildcards, the position of the last dot effects the maximum shift
	 * distance.
	 * The closer to the end the wild card is the slower the search.  A
	 * reverse version of this algorithm would be useful for wildcards near
	 * the end of the string.
	 *
	 * Examples:
	 * Pattern	Max shift
	 * -------	---------
	 * this		5
	 * .his		4
	 * t.is		3
	 * th.s		2
	 * thi.		1
	 */

	/* Adjust the shift based on location of the last dot ('.'). */
	shiftPatternLen = fg->len - hasDot;

	/* Preprocess pattern. */
	for (i = 0; i <= (signed)UCHAR_MAX; i++)
		fg->qsBc[i] = shiftPatternLen;
	for (i = hasDot + 1; i < fg->len; i++) {
		fg->qsBc[fg->pattern[i]] = fg->len - i;
	}

	/*
	 * Put pattern back to normal after pre-processing to allow for easy
	 * comparisons later.
	 */
	if (fg->reversed)
		grep_revstr(fg->pattern, fg->len);

	return (0);
}

int
grep_search(fastgrep_t *fg, const unsigned char *data, size_t len, regmatch_t *pmatch)
{
	unsigned int j;
	int ret = REG_NOMATCH;

	if (pmatch->rm_so == (ssize_t)len)
		return (ret);

	if (fg->bol && pmatch->rm_so != 0) {
		pmatch->rm_so = len;
		pmatch->rm_eo = len;
		return (ret);
	}

	/* No point in going farther if we do not have enough data. */
	if (len < fg->len)
		return (ret);

	/* Only try once at the beginning or ending of the line. */
	if (fg->bol || fg->eol) {
		/* Simple text comparison. */
		/* Verify data is >= pattern length before searching on it. */
		if (len >= fg->len) {
			/* Determine where in data to start search at. */
			j = fg->eol ? len - fg->len : 0;
			if (!((fg->bol && fg->eol) && (len != fg->len)))
				if (grep_cmp(fg->pattern, data + j,
				    fg->len) == -1) {
					pmatch->rm_so = j;
					pmatch->rm_eo = j + fg->len;
						ret = 0;
				}
		}
	} else if (fg->reversed) {
		/* Quick Search algorithm. */
		j = len;
		do {
			if (grep_cmp(fg->pattern, data + j - fg->len,
				fg->len) == -1) {
				pmatch->rm_so = j - fg->len;
				pmatch->rm_eo = j;
				ret = 0;
				break;
			}
			/* Shift if within bounds, otherwise, we are done. */
			if (j == fg->len)
				break;
			j -= fg->qsBc[data[j - fg->len - 1]];
		} while (j >= fg->len);
	} else {
		/* Quick Search algorithm. */
		j = pmatch->rm_so;
		do {
			if (grep_cmp(fg->pattern, data + j, fg->len) == -1) {
				pmatch->rm_so = j;
				pmatch->rm_eo = j + fg->len;
				ret = 0;
				break;
			}

			/* Shift if within bounds, otherwise, we are done. */
			if (j + fg->len == len)
				break;
			else
				j += fg->qsBc[data[j + fg->len]];
		} while (j <= (len - fg->len));
	}

	return (ret);
}

/*
 * Returns:	i >= 0 on failure (position that it failed)
 *		-1 on success
 */
static inline int
grep_cmp(const unsigned char *pat, const unsigned char *data, size_t len)
{
	size_t size;
	wchar_t *wdata, *wpat;
	unsigned int i;

	if (iflag) {
		if ((size = mbstowcs(NULL, (const char *)data, 0)) ==
		    ((size_t) - 1))
			return (-1);

		wdata = grep_malloc(size * sizeof(wint_t));

		if (mbstowcs(wdata, (const char *)data, size) ==
		    ((size_t) - 1))
			return (-1);

		if ((size = mbstowcs(NULL, (const char *)pat, 0)) ==
		    ((size_t) - 1))
			return (-1);

		wpat = grep_malloc(size * sizeof(wint_t));

		if (mbstowcs(wpat, (const char *)pat, size) == ((size_t) - 1))
			return (-1);
		for (i = 0; i < len; i++) {
			if ((towlower(wpat[i]) == towlower(wdata[i])) ||
			    ((grepbehave != GREP_FIXED) && wpat[i] == L'.'))
				continue;
			free(wpat);
			free(wdata);
				return (i);
		}
	} else {
		for (i = 0; i < len; i++) {
			if ((pat[i] == data[i]) || ((grepbehave != GREP_FIXED) &&
			    pat[i] == '.'))
				continue;
			return (i);
		}
	}
	return (-1);
}

static inline void
grep_revstr(unsigned char *str, int len)
{
	int i;
	char c;

	for (i = 0; i < len / 2; i++) {
		c = str[i];
		str[i] = str[len - i - 1];
		str[len - i - 1] = c;
	}
}
