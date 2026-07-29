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

/*
 * src/s_atanpi.c for implemenation details.
 */

#include "math.h"
#include "math_private.h"

#define _CC	(0x1p57L + 1)
#define _ROOT	sqrtl

volatile static const double tiny = 1.e-300;
static const double half = 0.5, one = 1., qrtr = 0.25;
static const double x0 = 0.625, x1 = 0.875, x2 = 1.5;

/* Full precision high and low parts. */
static const long double
invpihi =  3.18309886183790671537767526745028737e-01L,	/* 1/pi */
invpilo = -1.28821588763206006125693864783127482e-35L,	/* 1/pi */
a0hi =  1.77807684489352753115503587502248118e-01L,	/* atanpi(x0) */
a0lo = -1.04565241713355884231841538298149212e-35L,	/* atanpi(x0) */
a1hi =  2.28810695365053587806047702039884546e-01L,	/* atanpi(x1) */
a1lo = -5.01451892949247960667313017509908719e-37L,	/* atanpi(x1) */
a2hi =  3.12832958189001183813747252435221446e-01L,	/* atanpi(x2) */
a2lo =  1.35071436051692259858806390941906106e-36L;	/* atanpi(x2) */

/*
 * Prior to the leading multiplication by x^2, the rational approximation
 * has an absolute minimax error less than 2.86e-38 over the [0x1p-56,0.5]
 * domain (or log2(error) = -124.7).
 */
static inline long double
__r(long double xs)
{
	static const long double
	    R0 =  5.30516476972984452562945877908381207e-02L,
	    R1 = -2.60404681812983888677486288898051068e-01L,
	    R2 =  5.43274368732204127849880302812989126e-01L,
	    R3 = -6.27476039646895838848725721148947475e-01L,
	    R4 =  4.37806223811161460670580937913479055e-01L,
	    R5 = -1.88854683672201011042310131486279147e-01L,
	    R6 =  4.94528645546233985859708428238164776e-02L,
	    R7 = -7.38228286468784921885881430313681146e-03L,
	    R8 =  5.47609957562826794808966842094709768e-04L,
	    R9 = -1.44837427671490633843418618030301533e-05L,
	    R10=  1.60087061374239702655150492329207605e-08L,
	    S1 = -5.35851261206434695164324875722635942e+00L,
	    S2 =  1.23839541267281630422432341920191869e+01L,
	    S3 = -1.61473998442126533731585929134597693e+01L,
	    S4 =  1.30442314991246752613457576698550221e+01L,
	    S5 = -6.74685395302389922545194846871599697e+00L,
	    S6 =  2.22958089278066716953022085768628547e+00L,
	    S7 = -4.55313732093612723575589438727154778e-01L,
	    S8 =  5.33391487207251214709627478847204628e-02L,
	    S9 = -3.08343599417750507732125775680767303e-03L,
	    S10=  6.12260957946655623349049151336307244e-05L;

	long double r, s;
	r = R5 + (R6 + (R7 + (R8 + (R9 + R10 * xs) * xs) *xs) * xs) * xs;
	r = R0 + (R1 + (R2 + (R3 + (R4 + r * xs) * xs) * xs) * xs) * xs;
	s = S5 + (S6 + (S7 + (S8 + (S9 + S10 * xs) * xs) *xs) * xs) * xs;
	s =  1 + (S1 + (S2 + (S3 + (S4 + r * xs) * xs) * xs) * xs) * xs;
	return (xs * (r / s));
}

long double
atanpil(long double x)
{
	long double ax, hi, lo, xh, xl, y, zh, zl;

	if (isnan(x) || isinf(x))
		return ((x - x) / (x - x));

	ax = fabsl(x);

	if (ax > 1)				/* |x| > 1 */
		return ((x - x) / (x - x));


	if (ax <= 0.5) {			/* |x| <= 0.5 */
		if (ax < 0x1p-57L) {		/* |x| < 0x1p-57 */
			if (ax < 0x1p-16340L) {	/* |x| < 0x1p-16340 */
				if (ax == 0)
					return (x);
				/* Scale for near subnormal. */
				ax *= 0x1p114;
				_XMUL(ax, 0, invpihi, invpilo, hi, lo);
				y = (hi + lo) * 0x1p-114;
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
	} else if (ax < 1) {		/* |x| < 1 */
		if (ax < 0.75) {	/* |x| < 0.75 */
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
	} else if (ax < 2) {		/* |x| < 2 */
		if (ax == 1)
			return (x < 0 ? -qrtr : qrtr);
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

	return (x < 0 ? -y : y);
}
