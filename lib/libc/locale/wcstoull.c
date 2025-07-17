/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 2011 The FreeBSD Foundation
 *
 * Portions of this software were developed by David Chisnall
 * under sponsorship from the FreeBSD Foundation.
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

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>
#include "xlocale_private.h"

/*
 * Convert a wide character string to an unsigned long long integer.
 */
unsigned long long
wcstoull_l(const wchar_t * __restrict nptr, wchar_t ** __restrict endptr,
    int base, locale_t locale)
{
	const wchar_t *s;
	unsigned long long acc;
	wchar_t c;
	unsigned long long cutoff;
	int neg, any, cutlim;
	FIX_LOCALE(locale);

	/*
	 * See strtoull for comments as to the logic used.
	 */
	s = nptr;
	do {
		c = *s++;
	} while (iswspace_l(c, locale));
	if (c == L'-') {
		neg = 1;
		c = *s++;
	} else {
		neg = 0;
		if (c == L'+')
			c = *s++;
	}
	if ((base == 0 || base == 16) &&
	    c == L'0' && (*s == L'x' || *s == L'X') &&
	    ((s[1] >= L'0' && s[1] <= L'9') ||
	    (s[1] >= L'A' && s[1] <= L'F') ||
	    (s[1] >= L'a' && s[1] <= L'f'))) {
		c = s[1];
		s += 2;
		base = 16;
	}
	if ((base == 0 || base == 2) &&
	    c == L'0' && (*s == L'b' || *s == L'B') &&
	    (s[1] >= L'0' && s[1] <= L'1')) {
		c = s[1];
		s += 2;
		base = 2;
	}
	if (base == 0)
		base = c == L'0' ? 8 : 10;
	acc = any = 0;
	if (base < 2 || base > 36)
		goto noconv;

	cutoff = ULLONG_MAX / base;
	cutlim = ULLONG_MAX % base;
	for ( ; ; c = *s++) {
#ifdef notyet
		if (iswdigit_l(c, locale))
			c = digittoint_l(c, locale);
		else
#endif
		if (c >= L'0' && c <= L'9')
			c -= L'0';
		else if (c >= L'A' && c <= L'Z')
			c -= L'A' - 10;
		else if (c >= L'a' && c <= L'z')
			c -= L'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
			any = -1;
		else {
			any = 1;
			acc *= base;
			acc += c;
		}
	}
	if (any < 0) {
		acc = ULLONG_MAX;
		errno = ERANGE;
	} else if (!any) {
noconv:
		errno = EINVAL;
	} else if (neg)
		acc = -acc;
	if (endptr != NULL)
		*endptr = (wchar_t *)(any ? s - 1 : nptr);
	return (acc);
}
unsigned long long
wcstoull(const wchar_t * __restrict nptr, wchar_t ** __restrict endptr,
    int base)
{
	return wcstoull_l(nptr, endptr, base, __get_locale());
}
