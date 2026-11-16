/*
 * Copyright (c) 2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/cdefs.h>
#include <fenv.h>
#include <float.h>
#include <math.h>

#include "test-utils.h"

#pragma STDC FENV_ACCESS ON

/*
 * Test that iseqsig(x, y) returns exactly `expected' and raises exactly
 * `excepts' out of the standard exceptions.
 */
#define	TEST(tx, x, ty, y, expected, excepts) do {			\
	volatile tx __x = (x);						\
	volatile ty __y = (y);						\
	int __r;							\
	ATF_REQUIRE_EQ(0, feclearexcept(FE_ALL_EXCEPT));		\
	__r = iseqsig(__x, __y);					\
	CHECK_FP_EXCEPTIONS_MSG((excepts), ALL_STD_EXCEPT,		\
	    "iseqsig((" #tx ")%La, (" #ty ")%La)",			\
	    (long double)__x, (long double)__y);			\
	ATF_CHECK_EQ_MSG((expected), __r,				\
	    "iseqsig((" #tx ")%La, (" #ty ")%La) = %d, expected %d",	\
	    (long double)__x, (long double)__y, __r, (expected));	\
} while (0)

/* Test x and y in both orders, in each of the three types. */
#define	TEST_ALL(x, y, expected, excepts) do {				\
	TEST(float, x, float, y, expected, excepts);			\
	TEST(float, y, float, x, expected, excepts);			\
	TEST(double, x, double, y, expected, excepts);			\
	TEST(double, y, double, x, expected, excepts);			\
	TEST(long double, x, long double, y, expected, excepts);	\
	TEST(long double, y, long double, x, expected, excepts);	\
} while (0)

/* Edge cases that depend on the format, in both orders. */
#define	TEST_FORMAT(t, max, true_min, next1) do {			\
	TEST(t, max, t, max, 1, 0);					\
	TEST(t, -max, t, -max, 1, 0);					\
	TEST(t, max, t, INFINITY, 0, 0);				\
	TEST(t, INFINITY, t, max, 0, 0);				\
	TEST(t, true_min, t, true_min, 1, 0);				\
	TEST(t, true_min, t, 0.0, 0, 0);				\
	TEST(t, 0.0, t, true_min, 0, 0);				\
	TEST(t, -true_min, t, -0.0, 0, 0);				\
	TEST(t, true_min, t, -true_min, 0, 0);				\
	TEST(t, 1.0, t, next1, 0, 0);					\
	TEST(t, next1, t, 1.0, 0, 0);					\
	TEST(t, next1, t, next1, 1, 0);					\
} while (0)

ATF_TC_WITHOUT_HEAD(equal);
ATF_TC_BODY(equal, tc)
{
	TEST_ALL(0.0, 0.0, 1, 0);
	TEST_ALL(0.0, -0.0, 1, 0);
	TEST_ALL(-0.0, -0.0, 1, 0);
	TEST_ALL(1.0, 1.0, 1, 0);
	TEST_ALL(-0.5, -0.5, 1, 0);
	TEST_ALL(0x1.fffffep+100, 0x1.fffffep+100, 1, 0);
	TEST_ALL(INFINITY, INFINITY, 1, 0);
	TEST_ALL(-INFINITY, -INFINITY, 1, 0);
}

ATF_TC_WITHOUT_HEAD(unequal);
ATF_TC_BODY(unequal, tc)
{
	TEST_ALL(0.0, 1.0, 0, 0);
	TEST_ALL(1.0, -1.0, 0, 0);
	TEST_ALL(1.0, 2.0, 0, 0);
	TEST_ALL(0.0, INFINITY, 0, 0);
	TEST_ALL(-0.0, -INFINITY, 0, 0);
	TEST_ALL(INFINITY, -INFINITY, 0, 0);
}

ATF_TC_WITHOUT_HEAD(format_edges);
ATF_TC_BODY(format_edges, tc)
{
	TEST_FORMAT(float, FLT_MAX, FLT_TRUE_MIN, nextafterf(1, 2));
	TEST_FORMAT(double, DBL_MAX, DBL_TRUE_MIN, nextafter(1, 2));
	TEST_FORMAT(long double, LDBL_MAX, LDBL_TRUE_MIN, nextafterl(1, 2));
}

/*
 * A quiet NaN in either position, or both, is a domain error: the result
 * is 0 and "invalid" is raised, and nothing else is.
 */
