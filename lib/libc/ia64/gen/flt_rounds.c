/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/ia64/gen/flt_rounds.c,v 1.1.34.1 2010/12/21 17:10:29 kensmith Exp $");

#include <float.h>

static const int map[] = {
	1,	/* round to nearest */
	3,	/* round to zero */
	2,	/* round to negative infinity */
	0	/* round to positive infinity */
};

int
__flt_rounds(void)
{
	int x;

	__asm("mov %0=ar.fpsr" : "=r" (x));
        return (map[(x >> 10) & 0x03]);
}
