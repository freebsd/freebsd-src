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

#ifdef __i386__
#include <ieeefp.h>
#endif
#include <stdint.h>

#include "fpmath.h"
#include "math.h"
#include "math_private.h"

#define _CC	(0x1p32L + 1)
#define _ROOT	sqrtl
#define NBIT	(0x8000000000000000ull)

volatile static const double tiny = 1.e-300;
static const double half = 0.5, one = 1., qrtr = 0.25;
static const double x0 = 0.625, x1 = 0.875, x2 = 1.5;

/* 53-bit high and low parts. */
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
 * Prior to the leading multiplication by x^2, the rational approximation
 * has an absolute minimax error less than 1.22e-23 over the [0x1p-32,0.5]
 * domain (or log2(error) = -76.1).
 */
static inline long double
__r(long double xs)
{
	static const union IEEEl2bits
	    R0u = LD80C(0xd94caf3dbdb01c38,  -4, -1.06103295394596890513e-01L),
	    R1u = LD80C(0x845b6d12f35f0ccb,  -2, -2.58510025561632250608e-01L),
	    R2u = LD80C(0xe679d75bf98585ae,  -3, -2.25074162472429082081e-01L),
	    R3u = LD80C(0xaba63bb6f14e17e4,  -4, -8.38131585316324449243e-02L),
	    R4u = LD80C(0xca768e4a7d2fd52e,  -7, -1.23573674736290235872e-02L),
	    R5u = LD80C(0x802e09b751d4c6bc, -11, -4.88967268965928487608e-04L),
	    S1u = LD80C(0xc2545ef3d335e598,   1,  3.03639959155120062705e+00L),
	    S2u = LD80C(0xe0ee42f67b6b24bc,   1,  3.51454233236806805243e+00L),
	    S3u = LD80C(0xf7200820c8d271d1,   0,  1.93066503144077077959e+00L),
	    S4u = LD80C(0x820ec69663c6f1f3,  -1,  5.08037959781883845928e-01L),
	    S5u = LD80C(0xe61a8fde47e2f517,  -5,  5.61776752333507006525e-02L),
	    S6u = LD80C(0xe260573083b4d35b, -10,  1.72711433720654801154e-03L);

#define	R0	(R0u.e)
#define	R1	(R1u.e)
#define	R2	(R2u.e)
#define	R3	(R3u.e)
#define	R4	(R4u.e)
#define	R5	(R5u.e)
#define	S1	(S1u.e)
#define	S2	(S2u.e)
#define	S3	(S3u.e)
#define	S4	(S4u.e)
#define	S5	(S5u.e)
#define	S6	(S6u.e)

	long double r, s;
	r = R0 + (R1 + (R2 + (R3 + (R4 + R5 * xs) * xs) * xs) * xs) * xs;
	s =  1 + (S1 + (S2 + (S3 + (S4 + (S5 + S6 * xs) * xs) * xs) *
	    xs) * xs) * xs;
	return (xs * (r / s));
}

long double
atanpil(long double x)
{
	long double ax, hi, lo, xh, xl, y, zh, zl;
	uint64_t lx;
	uint16_t hx, ix;

	EXTRACT_LDBL80_WORDS(hx, lx, x);
	ix = hx & 0x7fff;

	/* x = +-inf, nan */
	if (ix >= 0x7fff && lx >= 0x8000000000000000ull) {
		if (lx > 0x8000000000000000ull)
			return (x + x);
		return ((hx & 0x8000) ? -half : half);
	}

	ENTERI();

	INSERT_LDBL80_WORDS(ax, ix, lx);

	if (ix < 0x3ffe ) { /* |x| < 0.5 */
		if (ix < 0x3fde) {		/* |x| < 0x1p-33 */
			if (ix < 0x002b) {	/* |x| < 0x1p-16340 */
				if ((ix | lx) == 0)
					RETURNI(x);
				/* Scale for near subnormal. */
				ax *= 0x1p65;
				_XMUL(ax, 0, invpihi, invpilo, hi, lo);
				y = (hi + lo) * 0x1p-65;
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
	} else if (ax < 1) {	/* |x| < 1 */
		/* |x| < 0.75 */
		if (ix == 0x3ffe && lx < 0xc000000000000000ull) {
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
	} else if (ix < 0x4000) {		/* |x| < 2 */
		if (ix == 0x3fff && lx == NBIT)
			return ((hx & 0x8000) ? -qrtr : qrtr);
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

	RETURNI((hx & 0x8000) ? -y : y);
}
