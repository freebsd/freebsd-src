/*-
 * Copyright (c) 2003-2004 Tim Kientzle
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

#include "bsdtar_platform.h"
__FBSDID("$FreeBSD$");

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "bsdtar.h"

struct match {
	struct match	 *next;
	int		  matches;
	char		  pattern[1];
};

struct matching {
	struct match	 *exclusions;
	int		  exclusions_count;
	struct match	 *inclusions;
	int		  inclusions_count;
	int		  inclusions_unmatched_count;
};


static void	add_pattern(struct bsdtar *, struct match **list,
		    const char *pattern);
static int	bsdtar_fnmatch(const char *p, const char *s);
static void	initialize_matching(struct bsdtar *);
static int	match_exclusion(struct match *, const char *pathname);
static int	match_inclusion(struct match *, const char *pathname);

/*
 * The matching logic here needs to be re-thought.  I started out to
 * try to mimic gtar's matching logic, but it's not entirely
 * consistent.  In particular 'tar -t' and 'tar -x' interpret patterns
 * on the command line as anchored, but --exclude doesn't.
 */

/*
 * Utility functions to manage exclusion/inclusion patterns
 */

int
exclude(struct bsdtar *bsdtar, const char *pattern)
{
	struct matching *matching;

	if (bsdtar->matching == NULL)
		initialize_matching(bsdtar);
	matching = bsdtar->matching;
	add_pattern(bsdtar, &(matching->exclusions), pattern);
	matching->exclusions_count++;
	return (0);
}

int
exclude_from_file(struct bsdtar *bsdtar, const char *pathname)
{
	return (process_lines(bsdtar, pathname, &exclude));
}

int
include(struct bsdtar *bsdtar, const char *pattern)
{
	struct matching *matching;

	if (bsdtar->matching == NULL)
		initialize_matching(bsdtar);
	matching = bsdtar->matching;
	add_pattern(bsdtar, &(matching->inclusions), pattern);
	matching->inclusions_count++;
	matching->inclusions_unmatched_count++;
	return (0);
}

int
include_from_file(struct bsdtar *bsdtar, const char *pathname)
{
	return (process_lines(bsdtar, pathname, &include));
}

static void
add_pattern(struct bsdtar *bsdtar, struct match **list, const char *pattern)
{
	struct match *match;

	match = malloc(sizeof(*match) + strlen(pattern) + 1);
	if (match == NULL)
		bsdtar_errc(bsdtar, 1, errno, "Out of memory");
	if (pattern[0] == '/')
		pattern++;
	strcpy(match->pattern, pattern);
	/* Both "foo/" and "foo" should match "foo/bar". */
	if (match->pattern[strlen(match->pattern)-1] == '/')
		match->pattern[strlen(match->pattern)-1] = '\0';
	match->next = *list;
	*list = match;
	match->matches = 0;
}


int
excluded(struct bsdtar *bsdtar, const char *pathname)
{
	struct matching *matching;
	struct match *match;
	struct match *matched;

	matching = bsdtar->matching;
	if (matching == NULL)
		return (0);

	/* Exclusions take priority */
	for (match = matching->exclusions; match != NULL; match = match->next){
		if (match_exclusion(match, pathname))
			return (1);
	}

	/* Then check for inclusions */
	matched = NULL;
	for (match = matching->inclusions; match != NULL; match = match->next){
		if (match_inclusion(match, pathname)) {
			/*
			 * If this pattern has never been matched,
			 * then we're done.
			 */
			if (match->matches == 0) {
				match->matches++;
				matching->inclusions_unmatched_count++;
				return (0);
			}
			/*
			 * Otherwise, remember the match but keep checking
			 * in case we can tick off an unmatched pattern.
			 */
			matched = match;
		}
	}
	/*
	 * We didn't find a pattern that had never been matched, but
	 * we did find a match, so count it and exit.
	 */
	if (matched != NULL) {
		matched->matches++;
		return (0);
	}

	/* If there were inclusions, default is to exclude. */
	if (matching->inclusions != NULL)
	    return (1);

	/* No explicit inclusions, default is to match. */
	return (0);
}

/*
 * This is a little odd, but it matches the default behavior of
 * gtar.  In particular, 'a*b' will match 'foo/a1111/222b/bar'
 *
 */
int
match_exclusion(struct match *match, const char *pathname)
{
	const char *p;

	if (*match->pattern == '*' || *match->pattern == '/')
		return (bsdtar_fnmatch(match->pattern, pathname) == 0);

	for (p = pathname; p != NULL; p = strchr(p, '/')) {
		if (*p == '/')
			p++;
		if (bsdtar_fnmatch(match->pattern, p) == 0)
			return (1);
	}
	return (0);
}

