/*-
 * Copyright (c) 1992, 1993
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
static char sccsid[] = "@(#)jn.c	8.2 (Berkeley) 11/30/93";
#endif /* not lint */

/*
 * 16 December 1992
 * Minor modifications by Peter McIlroy to adapt non-IEEE architecture.
 */

/*
 * ====================================================
 * Copyright (C) 1992 by Sun Microsystems, Inc.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 *
 * ******************* WARNING ********************
 * This is an alpha version of SunPro's FDLIBM (Freely
 * Distributable Math Library) for IEEE double precision 
 * arithmetic. FDLIBM is a basic math library written
 * in C that runs on machines that conform to IEEE 
 * Standard 754/854. This alpha version is distributed 
 * for testing purpose. Those who use this software 
 * should report any bugs to 
 *
 *		fdlibm-comments@sunpro.eng.sun.com
 *
 * -- K.C. Ng, Oct 12, 1992
 * ************************************************
 */

/*
 * jn(int n, double x), yn(int n, double x)
 * floating point Bessel's function of the 1st and 2nd kind
 * of order n
 *          
 * Special cases:
 *	y0(0)=y1(0)=yn(n,0) = -inf with division by zero signal;
 *	y0(-ve)=y1(-ve)=yn(n,-ve) are NaN with invalid signal.
 * Note 2. About jn(n,x), yn(n,x)
 *	For n=0, j0(x) is called,
 *	for n=1, j1(x) is called,
 *	for n<x, forward recursion us used starting
 *	from values of j0(x) and j1(x).
 *	for n>x, a continued fraction approximation to
 *	j(n,x)/j(n-1,x) is evaluated and then backward
 *	recursion is used starting from a supposed value
 *	for j(n,x). The resulting value of j(0,x) is
 *	compared with the actual value to correct the
 *	supposed value of j(n,x).
 *
 *	yn(n,x) is similar in all respects, except
 *	that forward recursion is used for all
 *	values of n>1.
 *	
 */

#include <math.h>
#include <float.h>
#include <errno.h>

#if defined(vax) || defined(tahoe)
#define _IEEE	0
#else
#define _IEEE	1
#define infnan(x) (0.0)
#endif

static double
invsqrtpi= 5.641895835477562869480794515607725858441e-0001,
two  = 2.0,
zero = 0.0,
one  = 1.0;

