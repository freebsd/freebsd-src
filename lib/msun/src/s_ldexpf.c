/* s_ldexpf.c -- float version of s_ldexp.c.
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
static char rcsid[] = "s_ldexpf.c,v 1.2 1995/05/30 05:49:53 rgrimes Exp";
#endif

#include "math.h"
#include "math_private.h"
#include <errno.h>

#ifdef __STDC__
	float ldexpf(float value, int exp)
#else
	float ldexpf(value, exp)
	float value; int exp;
#endif
{
	if(!finitef(value)||value==(float)0.0) return value;
	value = scalbnf(value,exp);
	if(!finitef(value)||value==(float)0.0) errno = ERANGE;
	return value;
}
