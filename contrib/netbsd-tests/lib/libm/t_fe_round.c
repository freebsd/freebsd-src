/*
 * Written by Maya Rashish <maya@NetBSD.org>
 * Public domain.
 *
 * Testing IEEE-754 rounding modes (and lrint)
 */

#include <atf-c.h>
#include <fenv.h>
#ifdef __HAVE_FENV
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/*#pragma STDC FENV_ACCESS ON gcc?? */

#define INT 9223L

#define EPSILON 0.001

static const struct {
	int round_mode;
	double input;
	long int expected;
} values[] = {
	{ FE_DOWNWARD,		3.7,		3},
	{ FE_DOWNWARD,		-3.7,		-4},
	{ FE_DOWNWARD,		+0,		0},
	{ FE_DOWNWARD,		-INT-0.01,	-INT-1},
	{ FE_DOWNWARD,		+INT-0.01,	INT-1},
	{ FE_DOWNWARD,		-INT+0.01,	-INT},
	{ FE_DOWNWARD,		+INT+0.01,	INT},
#if 0 /* cpu bugs? */
	{ FE_DOWNWARD,		-0,		-1},

	{ FE_UPWARD,		+0,		1},
#endif
	{ FE_UPWARD,		-0,		0},
	{ FE_UPWARD,		-123.7,		-123},
	{ FE_UPWARD,		123.999,	124},
	{ FE_UPWARD,		-INT-0.01,	-INT},
	{ FE_UPWARD,		+INT-0.01,	INT},
	{ FE_UPWARD,		-INT+0.01,	-INT+1},
	{ FE_UPWARD,		+INT+0.01,	INT+1},

	{ FE_TOWARDZERO,	1.99,		1},
	{ FE_TOWARDZERO,	-1.99,		-1},
	{ FE_TOWARDZERO,	0.2,		0},
	{ FE_TOWARDZERO,	INT+0.01,	INT},
	{ FE_TOWARDZERO,	INT-0.01,	INT - 1},
	{ FE_TOWARDZERO,	-INT+0.01,	-INT + 1},
	{ FE_TOWARDZERO,	+0,		0},
	{ FE_TOWARDZERO,	-0,		0},

	{ FE_TONEAREST,		-INT-0.01,	-INT},
	{ FE_TONEAREST,		+INT-0.01,	INT},
	{ FE_TONEAREST,		-INT+0.01,	-INT},
	{ FE_TONEAREST,		+INT+0.01,	INT},
	{ FE_TONEAREST,		-INT-0.501,	-INT-1},
	{ FE_TONEAREST,		+INT-0.501,	INT-1},
	{ FE_TONEAREST,		-INT+0.501,	-INT+1},
	{ FE_TONEAREST,		+INT+0.501,	INT+1},
	{ FE_TONEAREST,		+0,		0},
	{ FE_TONEAREST,		-0,		0},
};

ATF_TC(fe_round);
ATF_TC_HEAD(fe_round, tc)
{
	atf_tc_set_md_var(tc, "descr","Checking IEEE 754 rounding modes using lrint");
}

ATF_TC_BODY(fe_round, tc)
{
	long int received;

	for (unsigned int i = 0; i < __arraycount(values); i++) {
		fesetround(values[i].round_mode);

		received = lrint(values[i].input);
		ATF_CHECK_MSG(
		    (labs(received - values[i].expected) < EPSILON),
		    "lrint rounding wrong, difference too large\n"
		    "input: %f (index %d): got %ld, expected %ld\n",
		    values[i].input, i, received, values[i].expected);

		/* Do we get the same rounding mode out? */
		ATF_CHECK_MSG(
		    (fegetround() == values[i].round_mode),
		    "Didn't get the same rounding mode out!\n"
		    "(index %d) fed in %d rounding mode, got %d out\n",
		    i, values[i].round_mode, fegetround());
	}
}

ATF_TC(fe_nearbyint);
ATF_TC_HEAD(fe_nearbyint, tc)
{
	atf_tc_set_md_var(tc, "descr","Checking IEEE 754 rounding modes using nearbyint");
}

