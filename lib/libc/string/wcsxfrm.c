/*-
 * Copyright (c) 1995 Alex Tatmanjants <alex@elvisti.kiev.ua>
 *		at Electronni Visti IA, Kiev, Ukraine.
 *			All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if 0
__FBSDID("FreeBSD: src/lib/libc/string/strxfrm.c,v 1.15 2002/09/06 11:24:06 tjr Exp ");
#endif
__FBSDID("$FreeBSD: src/lib/libc/string/wcsxfrm.c,v 1.3 2004/04/07 09:47:56 tjr Exp $");

#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "collate.h"

static char *__mbsdup(const wchar_t *);

/*
 * Placeholder wcsxfrm() implementation. See wcscoll.c for a description of
 * the logic used.
 */
size_t
wcsxfrm(wchar_t * __restrict dest, const wchar_t * __restrict src, size_t len)
{
	int prim, sec, l;
	size_t slen;
	char *mbsrc, *s, *ss;

	if (*src == L'\0') {
		if (len != 0)
			*dest = L'\0';
		return (0);
	}

	if (__collate_load_error || MB_CUR_MAX > 1) {
		slen = wcslen(src);
		if (len > 0) {
			if (slen < len)
				wcscpy(dest, src);
			else {
				wcsncpy(dest, src, len - 1);
				dest[len - 1] = L'\0';
			}
		}
		return (slen);
	}

	mbsrc = __mbsdup(src);
	slen = 0;
	prim = sec = 0;
	ss = s = __collate_substitute(mbsrc);
	while (*s != '\0') {
		while (*s != '\0' && prim == 0) {
			__collate_lookup(s, &l, &prim, &sec);
			s += l;
		}
		if (prim != 0) {
			if (len > 1) {
				*dest++ = (wchar_t)prim;
				len--;
			}
			slen++;
			prim = 0;
		}
	}
	free(ss);
	free(mbsrc);
	if (len != 0)
		*dest = L'\0';

	return (slen);
}

static char *
__mbsdup(const wchar_t *ws)
{
	static const mbstate_t initial;
	mbstate_t st;
	const wchar_t *wcp;
	size_t len;
	char *mbs;

	wcp = ws;
	st = initial;
	if ((len = wcsrtombs(NULL, &wcp, 0, &st)) == (size_t)-1)
		return (NULL);
	if ((mbs = malloc(len + 1)) == NULL)
		return (NULL);
	st = initial;
	wcsrtombs(mbs, &ws, len + 1, &st);

	return (mbs);
}
