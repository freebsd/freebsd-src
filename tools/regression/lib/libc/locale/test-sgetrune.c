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
 * Test program for the 4.4BSD sgetrune() function.
 *
 * The function is tested with both the "C" ("POSIX") LC_CTYPE setting and
 * "ja_JP.eucJP". Other encodings are not tested.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <assert.h>
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
	rune_t r;
	const char *cres;
	char buf[MB_LEN_MAX + 1];

	/*
	 * C/POSIX locale.
	 */

	assert(MB_CUR_MAX == 1);

	/* Incomplete character sequence. */
	memset(buf, 0xcc, sizeof(buf));
	assert(sgetrune(buf, 0, &cres) == _INVALID_RUNE);
	assert(cres == buf);
	assert(sgetrune(buf, 0, NULL) == _INVALID_RUNE);

	/* Null wide character. */
	buf[0] = '\0';
	assert(sgetrune(buf, 0, &cres) == _INVALID_RUNE);
	assert(cres == buf);
	assert(sgetrune(buf, 0, NULL) == _INVALID_RUNE);
	assert(sgetrune(buf, 1, &cres) == 0);
	assert(cres == buf + 1);
	assert(sgetrune(buf, 2, &cres) == 0);
	assert(cres == buf + 1);
	assert(sgetrune(buf, 1, NULL) == 0);

	/* Latin letter A. */
	buf[0] = 'A';
	assert(sgetrune(buf, 1, &cres) == 'A');
	assert(cres == buf + 1);
	assert(sgetrune(buf, 2, &cres) == 'A');
	assert(cres == buf + 1);
	assert(sgetrune(buf, 1, NULL) == 'A');

	/*
	 * Japanese (EUC) locale.
	 */

	assert(strcmp(setlocale(LC_CTYPE, "ja_JP.eucJP"), "ja_JP.eucJP") == 0);
	assert(MB_CUR_MAX > 1);

	/* Incomplete character sequence (zero length). */
	memset(buf, 0xcc, sizeof(buf));
	assert(sgetrune(buf, 0, &cres) == _INVALID_RUNE);
	assert(cres == buf);
	assert(sgetrune(buf, 0, NULL) == _INVALID_RUNE);

	/* Null wide character. */
	buf[0] = '\0';
	assert(sgetrune(buf, 0, &cres) == _INVALID_RUNE);
	assert(cres == buf);
	assert(sgetrune(buf, 0, NULL) == _INVALID_RUNE);
	assert(sgetrune(buf, 1, &cres) == 0);
	assert(cres == buf + 1);
	assert(sgetrune(buf, 2, &cres) == 0);
	assert(cres == buf + 1);
	assert(sgetrune(buf, 1, NULL) == 0);

	/* Latin letter A. */
	buf[0] = 'A';
	assert(sgetrune(buf, 1, &cres) == 'A');
	assert(cres == buf + 1);
	assert(sgetrune(buf, 2, &cres) == 'A');
	assert(cres == buf + 1);
	assert(sgetrune(buf, 1, NULL) == 'A');

	/* Incomplete character sequence (truncated double-byte). */
	memset(buf, 0xcc, sizeof(buf));
	buf[0] = 0xa3;
	buf[1] = 0x00;
	assert(sgetrune(buf, 1, &cres) == _INVALID_RUNE);
	assert(cres == buf);
	assert(sgetrune(buf, 1, NULL) == _INVALID_RUNE);

	/* Same as above, but complete. */
	buf[1] = 0xc1;
	assert(sgetrune(buf, 2, &cres) == 0xa3c1);
	assert(cres == buf + 2);
	assert(sgetrune(buf, 2, NULL) == 0xa3c1);

	printf("PASS sgetrune()\n");

	return (0);
}
