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
 *
 *	@(#)trig.h	8.1 (Berkeley) 6/4/93
 */

#include "mathimpl.h"

vc(thresh, 2.6117239648121182150E-1 ,b863,3f85,6ea0,6b02, -1, .85B8636B026EA0)
vc(PIo4,   7.8539816339744830676E-1 ,0fda,4049,68c2,a221,  0, .C90FDAA22168C2)
vc(PIo2,   1.5707963267948966135E0  ,0fda,40c9,68c2,a221,  1, .C90FDAA22168C2)
vc(PI3o4,  2.3561944901923449203E0  ,cbe3,4116,0e92,f999,  2, .96CBE3F9990E92)
vc(PI,     3.1415926535897932270E0  ,0fda,4149,68c2,a221,  2, .C90FDAA22168C2)
vc(PI2,    6.2831853071795864540E0  ,0fda,41c9,68c2,a221,  3, .C90FDAA22168C2)

ic(thresh, 2.6117239648121182150E-1 , -2, 1.0B70C6D604DD4)
ic(PIo4,   7.8539816339744827900E-1 , -1, 1.921FB54442D18)
ic(PIo2,   1.5707963267948965580E0  ,  0, 1.921FB54442D18)
ic(PI3o4,  2.3561944901923448370E0  ,  1, 1.2D97C7F3321D2)
ic(PI,     3.1415926535897931160E0  ,  1, 1.921FB54442D18)
ic(PI2,    6.2831853071795862320E0  ,  2, 1.921FB54442D18)

#ifdef vccast
#define	thresh	vccast(thresh)
#define	PIo4	vccast(PIo4)
#define	PIo2	vccast(PIo2)
#define	PI3o4	vccast(PI3o4)
#define	PI	vccast(PI)
#define	PI2	vccast(PI2)
#endif

#ifdef national
static long fmaxx[]	= { 0xffffffff, 0x7fefffff};
#define   fmax    (*(double*)fmaxx)
#endif	/* national */

static const double
	zero = 0,
	one = 1,
	negone = -1,
	half = 1.0/2.0, 
	small = 1E-10,	/* 1+small**2 == 1; better values for small:
			 *		small	= 1.5E-9 for VAX D
			 *			= 1.2E-8 for IEEE Double
			 *			= 2.8E-10 for IEEE Extended
			 */
	big = 1E20;	/* big := 1/(small**2) */

/* sin__S(x*x) ... re-implemented as a macro
 * DOUBLE PRECISION (VAX D format 56 bits, IEEE DOUBLE 53 BITS)
 * STATIC KERNEL FUNCTION OF SIN(X), COS(X), AND TAN(X) 
 * CODED IN C BY K.C. NG, 1/21/85; 
 * REVISED BY K.C. NG on 8/13/85.
 *
 *	    sin(x*k) - x
 * RETURN  --------------- on [-PI/4,PI/4] , where k=pi/PI, PI is the rounded
 *	            x	
 * value of pi in machine precision:
 *
 *	Decimal:
 *		pi = 3.141592653589793 23846264338327 ..... 
 *    53 bits   PI = 3.141592653589793 115997963 ..... ,
 *    56 bits   PI = 3.141592653589793 227020265 ..... ,  
 *
 *	Hexadecimal:
 *		pi = 3.243F6A8885A308D313198A2E....
 *    53 bits   PI = 3.243F6A8885A30  =  2 * 1.921FB54442D18
 *    56 bits   PI = 3.243F6A8885A308 =  4 * .C90FDAA22168C2    
 *
 * Method:
 *	1. Let z=x*x. Create a polynomial approximation to 
 *	    (sin(k*x)-x)/x  =  z*(S0 + S1*z^1 + ... + S5*z^5).
 *	Then
 *      sin__S(x*x) = z*(S0 + S1*z^1 + ... + S5*z^5)
 *
 *	The coefficient S's are obtained by a special Remez algorithm.
 *
 * Accuracy:
 *	In the absence of rounding error, the approximation has absolute error 
 *	less than 2**(-61.11) for VAX D FORMAT, 2**(-57.45) for IEEE DOUBLE. 
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following constants.
 * The decimal values may be used, provided that the compiler will convert
 * from decimal to binary accurately enough to produce the hexadecimal values
 * shown.
 *
 */

vc(S0, -1.6666666666666646660E-1  ,aaaa,bf2a,aa71,aaaa,  -2, -.AAAAAAAAAAAA71)
vc(S1,  8.3333333333297230413E-3  ,8888,3d08,477f,8888,  -6,  .8888888888477F)
vc(S2, -1.9841269838362403710E-4  ,0d00,ba50,1057,cf8a, -12, -.D00D00CF8A1057)
vc(S3,  2.7557318019967078930E-6  ,ef1c,3738,bedc,a326, -18,  .B8EF1CA326BEDC)
vc(S4, -2.5051841873876551398E-8  ,3195,b3d7,e1d3,374c, -25, -.D73195374CE1D3)
vc(S5,  1.6028995389845827653E-10 ,3d9c,3030,cccc,6d26, -32,  .B03D9C6D26CCCC)
vc(S6, -6.2723499671769283121E-13 ,8d0b,ac30,ea82,7561, -40, -.B08D0B7561EA82)

ic(S0, -1.6666666666666463126E-1  ,  -3, -1.555555555550C)
ic(S1,  8.3333333332992771264E-3  ,  -7,  1.111111110C461)
ic(S2, -1.9841269816180999116E-4  , -13, -1.A01A019746345)
ic(S3,  2.7557309793219876880E-6  , -19,  1.71DE3209CDCD9)
ic(S4, -2.5050225177523807003E-8  , -26, -1.AE5C0E319A4EF)
ic(S5,  1.5868926979889205164E-10 , -33,  1.5CF61DF672B13)

#ifdef vccast
#define	S0	vccast(S0)
#define	S1	vccast(S1)
#define	S2	vccast(S2)
#define	S3	vccast(S3)
#define	S4	vccast(S4)
#define	S5	vccast(S5)
#define	S6	vccast(S6)
#endif

#if defined(vax)||defined(tahoe)
#  define	sin__S(z)	(z*(S0+z*(S1+z*(S2+z*(S3+z*(S4+z*(S5+z*S6)))))))
#else 	/* defined(vax)||defined(tahoe) */
#  define	sin__S(z)	(z*(S0+z*(S1+z*(S2+z*(S3+z*(S4+z*S5))))))
#endif 	/* defined(vax)||defined(tahoe) */

/* cos__C(x*x) ... re-implemented as a macro
 * DOUBLE PRECISION (VAX D FORMAT 56 BITS, IEEE DOUBLE 53 BITS)
 * STATIC KERNEL FUNCTION OF SIN(X), COS(X), AND TAN(X) 
 * CODED IN C BY K.C. NG, 1/21/85; 
 * REVISED BY K.C. NG on 8/13/85.
 *
 *	   		    x*x	
 * RETURN   cos(k*x) - 1 + ----- on [-PI/4,PI/4],  where k = pi/PI,
 *	  		     2	
 * PI is the rounded value of pi in machine precision :
 *
 *	Decimal:
 *		pi = 3.141592653589793 23846264338327 ..... 
 *    53 bits   PI = 3.141592653589793 115997963 ..... ,
 *    56 bits   PI = 3.141592653589793 227020265 ..... ,  
 *
 *	Hexadecimal:
 *		pi = 3.243F6A8885A308D313198A2E....
 *    53 bits   PI = 3.243F6A8885A30  =  2 * 1.921FB54442D18
 *    56 bits   PI = 3.243F6A8885A308 =  4 * .C90FDAA22168C2    
 *
 *
 * Method:
 *	1. Let z=x*x. Create a polynomial approximation to 
 *	    cos(k*x)-1+z/2  =  z*z*(C0 + C1*z^1 + ... + C5*z^5)
 *	then
 *      cos__C(z) =  z*z*(C0 + C1*z^1 + ... + C5*z^5)
 *
 *	The coefficient C's are obtained by a special Remez algorithm.
 *
 * Accuracy:
 *	In the absence of rounding error, the approximation has absolute error 
 *	less than 2**(-64) for VAX D FORMAT, 2**(-58.3) for IEEE DOUBLE. 
 *	
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following constants.
 * The decimal values may be used, provided that the compiler will convert
 * from decimal to binary accurately enough to produce the hexadecimal values
 * shown.
 */

vc(C0,  4.1666666666666504759E-2  ,aaaa,3e2a,a9f0,aaaa,  -4,  .AAAAAAAAAAA9F0)
vc(C1, -1.3888888888865302059E-3  ,0b60,bbb6,0cca,b60a,  -9, -.B60B60B60A0CCA)
vc(C2,  2.4801587285601038265E-5  ,0d00,38d0,098f,cdcd, -15,  .D00D00CDCD098F)
vc(C3, -2.7557313470902390219E-7  ,f27b,b593,e805,b593, -21, -.93F27BB593E805)
vc(C4,  2.0875623401082232009E-9  ,74c8,320f,3ff0,fa1e, -28,  .8F74C8FA1E3FF0)
vc(C5, -1.1355178117642986178E-11 ,c32d,ae47,5a63,0a5c, -36, -.C7C32D0A5C5A63)

ic(C0,  4.1666666666666504759E-2  ,  -5,  1.555555555553E)
ic(C1, -1.3888888888865301516E-3  , -10, -1.6C16C16C14199)
ic(C2,  2.4801587269650015769E-5  , -16,  1.A01A01971CAEB)
ic(C3, -2.7557304623183959811E-7  , -22, -1.27E4F1314AD1A)
ic(C4,  2.0873958177697780076E-9  , -29,  1.1EE3B60DDDC8C)
ic(C5, -1.1250289076471311557E-11 , -37, -1.8BD5986B2A52E)

#ifdef vccast
#define	C0	vccast(C0)
#define	C1	vccast(C1)
#define	C2	vccast(C2)
#define	C3	vccast(C3)
#define	C4	vccast(C4)
#define	C5	vccast(C5)
#endif

#define cos__C(z)	(z*z*(C0+z*(C1+z*(C2+z*(C3+z*(C4+z*C5))))))
