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

#ifndef INLINE_KERNEL_TANF
#ifndef lint
static char rcsid[] = "$FreeBSD$";
#endif
#endif

#include "math.h"
#include "math_private.h"

static const double
pio4 = M_PI_4,
/* |tan(x)/x - t(x)| < 2**-29.1 (~[-1.72e-09, 1.719e-09]). */
T[] =  {
  0x1555545f8b54d0.0p-54,	/* 0.333333104424423432022 */
  0x111160cdc2c9af.0p-55,	/* 0.133342838734802765499 */
  0x1b9097e5693cd0.0p-57,	/* 0.0538375346701457369036 */
  0x173b2333895b6f.0p-58,	/* 0.0226865291791357691353 */
  0x19fcb197e825ab.0p-60,	/* 0.00634450313965243938713 */
  0x1d5f3701b44a27.0p-60,	/* 0.00717088210082520490646 */
};

#ifdef INLINE_KERNEL_TANF
extern inline
#endif
float
__kernel_tandf(double x, int iy)
{
	double z,r,v,w,s;
	int32_t ix,hx;

	GET_FLOAT_WORD(hx,x);
	ix = hx&0x7fffffff;
	if(ix>=0x3f2ca140) { 			/* |x|>=0.67434 */
	    if(hx<0) {x = -x;}
	    x = pio4-x;
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
	r = z*s*(r+v);
	r += T[0]*s;
	w = x+r;
	if(ix>=0x3f2ca140) {
	    v = (double)iy;
	    return (double)(1-((hx>>30)&2))*(v-2.0*(x-(w*w/(w+v)-r)));
	}
	if(iy==1) return w;
	else return -1.0/w;
}
