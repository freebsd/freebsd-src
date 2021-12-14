/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1985, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * See bsdsrc/b_exp.c for implementation details.
 *
 * bsdrc/b_exp.c converted to long double by Steven G. Kargl.
 */

#include "fpmath.h"
#include "math_private.h"

static const union IEEEl2bits
    p0u = LD80C(0xaaaaaaaaaaaaaaab,    -3,  1.66666666666666666671e-01L),
    p1u = LD80C(0xb60b60b60b60b59a,    -9, -2.77777777777777775377e-03L),
    p2u = LD80C(0x8ab355e008a3cfce,   -14,  6.61375661375629297465e-05L),
    p3u = LD80C(0xddebbc994b0c1376,   -20, -1.65343915327882529784e-06L),
    p4u = LD80C(0xb354784cb4ef4c41,   -25,  4.17535101591534118469e-08L),
    p5u = LD80C(0x913e8a718382ce75,   -30, -1.05679137034774806475e-09L),
    p6u = LD80C(0xe8f0042aa134502e,   -36,  2.64819349895429516863e-11L);
#define	p1	(p0u.e)
#define	p2	(p1u.e)
#define	p3	(p2u.e)
#define	p4	(p3u.e)
#define	p5	(p4u.e)
#define	p6	(p5u.e)
#define	p7	(p6u.e)

/*
 * lnhuge = (LDBL_MAX_EXP + 9) * log(2.)
 * lntiny = (LDBL_MIN_EXP - 64 - 10) * log(2.)
 * invln2 = 1 / log(2.)
 */
static const union IEEEl2bits
ln2hiu  = LD80C(0xb17217f700000000,  -1,  6.93147180369123816490e-01L),
ln2lou  = LD80C(0xd1cf79abc9e3b398, -33,  1.90821492927058781614e-10L),
lnhugeu = LD80C(0xb18b0c0330a8fad9,  13,  1.13627617309191834574e+04L),
lntinyu = LD80C(0xb236f28a68bc3bd7,  13, -1.14057368561139000667e+04L),
invln2u = LD80C(0xb8aa3b295c17f0bc,   0,  1.44269504088896340739e+00L);
#define	ln2hi	(ln2hiu.e)
#define ln2lo	(ln2lou.e)
#define lnhuge	(lnhugeu.e)
#define	lntiny	(lntinyu.e)
#define	invln2	(invln2u.e)

/* returns exp(r = x + c) for |c| < |x| with no overlap.  */

static long double
__exp__D(long double x, long double c)
{
	long double hi, lo, z;
	int k;

	if (x != x)	/* x is NaN. */
		return(x);

	if (x <= lnhuge) {
		if (x >= lntiny) {
			/* argument reduction: x --> x - k*ln2 */
			z = invln2 * x;
			k = z + copysignl(0.5L, x);

		    	/*
			 * Express (x + c) - k * ln2 as hi - lo.
			 * Let x = hi - lo rounded.
			 */
			hi = x - k * ln2hi;	/* Exact. */
			lo = k * ln2lo - c;
			x = hi - lo;

			/* Return 2^k*[1+x+x*c/(2+c)]  */
			z = x * x;
			c = x - z * (p1 + z * (p2 + z * (p3 + z * (p4 +
			    z * (p5 + z * (p6 + z * p7))))));
			c = (x * c) / (2 - c);

			return (ldexpl(1 + (hi - (lo - c)), k));
		} else {
			/* exp(-INF) is 0. exp(-big) underflows to 0.  */
			return (isfinite(x) ? ldexpl(1., -5000) : 0);
		}
	} else
		/* exp(INF) is INF, exp(+big#) overflows to INF */
		return (isfinite(x) ? ldexpl(1., 5000) : x);
}
