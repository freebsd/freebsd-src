/* k_tanf.c -- float version of k_tan.c
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 */

/*
 * ====================================================
 * Copyright 2004 Sun Microsystems, Inc.  All Rights Reserved.
 *
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

static const float
pio4  =  7.8539812565e-01, /* 0x3f490fda */
pio4lo=  3.7748947079e-08, /* 0x33222168 */
/* |tan(x)/x - t(x)| < 2**-29.2 (~[-1.73e-09, 1.724e-09]). */
T[] =  {
  0xaaaaa3.0p-25,		/* 0.33333310485 */
  0x888b06.0p-26,		/* 0.13334283238 */
  0xdc84c8.0p-28,		/* 0.053837567568 */
  0xb9d8f1.0p-29,		/* 0.022686453536 */
  0xcfe632.0p-31,		/* 0.0063445800915 */
  0xeaf97e.0p-31,		/* 0.0071708550677 */
};

float
__kernel_tanf(float x, float y, int iy)
{
	float z,r,v,w,s;
	int32_t ix,hx;

	GET_FLOAT_WORD(hx,x);
	ix = hx&0x7fffffff;
	if(ix>=0x3f2ca140) { 			/* |x|>=0.67434 */
	    if(hx<0) {x = -x; y = -y;}
	    z = pio4-x;
	    w = pio4lo-y;
	    x = z+w; y = 0.0;
	}
	z	=  x*x;
	w 	=  z*z;
    /* Break x^5*(T[1]+x^2*T[2]+...) into
     *	  x^5*(T[1]+x^4*T[3]+x^8*T[5]) +
     *	  x^5*(x^2*(T[2]+x^4*T[4]))
     */
	r = T[1]+w*(T[3]+w*T[5]);
	v = z*(T[2]+w*T[4]);
	s = z*x;
	r = y + z*(s*(r+v)+y);
	r += T[0]*s;
	w = x+r;
	if(ix>=0x3f2ca140) {
	    v = (float)iy;
	    return (float)(1-((hx>>30)&2))*(v-(float)2.0*(x-(w*w/(w+v)-r)));
	}
	if(iy==1) return w;
	else return -1.0/((double)x+r);
}
