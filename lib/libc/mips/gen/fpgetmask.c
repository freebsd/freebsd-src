/*
 * Written by J.T. Conklin, Apr 11, 1995
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD$";
#endif /* LIBC_SCCS and not lint */

#include <ieeefp.h>

fp_except
fpgetmask()
{
	int x;

	__asm__("cfc1 %0,$31" : "=r" (x));
	return (x >> 7) & 0x1f;
}
