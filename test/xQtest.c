/****************************************************************

The author of this software is David M. Gay.

Copyright (C) 2002 by Lucent Technologies
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

#include <stdio.h>

 int
main(void)
{
	union { long double d; unsigned int bits[4]; } u, w;
	switch(sizeof(long double)) {
	  case 16:
		w.bits[0] = w.bits[3] = 0;
		w.d = 1.;
		u.d = 3.;
		w.d = w.d / u.d;
		if (w.bits[0] && w.bits[3])
			printf("cp x.ou0 x.out; cp xL.ou0 xL.out;"
				" cp Q.ou1 Q.out; cp pftestQ.out pftest.out\n");
		else
			printf("cp x.ou0 x.out; cp xL.ou0 xL.out;"
				" cp Q.ou0 Q.out; cp pftestx.out pftest.out\n");
		break;
	  case 10:
	  case 12:
		printf("cp x.ou1 x.out; cp xL.ou1 xL.out; cp Q.ou0 Q.out;"
			" cp pftestx.out pftest.out\n");
		break;
	  default:
		printf("cp x.ou0 x.out; cp xL.ou0 xL.out; cp Q.ou0 Q.out;"
			" cp pftestx.out pftest.out\n");
	  }
	return 0;
	}
