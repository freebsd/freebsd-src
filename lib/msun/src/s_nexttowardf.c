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

#include <float.h>

#include "fpmath.h"
#include "math.h"
#include "math_private.h"

#define	LDBL_INFNAN_EXP	(LDBL_MAX_EXP * 2 - 1)

float
nexttowardf(float x, long double y)
{
	union IEEEl2bits uy;
	volatile float t;
	int32_t hx,ix;

	if(isnan(x) || isnan(y))
	   return x+y;	/* x or y is nan */
	if(x==y) return (float)y;		/* x=y, return y */

	GET_FLOAT_WORD(hx,x);
	ix = hx&0x7fffffff;		/* |x| */
	uy.e = y;

	if(ix==0) {				/* x == 0 */
	    SET_FLOAT_WORD(x,(uy.bits.sign<<31)|1);/* return +-minsubnormal */
	    t = x*x;
	    if(t==x) return t; else return x;	/* raise underflow flag */
	}
	if(hx>=0 ^ x < y)			/* x -= ulp */
	    hx -= 1;
	else					/* x += ulp */
	    hx += 1;
	ix = hx&0x7f800000;
	if(ix>=0x7f800000) return x+x;	/* overflow  */
	if(ix<0x00800000) {		/* underflow */
	    t = x*x;
	    if(t!=x) {		/* raise underflow flag */
	        SET_FLOAT_WORD(x,hx);
		return x;
	    }
	}
	SET_FLOAT_WORD(x,hx);
	return x;
}
