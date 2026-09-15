/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Sriram Shastry
 * All rights reserved.
 */

#include <sys/cdefs.h>
#include <fenv.h>
#include <math.h>
#include <stddef.h>

#include "test-utils.h"

static const int rounding_modes[] = {
	FE_TONEAREST,
	FE_DOWNWARD,
	FE_UPWARD,
	FE_TOWARDZERO,
};

static double (*libround)(double) = round;
static float (*libroundf)(float) = roundf;
static long double (*libroundl)(long double) = roundl;

#pragma STDC FENV_ACCESS ON

#define	CHECK_ROUND(fn, type, input, expected, excepts) do { \
	type value = (input); \
	type result; \
	\
	ATF_REQUIRE_EQ(0, feclearexcept(ALL_STD_EXCEPT)); \
	result = (fn)(value); \
	CHECK_FPEQUAL((expected), result); \
	CHECK_FP_EXCEPTIONS_MSG((excepts), ALL_STD_EXCEPT, \
	    "for %s(%La)", #fn, (long double)value); \
} while (0)

static void
test_round(void)
{
	int original_rounding_mode;
	size_t index;

	original_rounding_mode = fegetround();
	for (index = 0; index < sizeof(rounding_modes) / sizeof(rounding_modes[0]);
	     index++) {
		ATF_REQUIRE_EQ(0, fesetround(rounding_modes[index]));
		CHECK_ROUND(libround, double, 0.0, 0.0, 0);
		CHECK_ROUND(libround, double, -0.0, -0.0, 0);
		CHECK_ROUND(libround, double, 0.25, 0.0, FE_INEXACT);
		CHECK_ROUND(libround, double, -0.25, -0.0, FE_INEXACT);
		CHECK_ROUND(libround, double, 0.5, 1.0, FE_INEXACT);
		CHECK_ROUND(libround, double, -0.5, -1.0, FE_INEXACT);
		CHECK_ROUND(libround, double, 1.0, 1.0, 0);
		CHECK_ROUND(libround, double, 1.25, 1.0, FE_INEXACT);
		CHECK_ROUND(libround, double, INFINITY, INFINITY, 0);
		CHECK_ROUND(libround, double, -INFINITY, -INFINITY, 0);
		CHECK_ROUND(libround, double, NAN, NAN, 0);
	}
	ATF_REQUIRE_EQ(0, fesetround(original_rounding_mode));
}

static void
test_roundf(void)
{
	int original_rounding_mode;
	size_t index;

	original_rounding_mode = fegetround();
	for (index = 0; index < sizeof(rounding_modes) / sizeof(rounding_modes[0]);
	     index++) {
		ATF_REQUIRE_EQ(0, fesetround(rounding_modes[index]));
		CHECK_ROUND(libroundf, float, 0.0F, 0.0F, 0);
		CHECK_ROUND(libroundf, float, -0.0F, -0.0F, 0);
		CHECK_ROUND(libroundf, float, 0.25F, 0.0F, FE_INEXACT);
		CHECK_ROUND(libroundf, float, -0.25F, -0.0F, FE_INEXACT);
		CHECK_ROUND(libroundf, float, 0.5F, 1.0F, FE_INEXACT);
		CHECK_ROUND(libroundf, float, -0.5F, -1.0F, FE_INEXACT);
		CHECK_ROUND(libroundf, float, 1.0F, 1.0F, 0);
		CHECK_ROUND(libroundf, float, 1.25F, 1.0F, FE_INEXACT);
		CHECK_ROUND(libroundf, float, INFINITY, INFINITY, 0);
		CHECK_ROUND(libroundf, float, -INFINITY, -INFINITY, 0);
		CHECK_ROUND(libroundf, float, NAN, NAN, 0);
	}
	ATF_REQUIRE_EQ(0, fesetround(original_rounding_mode));
}

static void
test_roundl(void)
{
	int original_rounding_mode;
	size_t index;

	original_rounding_mode = fegetround();
	for (index = 0; index < sizeof(rounding_modes) / sizeof(rounding_modes[0]);
	     index++) {
		ATF_REQUIRE_EQ(0, fesetround(rounding_modes[index]));
		CHECK_ROUND(libroundl, long double, 0.0L, 0.0L, 0);
		CHECK_ROUND(libroundl, long double, -0.0L, -0.0L, 0);
		CHECK_ROUND(libroundl, long double, 0.25L, 0.0L, FE_INEXACT);
		CHECK_ROUND(libroundl, long double, -0.25L, -0.0L, FE_INEXACT);
		CHECK_ROUND(libroundl, long double, 0.5L, 1.0L, FE_INEXACT);
		CHECK_ROUND(libroundl, long double, -0.5L, -1.0L, FE_INEXACT);
		CHECK_ROUND(libroundl, long double, 1.0L, 1.0L, 0);
		CHECK_ROUND(libroundl, long double, 1.25L, 1.0L, FE_INEXACT);
		CHECK_ROUND(libroundl, long double, INFINITY, INFINITY, 0);
		CHECK_ROUND(libroundl, long double, -INFINITY, -INFINITY, 0);
		CHECK_ROUND(libroundl, long double, NAN, NAN, 0);
	}
	ATF_REQUIRE_EQ(0, fesetround(original_rounding_mode));
}

ATF_TC_WITHOUT_HEAD(main);
ATF_TC_BODY(main, tc)
{
	test_round();
	test_roundf();
	test_roundl();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, main);

	return (atf_no_error());
}