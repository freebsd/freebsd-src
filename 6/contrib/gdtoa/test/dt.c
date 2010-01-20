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

/* Test program for strtod and dtoa.
 *
 * Inputs (on stdin):
 *		number[: mode [ndigits]]
 * or
 *		#hex0 hex1[: mode [ndigits]]
 * where number is a decimal floating-point number,
 * hex0 is a string of Hex digits for the most significant
 * word of the number, hex1 is a similar string for the other
 * (least significant) word, and mode and ndigits are
 * parameters to dtoa.
 */

#include <stdio.h>
#include "gdtoa.h"
#ifdef KR_headers
#define Void /*void*/
#else
#define Void void
#endif

#ifdef __STDC__
#include <stdlib.h>
#else
#ifdef __cplusplus
extern "C" double atof(const char*);
#else
extern double atof ANSI((char*));
#endif
#endif
#ifdef IEEE_8087
#define word0(x) ((ULong *)&x)[1]
#define word1(x) ((ULong *)&x)[0]
#else
#define word0(x) ((ULong *)&x)[0]
#define word1(x) ((ULong *)&x)[1]
#endif
#include "errno.h"

#ifdef __cplusplus
extern "C" char *dtoa(double, int, int, int*, int*, char **);
#else
extern char *dtoa ANSI((double, int, int, int*, int*, char **));
#endif

 static void
#ifdef KR_headers
g_fmt(b, x) char *b; double x;
#else
g_fmt(char *b, double x)
#endif
{
	char *s, *se;
	int decpt, i, j, k, sign;

	if (!x) {
		*b++ = '0';
		*b = 0;
		return;
		}
	s = dtoa(x, 0, 0, &decpt, &sign, &se);
	if (sign)
		*b++ = '-';
	if (decpt == 9999) /* Infinity or Nan */ {
		while(*b++ = *s++);
		return;
		}
	if (decpt <= -4 || decpt > se - s + 5) {
		*b++ = *s++;
		if (*s) {
			*b++ = '.';
			while(*b = *s++)
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
		for(j = 2, k = 10; 10*k <= decpt; j++, k *= 10){};
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
		*b++ = '.';
		for(; decpt < 0; decpt++)
			*b++ = '0';
		while(*b++ = *s++);
		}
	else {
		while(*b = *s++) {
			b++;
			if (--decpt == 0 && *s)
				*b++ = '.';
			}
		for(; decpt > 0; decpt--)
			*b++ = '0';
		*b = 0;
		}
	}

 static void
baderrno(Void)
{
	fflush(stdout);
	perror("\nerrno strtod");
	fflush(stderr);
	}

#define U (unsigned long)

 static void
#ifdef KR_headers
check(d) double d;
#else
check(double d)
#endif
{
	char buf[64];
	int decpt, sign;
	char *s, *se;
	double d1;

	s = dtoa(d, 0, 0, &decpt, &sign, &se);
	sprintf(buf, "%s.%se%d", sign ? "-" : "", s, decpt);
	errno = 0;
	d1 = strtod(buf, (char **)0);
	if (errno)
		baderrno();
	if (d != d1) {
		printf("sent d = %.17g = 0x%lx %lx, buf = %s\n",
			d, U word0(d), U word1(d), buf);
		printf("got d1 = %.17g = 0x%lx %lx\n",
			d1, U word0(d1), U word1(d1));
		}
	}

main(Void){
	char buf[2048], buf1[32];
	char *fmt, *s, *se;
	double d, d1;
	int decpt, sign;
	int mode = 0, ndigits = 17;
	ULong x, y;
#ifdef VAX
	ULong z;
#endif

	while(fgets(buf, sizeof(buf), stdin)) {
		if (*buf == '*') {
			printf("%s", buf);
			continue;
			}
		printf("Input: %s", buf);
		if (*buf == '#') {
			x = word0(d);
			y = word1(d);
			sscanf(buf+1, "%lx %lx:%d %d", &x, &y, &mode, &ndigits);
			word0(d) = x;
			word1(d) = y;
			fmt = "Output: d =\n%.17g = 0x%lx %lx\n";
			}
		else {
			errno = 0;
			d = strtod(buf,&se);
			if (*se == ':')
				sscanf(se+1,"%d %d", &mode, &ndigits);
			d1 = atof(buf);
			fmt = "Output: d =\n%.17g = 0x%lx %lx, se = %s";
			if (errno)
				baderrno();
			}
		printf(fmt, d, U word0(d), U word1(d), se);
		g_fmt(buf1, d);
		printf("\tg_fmt gives \"%s\"\n", buf1);
		if (*buf != '#' && d != d1)
			printf("atof gives\n\
	d1 = %.17g = 0x%lx %lx\nversus\n\
	d  = %.17g = 0x%lx %lx\n", d1, U word0(d1), U word1(d1),
				d, U word0(d), U word1(d));
		check(d);
		s = dtoa(d, mode, ndigits, &decpt, &sign, &se);
		printf("\tdtoa(mode = %d, ndigits = %d):\n", mode, ndigits);
		printf("\tdtoa returns sign = %d, decpt = %d, %d digits:\n%s\n",
			sign, decpt, se-s, s);
		x = word1(d);
		if (x != 0xffffffff
		 && (word0(d) & 0x7ff00000) != 0x7ff00000) {
#ifdef VAX
			z = x << 16 | x >> 16;
			z++;
			z = z << 16 | z >> 16;
			word1(d) = z;
#else
			word1(d) = x + 1;
#endif
			printf("\tnextafter(d,+Inf) = %.17g = 0x%lx %lx:\n",
				d, U word0(d), U word1(d));
			g_fmt(buf1, d);
			printf("\tg_fmt gives \"%s\"\n", buf1);
			s = dtoa(d, mode, ndigits, &decpt, &sign, &se);
			printf(
		"\tdtoa returns sign = %d, decpt = %d, %d digits:\n%s\n",
				sign, decpt, se-s, s);
			check(d);
			}
		if (x) {
#ifdef VAX
			z = x << 16 | x >> 16;
			z--;
			z = z << 16 | z >> 16;
			word1(d) = z;
#else
			word1(d) = x - 1;
#endif
			printf("\tnextafter(d,-Inf) = %.17g = 0x%lx %lx:\n",
				d, U word0(d), U word1(d));
			g_fmt(buf1, d);
			printf("\tg_fmt gives \"%s\"\n", buf1);
			s = dtoa(d, mode, ndigits, &decpt, &sign, &se);
			printf(
		"\tdtoa returns sign = %d, decpt = %d, %d digits:\n%s\n",
				sign, decpt, se-s, s);
			check(d);
			}
		}
	return 0;
	}
