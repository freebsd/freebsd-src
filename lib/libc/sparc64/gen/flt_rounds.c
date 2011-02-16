/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/sparc64/gen/flt_rounds.c,v 1.2.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $");

#include <sys/types.h>
#include <machine/float.h>

static const int map[] = {
	1,	/* round to nearest */
	0,	/* round to zero */
	2,	/* round to positive infinity */
	3	/* round to negative infinity */
};

int
__flt_rounds()
{
	int x;

	__asm("st %%fsr,%0" : "=m" (*&x));
	return map[(x >> 30) & 0x03];
}
