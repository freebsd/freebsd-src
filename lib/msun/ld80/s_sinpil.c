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
 * See ../src/s_sinpi.c for implementation details.
 */

#ifdef __i386__
#include <ieeefp.h>
#endif
#include <stdint.h>

#include "fpmath.h"
#include "math.h"
#include "math_private.h"

static const union IEEEl2bits
pi_hi_u = LD80C(0xc90fdaa200000000,   1, 3.14159265346825122833e+00L),
pi_lo_u = LD80C(0x85a308d313198a2e, -33, 1.21542010130123852029e-10L);
#define	pi_hi	(pi_hi_u.e)
#define	pi_lo	(pi_lo_u.e)

#include "k_cospil.h"
#include "k_sinpil.h"

volatile static const double vzero = 0;

long double
sinpil(long double x)
{
	long double ax, hi, lo, s;
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
				if (x == 0)
					RETURNI(x);
				INSERT_LDBL80_WORDS(hi, hx,
				    lx & 0xffffffff00000000ull);
				hi *= 0x1p63L;
				lo = x * 0x1p63L - hi;
				s = (pi_lo + pi_hi) * lo + pi_lo * hi +
				    pi_hi * hi;
				RETURNI(s * 0x1p-63L);
			}
			s = __kernel_sinpil(ax);
			RETURNI((hx & 0x8000) ? -s : s);
		}

		if (ix < 0x3ffe)			/* |x| < 0.5 */
			s = __kernel_cospil(0.5 - ax);
		else if (lx < 0xc000000000000000ull)	/* |x| < 0.75 */
			s = __kernel_cospil(ax - 0.5);
		else
			s = __kernel_sinpil(1 - ax);
		RETURNI((hx & 0x8000) ? -s : s);
	}

	if (ix < 0x403e) {			/* 1 <= |x| < 0x1p63 */
		FFLOORL80(x, j0, ix, lx);	/* Integer part of ax. */
		ax -= x;
		EXTRACT_LDBL80_WORDS(ix, lx, ax);

		if (ix == 0) {
			s = 0;
		} else {
			if (ix < 0x3ffe) {		/* |x| < 0.5 */
				if (ix < 0x3ffd)	/* |x| < 0.25 */
					s = __kernel_sinpil(ax);
				else 
					s = __kernel_cospil(0.5 - ax);
			} else {
							/* |x| < 0.75 */
				if (lx < 0xc000000000000000ull)
					s = __kernel_cospil(ax - 0.5);
				else
					s = __kernel_sinpil(1 - ax);
			}

			if (j0 > 40)
				x -= 0x1p40;
			if (j0 > 30)
				x -= 0x1p30;
			j0 = (uint32_t)x;
			if (j0 & 1) s = -s;
		}
		RETURNI((hx & 0x8000) ? -s : s);
	}

	/* x = +-inf or nan. */
	if (ix >= 0x7fff)
		RETURNI(vzero / vzero);

	/*
	 * |x| >= 0x1p63 is always an integer, so return +-0.
	 */
	RETURNI(copysignl(0, x));
}
