/*	$Id: mandoc.c,v 1.62 2011/12/03 16:08:51 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "mandoc.h"
#include "libmandoc.h"

#define DATESIZE 32

static	int	 a2time(time_t *, const char *, const char *);
static	char	*time2a(time_t);
static	int	 numescape(const char *);

/*
 * Pass over recursive numerical expressions.  This context of this
 * function is important: it's only called within character-terminating
 * escapes (e.g., \s[xxxyyy]), so all we need to do is handle initial
 * recursion: we don't care about what's in these blocks. 
 * This returns the number of characters skipped or -1 if an error
 * occurs (the caller should bail).
 */
static int
numescape(const char *start)
{
	int		 i;
	size_t		 sz;
	const char	*cp;

	i = 0;

	/* The expression consists of a subexpression. */

	if ('\\' == start[i]) {
		cp = &start[++i];
		/*
		 * Read past the end of the subexpression.
		 * Bail immediately on errors.
		 */
		if (ESCAPE_ERROR == mandoc_escape(&cp, NULL, NULL))
			return(-1);
		return(i + cp - &start[i]);
	} 

	if ('(' != start[i++])
		return(0);

	/*
	 * A parenthesised subexpression.  Read until the closing
	 * parenthesis, making sure to handle any nested subexpressions
	 * that might ruin our parse.
	 */

	while (')' != start[i]) {
		sz = strcspn(&start[i], ")\\");
		i += (int)sz;

		if ('\0' == start[i])
			return(-1);
		else if ('\\' != start[i])
			continue;

		cp = &start[++i];
		if (ESCAPE_ERROR == mandoc_escape(&cp, NULL, NULL))
			return(-1);
		i += cp - &start[i];
	}

	/* Read past the terminating ')'. */
	return(++i);
}

