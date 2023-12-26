/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Robert Clausecker
 */

#include <sys/cdefs.h>

#include <dlfcn.h>
#include <limits.h>
#include <string.h>

#include <atf-c.h>

static void *(*memrchr_fn)(const void *, int, size_t);

ATF_TC_WITHOUT_HEAD(null);
ATF_TC_BODY(null, tc)
{
	ATF_CHECK_EQ(memrchr_fn(NULL, 42, 0), NULL);
}

ATF_TC_WITHOUT_HEAD(not_found);
ATF_TC_BODY(not_found, tc)
{
	size_t i, j;
	char buf[1+15+64+1]; /* offset [0..15] + 64 buffer bytes + sentinels */

	buf[0] = 'X';
	memset(buf + 1, '-', sizeof(buf) - 1);

	for (i = 0; i < 16; i++)
		for (j = 0; j < 64; j++) {
			buf[i + j + 1] = 'X';
			ATF_CHECK_EQ(memrchr_fn(buf + i + 1, 'X', j), NULL);
			buf[i + j + 1] = '-';
		}
}

static void
do_found_test(char buf[], size_t len, size_t first, size_t second)
{
	/* invariant: first <= second */

	buf[first] = 'X';
	buf[second] = 'X';
	ATF_CHECK_EQ(memrchr_fn(buf, 'X', len), buf + second);
	buf[first] = '-';
	buf[second] = '-';
}

ATF_TC_WITHOUT_HEAD(found);
ATF_TC_BODY(found, tc)
{
	size_t i, j, k, l;
	char buf[1+15+64+1];

	buf[0] = 'X';
	memset(buf + 1, '-', sizeof(buf) - 1);

	for (i = 0; i < 16; i++)
		for (j = 0; j < 64; j++)
			for (k = 0; k < j; k++)
				for (l = 0; l <= k; l++) {
					buf[i + j + 1] = 'X';
					do_found_test(buf + i + 1, j, l, k);
					buf[i + j + 1] = '-';
				}
}

/* check that the right character is found */
static void
do_values_test(unsigned char buf[], size_t len, size_t i, int c)
{
	/* sentinels */
	buf[-1] = c;
	buf[len] = c;
	memset(buf, c + 1, len);

	if (i < len) {
		buf[i] = c;
		ATF_CHECK_EQ(memrchr_fn(buf, c, len), buf + i);
	} else
		ATF_CHECK_EQ(memrchr_fn(buf, c, len), NULL);
}

ATF_TC_WITHOUT_HEAD(values);
ATF_TC_BODY(values, tc)
{
	size_t i, j, k;
	int c;
	unsigned char buf[1+15+64+1];

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
	memrchr_fn = dlsym(dl_handle, "test_memrchr");
	if (memrchr_fn == NULL)
		memrchr_fn = memrchr;

	ATF_TP_ADD_TC(tp, null);
	ATF_TP_ADD_TC(tp, not_found);
	ATF_TP_ADD_TC(tp, found);
	ATF_TP_ADD_TC(tp, values);

	return (atf_no_error());
}
