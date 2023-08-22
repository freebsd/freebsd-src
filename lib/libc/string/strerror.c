/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)strerror.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
#if defined(NLS)
#include <nl_types.h>
#endif

#include <limits.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "errlst.h"
#include "../locale/xlocale_private.h"
#include "libc_private.h"

/*
 * Define buffer big enough to contain delimiter (": ", 2 bytes),
 * 64-bit signed integer converted to ASCII decimal (19 bytes) with
 * optional leading sign (1 byte), and a trailing NUL.
 */
#define	EBUFSIZE	(2 + 19 + 1 + 1)

/*
 * Doing this by hand instead of linking with stdio(3) avoids bloat for
 * statically linked binaries.
 */
static void
errstr(int num, const char *uprefix, char *buf, size_t len)
{
	char *t;
	unsigned int uerr;
	char tmp[EBUFSIZE];

	t = tmp + sizeof(tmp);
	*--t = '\0';
	uerr = (num >= 0) ? num : -num;
	do {
		*--t = "0123456789"[uerr % 10];
	} while (uerr /= 10);
	if (num < 0)
		*--t = '-';
	*--t = ' ';
	*--t = ':';
	strlcpy(buf, uprefix, len);
	strlcat(buf, t, len);
}

static int
strerror_rl(int errnum, char *strerrbuf, size_t buflen, locale_t locale)
{
	int retval = 0;
#if defined(NLS)
	int saved_errno = errno;
	nl_catd catd;

	catd = __catopen_l("libc", NL_CAT_LOCALE, locale);
#endif

	if (errnum < 0 || errnum >= __hidden_sys_nerr) {
		errstr(errnum,
#if defined(NLS)
		    catgets(catd, 1, 0xffff, __uprefix),
#else
		    __uprefix,
#endif
		   strerrbuf, buflen);
		retval = EINVAL;
	} else {
		if (strlcpy(strerrbuf,
#if defined(NLS)
		    catgets(catd, 1, errnum, __hidden_sys_errlist[errnum]),
#else
		    __hidden_sys_errlist[errnum],
#endif
		    buflen) >= buflen)
			retval = ERANGE;
	}

#if defined(NLS)
	catclose(catd);
	errno = saved_errno;
#endif

	return (retval);
}

int
strerror_r(int errnum, char *strerrbuf, size_t buflen)
{
	return (strerror_rl(errnum, strerrbuf, buflen, __get_locale()));
}

char *
strerror_l(int num, locale_t locale)
{
	static _Thread_local char ebuf[NL_TEXTMAX];

	if (strerror_rl(num, ebuf, sizeof(ebuf), locale) != 0)
		errno = EINVAL;
	return (ebuf);
}

char *
strerror(int num)
{
	static char ebuf[NL_TEXTMAX];

	if (strerror_rl(num, ebuf, sizeof(ebuf), __get_locale()) != 0)
		errno = EINVAL;
	return (ebuf);
}
