/* $NetBSD: t_bit.c,v 1.1 2019/04/26 08:52:16 maya Exp $ */

/*
 * Written by Maya Rashish <maya@NetBSD.org>
 * Public domain.
 *
 * Testing signbit{,f,l} function correctly
 */

#include <atf-c.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

static const struct {
	double input;
	bool is_negative;
} values[] = {
	{ -1,		true},
	{ -123,		true},
	{ -123E6,	true},
#ifdef INFINITY
	{ -INFINITY,	true},
	{ INFINITY,	false},
#endif
	{ 123E6,	false},
	{ 0,		false},
	{ -FLT_MIN,	true},
	{ FLT_MIN,	false},
	/* 
	 * Cannot be accurately represented as float,
	 * but sign should be preserved
	 */
	{ DBL_MAX,	false},
	{ -DBL_MAX,	true},
};

#ifdef __HAVE_LONG_DOUBLE
static const struct {
	long double input;
	bool is_negative;
} ldbl_values[] = {
	{ -LDBL_MIN,	true},
	{ LDBL_MIN,	false},
	{ LDBL_MAX,	false},
	{ -LDBL_MAX,	true},
};
#endif

ATF_TC(signbit);
ATF_TC_HEAD(signbit, tc)
{
	atf_tc_set_md_var(tc, "descr","Check that signbit functions correctly");
}

ATF_TC_BODY(signbit, tc)
{
	double iterator_d;
	float iterator_f;

	for (unsigned int i = 0; i < __arraycount(values); i++) {
		iterator_d = values[i].input;
		iterator_f = (float) values[i].input;
		if (signbit(iterator_f) != values[i].is_negative)
			atf_tc_fail("%s:%d iteration %d signbitf is wrong"
					" about the sign of %f", __func__,
					__LINE__, i, iterator_f);
		if (signbit(iterator_d) != values[i].is_negative)
			atf_tc_fail("%s:%d iteration %d signbit is wrong"
					"about the sign of %f", __func__,
					__LINE__,i, iterator_d);

#ifdef __HAVE_LONG_DOUBLE
		long double iterator_l = values[i].input;
		if (signbit(iterator_l) != values[i].is_negative)
			atf_tc_fail("%s:%d iteration %d signbitl is wrong"
					" about the sign of %Lf", __func__,
					__LINE__, i, iterator_l);
#endif
	}

#ifdef __HAVE_LONG_DOUBLE
	for (unsigned int i = 0; i < __arraycount(ldbl_values); i++) {
		if (signbit(ldbl_values[i].input) != ldbl_values[i].is_negative)
			atf_tc_fail("%s:%d iteration %d signbitl is"
					"wrong about the sign of %Lf",
					__func__, __LINE__, i,
					ldbl_values[i].input);
	}
#endif

}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, signbit);

	return atf_no_error();
}
