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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/* __kernel_logf(x)
 * Return log(x) - (x-1) for x in ~[sqrt(2)/2, sqrt(2)].
 */

static const float
/* |(log(1+s)-log(1-s))/s - Lg(s)| < 2**-34.24 (~[-4.95e-11, 4.97e-11]). */
Lg1 =      0xaaaaaa.0p-24,	/* 0.66666662693 */
Lg2 =      0xccce13.0p-25,	/* 0.40000972152 */
Lg3 =      0x91e9ee.0p-25,	/* 0.28498786688 */
Lg4 =      0xf89e26.0p-26;	/* 0.24279078841 */

static inline float
__kernel_logf(float x)
{
	float hfsq,f,s,z,R,w,t1,t2;
	int32_t ix,i,j;

	GET_FLOAT_WORD(ix,x);

	f = x-(float)1.0;
	if((0x007fffff&(0x8000+ix))<0xc000) {	/* -2**-9 <= f < 2**-9 */
	    if(f==0.0) return 0.0;
	    return f*f*((float)0.33333333333333333*f-(float)0.5);
	}
 	s = f/((float)2.0+f);
	z = s*s;
	ix &= 0x007fffff;
	i = ix-(0x6147a<<3);
	w = z*z;
	j = (0x6b851<<3)-ix;
	t1= w*(Lg2+w*Lg4);
	t2= z*(Lg1+w*Lg3);
	i |= j;
	R = t2+t1;
	if(i>0) {
	    hfsq=(float)0.5*f*f;
	    return s*(hfsq+R) - hfsq;
	} else {
	    return s*(R-f);
	}
}