/*
 * Again, mimic gtar:  inclusions are always anchored (have to match
 * the beginning of the path) even though exclusions are not anchored.
 */
int
match_inclusion(struct match *match, const char *pathname)
{
	return (bsdtar_fnmatch(match->pattern, pathname) == 0);
}

void
cleanup_exclusions(struct bsdtar *bsdtar)
{
	struct match *p, *q;

	if (bsdtar->matching) {
		p = bsdtar->matching->inclusions;
		while (p != NULL) {
			q = p;
			p = p->next;
			free(q);
		}
		p = bsdtar->matching->exclusions;
		while (p != NULL) {
			q = p;
			p = p->next;
			free(q);
		}
		free(bsdtar->matching);
	}
}

static void
initialize_matching(struct bsdtar *bsdtar)
{
	bsdtar->matching = malloc(sizeof(*bsdtar->matching));
	if (bsdtar->matching == NULL)
		bsdtar_errc(bsdtar, 1, errno, "No memory");
	memset(bsdtar->matching, 0, sizeof(*bsdtar->matching));
}

int
unmatched_inclusions(struct bsdtar *bsdtar)
{
	struct matching *matching;

	matching = bsdtar->matching;
	if (matching == NULL)
		return (0);
	return (matching->inclusions_unmatched_count);
}



#if defined(HAVE_FNMATCH) && defined(HAVE_FNM_LEADING_DIR)

/* Use system fnmatch() if it suits our needs. */
#include <fnmatch.h>
static int
bsdtar_fnmatch(const char *pattern, const char *string)
{
	return (fnmatch(pattern, string, FNM_LEADING_DIR));
}

#else
/*
 * The following was hacked from BSD C library
 * code:  src/lib/libc/gen/fnmatch.c,v 1.15 2002/02/01
 *
 * In particular, most of the flags were ripped out: this always
 * behaves like FNM_LEADING_DIR is set and other flags specified
 * by POSIX are unset.
 *
 * Normally, I would not conditionally compile something like this: If
 * I have to support it anyway, everyone may as well use it. ;-)
 * However, the full POSIX spec for fnmatch() includes a lot of
 * advanced character handling that I'm not ready to put in here, so
 * it's probably best if people use a local version when it's available.
 */

/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Guido van Rossum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

static int
bsdtar_fnmatch(const char *pattern, const char *string)
{
	const char *saved_pattern;
	int negate, matched;
	char c;

	for (;;) {
		switch (c = *pattern++) {
		case '\0':
			if (*string == '/' || *string == '\0')
				return (0);
			return (1);
		case '?':
			if (*string == '\0')
				return (1);
			++string;
			break;
		case '*':
			c = *pattern;
			/* Collapse multiple stars. */
			while (c == '*')
				c = *++pattern;

			/* Optimize for pattern with * at end. */
			if (c == '\0')
				return (0);

			/* General case, use recursion. */
			while (*string != '\0') {
				if (!bsdtar_fnmatch(pattern, string))
					return (0);
				++string;
			}
			return (1);
		case '[':
			if (*string == '\0')
				return (1);
			saved_pattern = pattern;
			if (*pattern == '!' || *pattern == '^') {
				negate = 1;
				++pattern;
			} else
				negate = 0;
			matched = 0;
			c = *pattern++;
			do {
				if (c == '\\')
					c = *pattern++;
				if (c == '\0') {
					pattern = saved_pattern;
					c = '[';
					goto norm;
				}
				if (*pattern == '-') {
					char c2 = *(pattern + 1);
					if (c2 == '\0') {
						pattern = saved_pattern;
						c = '[';
						goto norm;
					}
					if (c2 == ']') {
						/* [a-] is not a range. */
						if (c == *string
						    || '-' == *string)
							matched = 1;
						pattern ++;
					} else {
						if (c <= *string
						    && *string <= c2)
							matched = 1;
						pattern += 2;
					}
				} else if (c == *string)
					matched = 1;
				c = *pattern++;
			} while (c != ']');
			if (matched == negate)
				return (1);
			++string;
			break;
		case '\\':
			if ((c = *pattern++) == '\0') {
				c = '\\';
				--pattern;
			}
			/* FALLTHROUGH */
		default:
		norm:
			if (c != *string)
				return (1);
			string++;
			break;
		}
	}
	/* NOTREACHED */
}

#endif
