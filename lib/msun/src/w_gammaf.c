/* w_gammaf.c -- float version of w_gamma.c.
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
static char rcsid[] = "$FreeBSD: src/lib/msun/src/w_gammaf.c,v 1.5 1999/08/28 00:07:02 peter Exp $";
#endif

#include "math.h"
#include "math_private.h"

extern int signgam;

#ifdef __STDC__
	float gammaf(float x)
#else
	float gammaf(x)
	float x;
#endif
{
#ifdef _IEEE_LIBM
	return __ieee754_gammaf_r(x,&signgam);
#else
        float y;
        y = __ieee754_gammaf_r(x,&signgam);
        if(_LIB_VERSION == _IEEE_) return y;
        if(!finitef(y)&&finitef(x)) {
            if(floorf(x)==x&&x<=(float)0.0)
	        /* gammaf pole */
                return (float)__kernel_standard((double)x,(double)x,141);
            else
	        /* gammaf overflow */
                return (float)__kernel_standard((double)x,(double)x,140);
        } else
            return y;
#endif
}
