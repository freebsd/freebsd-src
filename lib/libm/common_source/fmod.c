/*
 * Copyright (c) 1989 The Regents of the University of California.
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
static char sccsid[] = "@(#)fmod.c	5.2 (Berkeley) 6/1/90";
#endif /* not lint */

/* fmod.c
 *
 * SYNOPSIS
 *
 *    #include <math.h>
 *    double fmod(double x, double y)
 *
 * DESCRIPTION
 *
 *    The fmod function computes the floating-point remainder of x/y.
 *
 * RETURNS
 *
 *    The fmod function returns the value x-i*y, for some integer i
 * such that, if y is nonzero, the result has the same sign as x and
 * magnitude less than the magnitude of y.
 *
 * On a VAX or CCI,
 *
 *    fmod(x,0) traps/faults on floating-point divided-by-zero.
 *
 * On IEEE-754 conforming machines with "isnan()" primitive,
 *
 *    fmod(x,0), fmod(INF,y) are invalid operations and NaN is returned.
 *
 */
#if !defined(vax) && !defined(tahoe)
extern int isnan(),finite();
#endif	/* !defined(vax) && !defined(tahoe) */
extern double frexp(),ldexp(),fabs();

#ifdef TEST_FMOD
static double
_fmod(x,y)
#else	/* TEST_FMOD */
double
fmod(x,y)
#endif	/* TEST_FMOD */
double x,y;
{
	int ir,iy;
	double r,w;

	if (y == (double)0
#if !defined(vax) && !defined(tahoe)	/* per "fmod" manual entry, SunOS 4.0 */
		|| isnan(y) || !finite(x)
#endif	/* !defined(vax) && !defined(tahoe) */
	    )
	    return (x*y)/(x*y);

	r = fabs(x);
	y = fabs(y);
	(void)frexp(y,&iy);
	while (r >= y) {
		(void)frexp(r,&ir);
		w = ldexp(y,ir-iy);
		r -= w <= r ? w : w*(double)0.5;
	}
	return x >= (double)0 ? r : -r;
}

#ifdef TEST_FMOD
extern long random();
extern double fmod();

#define	NTEST	10000
#define	NCASES	3

static int nfail = 0;

static void
doit(x,y)
double x,y;
{
	double ro = fmod(x,y),rn = _fmod(x,y);
	if (ro != rn) {
		(void)printf(" x    = 0x%08.8x %08.8x (%24.16e)\n",x,x);
		(void)printf(" y    = 0x%08.8x %08.8x (%24.16e)\n",y,y);
		(void)printf(" fmod = 0x%08.8x %08.8x (%24.16e)\n",ro,ro);
		(void)printf("_fmod = 0x%08.8x %08.8x (%24.16e)\n",rn,rn);
		(void)printf("\n");
	}
}

main()
{
	register int i,cases;
	double x,y;

	srandom(12345);
	for (i = 0; i < NTEST; i++) {
		x = (double)random();
		y = (double)random();
		for (cases = 0; cases < NCASES; cases++) {
			switch (cases) {
			case 0:
				break;
			case 1:
				y = (double)1/y; break;
			case 2:
				x = (double)1/x; break;
			default:
				abort(); break;
			}
			doit(x,y);
			doit(x,-y);
			doit(-x,y);
			doit(-x,-y);
		}
	}
	if (nfail)
		(void)printf("Number of failures: %d (out of a total of %d)\n",
			nfail,NTEST*NCASES*4);
	else
		(void)printf("No discrepancies were found\n");
	exit(0);
}
#endif	/* TEST_FMOD */
