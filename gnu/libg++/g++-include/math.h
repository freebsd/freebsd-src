// This may look like C code, but it is really -*- C++ -*-
/* 
Copyright (C) 1988 Free Software Foundation
    written by Doug Lea (dl@rocky.oswego.edu)

This file is part of the GNU C++ Library.  This library is free
software; you can redistribute it and/or modify it under the terms of
the GNU Library General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your
option) any later version.  This library is distributed in the hope
that it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free Software
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#ifndef _math_h
#ifdef __GNUG__
#pragma interface
#endif
#define _math_h 1

#if defined(hp300) && defined(__HAVE_FPU__)
#define __HAVE_68881__ 1
#endif

#if defined(masscomp)
#define __HAVE_68881__ 1
#endif

#ifdef __HAVE_68881__		/* MC68881/2 Floating-Point Coprocessor */
extern "C" {			/* fill in what we've left out */
#include <math-68881.h>

double  acosh(double);
double  asinh(double);
double  cbrt(double);
double  copysign(double,double);
double  erf(double);
double  erfc(double);
double  finite(double);
double  gamma(double);
double  hypot(double,double);
double  infnan(int);
int     isinf(double);
int     isnan(double);
double  j0(double);
double  j1(double);
double  jn(int, double);
double  lgamma(double);
double  y0(double);
double  y1(double);
double  yn(int, double);

double aint(double);
double anint(double);
int irint(double);
int nint(double);
}
/* Please add inline asm code for other machines here! */
#else
extern "C" {

#include <_G_config.h>

double  acos(double);
double  acosh(double);
double  asin(double);
double  asinh(double);
double  atan(double);
double  atan2(double, double);
double  atanh(double);
double  cbrt(double);
double  ceil(double);
double  copysign(double,double);
double  cos(double);
double  cosh(double);
double  drem(double,double);
double  erf(double);
double  erfc(double);
double  exp(double);
double  expm1(double);
double  fabs(double);
int finite(double);
double  floor(double);
double	fmod(double, double);
double  frexp(double, int*);
double  gamma(double);
double  hypot(double,double);
double  infnan(int);
#if !defined(sequent) && !defined(DGUX) &&!defined(sony) && !defined(masscomp) && !defined(hpux)
/* see below */
int     isinf(double);
int     isnan(double);
#endif
double  j0(double);
double  j1(double);
double  jn(int, double);
double  ldexp(double, int);
double  lgamma(double);
double  log(double);
double  log10(double);
double  log1p(double);
double  logb(double);
double  modf(double, double*);
double  pow(double, double);
double  rint(double);
double  scalb _G_ARGS((double, int));
double  sin(double);
double  sinh(double);
double  sqrt(double);
double  tan(double);
double  tanh(double);
double  y0(double);
double  y1(double);
double  yn(int, double);

double aint(double);
double anint(double);
int irint(double);
int nint(double);
}

#endif

/* libg++ doesn't use this since it is not available on some systems */

/* the following ifdef is just for compiling OOPS */

#ifndef DONT_DECLARE_EXCEPTION
struct libm_exception
{
  int type;
  char* name;
  double arg1, arg2, retval;
};

#define DOMAIN      1
#define SING        2
#define OVERFLOW    3
#define UNDERFLOW   4
#define TLOSS       5
#define PLOSS       6

extern "C" int matherr(libm_exception*);

#endif

#include <float.h>

/* On some systems, HUGE ought to be MAXFLOAT or IEEE infinity */

#ifndef HUGE
#define HUGE    DBL_MAX
#endif
#ifndef HUGE_VAL
#define HUGE_VAL    DBL_MAX
#endif


/* sequents don't supply these. The following should suffice */
#if defined(sequent) || defined(DGUX) || defined(sony) || defined(masscomp) \
|| defined(hpux)
#include <float.h>
static inline int isnan(double x) { return x != x; }
static inline int isinf(double x) { return x > DBL_MAX || x < -DBL_MAX; }
#endif

/* These seem to be sun & sysV names of these constants */

#ifndef M_E
#define M_E         2.7182818284590452354
#endif
#ifndef M_LOG2E
#define M_LOG2E     1.4426950408889634074
#endif
#ifndef M_LOG10E
#define M_LOG10E    0.43429448190325182765
#endif
#ifndef M_LN2
#define M_LN2       0.69314718055994530942
#endif
#ifndef M_LN10
#define M_LN10      2.30258509299404568402
#endif
#ifndef M_PI
#define M_PI        3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2      1.57079632679489661923
#endif
#ifndef M_1_PI
#define M_1_PI      0.31830988618379067154
#endif
#ifndef M_PI_4
#define M_PI_4      0.78539816339744830962
#endif
#ifndef M_2_PI
#define M_2_PI      0.63661977236758134308
#endif
#ifndef M_2_SQRTPI
#define M_2_SQRTPI  1.12837916709551257390
#endif
#ifndef M_SQRT2
#define M_SQRT2     1.41421356237309504880
#endif
#ifndef M_SQRT1_2
#define M_SQRT1_2   0.70710678118654752440
#endif

#ifndef PI                      // as in stroustrup
#define PI  M_PI
#endif
#ifndef PI2
#define PI2  M_PI_2
#endif

#endif
