/* e_coshf.c -- float version of e_cosh.c.
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

#ifndef lint
static char rcsid[] = "$FreeBSD$";
#endif

#include "math.h"
#include "math_private.h"

#ifdef __STDC__
static const float one = 1.0, half=0.5, huge = 1.0e30;
#else
static float one = 1.0, half=0.5, huge = 1.0e30;
#endif

#ifdef __STDC__
	float __ieee754_coshf(float x)
#else
	float __ieee754_coshf(x)
	float x;
#endif
{
	float t,w;
	int32_t ix;

	GET_FLOAT_WORD(ix,x);
	ix &= 0x7fffffff;

    /* x is INF or NaN */
	if(ix>=0x7f800000) return x*x;

    /* |x| in [0,0.5*ln2], return 1+expm1(|x|)^2/(2*exp(|x|)) */
	if(ix<0x3eb17218) {
	    t = expm1f(fabsf(x));
	    w = one+t;
	    if (ix<0x24000000) return w;	/* cosh(tiny) = 1 */
	    return one+(t*t)/(w+w);
	}

    /* |x| in [0.5*ln2,22], return (exp(|x|)+1/exp(|x|)/2; */
	if (ix < 0x41b00000) {
		t = __ieee754_expf(fabsf(x));
		return half*t+half/t;
	}

    /* |x| in [22, log(maxdouble)] return half*exp(|x|) */
	if (ix < 0x42b17180)  return half*__ieee754_expf(fabsf(x));

    /* |x| in [log(maxdouble), overflowthresold] */
	if (ix<=0x42b2d4fc) {
	    w = __ieee754_expf(half*fabsf(x));
	    t = half*w;
	    return t*w;
	}

    /* |x| > overflowthresold, cosh(x) overflow */
	return huge*huge;
}
