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

#include <stdio.h>
#include <string.h>
#include <errno.h>


int
strerror_r(int errnum, char *strerrbuf, size_t buflen)
{
#define	UPREFIX	"Unknown error: "
	unsigned int uerr;
	char *p, *t;
	char tmp[40];				/* 64-bit number + slop */
	int len;

	uerr = errnum;				/* convert to unsigned */
	if (uerr < sys_nerr) {
		len = strlcpy(strerrbuf, (char *)sys_errlist[uerr], buflen);
		return (len <= buflen) ? 0 : ERANGE;
	}

	/* Print unknown errno by hand so we don't link to stdio(3). */
	t = tmp;
	if (errnum < 0)
		uerr = -uerr;
	do {
		*t++ = "0123456789"[uerr % 10];
	} while (uerr /= 10);

	if (errnum < 0)
		*t++ = '-';

	strlcpy(strerrbuf, UPREFIX, buflen);
	for (p = strerrbuf + sizeof(UPREFIX) - 1; p < strerrbuf + buflen; ) {
		*p++ = *--t;
		if (t <= tmp)
			break;
	}

	if (p < strerrbuf + buflen) {
		*p = '\0';
		return 0;
	}

	return ERANGE;
}


/*
 * NOTE: the following length should be enough to hold the longest defined
 * error message in sys_errlist, defined in ../gen/errlst.c.  This is a WAG
 * that is better than the previous value.
 */
#define ERR_LEN 64

char *
strerror(num)
	int num;
{
	unsigned int uerr;
	static char ebuf[ERR_LEN];

	uerr = num;				/* convert to unsigned */
	if (uerr < sys_nerr)
		return (char *)sys_errlist[uerr];

	/* strerror can't fail so handle truncation semi-elegantly */
	if (strerror_r(num, ebuf, (size_t) ERR_LEN) != 0)
	    ebuf[ERR_LEN - 1] = '\0';

	return ebuf;
}


#ifdef STANDALONE_TEST
main()
{
	char mybuf[64];
	int ret;

	printf("strerror(47) yeilds: %s\n", strerror(47));
	strerror_r(11, mybuf, 64);
	printf("strerror_r(11) yeilds: %s\n", mybuf);
	strerror_r(1234, mybuf, 64);
	printf("strerror_r(1234) yeilds: %s\n", mybuf);
	memset(mybuf, '*', 63);
	ret = strerror_r(4321, mybuf, 16);
	printf("strerror_r on short buffer returns %d (%s)\n", ret, mybuf);
}
#endif
