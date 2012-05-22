/*-
 * Copyright (c) 1995 Alex Tatmanjants <alex@elvisti.kiev.ua>
 *		at Electronni Visti IA, Kiev, Ukraine.
 *			All rights reserved.
 *
 * Copyright (c) 2011 The FreeBSD Foundation
 * All rights reserved.
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
__FBSDID("$FreeBSD$");

#include <stdlib.h>
#include <string.h>
#include "collate.h"

size_t
strxfrm_l(char * __restrict dest, const char * __restrict src, size_t len, locale_t loc);
size_t
strxfrm(char * __restrict dest, const char * __restrict src, size_t len)
{
	return strxfrm_l(dest, src, len, __get_locale());
}

size_t
strxfrm_l(char * __restrict dest, const char * __restrict src, size_t len, locale_t locale)
{
	int prim, sec, l;
	size_t slen;
	char *s, *ss;
	FIX_LOCALE(locale);
	struct xlocale_collate *table =
		(struct xlocale_collate*)locale->components[XLC_COLLATE];

	if (!*src) {
		if (len > 0)
			*dest = '\0';
		return 0;
	}

	if (table->__collate_load_error)
		return strlcpy(dest, src, len);

	slen = 0;
	prim = sec = 0;
	ss = s = __collate_substitute(table, src);
	while (*s) {
		while (*s && !prim) {
			__collate_lookup(table, s, &l, &prim, &sec);
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
