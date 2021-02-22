/* $NetBSD: t_cos.c,v 1.9 2019/05/27 00:10:36 maya Exp $ */

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jukka Ruohonen.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <atf-c.h>
#include <float.h>
#include <math.h>

static const struct {
	int		angle;
	double		x;
	double		y;
	float		fy;
} angles[] = {
	{ -180, -3.141592653589793, -1.0000000000000000, 999 },
	{ -135, -2.356194490192345, -0.7071067811865476, 999 },
	{  -90, -1.5707963267948966, 6.123233995736766e-17, -4.3711388e-08 },
	{  -90, -1.5707963267948968, -1.6081226496766366e-16, -4.3711388e-08 },
	{  -45, -0.785398163397448,  0.7071067811865478, 999 },
	{    0,  0.000000000000000,  1.0000000000000000, 999 },
	{   30,  0.523598775598299,  0.8660254037844386, 999 },
	{   45,  0.785398163397448,  0.7071067811865478, 999 },
	{   60,  1.0471975511965976,  0.5000000000000001, 999 },
	{   60,  1.0471975511965979,  0.4999999999999999, 999 },
	{   90,  1.570796326794897, -3.8285686989269494e-16, -4.3711388e-08 },
	{  120,  2.0943951023931953, -0.4999999999999998, 999 },
	{  120,  2.0943951023931957, -0.5000000000000002, 999 },
	{  135,  2.356194490192345, -0.7071067811865476, 999 },
	{  150,  2.617993877991494, -0.8660254037844386, 999 },
	{  180,  3.141592653589793, -1.0000000000000000, 999 },
	{  270,  4.712388980384690, -1.8369701987210297e-16, 1.1924881e-08 },
	{  360,  6.283185307179586,  1.0000000000000000, 999 },
};

#ifdef __HAVE_LONG_DOUBLE
/*
 * cosl(3)
 */
ATF_TC(cosl_angles);
ATF_TC_HEAD(cosl_angles, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test some selected angles");
}

ATF_TC_BODY(cosl_angles, tc)
{
	/*
	 * XXX The given data is for double, so take that
	 * into account and expect less precise results..
	 */
	const long double eps = DBL_EPSILON;
	size_t i;

	for (i = 0; i < __arraycount(angles); i++) {
		int deg = angles[i].angle;
		long double theta = angles[i].x;
		long double cos_theta = angles[i].y;

		assert(cos_theta != 0);
		if (!(fabsl((cosl(theta) - cos_theta)/cos_theta) <= eps)) {
			atf_tc_fail_nonfatal("cos(%d deg = %.17Lg) = %.17Lg"
			    " != %.17Lg",
			    deg, theta, cosl(theta), cos_theta);
		}
	}
}

ATF_TC(cosl_nan);
ATF_TC_HEAD(cosl_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cosl(NaN) == NaN");
}

ATF_TC_BODY(cosl_nan, tc)
{
	const long double x = 0.0L / 0.0L;

	ATF_CHECK(isnan(x) != 0);
	ATF_CHECK(isnan(cosl(x)) != 0);
}

ATF_TC(cosl_inf_neg);
ATF_TC_HEAD(cosl_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cosl(-Inf) == NaN");
}

ATF_TC_BODY(cosl_inf_neg, tc)
{
	const long double x = -1.0L / 0.0L;

	ATF_CHECK(isnan(cosl(x)) != 0);
}

ATF_TC(cosl_inf_pos);
ATF_TC_HEAD(cosl_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cosl(+Inf) == NaN");
}

ATF_TC_BODY(cosl_inf_pos, tc)
{
	const long double x = 1.0L / 0.0L;

	ATF_CHECK(isnan(cosl(x)) != 0);
}


ATF_TC(cosl_zero_neg);
ATF_TC_HEAD(cosl_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cosl(-0.0) == 1.0");
}

ATF_TC_BODY(cosl_zero_neg, tc)
{
	const long double x = -0.0L;

	ATF_CHECK(cosl(x) == 1.0);
}

ATF_TC(cosl_zero_pos);
ATF_TC_HEAD(cosl_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cosl(+0.0) == 1.0");
}

ATF_TC_BODY(cosl_zero_pos, tc)
{
	const long double x = 0.0L;

	ATF_CHECK(cosl(x) == 1.0);
}
#endif

/*
 * cos(3)
 */
ATF_TC(cos_angles);
ATF_TC_HEAD(cos_angles, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test some selected angles");
}

ATF_TC_BODY(cos_angles, tc)
{
	const double eps = DBL_EPSILON;
	size_t i;

	for (i = 0; i < __arraycount(angles); i++) {
		int deg = angles[i].angle;
		double theta = angles[i].x;
		double cos_theta = angles[i].y;

		assert(cos_theta != 0);
		if (!(fabs((cos(theta) - cos_theta)/cos_theta) <= eps)) {
			atf_tc_fail_nonfatal("cos(%d deg = %.17g) = %.17g"
			    " != %.17g",
			    deg, theta, cos(theta), cos_theta);
		}
	}
}

ATF_TC(cos_nan);
ATF_TC_HEAD(cos_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cos(NaN) == NaN");
}

ATF_TC_BODY(cos_nan, tc)
{
	const double x = 0.0L / 0.0L;

	ATF_CHECK(isnan(x) != 0);
	ATF_CHECK(isnan(cos(x)) != 0);
}

ATF_TC(cos_inf_neg);
ATF_TC_HEAD(cos_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cos(-Inf) == NaN");
}

