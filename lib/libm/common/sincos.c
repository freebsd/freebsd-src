/*
 * Copyright (c) 1987, 1993
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
static char sccsid[] = "@(#)sincos.c	8.1 (Berkeley) 6/4/93";
#endif /* not lint */

#include "trig.h"
double
sin(x)
double x;
{
	double a,c,z;

        if(!finite(x))		/* sin(NaN) and sin(INF) must be NaN */
		return x-x;
	x=drem(x,PI2);		/* reduce x into [-PI,PI] */
	a=copysign(x,one);
	if (a >= PIo4) {
		if(a >= PI3o4)		/* ... in [3PI/4,PI] */
			x = copysign((a = PI-a),x);
		else {			/* ... in [PI/4,3PI/4]  */
			a = PIo2-a;		/* rtn. sign(x)*C(PI/2-|x|) */
			z = a*a;
			c = cos__C(z);
			z *= half;
			a = (z >= thresh ? half-((z-half)-c) : one-(z-c));
			return copysign(a,x);
		}
	}

	if (a < small) {		/* rtn. S(x) */
		big+a;
		return x;
	}
	return x+x*sin__S(x*x);
}

double
cos(x) 
double x;
{
	double a,c,z,s = 1.0;

	if(!finite(x))		/* cos(NaN) and cos(INF) must be NaN */
		return x-x;
	x=drem(x,PI2);		/* reduce x into [-PI,PI] */
	a=copysign(x,one);
	if (a >= PIo4) {
		if (a >= PI3o4) {	/* ... in [3PI/4,PI] */
			a = PI-a;
			s = negone;
		}
		else {			/* ... in [PI/4,3PI/4] */
			a = PIo2-a;
			return a+a*sin__S(a*a);	/* rtn. S(PI/2-|x|) */ 
		}
	}
	if (a < small) {
		big+a;
		return s;		/* rtn. s*C(a) */
	}
	z = a*a;
	c = cos__C(z);
	z *= half;
	a = (z >= thresh ? half-((z-half)-c) : one-(z-c));
	return copysign(a,s);
}
