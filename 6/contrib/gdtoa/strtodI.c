/****************************************************************

The author of this software is David M. Gay.

Copyright (C) 1998, 2000 by Lucent Technologies
All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the name of Lucent or any of its entities
not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior
permission.

LUCENT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
IN NO EVENT SHALL LUCENT OR ANY OF ITS ENTITIES BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
THIS SOFTWARE.

****************************************************************/

/* Please send bug reports to
	David M. Gay
	Bell Laboratories, Room 2C-463
	600 Mountain Avenue
	Murray Hill, NJ 07974-0636
	U.S.A.
	dmg@bell-labs.com
 */

#include "gdtoaimp.h"

 static double
#ifdef KR_headers
ulpdown(d) double *d;
#else
ulpdown(double *d)
#endif
{
	double u;
	ULong *L = (ULong*)d;

	u = ulp(*d);
	if (!(L[_1] | L[_0] & 0xfffff)
	 && (L[_0] & 0x7ff00000) > 0x00100000)
		u *= 0.5;
	return u;
	}

 int
#ifdef KR_headers
strtodI(s, sp, dd) CONST char *s; char **sp; double *dd;
#else
strtodI(CONST char *s, char **sp, double *dd)
#endif
{
#ifdef Sudden_Underflow
	static FPI fpi = { 53, 1-1023-53+1, 2046-1023-53+1, 1, 1 };
#else
	static FPI fpi = { 53, 1-1023-53+1, 2046-1023-53+1, 1, 0 };
#endif
	ULong bits[2], sign;
	Long exp;
	int j, k;
	typedef union {
		double d[2];
		ULong L[4];
		} U;
	U *u;

	k = strtodg(s, sp, &fpi, &exp, bits);
	u = (U*)dd;
	sign = k & STRTOG_Neg ? 0x80000000L : 0;
	switch(k & STRTOG_Retmask) {
	  case STRTOG_NoNumber:
		u->d[0] = u->d[1] = 0.;
		break;

	  case STRTOG_Zero:
		u->d[0] = u->d[1] = 0.;
#ifdef Sudden_Underflow
		if (k & STRTOG_Inexact) {
			if (sign)
				u->L[_0] = 0x80100000L;
			else
				u->L[2+_0] = 0x100000L;
			}
		break;
#else
		goto contain;
#endif

	  case STRTOG_Denormal:
		u->L[_1] = bits[0];
		u->L[_0] = bits[1];
		goto contain;

	  case STRTOG_Normal:
		u->L[_1] = bits[0];
		u->L[_0] = (bits[1] & ~0x100000) | ((exp + 0x3ff + 52) << 20);
	  contain:
		j = k & STRTOG_Inexact;
		if (sign) {
			u->L[_0] |= sign;
			j = STRTOG_Inexact - j;
			}
		switch(j) {
		  case STRTOG_Inexlo:
#ifdef Sudden_Underflow
			if ((u->L[_0] & 0x7ff00000) < 0x3500000) {
				u->L[2+_0] = u->L[_0] + 0x3500000;
				u->L[2+_1] = u->L[_1];
				u->d[1] += ulp(u->d[1]);
				u->L[2+_0] -= 0x3500000;
				if (!(u->L[2+_0] & 0x7ff00000)) {
					u->L[2+_0] = sign;
					u->L[2+_1] = 0;
					}
				}
			else
#endif
			u->d[1] = u->d[0] + ulp(u->d[0]);
			break;
		  case STRTOG_Inexhi:
			u->d[1] = u->d[0];
#ifdef Sudden_Underflow
			if ((u->L[_0] & 0x7ff00000) < 0x3500000) {
				u->L[_0] += 0x3500000;
				u->d[0] -= ulpdown(u->d);
				u->L[_0] -= 0x3500000;
				if (!(u->L[_0] & 0x7ff00000)) {
					u->L[_0] = sign;
					u->L[_1] = 0;
					}
				}
			else
#endif
			u->d[0] -= ulpdown(u->d);
			break;
		  default:
			u->d[1] = u->d[0];
		  }
		break;

	  case STRTOG_Infinite:
		u->L[_0] = u->L[2+_0] = sign | 0x7ff00000;
		u->L[_1] = u->L[2+_1] = 0;
		if (k & STRTOG_Inexact) {
			if (sign) {
				u->L[2+_0] = 0xffefffffL;
				u->L[2+_1] = 0xffffffffL;
				}
			else {
				u->L[_0] = 0x7fefffffL;
				u->L[_1] = 0xffffffffL;
				}
			}
		break;

	  case STRTOG_NaN:
		u->L[_0] = u->L[2+_0] = 0x7fffffff | sign;
		u->L[_1] = u->L[2+_1] = (ULong)-1;
		break;

	  case STRTOG_NaNbits:
		u->L[_0] = u->L[2+_0] = 0x7ff00000 | sign | bits[1];
		u->L[_1] = u->L[2+_1] = bits[0];
	  }
	return k;
	}
