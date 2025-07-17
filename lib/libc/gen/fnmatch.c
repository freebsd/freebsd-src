/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Guido van Rossum.
 *
 * Copyright (c) 2011 The FreeBSD Foundation
 *
 * Portions of this software were developed by David Chisnall
 * under sponsorship from the FreeBSD Foundation.
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
 * Function fnmatch() as specified in POSIX 1003.2-1992, section B.6.
 * Compares a filename or pathname to a pattern.
 */

/*
 * Some notes on multibyte character support:
 * 1. Patterns with illegal byte sequences match nothing.
 * 2. Illegal byte sequences in the "string" argument are handled by treating
 *    them as single-byte characters with a value of the first byte of the
 *    sequence cast to wchar_t.
 * 3. Multibyte conversion state objects (mbstate_t) are passed around and
 *    used for most, but not all, conversions. Further work will be required
 *    to support state-dependent encodings.
 */

#include <fnmatch.h>
#include <limits.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include "collate.h"

#define	EOS	'\0'

#define RANGE_MATCH     1
#define RANGE_NOMATCH   0
#define RANGE_ERROR     (-1)

static int rangematch(const char *, wchar_t, const char *, int, char **,
    char **, mbstate_t *, mbstate_t *);
static int fnmatch1(const char *, const char *, const char *, int, mbstate_t,
		mbstate_t);

int
fnmatch(const char *pattern, const char *string, int flags)
{
	static const mbstate_t initial;

	return (fnmatch1(pattern, string, string, flags, initial, initial));
}

static int
fnmatch1(const char *pattern, const char *string, const char *stringstart,
    int flags, mbstate_t patmbs, mbstate_t strmbs)
{
	const char *bt_pattern, *bt_string;
	mbstate_t bt_patmbs, bt_strmbs;
	char *newp, *news;
	char c;
	wchar_t pc, sc;
	size_t pclen, sclen;

	bt_pattern = bt_string = NULL;
	for (;;) {
		pclen = mbrtowc(&pc, pattern, MB_LEN_MAX, &patmbs);
		if (pclen == (size_t)-1 || pclen == (size_t)-2)
			return (FNM_NOMATCH);
		pattern += pclen;
		sclen = mbrtowc(&sc, string, MB_LEN_MAX, &strmbs);
		if (sclen == (size_t)-1 || sclen == (size_t)-2) {
			sc = (unsigned char)*string;
			sclen = 1;
			memset(&strmbs, 0, sizeof(strmbs));
		}
		switch (pc) {
		case EOS:
			if ((flags & FNM_LEADING_DIR) && sc == '/')
				return (0);
			if (sc == EOS)
				return (0);
			goto backtrack;
		case '?':
			if (sc == EOS)
				return (FNM_NOMATCH);
			if (sc == '/' && (flags & FNM_PATHNAME))
				goto backtrack;
			if (sc == '.' && (flags & FNM_PERIOD) &&
			    (string == stringstart ||
			    ((flags & FNM_PATHNAME) && *(string - 1) == '/')))
				goto backtrack;
			string += sclen;
			break;
		case '*':
			c = *pattern;
			/* Collapse multiple stars. */
			while (c == '*')
				c = *++pattern;

			if (sc == '.' && (flags & FNM_PERIOD) &&
			    (string == stringstart ||
			    ((flags & FNM_PATHNAME) && *(string - 1) == '/')))
				goto backtrack;

			/* Optimize for pattern with * at end or before /. */
			if (c == EOS)
				if (flags & FNM_PATHNAME)
					return ((flags & FNM_LEADING_DIR) ||
					    strchr(string, '/') == NULL ?
					    0 : FNM_NOMATCH);
				else
					return (0);
			else if (c == '/' && flags & FNM_PATHNAME) {
				if ((string = strchr(string, '/')) == NULL)
					return (FNM_NOMATCH);
				break;
			}

			/*
			 * First try the shortest match for the '*' that
			 * could work. We can forget any earlier '*' since
			 * there is no way having it match more characters
			 * can help us, given that we are already here.
			 */
			bt_pattern = pattern;
			bt_patmbs = patmbs;
			bt_string = string;
			bt_strmbs = strmbs;
			break;
		case '[':
			if (sc == EOS)
				return (FNM_NOMATCH);
			if (sc == '/' && (flags & FNM_PATHNAME))
				goto backtrack;
			if (sc == '.' && (flags & FNM_PERIOD) &&
			    (string == stringstart ||
			    ((flags & FNM_PATHNAME) && *(string - 1) == '/')))
				goto backtrack;

			switch (rangematch(pattern, sc, string + sclen, flags,
			    &newp, &news, &patmbs, &strmbs)) {
			case RANGE_ERROR:
				goto norm;
			case RANGE_MATCH:
				pattern = newp;
				string = news;
				break;
			case RANGE_NOMATCH:
				goto backtrack;
			}
			break;
		case '\\':
			if (!(flags & FNM_NOESCAPE)) {
				pclen = mbrtowc(&pc, pattern, MB_LEN_MAX,
				    &patmbs);
				if (pclen == 0 || pclen == (size_t)-1 ||
				    pclen == (size_t)-2)
					return (FNM_NOMATCH);
				pattern += pclen;
			}
			/* FALLTHROUGH */
		default:
		norm:
			string += sclen;
			if (pc == sc)
				;
			else if ((flags & FNM_CASEFOLD) &&
				 (towlower(pc) == towlower(sc)))
				;
			else {
		backtrack:
				/*
				 * If we have a mismatch (other than hitting
				 * the end of the string), go back to the last
				 * '*' seen and have it match one additional
				 * character.
				 */
				if (bt_pattern == NULL)
					return (FNM_NOMATCH);
				sclen = mbrtowc(&sc, bt_string, MB_LEN_MAX,
				    &bt_strmbs);
				if (sclen == (size_t)-1 ||
				    sclen == (size_t)-2) {
					sc = (unsigned char)*bt_string;
					sclen = 1;
					memset(&bt_strmbs, 0,
					    sizeof(bt_strmbs));
				}
				if (sc == EOS)
					return (FNM_NOMATCH);
				if (sc == '/' && flags & FNM_PATHNAME)
					return (FNM_NOMATCH);
				bt_string += sclen;
				pattern = bt_pattern;
				patmbs = bt_patmbs;
				string = bt_string;
				strmbs = bt_strmbs;
			}
			break;
		}
	}
	/* NOTREACHED */
}

