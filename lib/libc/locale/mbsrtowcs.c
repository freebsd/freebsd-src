/*-
 * Copyright (c) 2002 Tim J. Robbins.
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
#include <limits.h>
#include <stdlib.h>
#include <wchar.h>

size_t
mbsrtowcs(wchar_t * __restrict dst, const char ** __restrict src, size_t len,
    mbstate_t * __restrict ps __unused)
{
	const char *s;
	size_t nchr;
	wchar_t wc;
	int nb;

	s = *src;
	nchr = 0;

	if (dst == NULL) {
		for (;;) {
			if ((nb = (int)mbrtowc(&wc, s, MB_CUR_MAX, NULL)) < 0)
				/* Invalid sequence - mbrtowc() sets errno. */
				return ((size_t)-1);
			else if (nb == 0)
				return (nchr);
			s += nb;
			nchr++;
		}
		/*NOTREACHED*/
	}

	while (len-- > 0) {
		if ((nb = (int)mbrtowc(dst, s, MB_CUR_MAX, NULL)) < 0) {
			*src = s;
			return ((size_t)-1);
		} else if (nb == 0) {
			*src = NULL;
			return (nchr);
		}
		s += nb;
		nchr++;
		dst++;
	}
	*src = s;
	return (nchr);
}
