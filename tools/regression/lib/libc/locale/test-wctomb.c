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

/*
 * Test program for wctomb(), as specified by IEEE Std. 1003.1-2001 and
 * ISO/IEC 9899:1999.
 *
 * The function is tested with both the "C" ("POSIX") LC_CTYPE setting and
 * "ja_JP.eucJP". Other encodings are not tested.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char *argv[])
{
	size_t len;
	char buf[MB_LEN_MAX + 1];

	/*
	 * C/POSIX locale.
	 */

	assert(MB_CUR_MAX == 1);

	/* No shift states in C locale. */
	assert(wctomb(NULL, L'\0') == 0);

	/* Null wide character. */
	memset(buf, 0xcc, sizeof(buf));
	len = wctomb(buf, L'\0');
	assert(len == 1);
	assert((unsigned char)buf[0] == 0 && (unsigned char)buf[1] == 0xcc);

	/* Latin letter A. */
	memset(buf, 0xcc, sizeof(buf));
	len = wctomb(buf, L'A');
	assert(len == 1);
	assert((unsigned char)buf[0] == 'A' && (unsigned char)buf[1] == 0xcc);

	/* Invalid code. */
	assert(wctomb(buf, UCHAR_MAX + 1) == -1);

	/*
	 * Japanese (EUC) locale.
	 */

	assert(strcmp(setlocale(LC_CTYPE, "ja_JP.eucJP"), "ja_JP.eucJP") == 0);
	assert(MB_CUR_MAX == 3);

	/* No shift states in EUC encoding. */
	assert(wctomb(NULL, L'\0') == 0);

	/* Null wide character. */
	memset(buf, 0xcc, sizeof(buf));
	len = wctomb(buf, L'\0');
	assert(len == 1);
	assert((unsigned char)buf[0] == 0 && (unsigned char)buf[1] == 0xcc);

	/* Latin letter A. */
	memset(buf, 0xcc, sizeof(buf));
	len = wctomb(buf, L'A');
	assert(len == 1);
	assert((unsigned char)buf[0] == 'A' && (unsigned char)buf[1] == 0xcc);

	/* Full width letter A. */
	memset(buf, 0xcc, sizeof(buf));
	len = wctomb(buf, 0xa3c1);
	assert(len == 2);
	assert((unsigned char)buf[0] == 0xa3 &&
		(unsigned char)buf[1] == 0xc1 &&
		(unsigned char)buf[2] == 0xcc);

	printf("PASS wctomb()\n");

	return (0);
}
