/*-
 * Copyright (c) 2003-2004 Tim J. Robbins
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

/* Not required when sgetrune() and sputrune() are removed. */
#define	OBSOLETE_IN_6

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <limits.h>
#include <rune.h>
#include <string.h>
#include <wchar.h>
#include "mblocal.h"

/*
 * Emulate the deprecated 4.4BSD sgetrune() function in terms of
 * the ISO C mbrtowc() function.
 */
rune_t
__emulated_sgetrune(const char *string, size_t n, const char **result)
{
	static const mbstate_t initial;
	mbstate_t mbs;
	wchar_t wc;
	size_t nconv;

	/*
	 * Pass a NULL conversion state to mbrtowc() since multibyte
	 * conversion states are not supported.
	 */
	mbs = initial;
	nconv = mbrtowc(&wc, string, n, &mbs);
	if (nconv == (size_t)-2) {
		if (result != NULL)
			*result = string;
		return (_INVALID_RUNE);
	}
	if (nconv == (size_t)-1) {
		if (result != NULL)
			*result = string + 1;
		return (_INVALID_RUNE);
	}
	if (nconv == 0)
		nconv = 1;
	if (result != NULL)
		*result = string + nconv;
	return ((rune_t)wc);
}

/*
 * Emulate the deprecated 4.4BSD sputrune() function in terms of
 * the ISO C wcrtomb() function.
 */
int
__emulated_sputrune(rune_t rune, char *string, size_t n, char **result)
{
	static const mbstate_t initial;
	mbstate_t mbs;
	char buf[MB_LEN_MAX];
	size_t nconv;

	mbs = initial;
	nconv = wcrtomb(buf, (wchar_t)rune, &mbs);
	if (nconv == (size_t)-1) {
		if (result != NULL)
			*result = NULL;
		return (0);
	}
	if (string == NULL) {
		if (result != NULL)
			*result = (char *)0 + nconv;
	} else if (n >= nconv) {
		memcpy(string, buf, nconv);
		if (result != NULL)
			*result = string + nconv;
	} else {
		if (result != NULL)
			*result = NULL;
	}
	return (nconv);
}
