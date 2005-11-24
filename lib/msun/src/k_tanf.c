/* k_tanf.c -- float version of k_tan.c
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 */

/*
 * ====================================================
 * Copyright 2004 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

#ifndef INLINE_KERNEL_TANF
#ifndef lint
static char rcsid[] = "$FreeBSD$";
#endif
#endif

#include "math.h"
#include "math_private.h"

/* |tan(x)/x - t(x)| < 2**-25.5 (~[-2e-08, 2e-08]). */
static const double
T[] =  {
  0x15554d3418c99f.0p-54,	/* 0.333331395030791399758 */
  0x1112fd38999f72.0p-55,	/* 0.133392002712976742718 */
  0x1b54c91d865afe.0p-57,	/* 0.0533812378445670393523 */
  0x191df3908c33ce.0p-58,	/* 0.0245283181166547278873 */
  0x185dadfcecf44e.0p-61,	/* 0.00297435743359967304927 */
  0x1362b9bf971bcd.0p-59,	/* 0.00946564784943673166728 */
};

#ifdef INLINE_KERNEL_TANF
extern inline
#endif
float
__kernel_tandf(double x, int iy)
{
	double z,r,w,s;

	z	=  x*x;
	w 	=  z*z;
    /* Break x^5*(T[1]+x^2*T[2]+...) into
     *	  x^5*(T[1]+x^4*T[3]+x^8*T[5]) +
     *	  x^5*(x^2*(T[2]+x^4*T[4]))
     */
	r = T[1]+w*(T[3]+w*T[5]) + z*(T[2]+w*T[4]);
	s = z*x;
	r = (x+s*T[0])+(s*z)*r;
	if(iy==1) return r;
	else return -1.0/r;
}
