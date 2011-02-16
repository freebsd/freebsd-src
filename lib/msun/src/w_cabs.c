/*
 * cabs() wrapper for hypot().
 *
 * Written by J.T. Conklin, <jtc@wimsey.com>
 * Placed into the Public Domain, 1994.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/w_cabs.c,v 1.7.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $");

#include <complex.h>
#include <float.h>
#include <math.h>

double
cabs(double complex z)
{
	return hypot(creal(z), cimag(z));
}

#if LDBL_MANT_DIG == 53
__weak_reference(cabs, cabsl);
#endif
