/* e_acosf.c -- float version of e_acos.c.
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
pi =  3.1415925026e+00, /* 0x40490fda */
pio2_hi =  1.5707962513e+00; /* 0x3fc90fda */
static volatile float
pio2_lo =  7.5497894159e-08; /* 0x33a22168 */

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

float
acosf(float x)
{
	float z,p,q,r,w,s,c,df;
	int32_t hx,ix;
	GET_FLOAT_WORD(hx,x);
	ix = hx&0x7fffffff;
	if(ix>=0x3f800000) {		/* |x| >= 1 */
	    if(ix==0x3f800000) {	/* |x| == 1 */
		if(hx>0) return 0.0;	/* acos(1) = 0 */
		else return pi+(float)2.0*pio2_lo;	/* acos(-1)= pi */
	    }
	    return (x-x)/(x-x);		/* acos(|x|>1) is NaN */
	}
	if(ix<0x3f000000) {	/* |x| < 0.5 */
	    if(ix<=0x32800000) return pio2_hi+pio2_lo;/*if|x|<2**-26*/
	    z = x*x;
	    p = z*(pS0+z*(pS1+z*pS2));
	    q = one+z*(qS1+z*qS2);
	    r = p/q;
	    return pio2_hi - (x - (pio2_lo-x*r));
	} else  if (hx<0) {		/* x < -0.5 */
	    z = (one+x)*(float)0.5;
	    p = z*(pS0+z*(pS1+z*pS2));
	    q = one+z*(qS1+z*qS2);
	    s = sqrtf(z);
	    r = p/q;
	    w = r*s-pio2_lo;
	    return pi - (float)2.0*(s+w);
	} else {			/* x > 0.5 */
	    int32_t idf;
	    z = (one-x)*(float)0.5;
	    s = sqrtf(z);
	    df = s;
	    GET_FLOAT_WORD(idf,df);
	    SET_FLOAT_WORD(df,idf&0xfffff000);
	    c  = (z-df*df)/(s+df);
	    p = z*(pS0+z*(pS1+z*pS2));
	    q = one+z*(qS1+z*qS2);
	    r = p/q;
	    w = r*s+c;
	    return (float)2.0*(df+w);
	}
}