double jn(n,x)
	int n; double x;
{
	int i, sgn;
	double a, b, temp;
	double z, w;

    /* J(-n,x) = (-1)^n * J(n, x), J(n, -x) = (-1)^n * J(n, x)
     * Thus, J(-n,x) = J(n,-x)
     */
    /* if J(n,NaN) is NaN */
	if (_IEEE && isnan(x)) return x+x;
	if (n<0){		
		n = -n;
		x = -x;
	}
	if (n==0) return(j0(x));
	if (n==1) return(j1(x));
	sgn = (n&1)&(x < zero);		/* even n -- 0, odd n -- sign(x) */
	x = fabs(x);
	if (x == 0 || !finite (x)) 	/* if x is 0 or inf */
	    b = zero;
	else if ((double) n <= x) {
			/* Safe to use J(n+1,x)=2n/x *J(n,x)-J(n-1,x) */
	    if (_IEEE && x >= 8.148143905337944345e+090) {
					/* x >= 2**302 */
    /* (x >> n**2) 
     *	    Jn(x) = cos(x-(2n+1)*pi/4)*sqrt(2/x*pi)
     *	    Yn(x) = sin(x-(2n+1)*pi/4)*sqrt(2/x*pi)
     *	    Let s=sin(x), c=cos(x), 
     *		xn=x-(2n+1)*pi/4, sqt2 = sqrt(2),then
     *
     *		   n	sin(xn)*sqt2	cos(xn)*sqt2
     *		----------------------------------
     *		   0	 s-c		 c+s
     *		   1	-s-c 		-c+s
     *		   2	-s+c		-c-s
     *		   3	 s+c		 c-s
     */
		switch(n&3) {
		    case 0: temp =  cos(x)+sin(x); break;
		    case 1: temp = -cos(x)+sin(x); break;
		    case 2: temp = -cos(x)-sin(x); break;
		    case 3: temp =  cos(x)-sin(x); break;
		}
		b = invsqrtpi*temp/sqrt(x);
	    } else {	
	        a = j0(x);
	        b = j1(x);
	        for(i=1;i<n;i++){
		    temp = b;
		    b = b*((double)(i+i)/x) - a; /* avoid underflow */
		    a = temp;
	        }
	    }
	} else {
	    if (x < 1.86264514923095703125e-009) { /* x < 2**-29 */
    /* x is tiny, return the first Taylor expansion of J(n,x) 
     * J(n,x) = 1/n!*(x/2)^n  - ...
     */
		if (n > 33)	/* underflow */
		    b = zero;
		else {
		    temp = x*0.5; b = temp;
		    for (a=one,i=2;i<=n;i++) {
			a *= (double)i;		/* a = n! */
			b *= temp;		/* b = (x/2)^n */
		    }
		    b = b/a;
		}
	    } else {
		/* use backward recurrence */
		/* 			x      x^2      x^2       
		 *  J(n,x)/J(n-1,x) =  ----   ------   ------   .....
		 *			2n  - 2(n+1) - 2(n+2)
		 *
		 * 			1      1        1       
		 *  (for large x)   =  ----  ------   ------   .....
		 *			2n   2(n+1)   2(n+2)
		 *			-- - ------ - ------ - 
		 *			 x     x         x
		 *
		 * Let w = 2n/x and h=2/x, then the above quotient
		 * is equal to the continued fraction:
		 *		    1
		 *	= -----------------------
		 *		       1
		 *	   w - -----------------
		 *			  1
		 * 	        w+h - ---------
		 *		       w+2h - ...
		 *
		 * To determine how many terms needed, let
		 * Q(0) = w, Q(1) = w(w+h) - 1,
		 * Q(k) = (w+k*h)*Q(k-1) - Q(k-2),
		 * When Q(k) > 1e4	good for single 
		 * When Q(k) > 1e9	good for double 
		 * When Q(k) > 1e17	good for quadruple 
		 */
	    /* determine k */
		double t,v;
		double q0,q1,h,tmp; int k,m;
		w  = (n+n)/(double)x; h = 2.0/(double)x;
		q0 = w;  z = w+h; q1 = w*z - 1.0; k=1;
		while (q1<1.0e9) {
			k += 1; z += h;
			tmp = z*q1 - q0;
			q0 = q1;
			q1 = tmp;
		}
		m = n+n;
		for(t=zero, i = 2*(n+k); i>=m; i -= 2) t = one/(i/x-t);
		a = t;
		b = one;
		/*  estimate log((2/x)^n*n!) = n*log(2/x)+n*ln(n)
		 *  Hence, if n*(log(2n/x)) > ...
		 *  single 8.8722839355e+01
		 *  double 7.09782712893383973096e+02
		 *  long double 1.1356523406294143949491931077970765006170e+04
		 *  then recurrent value may overflow and the result will
		 *  likely underflow to zero
		 */
		tmp = n;
		v = two/x;
		tmp = tmp*log(fabs(v*tmp));
	    	for (i=n-1;i>0;i--){
		        temp = b;
		        b = ((i+i)/x)*b - a;
		        a = temp;
		    /* scale b to avoid spurious overflow */
#			if defined(vax) || defined(tahoe)
#				define BMAX 1e13
#			else
#				define BMAX 1e100
#			endif /* defined(vax) || defined(tahoe) */
			if (b > BMAX) {
				a /= b;
				t /= b;
				b = one;
			}
		}
	    	b = (t*j0(x)/b);
	    }
	}
	return ((sgn == 1) ? -b : b);
}
double yn(n,x) 
	int n; double x;
{
	int i, sign;
	double a, b, temp;

    /* Y(n,NaN), Y(n, x < 0) is NaN */
	if (x <= 0 || (_IEEE && x != x))
		if (_IEEE && x < 0) return zero/zero;
		else if (x < 0)     return (infnan(EDOM));
		else if (_IEEE)     return -one/zero;
		else		    return(infnan(-ERANGE));
	else if (!finite(x)) return(0);
	sign = 1;
	if (n<0){
		n = -n;
		sign = 1 - ((n&1)<<2);
	}
	if (n == 0) return(y0(x));
	if (n == 1) return(sign*y1(x));
	if(_IEEE && x >= 8.148143905337944345e+090) { /* x > 2**302 */
    /* (x >> n**2) 
     *	    Jn(x) = cos(x-(2n+1)*pi/4)*sqrt(2/x*pi)
     *	    Yn(x) = sin(x-(2n+1)*pi/4)*sqrt(2/x*pi)
     *	    Let s=sin(x), c=cos(x), 
     *		xn=x-(2n+1)*pi/4, sqt2 = sqrt(2),then
     *
     *		   n	sin(xn)*sqt2	cos(xn)*sqt2
     *		----------------------------------
     *		   0	 s-c		 c+s
     *		   1	-s-c 		-c+s
     *		   2	-s+c		-c-s
     *		   3	 s+c		 c-s
     */
		switch (n&3) {
		    case 0: temp =  sin(x)-cos(x); break;
		    case 1: temp = -sin(x)-cos(x); break;
		    case 2: temp = -sin(x)+cos(x); break;
		    case 3: temp =  sin(x)+cos(x); break;
		}
		b = invsqrtpi*temp/sqrt(x);
	} else {
	    a = y0(x);
	    b = y1(x);
	/* quit if b is -inf */
	    for (i = 1; i < n && !finite(b); i++){
		temp = b;
		b = ((double)(i+i)/x)*b - a;
		a = temp;
	    }
	}
	if (!_IEEE && !finite(b))
		return (infnan(-sign * ERANGE));
	return ((sign > 0) ? b : -b);
}
