/*-
 * Copyright (c) 2002 Tim J. Robbins
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

#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

/*
 * Convert a string to a double-precision number.
 *
 * This is the wide-character counterpart of strtod(). So that we do not
 * have to duplicate the code of strtod() here, we convert the supplied
 * wide character string to multibyte and call strtod() on the result.
 * This assumes that the multibyte encoding is compatible with ASCII
 * for at least the digits, radix character and letters.
 */
double
wcstod(const wchar_t * __restrict nptr, wchar_t ** __restrict endptr)
{
	static const mbstate_t initial;
	mbstate_t state;
	double val;
	char *buf, *end;
	const wchar_t *wcp;
	size_t clen, len;

	while (iswspace(*nptr))
		nptr++;

	/*
	 * Convert the supplied numeric wide char. string to multibyte.
	 *
	 * We could attempt to find the end of the numeric portion of the
	 * wide char. string to avoid converting unneeded characters but
	 * choose not to bother; optimising the uncommon case where
	 * the input string contains a lot of text after the number
	 * duplicates a lot of strtod()'s functionality and slows down the
	 * most common cases.
	 */
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

	/* Let strtod() do most of the work for us. */
	val = strtod(buf, &end);

	/*
	 * We only know where the number ended in the _multibyte_
	 * representation of the string. If the caller wants to know
	 * where it ended, count multibyte characters to find the
	 * corresponding position in the wide char string.
	 */
	if (endptr != NULL) {
#if 1					/* Fast, assume 1:1 WC:MBS mapping. */
		*endptr = (wchar_t *)nptr + (end - buf);
		(void)clen;
#else					/* Slow, conservative approach. */
		state = initial;
		*endptr = (wchar_t *)nptr;
		while (buf < end &&
		    (clen = mbrlen(buf, end - buf, &state)) > 0) {
			buf += clen;
			(*endptr)++;
		}
#endif
	}

	free(buf);

	return (val);
}
