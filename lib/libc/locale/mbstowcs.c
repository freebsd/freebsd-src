/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <stddef.h>
#include <rune.h>

size_t
mbstowcs(wchar_t * __restrict pwcs, const char * __restrict s, size_t n)
{
	const char *e;
	int cnt;
	rune_t r;

	if (s == NULL) {
		errno = EINVAL;
		return (-1);
	}

	if (pwcs == NULL) {
		/* Convert and count only, do not store. */
		cnt = 0;
		while ((r = sgetrune(s, MB_LEN_MAX, &e)) != _INVALID_RUNE &&
		    r != 0) {
			s = e;
			cnt++;
		}
		if (r == _INVALID_RUNE) {
			errno = EILSEQ;
			return (-1);
		}
		return (cnt);
	}

	/* Convert, store and count characters. */
	cnt = 0;
	while (n-- > 0) {
		*pwcs = sgetrune(s, MB_LEN_MAX, &e);
		if (*pwcs == _INVALID_RUNE) {
			errno = EILSEQ;
			return (-1);
		}
		if (*pwcs++ == L'\0')
			break;
		s = e;
		++cnt;
	}

	return (cnt);
}
