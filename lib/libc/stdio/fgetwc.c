/*-
 * Copyright (c) 2002-2004 Tim J. Robbins.
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
#include "mblocal.h"

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

/*
 * Non-MT-safe version.
 */
wint_t
__fgetwc(FILE *fp)
{
	wchar_t wc;
	size_t nconv;

	if (fp->_r <= 0 && __srefill(fp))
		return (WEOF);
	if (MB_CUR_MAX == 1) {
		/* Fast path for single-byte encodings. */
		wc = *fp->_p++;
		fp->_r--;
		return (wc);
	}
	do {
		nconv = __mbrtowc(&wc, fp->_p, fp->_r, &fp->_extra->mbstate);
		if (nconv == (size_t)-1)
			break;
		else if (nconv == (size_t)-2)
			continue;
		else if (nconv == 0) {
			/*
			 * Assume that the only valid representation of
			 * the null wide character is a single null byte.
			 */
			fp->_p++;
			fp->_r--;
			return (L'\0');
		} else {
			fp->_p += nconv;
			fp->_r -= nconv;
			return (wc);
		}
	} while (__srefill(fp) == 0);
	fp->_flags |= __SERR;
	errno = EILSEQ;
	return (WEOF);
}
