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
static char sccsid[] = "@(#)atan.c	8.1 (Berkeley) 6/4/93";
#endif /* not lint */

/* ATAN(X)
 * RETURNS ARC TANGENT OF X
 * DOUBLE PRECISION (IEEE DOUBLE 53 bits, VAX D FORMAT 56 bits)
 * CODED IN C BY K.C. NG, 4/16/85, REVISED ON 6/10/85.
 *
 * Required kernel function:
 *	atan2(y,x)
 *
 * Method:
 *	atan(x) = atan2(x,1.0).
 *
 * Special case:
 *	if x is NaN, return x itself.
 *
 * Accuracy:
 * 1)  If atan2() uses machine PI, then
 *
 *	atan(x) returns (PI/pi) * (the exact arc tangent of x) nearly rounded;
 *	and PI is the exact pi rounded to machine precision (see atan2 for
 *      details):
 *
 *	in decimal:
 *		pi = 3.141592653589793 23846264338327 .....
 *    53 bits   PI = 3.141592653589793 115997963 ..... ,
 *    56 bits   PI = 3.141592653589793 227020265 ..... ,
 *
 *	in hexadecimal:
 *		pi = 3.243F6A8885A308D313198A2E....
 *    53 bits   PI = 3.243F6A8885A30  =  2 * 1.921FB54442D18	error=.276ulps
 *    56 bits   PI = 3.243F6A8885A308 =  4 * .C90FDAA22168C2    error=.206ulps
 *
 *	In a test run with more than 200,000 random arguments on a VAX, the
 *	maximum observed error in ulps (units in the last place) was
 *	0.86 ulps.      (comparing against (PI/pi)*(exact atan(x))).
 *
 * 2)  If atan2() uses true pi, then
 *
 *	atan(x) returns the exact atan(x) with error below about 2 ulps.
 *
 *	In a test run with more than 1,024,000 random arguments on a VAX, the
 *	maximum observed error in ulps (units in the last place) was
 *	0.85 ulps.
 */

double atan(x)
double x;
{
	double atan2(),one=1.0;
	return(atan2(x,one));
}
