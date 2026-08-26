/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Steven G. Kargl
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 *   asinpi(x) = asin(x) / pi				Eq. (1)
 *
 * The rational approximation for asinpi(x) has the following form:
 *
 *                 x                 R(x^2)
 *   asinpi(x) = ---- + x * x^2 * ------------		Eq. (2)
 *                 pi              1 + S(x^2)
 *
 * with x^2 = x * x.  Define r(x^2) = x^2 * [R / (1 + S)], one then has
 *
 *   asinpi(x) = x * [1 / pi + r(x^2)]			Eq. (3)
 *
 * For |x| << 1, asinpi(x) = x / pi.  That is, the 2nd term in the righthand
 * side of Eq. (2) can be neglected for |x| < 0x1p{-N/2} where {N/2} is half
 * the precision (e.g., N = 53, {N/2} = 26).  It is noted that for
 * |x| < 0x1p{emin+m} with m chosen through testing, x / pi is approaching
 * or is subnormal.  To compute the result, x is scaled by 0x1p{N+1}, x / pi
 * is computed, and finally rescaled by 0x1p{-(N+1)}.
 *
 * In the domain, 0x1p{-N/2} <= |x| < 0.5, the approximation becomes
 *
 *   asinpi(x) = x * (lo + r(x^2) + hi)
 *
 * where lo and hi are full-precision low and high parts of 1 / pi.
 *
 * In the interval [0.5,1), the following relationship
 *
 *   asin(x) = pi / 2 - 2 * asin(t)
 *
 * with t = [(1 - x) / 2]^{1/2} is used to rewrite Eq. (1).  Thus,
 *
 *    asinpi(x) = 1 / 2 - 2 * t * [1 / pi + r(t^2)]	Eq. (4)
 *
 * Note, special cases:
 *
 *   asinpi(+-0) = +-0, exactly.
 *   asinpi(+-1) = +-1/2, exactly.
 *   asinpi(x) = nan for |x| > 1
 *   asinpi(nan) = nan
 */

#include <float.h>

#include "math.h"
#include "math_private.h"

#define _CC	(0x1p27 + 1)
#define _ROOT	sqrt

volatile static const double tiny = 1.e-300;
static const double half = 0.5, one = 1.;

/* Full precision high and low parts of 1 / pi. */
static const double
invpihi =  3.1830988618379069e-01,
invpilo = -1.9678676675182486e-17;

/*
 *                     R(x^2)
 * __r(x^2) = x^2 * ------------
 *                   1 + S(x^2)
 *
 * Prior to the leading multiplication by x^2, the rational approximation
 * has an absolute minimax error less than 8.57e-21 over the [0x1p-40,0.5]
 * domain (or log2(error) = -66.7).
 */
static inline double
__r(double xs)
{
	static const double
	    R0 =  5.3051647697298449e-02,
	    R1 = -1.2219903601836109e-01,
	    R2 =  9.7236612309627199e-02,
	    R3 = -3.0778625727037261e-02,
	    R4 =  3.1527637063244254e-03,
	    R5 = -1.9159514282614908e-05,
	    S1 = -2.7533975629862262e+00,
	    S2 =  2.8040387218379421e+00,
	    S3 = -1.2867553139513013e+00,
	    S4 =  2.5507476275412666e-01,
	    S5 = -1.6150977787265989e-02;
	double r, s;
	r = R0 + (R1 + (R2 + (R3 + (R4 + R5 * xs) * xs) * xs) * xs) * xs;
	s =  1 + (S1 + (S2 + (S3 + (S4 + S5 * xs) * xs) * xs) * xs) * xs;
	return (xs * (r / s));
}

