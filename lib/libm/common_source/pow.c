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
static char sccsid[] = "@(#)pow.c	5.9 (Berkeley) 12/16/92";
#endif /* not lint */

/* POW(X,Y)  
 * RETURN X**Y 
 * DOUBLE PRECISION (VAX D format 56 bits, IEEE DOUBLE 53 BITS)
 * CODED IN C BY K.C. NG, 1/8/85; 
 * REVISED BY K.C. NG on 7/10/85.
 * KERNEL pow_P() REPLACED BY P. McILROY 7/22/92.
 * Required system supported functions:
 *      scalb(x,n)      
 *      logb(x)         
 *	copysign(x,y)	
 *	finite(x)	
 *	drem(x,y)
 *
 * Required kernel functions:
 *	exp__D(a,c)			exp(a + c) for |a| << |c|
 *	struct d_double dlog(x)		r.a + r.b, |r.b| < |r.a|
 *
 * Method
 *	1. Compute and return log(x) in three pieces:
 *		log(x) = n*ln2 + hi + lo,
 *	   where n is an integer.
 *	2. Perform y*log(x) by simulating muti-precision arithmetic and 
 *	   return the answer in three pieces:
 *		y*log(x) = m*ln2 + hi + lo,
 *	   where m is an integer.
 *	3. Return x**y = exp(y*log(x))
 *		= 2^m * ( exp(hi+lo) ).
 *
 * Special cases:
 *	(anything) ** 0  is 1 ;
 *	(anything) ** 1  is itself;
 *	(anything) ** NaN is NaN;
 *	NaN ** (anything except 0) is NaN;
 *	+(anything > 1) ** +INF is +INF;
 *	-(anything > 1) ** +INF is NaN;
 *	+-(anything > 1) ** -INF is +0;
 *	+-(anything < 1) ** +INF is +0;
 *	+(anything < 1) ** -INF is +INF;
 *	-(anything < 1) ** -INF is NaN;
 *	+-1 ** +-INF is NaN and signal INVALID;
 *	+0 ** +(anything except 0, NaN)  is +0;
 *	-0 ** +(anything except 0, NaN, odd integer)  is +0;
 *	+0 ** -(anything except 0, NaN)  is +INF and signal DIV-BY-ZERO;
 *	-0 ** -(anything except 0, NaN, odd integer)  is +INF with signal;
 *	-0 ** (odd integer) = -( +0 ** (odd integer) );
 *	+INF ** +(anything except 0,NaN) is +INF;
 *	+INF ** -(anything except 0,NaN) is +0;
 *	-INF ** (odd integer) = -( +INF ** (odd integer) );
 *	-INF ** (even integer) = ( +INF ** (even integer) );
 *	-INF ** -(anything except integer,NaN) is NaN with signal;
 *	-(x=anything) ** (k=integer) is (-1)**k * (x ** k);
 *	-(anything except 0) ** (non-integer) is NaN with signal;
 *
 * Accuracy:
 *	pow(x,y) returns x**y nearly rounded. In particular, on a SUN, a VAX,
 *	and a Zilog Z8000,
 *			pow(integer,integer)
 *	always returns the correct integer provided it is representable.
 *	In a test run with 100,000 random arguments with 0 < x, y < 20.0
 *	on a VAX, the maximum observed error was 1.79 ulps (units in the 
 *	last place).
 *
 * Constants :
 * The hexadecimal values are the intended ones for the following constants.
 * The decimal values may be used, provided that the compiler will convert
 * from decimal to binary accurately enough to produce the hexadecimal values
 * shown.
 */

#include <errno.h>
#include <math.h>

#include "mathimpl.h"

#if (defined(vax) || defined(tahoe))
#define TRUNC(x)	x = (double) (float) x
#define _IEEE		0
#else
#define _IEEE		1
#define endian		(((*(int *) &one)) ? 1 : 0)
#define TRUNC(x) 	*(((int *) &x)+endian) &= 0xf8000000
#define infnan(x)	0.0
#endif		/* vax or tahoe */

const static double zero=0.0, one=1.0, two=2.0, negone= -1.0;

static double pow_P __P((double, double));

double pow(x,y)  	
double x,y;
{
	double t;
	if (y==zero)
		return (one);
	else if (y==one || (_IEEE && x != x))
		return (x);		/* if x is NaN or y=1 */
	else if (_IEEE && y!=y)		/* if y is NaN */
		return (y);
	else if (!finite(y))		/* if y is INF */
		if ((t=fabs(x))==one)	/* +-1 ** +-INF is NaN */
			return (y - y);
		else if (t>one)
			return ((y<0)? zero : ((x<zero)? y-y : y));
		else
			return ((y>0)? zero : ((x<0)? y-y : -y));
	else if (y==two)
		return (x*x);
	else if (y==negone)
		return (one/x);
    /* x > 0, x == +0 */
	else if (copysign(one, x) == one)
		return (pow_P(x, y));

    /* sign(x)= -1 */
	/* if y is an even integer */
	else if ( (t=drem(y,two)) == zero)
		return (pow_P(-x, y));

	/* if y is an odd integer */
	else if (copysign(t,one) == one)
		return (-pow_P(-x, y));

	/* Henceforth y is not an integer */
	else if (x==zero)	/* x is -0 */
		return ((y>zero)? -x : one/(-x));
	else if (_IEEE)
		return (zero/zero);
	else
		return (infnan(EDOM));
}
/* kernel function for x >= 0 */
static double
#ifdef _ANSI_SOURCE
pow_P(double x, double y)
#else
pow_P(x, y) double x, y;
#endif
{
	struct Double s, t, log__D();
	double  exp__D(), huge = 1e300, tiny = 1e-300;

	if (x == zero)
		return ((y>zero)? x : one/x);
	if (x == 1)
		return (one);
	if (y >= 7e18)		/* infinity */
		if (x < 1)
			return(tiny*tiny);
		else if (_IEEE)
			return (huge*huge);
		else
			return (infnan(ERANGE));

	/* Return exp(y*log(x)), using simulated extended */
	/* precision for the log and the multiply.	  */

	s = log__D(x);
	t.a = y;
	TRUNC(t.a);
	t.b = y - t.a;
	t.b = s.b*y + t.b*s.a;
	t.a *= s.a;
	s.a = t.a + t.b;
	s.b = (t.a - s.a) + t.b;
	return (exp__D(s.a, s.b));
}
