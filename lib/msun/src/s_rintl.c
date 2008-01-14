/*-
 * Copyright (c) 2008 David Schultz <das@FreeBSD.ORG>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <float.h>
#include <math.h>

#include "fpmath.h"

static const long double
shift[2]={
#if LDBL_MANT_DIG == 64
	0x1.0p63, -0x1.0p63
#elif LDBL_MANT_DIG == 113
	0x1.0p112, -0x1.0p112
#else
#error "Unsupported long double format"
#endif
};

long double
rintl(long double x)
{
	union IEEEl2bits u;
	short sign;

	u.e = x;

	if (u.bits.exp >= LDBL_MANT_DIG + LDBL_MAX_EXP - 2) {
		/*
		 * The biased exponent is greater than the number of digits
		 * in the mantissa, so x is inf, NaN, or an integer.
		 */
		if (u.bits.exp == 2 * LDBL_MAX_EXP - 1)
			return (x + x);	/* inf or NaN */
		else
			return (x);
	}

	/*
	 * The following code assumes that intermediate results are
	 * evaluated in long double precision. If they are evaluated in
	 * greater precision, double rounding will occur, and if they are
	 * evaluated in less precision (as on i386), results will be
	 * wildly incorrect.
	 */
	sign = u.bits.sign;
	u.e = shift[sign] + x;
	u.e -= shift[sign];
	u.bits.sign = sign;
	return (u.e);
}
