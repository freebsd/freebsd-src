/*
 * Copyright (c) 2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Test for bsearch_s() routine.
 */

#include <stdint.h>
#include <stdlib.h>

#define	THUNK 42

#include "test-search.h"

static errno_t error_code;
static int compar_calls;

static int
searchhelp_s(const void *a, const void *b, void *thunk)
{
	compar_calls++;
	if (thunk != NULL)
		ATF_REQUIRE_EQ(*(int *)thunk, THUNK);
	return (searchhelp(a, b));
}

static void
constraint_handler(const char * restrict msg __unused,
    void * restrict ptr __unused, errno_t error)
{
	error_code = error;
}

static void
expect_viol(const void *key, const void *base, rsize_t nmemb, rsize_t size,
    int (*compar)(const void *, const void *, void *), void *thunk)
{
	error_code = 0;
	compar_calls = 0;
	ATF_CHECK(bsearch_s(key, base, nmemb, size, compar, thunk) == NULL);
	ATF_CHECK(error_code > 0);
	ATF_CHECK_EQ(compar_calls, 0);
}

static void *
do_bsearch_s(const int *key, const int *base, size_t n, void *ctx)
{
	return (bsearch_s(key, base, n, sizeof(int), searchhelp_s, ctx));
}

ATF_TC_WITHOUT_HEAD(bsearch_s_constraints);
ATF_TC_BODY(bsearch_s_constraints, tc)
{
	int thunk = THUNK;
	int key = 4;
	int b[] = { 4, 7, 81 };

	set_constraint_handler_s(constraint_handler);
	expect_viol(&key, b, -1, sizeof(int), searchhelp_s, &thunk);
	expect_viol(&key, b, RSIZE_MAX + 1, sizeof(int), searchhelp_s, &thunk);
	expect_viol(&key, b, nitems(b), -1, searchhelp_s, &thunk);
	expect_viol(&key, b, nitems(b), RSIZE_MAX + 1, searchhelp_s, &thunk);
	expect_viol(NULL, b, nitems(b), sizeof(int), searchhelp_s, &thunk);
	expect_viol(&key, NULL, 1, sizeof(int), searchhelp_s, &thunk);
	expect_viol(&key, b, nitems(b), sizeof(int), NULL, &thunk);
	/* size > RSIZE_MAX is a violation even when nmemb is zero. */
	expect_viol(&key, b, 0, RSIZE_MAX + 1, searchhelp_s, &thunk);
}

ATF_TC_WITHOUT_HEAD(bsearch_s_nmemb_zero);
ATF_TC_BODY(bsearch_s_nmemb_zero, tc)
{
	int thunk = THUNK;
	int key = 4;
	int b[] = { 4, 7, 81 };

	error_code = 0;
	compar_calls = 0;
	set_constraint_handler_s(constraint_handler);
	ATF_CHECK(bsearch_s(&key, b, 0, sizeof(int), searchhelp_s,
	    &thunk) == NULL);
	ATF_CHECK(error_code == 0);
	ATF_CHECK_EQ(compar_calls, 0);
	ATF_CHECK(bsearch_s(NULL, NULL, 0, 0, NULL, NULL) == NULL);
	ATF_CHECK(error_code == 0);
}

ATF_TC_WITHOUT_HEAD(bsearch_s_h);
ATF_TC_BODY(bsearch_s_h, tc)
{
	int thunk = THUNK;
	int b[] = { 4, 7, 81 };
	int key = 7;
	int *found;

	error_code = 0;
	compar_calls = 0;
	set_constraint_handler_s(constraint_handler);
	found = bsearch_s(&key, b, nitems(b), sizeof(int), searchhelp_s,
	    &thunk);
	ATF_CHECK(error_code == 0);
	ATF_CHECK(compar_calls > 0);
	ATF_CHECK(found == &b[1]);

	compar_calls = 0;
	found = bsearch_s(&key, b, nitems(b), sizeof(int), searchhelp_s, NULL);
	ATF_CHECK(found == &b[1]);
	ATF_CHECK(compar_calls > 0);
}

ATF_TC_WITHOUT_HEAD(bsearch_s_test);
ATF_TC_BODY(bsearch_s_test, tc)
{
	int testvector[SVEC_LEN];
	int thunk = THUNK;
	int j;

	for (j = 1; j <= SVEC_LEN; j++)
		check_sorted_search(do_bsearch_s, &thunk, testvector,
		    (size_t)j);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, bsearch_s_constraints);
	ATF_TP_ADD_TC(tp, bsearch_s_nmemb_zero);
	ATF_TP_ADD_TC(tp, bsearch_s_h);
	ATF_TP_ADD_TC(tp, bsearch_s_test);

	return (atf_no_error());
}