enum mandoc_esc
mandoc_escape(const char **end, const char **start, int *sz)
{
	char		 c, term, numeric;
	int		 i, lim, ssz, rlim;
	const char	*cp, *rstart;
	enum mandoc_esc	 gly; 

	cp = *end;
	rstart = cp;
	if (start)
		*start = rstart;
	i = lim = 0;
	gly = ESCAPE_ERROR;
	term = numeric = '\0';

	switch ((c = cp[i++])) {
	/*
	 * First the glyphs.  There are several different forms of
	 * these, but each eventually returns a substring of the glyph
	 * name.
	 */
	case ('('):
		gly = ESCAPE_SPECIAL;
		lim = 2;
		break;
	case ('['):
		gly = ESCAPE_SPECIAL;
		/*
		 * Unicode escapes are defined in groff as \[uXXXX] to
		 * \[u10FFFF], where the contained value must be a valid
		 * Unicode codepoint.  Here, however, only check whether
		 * it's not a zero-width escape.
		 */
		if ('u' == cp[i] && ']' != cp[i + 1])
			gly = ESCAPE_UNICODE;
		term = ']';
		break;
	case ('C'):
		if ('\'' != cp[i])
			return(ESCAPE_ERROR);
		gly = ESCAPE_SPECIAL;
		term = '\'';
		break;

	/*
	 * Handle all triggers matching \X(xy, \Xx, and \X[xxxx], where
	 * 'X' is the trigger.  These have opaque sub-strings.
	 */
	case ('F'):
		/* FALLTHROUGH */
	case ('g'):
		/* FALLTHROUGH */
	case ('k'):
		/* FALLTHROUGH */
	case ('M'):
		/* FALLTHROUGH */
	case ('m'):
		/* FALLTHROUGH */
	case ('n'):
		/* FALLTHROUGH */
	case ('V'):
		/* FALLTHROUGH */
	case ('Y'):
		gly = ESCAPE_IGNORE;
		/* FALLTHROUGH */
	case ('f'):
		if (ESCAPE_ERROR == gly)
			gly = ESCAPE_FONT;

		rstart= &cp[i];
		if (start) 
			*start = rstart;

		switch (cp[i++]) {
		case ('('):
			lim = 2;
			break;
		case ('['):
			term = ']';
			break;
		default:
			lim = 1;
			i--;
			break;
		}
		break;

	/*
	 * These escapes are of the form \X'Y', where 'X' is the trigger
	 * and 'Y' is any string.  These have opaque sub-strings.
	 */
	case ('A'):
		/* FALLTHROUGH */
	case ('b'):
		/* FALLTHROUGH */
	case ('D'):
		/* FALLTHROUGH */
	case ('o'):
		/* FALLTHROUGH */
	case ('R'):
		/* FALLTHROUGH */
	case ('X'):
		/* FALLTHROUGH */
	case ('Z'):
		if ('\'' != cp[i++])
			return(ESCAPE_ERROR);
		gly = ESCAPE_IGNORE;
		term = '\'';
		break;

	/*
	 * These escapes are of the form \X'N', where 'X' is the trigger
	 * and 'N' resolves to a numerical expression.
	 */
	case ('B'):
		/* FALLTHROUGH */
	case ('h'):
		/* FALLTHROUGH */
	case ('H'):
		/* FALLTHROUGH */
	case ('L'):
		/* FALLTHROUGH */
	case ('l'):
		gly = ESCAPE_NUMBERED;
		/* FALLTHROUGH */
	case ('S'):
		/* FALLTHROUGH */
	case ('v'):
		/* FALLTHROUGH */
	case ('w'):
		/* FALLTHROUGH */
	case ('x'):
		if (ESCAPE_ERROR == gly)
			gly = ESCAPE_IGNORE;
		if ('\'' != cp[i++])
			return(ESCAPE_ERROR);
		term = numeric = '\'';
		break;

	/*
	 * Special handling for the numbered character escape.
	 * XXX Do any other escapes need similar handling?
	 */
	case ('N'):
		if ('\0' == cp[i])
			return(ESCAPE_ERROR);
		*end = &cp[++i];
		if (isdigit((unsigned char)cp[i-1]))
			return(ESCAPE_IGNORE);
		while (isdigit((unsigned char)**end))
			(*end)++;
		if (start)
			*start = &cp[i];
		if (sz)
			*sz = *end - &cp[i];
		if ('\0' != **end)
			(*end)++;
		return(ESCAPE_NUMBERED);

	/* 
	 * Sizes get a special category of their own.
	 */
	case ('s'):
		gly = ESCAPE_IGNORE;

		rstart = &cp[i];
		if (start) 
			*start = rstart;

		/* See +/- counts as a sign. */
		c = cp[i];
		if ('+' == c || '-' == c || ASCII_HYPH == c)
			++i;

		switch (cp[i++]) {
		case ('('):
			lim = 2;
			break;
		case ('['):
			term = numeric = ']';
			break;
		case ('\''):
			term = numeric = '\'';
			break;
		default:
			lim = 1;
			i--;
			break;
		}

		/* See +/- counts as a sign. */
		c = cp[i];
		if ('+' == c || '-' == c || ASCII_HYPH == c)
			++i;

		break;

	/*
	 * Anything else is assumed to be a glyph.
	 */
	default:
		gly = ESCAPE_SPECIAL;
		lim = 1;
		i--;
		break;
	}

	assert(ESCAPE_ERROR != gly);

	rstart = &cp[i];
	if (start)
		*start = rstart;

	/*
	 * If a terminating block has been specified, we need to
	 * handle the case of recursion, which could have their
	 * own terminating blocks that mess up our parse.  This, by the
	 * way, means that the "start" and "size" values will be
	 * effectively meaningless.
	 */

	ssz = 0;
	if (numeric && -1 == (ssz = numescape(&cp[i])))
		return(ESCAPE_ERROR);

	i += ssz;
	rlim = -1;

	/*
	 * We have a character terminator.  Try to read up to that
	 * character.  If we can't (i.e., we hit the nil), then return
	 * an error; if we can, calculate our length, read past the
	 * terminating character, and exit.
	 */

	if ('\0' != term) {
		*end = strchr(&cp[i], term);
		if ('\0' == *end)
			return(ESCAPE_ERROR);

		rlim = *end - &cp[i];
		if (sz)
			*sz = rlim;
		(*end)++;
		goto out;
	}

	assert(lim > 0);

	/*
	 * We have a numeric limit.  If the string is shorter than that,
	 * stop and return an error.  Else adjust our endpoint, length,
	 * and return the current glyph.
	 */

	if ((size_t)lim > strlen(&cp[i]))
		return(ESCAPE_ERROR);

	rlim = lim;
	if (sz)
		*sz = rlim;

	*end = &cp[i] + lim;

out:
	assert(rlim >= 0 && rstart);

	/* Run post-processors. */

	switch (gly) {
	case (ESCAPE_FONT):
		/*
		 * Pretend that the constant-width font modes are the
		 * same as the regular font modes.
		 */
		if (2 == rlim && 'C' == *rstart)
			rstart++;
		else if (1 != rlim)
			break;

		switch (*rstart) {
		case ('3'):
			/* FALLTHROUGH */
		case ('B'):
			gly = ESCAPE_FONTBOLD;
			break;
		case ('2'):
			/* FALLTHROUGH */
		case ('I'):
			gly = ESCAPE_FONTITALIC;
			break;
		case ('P'):
			gly = ESCAPE_FONTPREV;
			break;
		case ('1'):
			/* FALLTHROUGH */
		case ('R'):
			gly = ESCAPE_FONTROMAN;
			break;
		}
		break;
	case (ESCAPE_SPECIAL):
		if (1 != rlim)
			break;
		if ('c' == *rstart)
			gly = ESCAPE_NOSPACE;
		break;
	default:
		break;
	}

	return(gly);
}

