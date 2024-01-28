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
 * The original code, FreeBSD's old svn r93211, contained the following
 * attribution:
 *
 *    This code by P. McIlroy, Oct 1992;
 *
 *    The financial support of UUNET Communications Services is greatfully
 *    acknowledged.
 *
 *  The algorithm remains, but the code has been re-arranged to facilitate
 *  porting to other precisions.
 */

#include <float.h>

#include "math.h"
#include "math_private.h"

/* Used in b_log.c and below. */
struct Double {
	double a;
	double b;
};

#include "b_log.c"
#include "b_exp.c"

/*
 * The range is broken into several subranges.  Each is handled by its
 * helper functions.
 *
 *         x >=   6.0: large_gam(x)
 *   6.0 > x >= xleft: small_gam(x) where xleft = 1 + left + x0.
 * xleft > x >   iota: smaller_gam(x) where iota = 1e-17.
 *  iota > x >  -itoa: Handle x near 0.
 * -iota > x         : neg_gam
 *
 * Special values:
 *	-Inf:			return NaN and raise invalid;
 *	negative integer:	return NaN and raise invalid;
 *	other x ~< 177.79:	return +-0 and raise underflow;
 *	+-0:			return +-Inf and raise divide-by-zero;
 *	finite x ~> 171.63:	return +Inf and raise overflow;
 *	+Inf:			return +Inf;
 *	NaN: 			return NaN.
 *
 * Accuracy: tgamma(x) is accurate to within
 *	x > 0:  error provably < 0.9ulp.
 *	Maximum observed in 1,000,000 trials was .87ulp.
 *	x < 0:
 *	Maximum observed error < 4ulp in 1,000,000 trials.
 */

/*
 * Constants for large x approximation (x in [6, Inf])
 * (Accurate to 2.8*10^-19 absolute)
 */

static const double zero = 0.;
static const volatile double tiny = 1e-300;
/*
 * x >= 6
 *
 * Use the asymptotic approximation (Stirling's formula) adjusted fof
 * equal-ripples:
 *
 * log(G(x)) ~= (x-0.5)*(log(x)-1) + 0.5(log(2*pi)-1) + 1/x*P(1/(x*x))
 *
 * Keep extra precision in multiplying (x-.5)(log(x)-1), to avoid
 * premature round-off.
 *
 * Accurate to max(ulp(1/128) absolute, 2^-66 relative) error.
 */
static const double
    ln2pi_hi =  0.41894531250000000,
    ln2pi_lo = -6.7792953272582197e-6,
    Pa0 =  8.3333333333333329e-02, /* 0x3fb55555, 0x55555555 */
    Pa1 = -2.7777777777735404e-03, /* 0xbf66c16c, 0x16c145ec */
    Pa2 =  7.9365079044114095e-04, /* 0x3f4a01a0, 0x183de82d */
    Pa3 = -5.9523715464225254e-04, /* 0xbf438136, 0x0e681f62 */
    Pa4 =  8.4161391899445698e-04, /* 0x3f4b93f8, 0x21042a13 */
    Pa5 = -1.9065246069191080e-03, /* 0xbf5f3c8b, 0x357cb64e */
    Pa6 =  5.9047708485785158e-03, /* 0x3f782f99, 0xdaf5d65f */
    Pa7 = -1.6484018705183290e-02; /* 0xbf90e12f, 0xc4fb4df0 */

