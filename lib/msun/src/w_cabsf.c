/*
 * cabsf() wrapper for hypotf().
 *
 * Written by J.T. Conklin, <jtc@wimsey.com>
 * Placed into the Public Domain, 1994.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD: src/lib/msun/src/w_cabsf.c,v 1.3.30.1 2008/10/02 02:57:24 kensmith Exp $";
#endif /* not lint */

#include <complex.h>
#include <math.h>
#include "math_private.h"

float
cabsf(z)
	float complex z;
{

	return hypotf(crealf(z), cimagf(z));
}
