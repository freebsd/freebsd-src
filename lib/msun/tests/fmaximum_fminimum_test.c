/*
 * Copyright (c) 2008 David Schultz <das@FreeBSD.org>
 * Copyright (c) 2026 Jesús Blázquez <jesuscblazquez@gmail.com>
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
 * Tests for fmaximum{,f,l}(), fminimum{,f,l}(), fmaximum_mag{,f,l},
 * fminimum_mag{,f,l}, fmaximum_num{,f,l}, fminimum_num{,f,l}, 
 * fmaximum_mag_num{,f,l} and fminimum_mag_num{,f,l}
 */

#include <sys/cdefs.h>
#include <fenv.h>
#include <float.h>
#include <math.h>

#include "test-utils.h"

#pragma STDC FENV_ACCESS ON

/*
 * Test whether func(x, y) has the expected result, and make sure no
 * exceptions are raised.
 */
#define	TEST(func, type, x, y, expected, rmode) do {			      \
	type __x = (x);	/* convert before we clear exceptions */	      \
	type __y = (y);							      \
	ATF_REQUIRE_EQ(0, feclearexcept(ALL_STD_EXCEPT));		      \
	long double __result = func((__x), (__y));			      \
	CHECK_FP_EXCEPTIONS_MSG(0, ALL_STD_EXCEPT,			      \
	    #func "(%.20Lg, %.20Lg) rmode%d", (x), (y), rmode);		      \
	ATF_CHECK_MSG(fpequal_cs(__result, (expected), true),		      \
	    #func "(%.20Lg, %.20Lg) rmode%d = %.20Lg, expected %.20Lg",       \
	    (x), (y), rmode, __result, (expected));			      \
} while (0)

static void
testall_r(long double big, long double small, int rmode)
{
	long double expected_max, expected_min;
	if (isnan(big) || isnan(small)) {
		expected_max = NAN;
		expected_min = expected_max;
	} else {
		expected_max = big;
		expected_min = small;
	}

	TEST(fmaximumf, float, big, small, expected_max, rmode);
	TEST(fmaximumf, float, small, big, expected_max, rmode);
	TEST(fmaximum, double, big, small, expected_max, rmode);
	TEST(fmaximum, double, small, big, expected_max, rmode);
	TEST(fmaximuml, long double, big, small, expected_max, rmode);
	TEST(fmaximuml, long double, small, big, expected_max, rmode);
	TEST(fminimumf, float, big, small, expected_min, rmode);
	TEST(fminimumf, float, small, big, expected_min, rmode);
	TEST(fminimum, double, big, small, expected_min, rmode);
	TEST(fminimum, double, small, big, expected_min, rmode);
	TEST(fminimuml, long double, big, small, expected_min, rmode);
	TEST(fminimuml, long double, small, big, expected_min, rmode);
}

static void
testall_mag_r(long double big, long double small, int rmode) {
	long double expected_max_mag, expected_min_mag;
	if (isnan(big) || isnan(small)) {
		expected_max_mag = NAN;
		expected_min_mag = expected_max_mag;
	} else {
		if (fabsl(small) > fabsl(big)) {
			expected_max_mag = small;
			expected_min_mag = big;
		} else {
			expected_max_mag = big;
			expected_min_mag = small;
		}
	}

	TEST(fmaximum_magf, float, big, small, expected_max_mag, rmode);
	TEST(fmaximum_magf, float, small, big, expected_max_mag, rmode);
	TEST(fmaximum_mag, double, big, small, expected_max_mag, rmode);
	TEST(fmaximum_mag, double, small, big, expected_max_mag, rmode);
	TEST(fmaximum_magl, long double, big, small, expected_max_mag, rmode);
	TEST(fmaximum_magl, long double, small, big, expected_max_mag, rmode);
	TEST(fminimum_magf, float, big, small, expected_min_mag, rmode);
	TEST(fminimum_magf, float, small, big, expected_min_mag, rmode);
	TEST(fminimum_mag, double, big, small, expected_min_mag, rmode);
	TEST(fminimum_mag, double, small, big, expected_min_mag, rmode);
	TEST(fminimum_magl, long double, big, small, expected_min_mag, rmode);
	TEST(fminimum_magl, long double, small, big, expected_min_mag, rmode);
}

static void
testall_num_r(long double big, long double small, int rmode) {
	long double expected_max_num = isnan(big) ? small : big;
	long double expected_min_num = isnan(small) ? big : small;

	TEST(fmaximum_numf, float, big, small, expected_max_num, rmode);
	TEST(fmaximum_numf, float, small, big, expected_max_num, rmode);
	TEST(fmaximum_num, double, big, small, expected_max_num, rmode);
	TEST(fmaximum_num, double, small, big, expected_max_num, rmode);
	TEST(fmaximum_numl, long double, big, small, expected_max_num, rmode);
	TEST(fmaximum_numl, long double, small, big, expected_max_num, rmode);
	TEST(fminimum_numf, float, big, small, expected_min_num, rmode);
	TEST(fminimum_numf, float, small, big, expected_min_num, rmode);
	TEST(fminimum_num, double, big, small, expected_min_num, rmode);
	TEST(fminimum_num, double, small, big, expected_min_num, rmode);
	TEST(fminimum_numl, long double, big, small, expected_min_num, rmode);
	TEST(fminimum_numl, long double, small, big, expected_min_num, rmode);
}

