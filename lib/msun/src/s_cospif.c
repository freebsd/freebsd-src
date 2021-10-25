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
 * See ../src/s_cospi.c for implementation details.
 */
#define	INLINE_KERNEL_SINDF
#define	INLINE_KERNEL_COSDF

#include "math.h"
#include "math_private.h"
#include "k_cosf.c"
#include "k_sinf.c"

#define	__kernel_cospif(x)	(__kernel_cosdf(M_PI * (x)))
#define	__kernel_sinpif(x)	(__kernel_sindf(M_PI * (x)))

volatile static const float vzero = 0;

float
cospif(float x)
{
	float ax, c;
	uint32_t ix, j0;

	GET_FLOAT_WORD(ix, x);
	ix = ix & 0x7fffffff;
	SET_FLOAT_WORD(ax, ix);

	if (ix < 0x3f800000) {			/* |x| < 1 */
		if (ix < 0x3e800000) {		/* |x| < 0.25 */
			if (ix < 0x38800000) {	/* |x| < 0x1p-14 */
				/* Raise inexact iff != 0. */
				if ((int)ax == 0)
					return (1);
			}
			return (__kernel_cospif(ax));
		}

		if (ix < 0x3f000000)		/* |x| < 0.5 */
			c = __kernel_sinpif(0.5F - ax);
		else if (ix < 0x3f400000) {	/* |x| < 0.75 */
			if (ix == 0x3f000000)
				return (0);
			c = -__kernel_sinpif(ax - 0.5F);
		} else
			c = -__kernel_cospif(1 - ax);
		return (c);
	}

	if (ix < 0x4b000000) {			/* 1 <= |x| < 0x1p23 */
		/* Determine integer part of ax. */
		j0 = ((ix >> 23) & 0xff) - 0x7f;
		ix &= ~(0x007fffff >> j0);
		SET_FLOAT_WORD(x, ix);

		ax -= x;
		GET_FLOAT_WORD(ix, ax);

		if (ix < 0x3f000000) {		/* |x| < 0.5 */
			if (ix < 0x3e800000)	/* |x| < 0.25 */
				c = ix == 0 ? 1 : __kernel_cospif(ax);
			else
				c = __kernel_sinpif(0.5F - ax);
		} else {
			if (ix < 0x3f400000) {	/* |x| < 0.75 */
				if (ix == 0x3f000000)
					return (0);
				c = -__kernel_sinpif(ax - 0.5F);
			} else
				c = -__kernel_cospif(1 - ax);
		}

		j0 = (uint32_t)x;
		return (j0 & 1 ? -c : c);
	}

	/* x = +-inf or nan. */
	if (ix >= 0x7f800000)
		return (vzero / vzero);

	/*
	 * |x| >= 0x1p23 is always an even integer, so return 1.
	 */
	return (1);
}
