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
static char sccsid[] = "@(#)j1.c	8.2 (Berkeley) 11/30/93";
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

/* double j1(double x), y1(double x)
 * Bessel function of the first and second kinds of order zero.
 * Method -- j1(x):
 *	1. For tiny x, we use j1(x) = x/2 - x^3/16 + x^5/384 - ...
 *	2. Reduce x to |x| since j1(x)=-j1(-x),  and
 *	   for x in (0,2)
 *		j1(x) = x/2 + x*z*R0/S0,  where z = x*x;
 *	   (precision:  |j1/x - 1/2 - R0/S0 |<2**-61.51 )
 *	   for x in (2,inf)
 * 		j1(x) = sqrt(2/(pi*x))*(p1(x)*cos(x1)-q1(x)*sin(x1))
 * 		y1(x) = sqrt(2/(pi*x))*(p1(x)*sin(x1)+q1(x)*cos(x1))
 * 	   where x1 = x-3*pi/4. It is better to compute sin(x1),cos(x1)
 *	   as follows:
 *		cos(x1) =  cos(x)cos(3pi/4)+sin(x)sin(3pi/4)
 *			=  1/sqrt(2) * (sin(x) - cos(x))
 *		sin(x1) =  sin(x)cos(3pi/4)-cos(x)sin(3pi/4)
 *			= -1/sqrt(2) * (sin(x) + cos(x))
 * 	   (To avoid cancellation, use
 *		sin(x) +- cos(x) = -cos(2x)/(sin(x) -+ cos(x))
 * 	    to compute the worse one.)
 *
 *	3 Special cases
 *		j1(nan)= nan
 *		j1(0) = 0
 *		j1(inf) = 0
 *
 * Method -- y1(x):
 *	1. screen out x<=0 cases: y1(0)=-inf, y1(x<0)=NaN
 *	2. For x<2.
 *	   Since
 *		y1(x) = 2/pi*(j1(x)*(ln(x/2)+Euler)-1/x-x/2+5/64*x^3-...)
 *	   therefore y1(x)-2/pi*j1(x)*ln(x)-1/x is an odd function.
 *	   We use the following function to approximate y1,
 *		y1(x) = x*U(z)/V(z) + (2/pi)*(j1(x)*ln(x)-1/x), z= x^2
 *	   where for x in [0,2] (abs err less than 2**-65.89)
 *		U(z) = u0 + u1*z + ... + u4*z^4
 *		V(z) = 1  + v1*z + ... + v5*z^5
 *	   Note: For tiny x, 1/x dominate y1 and hence
 *		y1(tiny) = -2/pi/tiny, (choose tiny<2**-54)
 *	3. For x>=2.
 * 		y1(x) = sqrt(2/(pi*x))*(p1(x)*sin(x1)+q1(x)*cos(x1))
 * 	   where x1 = x-3*pi/4. It is better to compute sin(x1),cos(x1)
 *	   by method mentioned above.
 */

#include <math.h>
#include <float.h>

#if defined(vax) || defined(tahoe)
#define _IEEE	0
#else
#define _IEEE	1
#define infnan(x) (0.0)
#endif

static double pone(), qone();

static double
huge    = 1e300,
zero    = 0.0,
one	= 1.0,
invsqrtpi= 5.641895835477562869480794515607725858441e-0001,
tpi	= 0.636619772367581343075535053490057448,

	/* R0/S0 on [0,2] */
r00 =  -6.250000000000000020842322918309200910191e-0002,
r01 =   1.407056669551897148204830386691427791200e-0003,
r02 =  -1.599556310840356073980727783817809847071e-0005,
r03 =   4.967279996095844750387702652791615403527e-0008,
s01 =   1.915375995383634614394860200531091839635e-0002,
s02 =   1.859467855886309024045655476348872850396e-0004,
s03 =   1.177184640426236767593432585906758230822e-0006,
s04 =   5.046362570762170559046714468225101016915e-0009,
s05 =   1.235422744261379203512624973117299248281e-0011;

