/*-
 * Copyright (C) 2020 Edward Tomasz Napierala <trasz@FreeBSD.org>
 * Copyright (c) 2017 Juniper Networks.  All rights reserved.
 * Copyright (C) 2004 Maxim Sobolev <sobomax@FreeBSD.org>
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
 * Test for qsort_s(3) routine.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define	THUNK 42

#include "test-sort.h"

static errno_t e;

static int
sorthelp_s(const void *a, const void *b, void *thunk)
{
	const int *oa, *ob;

	ATF_REQUIRE_EQ(*(int *)thunk, THUNK);

	oa = a;
	ob = b;
	/* Don't use "return *oa - *ob" since it's easy to cause overflow! */
	if (*oa > *ob)
		return (1);
	if (*oa < *ob)
		return (-1);
	return (0);
}

void
h(const char * restrict msg __unused, void * restrict ptr __unused, errno_t error)
{
	e = error;
}

/* nmemb < 0 */
ATF_TC_WITHOUT_HEAD(qsort_s_nmemb_lt_zero);
ATF_TC_BODY(qsort_s_nmemb_lt_zero, tc)
{
	int thunk = THUNK;
	int b;

	ATF_CHECK(qsort_s(&b, -1, sizeof(int), sorthelp_s, &thunk) != 0);
}

/* nmemb > rmax */
ATF_TC_WITHOUT_HEAD(qsort_s_nmemb_gt_rmax);
ATF_TC_BODY(qsort_s_nmemb_gt_rmax, tc)
{
	int thunk = THUNK;
	int b;

	ATF_CHECK(qsort_s(&b, RSIZE_MAX + 1, sizeof(int), sorthelp_s, &thunk) != 0);
}

/* size < 0 */
ATF_TC_WITHOUT_HEAD(qsort_s_size_lt_zero);
ATF_TC_BODY(qsort_s_size_lt_zero, tc)
{
	int thunk = THUNK;
	int b;

	ATF_CHECK(qsort_s(&b, 1, -1, sorthelp_s, &thunk) != 0);
}

/* size > rmax */
ATF_TC_WITHOUT_HEAD(qsort_s_size_gt_rmax);
ATF_TC_BODY(qsort_s_size_gt_rmax, tc)
{
	int thunk = THUNK;
	int b;

	ATF_CHECK(qsort_s(&b, 1, RSIZE_MAX + 1, sorthelp_s, &thunk) != 0);
}

/* NULL compar */
ATF_TC_WITHOUT_HEAD(qsort_s_null_compar);
ATF_TC_BODY(qsort_s_null_compar, tc)
{
	int thunk = THUNK;
	int b;

	ATF_CHECK(qsort_s(&b, 1, sizeof(int), NULL, &thunk) != 0);
}

/* nmemb < 0, handler */
ATF_TC_WITHOUT_HEAD(qsort_s_nmemb_lt_zero_h);
ATF_TC_BODY(qsort_s_nmemb_lt_zero_h, tc)
{
	int thunk = THUNK;
	int b[] = {81, 4, 7};

	e = 0;
	set_constraint_handler_s(h);
	ATF_CHECK(qsort_s(&b, -1, sizeof(int), sorthelp_s, &thunk) != 0);
	ATF_CHECK(e > 0);
	ATF_CHECK_EQ(b[0], 81);
	ATF_CHECK_EQ(b[1], 4);
	ATF_CHECK_EQ(b[2], 7);
}

/* nmemb > rmax, handler */
ATF_TC_WITHOUT_HEAD(qsort_s_nmemb_gt_rmax_h);
ATF_TC_BODY(qsort_s_nmemb_gt_rmax_h, tc)
{
	int thunk = THUNK;
	int b[] = {81, 4, 7};

	e = 0;
	set_constraint_handler_s(h);
	ATF_CHECK(qsort_s(&b, RSIZE_MAX + 1, sizeof(int), sorthelp_s, &thunk) != 0);
	ATF_CHECK(e > 0);
	ATF_CHECK_EQ(b[0], 81);
	ATF_CHECK_EQ(b[1], 4);
	ATF_CHECK_EQ(b[2], 7);
}

