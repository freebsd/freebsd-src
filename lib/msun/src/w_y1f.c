/* w_y1f.c -- float version of w_y1.c.
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
static char rcsid[] = "$FreeBSD: src/lib/msun/src/w_y1f.c,v 1.3 1999/08/28 00:07:10 peter Exp $";
#endif

/*
 * wrapper of y1f
 */

#include "math.h"
#include "math_private.h"

#ifdef __STDC__
	float y1f(float x)		/* wrapper y1f */
#else
	float y1f(x)			/* wrapper y1f */
	float x;
#endif
{
#ifdef _IEEE_LIBM
	return __ieee754_y1f(x);
#else
	float z;
	z = __ieee754_y1f(x);
	if(_LIB_VERSION == _IEEE_ || isnanf(x) ) return z;
        if(x <= (float)0.0){
                if(x==(float)0.0)
                    /* d= -one/(x-x); */
                    return (float)__kernel_standard((double)x,(double)x,110);
                else
                    /* d = zero/(x-x); */
                    return (float)__kernel_standard((double)x,(double)x,111);
        }
	if(x>(float)X_TLOSS) {
		/* y1(x>X_TLOSS) */
	        return (float)__kernel_standard((double)x,(double)x,137);
	} else
	    return z;
#endif
}