#define two_129	6.80564733841876926e+038	/* 2^129 */
#define two_m54	5.55111512312578270e-017	/* 2^-54 */
double j1(x)
	double x;
{
	double z, s,c,ss,cc,r,u,v,y;
	y = fabs(x);
	if (!finite(x))			/* Inf or NaN */
		if (_IEEE && x != x)
			return(x);
		else
			return (copysign(x, zero));
	y = fabs(x);
	if (y >= 2)			/* |x| >= 2.0 */
	{
		s = sin(y);
		c = cos(y);
		ss = -s-c;
		cc = s-c;
		if (y < .5*DBL_MAX) {  	/* make sure y+y not overflow */
		    z = cos(y+y);
		    if ((s*c)<zero) cc = z/ss;
		    else 	    ss = z/cc;
		}
	/*
	 * j1(x) = 1/sqrt(pi) * (P(1,x)*cc - Q(1,x)*ss) / sqrt(x)
	 * y1(x) = 1/sqrt(pi) * (P(1,x)*ss + Q(1,x)*cc) / sqrt(x)
	 */
#if !defined(vax) && !defined(tahoe)
		if (y > two_129)	 /* x > 2^129 */
			z = (invsqrtpi*cc)/sqrt(y);
		else
#endif /* defined(vax) || defined(tahoe) */
		{
		    u = pone(y); v = qone(y);
		    z = invsqrtpi*(u*cc-v*ss)/sqrt(y);
		}
		if (x < 0) return -z;
		else  	 return  z;
	}
	if (y < 7.450580596923828125e-009) {	/* |x|<2**-27 */
	    if(huge+x>one) return 0.5*x;/* inexact if x!=0 necessary */
	}
	z = x*x;
	r =  z*(r00+z*(r01+z*(r02+z*r03)));
	s =  one+z*(s01+z*(s02+z*(s03+z*(s04+z*s05))));
	r *= x;
	return (x*0.5+r/s);
}

static double u0[5] = {
  -1.960570906462389484206891092512047539632e-0001,
   5.044387166398112572026169863174882070274e-0002,
  -1.912568958757635383926261729464141209569e-0003,
   2.352526005616105109577368905595045204577e-0005,
   -9.190991580398788465315411784276789663849e-0008,
};
static double v0[5] = {
   1.991673182366499064031901734535479833387e-0002,
   2.025525810251351806268483867032781294682e-0004,
   1.356088010975162198085369545564475416398e-0006,
   6.227414523646214811803898435084697863445e-0009,
   1.665592462079920695971450872592458916421e-0011,
};

double y1(x)
	double x;
{
	double z, s, c, ss, cc, u, v;
    /* if Y1(NaN) is NaN, Y1(-inf) is NaN, Y1(inf) is 0 */
	if (!finite(x))
		if (!_IEEE) return (infnan(EDOM));
		else if (x < 0)
			return(zero/zero);
		else if (x > 0)
			return (0);
		else
			return(x);
	if (x <= 0) {
		if (_IEEE && x == 0) return -one/zero;
		else if(x == 0) return(infnan(-ERANGE));
		else if(_IEEE) return (zero/zero);
		else return(infnan(EDOM));
	}
        if (x >= 2)			 /* |x| >= 2.0 */
	{
                s = sin(x);
                c = cos(x);
                ss = -s-c;
                cc = s-c;
		if (x < .5 * DBL_MAX)	/* make sure x+x not overflow */
		{
                    z = cos(x+x);
                    if ((s*c)>zero) cc = z/ss;
                    else            ss = z/cc;
                }
        /* y1(x) = sqrt(2/(pi*x))*(p1(x)*sin(x0)+q1(x)*cos(x0))
         * where x0 = x-3pi/4
         *      Better formula:
         *              cos(x0) = cos(x)cos(3pi/4)+sin(x)sin(3pi/4)
         *                      =  1/sqrt(2) * (sin(x) - cos(x))
         *              sin(x0) = sin(x)cos(3pi/4)-cos(x)sin(3pi/4)
         *                      = -1/sqrt(2) * (cos(x) + sin(x))
         * To avoid cancellation, use
         *              sin(x) +- cos(x) = -cos(2x)/(sin(x) -+ cos(x))
         * to compute the worse one.
         */
                if (_IEEE && x>two_129)
			z = (invsqrtpi*ss)/sqrt(x);
                else {
                    u = pone(x); v = qone(x);
                    z = invsqrtpi*(u*ss+v*cc)/sqrt(x);
                }
                return z;
        }
        if (x <= two_m54) {    /* x < 2**-54 */
            return (-tpi/x);
        }
        z = x*x;
        u = u0[0]+z*(u0[1]+z*(u0[2]+z*(u0[3]+z*u0[4])));
        v = one+z*(v0[0]+z*(v0[1]+z*(v0[2]+z*(v0[3]+z*v0[4]))));
        return (x*(u/v) + tpi*(j1(x)*log(x)-one/x));
}

