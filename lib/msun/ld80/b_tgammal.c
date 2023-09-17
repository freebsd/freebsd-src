/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1992, 1993
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
 * The original code, FreeBSD's old svn r93211, contain the following
 * attribution:
 *
 *    This code by P. McIlroy, Oct 1992;
 *
 *    The financial support of UUNET Communications Services is greatfully
 *    acknowledged.
 *
 * bsdrc/b_tgamma.c converted to long double by Steven G. Kargl.
 */

/*
 * See bsdsrc/t_tgamma.c for implementation details.
 */

#include <float.h>

#if LDBL_MAX_EXP != 0x4000
#error "Unsupported long double format"
#endif

#ifdef __i386__
#include <ieeefp.h>
#endif

#include "fpmath.h"
#include "math.h"
#include "math_private.h"

/* Used in b_log.c and below. */
struct Double {
	long double a;
	long double b;
};

#include "b_logl.c"
#include "b_expl.c"

static const double zero = 0.;
static const volatile double tiny = 1e-300;
/*
 * x >= 6
 *
 * Use the asymptotic approximation (Stirling's formula) adjusted for
 * equal-ripples:
 *
 * log(G(x)) ~= (x-0.5)*(log(x)-1) + 0.5(log(2*pi)-1) + 1/x*P(1/(x*x))
 *
 * Keep extra precision in multiplying (x-.5)(log(x)-1), to avoid
 * premature round-off.
 *
 * Accurate to max(ulp(1/128) absolute, 2^-66 relative) error.
 */

/*
 * The following is a decomposition of 0.5 * (log(2*pi) - 1) into the
 * first 12 bits in ln2pi_hi and the trailing 64 bits in ln2pi_lo.  The
 * variables are clearly misnamed.
 */
static const union IEEEl2bits
ln2pi_hiu = LD80C(0xd680000000000000,  -2,  4.18945312500000000000e-01L),
ln2pi_lou = LD80C(0xe379b414b596d687, -18, -6.77929532725821967032e-06L);
#define	ln2pi_hi	(ln2pi_hiu.e)
#define	ln2pi_lo	(ln2pi_lou.e)

static const union IEEEl2bits
    Pa0u = LD80C(0xaaaaaaaaaaaaaaaa,  -4,  8.33333333333333333288e-02L),
    Pa1u = LD80C(0xb60b60b60b5fcd59,  -9, -2.77777777777776516326e-03L),
    Pa2u = LD80C(0xd00d00cffbb47014, -11,  7.93650793635429639018e-04L),
    Pa3u = LD80C(0x9c09c07c0805343e, -11, -5.95238087960599252215e-04L),
    Pa4u = LD80C(0xdca8d31f8e6e5e8f, -11,  8.41749082509607342883e-04L),
    Pa5u = LD80C(0xfb4d4289632f1638, -10, -1.91728055205541624556e-03L),
    Pa6u = LD80C(0xd15a4ba04078d3f8,  -8,  6.38893788027752396194e-03L),
    Pa7u = LD80C(0xe877283110bcad95,  -6, -2.83771309846297590312e-02L),
    Pa8u = LD80C(0x8da97eed13717af8,  -3,  1.38341887683837576925e-01L),
    Pa9u = LD80C(0xf093b1c1584e30ce,  -2, -4.69876818515470146031e-01L);
#define	Pa0	(Pa0u.e)
#define	Pa1	(Pa1u.e)
#define	Pa2	(Pa2u.e)
#define	Pa3	(Pa3u.e)
#define	Pa4	(Pa4u.e)
#define	Pa5	(Pa5u.e)
#define	Pa6	(Pa6u.e)
#define	Pa7	(Pa7u.e)
#define	Pa8	(Pa8u.e)
#define	Pa9	(Pa9u.e)

static struct Double
large_gam(long double x)
{
	long double p, z, thi, tlo, xhi, xlo;
	long double logx;
	struct Double u;

	z = 1 / (x * x);
	p = Pa0 + z * (Pa1 + z * (Pa2 + z * (Pa3 + z * (Pa4 + z * (Pa5 +
	    z * (Pa6 + z * (Pa7 + z * (Pa8 + z * Pa9))))))));
	p = p / x;

	u = __log__D(x);
	u.a -= 1;

	/* Split (x - 0.5) in high and low parts. */
	x -= 0.5L;
	xhi = (float)x;
	xlo = x - xhi;

	/* Compute  t = (x-.5)*(log(x)-1) in extra precision. */
	thi = xhi * u.a;
	tlo = xlo * u.a + x * u.b;

	/* Compute thi + tlo + ln2pi_hi + ln2pi_lo + p. */
	tlo += ln2pi_lo;
	tlo += p;
	u.a = ln2pi_hi + tlo;
	u.a += thi;
	u.b = thi - u.a;
	u.b += ln2pi_hi;
	u.b += tlo;
	return (u);
}
/*
 * Rational approximation, A0 + x * x * P(x) / Q(x), on the interval
 * [1.066.., 2.066..] accurate to 4.25e-19.
 *
 * Returns r.a + r.b = a0 + (z + c)^2 * p / q, with r.a truncated.
 */
