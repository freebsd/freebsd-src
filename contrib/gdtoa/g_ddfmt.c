/****************************************************************

The author of this software is David M. Gay.

Copyright (C) 1998 by Lucent Technologies
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
#include <string.h>

 char *
#ifdef KR_headers
g_ddfmt(buf, dd, ndig, bufsize) char *buf; double *dd; int ndig; unsigned bufsize;
#else
g_ddfmt(char *buf, double *dd, int ndig, unsigned bufsize)
#endif
{
	FPI fpi;
	char *b, *s, *se;
	ULong *L, bits0[4], *bits, *zx;
	int bx, by, decpt, ex, ey, i, j, mode;
	Bigint *x, *y, *z;
	double ddx[2];

	if (bufsize < 10 || bufsize < ndig + 8)
		return 0;

	L = (ULong*)dd;
	if ((L[_0] & 0x7ff00000L) == 0x7ff00000L) {
		/* Infinity or NaN */
		if (L[_0] & 0xfffff || L[_1]) {
 nanret:
			return strcp(buf, "NaN");
			}
		if ((L[2+_0] & 0x7ff00000) == 0x7ff00000) {
			if (L[2+_0] & 0xfffff || L[2+_1])
				goto nanret;
			if ((L[_0] ^ L[2+_0]) & 0x80000000L)
				goto nanret;	/* Infinity - Infinity */
			}
 infret:
		b = buf;
		if (L[_0] & 0x80000000L)
			*b++ = '-';
		return strcp(b, "Infinity");
		}
	if ((L[2+_0] & 0x7ff00000) == 0x7ff00000) {
		L += 2;
		if (L[_0] & 0xfffff || L[_1])
			goto nanret;
		goto infret;
		}
	if (dd[0] + dd[1] == 0.) {
		b = buf;
#ifndef IGNORE_ZERO_SIGN
		if (L[_0] & L[2+_0] & 0x80000000L)
			*b++ = '-';
#endif
		*b++ = '0';
		*b = 0;
		return b;
		}
	if ((L[_0] & 0x7ff00000L) < (L[2+_0] & 0x7ff00000L)) {
		ddx[1] = dd[0];
		ddx[0] = dd[1];
		dd = ddx;
		L = (ULong*)dd;
		}
	z = d2b(dd[0], &ex, &bx);
	if (dd[1] == 0.)
		goto no_y;
	x = z;
	y = d2b(dd[1], &ey, &by);
	if ( (i = ex - ey) !=0) {
		if (i > 0) {
			x = lshift(x, i);
			ex = ey;
			}
		else
			y = lshift(y, -i);
		}
	if ((L[_0] ^ L[2+_0]) & 0x80000000L) {
		z = diff(x, y);
		if (L[_0] & 0x80000000L)
			z->sign = 1 - z->sign;
		}
	else {
		z = sum(x, y);
		if (L[_0] & 0x80000000L)
			z->sign = 1;
		}
	Bfree(x);
	Bfree(y);
 no_y:
	bits = zx = z->x;
	for(i = 0; !*zx; zx++)
		i += 32;
	i += lo0bits(zx);
	if (i) {
		rshift(z, i);
		ex += i;
		}
	fpi.nbits = z->wds * 32 - hi0bits(z->x[j = z->wds-1]);
	if (fpi.nbits < 106) {
		fpi.nbits = 106;
		if (j < 3) {
			for(i = 0; i <= j; i++)
				bits0[i] = bits[i];
			while(i < 4)
				bits0[i++] = 0;
			bits = bits0;
			}
		}
	mode = 2;
	if (ndig <= 0) {
		if (bufsize < (int)(fpi.nbits * .301029995664) + 10) {
			Bfree(z);
			return 0;
			}
		mode = 0;
		}
	fpi.emin = 1-1023-53+1;
	fpi.emax = 2046-1023-106+1;
	fpi.rounding = FPI_Round_near;
	fpi.sudden_underflow = 0;
	i = STRTOG_Normal;
	s = gdtoa(&fpi, ex, bits, &i, mode, ndig, &decpt, &se);
	b = g__fmt(buf, s, se, decpt, z->sign);
	Bfree(z);
	return b;
	}