/* For x >= 8, the asymptotic expansions of pone is
 *	1 + 15/128 s^2 - 4725/2^15 s^4 - ...,	where s = 1/x.
 * We approximate pone by
 * 	pone(x) = 1 + (R/S)
 * where  R = pr0 + pr1*s^2 + pr2*s^4 + ... + pr5*s^10
 * 	  S = 1 + ps0*s^2 + ... + ps4*s^10
 * and
 *	| pone(x)-1-R/S | <= 2  ** ( -60.06)
 */

static double pr8[6] = { /* for x in [inf, 8]=1/[0,0.125] */
   0.0,
   1.171874999999886486643746274751925399540e-0001,
   1.323948065930735690925827997575471527252e+0001,
   4.120518543073785433325860184116512799375e+0002,
   3.874745389139605254931106878336700275601e+0003,
   7.914479540318917214253998253147871806507e+0003,
};
static double ps8[5] = {
   1.142073703756784104235066368252692471887e+0002,
   3.650930834208534511135396060708677099382e+0003,
   3.695620602690334708579444954937638371808e+0004,
   9.760279359349508334916300080109196824151e+0004,
   3.080427206278887984185421142572315054499e+0004,
};

static double pr5[6] = { /* for x in [8,4.5454]=1/[0.125,0.22001] */
   1.319905195562435287967533851581013807103e-0011,
   1.171874931906140985709584817065144884218e-0001,
   6.802751278684328781830052995333841452280e+0000,
   1.083081829901891089952869437126160568246e+0002,
   5.176361395331997166796512844100442096318e+0002,
   5.287152013633375676874794230748055786553e+0002,
};
static double ps5[5] = {
   5.928059872211313557747989128353699746120e+0001,
   9.914014187336144114070148769222018425781e+0002,
   5.353266952914879348427003712029704477451e+0003,
   7.844690317495512717451367787640014588422e+0003,
   1.504046888103610723953792002716816255382e+0003,
};

static double pr3[6] = {/* for x in [4.547,2.8571]=1/[0.2199,0.35001] */
   3.025039161373736032825049903408701962756e-0009,
   1.171868655672535980750284752227495879921e-0001,
   3.932977500333156527232725812363183251138e+0000,
   3.511940355916369600741054592597098912682e+0001,
   9.105501107507812029367749771053045219094e+0001,
   4.855906851973649494139275085628195457113e+0001,
};
static double ps3[5] = {
   3.479130950012515114598605916318694946754e+0001,
   3.367624587478257581844639171605788622549e+0002,
   1.046871399757751279180649307467612538415e+0003,
   8.908113463982564638443204408234739237639e+0002,
   1.037879324396392739952487012284401031859e+0002,
};

static double pr2[6] = {/* for x in [2.8570,2]=1/[0.3499,0.5] */
   1.077108301068737449490056513753865482831e-0007,
   1.171762194626833490512746348050035171545e-0001,
   2.368514966676087902251125130227221462134e+0000,
   1.224261091482612280835153832574115951447e+0001,
   1.769397112716877301904532320376586509782e+0001,
   5.073523125888185399030700509321145995160e+0000,
};
static double ps2[5] = {
   2.143648593638214170243114358933327983793e+0001,
   1.252902271684027493309211410842525120355e+0002,
   2.322764690571628159027850677565128301361e+0002,
   1.176793732871470939654351793502076106651e+0002,
   8.364638933716182492500902115164881195742e+0000,
};

static double pone(x)
	double x;
{
	double *p,*q,z,r,s;
	if (x >= 8.0) 			   {p = pr8; q= ps8;}
	else if (x >= 4.54545211791992188) {p = pr5; q= ps5;}
	else if (x >= 2.85714149475097656) {p = pr3; q= ps3;}
	else /* if (x >= 2.0) */	   {p = pr2; q= ps2;}
	z = one/(x*x);
	r = p[0]+z*(p[1]+z*(p[2]+z*(p[3]+z*(p[4]+z*p[5]))));
	s = one+z*(q[0]+z*(q[1]+z*(q[2]+z*(q[3]+z*q[4]))));
	return (one + r/s);
}


