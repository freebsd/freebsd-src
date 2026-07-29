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
 *   atanpi(x) = atan(x) / pi				Eq. (1)
 *
 * Note, special cases:
 *
 *   atanpi(+-0) = +-0, exactly.
 *   atanpi(+-inf) = +-1/2, exactly.
 *   atanpi(nan) = nan
 *
 * Reflection symmetry atanpi(-|x|) = - atanpi(|x|) allows the
 * implementation to be defined for x >= 0.
 *
 * A rational approximation for atanpi(x) has the following form:
 *
 *                 x                 R(x^2)
 *   atanpi(x) = ---- + x * x^2 * ------------			Eq. (2)
 *                 pi              1 + S(x^2)
 *
 * with x^2 = x * x.  Define r(x^2) = x^2 * [R / (1 + S)], one then has
 *
 *   atanpi(x) = x * [1 / pi + r(x^2)]				Eq. (3)
 *
 * In addition, for some subdomains of x, the addition formula is used.
 *
 *    atanpi(x) = atanpi(v) + atanpi[(x - v) / (1 + v * x)]	Eq. (4)
 *
 * In the interval [0,0x1p{-N/2}) with N the precision of the floating
 * point type, Eq. (2) can be reduced to
 *
 *    atanpi(x) = x / pi.					Eq. (5)
 *
 * However, the division by pi (or more appropriately multiplication]
 * by the reciprocal) causes issues with |x| < 0x1p{emin+m} with 'm'
 * determined from testing.  The result of Eq. (5) approaches or is a
 * subnormal.  Here, x is scaled by 0x1p{N+1}, Eq. (4) is evaluated, and
 * then the result is scaled by 0x1p{-(N+1)}.
 *
 * In the interval [0xp{-N/2}, 0.5], Eq. (2) is evaluated where the
 * rational approximation has be found by a minimax procedure.
 *
 * In the interval [0.5,0.75), the addition formula gives
 *
 *   atanpi(x) = atanpi(x0) + atanpi[(x - x0)/(1 + x0 * x)]	Eq. (6)
 *
 * with x0 = 5/8 chosen at the center of the interval.
 *
 * In the interval [0.75,1), the addition formula gives
 *
 *   atanpi(x) = atanpi(x1) + atanpi[(x - x1)/(1 + x1 * x)]	Eq. (7)
 *
 * with x1 = 7/8 chosen at the center of the interval.
 *
 * In the interval [1,2), the addition formula gives
 *
 *   atanpi(x) = atanpi(x2) + atanpi[(x - x2)/(1 + x2 * x)]	Eq. (8)
 *
 * with x2 = 1.5 chosen at the center of the interval.
 *
 * Finally, in the interval [2,inf) the identity
 *
 *   atanpi(x) = 1/2 - atanpi(1 / x)				Eq. (9)
 */
#include <float.h>

#include "math.h"
#include "math_private.h"

#define _CC	(0x1p27 + 1)
#define _ROOT	sqrt

volatile static const double tiny = 1.e-300;
static const double half = 0.5, one = 1., qrtr = 0.25;
static const double x0 = 0.625, x1 = 0.875, x2 = 1.5;

/* Full precision high and low parts. */
static const double
invpihi =  3.1830988618379069e-01,	/* 1/pi */
invpilo = -1.9678676675182486e-17,	/* 1/pi */
a0hi =  1.7780768448935275e-01,		/* atanpi(x0) */
a0lo =  6.7223942595197191e-18,		/* atanpi(x0) */
a1hi =  2.2881069536505358e-01,		/* atanpi(x1) */
a1lo =  8.7193139538130510e-18,		/* atanpi(x1) */
a2hi =  3.1283295818900120e-01,		/* atanpi(x2) */
a2lo = -1.4076885713501453e-17;		/* atanpi(x2) */

/*
 *                     R(x^2)
 * __r(x^2) = x^2 * ------------
 *                   1 + S(x^2)
 *
 * Prior to the leading multiplication by x^2, the rational approximation
 * has an absolute minimax error less than 6.24e-19 over the [0x1p-40,0.5]
 * domain (or log2(error) = -63.8).
 */
static inline double
__r(double xs)
{
	static const double
	    R0 = -1.0610329539459690e-01,
	    R1 = -2.0683077993309035e-01,
	    R2 = -1.3099673469163398e-01,
	    R3 = -2.9655125284635996e-02,
	    R4 = -1.7208096636878276e-03,
	    S1 =  2.5493341763221311e+00,
	    S2 =  2.3356442152763948e+00,
	    S3 =  9.2164104384874268e-01,
	    S4 =  1.4526328350834117e-01,
	    S5 =  6.2132943401189099e-03;
	double r, s;
	r = R0 + (R1 + (R2 + (R3 + R4 * xs) * xs) * xs) * xs;
	s =  1 + (S1 + (S2 + (S3 + (S4 + S5 * xs) * xs) * xs) * xs) * xs;
	return (xs * (r / s));
}

double
atanpi(double x)
{
	double ax, hi, lo, xh, xl, y, zh, zl;
	uint32_t hx, ix, lx;

	EXTRACT_WORDS(hx, lx, x);
	ix = hx & 0x7fffffff;

	/* x = +-inf, nan */
	if (ix >= 0x7ff00000) {
		if (ix > 0x7ff00000)
			return (x + x);
		return ((hx & 0x80000000) ? -half : half);
	}

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
		if (ix < 0x3fe80000) {		/* |x| < 0.75 */
			x = (ax - x0) / (1 + x0 * ax);
			y = __r(x * x);
			_XADD(invpihi, invpilo, y, 0, xh, xl);
			_XMUL(x, 0, xh, xl, hi, lo);
			_XADD(a0hi, a0lo, hi, lo, y, xl);
		} else {
			x = (ax - x1) / (1 + x1 * ax);
			y = __r(x * x);
			_XADD(invpihi, invpilo, y, 0, xh, xl);
			_XMUL(x, 0, xh, xl, hi, lo);
			_XADD(a1hi, a1lo, hi, lo, y, xl);
		}
	} else if (ix < 0x40000000) {		/* |x| < 2 */
		if (ix == 0x3ff00000 && lx == 0)
			return ((hx & 0x80000000) ? -qrtr : qrtr);
		x = (ax - x2) / (1 + x2 * ax);
		y = __r(x * x);
		_XADD(invpihi, invpilo, y, 0, xh, xl);
		_XMUL(x, 0, xh, xl, hi, lo);
		_XADD(a2hi, a2lo, hi, lo, y, xl);
	} else {				/* |x| > 2 */
		x = 1 / ax;
		y = __r(x * x);
		_XADD(invpihi, invpilo, y, 0, xh, xl);
		_XMUL(x, 0, xh, xl, hi, lo);
		_XADD(half, 0, -hi, -lo, y, x);
	}

	return ((hx & 0x80000000) ? -y : y);
}

#if LDBL_MANT_DIG == 53
__weak_reference(atanpi, atanpil);
#endif
