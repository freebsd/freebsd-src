/*
 * Copyright (c) 2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Test for bsearch_b() routine.
 */

#include <stdlib.h>

#include "test-search.h"

#define	THUNK 42

static void *
do_bsearch_b(const int *key, const int *base, size_t n, void *ctx)
{
	int thunk = *(int *)ctx;

	return (bsearch_b(key, base, n, sizeof(int),
	    ^(const void *a, const void *b) {
		ATF_REQUIRE_EQ(thunk, THUNK);
		return (searchhelp(a, b));
	    }));
}

ATF_TC_WITHOUT_HEAD(bsearch_b_test);
ATF_TC_BODY(bsearch_b_test, tc)
{
	int testvector[SVEC_LEN];
	int thunk = THUNK;
	int key, j;

	for (j = 0; j <= SVEC_LEN; j++) {
		if (j == 0) {
			key = 0;
			ATF_CHECK(bsearch_b(&key, testvector, 0,
			    sizeof(testvector[0]),
			    ^(const void *a __unused, const void *b __unused) {
				atf_tc_fail(
				    "comparison block invoked unexpectedly");
				return (0);
			    }) == NULL);
			continue;
		}
		check_sorted_search(do_bsearch_b, &thunk, testvector,
		    (size_t)j);
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, bsearch_b_test);

	return (atf_no_error());
}