static int
rangematch(const char *pattern, wchar_t test, const char *string, int flags,
    char **newp, char **news, mbstate_t *patmbs, mbstate_t *strmbs)
{
	int negate, ok;
	wchar_t c, c2;
	size_t pclen;
	const char *origpat;
	struct xlocale_collate *table =
	    (struct xlocale_collate *)__get_locale()->components[XLC_COLLATE];
	wchar_t buf[COLLATE_STR_LEN];	/* STR_LEN defined in collate.h */
	const char *cp, *savestring;
	int special;
	mbstate_t save;
	size_t sclen, len;

	/*
	 * A bracket expression starting with an unquoted circumflex
	 * character produces unspecified results (IEEE 1003.2-1992,
	 * 3.13.2).  This implementation treats it like '!', for
	 * consistency with the regular expression syntax.
	 * J.T. Conklin (conklin@ngai.kaleida.com)
	 */
	if ((negate = (*pattern == '!' || *pattern == '^')))
		++pattern;

	if (flags & FNM_CASEFOLD)
		test = towlower(test);

	/*
	 * A right bracket shall lose its special meaning and represent
	 * itself in a bracket expression if it occurs first in the list.
	 * -- POSIX.2 2.8.3.2
	 */
	ok = 0;
	origpat = pattern;
	for (;;) {
		c = 0;
		if (*pattern == ']' && pattern > origpat) {
			break;
		} else if (*pattern == '\0') {
			return (RANGE_ERROR);
		} else if (*pattern == '/' && (flags & FNM_PATHNAME)) {
			return (RANGE_NOMATCH);
		} else if (*pattern == '\\' && !(flags & FNM_NOESCAPE)) {
			pattern++;
		} else if (*pattern == '[' &&
		    ((special = *(pattern + 1)) == '.' ||
		    special == '=' || special == ':')) {
			cp = (pattern += 2);
			while ((cp = strchr(cp, special))) {
				if (*(cp + 1) == ']')
					break;
				cp++;
			}
			if (!cp)
				return (RANGE_ERROR);
			if (special == '.') {
treat_like_collating_symbol:
				len = __collate_collating_symbol(buf,
				    COLLATE_STR_LEN, pattern,
				    cp - pattern, patmbs);
				if (len == (size_t)-1 || len == 0)
					return (RANGE_ERROR);
				pattern = cp + 2;
				if (len > 1) {
					wchar_t *wp, sc;

					/*
					 * No multi-character collation
					 * symbols as start of range.
					 */
					if (*(cp + 2) == '-' &&
					    *(cp + 3) != EOS &&
					    *(cp + 3) != ']')
						return (RANGE_ERROR);
					wp = buf;
					if (test != *wp++)
						continue;
					if (len == 1) {
						ok = 1;
						break;
					}
					memcpy(&save, strmbs, sizeof(save));
					savestring = string;
					while (--len > 0) {
						sclen = mbrtowc(&sc, string,
						    MB_LEN_MAX, strmbs);
						if (sclen == (size_t)-1 ||
						    sclen == (size_t)-2) {
							sc = (unsigned char)*string;
							sclen = 1;
							memset(&strmbs, 0,
							    sizeof(strmbs));
						}
						if (sc != *wp++) {
							memcpy(strmbs, &save,
							    sizeof(save));
							string = savestring;
							break;
						}
						string += sclen;
					}
					if (len == 0) {
						ok = 1;
						break;
					}
					continue; /* no match */
				}
				c = *buf;
			} else if (special == '=') {
				int ec;
				memcpy(&save, patmbs, sizeof(save));
				ec = __collate_equiv_class(pattern,
				    cp - pattern, patmbs);
				if (ec < 0)
					return (RANGE_ERROR);
				if (ec == 0) {
					memcpy(patmbs, &save, sizeof(save));
					goto treat_like_collating_symbol;
				}
				pattern = cp + 2;
				/* no equivalence classes as start of range */
				if (*(cp + 2) == '-' && *(cp + 3) != EOS &&
				    *(cp + 3) != ']')
					return (RANGE_ERROR);
				len = __collate_equiv_match(ec, NULL, 0, test,
				    string, strlen(string), strmbs, &sclen);
				if (len < 0)
					return (RANGE_ERROR);
				if (len > 0) {
					ok = 1;
					string += sclen;
					break;
				}
				continue;
			} else { /* special == ':' */
				wctype_t charclass;
				char name[CHARCLASS_NAME_MAX + 1];
				/* no character classes as start of range */
				if (*(cp + 2) == '-' && *(cp + 3) != EOS &&
				    *(cp + 3) != ']')
					return (RANGE_ERROR);
				/* assume character class names are ascii */
				if (cp - pattern > CHARCLASS_NAME_MAX)
					return (RANGE_ERROR);
				strlcpy(name, pattern, cp - pattern + 1);
				pattern = cp + 2;
				if ((charclass = wctype(name)) == 0)
					return (RANGE_ERROR);
				if (iswctype(test, charclass)) {
					ok = 1;
					break;
				}
				continue;
			}
		}
		if (!c) {
			pclen = mbrtowc(&c, pattern, MB_LEN_MAX, patmbs);
			if (pclen == (size_t)-1 || pclen == (size_t)-2)
				return (RANGE_NOMATCH);
			pattern += pclen;
		}
		if (flags & FNM_CASEFOLD)
			c = towlower(c);

		if (*pattern == '-' && *(pattern + 1) != EOS &&
		    *(pattern + 1) != ']') {
			if (*++pattern == '\\' && !(flags & FNM_NOESCAPE))
				if (*pattern != EOS)
					pattern++;
			pclen = mbrtowc(&c2, pattern, MB_LEN_MAX, patmbs);
			if (pclen == (size_t)-1 || pclen == (size_t)-2)
				return (RANGE_NOMATCH);
			pattern += pclen;
			if (c2 == EOS)
				return (RANGE_ERROR);

			if ((c2 == '[' && (special = *pattern) == '.') ||
			    special == '=' || special == ':') {

				/*
				 * No equivalence classes or character
				 * classes as end of range.
				 */
				if (special == '=' || special == ':')
					return (RANGE_ERROR);
				cp = ++pattern;
				while ((cp = strchr(cp, special))) {
					if (*(cp + 1) == ']')
						break;
					cp++;
				}
				if (!cp)
					return (RANGE_ERROR);
				len = __collate_collating_symbol(buf,
				    COLLATE_STR_LEN, pattern,
				    cp - pattern, patmbs);

				/*
				 * No multi-character collation symbols
				 *  as end of range.
				 */
				if (len != 1)
					return (RANGE_ERROR);
				pattern = cp + 2;
				c2 = *buf;
			}

			if (flags & FNM_CASEFOLD)
				c2 = towlower(c2);

			if (table->__collate_load_error ?
			    c <= test && test <= c2 :
			       __wcollate_range_cmp(c, test) <= 0
			    && __wcollate_range_cmp(test, c2) <= 0
			   ) {
				ok = 1;
				break;
			}
		} else if (c == test) {
			ok = 1;
			break;
		}
	}

	/* go to end of bracket expression */
	special = 0;
	while (*pattern != ']') {
		if (*pattern == 0)
			return (RANGE_ERROR);
		if (*pattern == special) {
			if (*++pattern == ']') {
				special = 0;
				pattern++;
			}
			continue;
		}
		if (!special && *pattern == '[') {
			special = *++pattern;
			if (special != '.' && special != '=' && special != ':')
				special = 0;
			else
				pattern++;
			continue;
		}
		pclen = mbrtowc(&c, pattern, MB_LEN_MAX, patmbs);
		if (pclen == (size_t)-1 || pclen == (size_t)-2)
			return (RANGE_NOMATCH);
		pattern += pclen;
	}

	*newp = (char *)++pattern;
	*news = (char *)string;

	return (ok == negate ? RANGE_NOMATCH : RANGE_MATCH);
}
