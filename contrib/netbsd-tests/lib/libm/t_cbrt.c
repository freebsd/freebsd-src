/* $NetBSD: t_cbrt.c,v 1.5 2018/11/15 05:14:20 riastradh Exp $ */

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
#include <sys/cdefs.h>
__RCSID("$NetBSD: t_cbrt.c,v 1.5 2018/11/15 05:14:20 riastradh Exp $");

#include <atf-c.h>
#include <float.h>
#include <math.h>
#include <stdio.h>

/*
 * cbrt(3)
 */
ATF_TC(cbrt_nan);
ATF_TC_HEAD(cbrt_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cbrt(NaN) == NaN");
}

ATF_TC_BODY(cbrt_nan, tc)
{
	const double x = 0.0L / 0.0L;

	ATF_CHECK(isnan(x) != 0);
	ATF_CHECK(isnan(cbrt(x)) != 0);
}

ATF_TC(cbrt_pow);
ATF_TC_HEAD(cbrt_pow, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cbrt(3) vs. pow(3)");
}

ATF_TC_BODY(cbrt_pow, tc)
{
	const double x[] = { 0.0, 0.005, 1.0, 99.0, 123.123, 9999.0 };
	/* Neither cbrt nor pow is required to be correctly rounded.  */
	const double eps = 2*DBL_EPSILON;
	size_t i;

	for (i = 0; i < __arraycount(x); i++) {
		double x_cbrt = cbrt(x[i]);
		double x_pow13 = pow(x[i], 1.0 / 3.0);
		bool ok;

		if (x[i] == 0) {
			ok = (x_cbrt == x_pow13);
		} else {
			ok = (fabs((x_cbrt - x_pow13)/x_cbrt) <= eps);
		}

		if (!ok) {
			atf_tc_fail_nonfatal("cbrt(%.17g) = %.17g != "
			    "pow(%.17g, 1/3) = %.17g\n",
			    x[i], x_cbrt, x[i], x_pow13);
		}
	}
}

ATF_TC(cbrt_inf_neg);
ATF_TC_HEAD(cbrt_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cbrt(-Inf) == -Inf");
}

ATF_TC_BODY(cbrt_inf_neg, tc)
{
	const double x = -1.0L / 0.0L;
	double y = cbrt(x);

	ATF_CHECK(isinf(y) != 0);
	ATF_CHECK(signbit(y) != 0);
}

ATF_TC(cbrt_inf_pos);
ATF_TC_HEAD(cbrt_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cbrt(+Inf) == +Inf");
}

ATF_TC_BODY(cbrt_inf_pos, tc)
{
	const double x = 1.0L / 0.0L;
	double y = cbrt(x);

	ATF_CHECK(isinf(y) != 0);
	ATF_CHECK(signbit(y) == 0);
}

ATF_TC(cbrt_zero_neg);
ATF_TC_HEAD(cbrt_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cbrt(-0.0) == -0.0");
}

ATF_TC_BODY(cbrt_zero_neg, tc)
{
	const double x = -0.0L;
	double y = cbrt(x);

	if (fabs(y) > 0.0 || signbit(y) == 0)
		atf_tc_fail_nonfatal("cbrt(-0.0) != -0.0");
}

ATF_TC(cbrt_zero_pos);
ATF_TC_HEAD(cbrt_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cbrt(+0.0) == +0.0");
}

ATF_TC_BODY(cbrt_zero_pos, tc)
{
	const double x = 0.0L;
	double y = cbrt(x);

	if (fabs(y) > 0.0 || signbit(y) != 0)
		atf_tc_fail_nonfatal("cbrt(+0.0) != +0.0");
}

/*
 * cbrtf(3)
 */
ATF_TC(cbrtf_nan);
ATF_TC_HEAD(cbrtf_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cbrtf(NaN) == NaN");
}

ATF_TC_BODY(cbrtf_nan, tc)
{
	const float x = 0.0L / 0.0L;

	ATF_CHECK(isnan(x) != 0);
	ATF_CHECK(isnan(cbrtf(x)) != 0);
}

ATF_TC(cbrtf_powf);
ATF_TC_HEAD(cbrtf_powf, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cbrtf(3) vs. powf(3)");
}

