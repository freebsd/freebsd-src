/*-
 * Copyright (c) 2017-2021 Steven G. Kargl
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

#include "math.h"
#include "math_private.h"

/*
 * pi_hi contains the leading 56 bits of a 169 bit approximation for pi.
 */
static const long double
pi_hi = 3.14159265358979322702026593105983920e+00L,
pi_lo = 1.14423774522196636802434264184180742e-17L;

#include "k_cospil.h"
#include "k_sinpil.h"

volatile static const double vzero = 0;

long double
cospil(long double x)
{
	long double ax, c, xf;
	uint32_t ix;

	ax = fabsl(x);

	if (ax <= 1) {
		if (ax < 0.25) {
			if (ax < 0x1p-60) {
				if ((int)x == 0)
					return (1);
			}
			return (__kernel_cospil(ax));
		}

		if (ax < 0.5)
			c = __kernel_sinpil(0.5 - ax);
		else if (ax < 0.75) {
			if (ax == 0.5)
				return (0);
			c = -__kernel_sinpil(ax - 0.5);
		} else
			c = -__kernel_cospil(1 - ax);
		return (c);
	}

	if (ax < 0x1p112) {
		/* Split x = n + r with 0 <= r < 1. */
		xf = (ax + 0x1p112L) - 0x1p112L;        /* Integer part */
		ax -= xf;                               /* Remainder */
		if (ax < 0) {
			ax += 1;
			xf -= 1;
		}

		if (ax < 0.5) {
			if (ax < 0.25)
				c = ax == 0 ? 1 : __kernel_cospil(ax);
			else
				c = __kernel_sinpil(0.5 - ax);
		} else {
			if (ax < 0.75) {
				if (ax == 0.5)
					return (0);
				c = -__kernel_sinpil(ax - 0.5);
			} else
				c = -__kernel_cospil(1 - ax);
		}

		if (xf > 0x1p64)
			xf -= 0x1p64;
		if (xf > 0x1p32)
			xf -= 0x1p32;
		ix = (uint32_t)xf;
		return (ix & 1 ? -c : c);
	}

	if (isinf(x) || isnan(x))
		return (vzero / vzero);

	/*
	 * |x| >= 0x1p112 is always an even integer, so return 1.
	 */
	return (1);
}
