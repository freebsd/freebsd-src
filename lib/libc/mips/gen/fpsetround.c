/*
 * Written by J.T. Conklin, Apr 11, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <ieeefp.h>

fp_rnd
fpsetround(rnd_dir)
	fp_rnd rnd_dir;
{
	fp_rnd old;
	fp_rnd new;

	__asm__("cfc1 %0,$31" : "=r" (old));

	new = old;
	new &= ~0x03;
	new |= (rnd_dir & 0x03);

	__asm__("ctc1 %0,$31" : : "r" (new));

	return old & 0x03;
}
