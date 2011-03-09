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

/* Please send bug reports to David M. Gay (dmg at acm dot org,
 * with " at " changed at "@" and " dot " changed to ".").	*/

/* Test program for g_ddfmt, strtoIdd, strtopdd, and strtordd.
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
 * word, etc., and ndig is a parameters to g_ddfmt.
 */

#include "gdtoaimp.h"
#include <stdio.h>
#include <stdlib.h>

 extern int getround ANSI((int,char*));

 static char ibuf[2048], obuf[1024];

#define U (unsigned long)

 static void
#ifdef KR_headers
dprint(what, d) char *what; double d;
#else
dprint(char *what, double d)
#endif
{
	char buf[32];
	union { double d; ULong L[2]; } u;

	u.d = d;
	g_dfmt(buf,&d,0,sizeof(buf));
	printf("%s = %s = #%lx %lx\n", what, buf, U u.L[_0], U u.L[_1]);
	}

 int
main(Void)
{
	char *s, *s1, *se, *se1;
	int dItry, i, j, r = 1, ndig = 0;
	double ddI[4];
	long LL[4];
	union { double dd[2]; ULong L[4]; } u;

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
			if (i <= ' ' || (i >= '0' && i <= '9')) {
				ndig = atoi(s+1);
				continue;
				}
			break; /* nan? */
		  case '#':
			LL[0] = u.L[_0];
			LL[1] = u.L[_1];
			LL[2] = u.L[2+_0];
			LL[3] = u.L[2+_1];
			sscanf(s+1, "%lx %lx %lx %lx", &LL[0], &LL[1],
				&LL[2], &LL[3]);
			u.L[_0] = LL[0];
			u.L[_1] = LL[1];
			u.L[2+_0] = LL[2];
			u.L[2+_1] = LL[3];
			printf("\nInput: %s", ibuf);
			printf(" --> f = #%lx %lx %lx %lx\n",
				LL[0],LL[1],LL[2],LL[3]);
			goto fmt_test;
			}
		printf("\nInput: %s", ibuf);
		for(s1 = s; *s1 > ' '; s1++){};
		while(*s1 <= ' ' && *s1) s1++;
		if (!*s1) {
			dItry = 1;
			i = strtordd(ibuf, &se, r, u.dd);
			if (r == 1) {
				j = strtopdd(ibuf, &se1, ddI);
				if (i != j || u.dd[0] != ddI[0]
				 || u.dd[1] != ddI[1] || se != se1)
					printf("***strtopdd and strtordd disagree!!\n:");
				}
			printf("strtopdd consumes %d bytes and returns %d\n",
				(int)(se-ibuf), i);
			}
		else {
			u.dd[0] = strtod(s, &se);
			u.dd[1] = strtod(se, &se);
			}
 fmt_test:
		dprint("dd[0]", u.dd[0]);
		dprint("dd[1]", u.dd[1]);
		se = g_ddfmt(obuf, u.dd, ndig, sizeof(obuf));
		printf("g_ddfmt(%d) gives %d bytes: \"%s\"\n\n",
			ndig, (int)(se-obuf), se ? obuf : "<null>");
		if (!dItry)
			continue;
		printf("strtoIdd returns %d,", strtoIdd(ibuf, &se, ddI,&ddI[2]));
		printf(" consuming %d bytes.\n", (int)(se-ibuf));
		if (ddI[0] == ddI[2] && ddI[1] == ddI[3]) {
			if (ddI[0] == u.dd[0] && ddI[1] == u.dd[1])
				printf("ddI[0] == ddI[1] == strtopdd\n");
			else
				printf("ddI[0] == ddI[1] = #%lx %lx + %lx %lx\n= %.17g + %17.g\n",
					U ((ULong*)ddI)[_0],
					U ((ULong*)ddI)[_1],
					U ((ULong*)ddI)[2+_0],
					U ((ULong*)ddI)[2+_1],
					ddI[0], ddI[1]);
			}
		else {
			printf("ddI[0] = #%lx %lx + %lx %lx\n= %.17g + %.17g\n",
				U ((ULong*)ddI)[_0], U ((ULong*)ddI)[_1],
				U ((ULong*)ddI)[2+_0], U ((ULong*)ddI)[2+_1],
				ddI[0], ddI[1]);
			printf("ddI[1] = #%lx %lx + %lx %lx\n= %.17g + %.17g\n",
				U ((ULong*)ddI)[4+_0], U ((ULong*)ddI)[4+_1],
				U ((ULong*)ddI)[6+_0], U ((ULong*)ddI)[6+_1],
				ddI[2], ddI[3]);
			if (ddI[0] == u.dd[0] && ddI[1] == u.dd[1])
				printf("ddI[0] == strtod\n");
			else if (ddI[2] == u.dd[0] && ddI[3] == u.dd[1])
				printf("ddI[1] == strtod\n");
			else
				printf("**** Both differ from strtopdd ****\n");
			}
		printf("\n");
		}
	return 0;
	}
