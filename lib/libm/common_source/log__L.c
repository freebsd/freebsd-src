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
static char sccsid[] = "@(#)log__L.c	8.1 (Berkeley) 6/4/93";
#endif /* not lint */

/* log__L(Z)
 *		LOG(1+X) - 2S			       X
 * RETURN      ---------------  WHERE Z = S*S,  S = ------- , 0 <= Z <= .0294...
 *		      S				     2 + X
 *		     
 * DOUBLE PRECISION (VAX D FORMAT 56 bits or IEEE DOUBLE 53 BITS)
 * KERNEL FUNCTION FOR LOG; TO BE USED IN LOG1P, LOG, AND POW FUNCTIONS
 * CODED IN C BY K.C. NG, 1/19/85; 
 * REVISED BY K.C. Ng, 2/3/85, 4/16/85.
 *
 * Method :
 *	1. Polynomial approximation: let s = x/(2+x). 
 *	   Based on log(1+x) = log(1+s) - log(1-s)
 *		 = 2s + 2/3 s**3 + 2/5 s**5 + .....,
 *
 *	   (log(1+x) - 2s)/s is computed by
 *
 *	       z*(L1 + z*(L2 + z*(... (L7 + z*L8)...)))
 *
 *	   where z=s*s. (See the listing below for Lk's values.) The 
 *	   coefficients are obtained by a special Remez algorithm. 
 *
 * Accuracy:
 *	Assuming no rounding error, the maximum magnitude of the approximation 
 *	error (absolute) is 2**(-58.49) for IEEE double, and 2**(-63.63)
 *	for VAX D format.
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following constants.
 * The decimal values may be used, provided that the compiler will convert
 * from decimal to binary accurately enough to produce the hexadecimal values
 * shown.
 */

#include "mathimpl.h"

vc(L1, 6.6666666666666703212E-1 ,aaaa,402a,aac5,aaaa,  0, .AAAAAAAAAAAAC5)
vc(L2, 3.9999999999970461961E-1 ,cccc,3fcc,2684,cccc, -1, .CCCCCCCCCC2684)
vc(L3, 2.8571428579395698188E-1 ,4924,3f92,5782,92f8, -1, .92492492F85782)
vc(L4, 2.2222221233634724402E-1 ,8e38,3f63,af2c,39b7, -2, .E38E3839B7AF2C)
vc(L5, 1.8181879517064680057E-1 ,2eb4,3f3a,655e,cc39, -2, .BA2EB4CC39655E)
vc(L6, 1.5382888777946145467E-1 ,8551,3f1d,781d,e8c5, -2, .9D8551E8C5781D)
vc(L7, 1.3338356561139403517E-1 ,95b3,3f08,cd92,907f, -2, .8895B3907FCD92)
vc(L8, 1.2500000000000000000E-1 ,0000,3f00,0000,0000, -2, .80000000000000)

ic(L1, 6.6666666666667340202E-1, -1, 1.5555555555592)
ic(L2, 3.9999999999416702146E-1, -2, 1.999999997FF24)
ic(L3, 2.8571428742008753154E-1, -2, 1.24924941E07B4)
ic(L4, 2.2222198607186277597E-1, -3, 1.C71C52150BEA6)
ic(L5, 1.8183562745289935658E-1, -3, 1.74663CC94342F)
ic(L6, 1.5314087275331442206E-1, -3, 1.39A1EC014045B)
ic(L7, 1.4795612545334174692E-1, -3, 1.2F039F0085122)

#ifdef vccast
#define	L1	vccast(L1)
#define	L2	vccast(L2)
#define	L3	vccast(L3)
#define	L4	vccast(L4)
#define	L5	vccast(L5)
#define	L6	vccast(L6)
#define	L7	vccast(L7)
#define	L8	vccast(L8)
#endif

double __log__L(z)
double z;
{
#if defined(vax)||defined(tahoe)
    return(z*(L1+z*(L2+z*(L3+z*(L4+z*(L5+z*(L6+z*(L7+z*L8))))))));
#else	/* defined(vax)||defined(tahoe) */
    return(z*(L1+z*(L2+z*(L3+z*(L4+z*(L5+z*(L6+z*L7)))))));
#endif	/* defined(vax)||defined(tahoe) */
}
