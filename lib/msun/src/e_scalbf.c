/* e_scalbf.c -- float version of e_scalb.c.
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
static char rcsid[] = "$FreeBSD: src/lib/msun/src/e_scalbf.c,v 1.5 1999/08/28 00:06:38 peter Exp $";
#endif

#include "math.h"
#include "math_private.h"

#ifdef _SCALB_INT
#ifdef __STDC__
	float __ieee754_scalbf(float x, int fn)
#else
	float __ieee754_scalbf(x,fn)
	float x; int fn;
#endif
#else
#ifdef __STDC__
	float __ieee754_scalbf(float x, float fn)
#else
	float __ieee754_scalbf(x,fn)
	float x, fn;
#endif
#endif
{
#ifdef _SCALB_INT
	return scalbnf(x,fn);
#else
	if (isnanf(x)||isnanf(fn)) return x*fn;
	if (!finitef(fn)) {
	    if(fn>(float)0.0) return x*fn;
	    else       return x/(-fn);
	}
	if (rintf(fn)!=fn) return (fn-fn)/(fn-fn);
	if ( fn > (float)65000.0) return scalbnf(x, 65000);
	if (-fn > (float)65000.0) return scalbnf(x,-65000);
	return scalbnf(x,(int)fn);
#endif
}
