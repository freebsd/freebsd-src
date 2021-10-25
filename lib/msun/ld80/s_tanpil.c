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

#ifdef __i386__
#include <ieeefp.h>
#endif
#include <stdint.h>

#include "fpmath.h"
#include "math.h"
#include "math_private.h"

static const double
pi_hi =  3.1415926814079285e+00,	/* 0x400921fb 0x58000000 */
pi_lo = -2.7818135228334233e-08;	/* 0xbe5dde97 0x3dcb3b3a */

static inline long double
__kernel_tanpil(long double x)
{
	long double hi, lo, t;

	if (x < 0.25) {
		hi = (float)x;
		lo = x - hi;
		lo = lo * (pi_lo + pi_hi) + hi * pi_lo;
		hi *= pi_hi;
		_2sumF(hi, lo);
		t = __kernel_tanl(hi, lo, -1);
	} else if (x > 0.25) {
		x = 0.5 - x;
		hi = (float)x;
		lo = x - hi;
		lo = lo * (pi_lo + pi_hi) + hi * pi_lo;
		hi *= pi_hi;
		_2sumF(hi, lo);
		t = - __kernel_tanl(hi, lo, 1);
	} else
		t = 1;

	return (t);
}

volatile static const double vzero = 0;

long double
tanpil(long double x)
{
	long double ax, hi, lo, t;
	uint64_t lx, m;
	uint32_t j0;
	uint16_t hx, ix;

	EXTRACT_LDBL80_WORDS(hx, lx, x);
	ix = hx & 0x7fff;
	INSERT_LDBL80_WORDS(ax, ix, lx);

	ENTERI();

	if (ix < 0x3fff) {			/* |x| < 1 */
		if (ix < 0x3ffe) {		/* |x| < 0.5 */
			if (ix < 0x3fdd) {	/* |x| < 0x1p-34 */
				if (x == 0)
					RETURNI(x);
				INSERT_LDBL80_WORDS(hi, hx,
				    lx & 0xffffffff00000000ull);
				hi *= 0x1p63L;
				lo = x * 0x1p63L - hi;
				t = (pi_lo + pi_hi) * lo + pi_lo * hi +
				    pi_hi * hi;
				RETURNI(t * 0x1p-63L);
			}
			t = __kernel_tanpil(ax);
		} else if (ax == 0.5)
			RETURNI((ax - ax) / (ax - ax));
		else
			t = -__kernel_tanpil(1 - ax);
		RETURNI((hx & 0x8000) ? -t : t);
	}

	if (ix < 0x403e) {		/* 1 <= |x| < 0x1p63 */
		/* Determine integer part of ax. */
		j0 = ix - 0x3fff + 1;
		if (j0 < 32) {
			lx = (lx >> 32) << 32;
			lx &= ~(((lx << 32)-1) >> j0);
		} else {
			m = (uint64_t)-1 >> (j0 + 1);
			if (lx & m) lx &= ~m;
		}
		INSERT_LDBL80_WORDS(x, ix, lx);

		ax -= x;
		EXTRACT_LDBL80_WORDS(ix, lx, ax);

		if (ix < 0x3ffe)		/* |x| < 0.5 */
			t = ax == 0 ? 0 : __kernel_tanpil(ax);
		else if (ax == 0.5)
			RETURNI((ax - ax) / (ax - ax));
		else
			t = -__kernel_tanpil(1 - ax);
		RETURNI((hx & 0x8000) ? -t : t);
	}

	/* x = +-inf or nan. */
	if (ix >= 0x7fff)
		RETURNI(vzero / vzero);

	/*
	 * |x| >= 0x1p63 is always an integer, so return +-0.
	 */
	RETURNI(copysignl(0, x));
}
