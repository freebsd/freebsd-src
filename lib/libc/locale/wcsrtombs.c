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

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "mblocal.h"

size_t
wcsrtombs(char * __restrict dst, const wchar_t ** __restrict src, size_t len,
    mbstate_t * __restrict ps)
{
	static mbstate_t mbs;

	if (ps == NULL)
		ps = &mbs;
	return (__wcsrtombs(dst, src, len, ps));
}

size_t
__wcsrtombs_std(char * __restrict dst, const wchar_t ** __restrict src,
    size_t len, mbstate_t * __restrict ps)
{
	mbstate_t mbsbak;
	char buf[MB_LEN_MAX];
	const wchar_t *s;
	size_t nbytes;
	int nb;

	s = *src;
	nbytes = 0;

	if (dst == NULL) {
		for (;;) {
			if ((nb = (int)__wcrtomb(buf, *s, ps)) < 0)
				/* Invalid character - wcrtomb() sets errno. */
				return ((size_t)-1);
			else if (*s == L'\0')
				return (nbytes + nb - 1);
			s++;
			nbytes += nb;
		}
		/*NOTREACHED*/
	}

	while (len > 0) {
		if (len > (size_t)MB_CUR_MAX) {
			/* Enough space to translate in-place. */
			if ((nb = (int)__wcrtomb(dst, *s, ps)) < 0) {
				*src = s;
				return ((size_t)-1);
			}
		} else {
			/*
			 * May not be enough space; use temp. buffer.
			 *
			 * We need to save a copy of the conversion state
			 * here so we can restore it if the multibyte
			 * character is too long for the buffer.
			 */
			mbsbak = *ps;
			if ((nb = (int)__wcrtomb(buf, *s, ps)) < 0) {
				*src = s;
				return ((size_t)-1);
			}
			if (nb > (int)len) {
				/* MB sequence for character won't fit. */
				*ps = mbsbak;
				break;
			}
			memcpy(dst, buf, nb);
		}
		if (*s == L'\0') {
			*src = NULL;
			return (nbytes + nb - 1);
		}
		s++;
		dst += nb;
		len -= nb;
		nbytes += nb;
	}
	*src = s;
	return (nbytes);
}
