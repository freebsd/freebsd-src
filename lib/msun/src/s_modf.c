/* @(#)s_modf.c 5.1 93/09/24 */
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
static char rcsid[] = "$Id: s_modf.c,v 1.1.1.1 1994/05/06 00:20:06 gclarkii Exp $";
#endif

/*
 * modf(double x, double *iptr) 
 * return fraction part of x, and return x's integral part in *iptr.
 * Method:
 *	Bit twiddling.
 *
 * Exception:
 *	No exception.
 */

#include "math.h"
#include <machine/endian.h>

#if BYTE_ORDER == LITTLE_ENDIAN
#define n0	1
#define n1	0
#else
#define n0	0
#define n1	1
#endif

#ifdef __STDC__
static const double one = 1.0;
#else
static double one = 1.0;
#endif

#ifdef __STDC__
	double modf(double x, double *iptr)
#else
	double modf(x, iptr)
	double x,*iptr;
#endif
{
	int i0,i1,j0;
	unsigned i;
	i0 =  *(n0+(int*)&x);		/* high x */
	i1 =  *(n1+(int*)&x);		/* low  x */
	j0 = ((i0>>20)&0x7ff)-0x3ff;	/* exponent of x */
	if(j0<20) {			/* integer part in high x */
	    if(j0<0) {			/* |x|<1 */
		*(n0+(int*)iptr) = i0&0x80000000;
		*(n1+(int*)iptr) = 0;		/* *iptr = +-0 */
		return x;
	    } else {
		i = (0x000fffff)>>j0;
		if(((i0&i)|i1)==0) {		/* x is integral */
		    *iptr = x;
		    *(n0+(int*)&x) &= 0x80000000;
		    *(n1+(int*)&x)  = 0;	/* return +-0 */
		    return x;
		} else {
		    *(n0+(int*)iptr) = i0&(~i);
		    *(n1+(int*)iptr) = 0;
		    return x - *iptr;
		}
	    }
	} else if (j0>51) {		/* no fraction part */
	    *iptr = x*one;
	    *(n0+(int*)&x) &= 0x80000000;
	    *(n1+(int*)&x)  = 0;	/* return +-0 */
	    return x;
	} else {			/* fraction part in low x */
	    i = ((unsigned)(0xffffffff))>>(j0-20);
	    if((i1&i)==0) { 		/* x is integral */
		*iptr = x;
		*(n0+(int*)&x) &= 0x80000000;
		*(n1+(int*)&x)  = 0;	/* return +-0 */
		return x;
	    } else {
		*(n0+(int*)iptr) = i0;
		*(n1+(int*)iptr) = i1&(~i);
		return x - *iptr;
	    }
	}
}
