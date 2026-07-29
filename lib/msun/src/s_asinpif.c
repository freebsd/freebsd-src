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
 * src/s_asinpi.c for implementation details.
 */

#include "math.h"
#include "math_private.h"

#define _CC	(0x1p12F + 1)
#define _ROOT	sqrtf

volatile static const float tiny = 1.e-30;
static const float half = 0.5f, one = 1.f;

/* Full precision high and low parts of 1 / pi. */
static const float
invpihi = 3.18309873e-01f,
invpilo = 1.28412765e-08f;

/*
 * Prior to the leading multiplication by x^2, the rational approximation
 * has an absolute minimax error less than 2.8e-10 over the [0x1p-12,0.5]
 * domain (or log2(error) = -31.7).
 */
static inline float
__r(float xs)
{
	static const float
	    R0 =  5.30516468e-02f,
	    R1 = -3.80413085e-02f,
	    R2 =  1.74116367e-03f,
	    S1 = -1.16706085e+00f,
	    S2 =  2.90115148e-01f;
	float r, s;
	r = R0 + (R1 + R2 * xs) * xs;
	s =  1 + (S1 + S2 * xs) * xs;
	return (xs * (r / s));
}

float
asinpif(float x)
{
	float ax, hi, lo, xh, xl, y, zh, zl;
	uint32_t hx, ix;

	GET_FLOAT_WORD(hx, x);
	ix = hx & 0x7fffffff;

	/* |x| > 1 */
	if (ix > 0x3f800000)
		return ((x - x) / (x - x));

	SET_FLOAT_WORD(ax, ix);

	if (ix <= 0x3f000000) {			/* |x| <= 0.5 */
		if (ix < 0x39800000) {		/* |x| < 0x1p-12 */
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


/*
 * See src/s_asinpi.c for implementation details.
 */

float
acospif(float x)
{
	float ax, hi, lo, xh, xl, y, zh, zl;
	uint32_t hx, ix;

	GET_FLOAT_WORD(hx, x);
	ix = hx & 0x7fffffff;

	/* |x| > 1 */
	if (ix > 0x3f800000)
		return ((x - x) / (x - x));

	if (ix <= 0x3f000000) {			/* |x| <= 0.5 */
		if (ix <= 0x3a800000) {		/* |x| <= 0x1p-10 */
			y = (ix == 0) ? half : ((ix < 0x33800000) ?
			    half - tiny : half - x * invpihi);
		} else {
			y = __r(x * x);
			_XADD(invpihi, invpilo, y, 0, xh, xl);
			_XMUL(x, 0, xh, xl, hi, lo);
			_XADD(half, 0, -hi, -lo, y, ax);
		}
	} else if (ix < 0x3f800000) {		/* |x| < 1 */
		SET_FLOAT_WORD(ax, ix);
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