static struct Double
large_gam(double x)
{
	double p, z, thi, tlo, xhi, xlo;
	struct Double u;

	z = 1 / (x * x);
	p = Pa0 + z * (Pa1 + z * (Pa2 + z * (Pa3 + z * (Pa4 + z * (Pa5 +
	    z * (Pa6 + z * Pa7))))));
	p = p / x;

	u = __log__D(x);
	u.a -= 1;

	/* Split (x - 0.5) in high and low parts. */
	x -= 0.5;
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
static const double
#if 0
    a0_hi =  8.8560319441088875e-1,
    a0_lo = -4.9964270364690197e-17,
#else
    a0_hi =  8.8560319441088875e-01, /* 0x3fec56dc, 0x82a74aef */
    a0_lo = -4.9642368725563397e-17, /* 0xbc8c9deb, 0xaa64afc3 */
#endif
    P0 =  6.2138957182182086e-1,
    P1 =  2.6575719865153347e-1,
    P2 =  5.5385944642991746e-3,
    P3 =  1.3845669830409657e-3,
    P4 =  2.4065995003271137e-3,
    Q0 =  1.4501953125000000e+0,
    Q1 =  1.0625852194801617e+0,
    Q2 = -2.0747456194385994e-1,
    Q3 = -1.4673413178200542e-1,
    Q4 =  3.0787817615617552e-2,
    Q5 =  5.1244934798066622e-3,
    Q6 = -1.7601274143166700e-3,
    Q7 =  9.3502102357378894e-5,
    Q8 =  6.1327550747244396e-6;

static struct Double
ratfun_gam(double z, double c)
{
	double p, q, thi, tlo;
	struct Double r;

	q = Q0 + z * (Q1 + z * (Q2 + z * (Q3 + z * (Q4 + z * (Q5 + 
	    z * (Q6 + z * (Q7 + z * Q8)))))));
	p = P0 + z * (P1 + z * (P2 + z * (P3 + z * P4)));
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
static const double
    left = -0.3955078125,	/* left boundary for rat. approx */
    x0 = 4.6163214496836236e-1;	/* xmin - 1 */

static double
small_gam(double x)
{
	double t, y, ym1;
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
static double
smaller_gam(double x)
{
	double d, rhi, rlo, t, xhi, xlo;
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
static double
neg_gam(double x)
{
	int sgn = 1;
	struct Double lg, lsine;
	double y, z;

	y = ceil(x);
	if (y == x)		/* Negative integer. */
		return ((x - x) / zero);

	z = y - x;
	if (z > 0.5)
		z = 1 - z;

	y = y / 2;
	if (y == ceil(y))
		sgn = -1;

	if (z < 0.25)
		z = sinpi(z);
	else
		z = cospi(0.5 - z);

	/* Special case: G(1-x) = Inf; G(x) may be nonzero. */
	if (x < -170) {

		if (x < -190)
			return (sgn * tiny * tiny);

		y = 1 - x;			/* exact: 128 < |x| < 255 */
		lg = large_gam(y);
		lsine = __log__D(M_PI / z);	/* = TRUNC(log(u)) + small */
		lg.a -= lsine.a;		/* exact (opposite signs) */
		lg.b -= lsine.b;
		y = -(lg.a + lg.b);
		z = (y + lg.a) + lg.b;
		y = __exp__D(y, z);
		if (sgn < 0) y = -y;
		return (y);
	}

	y = 1 - x;
	if (1 - y == x)
		y = tgamma(y);
	else		/* 1-x is inexact */
		y = - x * tgamma(-x);

	if (sgn < 0) y = -y;
	return (M_PI / (y * z));
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
static const double xmax = 171.624376956302725;
static const double iota = 0x1p-56;

double
tgamma(double x)
{
	struct Double u;

	if (x >= 6) {
		if (x > xmax)
			return (x / zero);
		u = large_gam(x);
		return (__exp__D(u.a, u.b));
	}

	if (x >= 1 + left + x0)
		return (small_gam(x));

	if (x > iota)
		return (smaller_gam(x));

	if (x > -iota) {
		if (x != 0.)
			u.a = 1 - tiny;	/* raise inexact */
		return (1 / x);
	}

	if (!isfinite(x))
		return (x - x);		/* x is NaN or -Inf */

	return (neg_gam(x));
}

#if (LDBL_MANT_DIG == 53)
__weak_reference(tgamma, tgammal);
#endif
