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

#include "lafe_platform.h"
__FBSDID("$FreeBSD: src/usr.bin/cpio/matching.c,v 1.2 2008/06/21 02:20:20 kientzle Exp $");

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "err.h"
#include "line_reader.h"
#include "matching.h"
#include "pathmatch.h"

struct match {
	struct match	 *next;
	int		  matches;
	char		  pattern[1];
};

struct lafe_matching {
	struct match	 *exclusions;
	int		  exclusions_count;
	struct match	 *inclusions;
	int		  inclusions_count;
	int		  inclusions_unmatched_count;
};

static void	add_pattern(struct match **list, const char *pattern);
static void	initialize_matching(struct lafe_matching **);
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
lafe_exclude(struct lafe_matching **matching, const char *pattern)
{

	if (*matching == NULL)
		initialize_matching(matching);
	add_pattern(&((*matching)->exclusions), pattern);
	(*matching)->exclusions_count++;
	return (0);
}

int
lafe_exclude_from_file(struct lafe_matching **matching, const char *pathname)
{
	struct lafe_line_reader *lr;
	const char *p;
	int ret = 0;

	lr = lafe_line_reader(pathname, 0);
	while ((p = lafe_line_reader_next(lr)) != NULL) {
		if (lafe_exclude(matching, p) != 0)
			ret = -1;
	}
	lafe_line_reader_free(lr);
	return (ret);
}

int
lafe_include(struct lafe_matching **matching, const char *pattern)
{

	if (*matching == NULL)
		initialize_matching(matching);
	add_pattern(&((*matching)->inclusions), pattern);
	(*matching)->inclusions_count++;
	(*matching)->inclusions_unmatched_count++;
	return (0);
}

int
lafe_include_from_file(struct lafe_matching **matching, const char *pathname,
    int nullSeparator)
{
	struct lafe_line_reader *lr;
	const char *p;
	int ret = 0;

	lr = lafe_line_reader(pathname, nullSeparator);
	while ((p = lafe_line_reader_next(lr)) != NULL) {
		if (lafe_include(matching, p) != 0)
			ret = -1;
	}
	lafe_line_reader_free(lr);
	return (ret);
}

static void
add_pattern(struct match **list, const char *pattern)
{
	struct match *match;
	size_t len;

	len = strlen(pattern);
	match = malloc(sizeof(*match) + len + 1);
	if (match == NULL)
		lafe_errc(1, errno, "Out of memory");
	strcpy(match->pattern, pattern);
	/* Both "foo/" and "foo" should match "foo/bar". */
	if (len && match->pattern[len - 1] == '/')
		match->pattern[strlen(match->pattern)-1] = '\0';
	match->next = *list;
	*list = match;
	match->matches = 0;
}


int
lafe_excluded(struct lafe_matching *matching, const char *pathname)
{
	struct match *match;
	struct match *matched;

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
				matching->inclusions_unmatched_count--;
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
static int
match_exclusion(struct match *match, const char *pathname)
{
	return (lafe_pathmatch(match->pattern,
		    pathname,
		    PATHMATCH_NO_ANCHOR_START | PATHMATCH_NO_ANCHOR_END));
}

/*
 * Again, mimic gtar:  inclusions are always anchored (have to match
 * the beginning of the path) even though exclusions are not anchored.
 */
static int
match_inclusion(struct match *match, const char *pathname)
{
#if 0
	return (lafe_pathmatch(match->pattern, pathname, 0));
#else
	return (lafe_pathmatch(match->pattern, pathname, PATHMATCH_NO_ANCHOR_END));
#endif	
}

void
lafe_cleanup_exclusions(struct lafe_matching **matching)
{
	struct match *p, *q;

	if (*matching == NULL)
		return;

	for (p = (*matching)->inclusions; p != NULL; ) {
		q = p;
		p = p->next;
		free(q);
	}

	for (p = (*matching)->exclusions; p != NULL; ) {
		q = p;
		p = p->next;
		free(q);
	}

	free(*matching);
	*matching = NULL;
}

static void
initialize_matching(struct lafe_matching **matching)
{
	*matching = calloc(sizeof(**matching), 1);
	if (*matching == NULL)
		lafe_errc(1, errno, "No memory");
}

int
lafe_unmatched_inclusions(struct lafe_matching *matching)
{

	if (matching == NULL)
		return (0);
	return (matching->inclusions_unmatched_count);
}

int
lafe_unmatched_inclusions_warn(struct lafe_matching *matching, const char *msg)
{
	struct match *p;

	if (matching == NULL)
		return (0);

	for (p = matching->inclusions; p != NULL; p = p->next) {
		if (p->matches == 0)
			lafe_warnc(0, "%s: %s", p->pattern, msg);
	}

	return (matching->inclusions_unmatched_count);
}
