/* @(#)s_isnan.c 5.1 93/09/24 */
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
static char rcsid[] = "$Id: s_isnan.c,v 1.1.1.1 1994/05/06 00:20:05 gclarkii Exp $";
#endif

/*
 * isnan(x) returns 1 is x is nan, else 0;
 * no branching!
 */

#include "math.h"
#include <machine/endian.h>

#if BYTE_ORDER == LITTLE_ENDIAN
#define n0	1
#else
#define n0	0
#endif

#ifdef __STDC__
static const double one = 1.0;
#else
static double one = 1.0;
#endif

#ifdef __STDC__
	int isnan(double x)
#else
	int isnan(x)
	double x;
#endif
{
	int hx,lx;
	hx = (*(n0+(int*)&x)&0x7fffffff);
	lx = *(1-n0+(int*)&x);
	hx |= (unsigned)(lx|(-lx))>>31;	
	hx = 0x7ff00000 - hx;
	return ((unsigned)(hx))>>31;
}
