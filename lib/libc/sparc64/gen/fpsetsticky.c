/*	$NetBSD: fpsetsticky.c,v 1.2 2002/01/13 21:45:51 thorpej Exp $	*/

/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <machine/fsr.h>
#include <ieeefp.h>

fp_except_t
fpsetsticky(sticky)
	fp_except_t sticky;
{
	unsigned int old;
	unsigned int new;

	__asm__("st %%fsr,%0" : "=m" (old));

	new = old;
	new &= ~FSR_AEXC_MASK;
	new |= FSR_AEXC(sticky & FSR_EXC_MASK);

	__asm__("ld %0,%%fsr" : : "m" (new));

	return (FSR_GET_AEXC(old));
}
