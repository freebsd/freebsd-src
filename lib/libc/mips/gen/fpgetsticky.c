/*
 * Written by J.T. Conklin, Apr 11, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <ieeefp.h>

fp_except
fpgetsticky()
{
	int x;

	__asm__("cfc1 %0,$31" : "=r" (x));
	return (x >> 2) & 0x1f;
}
