/* e_gammaf.c -- float version of e_gamma.c.
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 */

/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

/* gammaf(x)
 * Return the logarithm of the Gamma function of x.
 *
 * Method: call gammaf_r
 */

#include "math.h"
#include "math_private.h"

extern int signgam;

float
gammaf(float x)
{
	return gammaf_r(x,&signgam);
}