#include <stdio.h>
double
asinpi(double x)
{
	double ax, hi, lo, xh, xl, y, zh, zl;
	uint32_t hx, ix, lx;

	EXTRACT_WORDS(hx, lx, x);
	ix = hx & 0x7fffffff;

	if (ix > 0x3ff00000)			/* |x| > 1 */
		return ((x - x) / (x - x));

	INSERT_WORDS(ax, ix, lx);

	if (ix <= 0x3fe00000) {			/* |x| <= 0.5 */
		if (ix < 0x3e400000) {		/* |x| < 0x1p-27 */
			if (ix < 0x00800000) {	/* |x| < 0x1p-1015 */
				if ((ix | lx) == 0)
					return (x);
				/* Scale for near subnormal. */
				ax *= 0x1p54;
				_XMUL(ax, 0, invpihi, invpilo, hi, lo);
				y = (hi + lo) * 0x1p-54;
			} else {
				_XMUL(ax, 0, invpihi, invpilo, hi, lo);
				y = hi + lo;
			}
		} else {
			y = __r(ax * ax);
			_XADD(invpihi, invpilo, y, 0, xh, xl);
			_XMUL(ax, 0, xh, xl, hi, lo);
			y = hi + lo;
		}
	} else if (ix < 0x3ff00000) {		/* |x| < 1 */
		y = 1 - ax;
		x = __r(y / 2);
		_XADD(invpihi, invpilo, x, 0, xh, xl);
		_SQRT(2 * y, zh, zl);
		_XMUL(xh, xl, zh, zl, hi, lo);
		_XADD(half, 0, -hi, -lo, y, x);
	} else					/* |x| == 1 */
		y = half;

	return ((hx & 0x80000000) ? -y : y);
}

#if LDBL_MANT_DIG == 53
__weak_reference(asinpi, asinpil);
#endif

/*
 * acospi(x) = acos(x) / pi
 *
 * The implementation uses two identities:
 *
 *   acos(x) = pi / 2 - asin(x)				Eq. (5)
 *   acos(-|x|) = pi - acos(|x|)			Eq. (6)
 *
 * and the definitions for asinpi(x) above.  Conversion of Eq. (5)
 * with the aid of Eq. (3) leads to the form:
 *
 *   acospi(x) = 1 / 2 - x * [1 / pi + r(x^2)]		Eq. (7)
 *
 * where 0 <= |x| < 0.5.  There are two thresholds.  For |x| < 0x1p{-N}
 * acospi(x) = 1/2 - tiny, which raises FE_INEXACT while preventing spurious
 * underflow.  For |x| < 0x1p{-M}, acospi(x) = 1/2 - x / pi where M is
 * a sloppy threshold determined from testing.  For 0.5 <= |x| < 1, there
 * are two approximations:
 *
 *    acospi(x) = 2 * t * [1 / pi + r(t^2)]		Eq. (8)
 *
 * for 0.5 <= x < 1.  When -1 < x <= -0.5, the relevant expression is
 *
 *   acospi(x) = 1 - 2 * t * [1 / pi + r(t^2)]		Eq. (9)
 *
 * Note, special cases:
 *
 *    acospi(+-0) = 1/2, exactly
 *    acospi(1) = 0, exactly
 *    acospi(-1) = 1, exactly
 *   asinpi(x) = nan for |x| > 1
 *   asinpi(nan) = nan
 */

double
acospi(double x)
{
	double ax, hi, lo, xh, xl, y, zh, zl;
	uint32_t hx, ix, lx;

	EXTRACT_WORDS(hx, lx, x);
	ix = hx & 0x7fffffff;

	if (ix > 0x3ff00000)			/* |x| > 1 */
		return ((x - x) / (x - x));

	if (ix <= 0x3fe00000) {			/* |x| <= 0.5 */
		if (ix < 0x3eb00000) {		/* |x| < 0x1p-20 */
			y = ((ix | lx) == 0) ? half : ((ix < 0x3ca00000) ?
			    half - tiny : half - x * invpihi);
		} else {
			y = __r(x * x);
			_XADD(invpihi, invpilo, y, 0, xh, xl);
			_XMUL(x, 0, xh, xl, hi, lo);
			_XADD(half, 0, -hi, -lo, y, ax);
		}
	} else if (ix < 0x3ff00000) {		/* |x| < 1 */
		INSERT_WORDS(ax, ix, lx);
		y = 1 - ax;
		ax = __r(y / 2);
		_XADD(invpihi, invpilo, ax, 0, xh, xl);
		_SQRT(2 * y, zh, zl);
		_XMUL(xh, xl, zh, zl, hi, lo);
		if (hx & 0x80000000)
			_XADD(one, 0, -hi, -lo, y, ax);
		else
			y = hi + lo;
	} else					/* |x| == 1 */
		y = hx & 0x80000000 ? 1 : 0;

	return (y);
}

#if LDBL_MANT_DIG == 53
__weak_reference(acospi, acospil);
#endif