ATF_TC_BODY(cbrtf_powf, tc)
{
	const float x[] = { 0.0, 0.005, 1.0, 99.0, 123.123, 9999.0 };
	/* Neither cbrt nor pow is required to be correctly rounded.  */
	const float eps = 2*FLT_EPSILON;
	size_t i;

	for (i = 0; i < __arraycount(x); i++) {
		float x_cbrt = cbrtf(x[i]);
		float x_pow13 = powf(x[i], 1.0 / 3.0);
		bool ok;

		if (x[i] == 0) {
			ok = (x_cbrt == x_pow13);
		} else {
			ok = (fabsf((x_cbrt - x_pow13)/x_cbrt) <= eps);
		}

		if (!ok) {
			atf_tc_fail_nonfatal("cbrtf(%.9g) = %.9g. != "
			    "powf(%.9g, 1/3) = %.9g\n",
			    (double)x[i], (double)x_cbrt,
			    (double)x[i], (double)x_pow13);
		}
	}
}

ATF_TC(cbrtf_inf_neg);
ATF_TC_HEAD(cbrtf_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cbrtf(-Inf) == -Inf");
}

ATF_TC_BODY(cbrtf_inf_neg, tc)
{
	const float x = -1.0L / 0.0L;
	float y = cbrtf(x);

	ATF_CHECK(isinf(y) != 0);
	ATF_CHECK(signbit(y) != 0);
}

ATF_TC(cbrtf_inf_pos);
ATF_TC_HEAD(cbrtf_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cbrtf(+Inf) == +Inf");
}

ATF_TC_BODY(cbrtf_inf_pos, tc)
{
	const float x = 1.0L / 0.0L;
	float y = cbrtf(x);

	ATF_CHECK(isinf(y) != 0);
	ATF_CHECK(signbit(y) == 0);
}

ATF_TC(cbrtf_zero_neg);
ATF_TC_HEAD(cbrtf_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cbrtf(-0.0) == -0.0");
}

ATF_TC_BODY(cbrtf_zero_neg, tc)
{
	const float x = -0.0L;
	float y = cbrtf(x);

	if (fabsf(y) > 0.0 || signbit(y) == 0)
		atf_tc_fail_nonfatal("cbrtf(-0.0) != -0.0");
}

ATF_TC(cbrtf_zero_pos);
ATF_TC_HEAD(cbrtf_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cbrtf(+0.0) == +0.0");
}

ATF_TC_BODY(cbrtf_zero_pos, tc)
{
	const float x = 0.0L;
	float y = cbrtf(x);

	if (fabsf(y) > 0.0 || signbit(y) != 0)
		atf_tc_fail_nonfatal("cbrtf(+0.0) != +0.0");
}

#if !defined(__FreeBSD__) || LDBL_PREC != 53
/*
 * cbrtl(3)
 */
ATF_TC(cbrtl_nan);
ATF_TC_HEAD(cbrtl_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cbrtl(NaN) == NaN");
}

ATF_TC_BODY(cbrtl_nan, tc)
{
	const long double x = 0.0L / 0.0L;

	ATF_CHECK(isnan(x) != 0);
	ATF_CHECK(isnan(cbrtl(x)) != 0);
}

ATF_TC(cbrtl_powl);
ATF_TC_HEAD(cbrtl_powl, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cbrtl(3) vs. powl(3)");
}

