/* s_erff.c -- float version of s_erf.c.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "math.h"
#include "math_private.h"

static const float
tiny	    = 1e-30,
half=  5.0000000000e-01, /* 0x3F000000 */
one =  1.0000000000e+00, /* 0x3F800000 */
two =  2.0000000000e+00, /* 0x40000000 */
/*
 * Coefficients for approximation to erf on [0,0.84375]
 */
efx =  1.2837916613e-01, /* 0x3e0375d4 */
efx8=  1.0270333290e+00, /* 0x3f8375d4 */
/*
 *  Domain [0, 0.84375], range ~[-5.4446e-10,5.5197e-10]:
 *  |(erf(x) - x)/x - p(x)/q(x)| < 2**-31.
 */
pp0  =  1.28379166e-01F, /*  0x1.06eba8p-3 */
pp1  = -3.36030394e-01F, /* -0x1.58185ap-2 */
pp2  = -1.86260219e-03F, /* -0x1.e8451ep-10 */
qq1  =  3.12324286e-01F, /*  0x1.3fd1f0p-2 */
qq2  =  2.16070302e-02F, /*  0x1.620274p-6 */
qq3  = -1.98859419e-03F, /* -0x1.04a626p-9 */
/*
 * Domain [0.84375, 1.25], range ~[-1.953e-11,1.940e-11]:
 * |(erf(x) - erx) - p(x)/q(x)| < 2**-36.
 */
erx  =  8.42697144e-01F, /*  0x1.af7600p-1.  erf(1) rounded to 16 bits. */
pa0  =  3.64939137e-06F, /*  0x1.e9d022p-19 */
pa1  =  4.15109694e-01F, /*  0x1.a91284p-2 */
pa2  = -1.65179938e-01F, /* -0x1.5249dcp-3 */
pa3  =  1.10914491e-01F, /*  0x1.c64e46p-4 */
qa1  =  6.02074385e-01F, /*  0x1.344318p-1 */
qa2  =  5.35934687e-01F, /*  0x1.126608p-1 */
qa3  =  1.68576106e-01F, /*  0x1.593e6ep-3 */
qa4  =  5.62181212e-02F, /*  0x1.cc89f2p-5 */
/*
 * Domain [1.25,1/0.35], range ~[-7.043e-10,7.457e-10]:
 * |log(x*erfc(x)) + x**2 + 0.5625 - r(x)/s(x)| < 2**-30
 */
ra0  = -9.87132732e-03F, /* -0x1.4376b2p-7 */
ra1  = -5.53605914e-01F, /* -0x1.1b723cp-1 */
ra2  = -2.17589188e+00F, /* -0x1.1683a0p+1 */
ra3  = -1.43268085e+00F, /* -0x1.6ec42cp+0 */
sa1  =  5.45995426e+00F, /*  0x1.5d6fe4p+2 */
sa2  =  6.69798088e+00F, /*  0x1.acabb8p+2 */
sa3  =  1.43113089e+00F, /*  0x1.6e5e98p+0 */
sa4  = -5.77397496e-02F, /* -0x1.d90108p-5 */
/*
 * Domain [1/0.35, 11], range ~[-2.264e-13,2.336e-13]:
 * |log(x*erfc(x)) + x**2 + 0.5625 - r(x)/s(x)| < 2**-42
 */
rb0  = -9.86494310e-03F, /* -0x1.434124p-7 */
rb1  = -6.25171244e-01F, /* -0x1.401672p-1 */
rb2  = -6.16498327e+00F, /* -0x1.8a8f16p+2 */
rb3  = -1.66696873e+01F, /* -0x1.0ab70ap+4 */
rb4  = -9.53764343e+00F, /* -0x1.313460p+3 */
sb1  =  1.26884899e+01F, /*  0x1.96081cp+3 */
sb2  =  4.51839523e+01F, /*  0x1.6978bcp+5 */
sb3  =  4.72810211e+01F, /*  0x1.7a3f88p+5 */
sb4  =  8.93033314e+00F; /*  0x1.1dc54ap+3 */

