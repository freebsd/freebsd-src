/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023, 2026 Robert Clausecker <fuz@FreeBSD.org>
 *
 * Adapted from memrchr_test.c.
 */

#include <sys/cdefs.h>

#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <atf-c.h>

static char *(*strrchr_fn)(const char *, int);

/*
 * Check that when looking for the character NUL, we find the
 * string terminator, and not some NUL character after it.
 */
ATF_TC_WITHOUT_HEAD(nul);
ATF_TC_BODY(nul, tc)
{
	size_t i, j, k;
	char buf[1+15+64]; /* offset [0+15] + 64 buffer bytes + sentinels */

	buf[0] = '\0';
	memset(buf + 1, '-', sizeof(buf) - 1);

	for (i = 0; i < 16; i++)
		for (j = 0; j < 64; j++)
			for (k = j; k < 64; k++) {
				buf[i + j + 1] = '\0';
				buf[i + k + 1] = '\0';
				ATF_CHECK_EQ(strrchr_fn(buf + i + 1, '\0'), buf + i + j + 1);
				buf[i + j + 1] = '-';
				buf[i + k + 1] = '-';
			}
}

/*
 * Check that if the character 'X' does not occur in the string
 * (but occurs before and after it), we correctly return NULL.
 */
ATF_TC_WITHOUT_HEAD(not_found);
ATF_TC_BODY(not_found, tc)
{
	size_t i, j;
	char buf[1+15+64+2]; /* offset [0..15] + 64 buffer bytes + sentinels */

	buf[0] = 'X';
	memset(buf + 1, '-', sizeof(buf) - 1);

	for (i = 0; i < 16; i++)
		for (j = 0; j < 64; j++) {
			buf[i + j + 1] = '\0';
			buf[i + j + 2] = 'X';
			ATF_CHECK_EQ(strrchr_fn(buf + i + 1, 'X'), NULL);
			buf[i + j + 1] = '-';
			buf[i + j + 2] = '-';
		}
}

static void
do_found_test(char buf[], size_t first, size_t second)
{
	/* invariant: first <= second */

	buf[first] = 'X';
	buf[second] = 'X';
	ATF_CHECK_EQ(strrchr_fn(buf, 'X'), buf + second);
	buf[first] = '-';
	buf[second] = '-';
}

/*
 * Check that if the character 'X' occurs in the string multiple
 * times (i. e. twice), its last encounter is returned.
 */
ATF_TC_WITHOUT_HEAD(found);
ATF_TC_BODY(found, tc)
{
	size_t i, j, k, l;
	char buf[1+15+64+2];

	buf[0] = 'X';
	memset(buf + 1, '-', sizeof(buf) - 1);

	for (i = 0; i < 16; i++)
		for (j = 0; j < 64; j++)
			for (k = 0; k < j; k++)
				for (l = 0; l <= k; l++) {
					buf[i + j + 1] = '\0';
					buf[i + j + 2] = 'X';
					do_found_test(buf + i + 1, l, k);
					buf[i + j + 1] = '-';
					buf[i + j + 2] = '-';
				}
}

static void
do_values_test(char buf[], size_t len, size_t i, int c)
{
	/* sentinels */
	buf[-1] = c;
	buf[len] = '\0';
	buf[len + 1] = 'c';

	/* fill the string with some other character, but not with NUL */
	memset(buf, c == UCHAR_MAX ? c - 1 : c + 1, len);

	if (i < len) {
		buf[i] = c;
		ATF_CHECK_EQ(strrchr_fn(buf, c), buf + i);
	} else
		ATF_CHECK_EQ(strrchr_fn(buf, c), c == 0 ? buf + len : NULL);
}

/*
 * Check that the character is found regardless of its value.
 * This catches arithmetic (overflow) errors in incorrect SWAR
 * implementations of byte-parallel character matching.
 */
ATF_TC_WITHOUT_HEAD(values);
ATF_TC_BODY(values, tc)
{
	size_t i, j, k;
	int c;
	char buf[1+15+64+2];

	for (i = 0; i < 16; i++)
		for (j = 0; j < 64; j++)
			for (k = 0; k <= j; k++)
				for (c = 0; c <= UCHAR_MAX; c++)
					do_values_test(buf + i + 1, j, k, c);
}

ATF_TP_ADD_TCS(tp)
{
	void *dl_handle;

	dl_handle = dlopen(NULL, RTLD_LAZY);
	strrchr_fn = dlsym(dl_handle, "test_strrchr");
	if (strrchr_fn == NULL)
		strrchr_fn = strrchr;

	ATF_TP_ADD_TC(tp, nul);
	ATF_TP_ADD_TC(tp, not_found);
	ATF_TP_ADD_TC(tp, found);
	ATF_TP_ADD_TC(tp, values);

	return (atf_no_error());
}
