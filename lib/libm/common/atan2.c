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
static char sccsid[] = "@(#)atan2.c	8.1 (Berkeley) 6/4/93";
#endif /* not lint */

/* ATAN2(Y,X)
 * RETURN ARG (X+iY)
 * DOUBLE PRECISION (VAX D format 56 bits, IEEE DOUBLE 53 BITS)
 * CODED IN C BY K.C. NG, 1/8/85;
 * REVISED BY K.C. NG on 2/7/85, 2/13/85, 3/7/85, 3/30/85, 6/29/85.
 *
 * Required system supported functions :
 *	copysign(x,y)
 *	scalb(x,y)
 *	logb(x)
 *
 * Method :
 *	1. Reduce y to positive by atan2(y,x)=-atan2(-y,x).
 *	2. Reduce x to positive by (if x and y are unexceptional):
 *		ARG (x+iy) = arctan(y/x)   	   ... if x > 0,
 *		ARG (x+iy) = pi - arctan[y/(-x)]   ... if x < 0,
 *	3. According to the integer k=4t+0.25 truncated , t=y/x, the argument
 *	   is further reduced to one of the following intervals and the
 *	   arctangent of y/x is evaluated by the corresponding formula:
 *
 *         [0,7/16]	   atan(y/x) = t - t^3*(a1+t^2*(a2+...(a10+t^2*a11)...)
 *	   [7/16,11/16]    atan(y/x) = atan(1/2) + atan( (y-x/2)/(x+y/2) )
 *	   [11/16.19/16]   atan(y/x) = atan( 1 ) + atan( (y-x)/(x+y) )
 *	   [19/16,39/16]   atan(y/x) = atan(3/2) + atan( (y-1.5x)/(x+1.5y) )
 *	   [39/16,INF]     atan(y/x) = atan(INF) + atan( -x/y )
 *
 * Special cases:
 * Notations: atan2(y,x) == ARG (x+iy) == ARG(x,y).
 *
 *	ARG( NAN , (anything) ) is NaN;
 *	ARG( (anything), NaN ) is NaN;
 *	ARG(+(anything but NaN), +-0) is +-0  ;
 *	ARG(-(anything but NaN), +-0) is +-PI ;
 *	ARG( 0, +-(anything but 0 and NaN) ) is +-PI/2;
 *	ARG( +INF,+-(anything but INF and NaN) ) is +-0 ;
 *	ARG( -INF,+-(anything but INF and NaN) ) is +-PI;
 *	ARG( +INF,+-INF ) is +-PI/4 ;
 *	ARG( -INF,+-INF ) is +-3PI/4;
 *	ARG( (anything but,0,NaN, and INF),+-INF ) is +-PI/2;
 *
 * Accuracy:
 *	atan2(y,x) returns (PI/pi) * the exact ARG (x+iy) nearly rounded,
 *	where
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
 *	In a test run with 356,000 random argument on [-1,1] * [-1,1] on a
 *	VAX, the maximum observed error was 1.41 ulps (units of the last place)
 *	compared with (PI/pi)*(the exact ARG(x+iy)).
 *
 * Note:
 *	We use machine PI (the true pi rounded) in place of the actual
 *	value of pi for all the trig and inverse trig functions. In general,
 *	if trig is one of sin, cos, tan, then computed trig(y) returns the
 *	exact trig(y*pi/PI) nearly rounded; correspondingly, computed arctrig
 *	returns the exact arctrig(y)*PI/pi nearly rounded. These guarantee the
 *	trig functions have period PI, and trig(arctrig(x)) returns x for
 *	all critical values x.
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following constants.
 * The decimal values may be used, provided that the compiler will convert
 * from decimal to binary accurately enough to produce the hexadecimal values
 * shown.
 */

#include "mathimpl.h"

