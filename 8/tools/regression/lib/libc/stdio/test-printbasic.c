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

/*
 * Tests for basic and miscellaneous printf() formats.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <assert.h>
#include <err.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define	testfmt(result, fmt, ...)	\
	_testfmt((result), __LINE__, #__VA_ARGS__, fmt, __VA_ARGS__)
void _testfmt(const char *, int, const char *, const char *, ...);
void smash_stack(void);

#define	S_UINT64MAX	"18446744073709551615"
#define	S_UINT32MAX	"4294967295"
#define	S_INT64MIN	"-9223372036854775808"
#define	S_INT32MIN	"-2147483648"

#define	S_SIZEMAX	(SIZE_MAX == UINT64_MAX ? S_UINT64MAX : S_UINT32MAX)
#define	S_ULONGMAX	(ULONG_MAX == UINT64_MAX ? S_UINT64MAX : S_UINT32MAX)
#define	S_ULLONGMAX	(ULLONG_MAX == UINT64_MAX ? S_UINT64MAX : S_UINT32MAX)

int
main(int argc, char *argv[])
{

	printf("1..2\n");
	assert(setlocale(LC_NUMERIC, "C"));

	/* The test requires these to be true. */
	assert(UINTMAX_MAX == UINT64_MAX);
	assert(UINT_MAX == UINT32_MAX);
	assert(USHRT_MAX == 0xffff);
	assert(UCHAR_MAX == 0xff);

	/*
	 * Make sure we handle signed vs. unsigned args correctly.
	 */
	testfmt("-1", "%jd", (intmax_t)-1);
	testfmt(S_UINT64MAX, "%ju", UINT64_MAX);

	testfmt("-1", "%td", (ptrdiff_t)-1);
	testfmt(S_SIZEMAX, "%tu", (size_t)-1);

	testfmt("-1", "%zd", (ssize_t)-1);
	testfmt(S_SIZEMAX, "%zu", (ssize_t)-1);

	testfmt("-1", "%ld", (long)-1);
	testfmt(S_ULONGMAX, "%lu", ULONG_MAX);

	testfmt("-1", "%lld", (long long)-1);
	testfmt(S_ULONGMAX, "%lu", ULLONG_MAX);

	testfmt("-1", "%d", -1);
	testfmt(S_UINT32MAX, "%lu", UINT32_MAX);

	testfmt("-1", "%hd", -1);
	testfmt("65535", "%hu", USHRT_MAX);

	testfmt("-1", "%hhd", -1);
	testfmt("255", "%hhu", UCHAR_MAX);

	printf("ok 1 - printbasic signed/unsigned\n");

	/*
	 * Check that printing the largest negative number does not cause
	 * overflow when it is negated.
	 */
	testfmt(S_INT32MIN, "%d", INT_MIN);
	testfmt(S_INT64MIN, "%jd", INTMAX_MIN);

	printf("ok 2 - printbasic INT_MIN\n");


	return (0);
}

void
smash_stack(void)
{
	static uint32_t junk = 0xdeadbeef;
	uint32_t buf[512];
	int i;

	for (i = 0; i < sizeof(buf) / sizeof(buf[0]); i++)
		buf[i] = junk;
}

void
_testfmt(const char *result, int line, const char *argstr, const char *fmt,...)
{
#define	BUF	100
	wchar_t ws[BUF], wfmt[BUF], wresult[BUF];
	char s[BUF];
	va_list ap, ap2;

	va_start(ap, fmt);
	va_copy(ap2, ap);
	smash_stack();
	vsnprintf(s, sizeof(s), fmt, ap);
	if (strcmp(result, s) != 0) {
		fprintf(stderr,
		    "%d: printf(\"%s\", %s) ==> [%s], expected [%s]\n",
		    line, fmt, argstr, s, result);
		abort();
	}

	smash_stack();
	mbstowcs(ws, s, BUF - 1);
	mbstowcs(wfmt, fmt, BUF - 1);
	mbstowcs(wresult, result, BUF - 1);
	vswprintf(ws, sizeof(ws) / sizeof(ws[0]), wfmt, ap2);
	if (wcscmp(wresult, ws) != 0) {
		fprintf(stderr,
		    "%d: wprintf(\"%ls\", %s) ==> [%ls], expected [%ls]\n",
		    line, wfmt, argstr, ws, wresult);
		abort();
	}	
}
