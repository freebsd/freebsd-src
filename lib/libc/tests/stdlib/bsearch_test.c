/*
 * Copyright (c) 2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Test for bsearch() routine.
 */

#include <stdlib.h>

#include "test-search.h"

static const void *expected_key;
static const void *expected_base;
static size_t expected_nmemb;

static int
searchhelp_check(const void *a, const void *b)
{
	const char *p, *base;

	ATF_CHECK(a == expected_key);
	p = b;
	base = expected_base;
	ATF_CHECK(((size_t)(p - base) % sizeof(int)) == 0);
	ATF_CHECK(p >= base);
	ATF_CHECK(p < base + expected_nmemb * sizeof(int));
	return (searchhelp(a, b));
}

static int
searchhelp_never(const void *a __unused, const void *b __unused)
{
	atf_tc_fail("comparison function invoked unexpectedly");
	return (0);
}

static void *
do_bsearch(const int *key, const int *base, size_t n, void *ctx __unused)
{
	expected_key = key;
	return (bsearch(key, base, n, sizeof(int), searchhelp_check));
}

ATF_TC_WITHOUT_HEAD(bsearch_test);
ATF_TC_BODY(bsearch_test, tc)
{
	int testvector[SVEC_LEN];
	int key, j;

	for (j = 0; j <= SVEC_LEN; j++) {
		if (j == 0) {
			key = 0;
			ATF_CHECK(bsearch(&key, testvector, 0,
			    sizeof(testvector[0]), searchhelp_never) == NULL);
			continue;
		}
		expected_base = testvector;
		expected_nmemb = (size_t)j;
		check_sorted_search(do_bsearch, NULL, testvector, (size_t)j);
	}
}

ATF_TC_WITHOUT_HEAD(bsearch_duplicates);
ATF_TC_BODY(bsearch_duplicates, tc)
{
	int d[] = { 1, 2, 2, 2, 3 };
	int e[] = { 7, 7, 7 };
	int key, *found;

	key = 2;
	found = bsearch(&key, d, nitems(d), sizeof(d[0]), searchhelp);
	ATF_REQUIRE(found != NULL);
	ATF_CHECK(found >= &d[1] && found <= &d[3]);
	ATF_CHECK_EQ(*found, 2);

	key = 7;
	found = bsearch(&key, e, nitems(e), sizeof(e[0]), searchhelp);
	ATF_REQUIRE(found != NULL);
	ATF_CHECK(found >= &e[0] && found <= &e[2]);
	ATF_CHECK_EQ(*found, 7);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, bsearch_test);
	ATF_TP_ADD_TC(tp, bsearch_duplicates);

	return (atf_no_error());
}
