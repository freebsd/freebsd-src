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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)big5.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <rune.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

rune_t	_BIG5_sgetrune(const char *, size_t, char const **);
int	_BIG5_sputrune(rune_t, char *, size_t, char **);

int
_BIG5_init(rl)
	_RuneLocale *rl;
{
	rl->sgetrune = _BIG5_sgetrune;
	rl->sputrune = _BIG5_sputrune;
	_CurrentRuneLocale = rl;
	__mb_cur_max = 2;
	return (0);
}

static inline int
_big5_check(c)
	u_int c;
{
	c &= 0xff;
	return ((c >= 0xa1 && c <= 0xfe) ? 2 : 1);
}

rune_t
_BIG5_sgetrune(string, n, result)
	const char *string;
	size_t n;
	char const **result;
{
	rune_t rune = 0;
	int len;

	if (n < 1 || (len = _big5_check(*string)) > n) {
		if (result)
			*result = string;
		return (_INVALID_RUNE);
	}
	while (--len >= 0)
		rune = (rune << 8) | ((u_int)(*string++) & 0xff);
	if (result)
		*result = string;
	return rune;
}

int
_BIG5_sputrune(c, string, n, result)
	rune_t c;
	char *string, **result;
	size_t n;
{
	if (c & 0x8000) {
		if (n >= 2) {
			string[0] = (c >> 8) & 0xff;
			string[1] = c & 0xff;
			if (result)
				*result = string + 2;
			return (2);
		}
	}
	else {
		if (n >= 1) {
			*string = c & 0xff;
			if (result)
				*result = string + 1;
			return (1);
		}
	}
	if (result)
		*result = string;
	return (0);
	
}
