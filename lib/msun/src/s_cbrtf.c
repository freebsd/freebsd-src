/* s_cbrtf.c -- float version of s_cbrt.c.
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

/* cbrtf(x)
 * Return cube root of x
 */
static const unsigned
	B1 = 709958130, /* B1 = (127-127.0/3-0.03306235651)*2**23 */
	B2 = 642849266; /* B2 = (127-127.0/3-24/3-0.03306235651)*2**23 */

/* |1/cbrt(x) - p(x)| < 2**-14.5 (~[-4.37e-4, 4.366e-5]). */
static const float
P0 =  1.5586718321,			/*  0x3fc7828f */
P1 = -0.78271341324,			/* -0xbf485fe8 */
P2 =  0.22403796017;			/*  0x3e656a35 */

float
cbrtf(float x)
{
	float r,s,t,w;
	int32_t hx;
	u_int32_t sign;
	u_int32_t high;

	GET_FLOAT_WORD(hx,x);
	sign=hx&0x80000000; 		/* sign= sign(x) */
	hx  ^=sign;
	if(hx>=0x7f800000) return(x+x); /* cbrt(NaN,INF) is itself */
	if(hx==0)
	    return(x);			/* cbrt(0) is itself */

    /* rough cbrt to 5 bits */
	if(hx<0x00800000) { 		/* subnormal number */
	    SET_FLOAT_WORD(t,0x4b800000); /* set t= 2**24 */
	    t*=x;
	    GET_FLOAT_WORD(high,t);
	    SET_FLOAT_WORD(t,sign|((high&0x7fffffff)/3+B2));
	} else
	    SET_FLOAT_WORD(t,sign|(hx/3+B1));

    /* new cbrt to 14 bits */
	r=(t*t)*(t/x);
	t=t*((P0+r*P1)+(r*r)*P2);

    /*
     * Round t away from zero to 12 bits (sloppily except for ensuring that
     * the result is larger in magnitude than cbrt(x) but not much more than
     * 1 12-bit ulp larger).  With rounding towards zero, the error bound
     * would be ~5/6 instead of ~4/6, and with t 2 12-bit ulps larger the
     * infinite-precision error in the Newton approximation would affect
     * the second digit instead of the third digit of 4/6 = 0.666..., etc.
     */
	GET_FLOAT_WORD(high,t);
	SET_FLOAT_WORD(t,(high+0x1800)&0xfffff000);

    /* one step Newton iteration to 24 bits with error < 0.669 ulps */
	s=t*t;				/* t*t is exact */
	r=x/s;				/* error <= 0.5 ulps; |r| < |t| */
	w=t+t;				/* t+t is exact */
	r=(r-t)/(w+r);			/* r-t is exact; w+r ~= 3*t */
	t=t+t*r;			/* error <= 0.5 + 0.5/3 + epsilon */

	return(t);
}
