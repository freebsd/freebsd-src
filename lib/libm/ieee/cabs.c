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
static char sccsid[] = "@(#)cabs.c	5.6 (Berkeley) 10/9/90";
#endif /* not lint */

/* HYPOT(X,Y)
 * RETURN THE SQUARE ROOT OF X^2 + Y^2  WHERE Z=X+iY
 * DOUBLE PRECISION (VAX D format 56 bits, IEEE DOUBLE 53 BITS)
 * CODED IN C BY K.C. NG, 11/28/84; 
 * REVISED BY K.C. NG, 7/12/85.
 *
 * Required system supported functions :
 *	copysign(x,y)
 *	finite(x)
 *	scalb(x,N)
 *	sqrt(x)
 *
 * Method :
 *	1. replace x by |x| and y by |y|, and swap x and
 *	   y if y > x (hence x is never smaller than y).
 *	2. Hypot(x,y) is computed by:
 *	   Case I, x/y > 2
 *		
 *				       y
 *		hypot = x + -----------------------------
 *			 		    2
 *			    sqrt ( 1 + [x/y]  )  +  x/y
 *
 *	   Case II, x/y <= 2 
 *				                   y
 *		hypot = x + --------------------------------------------------
 *				          		     2 
 *				     			[x/y]   -  2
 *			   (sqrt(2)+1) + (x-y)/y + -----------------------------
 *			 		    			  2
 *			    			  sqrt ( 1 + [x/y]  )  + sqrt(2)
 *
 *
 *
 * Special cases:
 *	hypot(x,y) is INF if x or y is +INF or -INF; else
 *	hypot(x,y) is NAN if x or y is NAN.
 *
 * Accuracy:
 * 	hypot(x,y) returns the sqrt(x^2+y^2) with error less than 1 ulps (units
 *	in the last place). See Kahan's "Interval Arithmetic Options in the
 *	Proposed IEEE Floating Point Arithmetic Standard", Interval Mathematics
 *      1980, Edited by Karl L.E. Nickel, pp 99-128. (A faster but less accurate
 *	code follows in	comments.) In a test run with 500,000 random arguments
 *	on a VAX, the maximum observed error was .959 ulps.
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following constants.
 * The decimal values may be used, provided that the compiler will convert
 * from decimal to binary accurately enough to produce the hexadecimal values
 * shown.
 */
#include "mathimpl.h"

vc(r2p1hi, 2.4142135623730950345E0   ,8279,411a,ef32,99fc,   2, .9A827999FCEF32)
vc(r2p1lo, 1.4349369327986523769E-17 ,597d,2484,754b,89b3, -55, .84597D89B3754B)
vc(sqrt2,  1.4142135623730950622E0   ,04f3,40b5,de65,33f9,   1, .B504F333F9DE65)

ic(r2p1hi, 2.4142135623730949234E0   ,   1, 1.3504F333F9DE6)
ic(r2p1lo, 1.2537167179050217666E-16 , -53, 1.21165F626CDD5)
ic(sqrt2,  1.4142135623730951455E0   ,   0, 1.6A09E667F3BCD)

#ifdef vccast
#define	r2p1hi	vccast(r2p1hi)
#define	r2p1lo	vccast(r2p1lo)
#define	sqrt2	vccast(sqrt2)
#endif

double
hypot(x,y)
double x, y;
{
	static const double zero=0, one=1, 
		      small=1.0E-18;	/* fl(1+small)==1 */
	static const ibig=30;	/* fl(1+2**(2*ibig))==1 */
	double t,r;
	int exp;

	if(finite(x))
	    if(finite(y))
	    {	
		x=copysign(x,one);
		y=copysign(y,one);
		if(y > x) 
		    { t=x; x=y; y=t; }
		if(x == zero) return(zero);
		if(y == zero) return(x);
		exp= logb(x);
		if(exp-(int)logb(y) > ibig ) 	
			/* raise inexact flag and return |x| */
		   { one+small; return(x); }

	    /* start computing sqrt(x^2 + y^2) */
		r=x-y;
		if(r>y) { 	/* x/y > 2 */
		    r=x/y;
		    r=r+sqrt(one+r*r); }
		else {		/* 1 <= x/y <= 2 */
		    r/=y; t=r*(r+2.0);
		    r+=t/(sqrt2+sqrt(2.0+t));
		    r+=r2p1lo; r+=r2p1hi; }

		r=y/r;
		return(x+r);

	    }

	    else if(y==y)   	   /* y is +-INF */
		     return(copysign(y,one));
	    else 
		     return(y);	   /* y is NaN and x is finite */

	else if(x==x) 		   /* x is +-INF */
	         return (copysign(x,one));
	else if(finite(y))
	         return(x);		   /* x is NaN, y is finite */
#if !defined(vax)&&!defined(tahoe)
	else if(y!=y) return(y);  /* x and y is NaN */
#endif	/* !defined(vax)&&!defined(tahoe) */
	else return(copysign(y,one));   /* y is INF */
}

/* CABS(Z)
 * RETURN THE ABSOLUTE VALUE OF THE COMPLEX NUMBER  Z = X + iY
 * DOUBLE PRECISION (VAX D format 56 bits, IEEE DOUBLE 53 BITS)
 * CODED IN C BY K.C. NG, 11/28/84.
 * REVISED BY K.C. NG, 7/12/85.
 *
 * Required kernel function :
 *	hypot(x,y)
 *
 * Method :
 *	cabs(z) = hypot(x,y) .
 */

double
cabs(z)
struct { double x, y;} z;
{
	return hypot(z.x,z.y);
}

double
z_abs(z)
struct { double x,y;} *z;
{
	return hypot(z->x,z->y);
}

/* A faster but less accurate version of cabs(x,y) */
#if 0
double hypot(x,y)
double x, y;
{
	static const double zero=0, one=1;
		      small=1.0E-18;	/* fl(1+small)==1 */
	static const ibig=30;	/* fl(1+2**(2*ibig))==1 */
	double temp;
	int exp;

	if(finite(x))
	    if(finite(y))
	    {	
		x=copysign(x,one);
		y=copysign(y,one);
		if(y > x) 
		    { temp=x; x=y; y=temp; }
		if(x == zero) return(zero);
		if(y == zero) return(x);
		exp= logb(x);
		x=scalb(x,-exp);
		if(exp-(int)logb(y) > ibig ) 
			/* raise inexact flag and return |x| */
		   { one+small; return(scalb(x,exp)); }
		else y=scalb(y,-exp);
		return(scalb(sqrt(x*x+y*y),exp));
	    }

	    else if(y==y)   	   /* y is +-INF */
		     return(copysign(y,one));
	    else 
		     return(y);	   /* y is NaN and x is finite */

	else if(x==x) 		   /* x is +-INF */
	         return (copysign(x,one));
	else if(finite(y))
	         return(x);		   /* x is NaN, y is finite */
	else if(y!=y) return(y);  	/* x and y is NaN */
	else return(copysign(y,one));   /* y is INF */
}
#endif
