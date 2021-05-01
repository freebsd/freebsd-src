/* $NetBSD: t_scalbn.c,v 1.16 2018/11/07 03:59:36 riastradh Exp $ */

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
__RCSID("$NetBSD: t_scalbn.c,v 1.16 2018/11/07 03:59:36 riastradh Exp $");

#include <math.h>
#include <limits.h>
#include <float.h>
#include <errno.h>
#include <fenv.h>

#include <atf-c.h>

static const int exps[] = { 0, 1, -1, 100, -100 };

/* tests here do not require specific precision, so we just use double */
struct testcase {
	int exp;
	double inval;
	double result;
	int error;
	int except;
};
static struct testcase test_vals[] = {
	{ 0,		1.00085,	1.00085,	0, 0 },
	{ 0,		0.99755,	0.99755,	0, 0 },
	{ 0,		-1.00085,	-1.00085,	0, 0 },
	{ 0,		-0.99755,	-0.99755,	0, 0 },
	{ 1,		1.00085,	2.0* 1.00085,	0, 0 },
	{ 1,		0.99755,	2.0* 0.99755,	0, 0 },
	{ 1,		-1.00085,	2.0* -1.00085,	0, 0 },
	{ 1,		-0.99755,	2.0* -0.99755,	0, 0 },

	/*
	 * We could add more corner test cases here, but we would have to
	 * add some ifdefs for the exact format and use a reliable
	 * generator program - bail for now and only do trivial stuff above.
	 */
};

/*
 * scalbn(3)
 */
ATF_TC(scalbn_val);
ATF_TC_HEAD(scalbn_val, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test scalbn() for a few values");
}

ATF_TC_BODY(scalbn_val, tc)
{
	const struct testcase *tests = test_vals;
	const size_t tcnt = __arraycount(test_vals);
	size_t i;
	double rv;

	for (i = 0; i < tcnt; i++) {
		errno = 0;
#ifndef __vax__
		feclearexcept(FE_ALL_EXCEPT);
#endif
		rv = scalbn(tests[i].inval, tests[i].exp);
		ATF_CHECK_EQ_MSG(errno, tests[i].error,
		    "test %zu: errno %d instead of %d", i, errno,
		    tests[i].error);
#ifndef __vax__
		ATF_CHECK_EQ_MSG(errno, tests[i].error,
		    "test %zu: fetestexcept %d instead of %d", i,
		    fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW),
		    tests[i].except);
#endif
		/* scalbn is always exact except for underflow or overflow.  */
		ATF_CHECK_MSG(rv == tests[i].result,
		    "test %zu: return value %.17g instead of %.17g"
		    " (error %.17g)",
		    i, rv, tests[i].result,
		    fabs((tests[i].result - rv)/tests[i].result));
	}
}

ATF_TC(scalbn_nan);
ATF_TC_HEAD(scalbn_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test scalbn(NaN, n) == NaN");
}

ATF_TC_BODY(scalbn_nan, tc)
{
	const double x = 0.0L / 0.0L;
	double y;
	size_t i;

	ATF_REQUIRE(isnan(x) != 0);

	for (i = 0; i < __arraycount(exps); i++) {
		y = scalbn(x, exps[i]);
		ATF_CHECK(isnan(y) != 0);
	}
}

ATF_TC(scalbn_inf_neg);
ATF_TC_HEAD(scalbn_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test scalbn(-Inf, n) == -Inf");
}

ATF_TC_BODY(scalbn_inf_neg, tc)
{
	const double x = -1.0L / 0.0L;
	size_t i;

	for (i = 0; i < __arraycount(exps); i++)
		ATF_CHECK(scalbn(x, exps[i]) == x);
}

ATF_TC(scalbn_inf_pos);
ATF_TC_HEAD(scalbn_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test scalbn(+Inf, n) == +Inf");
}

ATF_TC_BODY(scalbn_inf_pos, tc)
{
	const double x = 1.0L / 0.0L;
	size_t i;

	for (i = 0; i < __arraycount(exps); i++)
		ATF_CHECK(scalbn(x, exps[i]) == x);
}

