/*
 * Copyright (c) 1985, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)sinh.c	8.1 (Berkeley) 6/4/93";
#endif /* not lint */

/* SINH(X)
 * RETURN THE HYPERBOLIC SINE OF X
 * DOUBLE PRECISION (VAX D format 56 bits, IEEE DOUBLE 53 BITS)
 * CODED IN C BY K.C. NG, 1/8/85;
 * REVISED BY K.C. NG on 2/8/85, 3/7/85, 3/24/85, 4/16/85.
 *
 * Required system supported functions :
 *	copysign(x,y)
 *	scalb(x,N)
 *
 * Required kernel functions:
 *	expm1(x)	...return exp(x)-1
 *
 * Method :
 *	1. reduce x to non-negative by sinh(-x) = - sinh(x).
 *	2.
 *
 *	                                      expm1(x) + expm1(x)/(expm1(x)+1)
 *	    0 <= x <= lnovfl     : sinh(x) := --------------------------------
 *			       		                      2
 *     lnovfl <= x <= lnovfl+ln2 : sinh(x) := expm1(x)/2 (avoid overflow)
 * lnovfl+ln2 <  x <  INF        :  overflow to INF
 *
 *
 * Special cases:
 *	sinh(x) is x if x is +INF, -INF, or NaN.
 *	only sinh(0)=0 is exact for finite argument.
 *
 * Accuracy:
 *	sinh(x) returns the exact hyperbolic sine of x nearly rounded. In
 *	a test run with 1,024,000 random arguments on a VAX, the maximum
 *	observed error was 1.93 ulps (units in the last place).
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following constants.
 * The decimal values may be used, provided that the compiler will convert
 * from decimal to binary accurately enough to produce the hexadecimal values
 * shown.
 */

#include "mathimpl.h"

vc(mln2hi, 8.8029691931113054792E1   ,0f33,43b0,2bdb,c7e2,   7, .B00F33C7E22BDB)
vc(mln2lo,-4.9650192275318476525E-16 ,1b60,a70f,582a,279e, -50,-.8F1B60279E582A)
vc(lnovfl, 8.8029691931113053016E1   ,0f33,43b0,2bda,c7e2,   7, .B00F33C7E22BDA)

ic(mln2hi, 7.0978271289338397310E2,    10, 1.62E42FEFA39EF)
ic(mln2lo, 2.3747039373786107478E-14, -45, 1.ABC9E3B39803F)
ic(lnovfl, 7.0978271289338397310E2,     9, 1.62E42FEFA39EF)

#ifdef vccast
#define	mln2hi	vccast(mln2hi)
#define	mln2lo	vccast(mln2lo)
#define	lnovfl	vccast(lnovfl)
#endif

#if defined(vax)||defined(tahoe)
static max = 126                      ;
#else	/* defined(vax)||defined(tahoe) */
static max = 1023                     ;
#endif	/* defined(vax)||defined(tahoe) */


double sinh(x)
double x;
{
	static const double  one=1.0, half=1.0/2.0 ;
	double t, sign;
#if !defined(vax)&&!defined(tahoe)
	if(x!=x) return(x);	/* x is NaN */
#endif	/* !defined(vax)&&!defined(tahoe) */
	sign=copysign(one,x);
	x=copysign(x,one);
	if(x<lnovfl)
	    {t=expm1(x); return(copysign((t+t/(one+t))*half,sign));}

	else if(x <= lnovfl+0.7)
		/* subtract x by ln(2^(max+1)) and return 2^max*exp(x)
	    		to avoid unnecessary overflow */
	    return(copysign(scalb(one+expm1((x-mln2hi)-mln2lo),max),sign));

	else  /* sinh(+-INF) = +-INF, sinh(+-big no.) overflow to +-INF */
	    return( expm1(x)*sign );
}
