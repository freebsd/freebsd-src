/*-
 * Copyright (c) 2002 Tim J. Robbins.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "namespace.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "un-namespace.h"
#include "libc_private.h"
#include "local.h"

static __inline wint_t	__fgetwc_nbf(FILE *);

/*
 * Non-MT-safe version.
 */
wint_t
__fgetwc(FILE *fp)
{
	wint_t wc;

	if (MB_CUR_MAX == 1) {
		/*
		 * Assume we're using a single-byte locale. A safer test
		 * might be to check _CurrentRuneLocale->encoding.
		 */
		wc = (wint_t)__sgetc(fp);
	} else
		wc = __fgetwc_nbf(fp);

	return (wc);
}

/*
 * MT-safe version.
 */
wint_t
fgetwc(FILE *fp)
{
	wint_t r;

	FLOCKFILE(fp);
	ORIENT(fp, 1);
	r = __fgetwc(fp);
	FUNLOCKFILE(fp);

	return (r);
}

static __inline wint_t
__fgetwc_nbf(FILE *fp)
{
	char buf[MB_LEN_MAX];
	mbstate_t mbs;
	size_t n, nconv;
	int c;
	wchar_t wc;

	n = 0;
	while (n < MB_CUR_MAX) {
		if ((c = __sgetc(fp)) == EOF) {
			if (n == 0)
				return (WEOF);
			break;
		}
		buf[n++] = (char)c;
		memset(&mbs, 0, sizeof(mbs));
		nconv = mbrtowc(&wc, buf, n, &mbs);
		if (nconv == n)
			return (wc);
		else if (nconv == 0)
			return (L'\0');
		else if (nconv == (size_t)-1)
			break;
	}

	while (n-- != 0)
		__ungetc((unsigned char)buf[n], fp);
	errno = EILSEQ;
	fp->_flags |= __SERR;
	return (WEOF);
}
