/* @(#)s_rint.c 5.1 93/09/24 */
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
static char rcsid[] = "$Id: s_rint.c,v 1.2 1995/05/30 05:50:18 rgrimes Exp $";
#endif

/*
 * rint(x)
 * Return x rounded to integral value according to the prevailing
 * rounding mode.
 * Method:
 *	Using floating addition.
 * Exception:
 *	Inexact flag raised if x not equal to rint(x).
 */

#include "math.h"
#include "math_private.h"

/*
 * TWO23 is long double instead of double to avoid a bug in gcc.  Without
 * this, gcc thinks that TWO23[sx]+x and w-TWO23[sx] already have double
 * precision and doesn't clip them to double precision when they are
 * assigned and returned.  Use long double even in the !__STDC__ case in
 * case this is compiled with gcc -traditional.
 */
#ifdef __STDC__
static const long double
#else
static long double
#endif
TWO52[2]={
  4.50359962737049600000e+15, /* 0x43300000, 0x00000000 */
 -4.50359962737049600000e+15, /* 0xC3300000, 0x00000000 */
};

#ifdef __STDC__
	double rint(double x)
#else
	double rint(x)
	double x;
#endif
{
	int32_t i0,j0,sx;
	u_int32_t i,i1;
	double w,t;
	EXTRACT_WORDS(i0,i1,x);
	sx = (i0>>31)&1;
	j0 = ((i0>>20)&0x7ff)-0x3ff;
	if(j0<20) {
	    if(j0<0) {
		if(((i0&0x7fffffff)|i1)==0) return x;
		i1 |= (i0&0x0fffff);
		i0 &= 0xfffe0000;
		i0 |= ((i1|-i1)>>12)&0x80000;
		SET_HIGH_WORD(x,i0);
	        w = TWO52[sx]+x;
	        t =  w-TWO52[sx];
		GET_HIGH_WORD(i0,t);
		SET_HIGH_WORD(t,(i0&0x7fffffff)|(sx<<31));
	        return t;
	    } else {
		i = (0x000fffff)>>j0;
		if(((i0&i)|i1)==0) return x; /* x is integral */
		i>>=1;
		if(((i0&i)|i1)!=0) {
		    if(j0==19) i1 = 0x40000000; else
		    i0 = (i0&(~i))|((0x20000)>>j0);
		}
	    }
	} else if (j0>51) {
	    if(j0==0x400) return x+x;	/* inf or NaN */
	    else return x;		/* x is integral */
	} else {
	    i = ((u_int32_t)(0xffffffff))>>(j0-20);
	    if((i1&i)==0) return x;	/* x is integral */
	    i>>=1;
	    if((i1&i)!=0) i1 = (i1&(~i))|((0x40000000)>>(j0-20));
	}
	INSERT_WORDS(x,i0,i1);
	w = TWO52[sx]+x;
	return w-TWO52[sx];
}
