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

/* Test program for g_Qfmt, strtoIQ, strtopQ, and strtorQ.
 *
 * Inputs (on stdin):
 *		r rounding_mode
 *		n ndig
 *		number
 *		#hex0 hex1 hex2 hex3
 *
 *	rounding_mode values:
 *		0 = toward zero
 *		1 = nearest
 *		2 = toward +Infinity
 *		3 = toward -Infinity
 *
 * where number is a decimal floating-point number,
 * hex0 is a string of <= 8 Hex digits for the most significant
 * word of the number, hex1 is a similar string for the next
 * word, etc., and ndig is a parameters to g_Qfmt.
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
#endif
#ifdef IEEE_8087
#define _0 3
#define _1 2
#define _2 1
#define _3 0
#endif

#define U (unsigned long)

 int
main(Void)
{
	char *s, *se, *se1;
	int i, dItry, ndig = 0, r = 1;
	union { long double d; ULong bits[4]; } u, v[2];

	while( (s = fgets(ibuf, sizeof(ibuf), stdin)) !=0) {
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
			sscanf(s+1, "%lx %lx %lx %lx", &u.bits[_0],
				&u.bits[_1], &u.bits[_2], &u.bits[_3]);
			printf("\nInput: %s", ibuf);
			printf(" --> f = #%lx %lx %lx %lx\n", u.bits[_0],
				u.bits[_1], u.bits[_2], u.bits[_3]);
			goto fmt_test;
			}
		dItry = 1;
		printf("\nInput: %s", ibuf);
		i = strtorQ(ibuf, &se, r, u.bits);
		if (r == 1 && (strtopQ(ibuf,&se1,v[0].bits) != i
		 || se != se1 || memcmp(u.bits, v[0].bits, 16)))
			printf("***strtoQ and strtorQ disagree!!\n:");
		printf("\nstrtoQ consumes %d bytes and returns %d\n",
				(int)(se-ibuf), i);
		printf("with bits = #%lx %lx %lx %lx\n",
			U u.bits[_0], U u.bits[_1], U u.bits[_2], U u.bits[_3]);
 fmt_test:
		if (sizeof(long double) == 16)
			printf("printf(\"%%.35Lg\") gives %.35Lg\n", u.d);
		se = g_Qfmt(obuf, u.bits, ndig, sizeof(obuf));
		printf("g_Qfmt(%d) gives %d bytes: \"%s\"\n\n", ndig,
			(int)(se-obuf), se ? obuf : "<null>");
		if (!dItry)
			continue;
		printf("strtoIQ returns %d,",
			strtoIQ(ibuf, &se, v[0].bits, v[1].bits));
		printf(" consuming %d bytes.\n", (int)(se-ibuf));
		if (!memcmp(v[0].bits, v[1].bits, 16)) {
			if (!memcpy(u.bits, v[0].bits, 16))
				printf("fI[0] == fI[1] == strtoQ\n");
			else {
				printf("fI[0] == fI[1] = #%lx %lx %lx %lx\n",
					U v[0].bits[_0], U v[0].bits[_1],
					U v[0].bits[_2], U v[0].bits[_3]);
				if (sizeof(long double) == 16)
				    printf("= %.35Lg\n", v[0].d);
				}
			}
		else {
			printf("fI[0] = #%lx %lx %lx %lx\n",
					U v[0].bits[_0], U v[0].bits[_1],
					U v[0].bits[_2], U v[0].bits[_3]);
			if (sizeof(long double) == 16)
				printf("= %.35Lg\n", v[0].d);
			printf("fI[1] = #%lx %lx %lx %lx\n",
					U v[1].bits[_0], U v[1].bits[_1],
					U v[1].bits[_2], U v[1].bits[_3]);
			if (sizeof(long double) == 16)
				printf("= %.35Lg\n", v[1].d);
			if (!memcmp(v[0].bits, u.bits, 16))
				printf("fI[0] == strtod\n");
			else if (!memcmp(v[1].bits, u.bits, 16))
				printf("fI[1] == strtod\n");
			else
				printf("**** Both differ from strtod ****\n");
			}
		printf("\n");
		}
	return 0;
	}