static void
testall_mag_num_r(long double big, long double small, int rmode) {
	long double expected_max_mag_num = isnan(big) ? small : big;
	long double expected_min_mag_num = isnan(small) ? big : small;

	if (fabsl(small) > fabsl(big)) {
		expected_max_mag_num = small;
		expected_min_mag_num = big;
	}

	TEST(fmaximum_mag_numf, float, big, small, expected_max_mag_num, rmode);
	TEST(fmaximum_mag_numf, float, small, big, expected_max_mag_num, rmode);
	TEST(fmaximum_mag_num, double, big, small, expected_max_mag_num, rmode);
	TEST(fmaximum_mag_num, double, small, big, expected_max_mag_num, rmode);
	TEST(fmaximum_mag_numl, long double, big, small, expected_max_mag_num, rmode);
	TEST(fmaximum_mag_numl, long double, small, big, expected_max_mag_num, rmode);
	TEST(fminimum_mag_numf, float, big, small, expected_min_mag_num, rmode);
	TEST(fminimum_mag_numf, float, small, big, expected_min_mag_num, rmode);
	TEST(fminimum_mag_num, double, big, small, expected_min_mag_num, rmode);
	TEST(fminimum_mag_num, double, small, big, expected_min_mag_num, rmode);
	TEST(fminimum_mag_numl, long double, big, small, expected_min_mag_num, rmode);
	TEST(fminimum_mag_numl, long double, small, big, expected_min_mag_num, rmode);
}

/*
 * Test all the functions: fmaximumf, fmaximum, fmaximuml, fminimumf, fminimum, fminimuml
 * in all rounding modes and with the arguments in different orders.
 * The input 'big' must be >= 'small'.
 */
static void
testall(long double big, long double small)
{
	static const int rmodes[] = {
		FE_TONEAREST, FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO
	};
	int i;

	for (i = 0; i < 4; i++) {
		fesetround(rmodes[i]);
		testall_r(big, small, rmodes[i]);
		testall_mag_r(big, small, rmodes[i]);
		testall_num_r(big, small, rmodes[i]);
		testall_mag_num_r(big, small, rmodes[i]);
	}
}

ATF_TC_WITHOUT_HEAD(test1);
ATF_TC_BODY(test1, tc)
{
	testall(1.0, 0.0);
}

ATF_TC_WITHOUT_HEAD(test2);
ATF_TC_BODY(test2, tc)
{
	testall(42.0, nextafterf(42.0, -INFINITY));
}
ATF_TC_WITHOUT_HEAD(test3);
ATF_TC_BODY(test3, tc)
{
	testall(nextafterf(42.0, INFINITY), 42.0);
}

ATF_TC_WITHOUT_HEAD(test4);
ATF_TC_BODY(test4, tc)
{
	testall(-5.0, -5.0);
}

ATF_TC_WITHOUT_HEAD(test5);
ATF_TC_BODY(test5, tc)
{
	testall(-3.0, -4.0);
}

ATF_TC_WITHOUT_HEAD(test6);
ATF_TC_BODY(test6, tc)
{
	testall(1.0, NAN);
}
ATF_TC_WITHOUT_HEAD(test7);
ATF_TC_BODY(test7, tc)
{
	testall(INFINITY, NAN);
}

ATF_TC_WITHOUT_HEAD(test8);
ATF_TC_BODY(test8, tc)
{
	testall(INFINITY, 1.0);
}

ATF_TC_WITHOUT_HEAD(test9);
ATF_TC_BODY(test9, tc)
{
	testall(-3.0, -INFINITY);
}

ATF_TC_WITHOUT_HEAD(test10);
ATF_TC_BODY(test10, tc)
{
	testall(3.0, -INFINITY);
}

ATF_TC_WITHOUT_HEAD(test11);
ATF_TC_BODY(test11, tc)
{
	testall(NAN, NAN);
}

ATF_TC_WITHOUT_HEAD(test12);
ATF_TC_BODY(test12, tc)
{
	testall(0.0, -0.0);
}


ATF_TC_WITHOUT_HEAD(test13);
ATF_TC_BODY(test13, tc)
{
        testall(2.0, -2.0);
}

ATF_TC_WITHOUT_HEAD(test14);
ATF_TC_BODY(test14, tc)
{
        testall(-0.0, -0.0);
}

ATF_TC_WITHOUT_HEAD(test15);
ATF_TC_BODY(test15, tc)
{
        testall(0.0, 0.0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, test1);
	ATF_TP_ADD_TC(tp, test2);
	ATF_TP_ADD_TC(tp, test3);
	ATF_TP_ADD_TC(tp, test4);
	ATF_TP_ADD_TC(tp, test5);
	ATF_TP_ADD_TC(tp, test6);
	ATF_TP_ADD_TC(tp, test7);
	ATF_TP_ADD_TC(tp, test8);
	ATF_TP_ADD_TC(tp, test9);
	ATF_TP_ADD_TC(tp, test10);
	ATF_TP_ADD_TC(tp, test11);
	ATF_TP_ADD_TC(tp, test12);
	ATF_TP_ADD_TC(tp, test13);
	ATF_TP_ADD_TC(tp, test14);
	ATF_TP_ADD_TC(tp, test15);

	return (atf_no_error());
}