void *
mandoc_calloc(size_t num, size_t size)
{
	void		*ptr;

	ptr = calloc(num, size);
	if (NULL == ptr) {
		perror(NULL);
		exit((int)MANDOCLEVEL_SYSERR);
	}

	return(ptr);
}


void *
mandoc_malloc(size_t size)
{
	void		*ptr;

	ptr = malloc(size);
	if (NULL == ptr) {
		perror(NULL);
		exit((int)MANDOCLEVEL_SYSERR);
	}

	return(ptr);
}


void *
mandoc_realloc(void *ptr, size_t size)
{

	ptr = realloc(ptr, size);
	if (NULL == ptr) {
		perror(NULL);
		exit((int)MANDOCLEVEL_SYSERR);
	}

	return(ptr);
}

char *
mandoc_strndup(const char *ptr, size_t sz)
{
	char		*p;

	p = mandoc_malloc(sz + 1);
	memcpy(p, ptr, sz);
	p[(int)sz] = '\0';
	return(p);
}

char *
mandoc_strdup(const char *ptr)
{
	char		*p;

	p = strdup(ptr);
	if (NULL == p) {
		perror(NULL);
		exit((int)MANDOCLEVEL_SYSERR);
	}

	return(p);
}

/*
 * Parse a quoted or unquoted roff-style request or macro argument.
 * Return a pointer to the parsed argument, which is either the original
 * pointer or advanced by one byte in case the argument is quoted.
 * Null-terminate the argument in place.
 * Collapse pairs of quotes inside quoted arguments.
 * Advance the argument pointer to the next argument,
 * or to the null byte terminating the argument line.
 */
char *
mandoc_getarg(struct mparse *parse, char **cpp, int ln, int *pos)
{
	char	 *start, *cp;
	int	  quoted, pairs, white;

	/* Quoting can only start with a new word. */
	start = *cpp;
	quoted = 0;
	if ('"' == *start) {
		quoted = 1;
		start++;
	} 

	pairs = 0;
	white = 0;
	for (cp = start; '\0' != *cp; cp++) {
		/* Move left after quoted quotes and escaped backslashes. */
		if (pairs)
			cp[-pairs] = cp[0];
		if ('\\' == cp[0]) {
			if ('\\' == cp[1]) {
				/* Poor man's copy mode. */
				pairs++;
				cp++;
			} else if (0 == quoted && ' ' == cp[1])
				/* Skip escaped blanks. */
				cp++;
		} else if (0 == quoted) {
			if (' ' == cp[0]) {
				/* Unescaped blanks end unquoted args. */
				white = 1;
				break;
			}
		} else if ('"' == cp[0]) {
			if ('"' == cp[1]) {
				/* Quoted quotes collapse. */
				pairs++;
				cp++;
			} else {
				/* Unquoted quotes end quoted args. */
				quoted = 2;
				break;
			}
		}
	}

	/* Quoted argument without a closing quote. */
	if (1 == quoted)
		mandoc_msg(MANDOCERR_BADQUOTE, parse, ln, *pos, NULL);

	/* Null-terminate this argument and move to the next one. */
	if (pairs)
		cp[-pairs] = '\0';
	if ('\0' != *cp) {
		*cp++ = '\0';
		while (' ' == *cp)
			cp++;
	}
	*pos += (int)(cp - start) + (quoted ? 1 : 0);
	*cpp = cp;

	if ('\0' == *cp && (white || ' ' == cp[-1]))
		mandoc_msg(MANDOCERR_EOLNSPACE, parse, ln, *pos, NULL);

	return(start);
}

