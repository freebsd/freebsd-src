/*-
 * Copyright (c) 2002, 2003 Tim J. Robbins
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
__FBSDID("$FreeBSD: src/lib/libc/locale/wcstold.c,v 1.1 2003/03/13 06:29:53 tjr Exp $");

#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

/*
 * See wcstod() for comments as to the logic used.
 */
long double
wcstold(const wchar_t * __restrict nptr, wchar_t ** __restrict endptr)
{
	static const mbstate_t initial;
	mbstate_t state;
	long double val;
	char *buf, *end, *p;
	const wchar_t *wcp;
	size_t clen, len;

	while (iswspace(*nptr))
		nptr++;

	state = initial;
	wcp = nptr;
	if ((len = wcsrtombs(NULL, &wcp, 0, &state)) == (size_t)-1) {
		if (endptr != NULL)
			*endptr = (wchar_t *)nptr;
		return (0.0);
	}
	if ((buf = malloc(len + 1)) == NULL)
		return (0.0);
	state = initial;
	wcsrtombs(buf, &wcp, len + 1, &state);

	val = strtold(buf, &end);

	if (endptr != NULL) {
#if 1					/* Fast, assume 1:1 WC:MBS mapping. */
		*endptr = (wchar_t *)nptr + (end - buf);
		(void)clen;
		(void)p;
#else					/* Slow, conservative approach. */
		state = initial;
		*endptr = (wchar_t *)nptr;
		p = buf;
		while (p < end &&
		    (clen = mbrlen(p, end - p, &state)) > 0) {
			p += clen;
			(*endptr)++;
		}
#endif
	}

	free(buf);

	return (val);
}
