/*-
 * Copyright (c) 2002-2004 Tim J. Robbins
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
/*
 * UTF2 encoding.
 *
 * This is an obsolete subset of UTF-8, maintained for temporary
 * compatibility with old applications. It is limited to 1-, 2- or
 * 3-byte encodings, and allows redundantly-encoded characters.
 *
 * See utf2(5) for details.
 */

/* UTF2 is obsolete and will be removed in FreeBSD 6 -- use UTF-8 instead. */
#define	OBSOLETE_IN_6

#include <sys/param.h>
__FBSDID("$FreeBSD$");

#include <errno.h>
#include <runetype.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "mblocal.h"

size_t	_UTF2_mbrtowc(wchar_t * __restrict, const char * __restrict, size_t,
	    mbstate_t * __restrict);
int	_UTF2_mbsinit(const mbstate_t *);
size_t	_UTF2_wcrtomb(char * __restrict, wchar_t, mbstate_t * __restrict);

typedef struct {
	int	count;
	u_char	bytes[3];
} _UTF2State;

int
_UTF2_init(_RuneLocale *rl)
{

	__mbrtowc = _UTF2_mbrtowc;
	__wcrtomb = _UTF2_wcrtomb;
	__mbsinit = _UTF2_mbsinit;
	_CurrentRuneLocale = rl;
	__mb_cur_max = 3;

	return (0);
}

int
_UTF2_mbsinit(const mbstate_t *ps)
{

	return (ps == NULL || ((const _UTF2State *)ps)->count == 0);
}

size_t
_UTF2_mbrtowc(wchar_t * __restrict pwc, const char * __restrict s, size_t n,
    mbstate_t * __restrict ps)
{
	_UTF2State *us;
	int ch, i, len, mask, ocount;
	wchar_t wch;
	size_t ncopy;

	us = (_UTF2State *)ps;

	if (us->count < 0 || us->count > sizeof(us->bytes)) {
		errno = EINVAL;
		return ((size_t)-1);
	}

	if (s == NULL) {
		s = "";
		n = 1;
		pwc = NULL;
	}

	ncopy = MIN(MIN(n, MB_CUR_MAX), sizeof(us->bytes) - us->count);
	memcpy(us->bytes + us->count, s, ncopy);
	ocount = us->count;
	us->count += ncopy;
	s = (char *)us->bytes;
	n = us->count;

	if (n == 0)
		return ((size_t)-2);

	ch = (unsigned char)*s;
	if ((ch & 0x80) == 0) {
		mask = 0x7f;
		len = 1;
	} else if ((ch & 0xe0) == 0xc0) {
		mask = 0x1f;
		len = 2;
	} else if ((ch & 0xf0) == 0xe0) {
		mask = 0x0f;
		len = 3;
	} else {
		errno = EILSEQ;
		return ((size_t)-1);
	}

	if (n < (size_t)len)
		return ((size_t)-2);

	wch = (unsigned char)*s++ & mask;
	i = len;
	while (--i != 0) {
		if ((*s & 0xc0) != 0x80) {
			errno = EILSEQ;
			return ((size_t)-1);
		}
		wch <<= 6;
		wch |= *s++ & 0x3f;
	}
	if (pwc != NULL)
		*pwc = wch;
	us->count = 0;
	return (wch == L'\0' ? 0 : len - ocount);
}

size_t
_UTF2_wcrtomb(char * __restrict s, wchar_t wc, mbstate_t * __restrict ps)
{
	_UTF2State *us;
	unsigned char lead;
	int i, len;

	us = (_UTF2State *)ps;

	if (us->count != 0) {
		errno = EINVAL;
		return ((size_t)-1);
	}

	if (s == NULL)
		/* Reset to initial conversion state. */
		return (1);

	if ((wc & ~0x7f) == 0) {
		lead = 0;
		len = 1;
	} else if ((wc & ~0x7ff) == 0) {
		lead = 0xc0;
		len = 2;
	} else if ((wc & ~0xffff) == 0) {
		lead = 0xe0;
		len = 3;
	} else {
		errno = EILSEQ;
		return ((size_t)-1);
	}

	for (i = len - 1; i > 0; i--) {
		s[i] = (wc & 0x3f) | 0x80;
		wc >>= 6;
	}
	*s = (wc & 0xff) | lead;

	return (len);
}
