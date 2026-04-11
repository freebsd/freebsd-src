/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2004 David Schultz <das@FreeBSD.ORG>
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

#include <float.h>
#include <math.h>

#include "fpmath.h"

#ifdef USE_BUILTIN_FMINIMUM_MAG
double
fminimum_mag(double x, double y)
{
	return (__builtin_fminimum_mag(x, y));
}
#else
double
fminimum_mag(double x, double y)
{
	union IEEEd2bits u[2];

	u[0].d = x;
	u[1].d = y;

	/* Handle NaN according to ISO/IEC 60559. NaN argument -> NaN return */
	if (u[0].bits.exp == 2047 && (u[0].bits.manh | u[0].bits.manl) != 0 ||
	    u[1].bits.exp == 2047 && (u[1].bits.manh | u[1].bits.manl) != 0)
		return (NAN);

	double ax = fabs(x);
	double ay = fabs(y);

	if (ay < ax)
		return (y);
	if (ax < ay)
		return (x);

	/* If magnitudes are equal, we break the tie with the sign */
	if (u[0].bits.sign != u[1].bits.sign)
		return (u[u[1].bits.sign].d);

	return (x);
}
#endif

#if (LDBL_MANT_DIG == 53)
__weak_reference(fminimum_mag, fminimum_magl);
#endif