/* size < 0, handler */
ATF_TC_WITHOUT_HEAD(qsort_s_size_lt_zero_h);
ATF_TC_BODY(qsort_s_size_lt_zero_h, tc)
{
	int thunk = THUNK;
	int b[] = {81, 4, 7};

	e = 0;
	set_constraint_handler_s(h);
	ATF_CHECK(qsort_s(&b, nitems(b), -1, sorthelp_s, &thunk) != 0);
	ATF_CHECK(e > 0);
	ATF_CHECK_EQ(b[0], 81);
	ATF_CHECK_EQ(b[1], 4);
	ATF_CHECK_EQ(b[2], 7);
}

/* size > rmax, handler */
ATF_TC_WITHOUT_HEAD(qsort_s_size_gt_rmax_h);
ATF_TC_BODY(qsort_s_size_gt_rmax_h, tc)
{
	int thunk = THUNK;
	int b[] = {81, 4, 7};

	e = 0;
	set_constraint_handler_s(h);
	ATF_CHECK(qsort_s(&b, nitems(b), RSIZE_MAX + 1, sorthelp_s, &thunk) != 0);
	ATF_CHECK(e > 0);
	ATF_CHECK_EQ(b[0], 81);
	ATF_CHECK_EQ(b[1], 4);
	ATF_CHECK_EQ(b[2], 7);
}

/* NULL compar, handler */
ATF_TC_WITHOUT_HEAD(qsort_s_null_compar_h);
ATF_TC_BODY(qsort_s_null_compar_h, tc)
{
	int thunk = THUNK;
	int b[] = {81, 4, 7};

	e = 0;
	set_constraint_handler_s(h);
	ATF_CHECK(qsort_s(&b, nitems(b), sizeof(int), NULL, &thunk) != 0);
	ATF_CHECK(e > 0);
	ATF_CHECK_EQ(b[0], 81);
	ATF_CHECK_EQ(b[1], 4);
	ATF_CHECK_EQ(b[2], 7);
}

ATF_TC_WITHOUT_HEAD(qsort_s_h);
ATF_TC_BODY(qsort_s_h, tc)
{
	int thunk = THUNK;
	int b[] = {81, 4, 7};

	e = 0;
	set_constraint_handler_s(h);
	ATF_CHECK(qsort_s(&b, nitems(b), sizeof(int), sorthelp_s, &thunk) == 0);
	ATF_CHECK(e == 0);
	ATF_CHECK_EQ(b[0], 4);
	ATF_CHECK_EQ(b[1], 7);
	ATF_CHECK_EQ(b[2], 81);
}

ATF_TC_WITHOUT_HEAD(qsort_s_test);
ATF_TC_BODY(qsort_s_test, tc)
{
	int testvector[IVEC_LEN];
	int sresvector[IVEC_LEN];
	int i, j;
	int thunk = THUNK;

	for (j = 2; j < IVEC_LEN; j++) {
		/* Populate test vectors */
		for (i = 0; i < j; i++)
			testvector[i] = sresvector[i] = initvector[i];

		/* Sort using qsort_s(3) */
		qsort_s(testvector, j, sizeof(testvector[0]),
		    sorthelp_s, &thunk);
		/* Sort using reference slow sorting routine */
		ssort(sresvector, j);

		/* Compare results */
		for (i = 0; i < j; i++)
			ATF_CHECK_MSG(testvector[i] == sresvector[i],
			    "item at index %d didn't match: %d != %d",
			    i, testvector[i], sresvector[i]);
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, qsort_s_nmemb_lt_zero);
	ATF_TP_ADD_TC(tp, qsort_s_nmemb_gt_rmax);
	ATF_TP_ADD_TC(tp, qsort_s_size_lt_zero);
	ATF_TP_ADD_TC(tp, qsort_s_size_gt_rmax);
	ATF_TP_ADD_TC(tp, qsort_s_null_compar);
	ATF_TP_ADD_TC(tp, qsort_s_nmemb_lt_zero_h);
	ATF_TP_ADD_TC(tp, qsort_s_nmemb_gt_rmax_h);
	ATF_TP_ADD_TC(tp, qsort_s_size_lt_zero_h);
	ATF_TP_ADD_TC(tp, qsort_s_size_gt_rmax_h);
	ATF_TP_ADD_TC(tp, qsort_s_null_compar_h);
	ATF_TP_ADD_TC(tp, qsort_s_h);
	ATF_TP_ADD_TC(tp, qsort_s_test);

	return (atf_no_error());
}
