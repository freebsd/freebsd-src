/* w_j1f.c -- float version of w_j1.c.
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
 * wrapper of j1f
 */

#include "math.h"
#include "math_private.h"

#ifdef __STDC__
	float j1f(float x)		/* wrapper j1f */
#else
	float j1f(x)			/* wrapper j1f */
	float x;
#endif
{
#ifdef _IEEE_LIBM
	return __ieee754_j1f(x);
#else
	float z;
	z = __ieee754_j1f(x);
	if(_LIB_VERSION == _IEEE_ || isnanf(x) ) return z;
	if(fabsf(x)>(float)X_TLOSS) {
		/* j1(|x|>X_TLOSS) */
	        return (float)__kernel_standard((double)x,(double)x,136);
	} else
	    return z;
#endif
}
