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
 *
 * $FreeBSD$
 */

#include <stdlib.h>
#include <string.h>
#include "collate.h"

size_t
strxfrm(dest, src, len)
	char *dest;
	const char *src;
	size_t len;
{
	int prim, sec, l;
	size_t slen;
	char *s, *ss;

	if (!*src) {
		if (len > 0)
			*dest = '\0';
		return 0;
	}

	if (__collate_load_error) {
		slen = strlen(src);
		if (len > 0) {
			if (slen < len)
				strcpy(dest, src);
			else {
				strncpy(dest, src, len - 1);
				dest[len - 1] = '\0';
			}
		}
		return slen;
	}

	slen = 0;
	prim = sec = 0;
	ss = s = __collate_substitute(src);
	while (*s) {
		while (*s && !prim) {
			__collate_lookup(s, &l, &prim, &sec);
			s += l;
		}
		if (prim) {
			if (len > 1) {
				*dest++ = (char)prim;
				len--;
			}
			slen++;
			prim = 0;
		}
	}
	free(ss);
	if (len > 0)
		*dest = '\0';

	return slen;
}
