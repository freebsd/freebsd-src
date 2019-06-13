/*	$Id: mandoc.c,v 1.114 2018/12/30 00:49:55 schwarze Exp $ */
/*
 * Copyright (c) 2008-2011, 2014 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011-2015, 2017, 2018 Ingo Schwarze <schwarze@openbsd.org>
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
#include "config.h"

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "mandoc_aux.h"
#include "mandoc.h"
#include "roff.h"
#include "libmandoc.h"
#include "roff_int.h"

static	int	 a2time(time_t *, const char *, const char *);
static	char	*time2a(time_t);


enum mandoc_esc
mandoc_font(const char *cp, int sz)
{
	switch (sz) {
	case 0:
		return ESCAPE_FONTPREV;
	case 1:
		switch (cp[0]) {
		case 'B':
		case '3':
			return ESCAPE_FONTBOLD;
		case 'I':
		case '2':
			return ESCAPE_FONTITALIC;
		case 'P':
			return ESCAPE_FONTPREV;
		case 'R':
		case '1':
			return ESCAPE_FONTROMAN;
		case '4':
			return ESCAPE_FONTBI;
		default:
			return ESCAPE_ERROR;
		}
	case 2:
		switch (cp[0]) {
		case 'B':
			switch (cp[1]) {
			case 'I':
				return ESCAPE_FONTBI;
			default:
				return ESCAPE_ERROR;
			}
		case 'C':
			switch (cp[1]) {
			case 'B':
				return ESCAPE_FONTBOLD;
			case 'I':
				return ESCAPE_FONTITALIC;
			case 'R':
			case 'W':
				return ESCAPE_FONTCW;
			default:
				return ESCAPE_ERROR;
			}
		default:
			return ESCAPE_ERROR;
		}
	default:
		return ESCAPE_ERROR;
	}
}

enum mandoc_esc
mandoc_escape(const char **end, const char **start, int *sz)
{
	const char	*local_start;
	int		 local_sz, c, i;
	char		 term;
	enum mandoc_esc	 gly;

	/*
	 * When the caller doesn't provide return storage,
	 * use local storage.
	 */

	if (NULL == start)
		start = &local_start;
	if (NULL == sz)
		sz = &local_sz;

	/*
	 * Treat "\E" just like "\";
	 * it only makes a difference in copy mode.
	 */

	if (**end == 'E')
		++*end;

	/*
	 * Beyond the backslash, at least one input character
	 * is part of the escape sequence.  With one exception
	 * (see below), that character won't be returned.
	 */

	gly = ESCAPE_ERROR;
	*start = ++*end;
	*sz = 0;
	term = '\0';

	switch ((*start)[-1]) {
	/*
	 * First the glyphs.  There are several different forms of
	 * these, but each eventually returns a substring of the glyph
	 * name.
	 */
	case '(':
		gly = ESCAPE_SPECIAL;
		*sz = 2;
		break;
	case '[':
		if (**start == ' ') {
			++*end;
			return ESCAPE_ERROR;
		}
		gly = ESCAPE_SPECIAL;
		term = ']';
		break;
	case 'C':
		if ('\'' != **start)
			return ESCAPE_ERROR;
		*start = ++*end;
		gly = ESCAPE_SPECIAL;
		term = '\'';
		break;

	/*
	 * Escapes taking no arguments at all.
	 */
	case '!':
	case '?':
		return ESCAPE_UNSUPP;
	case '%':
	case '&':
	case ')':
	case ',':
	case '/':
	case '^':
	case 'a':
	case 'd':
	case 'r':
	case 't':
	case 'u':
	case '{':
	case '|':
	case '}':
		return ESCAPE_IGNORE;
	case 'c':
		return ESCAPE_NOSPACE;
	case 'p':
		return ESCAPE_BREAK;

	/*
	 * The \z escape is supposed to output the following
	 * character without advancing the cursor position.
	 * Since we are mostly dealing with terminal mode,
	 * let us just skip the next character.
	 */
	case 'z':
		return ESCAPE_SKIPCHAR;

	/*
	 * Handle all triggers matching \X(xy, \Xx, and \X[xxxx], where
	 * 'X' is the trigger.  These have opaque sub-strings.
	 */
	case 'F':
	case 'f':
	case 'g':
	case 'k':
	case 'M':
	case 'm':
	case 'n':
	case 'O':
	case 'V':
	case 'Y':
		gly = (*start)[-1] == 'f' ? ESCAPE_FONT : ESCAPE_IGNORE;
		switch (**start) {
		case '(':
			if ((*start)[-1] == 'O')
				gly = ESCAPE_ERROR;
			*start = ++*end;
			*sz = 2;
			break;
		case '[':
			if ((*start)[-1] == 'O')
				gly = (*start)[1] == '5' ?
				    ESCAPE_UNSUPP : ESCAPE_ERROR;
			*start = ++*end;
			term = ']';
			break;
		default:
			if ((*start)[-1] == 'O') {
				switch (**start) {
				case '0':
					gly = ESCAPE_UNSUPP;
					break;
				case '1':
				case '2':
				case '3':
				case '4':
					break;
				default:
					gly = ESCAPE_ERROR;
					break;
				}
			}
			*sz = 1;
			break;
		}
		break;
	case '*':
		if (strncmp(*start, "(.T", 3) != 0)
			abort();
		gly = ESCAPE_DEVICE;
		*start = ++*end;
		*sz = 2;
		break;

	/*
	 * These escapes are of the form \X'Y', where 'X' is the trigger
	 * and 'Y' is any string.  These have opaque sub-strings.
	 * The \B and \w escapes are handled in roff.c, roff_res().
	 */
	case 'A':
	case 'b':
	case 'D':
	case 'R':
	case 'X':
	case 'Z':
		gly = ESCAPE_IGNORE;
		/* FALLTHROUGH */
	case 'o':
		if (**start == '\0')
			return ESCAPE_ERROR;
		if (gly == ESCAPE_ERROR)
			gly = ESCAPE_OVERSTRIKE;
		term = **start;
		*start = ++*end;
		break;

	/*
	 * These escapes are of the form \X'N', where 'X' is the trigger
	 * and 'N' resolves to a numerical expression.
	 */
	case 'h':
	case 'H':
	case 'L':
	case 'l':
	case 'S':
	case 'v':
	case 'x':
		if (strchr(" %&()*+-./0123456789:<=>", **start)) {
			if ('\0' != **start)
				++*end;
			return ESCAPE_ERROR;
		}
		switch ((*start)[-1]) {
		case 'h':
			gly = ESCAPE_HORIZ;
			break;
		case 'l':
			gly = ESCAPE_HLINE;
			break;
		default:
			gly = ESCAPE_IGNORE;
			break;
		}
		term = **start;
		*start = ++*end;
		break;

	/*
	 * Special handling for the numbered character escape.
	 * XXX Do any other escapes need similar handling?
	 */
	case 'N':
		if ('\0' == **start)
			return ESCAPE_ERROR;
		(*end)++;
		if (isdigit((unsigned char)**start)) {
			*sz = 1;
			return ESCAPE_IGNORE;
		}
		(*start)++;
		while (isdigit((unsigned char)**end))
			(*end)++;
		*sz = *end - *start;
		if ('\0' != **end)
			(*end)++;
		return ESCAPE_NUMBERED;

	/*
	 * Sizes get a special category of their own.
	 */
	case 's':
		gly = ESCAPE_IGNORE;

		/* See +/- counts as a sign. */
		if ('+' == **end || '-' == **end || ASCII_HYPH == **end)
			*start = ++*end;

		switch (**end) {
		case '(':
			*start = ++*end;
			*sz = 2;
			break;
		case '[':
			*start = ++*end;
			term = ']';
			break;
		case '\'':
			*start = ++*end;
			term = '\'';
			break;
		case '3':
		case '2':
		case '1':
			*sz = (*end)[-1] == 's' &&
			    isdigit((unsigned char)(*end)[1]) ? 2 : 1;
			break;
		default:
			*sz = 1;
			break;
		}

		break;

	/*
	 * Several special characters can be encoded as
	 * one-byte escape sequences without using \[].
	 */
	case ' ':
	case '\'':
	case '-':
	case '.':
	case '0':
	case ':':
	case '_':
	case '`':
	case 'e':
	case '~':
		gly = ESCAPE_SPECIAL;
		/* FALLTHROUGH */
	default:
		if (gly == ESCAPE_ERROR)
			gly = ESCAPE_UNDEF;
		*start = --*end;
		*sz = 1;
		break;
	}

	/*
	 * Read up to the terminating character,
	 * paying attention to nested escapes.
	 */

	if ('\0' != term) {
		while (**end != term) {
			switch (**end) {
			case '\0':
				return ESCAPE_ERROR;
			case '\\':
				(*end)++;
				if (ESCAPE_ERROR ==
				    mandoc_escape(end, NULL, NULL))
					return ESCAPE_ERROR;
				break;
			default:
				(*end)++;
				break;
			}
		}
		*sz = (*end)++ - *start;

		/*
		 * The file chars.c only provides one common list
		 * of character names, but \[-] == \- is the only
		 * one of the characters with one-byte names that
		 * allows enclosing the name in brackets.
		 */
		if (gly == ESCAPE_SPECIAL && *sz == 1 && **start != '-')
			return ESCAPE_ERROR;
	} else {
		assert(*sz > 0);
		if ((size_t)*sz > strlen(*start))
			return ESCAPE_ERROR;
		*end += *sz;
	}

	/* Run post-processors. */

	switch (gly) {
	case ESCAPE_FONT:
		gly = mandoc_font(*start, *sz);
		break;
	case ESCAPE_SPECIAL:
		if (**start == 'c') {
			if (*sz < 6 || *sz > 7 ||
			    strncmp(*start, "char", 4) != 0 ||
			    (int)strspn(*start + 4, "0123456789") + 4 < *sz)
				break;
			c = 0;
			for (i = 4; i < *sz; i++)
				c = 10 * c + ((*start)[i] - '0');
			if (c < 0x21 || (c > 0x7e && c < 0xa0) || c > 0xff)
				break;
			*start += 4;
			*sz -= 4;
			gly = ESCAPE_NUMBERED;
			break;
		}

		/*
		 * Unicode escapes are defined in groff as \[u0000]
		 * to \[u10FFFF], where the contained value must be
		 * a valid Unicode codepoint.  Here, however, only
		 * check the length and range.
		 */
		if (**start != 'u' || *sz < 5 || *sz > 7)
			break;
		if (*sz == 7 && ((*start)[1] != '1' || (*start)[2] != '0'))
			break;
		if (*sz == 6 && (*start)[1] == '0')
			break;
		if (*sz == 5 && (*start)[1] == 'D' &&
		    strchr("89ABCDEF", (*start)[2]) != NULL)
			break;
		if ((int)strspn(*start + 1, "0123456789ABCDEFabcdef")
		    + 1 == *sz)
			gly = ESCAPE_UNICODE;
		break;
	default:
		break;
	}

	return gly;
}

