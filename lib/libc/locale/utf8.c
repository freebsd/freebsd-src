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

#include <rune.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

rune_t	_UTF8_sgetrune(const char *, size_t, char const **);
int	_UTF8_sputrune(rune_t, char *, size_t, char **);

int
_UTF8_init(_RuneLocale *rl)
{

	rl->sgetrune = _UTF8_sgetrune;
	rl->sputrune = _UTF8_sputrune;
	_CurrentRuneLocale = rl;
	__mb_cur_max = 6;

	return (0);
}

rune_t
_UTF8_sgetrune(const char *string, size_t n, const char **result)
{
	int ch, len, mask, siglen;
	rune_t lbound, wch;

	if (n < 1) {
		if (result != NULL)
			*result = string;
		return (_INVALID_RUNE);
	}

	/*
	 * Determine the number of octets that make up this character from
	 * the first octet, and a mask that extracts the interesting bits of
	 * the first octet.
	 *
	 * We also specify a lower bound for the character code to detect
	 * redundant, non-"shortest form" encodings. For example, the
	 * sequence C0 80 is _not_ a legal representation of the null
	 * character. This enforces a 1-to-1 mapping between character
	 * codes and their multibyte representations.
	 */
	ch = (unsigned char)*string;
	if ((ch & 0x80) == 0) {
		mask = 0x7f;
		len = 1;
		lbound = 0;
	} else if ((ch & 0xe0) == 0xc0) {
		mask = 0x1f;
		len = 2;
		lbound = 0x80;
	} else if ((ch & 0xf0) == 0xe0) {
		mask = 0x0f;
		len = 3;
		lbound = 0x800;
	} else if ((ch & 0xf8) == 0xf0) {
		mask = 0x07;
		len = 4;
		lbound = 0x10000;
	} else if ((ch & 0xfc) == 0xf8) {
		mask = 0x03;
		len = 5;
		lbound = 0x200000;
	} else if ((ch & 0xfc) == 0xfc) {
		mask = 0x01;
		len = 6;
		lbound = 0x4000000;
	} else {
		/*
		 * Malformed input; input is not UTF-8.
		 */
		if (result != NULL)
			*result = string + 1;
		return (_INVALID_RUNE);
	}

	if (n < len) {
		/*
		 * Truncated or partial input.
		 */
		if (result != NULL)
			*result = string;
		return (_INVALID_RUNE);
	}

	/*
	 * Decode the octet sequence representing the character in chunks
	 * of 6 bits, most significant first.
	 */
	wch = (unsigned char)*string++ & mask;
	while (--len != 0) {
		if ((*string & 0xc0) != 0x80) {
			/*
			 * Malformed input; bad characters in the middle
			 * of a character.
			 */
			wch = _INVALID_RUNE;
			if (result != NULL)
				*result = string + 1;
			return (_INVALID_RUNE);
		}
		wch <<= 6;
		wch |= *string++ & 0x3f;
	}
	if (wch != _INVALID_RUNE && wch < lbound)
		/*
		 * Malformed input; redundant encoding.
		 */
		wch = _INVALID_RUNE;
	if (result != NULL)
		*result = string;
	return (wch);
}

int
_UTF8_sputrune(rune_t c, char *string, size_t n, char **result)
{
	unsigned char lead;
	int i, len;

	/*
	 * Determine the number of octets needed to represent this character.
	 * We always output the shortest sequence possible. Also specify the
	 * first few bits of the first octet, which contains the information
	 * about the sequence length.
	 */
	if ((c & ~0x7f) == 0) {
		lead = 0;
		len = 1;
	} else if ((c & ~0x7ff) == 0) {
		lead = 0xc0;
		len = 2;
	} else if ((c & ~0xffff) == 0) {
		lead = 0xe0;
		len = 3;
	} else if ((c & ~0x1fffff) == 0) {
		lead = 0xf0;
		len = 4;
	} else if ((c & ~0x3ffffff) == 0) {
		lead = 0xf8;
		len = 5;
	} else if ((c & ~0x7fffffff) == 0) {
		lead = 0xfc;
		len = 6;
	} else {
		/*
		 * Wide character code is out of range.
		 */
		if (result != NULL)
			*result = NULL;
		return (0);
	}

	if (n < len) {
		if (result != NULL)
			*result = NULL;
	} else {
		/*
		 * Output the octets representing the character in chunks
		 * of 6 bits, least significant last. The first octet is
		 * a special case because it contains the sequence length
		 * information.
		 */
		for (i = len - 1; i > 0; i--) {
			string[i] = (c & 0x3f) | 0x80;
			c >>= 6;
		}
		*string = (c & 0xff) | lead;
		if (result != NULL)
			*result = string + len;
	}

	return (len);
}
