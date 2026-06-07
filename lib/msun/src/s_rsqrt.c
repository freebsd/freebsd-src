/*-
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
 * Compute the inverse sqrt of x, i.e., rsqrt(x) = 1 / sqrt(x).
 *
 * First, filter out special cases:
 *
 *   1. rsqrt(+-0) = +-inf, and raise FE_DIVBYZERO exception.
 *   2. rsqrt(nan) = NaN.
 *   3. rsqrt(+inf) returns +0.
 *   2. rsqrt(x<0) = NaN, and raises FE_INVALID.
 *
 * If x is a subnormal, scale x into the normal range by x*0x1pN; while
 * recording the exponent of the scale factor N.  Split the possibly
 * scaled x into f*2^n with f in [0.5,1).  Set m=n or m=n-N (subnormal).
 * If n is odd, then set f = f/2 and increase n to n+1.  Thus, f is
 * in [0.25,1) with n even.
 *
 * An initial estimate of y = rqrt[f](x) is 1 / sqrt[f](x).  Exhaustive
 * testing of rsqrtf() gave a max ULP of 1.49; while testing 500M x in
 * [0,1000] gave a max ULP of 1.24 for rsqrt().  The value of y is then
 * used with one iteration of Goldschmidt's algorithm:
 *
 *	z = x * y
 *	h = y / 2
 *	r = 0.5 - h * z
 *	y = h * r + h
 *
 * A factor of 2 appears missing in the above, but it is included in the
 * exponent m.
 */
#include <fenv.h>
#include <float.h>
#include "math.h"
#include "math_private.h"

#pragma STDC FENV_ACCESS ON

#ifdef _CC
#undef _CC
#endif
#define _CC (0x1p27 + 1)

double
rsqrt(double x)
{
	volatile static const double vzero = 0;
	static const double half = 0.5;
	int hx, m, rnd;
	uint32_t lx, ux;
	double h, ph, pl, rh, rl, y, zh, zl;

	EXTRACT_WORDS(hx, lx, x);
	ux = (uint32_t)hx & 0x7fffffff;

	/* x = +-0.  Raise exception. */
	if ((ux | lx) == 0)
	    return (1 / x);

	/* x is NaN. */
	if (ux > 0x7ff00000)
	    return (x + x);

	/* x is +-inf. */
	if (ux == 0x7ff00000)
	    return (hx & 0x80000000 ? vzero / vzero : 0.);

	/* x < 0.  Raise exception. */
	if (hx < 0)
	    return (vzero / vzero);

	/*
	 * If x is subnormal, then scale it into the normal range.
	 * Split x into significand and exponent, x = f * 2^m, with
	 * f in [0.5,1) and m a biased exponent.
	 */
	m = 0;
	if (hx < 0x00100000) {		/* Subnormal */
	    x *= 0x1p54;
	    GET_HIGH_WORD(hx, x);
	    m = -54;
	}
	m += (hx >> 20) - 1022;
	hx = (hx & 0x000fffff) | 0x3fe00000;
	SET_HIGH_WORD(x, hx);

	/* m is odd.  Put x into [0.25,0.5) and increase m. */
	if (m & 1) {
	    x /= 2;
	    m += 1;
	}
	m = -(m >> 1);			/* Prepare for 2^(-m/2). */

	y = 1 / sqrt(x);		/* ~52-bit estimate. */

	h = y / 2;

	/*
	 * For values of x with a representation of 0x1.ffffffffffffepN
	 * with N an odd integer, the computed rsqrt() is not correctly
	 * rounded in round-to-nearest without toggling the rounding mode
	 * to FE_TOWARDZERO.  Note, FE_DOWNWARD also works.  However,
	 * messing with the rounding mode is expensive, so only do it
	 * when necessary. Example, x = 3.9999999999999991
	 * gives y --> hx = 0x3ff00000, lx = 0x00000001
	 */
	EXTRACT_WORDS(hx, lx, y);
	if ((hx & 0x000fffff) == 0 && lx == 1) {
	    rnd = fegetround();
	    fesetround(FE_TOWARDZERO);
	    _MUL(x, y, zh, zl);
	    _XMUL(zh, zl, h, 0, ph, pl);
	    fesetround(rnd);
	} else {
	    _MUL(x, y, zh, zl);
	    _XMUL(zh, zl, h, 0, ph, pl);
	}

	_XADD(-ph, -pl, half, 0, rh, rl);
	y = rh * h + h;

	ux = (m + 1024) << 20;
	INSERT_WORDS(x, ux, 0);
	return (y *= x);
}

#if LDBL_MANT_DIG == 53
__weak_reference(rsqrt, rsqrtl);
#endif
