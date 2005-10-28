/* k_cosf.c -- float version of k_cos.c
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 * Debugged and optimized by Bruce D. Evans.
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

#ifndef lint
static char rcsid[] = "$FreeBSD$";
#endif

#include "math.h"
#include "math_private.h"

/* Range of maximum relative error in polynomial: ~[-1.15e-10, 1.169e-10]. */
static const float
one =  1.0,
C1  =  0xaaaaa5.0p-28,	/*  0.04166664555668830871582031250 */
C2  = -0xb60615.0p-33,	/* -0.001388731063343584537506103516 */
C3  =  0xccf47d.0p-39;	/*  0.00002443254288664320483803749084 */

float
__kernel_cosf(float x, float y)
{
	float hz,z,r,w;

	z  = x*x;
	r  = z*(C1+z*(C2+z*C3));
	hz = (float)0.5*z;
	w  = one-hz;
	return w + (((one-w)-hz) + (z*r-x*y));
}