float
erff(float x)
{
	int32_t hx,ix,i;
	float R,S,P,Q,s,y,z,r;
	GET_FLOAT_WORD(hx,x);
	ix = hx&0x7fffffff;
	if(ix>=0x7f800000) {		/* erf(nan)=nan */
	    i = ((u_int32_t)hx>>31)<<1;
	    return (float)(1-i)+one/x;	/* erf(+-inf)=+-1 */
	}

	if(ix < 0x3f580000) {		/* |x|<0.84375 */
	    if(ix < 0x38800000) { 	/* |x|<2**-14 */
	        if (ix < 0x04000000)	/* |x|<0x1p-119 */
		    return (8*x+efx8*x)/8;	/* avoid spurious underflow */
		return x + efx*x;
	    }
	    z = x*x;
	    r = pp0+z*(pp1+z*pp2);
	    s = one+z*(qq1+z*(qq2+z*qq3));
	    y = r/s;
	    return x + x*y;
	}
	if(ix < 0x3fa00000) {		/* 0.84375 <= |x| < 1.25 */
	    s = fabsf(x)-one;
	    P = pa0+s*(pa1+s*(pa2+s*pa3));
	    Q = one+s*(qa1+s*(qa2+s*(qa3+s*qa4)));
	    if(hx>=0) return erx + P/Q; else return -erx - P/Q;
	}
	if (ix >= 0x40800000) {		/* inf>|x|>=4 */
	    if(hx>=0) return one-tiny; else return tiny-one;
	}
	x = fabsf(x);
 	s = one/(x*x);
	if(ix< 0x4036DB6E) {	/* |x| < 1/0.35 */
	    R=ra0+s*(ra1+s*(ra2+s*ra3));
	    S=one+s*(sa1+s*(sa2+s*(sa3+s*sa4)));
	} else {	/* |x| >= 1/0.35 */
	    R=rb0+s*(rb1+s*(rb2+s*(rb3+s*rb4)));
	    S=one+s*(sb1+s*(sb2+s*(sb3+s*sb4)));
	}
	SET_FLOAT_WORD(z,hx&0xffffe000);
	r  = expf(-z*z-0.5625F)*expf((z-x)*(z+x)+R/S);
	if(hx>=0) return one-r/x; else return  r/x-one;
}

float
erfcf(float x)
{
	int32_t hx,ix;
	float R,S,P,Q,s,y,z,r;
	GET_FLOAT_WORD(hx,x);
	ix = hx&0x7fffffff;
	if(ix>=0x7f800000) {			/* erfc(nan)=nan */
						/* erfc(+-inf)=0,2 */
	    return (float)(((u_int32_t)hx>>31)<<1)+one/x;
	}

	if(ix < 0x3f580000) {		/* |x|<0.84375 */
	    if(ix < 0x33800000)  	/* |x|<2**-24 */
		return one-x;
	    z = x*x;
	    r = pp0+z*(pp1+z*pp2);
	    s = one+z*(qq1+z*(qq2+z*qq3));
	    y = r/s;
	    if(hx < 0x3e800000) {  	/* x<1/4 */
		return one-(x+x*y);
	    } else {
		r = x*y;
		r += (x-half);
	        return half - r ;
	    }
	}
	if(ix < 0x3fa00000) {		/* 0.84375 <= |x| < 1.25 */
	    s = fabsf(x)-one;
	    P = pa0+s*(pa1+s*(pa2+s*pa3));
	    Q = one+s*(qa1+s*(qa2+s*(qa3+s*qa4)));
	    if(hx>=0) {
	        z  = one-erx; return z - P/Q;
	    } else {
		z = erx+P/Q; return one+z;
	    }
	}
	if (ix < 0x41300000) {		/* |x|<11 */
	    x = fabsf(x);
 	    s = one/(x*x);
	    if(ix< 0x4036DB6D) {	/* |x| < 1/.35 ~ 2.857143*/
	        R=ra0+s*(ra1+s*(ra2+s*ra3));
	        S=one+s*(sa1+s*(sa2+s*(sa3+s*sa4)));
	    } else {			/* |x| >= 1/.35 ~ 2.857143 */
		if(hx<0&&ix>=0x40a00000) return two-tiny;/* x < -5 */
	        R=rb0+s*(rb1+s*(rb2+s*(rb3+s*rb4)));
		S=one+s*(sb1+s*(sb2+s*(sb3+s*sb4)));
	    }
	    SET_FLOAT_WORD(z,hx&0xffffe000);
	    r  = expf(-z*z-0.5625F)*expf((z-x)*(z+x)+R/S);
	    if(hx>0) return r/x; else return two-r/x;
	} else {
	    if(hx>0) return tiny*tiny; else return two-tiny;
	}
}
