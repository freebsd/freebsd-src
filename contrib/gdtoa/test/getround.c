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

#include <stdio.h>
#include <stdlib.h>

static char *dir[4] = { "toward zero", "nearest", "toward +Infinity",
			"toward -Infinity" };

 int
#ifdef KR_headers
getround(r, s) int r; char *s;
#else
getround(int r, char *s)
#endif
{
	int i;

	i = atoi(s+1);
	if (i >= 0 && i < 4) {
		printf("Rounding mode for strtor... ");
		if (i == r)
			printf("was and is %d (%s)\n", i, dir[i]);
		else
			printf("changed from %d (%s) to %d (%s)\n",
				r, dir[r], i, dir[i]);
		return i;
		}
	printf("Bad rounding direction %d: choose among\n", i);
	for(i = 0; i < 4; i++)
		printf("\t%d (%s)\n", i, dir[i]);
	printf("Leaving rounding mode for strtor... at %d (%s)\n", r, dir[r]);
	return r;
	}
