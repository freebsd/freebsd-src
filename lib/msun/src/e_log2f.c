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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "math.h"
#include "math_private.h"
#include "k_logf.h"

static const float
two25      =  3.3554432000e+07, /* 0x4c000000 */
ivln2hi    =  0x1.716p+0f,
ivln2lo    = -0x1.7135a8fa03d11p-13;

static const float zero   =  0.0;

float
__ieee754_log2f(float x)
{
	float f,hi,lo;
	int32_t i,k,hx;

	GET_FLOAT_WORD(hx,x);

        k=0;
        if (hx < 0x00800000) {                  /* x < 2**-126  */
            if ((hx&0x7fffffff)==0)
                return -two25/zero;             /* log(+-0)=-inf */
            if (hx<0) return (x-x)/zero;        /* log(-#) = NaN */
            k -= 25; x *= two25; /* subnormal number, scale up x */
	    GET_FLOAT_WORD(hx,x);
        }
	if (hx >= 0x7f800000) return x+x;
	k += (hx>>23)-127;
	hx &= 0x007fffff;
	i = (hx+(0x4afb0d))&0x800000;
	SET_FLOAT_WORD(x,hx|(i^0x3f800000));	/* normalize x or x/2 */
	k += (i>>23);
	f = __kernel_logf(x);
	x = x - 1;
	GET_FLOAT_WORD(hx,x);
	SET_FLOAT_WORD(hi,hx&0xfffff000);
	lo = x - hi;
	return (x+f)*ivln2lo + (lo+f)*ivln2hi + hi*ivln2hi + k;
}