vc(athfhi, 4.6364760900080611433E-1  ,6338,3fed,da7b,2b0d,  -1, .ED63382B0DDA7B)
vc(athflo, 1.9338828231967579916E-19 ,5005,2164,92c0,9cfe, -62, .E450059CFE92C0)
vc(PIo4,   7.8539816339744830676E-1  ,0fda,4049,68c2,a221,   0, .C90FDAA22168C2)
vc(at1fhi, 9.8279372324732906796E-1  ,985e,407b,b4d9,940f,   0, .FB985E940FB4D9)
vc(at1flo,-3.5540295636764633916E-18 ,1edc,a383,eaea,34d6, -57,-.831EDC34D6EAEA)
vc(PIo2,   1.5707963267948966135E0   ,0fda,40c9,68c2,a221,   1, .C90FDAA22168C2)
vc(PI,     3.1415926535897932270E0   ,0fda,4149,68c2,a221,   2, .C90FDAA22168C2)
vc(a1,     3.3333333333333473730E-1  ,aaaa,3faa,ab75,aaaa,  -1, .AAAAAAAAAAAB75)
vc(a2,    -2.0000000000017730678E-1  ,cccc,bf4c,946e,cccd,  -2,-.CCCCCCCCCD946E)
vc(a3,     1.4285714286694640301E-1  ,4924,3f12,4262,9274,  -2, .92492492744262)
vc(a4,    -1.1111111135032672795E-1  ,8e38,bee3,6292,ebc6,  -3,-.E38E38EBC66292)
vc(a5,     9.0909091380563043783E-2  ,2e8b,3eba,d70c,b31b,  -3, .BA2E8BB31BD70C)
vc(a6,    -7.6922954286089459397E-2  ,89c8,be9d,7f18,27c3,  -3,-.9D89C827C37F18)
vc(a7,     6.6663180891693915586E-2  ,86b4,3e88,9e58,ae37,  -3, .8886B4AE379E58)
vc(a8,    -5.8772703698290408927E-2  ,bba5,be70,a942,8481,  -4,-.F0BBA58481A942)
vc(a9,     5.2170707402812969804E-2  ,b0f3,3e55,13ab,a1ab,  -4, .D5B0F3A1AB13AB)
vc(a10,   -4.4895863157820361210E-2  ,e4b9,be37,048f,7fd1,  -4,-.B7E4B97FD1048F)
vc(a11,    3.3006147437343875094E-2  ,3174,3e07,2d87,3cf7,  -4, .8731743CF72D87)
vc(a12,   -1.4614844866464185439E-2  ,731a,bd6f,76d9,2f34,  -6,-.EF731A2F3476D9)

ic(athfhi, 4.6364760900080609352E-1  ,  -2,  1.DAC670561BB4F)
ic(athflo, 4.6249969567426939759E-18 , -58,  1.5543B8F253271)
ic(PIo4,   7.8539816339744827900E-1  ,  -1,  1.921FB54442D18)
ic(at1fhi, 9.8279372324732905408E-1  ,  -1,  1.F730BD281F69B)
ic(at1flo,-2.4407677060164810007E-17 , -56, -1.C23DFEFEAE6B5)
ic(PIo2,   1.5707963267948965580E0   ,   0,  1.921FB54442D18)
ic(PI,     3.1415926535897931160E0   ,   1,  1.921FB54442D18)
ic(a1,     3.3333333333333942106E-1  ,  -2,  1.55555555555C3)
ic(a2,    -1.9999999999979536924E-1  ,  -3, -1.9999999997CCD)
ic(a3,     1.4285714278004377209E-1  ,  -3,  1.24924921EC1D7)
ic(a4,    -1.1111110579344973814E-1  ,  -4, -1.C71C7059AF280)
ic(a5,     9.0908906105474668324E-2  ,  -4,  1.745CE5AA35DB2)
ic(a6,    -7.6919217767468239799E-2  ,  -4, -1.3B0FA54BEC400)
ic(a7,     6.6614695906082474486E-2  ,  -4,  1.10DA924597FFF)
ic(a8,    -5.8358371008508623523E-2  ,  -5, -1.DE125FDDBD793)
ic(a9,     4.9850617156082015213E-2  ,  -5,  1.9860524BDD807)
ic(a10,   -3.6700606902093604877E-2  ,  -5, -1.2CA6C04C6937A)
ic(a11,    1.6438029044759730479E-2  ,  -6,  1.0D52174A1BB54)

