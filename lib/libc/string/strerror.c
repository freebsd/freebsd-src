/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static char sccsid[] = "@(#)strerror.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>

int
strerror_r(int errnum, char *strerrbuf, size_t buflen)
{
	int             len;

	if ((errnum >= 0) && (errnum < sys_nerr)) {
		len = strlcpy(strerrbuf, (char *)sys_errlist[errnum], buflen);
		return ((len < buflen) ? 0 : ERANGE);
	}
	return (EINVAL);
}

char *
strerror(num)
	int             num;
{
	char           *p, *t;
	unsigned int	uerr;
	static char const unknown_prefix[] = "Unknown error: ";

	/*
	 * Define a buffer size big enough to describe a 64-bit
	 * number in ASCII decimal (19), with optional leading sign
	 * (+1) and trailing NUL (+1).
	 */
#       define		NUMLEN 21
#	define		EBUFLEN (sizeof unknown_prefix + NUMLEN)
	char            tmp[NUMLEN];	/* temporary number */
	static char     ebuf[EBUFLEN];	/* error message */

	if ((num >= 0) && (num < sys_nerr))
		return ((char *)sys_errlist[num]);

	/*
	 * Set errno to EINVAL per P1003.1-200x Draft June 14, 2001.
	 */
	errno = EINVAL;

	/*
	 * Print unknown errno by hand so we don't link to stdio(3).
	 * This collects the ASCII digits in reverse order.
	 */
	uerr = (num > 0) ? num : -num;
	t = tmp;
	do {
		*t++ = "0123456789"[uerr % 10];
	} while (uerr /= 10);
	if (num < 0)
		*t++ = '-';

	/*
	 * Copy the "unknown" message and the number into the caller
	 * supplied buffer, inverting the number string.
	 */
	strcpy(ebuf, unknown_prefix);
	for (p = ebuf + sizeof unknown_prefix - 1; t >= tmp; )
		*p++ = *--t;
	*p = '\0';
	return (ebuf);
}

#ifdef STANDALONE_TEST

#include <limits.h>

main()
{
	char            mybuf[64];
	int             ret;

	errno = 0;

	printf("strerror(0) yeilds: %s\n", strerror(0));
	printf("strerror(1) yeilds: %s\n", strerror(1));
	printf("strerror(47) yeilds: %s\n", strerror(47));
	printf("strerror(sys_nerr - 1) yeilds: %s\n", strerror(sys_nerr - 1));
	printf("errno = %d\n", errno); errno = 0;

	printf("strerror(sys_nerr) yeilds: %s\n", strerror(sys_nerr));
	printf("errno = %d\n", errno);  errno = 0;

	printf("strerror(437) yeilds: %s\n", strerror(437));
	printf("errno = %d\n", errno);  errno = 0;

	printf("strerror(LONG_MAX) yeilds: %s\n", strerror(LONG_MAX));
	printf("strerror(LONG_MIN) yeilds: %s\n", strerror(LONG_MIN));
	printf("strerror(ULONG_MAX) yeilds: %s\n", strerror(ULONG_MAX));

	memset(mybuf, '*', 63); mybuf[63] = '\0';
	strerror_r(11, mybuf, 64);
	printf("strerror_r(11) yeilds: %s\n", mybuf);

	memset(mybuf, '*', 63); mybuf[63] = '\0';
	ret = strerror_r(1234, mybuf, 64);
	printf("strerror_r(1234) returns %d (%s)\n", ret, mybuf);

	memset(mybuf, '*', 63); mybuf[63] = '\0';
	ret = strerror_r(1, mybuf, 10);
	printf("strerror_r on short buffer returns %d (%s)\n", ret, mybuf);
}
#endif
