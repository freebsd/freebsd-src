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
static char sccsid[] = "@(#)cbrt.c	5.9 (Berkeley) 2/13/91";
#endif /* not lint */

#include <sys/cdefs.h>

/* kahan's cube root (53 bits IEEE double precision)
 * for IEEE machines only
 * coded in C by K.C. Ng, 4/30/85
 *
 * Accuracy:
 *	better than 0.667 ulps according to an error analysis. Maximum
 * error observed was 0.666 ulps in an 1,000,000 random arguments test.
 *
 * Warning: this code is semi machine dependent; the ordering of words in
 * a floating point number must be known in advance. I assume that the
 * long interger at the address of a floating point number will be the
 * leading 32 bits of that floating point number (i.e., sign, exponent,
 * and the 20 most significant bits).
 * On a National machine, it has different ordering; therefore, this code 
 * must be compiled with flag -DNATIONAL. 
 */
#if !defined(vax)&&!defined(tahoe)

static const unsigned long
		     B1 = 715094163, /* B1 = (682-0.03306235651)*2**20 */
	             B2 = 696219795; /* B2 = (664-0.03306235651)*2**20 */
static const double
	    C= 19./35.,
	    D= -864./1225.,
	    E= 99./70.,
	    F= 45./28.,
	    G= 5./14.;

double cbrt(x) 
double x;
{
	double r,s,t=0.0,w;
	unsigned long *px = (unsigned long *) &x,
	              *pt = (unsigned long *) &t,
		      mexp,sign;

#ifdef national /* ordering of words in a floating points number */
	const int n0=1,n1=0;
#else	/* national */
	const int n0=0,n1=1;
#endif	/* national */

	mexp=px[n0]&0x7ff00000;
	if(mexp==0x7ff00000) return(x); /* cbrt(NaN,INF) is itself */
	if(x==0.0) return(x);		/* cbrt(0) is itself */

	sign=px[n0]&0x80000000; /* sign= sign(x) */
	px[n0] ^= sign;		/* x=|x| */


    /* rough cbrt to 5 bits */
	if(mexp==0) 		/* subnormal number */
	  {pt[n0]=0x43500000; 	/* set t= 2**54 */
	   t*=x; pt[n0]=pt[n0]/3+B2;
	  }
	else
	  pt[n0]=px[n0]/3+B1;	


    /* new cbrt to 23 bits, may be implemented in single precision */
	r=t*t/x;
	s=C+r*t;
	t*=G+F/(s+E+D/s);	

    /* chopped to 20 bits and make it larger than cbrt(x) */ 
	pt[n1]=0; pt[n0]+=0x00000001;


    /* one step newton iteration to 53 bits with error less than 0.667 ulps */
	s=t*t;		/* t*t is exact */
	r=x/s;
	w=t+t;
	r=(r-t)/(w+r);	/* r-t is exact */
	t=t+t*r;


    /* retore the sign bit */
	pt[n0] |= sign;
	return(t);
}
#endif
