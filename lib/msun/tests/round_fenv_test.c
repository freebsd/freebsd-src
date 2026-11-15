/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Sriram Shastry
 * All rights reserved.
 */

#include <sys/cdefs.h>
#include <fenv.h>
#include <float.h>
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
static double (*libroundeven)(double) = roundeven;
static float (*libroundevenf)(float) = roundevenf;
static long double (*libroundevenl)(long double) = roundevenl;

struct round_vector {
	const char *name;
	long double input;
	long double expected;
};

static const struct round_vector roundevenf_vectors[] = {
	{ "positive zero", 0.0L, 0.0L },
	{ "negative zero", -0.0L, -0.0L },
	{ "smallest positive subnormal", 0x1p-149L, 0.0L },
	{ "smallest negative subnormal", -0x1p-149L, -0.0L },
	{ "immediately below 0.5", 0x1.fffffep-2L, 0.0L },
	{ "positive 0.5 tie", 0.5L, 0.0L },
	{ "immediately above 0.5", 0x1.000002p-1L, 1.0L },
	{ "immediately above -0.5", -0x1.fffffep-2L, -0.0L },
	{ "negative 0.5 tie", -0.5L, -0.0L },
	{ "immediately below -0.5", -0x1.000002p-1L, -1.0L },
	{ "below an integer", 1.25L, 1.0L },
	{ "above an integer", 1.75L, 2.0L },
	{ "odd tie rounds up", 1.5L, 2.0L },
	{ "even tie rounds down", 2.5L, 2.0L },
	{ "next odd tie rounds up", 3.5L, 4.0L },
	{ "negative odd tie", -1.5L, -2.0L },
	{ "negative even tie", -2.5L, -2.0L },
	{ "largest even tie", 8388606.5L, 8388606.0L },
	{ "largest odd tie", 8388607.5L, 8388608.0L },
	{ "first always-integral value", 0x1p23L, 0x1p23L },
	{ "positive infinity", INFINITY, INFINITY },
	{ "negative infinity", -INFINITY, -INFINITY },
	{ "quiet NaN", NAN, NAN },
};

static const struct round_vector roundeven_vectors[] = {
	{ "positive zero", 0.0L, 0.0L },
	{ "negative zero", -0.0L, -0.0L },
	{ "smallest positive subnormal", 0x1p-1074L, 0.0L },
	{ "smallest negative subnormal", -0x1p-1074L, -0.0L },
	{ "immediately below 0.5", 0x1.fffffffffffffp-2L, 0.0L },
	{ "positive 0.5 tie", 0.5L, 0.0L },
	{ "immediately above 0.5", 0x1.0000000000001p-1L, 1.0L },
	{ "immediately above -0.5", -0x1.fffffffffffffp-2L, -0.0L },
	{ "negative 0.5 tie", -0.5L, -0.0L },
	{ "immediately below -0.5", -0x1.0000000000001p-1L, -1.0L },
	{ "below an integer", 1.25L, 1.0L },
	{ "above an integer", 1.75L, 2.0L },
	{ "odd tie rounds up", 1.5L, 2.0L },
	{ "even tie rounds down", 2.5L, 2.0L },
	{ "next odd tie rounds up", 3.5L, 4.0L },
	{ "negative odd tie", -1.5L, -2.0L },
	{ "negative even tie", -2.5L, -2.0L },
	{ "largest even tie", 4503599627370494.5L, 4503599627370494.0L },
	{ "largest odd tie", 4503599627370495.5L, 4503599627370496.0L },
	{ "first always-integral value", 0x1p52L, 0x1p52L },
	{ "positive infinity", INFINITY, INFINITY },
	{ "negative infinity", -INFINITY, -INFINITY },
	{ "quiet NaN", NAN, NAN },
};

static const struct round_vector roundevenl_vectors[] = {
	{ "positive zero", 0.0L, 0.0L },
	{ "negative zero", -0.0L, -0.0L },
	{ "smallest positive subnormal", LDBL_TRUE_MIN, 0.0L },
	{ "smallest negative subnormal", -LDBL_TRUE_MIN, -0.0L },
	{ "immediately below 0.5", 0.5L - LDBL_EPSILON / 4, 0.0L },
	{ "positive 0.5 tie", 0.5L, 0.0L },
	{ "immediately above 0.5", 0.5L + LDBL_EPSILON / 2, 1.0L },
	{ "immediately above -0.5", -0.5L + LDBL_EPSILON / 4, -0.0L },
	{ "negative 0.5 tie", -0.5L, -0.0L },
	{ "immediately below -0.5", -0.5L - LDBL_EPSILON / 2, -1.0L },
	{ "below an integer", 1.25L, 1.0L },
	{ "above an integer", 1.75L, 2.0L },
	{ "odd tie rounds up", 1.5L, 2.0L },
	{ "even tie rounds down", 2.5L, 2.0L },
	{ "next odd tie rounds up", 3.5L, 4.0L },
	{ "negative odd tie", -1.5L, -2.0L },
	{ "negative even tie", -2.5L, -2.0L },
#if LDBL_MANT_DIG == 64
	{ "high-low word even tie", 0x1p31L + 0.5L, 0x1p31L },
	{ "high-low word odd tie", 0x1p31L + 1.5L, 0x1p31L + 2.0L },
#elif LDBL_MANT_DIG == 113
	{ "high-low word even tie", 0x1p48L + 0.5L, 0x1p48L },
	{ "high-low word odd tie", 0x1p48L + 1.5L, 0x1p48L + 2.0L },
#endif
	{ "positive infinity", INFINITY, INFINITY },
	{ "negative infinity", -INFINITY, -INFINITY },
	{ "quiet NaN", NAN, NAN },
};

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

