/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 2011 The FreeBSD Foundation
 *
 * Portions of this software were developed by David Chisnall
 * under sponsorship from the FreeBSD Foundation.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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
#include <stdio.h>
#include <stdarg.h>
#include "xlocale_private.h"

#include "local.h"

#undef snprintf	/* _FORTIFY_SOURCE */

int
snprintf(char * __restrict str, size_t n, char const * __restrict fmt, ...)
{
	FILE f = FAKE_FILE;
	va_list ap;
	size_t on;
	int serrno = errno;
	int ret;

	on = n;
	if (n != 0)
		n--;
	if (n > INT_MAX) {
		errno = EOVERFLOW;
		*str = '\0';
		return (EOF);
	}
	va_start(ap, fmt);
	f._flags = __SWR | __SSTR;
	f._bf._base = f._p = (unsigned char *)str;
	f._bf._size = f._w = n;
	ret = __vfprintf(&f, __get_locale(), serrno, fmt, ap);
	if (on > 0)
		*f._p = '\0';
	va_end(ap);
	return (ret);
}
int
snprintf_l(char * __restrict str, size_t n, locale_t locale,
		char const * __restrict fmt, ...)
{
	FILE f = FAKE_FILE;
	va_list ap;
	size_t on;
	int serrno = errno;
	int ret;
	FIX_LOCALE(locale);

	on = n;
	if (n != 0)
		n--;
	if (n > INT_MAX) {
		errno = EOVERFLOW;
		*str = '\0';
		return (EOF);
	}
	va_start(ap, fmt);
	f._flags = __SWR | __SSTR;
	f._bf._base = f._p = (unsigned char *)str;
	f._bf._size = f._w = n;
	ret = __vfprintf(&f, locale, serrno, fmt, ap);
	if (on > 0)
		*f._p = '\0';
	va_end(ap);
	return (ret);
}
