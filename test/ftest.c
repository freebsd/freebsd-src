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

/* Test program for g_ffmt, strtof, strtoIf, strtopf, and strtorf.
 *
 * Inputs (on stdin):
 *		r rounding_mode
 *		n ndig
 *		number
 *		#hex
 *
 *	rounding_mode values:
 *		0 = toward zero
 *		1 = nearest
 *		2 = toward +Infinity
 *		3 = toward -Infinity
 *
 * where number is a decimal floating-point number,
 * hex is a string of <= 8 Hex digits for the internal representation
 * of the number, and ndig is a parameters to g_ffmt.
 */

#include "gdtoa.h"
#include <stdio.h>
#include <stdlib.h>

 extern int getround ANSI((int,char*));

 static char ibuf[2048], obuf[1024];

#define U (unsigned long)

 int
main(Void)
{
	char *s, *se, *se1;
	int dItry, i, i1, ndig = 0, r = 1;
	float f1, fI[2];
	union { float f; ULong L[1]; } u;

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
			/* sscanf(s+1, "%lx", &u.L[0]); */
			u.L[0] = (ULong)strtoul(s+1, &se, 16);
			printf("\nInput: %s", ibuf);
			printf(" --> f = #%lx\n", U u.L[0]);
			goto fmt_test;
			}
		dItry = 1;
		printf("\nInput: %s", ibuf);
		i = strtorf(ibuf, &se, r, &u.f);
		if (r == 1) {
		    if (u.f != (i1 = strtopf(ibuf, &se1, &f1), f1)
				 || se != se1 || i != i1) {
			printf("***strtopf and strtorf disagree!!\n");
			if (u.f != f1)
				printf("\tf1 = %g\n", (double)f1);
			if (i != i1)
				printf("\ti = %d but i1 = %d\n", i, i1);
			if (se != se1)
				printf("se - se1 = %d\n", (int)(se-se1));
			}
		    if (u.f != strtof(ibuf, &se1) || se != se1)
			printf("***strtof and strtorf disagree!\n");
		    }
		printf("strtof consumes %d bytes and returns %.8g = #%lx\n",
				(int)(se-ibuf), u.f, U u.L[0]);
 fmt_test:
		se = g_ffmt(obuf, &u.f, ndig, sizeof(obuf));
		printf("g_ffmt(%d) gives %d bytes: \"%s\"\n\n",
			ndig, (int)(se-obuf), se ? obuf : "<null>");
		if (!dItry)
			continue;
		printf("strtoIf returns %d,", strtoIf(ibuf, &se, fI, &fI[1]));
		printf(" consuming %d bytes.\n", (int)(se-ibuf));
		if (fI[0] == fI[1]) {
			if (fI[0] == u.f)
				printf("fI[0] == fI[1] == strtof\n");
			else
				printf("fI[0] == fI[1] = #%lx = %.8g\n",
					U *(ULong*)fI, fI[0]);
			}
		else {
			printf("fI[0] = #%lx = %.8g\nfI[1] = #%lx = %.8g\n",
				U *(ULong*)fI, fI[0],
				U *(ULong*)&fI[1], fI[1]);
			if (fI[0] == u.f)
				printf("fI[0] == strtof\n");
			else if (fI[1] == u.f)
				printf("fI[1] == strtof\n");
			else
				printf("**** Both differ from strtof ****\n");
			}
		printf("\n");
		}
	return 0;
	}
