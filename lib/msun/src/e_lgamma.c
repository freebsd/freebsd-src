
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

/* lgamma(x)
 * Return the logarithm of the Gamma function of x.
 *
 * Method: call lgamma_r
 */

#include <float.h>

#include "math.h"
#include "math_private.h"

extern int signgam;

double
lgamma(double x)
{
	return lgamma_r(x,&signgam);
}

#if (LDBL_MANT_DIG == 53)
__weak_reference(lgamma, lgammal);
#endif