static int
a2time(time_t *t, const char *fmt, const char *p)
{
	struct tm	 tm;
	char		*pp;

	memset(&tm, 0, sizeof(struct tm));

	pp = NULL;
#if HAVE_STRPTIME
	pp = strptime(p, fmt, &tm);
#endif
	if (NULL != pp && '\0' == *pp) {
		*t = mktime(&tm);
		return 1;
	}

	return 0;
}

static char *
time2a(time_t t)
{
	struct tm	*tm;
	char		*buf, *p;
	size_t		 ssz;
	int		 isz;

	tm = localtime(&t);
	if (tm == NULL)
		return NULL;

	/*
	 * Reserve space:
	 * up to 9 characters for the month (September) + blank
	 * up to 2 characters for the day + comma + blank
	 * 4 characters for the year and a terminating '\0'
	 */

	p = buf = mandoc_malloc(10 + 4 + 4 + 1);

	if ((ssz = strftime(p, 10 + 1, "%B ", tm)) == 0)
		goto fail;
	p += (int)ssz;

	/*
	 * The output format is just "%d" here, not "%2d" or "%02d".
	 * That's also the reason why we can't just format the
	 * date as a whole with "%B %e, %Y" or "%B %d, %Y".
	 * Besides, the present approach is less prone to buffer
	 * overflows, in case anybody should ever introduce the bug
	 * of looking at LC_TIME.
	 */

	if ((isz = snprintf(p, 4 + 1, "%d, ", tm->tm_mday)) == -1)
		goto fail;
	p += isz;

	if (strftime(p, 4 + 1, "%Y", tm) == 0)
		goto fail;
	return buf;

fail:
	free(buf);
	return NULL;
}