ATF_TC_WITHOUT_HEAD(quiet_nan);
ATF_TC_BODY(quiet_nan, tc)
{
	TEST_ALL(NAN, 0.0, 0, FE_INVALID);
	TEST_ALL(NAN, -0.0, 0, FE_INVALID);
	TEST_ALL(NAN, 1.0, 0, FE_INVALID);
	TEST_ALL(NAN, INFINITY, 0, FE_INVALID);
	TEST_ALL(NAN, -INFINITY, 0, FE_INVALID);
	TEST_ALL(NAN, NAN, 0, FE_INVALID);
	TEST_ALL(-NAN, 1.0, 0, FE_INVALID);
	TEST_ALL(-NAN, NAN, 0, FE_INVALID);
}

ATF_TC_WITHOUT_HEAD(signaling_nan);
ATF_TC_BODY(signaling_nan, tc)
{
	TEST(float, __builtin_nansf(""), float, 1.0f, 0, FE_INVALID);
	TEST(float, 1.0f, float, __builtin_nansf(""), 0, FE_INVALID);
	TEST(float, __builtin_nansf(""), float, __builtin_nansf(""), 0,
	    FE_INVALID);
	TEST(double, __builtin_nans(""), double, 1.0, 0, FE_INVALID);
	TEST(double, 1.0, double, __builtin_nans(""), 0, FE_INVALID);
	TEST(double, __builtin_nans(""), double, __builtin_nans(""), 0,
	    FE_INVALID);
	TEST(long double, __builtin_nansl(""), long double, 1.0L, 0,
	    FE_INVALID);
	TEST(long double, 1.0L, long double, __builtin_nansl(""), 0,
	    FE_INVALID);
	TEST(long double, __builtin_nansl(""), long double,
	    __builtin_nansl(""), 0, FE_INVALID);
}

/*
 * Arguments of different types are compared as == would compare them,
 * after the usual arithmetic conversions.
 */
ATF_TC_WITHOUT_HEAD(mixed_types);
ATF_TC_BODY(mixed_types, tc)
{
	TEST(float, 0.5f, double, 0.5, 1, 0);
	TEST(double, 0.5, float, 0.5f, 1, 0);
	TEST(float, 0.5f, long double, 0.5L, 1, 0);
	TEST(double, 0.5, long double, 0.5L, 1, 0);
	TEST(long double, 0.5L, double, 0.5, 1, 0);

	/* 0.1 is not representable; its roundings differ per format. */
	TEST(float, 0.1f, double, 0.1, 0, 0);
	TEST(double, 0.1, float, 0.1f, 0, 0);
	TEST(float, 0.1f, long double, 0.1L, 0, 0);
	TEST(double, 0.1, long double, 0.1L,
	    LDBL_MANT_DIG == DBL_MANT_DIG, 0);
	TEST(long double, 0.1L, double, 0.1,
	    LDBL_MANT_DIG == DBL_MANT_DIG, 0);

	/* The wider argument must not be narrowed to the narrower type. */
	TEST(float, 1.0f, double, nextafter(1, 2), 0, 0);
	TEST(double, 1.0, long double, nextafterl(1, 2), 0, 0);

	TEST(float, NAN, double, 1.0, 0, FE_INVALID);
	TEST(double, 1.0, long double, NAN, 0, FE_INVALID);
	TEST(float, 1.0f, long double, NAN, 0, FE_INVALID);
}

/* iseqsig() is a macro; each argument must be evaluated exactly once. */
ATF_TC_WITHOUT_HEAD(single_evaluation);
ATF_TC_BODY(single_evaluation, tc)
{
	double a[] = { 1.0, 1.0, 2.0 };
	float b[] = { 1.0f, 2.0f };
	int i, j, r;

	i = j = 0;
	r = iseqsig(a[i++], a[++j]);
	ATF_CHECK_EQ(1, r);
	ATF_CHECK_EQ(1, i);
	ATF_CHECK_EQ(1, j);

	i = j = 0;
	r = iseqsig(b[i++], a[j += 2]);
	ATF_CHECK_EQ(0, r);
	ATF_CHECK_EQ(1, i);
	ATF_CHECK_EQ(2, j);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, equal);
	ATF_TP_ADD_TC(tp, unequal);
	ATF_TP_ADD_TC(tp, format_edges);
	ATF_TP_ADD_TC(tp, quiet_nan);
	ATF_TP_ADD_TC(tp, signaling_nan);
	ATF_TP_ADD_TC(tp, mixed_types);
	ATF_TP_ADD_TC(tp, single_evaluation);

	return (atf_no_error());
}
