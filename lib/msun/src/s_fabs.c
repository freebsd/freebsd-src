/* @(#)s_fabs.c 5.1 93/09/24 */
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
static char rcsid[] = "$Id: s_fabs.c,v 1.1.1.1 1994/05/06 00:20:04 gclarkii Exp $";
#endif

/*
 * fabs(x) returns the absolute value of x.
 */

#include "math.h"

#ifdef __STDC__
static const double one = 1.0;
#else
static double one = 1.0;
#endif

#ifdef __STDC__
	double fabs(double x)
#else
	double fabs(x)
	double x;
#endif
{
	*((((*(int*)&one)>>29)^1)+(int*)&x) &= 0x7fffffff;
        return x;
}
