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
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#if defined(NLS)
#include <nl_types.h>
#endif

#include <limits.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#define	UPREFIX		"Unknown signal"

/* XXX: negative 'num' ? (REGR) */
char *
strsignal(int num)
{
	static char ebuf[NL_TEXTMAX];
	char tmp[20];
	int signum, n;
	char *t, *p;

#if defined(NLS)
	int saved_errno = errno;
	nl_catd catd;
	catd = catopen("libc", NL_CAT_LOCALE);
#endif

	if (num > 0 && num < sys_nsig) {
		strlcpy(ebuf,
#if defined(NLS)
			catgets(catd, 2, num, sys_siglist[num]),
#else
			sys_siglist[num],
#endif
			sizeof(ebuf));
	} else {
		n = strlcpy(ebuf,
#if defined(NLS)
			catgets(catd, 2, 0xffff, UPREFIX),
#else
			UPREFIX,
#endif
			sizeof(ebuf));
	}

	signum = num;
	if (num < 0)
		signum = -signum;

	t = tmp;
	do {
		*t++ = "0123456789"[signum % 10];
	} while (signum /= 10);
	if (num < 0)
		*t++ = '-';

	p = (ebuf + n);
	*p++ = ':';
	*p++ = ' ';

	for (;;) {
		*p++ = *--t;
		if (t <= tmp)
			break;
	}
	*p = '\0';

#if defined(NLS)
	catclose(catd);
	errno = saved_errno;
#endif
	return (ebuf);
}
