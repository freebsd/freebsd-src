/*
 * cabs() wrapper for hypot().
 *
 * Written by J.T. Conklin, <jtc@wimsey.com>
 * Placed into the Public Domain, 1994.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD: src/lib/msun/src/w_cabs.c,v 1.4.34.1 2009/04/15 03:14:26 kensmith Exp $";
#endif /* not lint */

#include <complex.h>
#include <math.h>

double
cabs(z)
	double complex z;
{
	return hypot(creal(z), cimag(z));
}

double
z_abs(z)
	double complex *z;
{
	return hypot(creal(*z), cimag(*z));
}