ATF_TC_BODY(cbrtl_powl, tc)
{
	const long double x[] = { 0.0, 0.005, 1.0, 99.0, 123.123, 9999.0 };
	/* Neither cbrt nor pow is required to be correctly rounded.  */
	const long double eps = 2*LDBL_EPSILON;
	size_t i;

#if defined(__amd64__) && defined(__clang__) && __clang_major__ >= 7 && \
    __clang_major__ < 10 && __FreeBSD_cc_version < 1300002
	atf_tc_expect_fail("test fails with clang 7-9 - bug 234040");
#endif
	for (i = 0; i < __arraycount(x); i++) {
		long double x_cbrt = cbrtl(x[i]);
#ifdef __FreeBSD__
		/*
		 * NetBSD doesn't have a real powl/cbrtl implementation, they
		 * just call the double version. On FreeBSD we have a real
		 * powl implementation so we have to cast the second argument
		 * to long double before dividing to get a more precise
		 * approximation of 1/3.
		 * TODO: upstream this diff.
		 */
		long double x_pow13 = powl(x[i], (long double)1.0 / 3.0);
#else
		long double x_pow13 = powl(x[i], 1.0 / 3.0);
#endif
		bool ok;

		if (x[i] == 0) {
			ok = (x_cbrt == x_pow13);
		} else {
			ok = (fabsl((x_cbrt - x_pow13)/x_cbrt) <= eps);
		}

		if (!ok) {
			atf_tc_fail_nonfatal("cbrtl(%.35Lg) = %.35Lg != "
			    "powl(%.35Lg, 1/3) = %.35Lg\n",
			    x[i], x_cbrt, x[i], x_pow13);
		}
	}
}

ATF_TC(cbrtl_inf_neg);
ATF_TC_HEAD(cbrtl_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cbrtl(-Inf) == -Inf");
}

ATF_TC_BODY(cbrtl_inf_neg, tc)
{
	const long double x = -1.0L / 0.0L;
	long double y = cbrtl(x);

	ATF_CHECK(isinf(y) != 0);
	ATF_CHECK(signbit(y) != 0);
}

ATF_TC(cbrtl_inf_pos);
ATF_TC_HEAD(cbrtl_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cbrtl(+Inf) == +Inf");
}

ATF_TC_BODY(cbrtl_inf_pos, tc)
{
	const long double x = 1.0L / 0.0L;
	long double y = cbrtl(x);

	ATF_CHECK(isinf(y) != 0);
	ATF_CHECK(signbit(y) == 0);
}

ATF_TC(cbrtl_zero_neg);
ATF_TC_HEAD(cbrtl_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cbrtl(-0.0) == -0.0");
}

ATF_TC_BODY(cbrtl_zero_neg, tc)
{
	const long double x = -0.0L;
	long double y = cbrtl(x);

	if (fabsl(y) > 0.0 || signbit(y) == 0)
		atf_tc_fail_nonfatal("cbrtl(-0.0) != -0.0");
}

ATF_TC(cbrtl_zero_pos);
ATF_TC_HEAD(cbrtl_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cbrtl(+0.0) == +0.0");
}

ATF_TC_BODY(cbrtl_zero_pos, tc)
{
	const long double x = 0.0L;
	long double y = cbrtl(x);

	if (fabsl(y) > 0.0 || signbit(y) != 0)
		atf_tc_fail_nonfatal("cbrtl(+0.0) != +0.0");
}
#endif

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, cbrt_nan);
	ATF_TP_ADD_TC(tp, cbrt_pow);
	ATF_TP_ADD_TC(tp, cbrt_inf_neg);
	ATF_TP_ADD_TC(tp, cbrt_inf_pos);
	ATF_TP_ADD_TC(tp, cbrt_zero_neg);
	ATF_TP_ADD_TC(tp, cbrt_zero_pos);

	ATF_TP_ADD_TC(tp, cbrtf_nan);
	ATF_TP_ADD_TC(tp, cbrtf_powf);
	ATF_TP_ADD_TC(tp, cbrtf_inf_neg);
	ATF_TP_ADD_TC(tp, cbrtf_inf_pos);
	ATF_TP_ADD_TC(tp, cbrtf_zero_neg);
	ATF_TP_ADD_TC(tp, cbrtf_zero_pos);

#if !defined(__FreeBSD__) || LDBL_PREC != 53
	ATF_TP_ADD_TC(tp, cbrtl_nan);
	ATF_TP_ADD_TC(tp, cbrtl_powl);
	ATF_TP_ADD_TC(tp, cbrtl_inf_neg);
	ATF_TP_ADD_TC(tp, cbrtl_inf_pos);
	ATF_TP_ADD_TC(tp, cbrtl_zero_neg);
	ATF_TP_ADD_TC(tp, cbrtl_zero_pos);
#endif

	return atf_no_error();
}
