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
 * src/s_asinpi.c for implemenation details.
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

volatile static const double tiny = 1.e-300;
static const double half = 0.5, one = 1.;

/* 1/pi split into the leading and trailing 53 bits. */
static const double
invpihi =  3.1830988618379069e-01,
invpilo = -1.9678676675182486e-17;

/*
 * Prior to the leading multiplication by x^2, the rational approximation
 * has an absolute minimax error less than 1.36e-22 over the [0x1p-32,0.5]
 * domain (or log2(error) = -72.6).
 */
static inline long double
__r(long double xs)
{
	static const union IEEEl2bits
	    R0u = LD80C(0xd94caf3dbdb01c38,  -5,  5.30516476972984452564e-02L),
	    R1u = LD80C(0x8d346599ebe3212a,  -3, -1.37895190734499743665e-01L),
	    R2u = LD80C(0x852c1751d7112655,  -3,  1.30051006670116064731e-01L),
	    R3u = LD80C(0xdb55697553742657,  -5, -5.35482520546937456640e-02L),
	    R4u = LD80C(0x931ba448fbd6e8c3,  -7,  8.97875827280130762861e-03L),
	    R5u = LD80C(0xdbbe8e80762881b0, -12, -4.19129108247665906395e-04L),
	    S1u = LD80C(0xc3272074944c6389,   1, -3.04926310906120631911e+00L),
	    S2u = LD80C(0xe390d58f48c1a96d,   1,  3.55571497910116337692e+00L),
	    S3u = LD80C(0xfccb6664d1fbfc39,   0, -1.97495727465499715865e+00L),
	    S4u = LD80C(0x86f4f140e3be416b,  -1,  5.27175024358933283746e-01L),
	    S5u = LD80C(0xf218070935962bdb,  -5, -5.91049456446391222176e-02L),
	    S6u = LD80C(0xec656d26215b624b, -10,  1.80355985054588957542e-03L);

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

#define	GREATER(a)	(ix == a && lx >  0x8000000000000000ull)
#define	LESSEQ(a)	(ix == a && lx <= 0x8000000000000000ull)

long double
asinpil(long double x)
{
	long double ax, hi, lo, xh, xl, y, zh, zl;
	uint64_t lx;
	uint16_t hx, ix;

	EXTRACT_LDBL80_WORDS(hx, lx, x);
	ix = hx & 0x7fff;

	if (ix >= 0x4000 || GREATER(0x3fff))	/* |x| > 1 */
		return ((x - x) / (x - x));

	ENTERI();

	INSERT_LDBL80_WORDS(ax, ix, lx);

	if (ix < 0x3ffe || LESSEQ(0x3ffe)) {	/* |x| <= 0.5 */
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
	} else if (ix < 0x3fff) {		/* |x| < 1 */
		y = 1 - ax;
		x = __r(y / 2);
		_XADD(invpihi, invpilo, x, 0, xh, xl);	/* 1 / pi + r(t^2) */
		_SQRT(2 * y, zh, zl);			/* 2 * t */
		_XMUL(xh, xl, zh, zl, hi, lo);
		_XADD(half, 0, -hi, -lo, y, x);
	} else					/* |x| == 1 */
		y = half;

	RETURNI((hx & 0x8000) ? -y : y);
}

/*
 * See src/s_asinpi.c for implementation details.
 */

long double
acospil(long double x)
{
	long double ax, hi, lo, xh, xl, y, zh, zl;
	uint64_t lx;
	uint16_t hx, ix;

	EXTRACT_LDBL80_WORDS(hx, lx, x);
	ix = hx & 0x7fff;

	if (ix >= 0x4000 || GREATER(0x3fff))	/* |x| > 1 */
		return ((x - x) / (x - x));

	ENTERI();

	if (ix < 0x3ffe || LESSEQ(0x3ffe)) {	/* |x| <= 0.5 */
		if (ix < 0x3fe9) {		/* |x| < 0x1p-22 */
			y = ((ix | lx) == 0) ? half : (LESSEQ(0x3fbf) ?
			    half - tiny : half - x * invpihi);
		} else {
			y = __r(x * x);
			_XADD(invpihi, invpilo, y, 0, xh, xl);
			_XMUL(x, 0, xh, xl, hi, lo);
			_XADD(half, 0, -hi, -lo, y, ax);
		}
	} else if (ix < 0x3fff) {		/* |x| < 1 */
		INSERT_LDBL80_WORDS(ax, ix, lx);
		y = 1 - ax;
		ax = __r(y / 2);
		_XADD(invpihi, invpilo, ax, 0, xh, xl);	/* 1 / pi + r(t^2) */
		_SQRT(2 * y, zh, zl);			/* 2 * t */
		_XMUL(xh, xl, zh, zl, hi, lo);
		if (hx & 0x8000)
			_XADD(one, 0, -hi, -lo, y, ax);
		else
			y = hi + lo;
	} else					/* |x| == 1 */
		y = half;

	RETURNI(y);
}
