/*
 * Copyright (c) 1985 Regents of the University of California.
 * All rights reserved.
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
static char sccsid[] = "@(#)exp.c	5.7 (Berkeley) 12/2/92";
#endif /* not lint */

/* EXP(X)
 * RETURN THE EXPONENTIAL OF X
 * DOUBLE PRECISION (IEEE 53 bits, VAX D FORMAT 56 BITS)
 * CODED IN C BY K.C. NG, 1/19/85; 
 * REVISED BY K.C. NG on 2/6/85, 2/15/85, 3/7/85, 3/24/85, 4/16/85, 6/14/86.
 *
 * Required system supported functions:
 *	scalb(x,n)	
 *	copysign(x,y)	
 *	finite(x)
 *
 * Method:
 *	1. Argument Reduction: given the input x, find r and integer k such 
 *	   that
 *	                   x = k*ln2 + r,  |r| <= 0.5*ln2 .  
 *	   r will be represented as r := z+c for better accuracy.
 *
 *	2. Compute exp(r) by 
 *
 *		exp(r) = 1 + r + r*R1/(2-R1),
 *	   where
 *		R1 = x - x^2*(p1+x^2*(p2+x^2*(p3+x^2*(p4+p5*x^2)))).
 *
 *	3. exp(x) = 2^k * exp(r) .
 *
 * Special cases:
 *	exp(INF) is INF, exp(NaN) is NaN;
 *	exp(-INF)=  0;
 *	for finite argument, only exp(0)=1 is exact.
 *
 * Accuracy:
 *	exp(x) returns the exponential of x nearly rounded. In a test run
 *	with 1,156,000 random arguments on a VAX, the maximum observed
 *	error was 0.869 ulps (units in the last place).
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following constants.
 * The decimal values may be used, provided that the compiler will convert
 * from decimal to binary accurately enough to produce the hexadecimal values
 * shown.
 */

#include "mathimpl.h"

vc(ln2hi,  6.9314718055829871446E-1  ,7217,4031,0000,f7d0,   0, .B17217F7D00000)
vc(ln2lo,  1.6465949582897081279E-12 ,bcd5,2ce7,d9cc,e4f1, -39, .E7BCD5E4F1D9CC)
vc(lnhuge, 9.4961163736712506989E1   ,ec1d,43bd,9010,a73e,   7, .BDEC1DA73E9010)
vc(lntiny,-9.5654310917272452386E1   ,4f01,c3bf,33af,d72e,   7,-.BF4F01D72E33AF)
vc(invln2, 1.4426950408889634148E0   ,aa3b,40b8,17f1,295c,   1, .B8AA3B295C17F1)
vc(p1,     1.6666666666666602251E-1  ,aaaa,3f2a,a9f1,aaaa,  -2, .AAAAAAAAAAA9F1)
vc(p2,    -2.7777777777015591216E-3  ,0b60,bc36,ec94,b5f5,  -8,-.B60B60B5F5EC94)
vc(p3,     6.6137563214379341918E-5  ,b355,398a,f15f,792e, -13, .8AB355792EF15F)
vc(p4,    -1.6533902205465250480E-6  ,ea0e,b6dd,5f84,2e93, -19,-.DDEA0E2E935F84)
vc(p5,     4.1381367970572387085E-8  ,bb4b,3431,2683,95f5, -24, .B1BB4B95F52683)

#ifdef vccast
#define    ln2hi    vccast(ln2hi)
#define    ln2lo    vccast(ln2lo)
#define   lnhuge    vccast(lnhuge)
#define   lntiny    vccast(lntiny)
#define   invln2    vccast(invln2)
#define       p1    vccast(p1)
#define       p2    vccast(p2)
#define       p3    vccast(p3)
#define       p4    vccast(p4)
#define       p5    vccast(p5)
#endif

ic(p1,     1.6666666666666601904E-1,  -3,  1.555555555553E)
ic(p2,    -2.7777777777015593384E-3,  -9, -1.6C16C16BEBD93)
ic(p3,     6.6137563214379343612E-5, -14,  1.1566AAF25DE2C)
ic(p4,    -1.6533902205465251539E-6, -20, -1.BBD41C5D26BF1)
ic(p5,     4.1381367970572384604E-8, -25,  1.6376972BEA4D0)
ic(ln2hi,  6.9314718036912381649E-1,  -1,  1.62E42FEE00000)
ic(ln2lo,  1.9082149292705877000E-10,-33,  1.A39EF35793C76)
ic(lnhuge, 7.1602103751842355450E2,    9,  1.6602B15B7ECF2)
ic(lntiny,-7.5137154372698068983E2,    9, -1.77AF8EBEAE354)
ic(invln2, 1.4426950408889633870E0,    0,  1.71547652B82FE)

double exp(x)
double x;
{
	double  z,hi,lo,c;
	int k;

#if !defined(vax)&&!defined(tahoe)
	if(x!=x) return(x);	/* x is NaN */
#endif	/* !defined(vax)&&!defined(tahoe) */
	if( x <= lnhuge ) {
		if( x >= lntiny ) {

		    /* argument reduction : x --> x - k*ln2 */

			k=invln2*x+copysign(0.5,x);	/* k=NINT(x/ln2) */

		    /* express x-k*ln2 as hi-lo and let x=hi-lo rounded */

			hi=x-k*ln2hi;
			x=hi-(lo=k*ln2lo);

		    /* return 2^k*[1+x+x*c/(2+c)]  */
			z=x*x;
			c= x - z*(p1+z*(p2+z*(p3+z*(p4+z*p5))));
			return  scalb(1.0+(hi-(lo-(x*c)/(2.0-c))),k);

		}
		/* end of x > lntiny */

		else 
		     /* exp(-big#) underflows to zero */
		     if(finite(x))  return(scalb(1.0,-5000));

		     /* exp(-INF) is zero */
		     else return(0.0);
	}
	/* end of x < lnhuge */

	else 
	/* exp(INF) is INF, exp(+big#) overflows to INF */
	    return( finite(x) ?  scalb(1.0,5000)  : x);
}

/* returns exp(r = x + c) for |c| < |x| with no overlap.  */

double exp__D(x, c)
double x, c;
{
	double  z,hi,lo, t;
	int k;

#if !defined(vax)&&!defined(tahoe)
	if (x!=x) return(x);	/* x is NaN */
#endif	/* !defined(vax)&&!defined(tahoe) */
	if ( x <= lnhuge ) {
		if ( x >= lntiny ) {

		    /* argument reduction : x --> x - k*ln2 */
			z = invln2*x;
			k = z + copysign(.5, x);

		    /* express (x+c)-k*ln2 as hi-lo and let x=hi-lo rounded */

			hi=(x-k*ln2hi);			/* Exact. */
			x= hi - (lo = k*ln2lo-c);
		    /* return 2^k*[1+x+x*c/(2+c)]  */
			z=x*x;
			c= x - z*(p1+z*(p2+z*(p3+z*(p4+z*p5))));
			c = (x*c)/(2.0-c);

			return  scalb(1.+(hi-(lo - c)), k);
		}
		/* end of x > lntiny */

		else 
		     /* exp(-big#) underflows to zero */
		     if(finite(x))  return(scalb(1.0,-5000));

		     /* exp(-INF) is zero */
		     else return(0.0);
	}
	/* end of x < lnhuge */

	else 
	/* exp(INF) is INF, exp(+big#) overflows to INF */
	    return( finite(x) ?  scalb(1.0,5000)  : x);
}
