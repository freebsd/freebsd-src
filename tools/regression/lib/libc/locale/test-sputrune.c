/*-
 * Copyright (c) 2002, 2003 Tim J. Robbins
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
 * Test program for the 4.4BSD sputrune() function.
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
#include <rune.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char *argv[])
{
	size_t len;
	char *res;
	char buf[MB_LEN_MAX + 1];

	/*
	 * C/POSIX locale.
	 */

	assert(MB_CUR_MAX == 1);

	/* Null wide character. */
	memset(buf, 0xcc, sizeof(buf));
	assert(sputrune(L'\0', buf, 1, &res) == 1);
	assert(res == buf + 1);
	assert((unsigned char)buf[0] == 0 && (unsigned char)buf[1] == 0xcc);
	assert(sputrune(L'\0', buf, 1, NULL) == 1);
	assert(sputrune(L'\0', buf, 2, &res) == 1);
	assert(res == buf + 1);

	/* Latin letter A. */
	memset(buf, 0xcc, sizeof(buf));
	assert(sputrune(L'A', buf, 1, &res) == 1);
	assert(res == buf + 1);
	assert((unsigned char)buf[0] == 'A' && (unsigned char)buf[1] == 0xcc);

	/* Invalid code. */
	assert(sputrune(UCHAR_MAX + 1, buf, 1, &res) == 0);
	assert(res == NULL);
	assert(sputrune(UCHAR_MAX + 1, buf, 2, &res) == 0);
	assert(res == NULL);
	assert(sputrune(UCHAR_MAX + 1, buf, 1, NULL) == 0);

	/*
	 * Compute space required for MB char. This only ever worked in the
	 * C/POSIX locale.
	 */
	assert(sputrune(L'a', NULL, MB_CUR_MAX, &res) == 1);
	assert(res == (char *)1);
	assert(sputrune(L'a', NULL, MB_CUR_MAX, NULL) == 1);

	/*
	 * Japanese (EUC) locale.
	 */

	assert(strcmp(setlocale(LC_CTYPE, "ja_JP.eucJP"), "ja_JP.eucJP") == 0);
	assert(MB_CUR_MAX == 3);

	/* Null wide character. */
	memset(buf, 0xcc, sizeof(buf));
	assert(sputrune(L'\0', buf, 1, &res) == 1);
	assert(res == buf + 1);
	assert((unsigned char)buf[0] == 0 && (unsigned char)buf[1] == 0xcc);
	assert(sputrune(L'\0', buf, 1, NULL) == 1);
	assert(sputrune(L'\0', buf, 2, &res) == 1);
	assert(res == buf + 1);

	/* Latin letter A. */
	memset(buf, 0xcc, sizeof(buf));
	assert(sputrune(L'A', buf, 1, &res) == 1);
	assert(res == buf + 1);
	assert((unsigned char)buf[0] == 'A' && (unsigned char)buf[1] == 0xcc);

	/* Full width letter A. */
	memset(buf, 0xcc, sizeof(buf));
	assert(sputrune(0xa3c1, buf, 2, &res) == 2);
	assert(res == buf + 2);
	assert((unsigned char)buf[0] == 0xa3 &&
		(unsigned char)buf[1] == 0xc1 &&
		(unsigned char)buf[2] == 0xcc);
	memset(buf, 0xcc, sizeof(buf));
	assert(sputrune(0xa3c1, buf, 2, NULL) == 2);
	assert((unsigned char)buf[0] == 0xa3 &&
		(unsigned char)buf[1] == 0xc1 &&
		(unsigned char)buf[2] == 0xcc);

	memset(buf, 0xcc, sizeof(buf));
	assert(sputrune(0xa3c1, buf, 1, &res) == 2);
	assert(res == NULL);
	assert((unsigned char)buf[0] == 0xcc);


	printf("PASS sputrune()\n");

	return (0);
}