/* For x >= 8, the asymptotic expansions of qone is
 *	3/8 s - 105/1024 s^3 - ..., where s = 1/x.
 * We approximate pone by
 * 	qone(x) = s*(0.375 + (R/S))
 * where  R = qr1*s^2 + qr2*s^4 + ... + qr5*s^10
 * 	  S = 1 + qs1*s^2 + ... + qs6*s^12
 * and
 *	| qone(x)/s -0.375-R/S | <= 2  ** ( -61.13)
 */

static double qr8[6] = { /* for x in [inf, 8]=1/[0,0.125] */
   0.0,
  -1.025390624999927207385863635575804210817e-0001,
  -1.627175345445899724355852152103771510209e+0001,
  -7.596017225139501519843072766973047217159e+0002,
  -1.184980667024295901645301570813228628541e+0004,
  -4.843851242857503225866761992518949647041e+0004,
};
static double qs8[6] = {
   1.613953697007229231029079421446916397904e+0002,
   7.825385999233484705298782500926834217525e+0003,
   1.338753362872495800748094112937868089032e+0005,
   7.196577236832409151461363171617204036929e+0005,
   6.666012326177764020898162762642290294625e+0005,
  -2.944902643038346618211973470809456636830e+0005,
};

static double qr5[6] = { /* for x in [8,4.5454]=1/[0.125,0.22001] */
  -2.089799311417640889742251585097264715678e-0011,
  -1.025390502413754195402736294609692303708e-0001,
  -8.056448281239359746193011295417408828404e+0000,
  -1.836696074748883785606784430098756513222e+0002,
  -1.373193760655081612991329358017247355921e+0003,
  -2.612444404532156676659706427295870995743e+0003,
};
static double qs5[6] = {
   8.127655013843357670881559763225310973118e+0001,
   1.991798734604859732508048816860471197220e+0003,
   1.746848519249089131627491835267411777366e+0004,
   4.985142709103522808438758919150738000353e+0004,
   2.794807516389181249227113445299675335543e+0004,
  -4.719183547951285076111596613593553911065e+0003,
};

static double qr3[6] = {/* for x in [4.547,2.8571]=1/[0.2199,0.35001] */
  -5.078312264617665927595954813341838734288e-0009,
  -1.025378298208370901410560259001035577681e-0001,
  -4.610115811394734131557983832055607679242e+0000,
  -5.784722165627836421815348508816936196402e+0001,
  -2.282445407376317023842545937526967035712e+0002,
  -2.192101284789093123936441805496580237676e+0002,
};
static double qs3[6] = {
   4.766515503237295155392317984171640809318e+0001,
   6.738651126766996691330687210949984203167e+0002,
   3.380152866795263466426219644231687474174e+0003,
   5.547729097207227642358288160210745890345e+0003,
   1.903119193388108072238947732674639066045e+0003,
  -1.352011914443073322978097159157678748982e+0002,
};

static double qr2[6] = {/* for x in [2.8570,2]=1/[0.3499,0.5] */
  -1.783817275109588656126772316921194887979e-0007,
  -1.025170426079855506812435356168903694433e-0001,
  -2.752205682781874520495702498875020485552e+0000,
  -1.966361626437037351076756351268110418862e+0001,
  -4.232531333728305108194363846333841480336e+0001,
  -2.137192117037040574661406572497288723430e+0001,
};
static double qs2[6] = {
   2.953336290605238495019307530224241335502e+0001,
   2.529815499821905343698811319455305266409e+0002,
   7.575028348686454070022561120722815892346e+0002,
   7.393932053204672479746835719678434981599e+0002,
   1.559490033366661142496448853793707126179e+0002,
  -4.959498988226281813825263003231704397158e+0000,
};

static double qone(x)
	double x;
{
	double *p,*q, s,r,z;
	if (x >= 8.0)			   {p = qr8; q= qs8;}
	else if (x >= 4.54545211791992188) {p = qr5; q= qs5;}
	else if (x >= 2.85714149475097656) {p = qr3; q= qs3;}
	else /* if (x >= 2.0) */	   {p = qr2; q= qs2;}
	z = one/(x*x);
	r = p[0]+z*(p[1]+z*(p[2]+z*(p[3]+z*(p[4]+z*p[5]))));
	s = one+z*(q[0]+z*(q[1]+z*(q[2]+z*(q[3]+z*(q[4]+z*q[5])))));
	return (.375 + r/s)/x;
}
