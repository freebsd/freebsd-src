/* k_sinf.c -- float version of k_sin.c
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 * Optimized by Bruce D. Evans.
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

/* Range of maximum relative error in polynomial: ~[-1.61e-10, 1.621e-10]. */
static const float
half = 0.5,
S1  = -0xaaaaab.0p-26,	/* -0.1666666716337203979492187500 */
S2  =  0x8888ba.0p-30,	/*  0.008333379402756690979003906250 */
S3  = -0xd02cb0.0p-36,	/* -0.0001985307317227125167846679687 */
S4  =  0xbe18ff.0p-42;	/*  0.000002832675590980215929448604584 */

float
__kernel_sinf(float x, float y, int iy)
{
	float z,r,v;

	z	=  x*x;
	v	=  z*x;
	r	=  S2+z*(S3+z*S4);
	if(iy==0) return x+v*(S1+z*r);
	else      return x-((z*(half*y-v*r)-y)-v*S1);
}
