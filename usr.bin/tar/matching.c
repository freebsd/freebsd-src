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
#include <fnmatch.h>
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
 * XXX TODO: fnmatch isn't the most portable thing around, and even
 * worse, FNM_LEADING_DIR is a non-POSIX extension.  <sigh> Thus, the
 * following two functions need to eventually be replaced with code
 * that does not rely on fnmatch().
 */
int
match_exclusion(struct match *match, const char *pathname)
{
	const char *p;

	if (*match->pattern == '*' || *match->pattern == '/')
		return (fnmatch(match->pattern, pathname, FNM_LEADING_DIR) == 0);

	for (p = pathname; p != NULL; p = strchr(p, '/')) {
		if (*p == '/')
			p++;
		if (fnmatch(match->pattern, p, FNM_LEADING_DIR) == 0)
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
	return (fnmatch(match->pattern, pathname, FNM_LEADING_DIR) == 0);
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
