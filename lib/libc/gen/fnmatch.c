/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)fnmatch.c	5.6 (Berkeley) 6/28/92";
#endif /* LIBC_SCCS and not lint */

/*
 * Function fnmatch() as proposed in POSIX 1003.2 B.6 (D11.2).
 * Compares a filename or pathname to a pattern.
 */

#include <fnmatch.h>
#include <string.h>

#define	EOS	'\0'

static const char *rangematch __P((const char *, int));

fnmatch(pattern, string, flags)
	register const char *pattern, *string;
	int flags;
{
	register char c;
	char test;

	for (;;)
		switch (c = *pattern++) {
		case EOS:
			return (*string == EOS ? 0 : FNM_NOMATCH);
		case '?':
			if ((test = *string++) == EOS ||
			    test == '/' && flags & FNM_PATHNAME)
				return (FNM_NOMATCH);
			break;
		case '*':
			c = *pattern;
			/* Collapse multiple stars. */
			while (c == '*')
				c = *++pattern;

			/* Optimize for pattern with * at end or before /. */
			if (c == EOS)
				if (flags & FNM_PATHNAME)
					return (index(string, '/') == NULL ?
					    0 : FNM_NOMATCH);
				else
					return (0);
			else if (c == '/' && flags & FNM_PATHNAME) {
				if ((string = index(string, '/')) == NULL)
					return (FNM_NOMATCH);
				break;
			}

			/* General case, use recursion. */
			while ((test = *string) != EOS) {
				if (!fnmatch(pattern, string, flags))
					return (0);
				if (test == '/' && flags & FNM_PATHNAME)
					break;
				++string;
			}
			return (FNM_NOMATCH);
		case '[':
			if ((test = *string++) == EOS ||
			    test == '/' && flags & FNM_PATHNAME)
				return (FNM_NOMATCH);
			if ((pattern = rangematch(pattern, test)) == NULL)
				return (FNM_NOMATCH);
			break;
		case '\\':
			if (!(flags & FNM_NOESCAPE)) {
				if ((c = *pattern++) == EOS) {
					c = '\\';
					--pattern;
				}
				if (c != *string++)
					return (FNM_NOMATCH);
				break;
			}
			/* FALLTHROUGH */
		default:
			if (c != *string++)
				return (FNM_NOMATCH);
			break;
		}
	/* NOTREACHED */
}

static const char *
rangematch(pattern, test)
	register const char *pattern;
	register int test;
{
	register char c, c2;
	int negate, ok;

	if (negate = (*pattern == '!'))
		++pattern;

	/*
	 * XXX
	 * TO DO: quoting
	 */
	for (ok = 0; (c = *pattern++) != ']';) {
		if (c == EOS)
			return (NULL);		/* Illegal pattern. */
		if (*pattern == '-' && (c2 = pattern[1]) != EOS && c2 != ']') {
			if (c <= test && test <= c2)
				ok = 1;
			pattern += 2;
		}
		else if (c == test)
			ok = 1;
	}
	return (ok == negate ? NULL : pattern);
}