static const union IEEEl2bits
    a0_hiu = LD80C(0xe2b6e4153a57746c,  -1, 8.85603194410888700265e-01L),
    a0_lou = LD80C(0x851566d40f32c76d, -66, 1.40907742727049706207e-20L);
#define	a0_hi	(a0_hiu.e)
#define	a0_lo	(a0_lou.e)

static const union IEEEl2bits
P0u = LD80C(0xdb629fb9bbdc1c1d,    -2,  4.28486815855585429733e-01L),
P1u = LD80C(0xe6f4f9f5641aa6be,    -3,  2.25543885805587730552e-01L),
P2u = LD80C(0xead1bd99fdaf7cc1,    -6,  2.86644652514293482381e-02L),
P3u = LD80C(0x9ccc8b25838ab1e0,    -8,  4.78512567772456362048e-03L),
P4u = LD80C(0x8f0c4383ef9ce72a,    -9,  2.18273781132301146458e-03L),
P5u = LD80C(0xe732ab2c0a2778da,   -13,  2.20487522485636008928e-04L),
P6u = LD80C(0xce70b27ca822b297,   -16,  2.46095923774929264284e-05L),
P7u = LD80C(0xa309e2e16fb63663,   -19,  2.42946473022376182921e-06L),
P8u = LD80C(0xaf9c110efb2c633d,   -23,  1.63549217667765869987e-07L),
Q1u = LD80C(0xd4d7422719f48f15,    -1,  8.31409582658993993626e-01L),
Q2u = LD80C(0xe13138ea404f1268,    -5, -5.49785826915643198508e-02L),
Q3u = LD80C(0xd1c6cc91989352c0,    -4, -1.02429960435139887683e-01L),
Q4u = LD80C(0xa7e9435a84445579,    -7,  1.02484853505908820524e-02L),
Q5u = LD80C(0x83c7c34db89b7bda,    -8,  4.02161632832052872697e-03L),
Q6u = LD80C(0xbed06bf6e1c14e5b,   -11, -7.27898206351223022157e-04L),
Q7u = LD80C(0xef05bf841d4504c0,   -18,  7.12342421869453515194e-06L),
Q8u = LD80C(0xf348d08a1ff53cb1,   -19,  3.62522053809474067060e-06L);
#define	P0	(P0u.e)
#define	P1	(P1u.e)
#define	P2	(P2u.e)
#define	P3	(P3u.e)
#define	P4	(P4u.e)
#define	P5	(P5u.e)
#define	P6	(P6u.e)
#define	P7	(P7u.e)
#define	P8	(P8u.e)
#define	Q1	(Q1u.e)
#define	Q2	(Q2u.e)
#define	Q3	(Q3u.e)
#define	Q4	(Q4u.e)
#define	Q5	(Q5u.e)
#define	Q6	(Q6u.e)
#define	Q7	(Q7u.e)
#define	Q8	(Q8u.e)

static struct Double
ratfun_gam(long double z, long double c)
{
	long double p, q, thi, tlo;
	struct Double r;

	q = 1  + z * (Q1 + z * (Q2 + z * (Q3 + z * (Q4 + z * (Q5 + 
	    z * (Q6 + z * (Q7 + z * Q8)))))));
	p = P0 + z * (P1 + z * (P2 + z * (P3 + z * (P4 + z * (P5 +
	    z * (P6 + z * (P7 + z * P8)))))));
	p = p / q;

	/* Split z into high and low parts. */
	thi = (float)z;
	tlo = (z - thi) + c;
	tlo *= (thi + z);

	/* Split (z+c)^2 into high and low parts. */
	thi *= thi;
	q = thi;
	thi = (float)thi;
	tlo += (q - thi);

	/* Split p/q into high and low parts. */
	r.a = (float)p;
	r.b = p - r.a;

	tlo = tlo * p + thi * r.b + a0_lo;
	thi *= r.a;				/* t = (z+c)^2*(P/Q) */
	r.a = (float)(thi + a0_hi);
	r.b = ((a0_hi - r.a) + thi) + tlo;
	return (r);				/* r = a0 + t */
}
/*
 * x < 6
 *
 * Use argument reduction G(x+1) = xG(x) to reach the range [1.066124,
 * 2.066124].  Use a rational approximation centered at the minimum
 * (x0+1) to ensure monotonicity.
 *
 * Good to < 1 ulp.  (provably .90 ulp; .87 ulp on 1,000,000 runs.)
 * It also has correct monotonicity.
 */
