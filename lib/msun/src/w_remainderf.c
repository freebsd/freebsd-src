/* w_remainderf.c -- float version of w_remainder.c.
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

/*
 * wrapper remainderf(x,p)
 */

#include "math.h"
#include "math_private.h"

float
remainderf(float x, float y)	/* wrapper remainder */
{
#ifdef _IEEE_LIBM
	return __ieee754_remainderf(x,y);
#else
	float z;
	z = __ieee754_remainderf(x,y);
	if(_LIB_VERSION == _IEEE_ || isnanf(y)) return z;
	if(y==(float)0.0)
	    /* remainder(x,0) */
	    return (float)__kernel_standard((double)x,(double)y,128);
	else
	    return z;
#endif
}
