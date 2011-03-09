/****************************************************************

The author of this software is David M. Gay.

Copyright (C) 2009 by David M. Gay
All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
source-code copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation.

THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, INDIRECT OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.

****************************************************************/
#include "stdio1.h"
#include "gdtoa.h"
#include <string.h>

#undef allow_Quad
#undef want_Quad
#undef want_Ux
#define want_LD
typedef union Ud {double x; unsigned int u[2]; } Ud;
#ifdef __x86_64 /*{{*/
#define want_Ux
#ifndef NO_GDTOA_i386_Quad /*{*/
typedef union UQ {__float128 x; unsigned int u[4]; } UQ;
#define allow_Quad(x) x
#define want_Quad
#endif /*}*/
#else /*}{*/
#ifdef __i386 /*{{*/
#define want_Ux
#else /*}{*/
#ifdef __sparc /*{{*/
typedef union UQ {long double x; unsigned int u[4]; } Ux;
#else /*}{*/
#ifdef __INTEL_COMPILER /*{*/
#undef want_Quad
#undef want_Ux
#undef want_LD
#endif /*}*/
#endif /*}}*/
#endif /*}}*/
#endif /*}}*/

#ifndef allow_Quad
#define allow_Quad(x) /*nothing*/
#endif

#ifdef want_Ux /*{{*/
typedef union Ux {long double x; unsigned short u[5]; } Ux;
#else /*}{*/
#ifdef __sparc
#define want_Ux
#endif
#endif /*}}*/

 int
main(void)
{
	Ud d;
	allow_Quad(UQ q;)
	char *b, buf[256], fmt[32], *s;
#ifdef want_Ux
	Ux x;
	x.x = 0.;
#endif
	int k;

	k = 0;
	strcpy(fmt, "%.g");
	d.x = 0.;
	allow_Quad(q.x = 0.;)
	while(fgets(buf, sizeof(buf), stdin)) {
		for(b = buf; *b && *b != '\n'; ++b);
		*b = 0;
		if (b == buf)
			continue;
		b = buf;
		if (*b == '%') {
			for(k = 0; *b > ' '; ++b)
#ifdef want_LD /*{{*/
				switch(*b) {
				  case 'L':
					k = 1;
#ifdef want_Quad
					break;
				  case 'q':
					if (k >= 1)
						k = 2;
#endif
				  }
#else /*}{*/
				;
#endif /*}}*/
			if (*b)
				*b++ = 0;
			if (b - buf < sizeof(fmt)) {
				strcpy(fmt, buf);
				}
			}
		if (*b) {
			switch(k) {
			  case 0:
				d.x = strtod(b,&s);
				break;
			  case 1:
#ifdef want_Ux
#ifdef __sparc
				strtopQ(b,&s,&x.x);
#else
				strtopx(b,&s,&x.x);
#endif
#else
				strtopQ(b,&s,&q.x);
#endif
				break;
			  allow_Quad(case 2: strtopQ(b,&s,&q.x);)
			  }
			if (*s)
				printf("Ignoring \"%s\"\n", s);
			}
		switch(k) {
			case 0:
				printf("d.x = %.g = #%x %x; %s ==> ", d.x, d.u[1], d.u[0], fmt);
				printf(fmt, d.x);
				break;
			case 1:
#ifdef __sparc
				printf("x.x = %.Lg = #%x %x %x %x; %s ==> ", x.x,
					x.u[0], x.u[1], x.u[2], x.u[3], fmt);
#else
				printf("x.x = %.Lg = #%x %x %x %x %x; %s ==> ", x.x,
					x.u[4], x.u[3], x.u[2], x.u[1], x.u[0], fmt);
#endif
				printf(fmt, x.x);
#ifdef want_Quad
				break;
			case 2:
				printf("q.x = %.Lqg = #%x %x %x %x; %s ==> ", q.x,
					q.u[3], q.u[2], q.u[1], q.u[0], fmt);
				printf(fmt, q.x);
#endif
			}
		putchar('\n');
		}
	return 0;
	}
