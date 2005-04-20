/****************************************************************

The author of this software is David M. Gay.

Copyright (C) 1998-2001 by Lucent Technologies
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

/* Test program for g_xfmt, strtoIx, strtopx, and strtorx.
 *
 * Inputs (on stdin):
 *		r rounding_mode
 *		n ndig
 *		number
 *		#hex0 hex1 hex2 hex3 hex4
 *
 *	rounding_mode values:
 *		0 = toward zero
 *		1 = nearest
 *		2 = toward +Infinity
 *		3 = toward -Infinity
 *
 * where number is a decimal floating-point number,
 * hex0 is a string of <= 4 Hex digits for the most significant
 * half-word of the number, hex1 is a similar string for the next
 * half-word, etc., and ndig is a parameters to g_xfmt.
 */

#include "gdtoa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

 extern int getround ANSI((int,char*));

 static char ibuf[2048], obuf[2048];

#undef _0
#undef _1

/* one or the other of IEEE_MC68k or IEEE_8087 should be #defined */

#ifdef IEEE_MC68k
#define _0 0
#define _1 1
#define _2 2
#define _3 3
#define _4 4
#endif
#ifdef IEEE_8087
#define _0 4
#define _1 3
#define _2 2
#define _3 1
#define _4 0
#endif

 int
main(Void)
{
	char *s, *se, *se1;
	int i, dItry, ndig = 0, r = 1;
	union { long double d; UShort bits[5]; } u, v[2];

	while(s = fgets(ibuf, sizeof(ibuf), stdin)) {
		while(*s <= ' ')
			if (!*s++)
				continue;
		dItry = 0;
		switch(*s) {
		  case 'r':
			r = getround(r, s);
			continue;
		  case 'n':
			i = s[1];
			if (i <= ' ' || i >= '0' && i <= '9') {
				ndig = atoi(s+1);
				continue;
				}
			break; /* nan? */
		  case '#':
			sscanf(s+1, "%hx %hx %hx %hx hx", &u.bits[_0],
				&u.bits[_1], &u.bits[_2], &u.bits[_3],
				&u.bits[_4]);
			printf("\nInput: %s", ibuf);
			printf(" --> f = #%x %x %x %x %x\n", u.bits[_0],
				u.bits[_1], u.bits[_2], u.bits[_3], u.bits[4]);
			goto fmt_test;
			}
		dItry = 1;
		printf("\nInput: %s", ibuf);
		i = strtorx(ibuf, &se, r, u.bits);
		if (r == 1 && (i != strtopx(ibuf, &se1, v[0].bits) || se1 != se
		 || memcmp(u.bits, v[0].bits, 10)))
			printf("***strtox and strtorx disagree!!\n:");
		printf("\nstrtox consumes %d bytes and returns %d\n",
				(int)(se-ibuf), i);
		printf("with bits = #%x %x %x %x %x\n",
			u.bits[_0], u.bits[_1], u.bits[_2],
			u.bits[_3], u.bits[_4]);
		if (sizeof(long double) == 12)
			printf("printf(\"%%.21Lg\") gives %.21Lg\n", u.d);
 fmt_test:
		se = g_xfmt(obuf, u.bits, ndig, sizeof(obuf));
		printf("g_xfmt(%d) gives %d bytes: \"%s\"\n\n",
			ndig, (int)(se-obuf), se ? obuf : "<null>");
		if (!dItry)
			continue;
		printf("strtoIx returns %d,",
			strtoIx(ibuf, &se, v[0].bits, v[1].bits));
		printf(" consuming %d bytes.\n", (int)(se-ibuf));
		if (!memcmp(v[0].bits, v[1].bits, 10)) {
			if (!memcmp(u.bits, v[0].bits, 10))
				printf("fI[0] == fI[1] == strtox\n");
			else {
				printf("fI[0] == fI[1] = #%x %x %x %x %x\n",
					v[0].bits[_0], v[0].bits[_1],
					v[0].bits[_2], v[0].bits[_3],
					v[0].bits[_4]);
				if (sizeof(long double) == 12)
				    printf("= %.21Lg\n", v[0].d);
				}
			}
		else {
			printf("fI[0] = #%x %x %x %x %x\n",
					v[0].bits[_0], v[0].bits[_1],
					v[0].bits[_2], v[0].bits[_3],
					v[0].bits[_4]);
			if (sizeof(long double) == 12)
				printf("= %.21Lg\n", v[0].d);
			printf("fI[1] = #%x %x %x %x %x\n",
					v[1].bits[_0], v[1].bits[_1],
					v[1].bits[_2], v[0].bits[_3],
					v[0].bits[_4]);
			if (sizeof(long double) == 12)
				printf("= %.21Lg\n", v[1].d);
			if (!memcmp(v[0].bits, u.bits, 10))
				printf("fI[0] == strtox\n");
			else if (!memcmp(v[1].bits, u.bits, 10))
				printf("fI[1] == strtox\n");
			else
				printf("**** Both differ from strtod ****\n");
			}
		printf("\n");
		}
	return 0;
	}
