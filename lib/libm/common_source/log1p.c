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
static char sccsid[] = "@(#)log1p.c	8.1 (Berkeley) 6/4/93";
#endif /* not lint */

/* LOG1P(x) 
 * RETURN THE LOGARITHM OF 1+x
 * DOUBLE PRECISION (VAX D FORMAT 56 bits, IEEE DOUBLE 53 BITS)
 * CODED IN C BY K.C. NG, 1/19/85; 
 * REVISED BY K.C. NG on 2/6/85, 3/7/85, 3/24/85, 4/16/85.
 * 
 * Required system supported functions:
 *	scalb(x,n) 
 *	copysign(x,y)
 *	logb(x)	
 *	finite(x)
 *
 * Required kernel function:
 *	log__L(z)
 *
 * Method :
 *	1. Argument Reduction: find k and f such that 
 *			1+x  = 2^k * (1+f), 
 *	   where  sqrt(2)/2 < 1+f < sqrt(2) .
 *
 *	2. Let s = f/(2+f) ; based on log(1+f) = log(1+s) - log(1-s)
 *		 = 2s + 2/3 s**3 + 2/5 s**5 + .....,
 *	   log(1+f) is computed by
 *
 *	     		log(1+f) = 2s + s*log__L(s*s)
 *	   where
 *		log__L(z) = z*(L1 + z*(L2 + z*(... (L6 + z*L7)...)))
 *
 *	   See log__L() for the values of the coefficients.
 *
 *	3. Finally,  log(1+x) = k*ln2 + log(1+f).  
 *
 *	Remarks 1. In step 3 n*ln2 will be stored in two floating point numbers
 *		   n*ln2hi + n*ln2lo, where ln2hi is chosen such that the last 
 *		   20 bits (for VAX D format), or the last 21 bits ( for IEEE 
 *		   double) is 0. This ensures n*ln2hi is exactly representable.
 *		2. In step 1, f may not be representable. A correction term c
 *	 	   for f is computed. It follows that the correction term for
 *		   f - t (the leading term of log(1+f) in step 2) is c-c*x. We
 *		   add this correction term to n*ln2lo to attenuate the error.
 *
 *
 * Special cases:
 *	log1p(x) is NaN with signal if x < -1; log1p(NaN) is NaN with no signal;
 *	log1p(INF) is +INF; log1p(-1) is -INF with signal;
 *	only log1p(0)=0 is exact for finite argument.
 *
 * Accuracy:
 *	log1p(x) returns the exact log(1+x) nearly rounded. In a test run 
 *	with 1,536,000 random arguments on a VAX, the maximum observed
 *	error was .846 ulps (units in the last place).
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following constants.
 * The decimal values may be used, provided that the compiler will convert
 * from decimal to binary accurately enough to produce the hexadecimal values
 * shown.
 */

#include <errno.h>
#include "mathimpl.h"

vc(ln2hi, 6.9314718055829871446E-1  ,7217,4031,0000,f7d0,   0, .B17217F7D00000)
vc(ln2lo, 1.6465949582897081279E-12 ,bcd5,2ce7,d9cc,e4f1, -39, .E7BCD5E4F1D9CC)
vc(sqrt2, 1.4142135623730950622E0   ,04f3,40b5,de65,33f9,   1, .B504F333F9DE65)

ic(ln2hi, 6.9314718036912381649E-1,   -1, 1.62E42FEE00000)
ic(ln2lo, 1.9082149292705877000E-10, -33, 1.A39EF35793C76)
ic(sqrt2, 1.4142135623730951455E0,     0, 1.6A09E667F3BCD)

#ifdef vccast
#define	ln2hi	vccast(ln2hi)
#define	ln2lo	vccast(ln2lo)
#define	sqrt2	vccast(sqrt2)
#endif

double log1p(x)
double x;
{
	const static double zero=0.0, negone= -1.0, one=1.0, 
		      half=1.0/2.0, small=1.0E-20;   /* 1+small == 1 */
	double z,s,t,c;
	int k;

#if !defined(vax)&&!defined(tahoe)
	if(x!=x) return(x);	/* x is NaN */
#endif	/* !defined(vax)&&!defined(tahoe) */

	if(finite(x)) {
	   if( x > negone ) {

	   /* argument reduction */
	      if(copysign(x,one)<small) return(x);
	      k=logb(one+x); z=scalb(x,-k); t=scalb(one,-k);
	      if(z+t >= sqrt2 ) 
		  { k += 1 ; z *= half; t *= half; }
	      t += negone; x = z + t;
	      c = (t-x)+z ;		/* correction term for x */

 	   /* compute log(1+x)  */
              s = x/(2+x); t = x*x*half;
	      c += (k*ln2lo-c*x);
	      z = c+s*(t+__log__L(s*s));
	      x += (z - t) ;

	      return(k*ln2hi+x);
	   }
	/* end of if (x > negone) */

	    else {
#if defined(vax)||defined(tahoe)
		if ( x == negone )
		    return (infnan(-ERANGE));	/* -INF */
		else
		    return (infnan(EDOM));	/* NaN */
#else	/* defined(vax)||defined(tahoe) */
		/* x = -1, return -INF with signal */
		if ( x == negone ) return( negone/zero );

		/* negative argument for log, return NaN with signal */
	        else return ( zero / zero );
#endif	/* defined(vax)||defined(tahoe) */
	    }
	}
    /* end of if (finite(x)) */

    /* log(-INF) is NaN */
	else if(x<0) 
	     return(zero/zero);

    /* log(+INF) is INF */
	else return(x);      
}
