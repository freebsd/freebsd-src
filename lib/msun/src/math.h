/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

/*
 * from: @(#)fdlibm.h 5.1 93/09/24
 * $FreeBSD$
 */

#ifndef _MATH_H_
#define	_MATH_H_

/*
 * ANSI/POSIX
 */
extern char __infinity[];
#define HUGE_VAL	(*(double *) __infinity)

/*
 * XOPEN/SVID
 */
#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
#define	M_E		2.7182818284590452354	/* e */
#define	M_LOG2E		1.4426950408889634074	/* log 2e */
#define	M_LOG10E	0.43429448190325182765	/* log 10e */
#define	M_LN2		0.69314718055994530942	/* log e2 */
#define	M_LN10		2.30258509299404568402	/* log e10 */
#define	M_PI		3.14159265358979323846	/* pi */
#define	M_PI_2		1.57079632679489661923	/* pi/2 */
#define	M_PI_4		0.78539816339744830962	/* pi/4 */
#define	M_1_PI		0.31830988618379067154	/* 1/pi */
#define	M_2_PI		0.63661977236758134308	/* 2/pi */
#define	M_2_SQRTPI	1.12837916709551257390	/* 2/sqrt(pi) */
#define	M_SQRT2		1.41421356237309504880	/* sqrt(2) */
#define	M_SQRT1_2	0.70710678118654752440	/* 1/sqrt(2) */

#define	MAXFLOAT	((float)3.40282346638528860e+38)
extern int signgam;

#if !defined(_XOPEN_SOURCE)
enum fdversion {fdlibm_ieee = -1, fdlibm_svid, fdlibm_xopen, fdlibm_posix};

#define _LIB_VERSION_TYPE enum fdversion
#define _LIB_VERSION _fdlib_version

/* if global variable _LIB_VERSION is not desirable, one may
 * change the following to be a constant by:
 *	#define _LIB_VERSION_TYPE const enum version
 * In that case, after one initializes the value _LIB_VERSION (see
 * s_lib_version.c) during compile time, it cannot be modified
 * in the middle of a program
 */
extern  _LIB_VERSION_TYPE  _LIB_VERSION;

#define _IEEE_  fdlibm_ieee
#define _SVID_  fdlibm_svid
#define _XOPEN_ fdlibm_xopen
#define _POSIX_ fdlibm_posix

/* We have a problem when using C++ since `exception' is a reserved
   name in C++.  */
#ifndef __cplusplus
struct exception {
	int type;
	char *name;
	double arg1;
	double arg2;
	double retval;
};
#endif

#define	HUGE		MAXFLOAT

/*
 * set X_TLOSS = pi*2**52, which is possibly defined in <values.h>
 * (one may replace the following line by "#include <values.h>")
 */

#define X_TLOSS		1.41484755040568800000e+16

#define	DOMAIN		1
#define	SING		2
#define	OVERFLOW	3
#define	UNDERFLOW	4
#define	TLOSS		5
#define	PLOSS		6

#endif /* !_XOPEN_SOURCE */
#endif /* !_ANSI_SOURCE && !_POSIX_SOURCE */


#include <sys/cdefs.h>
/*
 * ANSI/POSIX
 */
__BEGIN_DECLS
double	acos __P((double));
double	asin __P((double));
double	atan __P((double));
double	atan2 __P((double, double));
double	cos __P((double));
double	sin __P((double));
double	tan __P((double));

double	cosh __P((double));
double	sinh __P((double));
double	tanh __P((double));

double	exp __P((double));
double	frexp __P((double, int *));
double	ldexp __P((double, int));
double	log __P((double));
double	log10 __P((double));
double	modf __P((double, double *));

double	pow __P((double, double));
double	sqrt __P((double));

double	ceil __P((double));
double	fabs __P((double));
double	floor __P((double));
double	fmod __P((double, double));

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
double	erf __P((double));
double	erfc __P((double));
double	gamma __P((double));
double	hypot __P((double, double));
int	isinf __P((double));
int	isnan __P((double));
int	finite __P((double));
double	j0 __P((double));
double	j1 __P((double));
double	jn __P((int, double));
double	lgamma __P((double));
double	y0 __P((double));
double	y1 __P((double));
double	yn __P((int, double));

#if !defined(_XOPEN_SOURCE)
double	acosh __P((double));
double	asinh __P((double));
double	atanh __P((double));
double	cbrt __P((double));
double	logb __P((double));
double	nextafter __P((double, double));
double	remainder __P((double, double));
double	scalb __P((double, double));

#ifndef __cplusplus
int	matherr __P((struct exception *));
#endif

/*
 * IEEE Test Vector
 */
double	significand __P((double));

/*
 * Functions callable from C, intended to support IEEE arithmetic.
 */
double	copysign __P((double, double));
int	ilogb __P((double));
double	rint __P((double));
double	scalbn __P((double, int));

/*
 * BSD math library entry points
 */
double	drem __P((double, double));
double	expm1 __P((double));
double	log1p __P((double));

/*
 * Reentrant version of gamma & lgamma; passes signgam back by reference
 * as the second argument; user must allocate space for signgam.
 */
#ifdef _REENTRANT
double	gamma_r __P((double, int *));
double	lgamma_r __P((double, int *));
#endif /* _REENTRANT */

/* float versions of ANSI/POSIX functions */
float	acosf __P((float));
float	asinf __P((float));
float	atanf __P((float));
float	atan2f __P((float, float));
float	cosf __P((float));
float	sinf __P((float));
float	tanf __P((float));

float	coshf __P((float));
float	sinhf __P((float));
float	tanhf __P((float));

float	expf __P((float));
float	frexpf __P((float, int *));
float	ldexpf __P((float, int));
float	logf __P((float));
float	log10f __P((float));
float	modff __P((float, float *));

float	powf __P((float, float));
float	sqrtf __P((float));

float	ceilf __P((float));
float	fabsf __P((float));
float	floorf __P((float));
float	fmodf __P((float, float));

float	erff __P((float));
float	erfcf __P((float));
float	gammaf __P((float));
float	hypotf __P((float, float));
int	isnanf __P((float));
int	finitef __P((float));
float	j0f __P((float));
float	j1f __P((float));
float	jnf __P((int, float));
float	lgammaf __P((float));
float	y0f __P((float));
float	y1f __P((float));
float	ynf __P((int, float));

float	acoshf __P((float));
float	asinhf __P((float));
float	atanhf __P((float));
float	cbrtf __P((float));
float	logbf __P((float));
float	nextafterf __P((float, float));
float	remainderf __P((float, float));
float	scalbf __P((float, float));

/*
 * float version of IEEE Test Vector
 */
float	significandf __P((float));

/*
 * Float versions of functions callable from C, intended to support
 * IEEE arithmetic.
 */
float	copysignf __P((float, float));
int	ilogbf __P((float));
float	rintf __P((float));
float	scalbnf __P((float, int));

/*
 * float versions of BSD math library entry points
 */
float	dremf __P((float, float));
float	expm1f __P((float));
float	log1pf __P((float));

/*
 * Float versions of reentrant version of gamma & lgamma; passes
 * signgam back by reference as the second argument; user must
 * allocate space for signgam.
 */
#ifdef _REENTRANT
float	gammaf_r __P((float, int *));
float	lgammaf_r __P((float, int *));
#endif	/* _REENTRANT */

#endif /* !_XOPEN_SOURCE */
#endif /* !_ANSI_SOURCE && !_POSIX_SOURCE */
__END_DECLS

#endif /* !_MATH_H_ */