ATF_TC(scalbn_ldexp);
ATF_TC_HEAD(scalbn_ldexp, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test scalbn(x, n) == ldexp(x, n)");
}

ATF_TC_BODY(scalbn_ldexp, tc)
{
#if FLT_RADIX == 2
	const double x = 2.91288191221812821;
	double y;
	size_t i;

	for (i = 0; i < __arraycount(exps); i++) {
		y = scalbn(x, exps[i]);
		ATF_CHECK_MSG(y == ldexp(x, exps[i]), "test %zu: exponent=%d, "
		    "y=%g, expected %g (diff: %g)", i, exps[i], y, 
		    ldexp(x, exps[i]), y - ldexp(x, exps[i]));
	}
#endif
}

ATF_TC(scalbn_zero_neg);
ATF_TC_HEAD(scalbn_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test scalbn(-0.0, n) == -0.0");
}

ATF_TC_BODY(scalbn_zero_neg, tc)
{
	const double x = -0.0L;
	double y;
	size_t i;

	ATF_REQUIRE(signbit(x) != 0);

	for (i = 0; i < __arraycount(exps); i++) {
		y = scalbn(x, exps[i]);
		ATF_CHECK(x == y);
		ATF_CHECK(signbit(y) != 0);
	}
}

ATF_TC(scalbn_zero_pos);
ATF_TC_HEAD(scalbn_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test scalbn(+0.0, n) == +0.0");
}

ATF_TC_BODY(scalbn_zero_pos, tc)
{
	const double x = 0.0L;
	double y;
	size_t i;

	ATF_REQUIRE(signbit(x) == 0);

	for (i = 0; i < __arraycount(exps); i++) {
		y = scalbn(x, exps[i]);
		ATF_CHECK(x == y);
		ATF_CHECK(signbit(y) == 0);
	}
}

/*
 * scalbnf(3)
 */
ATF_TC(scalbnf_val);
ATF_TC_HEAD(scalbnf_val, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test scalbnf() for a few values");
}

ATF_TC_BODY(scalbnf_val, tc)
{
	const struct testcase *tests = test_vals;
	const size_t tcnt = __arraycount(test_vals);
	size_t i;
	double rv;

	for (i = 0; i < tcnt; i++) {
		errno = 0;
		rv = scalbnf(tests[i].inval, tests[i].exp);
		ATF_CHECK_EQ_MSG(errno, tests[i].error,
		    "test %zu: errno %d instead of %d", i, errno,
		    tests[i].error);
		/* scalbn is always exact except for underflow or overflow.  */
		ATF_CHECK_MSG(rv == (float)tests[i].result,
		    "test %zu: return value %.8g instead of %.8g"
		    " (error %.8g)",
		    i, rv, tests[i].result,
		    fabs((tests[i].result - rv)/tests[i].result));
	}
}

ATF_TC(scalbnf_nan);
ATF_TC_HEAD(scalbnf_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test scalbnf(NaN, n) == NaN");
}

ATF_TC_BODY(scalbnf_nan, tc)
{
	const float x = 0.0L / 0.0L;
	float y;
	size_t i;

	ATF_REQUIRE(isnan(x) != 0);

	for (i = 0; i < __arraycount(exps); i++) {
		y = scalbnf(x, exps[i]);
		ATF_CHECK(isnan(y) != 0);
	}
}

ATF_TC(scalbnf_inf_neg);
ATF_TC_HEAD(scalbnf_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test scalbnf(-Inf, n) == -Inf");
}

ATF_TC_BODY(scalbnf_inf_neg, tc)
{
	const float x = -1.0L / 0.0L;
	size_t i;

	for (i = 0; i < __arraycount(exps); i++)
		ATF_CHECK(scalbnf(x, exps[i]) == x);
}

ATF_TC(scalbnf_inf_pos);
ATF_TC_HEAD(scalbnf_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test scalbnf(+Inf, n) == +Inf");
}

