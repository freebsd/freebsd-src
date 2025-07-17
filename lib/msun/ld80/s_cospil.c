/*-
 * Copyright (c) 2017, 2023 Steven G. Kargl
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

#ifdef __i386__
#include <ieeefp.h>
#endif
#include <stdint.h>

#include "fpmath.h"
#include "math.h"
#include "math_private.h"

static const double
pi_hi = 3.1415926814079285e+00,	/* 0x400921fb 0x58000000 */
pi_lo =-2.7818135228334233e-08;	/* 0xbe5dde97 0x3dcb3b3a */

#include "k_cospil.h"
#include "k_sinpil.h"

volatile static const double vzero = 0;

long double
cospil(long double x)
{
	long double ax, c;
	uint64_t lx, m;
	uint32_t j0;
	uint16_t hx, ix;

	EXTRACT_LDBL80_WORDS(hx, lx, x);
	ix = hx & 0x7fff;
	INSERT_LDBL80_WORDS(ax, ix, lx);

	ENTERI();

	if (ix < 0x3fff) {			/* |x| < 1 */
		if (ix < 0x3ffd) {		/* |x| < 0.25 */
			if (ix < 0x3fdd) {	/* |x| < 0x1p-34 */
				if ((int)x == 0)
					RETURNI(1);
			}
			RETURNI(__kernel_cospil(ax));
		}

		if (ix < 0x3ffe)			/* |x| < 0.5 */
			c = __kernel_sinpil(0.5 - ax);
		else if (lx < 0xc000000000000000ull) {	/* |x| < 0.75 */
			if (ax == 0.5)
				RETURNI(0);
			c = -__kernel_sinpil(ax - 0.5);
		} else
			c = -__kernel_cospil(1 - ax);
		RETURNI(c);
	}

	if (ix < 0x403e) {			/* 1 <= |x| < 0x1p63 */
		FFLOORL80(x, j0, ix, lx);	/* Integer part of ax. */
		ax -= x;
		EXTRACT_LDBL80_WORDS(ix, lx, ax);

		if (ix < 0x3ffe) {			/* |x| < 0.5 */
			if (ix < 0x3ffd)		/* |x| < 0.25 */
				c = ix == 0 ? 1 : __kernel_cospil(ax);
			else
				c = __kernel_sinpil(0.5 - ax);

		} else {
			if (lx < 0xc000000000000000ull) { /* |x| < 0.75 */
				if (ax == 0.5)
					RETURNI(0);
				c = -__kernel_sinpil(ax - 0.5);
			} else
				c = -__kernel_cospil(1 - ax);
		}

		if (j0 > 40)
			x -= 0x1p40;
		if (j0 > 30)
			x -= 0x1p30;
		j0 = (uint32_t)x;

		RETURNI(j0 & 1 ? -c : c);
	}

	if (ix >= 0x7fff)
		RETURNI(vzero / vzero);

	/*
	 * For 0x1p63 <= |x| < 0x1p64 need to determine if x is an even
	 * or odd integer to return t = +1 or -1.
	 * For |x| >= 0x1p64, it is always an even integer, so t = 1.
	 */
	RETURNI(ix >= 0x403f ? 1 : ((lx & 1) ? -1 : 1));
}
