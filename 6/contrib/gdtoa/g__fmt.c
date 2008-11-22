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
	dmg@acm.org
 */

#include "gdtoaimp.h"

#ifdef USE_LOCALE
#include "locale.h"
#endif

 char *
#ifdef KR_headers
g__fmt(b, s, se, decpt, sign) char *b; char *s; char *se; int decpt; ULong sign;
#else
g__fmt(char *b, char *s, char *se, int decpt, ULong sign)
#endif
{
	int i, j, k;
	char *s0 = s;
#ifdef USE_LOCALE
	char decimalpoint = *localeconv()->decimal_point;
#else
#define decimalpoint '.'
#endif
	if (sign)
		*b++ = '-';
	if (decpt <= -4 || decpt > se - s + 5) {
		*b++ = *s++;
		if (*s) {
			*b++ = decimalpoint;
			while((*b = *s++) !=0)
				b++;
			}
		*b++ = 'e';
		/* sprintf(b, "%+.2d", decpt - 1); */
		if (--decpt < 0) {
			*b++ = '-';
			decpt = -decpt;
			}
		else
			*b++ = '+';
		for(j = 2, k = 10; 10*k <= decpt; j++, k *= 10){}
		for(;;) {
			i = decpt / k;
			*b++ = i + '0';
			if (--j <= 0)
				break;
			decpt -= i*k;
			decpt *= 10;
			}
		*b = 0;
		}
	else if (decpt <= 0) {
		*b++ = decimalpoint;
		for(; decpt < 0; decpt++)
			*b++ = '0';
		while((*b = *s++) !=0)
			b++;
		}
	else {
		while((*b = *s++) !=0) {
			b++;
			if (--decpt == 0 && *s)
				*b++ = decimalpoint;
			}
		for(; decpt > 0; decpt--)
			*b++ = '0';
		*b = 0;
		}
	freedtoa(s0);
	return b;
 	}