#ifdef vccast
#define	athfhi	vccast(athfhi)
#define	athflo	vccast(athflo)
#define	PIo4	vccast(PIo4)
#define	at1fhi	vccast(at1fhi)
#define	at1flo	vccast(at1flo)
#define	PIo2	vccast(PIo2)
#define	PI	vccast(PI)
#define	a1	vccast(a1)
#define	a2	vccast(a2)
#define	a3	vccast(a3)
#define	a4	vccast(a4)
#define	a5	vccast(a5)
#define	a6	vccast(a6)
#define	a7	vccast(a7)
#define	a8	vccast(a8)
#define	a9	vccast(a9)
#define	a10	vccast(a10)
#define	a11	vccast(a11)
#define	a12	vccast(a12)
#endif

double atan2(y,x)
double  y,x;
{
	static const double zero=0, one=1, small=1.0E-9, big=1.0E18;
	double t,z,signy,signx,hi,lo;
	int k,m;

#if !defined(vax)&&!defined(tahoe)
    /* if x or y is NAN */
	if(x!=x) return(x); if(y!=y) return(y);
#endif	/* !defined(vax)&&!defined(tahoe) */

    /* copy down the sign of y and x */
	signy = copysign(one,y) ;
	signx = copysign(one,x) ;

    /* if x is 1.0, goto begin */
	if(x==1) { y=copysign(y,one); t=y; if(finite(t)) goto begin;}

    /* when y = 0 */
	if(y==zero) return((signx==one)?y:copysign(PI,signy));

    /* when x = 0 */
	if(x==zero) return(copysign(PIo2,signy));

    /* when x is INF */
	if(!finite(x))
	    if(!finite(y))
		return(copysign((signx==one)?PIo4:3*PIo4,signy));
	    else
		return(copysign((signx==one)?zero:PI,signy));

    /* when y is INF */
	if(!finite(y)) return(copysign(PIo2,signy));

    /* compute y/x */
	x=copysign(x,one);
	y=copysign(y,one);
	if((m=(k=logb(y))-logb(x)) > 60) t=big+big;
	    else if(m < -80 ) t=y/x;
	    else { t = y/x ; y = scalb(y,-k); x=scalb(x,-k); }

    /* begin argument reduction */
begin:
	if (t < 2.4375) {

	/* truncate 4(t+1/16) to integer for branching */
	    k = 4 * (t+0.0625);
	    switch (k) {

	    /* t is in [0,7/16] */
	    case 0:
	    case 1:
		if (t < small)
		    { big + small ;  /* raise inexact flag */
		      return (copysign((signx>zero)?t:PI-t,signy)); }

		hi = zero;  lo = zero;  break;

	    /* t is in [7/16,11/16] */
	    case 2:
		hi = athfhi; lo = athflo;
		z = x+x;
		t = ( (y+y) - x ) / ( z +  y ); break;

	    /* t is in [11/16,19/16] */
	    case 3:
	    case 4:
		hi = PIo4; lo = zero;
		t = ( y - x ) / ( x + y ); break;

	    /* t is in [19/16,39/16] */
	    default:
		hi = at1fhi; lo = at1flo;
		z = y-x; y=y+y+y; t = x+x;
		t = ( (z+z)-x ) / ( t + y ); break;
	    }
	}
	/* end of if (t < 2.4375) */

	else
	{
	    hi = PIo2; lo = zero;

	    /* t is in [2.4375, big] */
	    if (t <= big)  t = - x / y;

	    /* t is in [big, INF] */
	    else
	      { big+small;	/* raise inexact flag */
		t = zero; }
	}
    /* end of argument reduction */

    /* compute atan(t) for t in [-.4375, .4375] */
	z = t*t;
#if defined(vax)||defined(tahoe)
	z = t*(z*(a1+z*(a2+z*(a3+z*(a4+z*(a5+z*(a6+z*(a7+z*(a8+
			z*(a9+z*(a10+z*(a11+z*a12))))))))))));
#else	/* defined(vax)||defined(tahoe) */
	z = t*(z*(a1+z*(a2+z*(a3+z*(a4+z*(a5+z*(a6+z*(a7+z*(a8+
			z*(a9+z*(a10+z*a11)))))))))));
#endif	/* defined(vax)||defined(tahoe) */
	z = lo - z; z += t; z += hi;

	return(copysign((signx>zero)?z:PI-z,signy));
}
