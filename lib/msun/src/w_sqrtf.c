/* w_sqrtf.c -- float version of w_sqrt.c.
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
static char rcsid[] = "$FreeBSD: src/lib/msun/src/w_sqrtf.c,v 1.5 1999/08/28 00:07:09 peter Exp $";
#endif

/*
 * wrapper sqrtf(x)
 */

#include "math.h"
#include "math_private.h"

#ifdef __STDC__
	float sqrtf(float x)		/* wrapper sqrtf */
#else
	float sqrt(x)			/* wrapper sqrtf */
	float x;
#endif
{
#ifdef _IEEE_LIBM
	return __ieee754_sqrtf(x);
#else
	float z;
	z = __ieee754_sqrtf(x);
	if(_LIB_VERSION == _IEEE_ || isnanf(x)) return z;
	if(x<(float)0.0) {
	    /* sqrtf(negative) */
	    return (float)__kernel_standard((double)x,(double)x,126);
	} else
	    return z;
#endif
}
