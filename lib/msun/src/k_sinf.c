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

#ifndef INLINE_KERNEL_SINF
#ifndef lint
static char rcsid[] = "$FreeBSD$";
#endif
#endif

#include "math.h"
#include "math_private.h"

/* |sin(x)/x - s(x)| < 2**-32.5 (~[-1.57e-10, 1.572e-10]). */
static const float
half = 0.5,
S1  = -0xaaaaab.0p-26,		/* -0.16666667163 */
S2  =  0x8888bb.0p-30,		/*  0.0083333803341 */
S3  = -0xd02de1.0p-36,		/* -0.00019853517006 */
S4  =  0xbe6dbe.0p-42;		/*  0.0000028376084629 */

#ifdef INLINE_KERNEL_SINF
extern inline
#endif
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