ATF_TC_BODY(scalbnf_inf_pos, tc)
{
	const float x = 1.0L / 0.0L;
	size_t i;

	for (i = 0; i < __arraycount(exps); i++)
		ATF_CHECK(scalbnf(x, exps[i]) == x);
}

ATF_TC(scalbnf_ldexpf);
ATF_TC_HEAD(scalbnf_ldexpf, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test scalbnf(x, n) == ldexpf(x, n)");
}

ATF_TC_BODY(scalbnf_ldexpf, tc)
{
#if FLT_RADIX == 2
	const float x = 2.91288191221812821;
	float y;
	size_t i;

	for (i = 0; i < __arraycount(exps); i++) {
		y = scalbnf(x, exps[i]);
		ATF_CHECK_MSG(y == ldexpf(x, exps[i]),
		    "test %zu: exponent=%d, y=%g ldexpf returns %g (diff: %g)",
		    i, exps[i], y, ldexpf(x, exps[i]), y-ldexpf(x, exps[i]));
	}
#endif
}

ATF_TC(scalbnf_zero_neg);
ATF_TC_HEAD(scalbnf_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test scalbnf(-0.0, n) == -0.0");
}

ATF_TC_BODY(scalbnf_zero_neg, tc)
{
	const float x = -0.0L;
	float y;
	size_t i;

	ATF_REQUIRE(signbit(x) != 0);

	for (i = 0; i < __arraycount(exps); i++) {
		y = scalbnf(x, exps[i]);
		ATF_CHECK(x == y);
		ATF_CHECK(signbit(y) != 0);
	}
}

ATF_TC(scalbnf_zero_pos);
ATF_TC_HEAD(scalbnf_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test scalbnf(+0.0, n) == +0.0");
}

ATF_TC_BODY(scalbnf_zero_pos, tc)
{
	const float x = 0.0L;
	float y;
	size_t i;

	ATF_REQUIRE(signbit(x) == 0);

	for (i = 0; i < __arraycount(exps); i++) {
		y = scalbnf(x, exps[i]);
		ATF_CHECK(x == y);
		ATF_CHECK(signbit(y) == 0);
	}
}

/*
 * scalbnl(3)
 */
ATF_TC(scalbnl_val);
ATF_TC_HEAD(scalbnl_val, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test scalbnl() for a few values");
}

ATF_TC_BODY(scalbnl_val, tc)
{
#ifndef __HAVE_LONG_DOUBLE
	atf_tc_skip("Requires long double support");
#else
	const struct testcase *tests = test_vals;
	const size_t tcnt = __arraycount(test_vals);
	size_t i;
	long double rv;

	for (i = 0; i < tcnt; i++) {
		errno = 0;
		rv = scalbnl(tests[i].inval, tests[i].exp);
		ATF_CHECK_EQ_MSG(errno, tests[i].error,
		    "test %zu: errno %d instead of %d", i, errno,
		    tests[i].error);
		/* scalbn is always exact except for underflow or overflow.  */
		ATF_CHECK_MSG(rv == (long double)tests[i].result,
		    "test %zu: return value %.35Lg instead of %.35Lg"
		    " (error %.35Lg)",
		    i, rv, (long double)tests[i].result,
		    fabsl(((long double)tests[i].result - rv)/tests[i].result));
	}
#endif
}

ATF_TC(scalbnl_nan);
ATF_TC_HEAD(scalbnl_nan, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test scalbnl(NaN, n) == NaN");
}

ATF_TC_BODY(scalbnl_nan, tc)
{
#ifndef __HAVE_LONG_DOUBLE
	atf_tc_skip("Requires long double support");
#else
	const long double x = 0.0L / 0.0L;
	long double y;
	size_t i;

	if (isnan(x) == 0) {
		atf_tc_expect_fail("PR lib/45362");
		atf_tc_fail("(0.0L / 0.0L) != NaN");
	}

	for (i = 0; i < __arraycount(exps); i++) {
		y = scalbnl(x, exps[i]);
		ATF_CHECK(isnan(y) != 0);
	}
#endif
}

