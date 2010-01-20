/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "matching.h"
#include "pathmatch.h"

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

static void	add_pattern(struct match **list, const char *pattern);
static void	initialize_matching(struct cpio *);
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
exclude(struct cpio *cpio, const char *pattern)
{
	struct matching *matching;

	if (cpio->matching == NULL)
		initialize_matching(cpio);
	matching = cpio->matching;
	add_pattern(&(matching->exclusions), pattern);
	matching->exclusions_count++;
	return (0);
}

#if 0
int
exclude_from_file(struct cpio *cpio, const char *pathname)
{
	return (process_lines(cpio, pathname, &exclude));
}
#endif

int
include(struct cpio *cpio, const char *pattern)
{
	struct matching *matching;

	if (cpio->matching == NULL)
		initialize_matching(cpio);
	matching = cpio->matching;
	add_pattern(&(matching->inclusions), pattern);
	matching->inclusions_count++;
	matching->inclusions_unmatched_count++;
	return (0);
}

int
include_from_file(struct cpio *cpio, const char *pathname)
{
	struct line_reader *lr;
	const char *p;
	int ret = 0;

	lr = process_lines_init(pathname, '\n');
	while ((p = process_lines_next(lr)) != NULL)
		if (include(cpio, p) != 0)
			ret = -1;
	process_lines_free(lr);
	return (ret);
}

static void
add_pattern(struct match **list, const char *pattern)
{
	struct match *match;

	match = malloc(sizeof(*match) + strlen(pattern) + 1);
	if (match == NULL)
		cpio_errc(1, errno, "Out of memory");
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
excluded(struct cpio *cpio, const char *pathname)
{
	struct matching *matching;
	struct match *match;
	struct match *matched;

	matching = cpio->matching;
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
	return (pathmatch(match->pattern,
		    pathname,
		    PATHMATCH_NO_ANCHOR_START | PATHMATCH_NO_ANCHOR_END));
}

/*
 * Again, mimic gtar:  inclusions are always anchored (have to match
 * the beginning of the path) even though exclusions are not anchored.
 */
int
match_inclusion(struct match *match, const char *pathname)
{
	return (pathmatch(match->pattern, pathname, 0));
}

void
cleanup_exclusions(struct cpio *cpio)
{
	struct match *p, *q;

	if (cpio->matching) {
		p = cpio->matching->inclusions;
		while (p != NULL) {
			q = p;
			p = p->next;
			free(q);
		}
		p = cpio->matching->exclusions;
		while (p != NULL) {
			q = p;
			p = p->next;
			free(q);
		}
		free(cpio->matching);
	}
}

static void
initialize_matching(struct cpio *cpio)
{
	cpio->matching = malloc(sizeof(*cpio->matching));
	if (cpio->matching == NULL)
		cpio_errc(1, errno, "No memory");
	memset(cpio->matching, 0, sizeof(*cpio->matching));
}

int
unmatched_inclusions(struct cpio *cpio)
{
	struct matching *matching;

	matching = cpio->matching;
	if (matching == NULL)
		return (0);
	return (matching->inclusions_unmatched_count);
}
