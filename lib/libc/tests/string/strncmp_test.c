/*-
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * This software was developed by Robert Clausecker <fuz@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ''AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE
 */

#include <sys/cdefs.h>

#include <atf-c.h>
#include <dlfcn.h>
#include <string.h>

int (*volatile strncmp_fn)(const char *, const char *, size_t);

static void
alignment_testcase(char *a, char *b, int want, size_t len)
{
	int res;

	res = strncmp_fn(a, b, len);
	ATF_CHECK_MSG(want == (res > 0) - (res < 0),
	    "strcmp(%p \"%s\", %p \"%s\", %zu) = %d != %d",
	    (void *)a, a, (void *)b, b, len, res, want);
}

static void
check_strncmp_alignments(char a[], char b[],
    size_t a_off, size_t b_off, size_t len, size_t pos)
{
	char *a_str, *b_str, a_orig, b_orig;

	a[a_off] = '\0';
	b[b_off] = '\0';

	a_str = a + a_off + 1;
	b_str = b + b_off + 1;

	a_str[len] = '\0';
	b_str[len] = '\0';
	a_str[len+1] = 'A';
	b_str[len+1] = 'B';

	a_orig = a_str[pos];
	b_orig = b_str[pos];

	alignment_testcase(a_str, b_str, 0, len + 16);
	alignment_testcase(a_str, b_str, 0, len + 1);
	alignment_testcase(a_str, b_str, 0, len);

	if (pos < len) {
		a_str[pos] = '\0';
		alignment_testcase(a_str, b_str, -1, len + 16);
		alignment_testcase(a_str, b_str, -1, len + 1);
		alignment_testcase(a_str, b_str, -1, len);
		alignment_testcase(a_str, b_str, -1, pos + 1);
		alignment_testcase(a_str, b_str, 0, pos);
		a_str[pos] = a_orig;

		b_str[pos] = '\0';
		alignment_testcase(a_str, b_str, 1, len + 16);
		alignment_testcase(a_str, b_str, 1, len + 1);
		alignment_testcase(a_str, b_str, 1, len);
		alignment_testcase(a_str, b_str, 1, pos + 1);
		alignment_testcase(a_str, b_str, 0, pos);
		b_str[pos] = b_orig;
	}

	a_str[pos] = 'X';
	alignment_testcase(a_str, b_str, 1, len + 16);
	alignment_testcase(a_str, b_str, 0, pos);
	alignment_testcase(a_str, b_str, 1, pos + 1);
	if (pos < len) {
		alignment_testcase(a_str, b_str, 1, len);
		alignment_testcase(a_str, b_str, 1, len + 1);
	}
	a_str[pos] = a_orig;

	b_str[pos] = 'X';
	alignment_testcase(a_str, b_str, -1, len + 16);
	alignment_testcase(a_str, b_str, 0, pos);
	alignment_testcase(a_str, b_str, -1, pos + 1);
	if (pos < len) {
		alignment_testcase(a_str, b_str, -1, len);
		alignment_testcase(a_str, b_str, -1, len + 1);
	}
	b_str[pos] = b_orig;

	a[a_off] = '-';
	b[b_off] = '-';
	a_str[len] = '-';
	b_str[len] = '-';
	a_str[len+1] = '-';
	b_str[len+1] = '-';
}

ATF_TC(strncmp_alignments);
ATF_TC_HEAD(strncmp_alignments, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test strncmp(3) with various alignments");
}

ATF_TC_BODY(strncmp_alignments, tc)
{
	size_t a_off, b_off, len, pos;
	char a[64+16+16+3], b[64+16+16+3];

	memset(a, '-', sizeof(a));
	memset(b, '-', sizeof(b));
	a[sizeof(a) - 1] = '\0';
	b[sizeof(b) - 1] = '\0';

	for (a_off = 0; a_off < 16; a_off++)
		for (b_off = 0; b_off < 16; b_off++)
			for (len = 1; len <= 64; len++)
				for (pos = 0; pos <= len; pos++)
					check_strncmp_alignments(a, b, a_off, b_off, len, pos);
}

ATF_TC(strncmp_null);
ATF_TC_HEAD(strncmp_null, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test strncmp(3) with null pointers");
}

ATF_TC_BODY(strncmp_null, tc)
{
	alignment_testcase(NULL, NULL, 0, 0);
}

ATF_TP_ADD_TCS(tp)
{
	void *dl_handle;

	dl_handle = dlopen(NULL, RTLD_LAZY);
	strncmp_fn = dlsym(dl_handle, "test_strncmp");
	if (strncmp_fn == NULL)
		strncmp_fn = strncmp;

	ATF_TP_ADD_TC(tp, strncmp_alignments);
	ATF_TP_ADD_TC(tp, strncmp_null);

	return atf_no_error();
}
