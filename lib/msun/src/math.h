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

#include <sys/_types.h>

/*
 * ANSI/POSIX
 */
extern const union __infinity_un {
	unsigned char	__uc[8];
	double		__ud;
} __infinity;

extern const union __nan_un {
	unsigned char	__uc[sizeof(float)];
	float		__uf;
} __nan;

#define	FP_ILOGB0	(-0x7fffffff - 1)	/* INT_MIN */
#define	FP_ILOGBNAN	0x7fffffff		/* INT_MAX */
#define HUGE_VAL	(__infinity.__ud)
#define	HUGE_VALF	(float)HUGE_VAL
#define	HUGE_VALL	(long double)HUGE_VAL
#define	INFINITY	HUGE_VALF
#define	NAN		(__nan.__uf)

/* Symbolic constants to classify floating point numbers. */
#define	FP_INFINITE	0x01
#define	FP_NAN		0x02
#define	FP_NORMAL	0x04
#define	FP_SUBNORMAL	0x08
#define	FP_ZERO		0x10
#define	fpclassify(x) \
    ((sizeof (x) == sizeof (float)) ? __fpclassifyf(x) \
    : (sizeof (x) == sizeof (double)) ? __fpclassifyd(x) \
    : __fpclassifyl(x))

#define	isfinite(x)	(fpclassify(x) & (FP_INFINITE|FP_NAN) == 0)
#define	isinf(x)	(fpclassify(x) == FP_INFINITE)
#define	isnan(x)	(fpclassify(x) == FP_NAN)
#define	isnanf(x)      	isnan(x)
#define	isnormal(x)	(fpclassify(x) == FP_NORMAL)

#define	isgreater(x, y)		(!isunordered((x), (y)) && (x) > (y))
#define	isgreaterequal(x, y)	(!isunordered((x), (y)) && (x) >= (y))
#define	isless(x, y)		(!isunordered((x), (y)) && (x) < (y))
#define	islessequal(x, y)	(!isunordered((x), (y)) && (x) <= (y))
#define	islessgreater(x, y)	(!isunordered((x), (y)) && \
					((x) > (y) || (y) > (x)))
#define	isunordered(x, y)	(isnan(x) || isnan(y))

#define	signbit(x)	__signbit(x)

typedef	__double_t	double_t;
typedef	__float_t	float_t;

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

#if 0
/* Old value from 4.4BSD-Lite math.h; this is probably better. */
#define	HUGE		HUGE_VAL
#else
#define	HUGE		MAXFLOAT
#endif

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
 * Most of these functions have the side effect of setting errno, so they
 * are not declared as __pure2.  (XXX: this point needs to be revisited,
 * since C99 doesn't require the mistake of setting errno, and we mostly
 * don't set it anyway.  In C99, pragmas and functions for changing the
 * rounding mode affect the purity of these functions.)
 */
__BEGIN_DECLS
/*
 * ANSI/POSIX
 */
int	__fpclassifyd(double) __pure2;
int	__fpclassifyf(float) __pure2;
int	__fpclassifyl(long double) __pure2;
int	__signbit(double) __pure2;

double	acos(double);
double	asin(double);
double	atan(double);
double	atan2(double, double);
double	cos(double);
double	sin(double);
double	tan(double);

double	cosh(double);
double	sinh(double);
double	tanh(double);

double	exp(double);
double	frexp(double, int *);	/* fundamentally !__pure2 */
double	ldexp(double, int);
double	log(double);
double	log10(double);
double	modf(double, double *);	/* fundamentally !__pure2 */

double	pow(double, double);
double	sqrt(double);

double	ceil(double);
double	fabs(double);
double	floor(double);
double	fmod(double, double);

/*
 * These functions are not in C90 so they can be "right".  The ones that
 * never set errno in lib/msun are declared as __pure2.
 */
#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
double	erf(double);
double	erfc(double) __pure2;
int	finite(double) __pure2;
double	gamma(double);
double	hypot(double, double);
double	j0(double);
double	j1(double);
double	jn(int, double);
double	lgamma(double);
double	y0(double);
double	y1(double);
double	yn(int, double);

#if !defined(_XOPEN_SOURCE)
double	acosh(double);
double	asinh(double);
double	atanh(double);
double	cbrt(double) __pure2;
double	logb(double) __pure2;
double	nextafter(double, double);
double	remainder(double, double);
double	scalb(double, double);
double	tgamma(double);

#ifndef __cplusplus
int	matherr(struct exception *);
#endif

/*
 * IEEE Test Vector
 */
double	significand(double);

/*
 * Functions callable from C, intended to support IEEE arithmetic.
 */
double	copysign(double, double) __pure2;
int	ilogb(double);
double	rint(double) __pure2;
double	scalbn(double, int);

/*
 * BSD math library entry points
 */
double	drem(double, double);
double	expm1(double) __pure2;
double	log1p(double) __pure2;

/*
 * Reentrant version of gamma & lgamma; passes signgam back by reference
 * as the second argument; user must allocate space for signgam.
 */
#ifdef _REENTRANT
double	gamma_r(double, int *);
double	lgamma_r(double, int *);
#endif /* _REENTRANT */

/* float versions of ANSI/POSIX functions */
float	acosf(float);
float	asinf(float);
float	atanf(float);
float	atan2f(float, float);
float	cosf(float);
float	sinf(float);
float	tanf(float);

float	coshf(float);
float	sinhf(float);
float	tanhf(float);

float	expf(float);
float	frexpf(float, int *);	/* fundamentally !__pure2 */
float	ldexpf(float, int);
float	logf(float);
float	log10f(float);
float	modff(float, float *);	/* fundamentally !__pure2 */

float	powf(float, float);
float	sqrtf(float);

float	ceilf(float);
float	fabsf(float);
float	floorf(float);
float	fmodf(float, float);

float	erff(float);
float	erfcf(float) __pure2;
int	finitef(float) __pure2;
float	gammaf(float);
float	hypotf(float, float) __pure2;
float	j0f(float);
float	j1f(float);
float	jnf(int, float);
float	lgammaf(float);
float	y0f(float);
float	y1f(float);
float	ynf(int, float);

float	acoshf(float);
float	asinhf(float);
float	atanhf(float);
float	cbrtf(float) __pure2;
float	logbf(float) __pure2;
float	nextafterf(float, float);
float	remainderf(float, float);
float	scalbf(float, float);

/*
 * float version of IEEE Test Vector
 */
float	significandf(float);

/*
 * Float versions of functions callable from C, intended to support
 * IEEE arithmetic.
 */
float	copysignf(float, float) __pure2;
int	ilogbf(float);
float	rintf(float);
float	scalbnf(float, int);

/*
 * float versions of BSD math library entry points
 */
float	dremf(float, float);
float	expm1f(float) __pure2;
float	log1pf(float) __pure2;

/*
 * Float versions of reentrant version of gamma & lgamma; passes
 * signgam back by reference as the second argument; user must
 * allocate space for signgam.
 */
#ifdef _REENTRANT
float	gammaf_r(float, int *);
float	lgammaf_r(float, int *);
#endif	/* _REENTRANT */

#endif /* !_XOPEN_SOURCE */
#endif /* !_ANSI_SOURCE && !_POSIX_SOURCE */
__END_DECLS

#endif /* !_MATH_H_ */
