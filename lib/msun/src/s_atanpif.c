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
 * src/s_atanpi.c for implementation details.
 */

#include "math.h"
#include "math_private.h"

#define _CC	(0x1p12F + 1)
#define _ROOT	sqrtf

volatile static const float tiny = 1.e-30;
static const float half = 0.5f, one = 1.f, qrtr = 0.25f;
static const float x0 = 0.625, x1 = 0.875, x2 = 1.5;

/* Full precision high and low parts. */
static const float
invpihi = 3.18309873e-01f,	/* 1 / pi */
invpilo = 1.28412765e-08f,	/* 1 / pi */
a0hi =  1.77807689e-01f,	/* atanpi(x0) */
a0lo = -4.22372093e-09f,	/* atanpi(x0) */
a1hi =  2.28810698e-01f,	/* atanpi(x1) */
a1lo = -2.42890708e-09f,	/* atanpi(x1) */
a2hi =  3.12832952e-01f,	/* atanpi(x2) */
a2lo =  6.64328592e-09f;	/* atanpi(x2) */

/*
 * Prior to the leading multiplication by x^2, the rational approximation
 * has an absolute minimax error less than 1.59e-10 over the [0x1p-12,0.5]
 * domain (or log2(error) = -32.5).
 */
static inline float
__r(float xs)
{
	static const float
	    R0 = -1.06103294e-01f,
	    R1 = -6.81197494e-02f,
	    R2 = -1.61480496e-03f,
	    S1 =  1.24201322e+00f,
	    S2 =  3.31868112e-01f;
	float r, s;
	r = R0 + (R1 + R2 * xs) * xs;
	s =  1 + (S1 + S2 * xs) * xs;
	return (xs * (r / s));
}

float
atanpif(float x)
{
	float ax, hi, lo, xh, xl, y, zh, zl;
	uint32_t hx, ix;

	GET_FLOAT_WORD(hx, x);
	ix = hx & 0x7fffffff;

	/* x = +-inf, nan */
	if (ix >= 0x7f800000) {
		if (ix > 0x7f800000)
			return (x + x);
		return ((hx & 0x80000000) ? -half : half);
	}

	SET_FLOAT_WORD(ax, ix);

	if (ix <= 0x3f000000) {			/* |x| <= 0.5 */
		if (ix < 0x39000000) {		/* |x| < 0x1p-13 */
			if (ix < 0x03800000) {	/* |x| < 0x1p-120 */
				if (ix == 0)
					return (x);
				/* Scale for near subnormal. */
				ax *= 0x1p25f;
				_XMUL(ax, 0, invpihi, invpilo, hi, lo);
				y = (hi + lo) * 0x1p-25f;
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
	} else if (ix < 0x3f800000) {		/* |x| < 1 */
		if (ix < 0x3f400000) {		/* |x| < 0.75 */
			x = (ax - x0) / (1 + x0 * ax);
			y = __r(x * x);
			_XADD(invpihi, invpilo, y, 0, xh, xl);
			_XMUL(x, 0, xh, xl, hi, lo);
			_XADD(a0hi, a0lo, hi, lo, y, xl);
		} else {		/* |x| < 1 */
			x = (ax - x1) / (1 + x1 * ax);
			y = __r(x * x);
			_XADD(invpihi, invpilo, y, 0, xh, xl);
			_XMUL(x, 0, xh, xl, hi, lo);
			_XADD(a1hi, a1lo, hi, lo, y, xl);
		}
	} else if (ix < 0x40000000) {		/* |x| < 2 */
		if (ix == 0x3f800000)
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
