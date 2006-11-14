/****************************************************************

The author of this software is David M. Gay.

Copyright (C) 1998, 2001 by Lucent Technologies
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
#include <stdio.h>
#include <stdlib.h>

 static char ibuf[2048];

#define U (unsigned long)

 static void
#ifdef KR_headers
dshow(what, d) char *what; double d;
#else
dshow(char *what, double d)
#endif
{
	char buf[32];
	g_dfmt(buf, &d, 0, sizeof(buf));
	printf("%s = #%lx %lx = %s\n", what,
		U ((ULong*)&d)[_0], U ((ULong*)&d)[_1], buf);
	}

 int
main(Void)
{
	/* Input: one number per line */

	char *s, *se, *se1;
	int i, j;
	double dd[2], dd1, dd2;
	static char cfmt[] = "%s consumes %d bytes and returns %d\n";

	while( (s = fgets(ibuf, sizeof(ibuf), stdin)) !=0) {
		while(*s <= ' ')
			if (!*s++)
				continue;
		printf("\nInput: %s", ibuf);
		i = strtodI(ibuf, &se, dd);
		printf(cfmt, "strtodI", (int)(se-ibuf), i);
		dshow("dd[0]", dd[0]);
		dshow("dd[1]", dd[1]);
		printf("\n");
		j = strtoId(ibuf, &se1, &dd1, &dd2);
		if (j != i || se != se1
		 || dd[0] != dd1 || dd[1] != dd2) {
			printf(cfmt, "**** strtoId", (int)(se-ibuf), j);
			dshow("dd1", dd1);
			dshow("dd2", dd2);
			}
		}
	return 0;
	}
