/*-
 * Copyright (c) 2002 Tim J. Robbins
 * All rights reserved.
 *
 * Copyright (c) 2013 Ed Schouten <ed@FreeBSD.org>
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
/*
 * Test program for mbrtoc16() as specified by ISO/IEC 9899:2011.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <uchar.h>

int
main(int argc, char *argv[])
{
	mbstate_t s;
	size_t len;
	char16_t c16;

	/*
	 * C/POSIX locale.
	 */

	printf("1..1\n");

	/* Null wide character, internal state. */
	assert(mbrtoc16(&c16, "", 1, NULL) == 0);
	assert(c16 == 0);

	/* Null wide character. */
	memset(&s, 0, sizeof(s));
	assert(mbrtoc16(&c16, "", 1, &s) == 0);
	assert(c16 == 0);

	/* Latin letter A, internal state. */
	assert(mbrtoc16(NULL, 0, 0, NULL) == 0);
	assert(mbrtoc16(&c16, "A", 1, NULL) == 1);
	assert(c16 == L'A');

	/* Latin letter A. */
	memset(&s, 0, sizeof(s));
	assert(mbrtoc16(&c16, "A", 1, &s) == 1);
	assert(c16 == L'A');

	/* Incomplete character sequence. */
	c16 = L'z';
	memset(&s, 0, sizeof(s));
	assert(mbrtoc16(&c16, "", 0, &s) == (size_t)-2);
	assert(c16 == L'z');

	/* Check that mbrtoc16() doesn't access the buffer when n == 0. */
	c16 = L'z';
	memset(&s, 0, sizeof(s));
	assert(mbrtoc16(&c16, "", 0, &s) == (size_t)-2);
	assert(c16 == L'z');

	/* Check that mbrtoc16() doesn't read ahead too aggressively. */
	memset(&s, 0, sizeof(s));
	assert(mbrtoc16(&c16, "AB", 2, &s) == 1);
	assert(c16 == L'A');
	assert(mbrtoc16(&c16, "C", 1, &s) == 1);
	assert(c16 == L'C');

	/*
	 * ISO-8859-1.
	 */

	assert(strcmp(setlocale(LC_CTYPE, "en_US.ISO8859-1"),
	    "en_US.ISO8859-1") == 0);

	/* Currency sign. */
	memset(&s, 0, sizeof(s));
	assert(mbrtoc16(&c16, "\xa4", 1, &s) == 1);
	assert(c16 == 0xa4);

	/*
	 * ISO-8859-15.
	 */

	assert(strcmp(setlocale(LC_CTYPE, "en_US.ISO8859-15"),
	    "en_US.ISO8859-15") == 0);

	/* Euro sign. */
	memset(&s, 0, sizeof(s));
	assert(mbrtoc16(&c16, "\xa4", 1, &s) == 1);
	assert(c16 == 0x20ac);

	/*
	 * UTF-8.
	 */

	assert(strcmp(setlocale(LC_CTYPE, "en_US.UTF-8"), "en_US.UTF-8") == 0);

	/* Null wide character, internal state. */
	assert(mbrtoc16(NULL, 0, 0, NULL) == 0);
	assert(mbrtoc16(&c16, "", 1, NULL) == 0);
	assert(c16 == 0);

	/* Null wide character. */
	memset(&s, 0, sizeof(s));
	assert(mbrtoc16(&c16, "", 1, &s) == 0);
	assert(c16 == 0);

	/* Latin letter A, internal state. */
	assert(mbrtoc16(NULL, 0, 0, NULL) == 0);
	assert(mbrtoc16(&c16, "A", 1, NULL) == 1);
	assert(c16 == L'A');

	/* Latin letter A. */
	memset(&s, 0, sizeof(s));
	assert(mbrtoc16(&c16, "A", 1, &s) == 1);
	assert(c16 == L'A');

	/* Incomplete character sequence (zero length). */
	c16 = L'z';
	memset(&s, 0, sizeof(s));
	assert(mbrtoc16(&c16, "", 0, &s) == (size_t)-2);
	assert(c16 == L'z');

	/* Incomplete character sequence (truncated double-byte). */
	memset(&s, 0, sizeof(s));
	c16 = 0;
	assert(mbrtoc16(&c16, "\xc3", 1, &s) == (size_t)-2);

	/* Same as above, but complete. */
	memset(&s, 0, sizeof(s));
	c16 = 0;
	assert(mbrtoc16(&c16, "\xc3\x84", 2, &s) == 2);
	assert(c16 == 0xc4);

	/* Test restarting behaviour. */
	memset(&s, 0, sizeof(s));
	c16 = 0;
	assert(mbrtoc16(&c16, "\xc3", 1, &s) == (size_t)-2);
	assert(c16 == 0);
	assert(mbrtoc16(&c16, "\xb7", 1, &s) == 1);
	assert(c16 == 0xf7);

	/* Surrogate pair. */
	memset(&s, 0, sizeof(s));
	c16 = 0;
	assert(mbrtoc16(&c16, "\xf0\x9f\x92\xa9", 4, &s) == 4);
	assert(c16 == 0xd83d);
	assert(mbrtoc16(&c16, "", 0, &s) == (size_t)-3);
	assert(c16 == 0xdca9);

	/* Letter e with acute, precomposed. */
	memset(&s, 0, sizeof(s));
	c16 = 0;
	assert(mbrtoc16(&c16, "\xc3\xa9", 2, &s) == 2);
	assert(c16 == 0xe9);

	/* Letter e with acute, combined. */
	memset(&s, 0, sizeof(s));
	c16 = 0;
	assert(mbrtoc16(&c16, "\x65\xcc\x81", 3, &s) == 1);
	assert(c16 == 0x65);
	assert(mbrtoc16(&c16, "\xcc\x81", 2, &s) == 2);
	assert(c16 == 0x301);

	printf("ok 1 - mbrtoc16()\n");

	return (0);
}
