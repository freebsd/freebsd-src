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
#include <rune.h>
#include <stdlib.h>
#include <wchar.h>

size_t
mbrtowc(wchar_t * __restrict pwc, const char * __restrict s, size_t n,
    mbstate_t * __restrict ps __unused)
{
        const char *e;
        rune_t r;

	if (s == NULL) {
		pwc = NULL;
		s = "";
		n = 1;
	}

	if ((r = sgetrune(s, n, &e)) == _INVALID_RUNE) {
		/*
		 * The design of sgetrune() doesn't give us any way to tell
		 * between incomplete and invalid multibyte sequences.
		 */

		if (n >= (size_t)MB_CUR_MAX) {
			/*
			 * If we have been supplied with at least MB_CUR_MAX
			 * bytes and still cannot find a valid character, the
			 * data must be invalid.
			 */
			errno = EILSEQ;
			return ((size_t)-1);
		}

		/*
		 * .. otherwise, it's an incomplete character or an invalid
		 * character we cannot detect yet.
		 */
		return ((size_t)-2);
	}

	if (pwc != NULL)
		*pwc = (wchar_t)r;

	return (r != 0 ? (size_t)(e - s) : 0);
}