static int
a2time(time_t *t, const char *fmt, const char *p)
{
	struct tm	 tm;
	char		*pp;

	memset(&tm, 0, sizeof(struct tm));

	pp = NULL;
#ifdef	HAVE_STRPTIME
	pp = strptime(p, fmt, &tm);
#endif
	if (NULL != pp && '\0' == *pp) {
		*t = mktime(&tm);
		return(1);
	}

	return(0);
}

static char *
time2a(time_t t)
{
	struct tm	*tm;
	char		*buf, *p;
	size_t		 ssz;
	int		 isz;

	tm = localtime(&t);

	/*
	 * Reserve space:
	 * up to 9 characters for the month (September) + blank
	 * up to 2 characters for the day + comma + blank
	 * 4 characters for the year and a terminating '\0'
	 */
	p = buf = mandoc_malloc(10 + 4 + 4 + 1);

	if (0 == (ssz = strftime(p, 10 + 1, "%B ", tm)))
		goto fail;
	p += (int)ssz;

	if (-1 == (isz = snprintf(p, 4 + 1, "%d, ", tm->tm_mday)))
		goto fail;
	p += isz;

	if (0 == strftime(p, 4 + 1, "%Y", tm))
		goto fail;
	return(buf);

fail:
	free(buf);
	return(NULL);
}

char *
mandoc_normdate(struct mparse *parse, char *in, int ln, int pos)
{
	char		*out;
	time_t		 t;

	if (NULL == in || '\0' == *in ||
	    0 == strcmp(in, "$" "Mdocdate$")) {
		mandoc_msg(MANDOCERR_NODATE, parse, ln, pos, NULL);
		time(&t);
	}
	else if (a2time(&t, "%Y-%m-%d", in))
		t = 0;
	else if (!a2time(&t, "$" "Mdocdate: %b %d %Y $", in) &&
	    !a2time(&t, "%b %d, %Y", in)) {
		mandoc_msg(MANDOCERR_BADDATE, parse, ln, pos, NULL);
		t = 0;
	}
	out = t ? time2a(t) : NULL;
	return(out ? out : mandoc_strdup(in));
}

int
mandoc_eos(const char *p, size_t sz, int enclosed)
{
	const char *q;
	int found;

	if (0 == sz)
		return(0);

	/*
	 * End-of-sentence recognition must include situations where
	 * some symbols, such as `)', allow prior EOS punctuation to
	 * propagate outward.
	 */

	found = 0;
	for (q = p + (int)sz - 1; q >= p; q--) {
		switch (*q) {
		case ('\"'):
			/* FALLTHROUGH */
		case ('\''):
			/* FALLTHROUGH */
		case (']'):
			/* FALLTHROUGH */
		case (')'):
			if (0 == found)
				enclosed = 1;
			break;
		case ('.'):
			/* FALLTHROUGH */
		case ('!'):
			/* FALLTHROUGH */
		case ('?'):
			found = 1;
			break;
		default:
			return(found && (!enclosed || isalnum((unsigned char)*q)));
		}
	}

	return(found && !enclosed);
}

/*
 * Find out whether a line is a macro line or not.  If it is, adjust the
 * current position and return one; if it isn't, return zero and don't
 * change the current position.
 */
int
mandoc_getcontrol(const char *cp, int *ppos)
{
	int		pos;

	pos = *ppos;

	if ('\\' == cp[pos] && '.' == cp[pos + 1])
		pos += 2;
	else if ('.' == cp[pos] || '\'' == cp[pos])
		pos++;
	else
		return(0);

	while (' ' == cp[pos] || '\t' == cp[pos])
		pos++;

	*ppos = pos;
	return(1);
}

/*
 * Convert a string to a long that may not be <0.
 * If the string is invalid, or is less than 0, return -1.
 */
int
mandoc_strntoi(const char *p, size_t sz, int base)
{
	char		 buf[32];
	char		*ep;
	long		 v;

	if (sz > 31)
		return(-1);

	memcpy(buf, p, sz);
	buf[(int)sz] = '\0';

	errno = 0;
	v = strtol(buf, &ep, base);

	if (buf[0] == '\0' || *ep != '\0')
		return(-1);

	if (v > INT_MAX)
		v = INT_MAX;
	if (v < INT_MIN)
		v = INT_MIN;

	return((int)v);
}
