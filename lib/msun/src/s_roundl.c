/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003, Steven G. Kargl
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

#include <float.h>
#ifdef __i386__
#include <ieeefp.h>
#endif
#include <stdint.h>

#include "fpmath.h"
#include "math.h"
#include "math_private.h"

#ifdef LDBL_IMPLICIT_NBIT
#define	MANH_SIZE	(LDBL_MANH_SIZE + 1)
#define	INC_MANH(u, c)	do {					\
	uint64_t o = u.bits.manh;				\
	u.bits.manh += (c);					\
	if (u.bits.manh < o)					\
		u.bits.exp++;					\
} while (0)
#else
#define	MANH_SIZE	LDBL_MANH_SIZE
#define	INC_MANH(u, c)	do {					\
	uint64_t o = u.bits.manh;				\
	u.bits.manh += (c);					\
	if (u.bits.manh < o) {					\
		u.bits.exp++;					\
		u.bits.manh |= 1llu << (LDBL_MANH_SIZE - 1);	\
	}							\
} while (0)
#endif

static const long double huge = 1.0e300L;

long double
roundl(long double x)
{
	union IEEEl2bits u = { .e = x };
	uint64_t m, o, round_bit;
	uint16_t hx;
	int e;

	GET_LDBL_EXPSIGN(hx, x);
	if ((hx & 0x7fff) == 0x7fff)
		return (x + x);

	ENTERI();

	e = u.bits.exp - LDBL_MAX_EXP + 1;
	if (e < MANH_SIZE - 1) {
		if (e < 0) {
			if ((u.bits.exp > 0 || (u.bits.manh | u.bits.manl) != 0) &&
			    huge + x > 0.0L)
				u.e = e == -1 ?
				    (u.bits.sign ? -1.0L : 1.0L) :
				    (u.bits.sign ? -0.0L : 0.0L);
		} else {
			m = (uint64_t)-1 >> (64 - MANH_SIZE + e + 1);
			if (((u.bits.manh & m) | u.bits.manl) == 0)
				RETURNI(x);
			round_bit = 1llu << (MANH_SIZE - e - 2);
			INC_MANH(u, round_bit);
			if (huge + x > 0.0L) {
				u.bits.manh &= ~m;
				u.bits.manl = 0;
			}
		}
	} else if (e < LDBL_MANT_DIG - 1) {
		m = (uint64_t)-1 >> (64 - LDBL_MANT_DIG + e + 1);
		if ((u.bits.manl & m) == 0)
			RETURNI(x);
		round_bit = 1llu << (LDBL_MANT_DIG - e - 2);
		o = u.bits.manl;
		u.bits.manl += round_bit;
		if (u.bits.manl < o)
			INC_MANH(u, 1);
		if (huge + x > 0.0L)
			u.bits.manl &= ~m;
	}
	RETURNI(u.e);
}

#if LDBL_MANT_DIG == 113
__weak_reference(roundl, roundf128);
#endif
