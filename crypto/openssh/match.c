/* $OpenBSD: match.c,v 1.26 2006/08/03 03:34:42 deraadt Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Simple pattern matching, with '*' and '?' as wildcards.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */
/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#include <sys/types.h>

#include <ctype.h>
#include <string.h>

#include "xmalloc.h"
#include "match.h"

/*
 * Returns true if the given string matches the pattern (which may contain ?
 * and * as wildcards), and zero if it does not match.
 */

int
match_pattern(const char *s, const char *pattern)
{
	for (;;) {
		/* If at end of pattern, accept if also at end of string. */
		if (!*pattern)
			return !*s;

		if (*pattern == '*') {
			/* Skip the asterisk. */
			pattern++;

			/* If at end of pattern, accept immediately. */
			if (!*pattern)
				return 1;

			/* If next character in pattern is known, optimize. */
			if (*pattern != '?' && *pattern != '*') {
				/*
				 * Look instances of the next character in
				 * pattern, and try to match starting from
				 * those.
				 */
				for (; *s; s++)
					if (*s == *pattern &&
					    match_pattern(s + 1, pattern + 1))
						return 1;
				/* Failed. */
				return 0;
			}
			/*
			 * Move ahead one character at a time and try to
			 * match at each position.
			 */
			for (; *s; s++)
				if (match_pattern(s, pattern))
					return 1;
			/* Failed. */
			return 0;
		}
		/*
		 * There must be at least one more character in the string.
		 * If we are at the end, fail.
		 */
		if (!*s)
			return 0;

		/* Check if the next character of the string is acceptable. */
		if (*pattern != '?' && *pattern != *s)
			return 0;

		/* Move to the next character, both in string and in pattern. */
		s++;
		pattern++;
	}
	/* NOTREACHED */
}

/*
 * Tries to match the string against the
 * comma-separated sequence of subpatterns (each possibly preceded by ! to
 * indicate negation).  Returns -1 if negation matches, 1 if there is
 * a positive match, 0 if there is no match at all.
 */

int
match_pattern_list(const char *string, const char *pattern, u_int len,
    int dolower)
{
	char sub[1024];
	int negated;
	int got_positive;
	u_int i, subi;

	got_positive = 0;
	for (i = 0; i < len;) {
		/* Check if the subpattern is negated. */
		if (pattern[i] == '!') {
			negated = 1;
			i++;
		} else
			negated = 0;

		/*
		 * Extract the subpattern up to a comma or end.  Convert the
		 * subpattern to lowercase.
		 */
		for (subi = 0;
		    i < len && subi < sizeof(sub) - 1 && pattern[i] != ',';
		    subi++, i++)
			sub[subi] = dolower && isupper(pattern[i]) ?
			    (char)tolower(pattern[i]) : pattern[i];
		/* If subpattern too long, return failure (no match). */
		if (subi >= sizeof(sub) - 1)
			return 0;

		/* If the subpattern was terminated by a comma, skip the comma. */
		if (i < len && pattern[i] == ',')
			i++;

		/* Null-terminate the subpattern. */
		sub[subi] = '\0';

		/* Try to match the subpattern against the string. */
		if (match_pattern(string, sub)) {
			if (negated)
				return -1;		/* Negative */
			else
				got_positive = 1;	/* Positive */
		}
	}

	/*
	 * Return success if got a positive match.  If there was a negative
	 * match, we have already returned -1 and never get here.
	 */
	return got_positive;
}

/*
 * Tries to match the host name (which must be in all lowercase) against the
 * comma-separated sequence of subpatterns (each possibly preceded by ! to
 * indicate negation).  Returns -1 if negation matches, 1 if there is
 * a positive match, 0 if there is no match at all.
 */
int
match_hostname(const char *host, const char *pattern, u_int len)
{
	return match_pattern_list(host, pattern, len, 1);
}

/*
 * returns 0 if we get a negative match for the hostname or the ip
 * or if we get no match at all.  returns 1 otherwise.
 */
int
match_host_and_ip(const char *host, const char *ipaddr,
    const char *patterns)
{
	int mhost, mip;

	/* negative ipaddr match */
	if ((mip = match_hostname(ipaddr, patterns, strlen(patterns))) == -1)
		return 0;
	/* negative hostname match */
	if ((mhost = match_hostname(host, patterns, strlen(patterns))) == -1)
		return 0;
	/* no match at all */
	if (mhost == 0 && mip == 0)
		return 0;
	return 1;
}

/*
 * match user, user@host_or_ip, user@host_or_ip_list against pattern
 */
int
match_user(const char *user, const char *host, const char *ipaddr,
    const char *pattern)
{
	char *p, *pat;
	int ret;

	if ((p = strchr(pattern,'@')) == NULL)
		return match_pattern(user, pattern);

	pat = xstrdup(pattern);
	p = strchr(pat, '@');
	*p++ = '\0';

	if ((ret = match_pattern(user, pat)) == 1)
		ret = match_host_and_ip(host, ipaddr, p);
	xfree(pat);

	return ret;
}

/*
 * Returns first item from client-list that is also supported by server-list,
 * caller must xfree() returned string.
 */
#define	MAX_PROP	40
#define	SEP	","
char *
match_list(const char *client, const char *server, u_int *next)
{
	char *sproposals[MAX_PROP];
	char *c, *s, *p, *ret, *cp, *sp;
	int i, j, nproposals;

	c = cp = xstrdup(client);
	s = sp = xstrdup(server);

	for ((p = strsep(&sp, SEP)), i=0; p && *p != '\0';
	    (p = strsep(&sp, SEP)), i++) {
		if (i < MAX_PROP)
			sproposals[i] = p;
		else
			break;
	}
	nproposals = i;

	for ((p = strsep(&cp, SEP)), i=0; p && *p != '\0';
	    (p = strsep(&cp, SEP)), i++) {
		for (j = 0; j < nproposals; j++) {
			if (strcmp(p, sproposals[j]) == 0) {
				ret = xstrdup(p);
				if (next != NULL)
					*next = (cp == NULL) ?
					    strlen(c) : (u_int)(cp - c);
				xfree(c);
				xfree(s);
				return ret;
			}
		}
	}
	if (next != NULL)
		*next = strlen(c);
	xfree(c);
	xfree(s);
	return NULL;
}
