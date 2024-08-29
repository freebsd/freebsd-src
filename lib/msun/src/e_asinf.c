/* e_asinf.c -- float version of e_asin.c.
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

#include "math.h"
#include "math_private.h"

static const float
one =  1.0000000000e+00, /* 0x3F800000 */
huge =  1.000e+30;

/*
 * The coefficients for the rational approximation were generated over
 *  0x1p-12f <= x <= 0.5f.  The maximum error satisfies log2(e) < -30.084.
 */
static const float
pS0 =  1.66666672e-01f, /* 0x3e2aaaab */
pS1 = -1.19510300e-01f, /* 0xbdf4c1d1 */
pS2 =  5.47002675e-03f, /* 0x3bb33de9 */
qS1 = -1.16706085e+00f, /* 0xbf956240 */
qS2 =  2.90115148e-01f; /* 0x3e9489f9 */

static const double
pio2 =  1.570796326794896558e+00;

float
asinf(float x)
{
	double s;
	float t,w,p,q;
	int32_t hx,ix;
	GET_FLOAT_WORD(hx,x);
	ix = hx&0x7fffffff;
	if(ix>=0x3f800000) {		/* |x| >= 1 */
	    if(ix==0x3f800000)		/* |x| == 1 */
		return x*pio2;		/* asin(+-1) = +-pi/2 with inexact */
	    return (x-x)/(x-x);		/* asin(|x|>1) is NaN */
	} else if (ix<0x3f000000) {	/* |x|<0.5 */
	    if(ix<0x39800000) {		/* |x| < 2**-12 */
		if(huge+x>one) return x;/* return x with inexact if x!=0*/
	    }
	    t = x*x;
	    p = t*(pS0+t*(pS1+t*pS2));
	    q = one+t*(qS1+t*qS2);
	    w = p/q;
	    return x+x*w;
	}
	/* 1> |x|>= 0.5 */
	w = one-fabsf(x);
	t = w*(float)0.5;
	p = t*(pS0+t*(pS1+t*pS2));
	q = one+t*(qS1+t*qS2);
	s = sqrt(t);
	w = p/q;
	t = pio2-2.0*(s+s*w);
	if(hx>0) return t; else return -t;
}
