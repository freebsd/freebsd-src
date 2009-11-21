/*
 * cabs() wrapper for hypot().
 *
 * Written by J.T. Conklin, <jtc@wimsey.com>
 * Placed into the Public Domain, 1994.
 *
 * Modified by Steven G. Kargl for the long double type.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/w_cabsl.c,v 1.1.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $");

#include <complex.h>
#include <math.h>

long double
cabsl(long double complex z)
{
	return hypotl(creall(z), cimagl(z));
}
