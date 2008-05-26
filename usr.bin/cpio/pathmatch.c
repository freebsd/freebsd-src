/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "cpio_platform.h"
__FBSDID("$FreeBSD$");

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "pathmatch.h"

/*
 * Check whether a character 'c' is matched by a list specification [...]:
 *    * Leading '!' negates the class.
 *    * <char>-<char> is a range of characters
 *    * \<char> removes any special meaning for <char>
 *
 * Some interesting boundary cases:
 *   a-d-e is one range (a-d) followed by two single characters - and e.
 *   \a-\d is same as a-d
 *   a\-d is three single characters: a, d, -
 *   Trailing - is not special (so [a-] is two characters a and -).
 *   Initial - is not special ([a-] is same as [-a] is same as [\\-a])
 *   This function never sees a trailing \.
 *   [] always fails
 *   [!] always succeeds
 */
static int
pm_list(const char *start, const char *end, const char c, int flags)
{
	const char *p = start;
	char rangeStart = '\0', nextRangeStart;
	int match = 1, nomatch = 0;

	/* This will be used soon... */
	(void)flags; /* UNUSED */

	/* If this is a negated class, return success for nomatch. */
	if (*p == '!' && p < end) {
		match = 0;
		nomatch = 1;
		++p;
	}

	while (p < end) {
		nextRangeStart = '\0';
		switch (*p) {
		case '-':
			/* Trailing or initial '-' is not special. */
			if ((rangeStart == '\0') || (p == end - 1)) {
				if (*p == c)
					return (match);
			} else {
				char rangeEnd = *++p;
				if (rangeEnd == '\\')
					rangeEnd = *++p;
				if ((rangeStart <= c) && (c <= rangeEnd))
					return (match);
			}
			break;
		case '\\':
			++p;
			/* Fall through */
		default:
			if (*p == c)
				return (match);
			nextRangeStart = *p; /* Possible start of range. */
		}
		rangeStart = nextRangeStart;
		++p;
	}
	return (nomatch);
}

/*
 * If s is pointing to "./", ".//", "./././" or the like, skip it.
 */
static const char *
pm_slashskip(const char *s) {
	while (*s == '.' || *s == '/') {
		if (s[0] != '/' && s[1] != '/')
			break;
		++s;
	}
	return (s);
}

static int
pm(const char *p, const char *s, int flags)
{
	const char *end;

	/*
	 * Ignore leading './', './/', '././', etc.
	 */
	if (s[0] == '.' && s[1] == '/')
		s = pm_slashskip(s + 1);
	if (p[0] == '.' && p[1] == '/')
		p = pm_slashskip(p + 1);

	for (;;) {
		switch (*p) {
		case '\0':
			if (s[0] == '/') {
				if (flags & PATHMATCH_NO_ANCHOR_END)
					return (1);
				/* "dir" == "dir/" == "dir/." */
				s = pm_slashskip(s);
				if (s[0] == '.' && s[1] == '\0')
					return (1);
			}
			return (*s == '\0');
			break;
		case '?':
			/* ? always succeds, unless we hit end of 's' */
			if (*s == '\0')
				return (0);
			break;
		case '*':
			/* "*" == "**" == "***" ... */
			while (*p == '*')
				++p;
			/* Trailing '*' always succeeds. */
			if (*p == '\0')
				return (1);
			while (*s) {
				if (pathmatch(p, s, flags))
					return (1);
				++s;
			}
			return (0);
			break;
		case '[':
			/*
			 * Find the end of the [...] character class,
			 * ignoring \] that might occur within the class.
			 */
			end = p + 1;
			while (*end != '\0' && *end != ']') {
				if (*end == '\\' && end[1] != '\0')
					++end;
				++end;
			}
			if (*end == ']') {
				/* We found [...], try to match it. */
				if (!pm_list(p + 1, end, *s, flags))
					return (0);
				p = end; /* Jump to trailing ']' char. */
				break;
			} else
				/* No final ']', so just match '['. */
				if (*p != *s)
					return (0);
			break;
		default:
			if (*p == *s)
				break;
			if ((*s == '\0') && (*p == '/')) {
				p = pm_slashskip(p);
				if (*p == '\0')
					return (1);
				if (p[0] == '.' && p[1] == '\0')
					return (1);
				return (0);
			}
			return (0);
			break;
		case '\\':
			/* Trailing '\\' matches itself. */
			if (p[1] == '\0') {
				if (*s != '\\')
					return (0);
			} else {
				++p;
				if (*p != *s)
					return (0);
			}
			break;
		}
		/*
		 * TODO: pattern of "\/\.\/" should not match plain "/",
		 * it should only match explicit "/./".
		 */
		if (*p == '/')
			p = pm_slashskip(p);
		else
			++p;
		if (*s == '/')
			s = pm_slashskip(s);
		else
			++s;
	}
}

/* Main entry point. */
int
pathmatch(const char *p, const char *s, int flags)
{
	/* Empty pattern only matches the empty string. */
	if (p == NULL || *p == '\0')
		return (s == NULL || *s == '\0');

	/* Leading '^' anchors the start of the pattern. */
	if (*p == '^') {
		++p;
		flags &= ~PATHMATCH_NO_ANCHOR_START;
	}

	/* Certain patterns anchor implicitly. */
	if (*p == '*' || *p == '/')
		return (pm(p, s, flags));

	/* If start is unanchored, try to match start of each path element. */
	if (flags & PATHMATCH_NO_ANCHOR_START) {
		for ( ; p != NULL; p = strchr(p, '/')) {
			if (*p == '/')
				p++;
			if (pm(p, s, flags))
				return (1);
		}
		return (0);
	}

	/* Default: Match from beginning. */
	return (pm(p, s, flags));
}
