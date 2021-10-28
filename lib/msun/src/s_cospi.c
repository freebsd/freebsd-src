/*-
 * Copyright (c) 2017 Steven G. Kargl
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
 * cospi(x) computes cos(pi*x) without multiplication by pi (almost).  First,
 * note that cospi(-x) = cospi(x), so the algorithm considers only |x|.  The
 * method used depends on the magnitude of x.
 *
 * 1. For small |x|, cospi(x) = 1 with FE_INEXACT raised where a sloppy
 *    threshold is used.  The threshold is |x| < 0x1pN with N = -(P/2+M).
 *    P is the precision of the floating-point type and M = 2 to 4.
 *
 * 2. For |x| < 1, argument reduction is not required and sinpi(x) is 
 *    computed by calling a kernel that leverages the kernels for sin(x)
 *    ans cos(x).  See k_sinpi.c and k_cospi.c for details.
 *
 * 3. For 1 <= |x| < 0x1p(P-1), argument reduction is required where
 *    |x| = j0 + r with j0 an integer and the remainder r satisfies
 *    0 <= r < 1.  With the given domain, a simplified inline floor(x)
 *    is used.  Also, note the following identity
 *
 *    cospi(x) = cos(pi*(j0+r))
 *             = cos(pi*j0) * cos(pi*r) - sin(pi*j0) * sin(pi*r)
 *             = cos(pi*j0) * cos(pi*r)
 *             = +-cospi(r)
 *
 *    If j0 is even, then cos(pi*j0) = 1. If j0 is odd, then cos(pi*j0) = -1.
 *    cospi(r) is then computed via an appropriate kernel.
 *
 * 4. For |x| >= 0x1p(P-1), |x| is integral and cospi(x) = 1.
 *
 * 5. Special cases:
 *
 *    cospi(+-0) = 1.
 *    cospi(n.5) = 0 for n an integer.
 *    cospi(+-inf) = nan.  Raises the "invalid" floating-point exception.
 *    cospi(nan) = nan.  Raises the "invalid" floating-point exception.
 */

#include <float.h>
#include "math.h"
#include "math_private.h"

static const double
pi_hi = 3.1415926814079285e+00,	/* 0x400921fb 0x58000000 */
pi_lo =-2.7818135228334233e-08;	/* 0xbe5dde97 0x3dcb3b3a */

#include "k_cospi.h"
#include "k_sinpi.h"

volatile static const double vzero = 0;

double
cospi(double x)
{
	double ax, c;
	uint32_t hx, ix, j0, lx;

	EXTRACT_WORDS(hx, lx, x);
	ix = hx & 0x7fffffff;
	INSERT_WORDS(ax, ix, lx);

	if (ix < 0x3ff00000) {			/* |x| < 1 */
		if (ix < 0x3fd00000) {		/* |x| < 0.25 */
			if (ix < 0x3e200000) {	/* |x| < 0x1p-29 */
				if ((int)ax == 0)
					return (1);
			}
			return (__kernel_cospi(ax));
		}

		if (ix < 0x3fe00000)		/* |x| < 0.5 */
			c = __kernel_sinpi(0.5 - ax);
		else if (ix < 0x3fe80000){	/* |x| < 0.75 */
			if (ax == 0.5)
				return (0);
			c = -__kernel_sinpi(ax - 0.5);
		} else
			c = -__kernel_cospi(1 - ax);
		return (c);
	}

	if (ix < 0x43300000) {		/* 1 <= |x| < 0x1p52 */
		/* Determine integer part of ax. */
		j0 = ((ix >> 20) & 0x7ff) - 0x3ff;
		if (j0 < 20) {
			ix &= ~(0x000fffff >> j0);
			lx = 0;
		} else {
			lx &= ~((uint32_t)0xffffffff >> (j0 - 20));
		}
		INSERT_WORDS(x, ix, lx);

		ax -= x;
		EXTRACT_WORDS(ix, lx, ax);


		if (ix < 0x3fe00000) {		/* |x| < 0.5 */
			if (ix < 0x3fd00000)	/* |x| < 0.25 */
				c = ix == 0 ? 1 : __kernel_cospi(ax);
			else 
				c = __kernel_sinpi(0.5 - ax);
		} else {
			if (ix < 0x3fe80000) {	/* |x| < 0.75 */
				if (ax == 0.5)
					return (0);
				c = -__kernel_sinpi(ax - 0.5);
			} else
				c = -__kernel_cospi(1 - ax);
		}

		if (j0 > 30)
			x -= 0x1p30;
		j0 = (uint32_t)x;
		return (j0 & 1 ? -c : c);
	}

	if (ix >= 0x7f800000)
		return (vzero / vzero);

	/*
	 * |x| >= 0x1p52 is always an even integer, so return 1.
	 */
	return (1);
}

#if LDBL_MANT_DIG == 53
__weak_reference(cospi, cospil);
#endif
