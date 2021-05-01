/* $NetBSD: t_sin.c,v 1.7 2019/05/27 00:24:37 maya Exp $ */

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
	{ -360, -6.283185307179586,  2.4492935982947064e-16, -1.7484555e-07 },
	{ -180, -3.141592653589793, -1.2246467991473532e-16, 8.7422777e-08 },
	{ -135, -2.356194490192345, -0.7071067811865476, 999 },
	{  -90, -1.570796326794897, -1.0000000000000000, 999 },
	{  -45, -0.785398163397448, -0.7071067811865472, 999 },
	{    0,  0.000000000000000,  0.0000000000000000, 999 },
	{   30,  0.5235987755982989, 0.5000000000000000, 999 },
	{   45,  0.785398163397448,  0.7071067811865472, 999 },
	{   60,  1.047197551196598,  0.8660254037844388, 999 },
	{   90,  1.570796326794897,  1.0000000000000000, 999 },
	{  120,  2.094395102393195,  0.8660254037844389, 999 },
	{  135,  2.356194490192345,  0.7071067811865476, 999 },
	{  150,  2.617993877991494,  0.5000000000000003, 999 },
	{  180,  3.141592653589793,  1.2246467991473532e-16, -8.7422777e-08 },
	{  270,  4.712388980384690, -1.0000000000000000, 999 },
	{  360,  6.283185307179586, -2.4492935982947064e-16, 1.7484555e-07 },
};

/*
 * sin(3)
 */
ATF_TC(sin_angles);
ATF_TC_HEAD(sin_angles, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test some selected angles");
}

ATF_TC_BODY(sin_angles, tc)
{
	const double eps = DBL_EPSILON;
	size_t i;

	for (i = 0; i < __arraycount(angles); i++) {
		int deg = angles[i].angle;
		double theta = angles[i].x;
		double sin_theta = angles[i].y;
		bool ok;

		if (sin_theta == 0) {
			/* Should be computed exactly.  */
			assert(sin_theta == 0);
			ok = (sin(theta) == 0);
		} else {
			assert(sin_theta != 0);
			ok = (fabs((sin(theta) - sin_theta)/sin_theta) <= eps);
		}

		if (!ok) {
			atf_tc_fail_nonfatal("sin(%d deg = %.17g) = %.17g"
			    " != %.17g",
			    deg, theta, sin(theta), sin_theta);
		}
	}
}

ATF_TC(sin_nan);
ATF_TC_HEAD(sin_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test sin(NaN) == NaN");
}

ATF_TC_BODY(sin_nan, tc)
{
	const double x = 0.0L / 0.0L;

	ATF_CHECK(isnan(x) != 0);
	ATF_CHECK(isnan(sin(x)) != 0);
}

ATF_TC(sin_inf_neg);
ATF_TC_HEAD(sin_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test sin(-Inf) == NaN");
}

ATF_TC_BODY(sin_inf_neg, tc)
{
	const double x = -1.0L / 0.0L;

	ATF_CHECK(isnan(sin(x)) != 0);
}

ATF_TC(sin_inf_pos);
ATF_TC_HEAD(sin_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test sin(+Inf) == NaN");
}

ATF_TC_BODY(sin_inf_pos, tc)
{
	const double x = 1.0L / 0.0L;

	ATF_CHECK(isnan(sin(x)) != 0);
}


ATF_TC(sin_zero_neg);
ATF_TC_HEAD(sin_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test sin(-0.0) == -0.0");
}

ATF_TC_BODY(sin_zero_neg, tc)
{
	const double x = -0.0L;

	ATF_CHECK(sin(x) == x);
}

ATF_TC(sin_zero_pos);
ATF_TC_HEAD(sin_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test sin(+0.0) == +0.0");
}

ATF_TC_BODY(sin_zero_pos, tc)
{
	const double x = 0.0L;

	ATF_CHECK(sin(x) == x);
}

/*
 * sinf(3)
 */
ATF_TC(sinf_angles);
ATF_TC_HEAD(sinf_angles, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test some selected angles");
}

ATF_TC_BODY(sinf_angles, tc)
{
	const float eps = FLT_EPSILON;
	size_t i;

	for (i = 0; i < __arraycount(angles); i++) {
		int deg = angles[i].angle;
		float theta = angles[i].x;
		float sin_theta = angles[i].fy;
		bool ok;

		if (sin_theta == 999)
			sin_theta = angles[i].y;

		if (sin_theta == 0) {
			/* Should be computed exactly.  */
			ok = (sinf(theta) == 0);
		} else {
			ok = (fabsf((sinf(theta) - sin_theta)/sin_theta)
			    <= eps);
		}

		if (!ok) {
			atf_tc_fail_nonfatal("sinf(%d deg) = %.8g != %.8g",
			    deg, sinf(theta), sin_theta);
		}
	}
}

ATF_TC(sinf_nan);
ATF_TC_HEAD(sinf_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test sinf(NaN) == NaN");
}

ATF_TC_BODY(sinf_nan, tc)
{
	const float x = 0.0L / 0.0L;

	ATF_CHECK(isnan(x) != 0);
	ATF_CHECK(isnan(sinf(x)) != 0);
}

ATF_TC(sinf_inf_neg);
ATF_TC_HEAD(sinf_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test sinf(-Inf) == NaN");
}

ATF_TC_BODY(sinf_inf_neg, tc)
{
	const float x = -1.0L / 0.0L;

	if (isnan(sinf(x)) == 0) {
		atf_tc_expect_fail("PR lib/45362");
		atf_tc_fail("sinf(-Inf) != NaN");
	}
}

ATF_TC(sinf_inf_pos);
ATF_TC_HEAD(sinf_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test sinf(+Inf) == NaN");
}

ATF_TC_BODY(sinf_inf_pos, tc)
{
	const float x = 1.0L / 0.0L;

	if (isnan(sinf(x)) == 0) {
		atf_tc_expect_fail("PR lib/45362");
		atf_tc_fail("sinf(+Inf) != NaN");
	}
}


ATF_TC(sinf_zero_neg);
ATF_TC_HEAD(sinf_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test sinf(-0.0) == -0.0");
}

ATF_TC_BODY(sinf_zero_neg, tc)
{
	const float x = -0.0L;

	ATF_CHECK(sinf(x) == x);
}

ATF_TC(sinf_zero_pos);
ATF_TC_HEAD(sinf_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test sinf(+0.0) == +0.0");
}

ATF_TC_BODY(sinf_zero_pos, tc)
{
	const float x = 0.0L;

	ATF_CHECK(sinf(x) == x);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, sin_angles);
	ATF_TP_ADD_TC(tp, sin_nan);
	ATF_TP_ADD_TC(tp, sin_inf_neg);
	ATF_TP_ADD_TC(tp, sin_inf_pos);
	ATF_TP_ADD_TC(tp, sin_zero_neg);
	ATF_TP_ADD_TC(tp, sin_zero_pos);

	ATF_TP_ADD_TC(tp, sinf_angles);
	ATF_TP_ADD_TC(tp, sinf_nan);
	ATF_TP_ADD_TC(tp, sinf_inf_neg);
	ATF_TP_ADD_TC(tp, sinf_inf_pos);
	ATF_TP_ADD_TC(tp, sinf_zero_neg);
	ATF_TP_ADD_TC(tp, sinf_zero_pos);

	return atf_no_error();
}
