/*-
 * Copyright (c) 2009 David Schultz <das@FreeBSD.org>
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

#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

int
main(int argc, char *argv[])
{

	printf("1..6\n");

	setlocale(LC_CTYPE, "C");

	assert(wcscasecmp(L"", L"") == 0);
	assert(wcsncasecmp(L"", L"", 50) == 0);
	assert(wcsncasecmp(L"", L"", 0) == 0);
	printf("ok 1 - wcscasecmp\n");

	assert(wcscasecmp(L"abc", L"abc") == 0);
	assert(wcscasecmp(L"ABC", L"ABC") == 0);
	assert(wcscasecmp(L"abc", L"ABC") == 0);
	assert(wcscasecmp(L"ABC", L"abc") == 0);
	printf("ok 2 - wcscasecmp\n");

	assert(wcscasecmp(L"abc", L"xyz") < 0);
	assert(wcscasecmp(L"ABC", L"xyz") < 0);
	assert(wcscasecmp(L"abc", L"XYZ") < 0);
	assert(wcscasecmp(L"ABC", L"XYZ") < 0);
	assert(wcscasecmp(L"xyz", L"abc") > 0);
	assert(wcscasecmp(L"XYZ", L"abc") > 0);
	assert(wcscasecmp(L"xyz", L"ABC") > 0);
	assert(wcscasecmp(L"XYZ", L"ABC") > 0);
	printf("ok 3 - wcscasecmp\n");

	assert(wcscasecmp(L"abc", L"ABCD") < 0);
	assert(wcscasecmp(L"ABC", L"abcd") < 0);
	assert(wcscasecmp(L"abcd", L"ABC") > 0);
	assert(wcscasecmp(L"ABCD", L"abc") > 0);
	printf("ok 4 - wcscasecmp\n");

	assert(wcsncasecmp(L"abc", L"ABCD", 4) < 0);
	assert(wcsncasecmp(L"ABC", L"abcd", 4) < 0);
	assert(wcsncasecmp(L"abcd", L"ABC", 4) > 0);
	assert(wcsncasecmp(L"ABCD", L"abc", 4) > 0);
	assert(wcsncasecmp(L"abc", L"ABCD", 3) == 0);
	assert(wcsncasecmp(L"ABC", L"abcd", 3) == 0);
	printf("ok 5 - wcsncasecmp\n");

	assert(wcscasecmp(L"λ", L"Λ") != 0);
	setlocale(LC_CTYPE, "el_GR.UTF-8");
	assert(wcscasecmp(L"λ", L"Λ") == 0);
	assert(wcscasecmp(L"λ", L"Ω") < 0);
	assert(wcscasecmp(L"Ω", L"λ") > 0);
	printf("ok 6 - greek\n");

	exit(0);
}
