/*-
 * Copyright (C) 2020 Edward Tomasz Napierala <trasz@FreeBSD.org>
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
 * Test for historical qsort_r(3) routine.
 */

#include <stdio.h>
#include <stdlib.h>

#include "test-sort.h"

#define	THUNK 42

static int
sorthelp_r(void *thunk, const void *a, const void *b)
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

ATF_TC_WITHOUT_HEAD(qsort_r_compat_test);
ATF_TC_BODY(qsort_r_compat_test, tc)
{
	int testvector[IVEC_LEN];
	int sresvector[IVEC_LEN];
	int i, j;
	int thunk = THUNK;

	for (j = 2; j < IVEC_LEN; j++) {
		/* Populate test vectors */
		for (i = 0; i < j; i++)
			testvector[i] = sresvector[i] = initvector[i];

		/* Sort using qsort_r(3) */
		qsort_r(testvector, j, sizeof(testvector[0]), &thunk,
		    sorthelp_r);
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

	ATF_TP_ADD_TC(tp, qsort_r_compat_test);

	return (atf_no_error());
}
