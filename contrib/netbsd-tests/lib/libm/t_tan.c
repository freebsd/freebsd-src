/* $NetBSD: t_tan.c,v 1.7 2018/11/07 04:00:13 riastradh Exp $ */

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
	{ -180, -3.141592653589793,  1.2246467991473532e-16, -8.7422777e-08 },
	{ -135, -2.356194490192345,  1.0000000000000002, 999 },
	{  -45, -0.785398163397448, -0.9999999999999992, 999 },
	{    0,  0.000000000000000,  0.0000000000000000, 999 },
	{   30,  0.5235987755982988, 0.57735026918962573, 999 },
	{   45,  0.785398163397448,  0.9999999999999992, 999 },
	{   60,  1.047197551196598,  1.7320508075688785,  1.7320509 },
	{  120,  2.094395102393195, -1.7320508075688801, -1.7320505 },
	{  135,  2.356194490192345, -1.0000000000000002, 999 },
	{  150,  2.617993877991494, -0.57735026918962629, -0.57735032 },
	{  180,  3.141592653589793, -1.2246467991473532e-16, 8.7422777e-08 },
	{  360,  6.283185307179586, -2.4492935982947064e-16, 1.7484555e-07 },
};

/*
 * tan(3)
 */
ATF_TC(tan_angles);
ATF_TC_HEAD(tan_angles, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test some selected angles");
}

ATF_TC_BODY(tan_angles, tc)
{
	const double eps = DBL_EPSILON;
	size_t i;

	for (i = 0; i < __arraycount(angles); i++) {
		int deg = angles[i].angle;
		double theta = angles[i].x;
		double tan_theta = angles[i].y;
		bool ok;

		if (theta == 0) {
			/* Should be computed exactly.  */
			assert(tan_theta == 0);
			ok = (tan(theta) == 0);
		} else {
			assert(tan_theta != 0);
			ok = (fabs((tan(theta) - tan_theta)/tan_theta) <= eps);
		}

		if (!ok) {
			atf_tc_fail_nonfatal("tan(%d deg = %.17g) = %.17g"
			    " != %.17g",
			    deg, theta, tan(theta), tan_theta);
		}
	}
}

ATF_TC(tan_nan);
ATF_TC_HEAD(tan_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test tan(NaN) == NaN");
}

ATF_TC_BODY(tan_nan, tc)
{
	const double x = 0.0L / 0.0L;

	ATF_CHECK(isnan(x) != 0);
	ATF_CHECK(isnan(tan(x)) != 0);
}

ATF_TC(tan_inf_neg);
ATF_TC_HEAD(tan_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test tan(-Inf) == NaN");
}

ATF_TC_BODY(tan_inf_neg, tc)
{
	const double x = -1.0L / 0.0L;

	ATF_CHECK(isnan(tan(x)) != 0);
}

ATF_TC(tan_inf_pos);
ATF_TC_HEAD(tan_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test tan(+Inf) == NaN");
}

ATF_TC_BODY(tan_inf_pos, tc)
{
	const double x = 1.0L / 0.0L;

	ATF_CHECK(isnan(tan(x)) != 0);
}


ATF_TC(tan_zero_neg);
ATF_TC_HEAD(tan_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test tan(-0.0) == -0.0");
}

ATF_TC_BODY(tan_zero_neg, tc)
{
	const double x = -0.0L;

	ATF_CHECK(tan(x) == x);
}

ATF_TC(tan_zero_pos);
ATF_TC_HEAD(tan_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test tan(+0.0) == +0.0");
}

ATF_TC_BODY(tan_zero_pos, tc)
{
	const double x = 0.0L;

	ATF_CHECK(tan(x) == x);
}

/*
 * tanf(3)
 */
ATF_TC(tanf_angles);
ATF_TC_HEAD(tanf_angles, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test some selected angles");
}

ATF_TC_BODY(tanf_angles, tc)
{
	const float eps = FLT_EPSILON;
	size_t i;

	for (i = 0; i < __arraycount(angles); i++) {
		int deg = angles[i].angle;
		float theta = angles[i].x;
		float tan_theta = angles[i].fy;
		bool ok;

		if (tan_theta == 999)
			tan_theta = angles[i].y;

		if (theta == 0) {
			/* Should be computed exactly.  */
			assert(tan_theta == 0);
			ok = (tan(theta) == 0);
		} else {
			assert(tan_theta != 0);
			ok = (fabsf((tanf(theta) - tan_theta)/tan_theta)
			    <= eps);
		}

		if (!ok) {
			atf_tc_fail_nonfatal("tanf(%d deg) = %.8g != %.8g",
			    deg, tanf(theta), tan_theta);
		}
	}
}

ATF_TC(tanf_nan);
ATF_TC_HEAD(tanf_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test tanf(NaN) == NaN");
}

ATF_TC_BODY(tanf_nan, tc)
{
	const float x = 0.0L / 0.0L;

	ATF_CHECK(isnan(x) != 0);
	ATF_CHECK(isnan(tanf(x)) != 0);
}

ATF_TC(tanf_inf_neg);
ATF_TC_HEAD(tanf_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test tanf(-Inf) == NaN");
}

ATF_TC_BODY(tanf_inf_neg, tc)
{
	const float x = -1.0L / 0.0L;

	if (isnan(tanf(x)) == 0) {
		atf_tc_expect_fail("PR lib/45362");
		atf_tc_fail("tanf(-Inf) != NaN");
	}
}

ATF_TC(tanf_inf_pos);
ATF_TC_HEAD(tanf_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test tanf(+Inf) == NaN");
}

ATF_TC_BODY(tanf_inf_pos, tc)
{
	const float x = 1.0L / 0.0L;

	if (isnan(tanf(x)) == 0) {
		atf_tc_expect_fail("PR lib/45362");
		atf_tc_fail("tanf(+Inf) != NaN");
	}
}


ATF_TC(tanf_zero_neg);
ATF_TC_HEAD(tanf_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test tanf(-0.0) == -0.0");
}

ATF_TC_BODY(tanf_zero_neg, tc)
{
	const float x = -0.0L;

	ATF_CHECK(tanf(x) == x);
}

ATF_TC(tanf_zero_pos);
ATF_TC_HEAD(tanf_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test tanf(+0.0) == +0.0");
}

ATF_TC_BODY(tanf_zero_pos, tc)
{
	const float x = 0.0L;

	ATF_CHECK(tanf(x) == x);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, tan_angles);
	ATF_TP_ADD_TC(tp, tan_nan);
	ATF_TP_ADD_TC(tp, tan_inf_neg);
	ATF_TP_ADD_TC(tp, tan_inf_pos);
	ATF_TP_ADD_TC(tp, tan_zero_neg);
	ATF_TP_ADD_TC(tp, tan_zero_pos);

	ATF_TP_ADD_TC(tp, tanf_angles);
	ATF_TP_ADD_TC(tp, tanf_nan);
	ATF_TP_ADD_TC(tp, tanf_inf_neg);
	ATF_TP_ADD_TC(tp, tanf_inf_pos);
	ATF_TP_ADD_TC(tp, tanf_zero_neg);
	ATF_TP_ADD_TC(tp, tanf_zero_pos);

	return atf_no_error();
}