ATF_TC(scalbnl_inf_neg);
ATF_TC_HEAD(scalbnl_inf_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test scalbnl(-Inf, n) == -Inf");
}

ATF_TC_BODY(scalbnl_inf_neg, tc)
{
#ifndef __HAVE_LONG_DOUBLE
	atf_tc_skip("Requires long double support");
#else
	const long double x = -1.0L / 0.0L;
	size_t i;

	for (i = 0; i < __arraycount(exps); i++)
		ATF_CHECK(scalbnl(x, exps[i]) == x);
#endif
}

ATF_TC(scalbnl_inf_pos);
ATF_TC_HEAD(scalbnl_inf_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test scalbnl(+Inf, n) == +Inf");
}

ATF_TC_BODY(scalbnl_inf_pos, tc)
{
#ifndef __HAVE_LONG_DOUBLE
	atf_tc_skip("Requires long double support");
#else
	const long double x = 1.0L / 0.0L;
	size_t i;

	for (i = 0; i < __arraycount(exps); i++)
		ATF_CHECK(scalbnl(x, exps[i]) == x);
#endif
}

ATF_TC(scalbnl_zero_neg);
ATF_TC_HEAD(scalbnl_zero_neg, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test scalbnl(-0.0, n) == -0.0");
}

ATF_TC_BODY(scalbnl_zero_neg, tc)
{
#ifndef __HAVE_LONG_DOUBLE
	atf_tc_skip("Requires long double support");
#else
	const long double x = -0.0L;
	long double y;
	size_t i;

	ATF_REQUIRE(signbit(x) != 0);

	for (i = 0; i < __arraycount(exps); i++) {
		y = scalbnl(x, exps[i]);
		ATF_CHECK(x == y);
		ATF_CHECK(signbit(y) != 0);
	}
#endif
}

ATF_TC(scalbnl_zero_pos);
ATF_TC_HEAD(scalbnl_zero_pos, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test scalbnl(+0.0, n) == +0.0");
}

ATF_TC_BODY(scalbnl_zero_pos, tc)
{
#ifndef __HAVE_LONG_DOUBLE
	atf_tc_skip("Requires long double support");
#else
	const long double x = 0.0L;
	long double y;
	size_t i;

	ATF_REQUIRE(signbit(x) == 0);

	for (i = 0; i < __arraycount(exps); i++) {
		y = scalbnl(x, exps[i]);
		ATF_CHECK(x == y);
		ATF_CHECK(signbit(y) == 0);
	}
#endif
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, scalbn_val);
	ATF_TP_ADD_TC(tp, scalbn_nan);
	ATF_TP_ADD_TC(tp, scalbn_inf_neg);
	ATF_TP_ADD_TC(tp, scalbn_inf_pos);
	ATF_TP_ADD_TC(tp, scalbn_ldexp);
	ATF_TP_ADD_TC(tp, scalbn_zero_neg);
	ATF_TP_ADD_TC(tp, scalbn_zero_pos);

	ATF_TP_ADD_TC(tp, scalbnf_val);
	ATF_TP_ADD_TC(tp, scalbnf_nan);
	ATF_TP_ADD_TC(tp, scalbnf_inf_neg);
	ATF_TP_ADD_TC(tp, scalbnf_inf_pos);
	ATF_TP_ADD_TC(tp, scalbnf_ldexpf);
	ATF_TP_ADD_TC(tp, scalbnf_zero_neg);
	ATF_TP_ADD_TC(tp, scalbnf_zero_pos);

	ATF_TP_ADD_TC(tp, scalbnl_val);
	ATF_TP_ADD_TC(tp, scalbnl_nan);
	ATF_TP_ADD_TC(tp, scalbnl_inf_neg);
	ATF_TP_ADD_TC(tp, scalbnl_inf_pos);
/*	ATF_TP_ADD_TC(tp, scalbnl_ldexp);	*/
	ATF_TP_ADD_TC(tp, scalbnl_zero_neg);
	ATF_TP_ADD_TC(tp, scalbnl_zero_pos);

	return atf_no_error();
}