#define	CHECK_ROUND_VECTOR(fn, type, vector) do { \
	type value = (type)(vector).input; \
	type expected = (type)(vector).expected; \
	type result; \
	\
	ATF_REQUIRE_EQ(0, feclearexcept(ALL_STD_EXCEPT)); \
	result = (fn)(value); \
	ATF_CHECK_MSG(fpequal_cs(expected, result, true), \
	    "%s: %s(%La) returned %La instead of %La", (vector).name, \
	    #fn, (long double)value, (long double)result, \
	    (long double)expected); \
	CHECK_FP_EXCEPTIONS_MSG(0, ALL_STD_EXCEPT, \
	    "for vector '%s', %s(%La)", (vector).name, #fn, \
	    (long double)value); \
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

static void
test_roundeven(void)
{
	int original_rounding_mode;
	size_t mode_index, vector_index;

	original_rounding_mode = fegetround();
	for (mode_index = 0;
	    mode_index < sizeof(rounding_modes) / sizeof(rounding_modes[0]);
	    mode_index++) {
		ATF_REQUIRE_EQ(0, fesetround(rounding_modes[mode_index]));
		for (vector_index = 0;
		    vector_index < sizeof(roundeven_vectors) /
		    sizeof(roundeven_vectors[0]); vector_index++)
			CHECK_ROUND_VECTOR(libroundeven, double,
			    roundeven_vectors[vector_index]);
	}
	ATF_REQUIRE_EQ(0, fesetround(original_rounding_mode));
}

static void
test_roundevenf(void)
{
	int original_rounding_mode;
	size_t mode_index, vector_index;

	original_rounding_mode = fegetround();
	for (mode_index = 0;
	    mode_index < sizeof(rounding_modes) / sizeof(rounding_modes[0]);
	    mode_index++) {
		ATF_REQUIRE_EQ(0, fesetround(rounding_modes[mode_index]));
		for (vector_index = 0;
		    vector_index < sizeof(roundevenf_vectors) /
		    sizeof(roundevenf_vectors[0]); vector_index++)
			CHECK_ROUND_VECTOR(libroundevenf, float,
			    roundevenf_vectors[vector_index]);
	}
	ATF_REQUIRE_EQ(0, fesetround(original_rounding_mode));
}

static void
test_roundevenl(void)
{
	struct round_vector limit_vectors[3];
	long double limit, previous;
	int original_rounding_mode;
	size_t mode_index, vector_index;

	original_rounding_mode = fegetround();
	ATF_REQUIRE_EQ(0, fesetround(FE_TONEAREST));
	limit = ldexpl(1.0L, LDBL_MANT_DIG - 1);
	previous = nextafterl(limit, 0.0L);
	limit_vectors[0] = (struct round_vector){
	    "largest odd tie", previous, limit };
	previous = nextafterl(nextafterl(previous, 0.0L), 0.0L);
	limit_vectors[1] = (struct round_vector){
	    "largest even tie", previous, limit - 2.0L };
	limit_vectors[2] = (struct round_vector){
	    "first always-integral value", limit, limit };

	for (mode_index = 0;
	    mode_index < sizeof(rounding_modes) / sizeof(rounding_modes[0]);
	    mode_index++) {
		ATF_REQUIRE_EQ(0, fesetround(rounding_modes[mode_index]));
		for (vector_index = 0;
		    vector_index < sizeof(roundevenl_vectors) /
		    sizeof(roundevenl_vectors[0]); vector_index++)
			CHECK_ROUND_VECTOR(libroundevenl, long double,
			    roundevenl_vectors[vector_index]);
		for (vector_index = 0;
		    vector_index < sizeof(limit_vectors) /
		    sizeof(limit_vectors[0]); vector_index++)
			CHECK_ROUND_VECTOR(libroundevenl, long double,
			    limit_vectors[vector_index]);
	}
	ATF_REQUIRE_EQ(0, fesetround(original_rounding_mode));
}

ATF_TC_WITHOUT_HEAD(main);
ATF_TC_BODY(main, tc)
{
	test_round();
	test_roundf();
	test_roundl();
	test_roundeven();
	test_roundevenf();
	test_roundevenl();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, main);

	return (atf_no_error());
}