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

/*
 * See ../src/s_tanpi.c for implementation details.
 */

#define INLINE_KERNEL_TANDF

#include "math.h"
#include "math_private.h"
#include "k_tanf.c"

static const float
pi_hi =  3.14160156e+00F,	/* 0x40491000 */
pi_lo = -8.90890988e-06F;	/* 0xb715777a */

static inline float
__kernel_tanpif(float x)
{
	float t;

	if (x < 0.25F)
		t = __kernel_tandf(M_PI * x, 1);
	else if (x > 0.25F)
		t = -__kernel_tandf(M_PI * (0.5 - x), -1);
	else
		t = 1;

	return (t);
}

volatile static const float vzero = 0;

float
tanpif(float x)
{
	float ax, hi, lo, t;
	uint32_t hx, ix, j0;

	GET_FLOAT_WORD(hx, x);
	ix = hx & 0x7fffffff;
	SET_FLOAT_WORD(ax, ix);

	if (ix < 0x3f800000) {			/* |x| < 1 */
		if (ix < 0x3f000000) {		/* |x| < 0.5 */
			if (ix < 0x38800000) {	/* |x| < 0x1p-14 */
				if (ix == 0)
					return (x);
				SET_FLOAT_WORD(hi, hx & 0xffff0000);
				hi *= 0x1p23F;
				lo = x * 0x1p23F - hi;
				t = (pi_lo + pi_hi) * lo + pi_lo * hi +
				    pi_hi * hi;
				return (t * 0x1p-23F);
			}
			t = __kernel_tanpif(ax);
		} else if (ix == 0x3f000000)
			return ((ax - ax) / (ax - ax));
		else
			t = - __kernel_tanpif(1 - ax);
		return ((hx & 0x80000000) ? -t : t);
	}

	if (ix < 0x4b000000) {		/* 1 <= |x| < 0x1p23 */
		/* Determine integer part of ax. */
		j0 = ((ix >> 23) & 0xff) - 0x7f;
		ix &= ~(0x007fffff >> j0);
		SET_FLOAT_WORD(x, ix);

		ax -= x;
		GET_FLOAT_WORD(ix, ax);

		if (ix < 0x3f000000)		/* |x| < 0.5 */
			t = ix == 0 ? 0 : __kernel_tanpif(ax);
		else if (ix == 0x3f000000)
			return ((ax - ax) / (ax - ax));
		else
			t = - __kernel_tanpif(1 - ax);
		return ((hx & 0x80000000) ? -t : t);
	}

	/* x = +-inf or nan. */
	if (ix >= 0x7f800000)
		return (vzero / vzero);

	/*
	 * |x| >= 0x1p23 is always an integer, so return +-0.
	 */
	return (copysignf(0, x));
}
