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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "collate.h"

static char *__mbsdup(const wchar_t *);

/*
 * Placeholder implementation of wcscoll(). Attempts to use the single-byte
 * collation ordering where possible, and falls back on wcscmp() in locales
 * with extended character sets.
 */
int
wcscoll(const wchar_t *ws1, const wchar_t *ws2)
{
	char *mbs1, *mbs2;
	int diff, sverrno;

	if (__collate_load_error || MB_CUR_MAX > 1)
		/*
		 * Locale has no special collating order, could not be
		 * loaded, or has an extended character set; do a fast binary
		 * comparison.
		 */
		return (wcscmp(ws1, ws2));

	if ((mbs1 = __mbsdup(ws1)) == NULL || (mbs2 = __mbsdup(ws2)) == NULL) {
		/*
		 * Out of memory or illegal wide chars; fall back to wcscmp()
		 * but leave errno indicating the error. Callers that don't
		 * check for error will get a reasonable but often slightly
		 * incorrect result.
		 */
		sverrno = errno;
		free(mbs1);
		errno = sverrno;
		return (wcscmp(ws1, ws2));
	}

	diff = strcoll(mbs1, mbs2);
	sverrno = errno;
	free(mbs1);
	free(mbs2);
	errno = sverrno;

	return (diff);
}

static char *
__mbsdup(const wchar_t *ws)
{
	const wchar_t *wcp;
	size_t len;
	char *mbs;

	wcp = ws;
	if ((len = wcsrtombs(NULL, &wcp, 0, NULL)) == (size_t)-1)
		return (NULL);
	if ((mbs = malloc(len + 1)) == NULL)
		return (NULL);
	wcsrtombs(mbs, &ws, len + 1, NULL);

	return (mbs);
}
