/* w_j0f.c -- float version of w_j0.c.
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
static char rcsid[] = "$FreeBSD: src/lib/msun/src/w_j0f.c,v 1.6 1999/08/28 00:07:03 peter Exp $";
#endif

/*
 * wrapper j0f(float x)
 */

#include "math.h"
#include "math_private.h"

#ifdef __STDC__
	float j0f(float x)		/* wrapper j0f */
#else
	float j0f(x)			/* wrapper j0f */
	float x;
#endif
{
#ifdef _IEEE_LIBM
	return __ieee754_j0f(x);
#else
	float z = __ieee754_j0f(x);
	if(_LIB_VERSION == _IEEE_ || isnanf(x)) return z;
	if(fabsf(x)>(float)X_TLOSS) {
		/* j0f(|x|>X_TLOSS) */
	        return (float)__kernel_standard((double)x,(double)x,134);
	} else
	    return z;
#endif
}
