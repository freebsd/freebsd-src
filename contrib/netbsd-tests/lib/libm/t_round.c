/* $NetBSD: t_round.c,v 1.9 2017/09/03 13:41:19 wiz Exp $ */

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
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

#include <sys/param.h>

#include <atf-c.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

/*
 * This tests for a bug in the initial implementation where
 * precision was lost in an internal substraction, leading to
 * rounding into the wrong direction.
 */

/* 0.5 - EPSILON */
#define VAL	0x0.7ffffffffffffcp0
#define VALF	0x0.7fffff8p0
#define VALL	(0.5 - LDBL_EPSILON)

#ifdef __vax__
#define SMALL_NUM	1.0e-38
#else
#define SMALL_NUM	1.0e-40
#endif

ATF_TC(round_dir);
ATF_TC_HEAD(round_dir, tc)
{
	atf_tc_set_md_var(tc, "descr","Check for rounding in wrong direction");
}

ATF_TC_BODY(round_dir, tc)
{
	double a = VAL, b, c;
	float af = VALF, bf, cf;
	long double al = VALL, bl, cl;

	b = round(a);
	bf = roundf(af);
	bl = roundl(al);

	ATF_CHECK(fabs(b) < SMALL_NUM);
	ATF_CHECK(fabsf(bf) < SMALL_NUM);
	ATF_CHECK(fabsl(bl) < SMALL_NUM);

	c = round(-a);
	cf = roundf(-af);
	cl = roundl(-al);

	ATF_CHECK(fabs(c) < SMALL_NUM);
	ATF_CHECK(fabsf(cf) < SMALL_NUM);
	ATF_CHECK(fabsl(cl) < SMALL_NUM);
}

ATF_TC(rounding_alpha);
ATF_TC_HEAD(rounding_alpha, tc)
{
	atf_tc_set_md_var(tc, "descr","Checking MPFR's config failure with -mieee on Alpha");
}

typedef uint64_t gimpy_limb_t;
#define GIMPY_NUMB_BITS 64

ATF_TC_BODY(rounding_alpha, tc)
{
        double d;
        gimpy_limb_t u;
        int i;

        d = 1.0;
        for (i = 0; i < GIMPY_NUMB_BITS - 1; i++)
                d = d + d;

        printf("d = %g\n", d);
        u = (gimpy_limb_t) d;

        for (; i > 0; i--) {
                ATF_CHECK_MSG((u % 2 == 0),
		    "%"PRIu64" is not an even number! (iteration %d)", u , i);
                u = u >> 1;
        }
}

ATF_TC(rounding_alpha_simple);
ATF_TC_HEAD(rounding_alpha_simple, tc)
{
	atf_tc_set_md_var(tc, "descr","Checking double to uint64_t edge case");
}


static double rounding_alpha_simple_even = 9223372036854775808.000000; /* 2^63 */

ATF_TC_BODY(rounding_alpha_simple, tc)
{
	uint64_t unsigned_even = rounding_alpha_simple_even;

	ATF_CHECK_MSG(unsigned_even % 2 == 0,
	    "2^63 cast to uint64_t is odd (got %"PRIu64")", unsigned_even);

}
ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, round_dir);
	ATF_TP_ADD_TC(tp, rounding_alpha);
	ATF_TP_ADD_TC(tp, rounding_alpha_simple);

	return atf_no_error();
}
