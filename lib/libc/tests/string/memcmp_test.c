/*-
 * Copyright (c) 2016 Jilles Tjoelker <jilles@FreeBSD.org>
 * Copyright (c) 2023 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Robert Clausecker
 * <fuz@FreeBSD.org> under sponsorship from the FreeBSD Foundation.
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

#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <atf-c.h>

#ifndef MEMCMP
#define MEMCMP memcmp
#endif

/*
 * On FreeBSD we previously demanded that memcmp returns the difference
 * between the characters at the first site of mismatch.  However,
 * ISO/IEC 9899:1990 only specifies that a number greater than, equal
 * to, or less than zero shall be returned.  If a unit test for the
 * more strict behaviour is desired, define RES(x) to be (x).
 */
#ifndef RES
#define RES(x) (((x) > 0) - ((x) < 0))
#endif

static int (*memcmp_fn)(const void *, const void *, size_t);

static void
check_memcmp(const char *a, const char *b, size_t len, int expected)
{
	int got;

	got = memcmp_fn(a, b, len);
	ATF_CHECK_EQ_MSG(RES(expected), RES(got),
	    "%s(%p, %p, %zu) gave %d, but wanted %d",
	    __XSTRING(MEMCMP), a, b, len, got, expected);
}

ATF_TC_WITHOUT_HEAD(zero);
ATF_TC_BODY(zero, tc)
{

	check_memcmp("a", "b", 0, 0);
	check_memcmp("", "", 0, 0);
}

ATF_TC_WITHOUT_HEAD(eq);
ATF_TC_BODY(eq, tc)
{
	unsigned char data1[256], data2[256];
	int i;

	for (i = 0; i < 256; i++)
		data1[i] = data2[i] = i ^ 0x55;
	for (i = 1; i < 256; i++)
		check_memcmp(data1, data2, i, 0);
	for (i = 1; i < 256; i++)
		check_memcmp(data1 + i, data2 + i, 256 - i, 0);
}

ATF_TC_WITHOUT_HEAD(neq);
ATF_TC_BODY(neq, tc)
{
	unsigned char data1[256], data2[256];
	int i;

	for (i = 0; i < 256; i++) {
		data1[i] = i;
		data2[i] = i ^ 0x55;
	}
	for (i = 1; i < 256; i++)
		check_memcmp(data1, data2, i, -0x55);
	for (i = 1; i < 256; i++)
		check_memcmp(data1 + i, data2 + i, 256 - i, i - (i ^ 0x55));
}

ATF_TC_WITHOUT_HEAD(diff);
ATF_TC_BODY(diff, tc)
{
	unsigned char data1[256], data2[256];
	int i;

	memset(data1, 'a', sizeof(data1));
	memset(data2, 'a', sizeof(data2));
	data1[128] = 255;
	data2[128] = 0;
	for (i = 1; i < 66; i++) {
		check_memcmp(data1 + 128, data2 + 128, i, 255);
		check_memcmp(data2 + 128, data1 + 128, i, -255);
		check_memcmp(data1 + 129 - i, data2 + 129 - i, i, 255);
		check_memcmp(data2 + 129 - i, data1 + 129 - i, i, -255);
		check_memcmp(data1 + 129 - i, data2 + 129 - i, i * 2, 255);
		check_memcmp(data2 + 129 - i, data1 + 129 - i, i * 2, -255);
	}
	data1[128] = 'c';
	data2[128] = 'e';
	for (i = 1; i < 66; i++) {
		check_memcmp(data1 + 128, data2 + 128, i, -2);
		check_memcmp(data2 + 128, data1 + 128, i, 2);
		check_memcmp(data1 + 129 - i, data2 + 129 - i, i, -2);
		check_memcmp(data2 + 129 - i, data1 + 129 - i, i, 2);
		check_memcmp(data1 + 129 - i, data2 + 129 - i, i * 2, -2);
		check_memcmp(data2 + 129 - i, data1 + 129 - i, i * 2, 2);
	}
	memset(data1 + 129, 'A', sizeof(data1) - 129);
	memset(data2 + 129, 'Z', sizeof(data2) - 129);
	for (i = 1; i < 66; i++) {
		check_memcmp(data1 + 128, data2 + 128, i, -2);
		check_memcmp(data2 + 128, data1 + 128, i, 2);
		check_memcmp(data1 + 129 - i, data2 + 129 - i, i, -2);
		check_memcmp(data2 + 129 - i, data1 + 129 - i, i, 2);
		check_memcmp(data1 + 129 - i, data2 + 129 - i, i * 2, -2);
		check_memcmp(data2 + 129 - i, data1 + 129 - i, i * 2, 2);
	}
}

ATF_TP_ADD_TCS(tp)
{
	void *dl_handle;

	dl_handle = dlopen(NULL, RTLD_LAZY);
	memcmp_fn = dlsym(dl_handle, "test_" __XSTRING(MEMCMP));
	if (memcmp_fn == NULL)
		memcmp_fn = MEMCMP;

	ATF_TP_ADD_TC(tp, zero);
	ATF_TP_ADD_TC(tp, eq);
	ATF_TP_ADD_TC(tp, neq);
	ATF_TP_ADD_TC(tp, diff);

	return (atf_no_error());
}