static const union IEEEl2bits
  xm1u = LD80C(0xec5b0c6ad7c7edc3, -2, 4.61632144968362341254e-01L);
#define	x0	(xm1u.e)

static const double
    left = -0.3955078125;	/* left boundary for rat. approx */

static long double
small_gam(long double x)
{
	long double t, y, ym1;
	struct Double yy, r;

	y = x - 1;

	if (y <= 1 + (left + x0)) {
		yy = ratfun_gam(y - x0, 0);
		return (yy.a + yy.b);
	}

	r.a = (float)y;
	yy.a = r.a - 1;
	y = y - 1 ;
	r.b = yy.b = y - yy.a;

	/* Argument reduction: G(x+1) = x*G(x) */
	for (ym1 = y - 1; ym1 > left + x0; y = ym1--, yy.a--) {
		t = r.a * yy.a;
		r.b = r.a * yy.b + y * r.b;
		r.a = (float)t;
		r.b += (t - r.a);
	}

	/* Return r*tgamma(y). */
	yy = ratfun_gam(y - x0, 0);
	y = r.b * (yy.a + yy.b) + r.a * yy.b;
	y += yy.a * r.a;
	return (y);
}
/*
 * Good on (0, 1+x0+left].  Accurate to 1 ulp.
 */
static long double
smaller_gam(long double x)
{
	long double d, rhi, rlo, t, xhi, xlo;
	struct Double r;

	if (x < x0 + left) {
		t = (float)x;
		d = (t + x) * (x - t);
		t *= t;
		xhi = (float)(t + x);
		xlo = x - xhi;
		xlo += t;
		xlo += d;
		t = 1 - x0;
		t += x;
		d = 1 - x0;
		d -= t;
		d += x;
		x = xhi + xlo;
	} else {
		xhi = (float)x;
		xlo = x - xhi;
		t = x - x0;
		d = - x0 - t;
		d += x;
	}

	r = ratfun_gam(t, d);
	d = (float)(r.a / x);
	r.a -= d * xhi;
	r.a -= d * xlo;
	r.a += r.b;

	return (d + r.a / x);
}
/*
 * x < 0
 *
 * Use reflection formula, G(x) = pi/(sin(pi*x)*x*G(x)).
 * At negative integers, return NaN and raise invalid.
 */
static const union IEEEl2bits
piu = LD80C(0xc90fdaa22168c235, 1, 3.14159265358979323851e+00L);
#define	pi	(piu.e)

static long double
neg_gam(long double x)
{
	int sgn = 1;
	struct Double lg, lsine;
	long double y, z;

	y = ceill(x);
	if (y == x)		/* Negative integer. */
		return ((x - x) / zero);

	z = y - x;
	if (z > 0.5)
		z = 1 - z;

	y = y / 2;
	if (y == ceill(y))
		sgn = -1;

	if (z < 0.25)
		z = sinpil(z);
	else
		z = cospil(0.5 - z);

	/* Special case: G(1-x) = Inf; G(x) may be nonzero. */
	if (x < -1753) {

		if (x < -1760)
			return (sgn * tiny * tiny);
		y = expl(lgammal(x) / 2);
		y *= y;
		return (sgn < 0 ? -y : y);
	}


	y = 1 - x;
	if (1 - y == x)
		y = tgammal(y);
	else		/* 1-x is inexact */
		y = - x * tgammal(-x);

	if (sgn < 0) y = -y;
	return (pi / (y * z));
}
/*
 * xmax comes from lgamma(xmax) - emax * log(2) = 0.
 * static const float  xmax = 35.040095f
 * static const double xmax = 171.624376956302725;
 * ld80: LD80C(0xdb718c066b352e20, 10, 1.75554834290446291689e+03L),
 * ld128: 1.75554834290446291700388921607020320e+03L,
 *
 * iota is a sloppy threshold to isolate x = 0.
 */
static const double xmax = 1755.54834290446291689;
static const double iota = 0x1p-116;

long double
tgammal(long double x)
{
	struct Double u;

	ENTERI();

	if (x >= 6) {
		if (x > xmax)
			RETURNI(x / zero);
		u = large_gam(x);
		RETURNI(__exp__D(u.a, u.b));
	}

	if (x >= 1 + left + x0)
		RETURNI(small_gam(x));

	if (x > iota)
		RETURNI(smaller_gam(x));

	if (x > -iota) {
		if (x != 0)
			u.a = 1 - tiny;	/* raise inexact */
		RETURNI(1 / x);
	}

	if (!isfinite(x))
		RETURNI(x - x);		/* x is NaN or -Inf */

	RETURNI(neg_gam(x));
}