ATF_TC_BODY(fe_nearbyint, tc)
{
	double received;

	for (unsigned int i = 0; i < __arraycount(values); i++) {
		fesetround(values[i].round_mode);

		received = nearbyint(values[i].input);
		ATF_CHECK_MSG(
		    (fabs(received - values[i].expected) < EPSILON),
		    "nearbyint rounding wrong, difference too large\n"
		    "input: %f (index %d): got %f, expected %ld\n",
		    values[i].input, i, received, values[i].expected);

		/* Do we get the same rounding mode out? */
		ATF_CHECK_MSG(
		    (fegetround() == values[i].round_mode),
		    "Didn't get the same rounding mode out!\n"
		    "(index %d) fed in %d rounding mode, got %d out\n",
		    i, values[i].round_mode, fegetround());
	}
}

static const struct {
	double input;
	double toward;
	double expected;
} values2[] = {
	{ 10.0, 11.0, 10.0 },
	{ -5.0, -6.0, -5.0 },
};

ATF_TC(fe_nextafter);
ATF_TC_HEAD(fe_nextafter, tc)
{
	atf_tc_set_md_var(tc, "descr", "Checking IEEE 754 rounding using nextafter()");
}

ATF_TC_BODY(fe_nextafter, tc)
{
	double received;
	int res;

	for (unsigned int i = 0; i < __arraycount(values2); i++) {
		received = nextafter(values2[i].input, values2[i].toward);
		if (values2[i].input < values2[i].toward) {
			res = (received > values2[i].input);
		} else {
			res = (received < values2[i].input);
		}
		ATF_CHECK_MSG(
			res && (fabs(received - values2[i].expected) < EPSILON),
			"nextafter() rounding wrong, difference too large\n"
			"input: %f (index %d): got %f, expected %f, res %d\n",
			values2[i].input, i, received, values2[i].expected, res);
	}
}

ATF_TC(fe_nexttoward);
ATF_TC_HEAD(fe_nexttoward, tc)
{
	atf_tc_set_md_var(tc, "descr", "Checking IEEE 754 rounding using nexttoward()");
}

ATF_TC_BODY(fe_nexttoward, tc)
{
	double received;
	int res;

	for (unsigned int i = 0; i < __arraycount(values2); i++) {
		received = nexttoward(values2[i].input, values2[i].toward);
		if (values2[i].input < values2[i].toward) {
			res = (received > values2[i].input);
		} else {
			res = (received < values2[i].input);
		}
		ATF_CHECK_MSG(
			res && (fabs(received - values2[i].expected) < EPSILON),
			"nexttoward() rounding wrong, difference too large\n"
			"input: %f (index %d): got %f, expected %f, res %d\n",
			values2[i].input, i, received, values2[i].expected, res);
	}
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, fe_round);
	ATF_TP_ADD_TC(tp, fe_nearbyint);
	ATF_TP_ADD_TC(tp, fe_nextafter);
	ATF_TP_ADD_TC(tp, fe_nexttoward);

	return atf_no_error();
}
#else
ATF_TC(t_nofe_round);

ATF_TC_HEAD(t_nofe_round, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "dummy test case - no fenv.h support");
}

ATF_TC_BODY(t_nofe_round, tc)
{
	atf_tc_skip("no fenv.h support on this architecture");
}

ATF_TC(t_nofe_nearbyint);

ATF_TC_HEAD(t_nofe_nearbyint, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "dummy test case - no fenv.h support");
}

ATF_TC_BODY(t_nofe_nearbyint, tc)
{
	atf_tc_skip("no fenv.h support on this architecture");
}

ATF_TC(t_nofe_nextafter);

ATF_TC_HEAD(t_nofe_nextafter, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "dummy test case - no fenv.h support");
}

ATF_TC_BODY(t_nofe_nextafter, tc)
{
	atf_tc_skip("no fenv.h support on this architecture");
}

ATF_TC(t_nofe_nexttoward);

ATF_TC_HEAD(t_nofe_nexttoward, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "dummy test case - no fenv.h support");
}

ATF_TC_BODY(t_nofe_nexttoward, tc)
{
	atf_tc_skip("no fenv.h support on this architecture");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, t_nofe_round);
	ATF_TP_ADD_TC(tp, t_nofe_nearbyint);
	ATF_TP_ADD_TC(tp, t_nofe_nextafter);
	ATF_TP_ADD_TC(tp, t_nofe_nexttoward);
	return atf_no_error();
}

#endif
