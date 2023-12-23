
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 *
 */

#include <sys/cdefs.h>
/* gamma(x)
 * Return the logarithm of the Gamma function of x.
 *
 * Method: call gamma_r
 */

#include "math.h"
#include "math_private.h"

extern int signgam;

double
gamma(double x)
{
	return gamma_r(x,&signgam);
}