ATF_TC_BODY(cos_inf_neg, tc)
{
	const double x = -1.0L / 0.0L;

	ATF_CHECK(isnan(cos(x)) != 0);
}

ATF_TC(cos_inf_pos);
ATF_TC_HEAD(cos_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cos(+Inf) == NaN");
}

ATF_TC_BODY(cos_inf_pos, tc)
{
	const double x = 1.0L / 0.0L;

	ATF_CHECK(isnan(cos(x)) != 0);
}


ATF_TC(cos_zero_neg);
ATF_TC_HEAD(cos_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cos(-0.0) == 1.0");
}

ATF_TC_BODY(cos_zero_neg, tc)
{
	const double x = -0.0L;

	ATF_CHECK(cos(x) == 1.0);
}

ATF_TC(cos_zero_pos);
ATF_TC_HEAD(cos_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cos(+0.0) == 1.0");
}

ATF_TC_BODY(cos_zero_pos, tc)
{
	const double x = 0.0L;

	ATF_CHECK(cos(x) == 1.0);
}

/*
 * cosf(3)
 */
ATF_TC(cosf_angles);
ATF_TC_HEAD(cosf_angles, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test some selected angles");
}

ATF_TC_BODY(cosf_angles, tc)
{
	const float eps = FLT_EPSILON;
	size_t i;

	for (i = 0; i < __arraycount(angles); i++) {
		int deg = angles[i].angle;
		float theta = angles[i].x;
		float cos_theta = angles[i].fy;

		/*
		 * Force rounding to float even if FLT_EVAL_METHOD=2,
		 * as is the case on i386.
		 *
		 * The volatile should not be necessary, by C99 Sec.
		 * 5.2.4.2.2. para. 8 on p. 24 which specifies that
		 * assignment and cast remove all extra range and precision,
		 * but seems to be needed to work around a compiler bug.
		 */ 
		volatile float result = cosf(theta);

		if (cos_theta == 999)
			cos_theta = angles[i].y;

		assert(cos_theta != 0);
		if (!(fabsf((result - cos_theta)/cos_theta) <= eps)) {
			atf_tc_fail_nonfatal("cosf(%d deg = %.8g) = %.8g"
			    " != %.8g", deg, theta, result, cos_theta);
		}
	}
}

ATF_TC(cosf_nan);
ATF_TC_HEAD(cosf_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cosf(NaN) == NaN");
}

ATF_TC_BODY(cosf_nan, tc)
{
	const float x = 0.0L / 0.0L;

	ATF_CHECK(isnan(x) != 0);
	ATF_CHECK(isnan(cosf(x)) != 0);
}

ATF_TC(cosf_inf_neg);
ATF_TC_HEAD(cosf_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cosf(-Inf) == NaN");
}

ATF_TC_BODY(cosf_inf_neg, tc)
{
	const float x = -1.0L / 0.0L;

	if (isnan(cosf(x)) == 0) {
		atf_tc_expect_fail("PR lib/45362");
		atf_tc_fail("cosf(-Inf) != NaN");
	}
}

ATF_TC(cosf_inf_pos);
ATF_TC_HEAD(cosf_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cosf(+Inf) == NaN");
}

ATF_TC_BODY(cosf_inf_pos, tc)
{
	const float x = 1.0L / 0.0L;

	if (isnan(cosf(x)) == 0) {
		atf_tc_expect_fail("PR lib/45362");
		atf_tc_fail("cosf(+Inf) != NaN");
	}
}


ATF_TC(cosf_zero_neg);
ATF_TC_HEAD(cosf_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cosf(-0.0) == 1.0");
}

ATF_TC_BODY(cosf_zero_neg, tc)
{
	const float x = -0.0L;

	ATF_CHECK(cosf(x) == 1.0);
}

ATF_TC(cosf_zero_pos);
ATF_TC_HEAD(cosf_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cosf(+0.0) == 1.0");
}

ATF_TC_BODY(cosf_zero_pos, tc)
{
	const float x = 0.0L;

	ATF_CHECK(cosf(x) == 1.0);
}

ATF_TP_ADD_TCS(tp)
{
#ifdef __HAVE_LONG_DOUBLE
	ATF_TP_ADD_TC(tp, cosl_angles);
	ATF_TP_ADD_TC(tp, cosl_nan);
	ATF_TP_ADD_TC(tp, cosl_inf_neg);
	ATF_TP_ADD_TC(tp, cosl_inf_pos);
	ATF_TP_ADD_TC(tp, cosl_zero_neg);
	ATF_TP_ADD_TC(tp, cosl_zero_pos);
#endif

	ATF_TP_ADD_TC(tp, cos_angles);
	ATF_TP_ADD_TC(tp, cos_nan);
	ATF_TP_ADD_TC(tp, cos_inf_neg);
	ATF_TP_ADD_TC(tp, cos_inf_pos);
	ATF_TP_ADD_TC(tp, cos_zero_neg);
	ATF_TP_ADD_TC(tp, cos_zero_pos);

	ATF_TP_ADD_TC(tp, cosf_angles);
	ATF_TP_ADD_TC(tp, cosf_nan);
	ATF_TP_ADD_TC(tp, cosf_inf_neg);
	ATF_TP_ADD_TC(tp, cosf_inf_pos);
	ATF_TP_ADD_TC(tp, cosf_zero_neg);
	ATF_TP_ADD_TC(tp, cosf_zero_pos);

	return atf_no_error();
}
