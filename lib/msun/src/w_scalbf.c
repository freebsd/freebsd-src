/* w_scalbf.c -- float version of w_scalb.c.
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
static char rcsid[] = "$FreeBSD: src/lib/msun/src/w_scalbf.c,v 1.5 1999/08/28 00:07:08 peter Exp $";
#endif

/*
 * wrapper scalbf(float x, float fn) is provide for
 * passing various standard test suite. One
 * should use scalbn() instead.
 */

#include "math.h"
#include "math_private.h"

#include <errno.h>

#ifdef __STDC__
#ifdef _SCALB_INT
	float scalbf(float x, int fn)		/* wrapper scalbf */
#else
	float scalbf(float x, float fn)		/* wrapper scalbf */
#endif
#else
	float scalbf(x,fn)			/* wrapper scalbf */
#ifdef _SCALB_INT
	float x; int fn;
#else
	float x,fn;
#endif
#endif
{
#ifdef _IEEE_LIBM
	return __ieee754_scalbf(x,fn);
#else
	float z;
	z = __ieee754_scalbf(x,fn);
	if(_LIB_VERSION == _IEEE_) return z;
	if(!(finitef(z)||isnanf(z))&&finitef(x)) {
	    /* scalbf overflow */
	    return (float)__kernel_standard((double)x,(double)fn,132);
	}
	if(z==(float)0.0&&z!=x) {
	    /* scalbf underflow */
	    return (float)__kernel_standard((double)x,(double)fn,133);
	}
#ifndef _SCALB_INT
	if(!finitef(fn)) errno = ERANGE;
#endif
	return z;
#endif
}