char *
mandoc_normdate(struct roff_man *man, char *in, int ln, int pos)
{
	char		*cp;
	time_t		 t;

	/* No date specified: use today's date. */

	if (in == NULL || *in == '\0' || strcmp(in, "$" "Mdocdate$") == 0) {
		mandoc_msg(MANDOCERR_DATE_MISSING, ln, pos, NULL);
		return time2a(time(NULL));
	}

	/* Valid mdoc(7) date format. */

	if (a2time(&t, "$" "Mdocdate: %b %d %Y $", in) ||
	    a2time(&t, "%b %d, %Y", in)) {
		cp = time2a(t);
		if (t > time(NULL) + 86400)
			mandoc_msg(MANDOCERR_DATE_FUTURE, ln, pos, "%s", cp);
		else if (*in != '$' && strcmp(in, cp) != 0)
			mandoc_msg(MANDOCERR_DATE_NORM, ln, pos, "%s", cp);
		return cp;
	}

	/* In man(7), do not warn about the legacy format. */

	if (a2time(&t, "%Y-%m-%d", in) == 0)
		mandoc_msg(MANDOCERR_DATE_BAD, ln, pos, "%s", in);
	else if (t > time(NULL) + 86400)
		mandoc_msg(MANDOCERR_DATE_FUTURE, ln, pos, "%s", in);
	else if (man->meta.macroset == MACROSET_MDOC)
		mandoc_msg(MANDOCERR_DATE_LEGACY, ln, pos, "Dd %s", in);

	/* Use any non-mdoc(7) date verbatim. */

	return mandoc_strdup(in);
}

int
mandoc_eos(const char *p, size_t sz)
{
	const char	*q;
	int		 enclosed, found;

	if (0 == sz)
		return 0;

	/*
	 * End-of-sentence recognition must include situations where
	 * some symbols, such as `)', allow prior EOS punctuation to
	 * propagate outward.
	 */

	enclosed = found = 0;
	for (q = p + (int)sz - 1; q >= p; q--) {
		switch (*q) {
		case '\"':
		case '\'':
		case ']':
		case ')':
			if (0 == found)
				enclosed = 1;
			break;
		case '.':
		case '!':
		case '?':
			found = 1;
			break;
		default:
			return found &&
			    (!enclosed || isalnum((unsigned char)*q));
		}
	}

	return found && !enclosed;
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
		return -1;

	memcpy(buf, p, sz);
	buf[(int)sz] = '\0';

	errno = 0;
	v = strtol(buf, &ep, base);

	if (buf[0] == '\0' || *ep != '\0')
		return -1;

	if (v > INT_MAX)
		v = INT_MAX;
	if (v < INT_MIN)
		v = INT_MIN;

	return (int)v;
}
