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

#ifndef _MATH_PRIVATE_H_
#define	_MATH_PRIVATE_H_

#include <sys/types.h>
#include <machine/endian.h>

/*
 * The original fdlibm code used statements like:
 *	n0 = ((*(int*)&one)>>29)^1;		* index of high word *
 *	ix0 = *(n0+(int*)&x);			* high word of x *
 *	ix1 = *((1-n0)+(int*)&x);		* low word of x *
 * to dig two 32 bit words out of the 64 bit IEEE floating point
 * value.  That is non-ANSI, and, moreover, the gcc instruction
 * scheduler gets it wrong.  We instead use the following macros.
 * Unlike the original code, we determine the endianness at compile
 * time, not at run time; I don't see much benefit to selecting
 * endianness at run time.
 */

/*
 * A union which permits us to convert between a double and two 32 bit
 * ints.
 */

#if BYTE_ORDER == BIG_ENDIAN

typedef union
{
  double value;
  struct
  {
    u_int32_t msw;
    u_int32_t lsw;
  } parts;
} ieee_double_shape_type;

#endif

#if BYTE_ORDER == LITTLE_ENDIAN

typedef union
{
  double value;
  struct
  {
    u_int32_t lsw;
    u_int32_t msw;
  } parts;
} ieee_double_shape_type;

#endif

/* Get two 32 bit ints from a double.  */

#define EXTRACT_WORDS(ix0,ix1,d)				\
do {								\
  ieee_double_shape_type ew_u;					\
  ew_u.value = (d);						\
  (ix0) = ew_u.parts.msw;					\
  (ix1) = ew_u.parts.lsw;					\
} while (0)

/* Get the more significant 32 bit int from a double.  */

#define GET_HIGH_WORD(i,d)					\
do {								\
  ieee_double_shape_type gh_u;					\
  gh_u.value = (d);						\
  (i) = gh_u.parts.msw;						\
} while (0)

/* Get the less significant 32 bit int from a double.  */

#define GET_LOW_WORD(i,d)					\
do {								\
  ieee_double_shape_type gl_u;					\
  gl_u.value = (d);						\
  (i) = gl_u.parts.lsw;						\
} while (0)

/* Set a double from two 32 bit ints.  */

#define INSERT_WORDS(d,ix0,ix1)					\
do {								\
  ieee_double_shape_type iw_u;					\
  iw_u.parts.msw = (ix0);					\
  iw_u.parts.lsw = (ix1);					\
  (d) = iw_u.value;						\
} while (0)

/* Set the more significant 32 bits of a double from an int.  */

#define SET_HIGH_WORD(d,v)					\
do {								\
  ieee_double_shape_type sh_u;					\
  sh_u.value = (d);						\
  sh_u.parts.msw = (v);						\
  (d) = sh_u.value;						\
} while (0)

/* Set the less significant 32 bits of a double from an int.  */

#define SET_LOW_WORD(d,v)					\
do {								\
  ieee_double_shape_type sl_u;					\
  sl_u.value = (d);						\
  sl_u.parts.lsw = (v);						\
  (d) = sl_u.value;						\
} while (0)

/*
 * A union which permits us to convert between a float and a 32 bit
 * int.
 */

typedef union
{
  float value;
  /* FIXME: Assumes 32 bit int.  */
  unsigned int word;
} ieee_float_shape_type;

/* Get a 32 bit int from a float.  */

#define GET_FLOAT_WORD(i,d)					\
do {								\
  ieee_float_shape_type gf_u;					\
  gf_u.value = (d);						\
  (i) = gf_u.word;						\
} while (0)

/* Set a float from a 32 bit int.  */

#define SET_FLOAT_WORD(d,i)					\
do {								\
  ieee_float_shape_type sf_u;					\
  sf_u.word = (i);						\
  (d) = sf_u.value;						\
} while (0)

/* ieee style elementary functions */
double	__ieee754_sqrt(double);
double	__ieee754_acos(double);
double	__ieee754_acosh(double);
double	__ieee754_log(double);
double	__ieee754_atanh(double);
double	__ieee754_asin(double);
double	__ieee754_atan2(double,double);
double	__ieee754_exp(double);
double	__ieee754_cosh(double);
double	__ieee754_fmod(double,double);
double	__ieee754_pow(double,double);
double	__ieee754_lgamma_r(double,int *);
double	__ieee754_gamma_r(double,int *);
double	__ieee754_lgamma(double);
double	__ieee754_gamma(double);
double	__ieee754_log10(double);
double	__ieee754_sinh(double);
double	__ieee754_hypot(double,double);
double	__ieee754_j0(double);
double	__ieee754_j1(double);
double	__ieee754_y0(double);
double	__ieee754_y1(double);
double	__ieee754_jn(int,double);
double	__ieee754_yn(int,double);
double	__ieee754_remainder(double,double);
int	__ieee754_rem_pio2(double,double*);
double	__ieee754_scalb(double,double);

/* fdlibm kernel function */
double	__kernel_standard(double,double,int);
double	__kernel_sin(double,double,int);
double	__kernel_cos(double,double);
double	__kernel_tan(double,double,int);
int	__kernel_rem_pio2(double*,double*,int,int,int,const int*);

/* ieee style elementary float functions */
float	__ieee754_sqrtf(float);
float	__ieee754_acosf(float);
float	__ieee754_acoshf(float);
float	__ieee754_logf(float);
float	__ieee754_atanhf(float);
float	__ieee754_asinf(float);
float	__ieee754_atan2f(float,float);
float	__ieee754_expf(float);
float	__ieee754_coshf(float);
float	__ieee754_fmodf(float,float);
float	__ieee754_powf(float,float);
float	__ieee754_lgammaf_r(float,int *);
float	__ieee754_gammaf_r(float,int *);
float	__ieee754_lgammaf(float);
float	__ieee754_gammaf(float);
float	__ieee754_log10f(float);
float	__ieee754_sinhf(float);
float	__ieee754_hypotf(float,float);
float	__ieee754_j0f(float);
float	__ieee754_j1f(float);
float	__ieee754_y0f(float);
float	__ieee754_y1f(float);
float	__ieee754_jnf(int,float);
float	__ieee754_ynf(int,float);
float	__ieee754_remainderf(float,float);
int	__ieee754_rem_pio2f(float,float*);
float	__ieee754_scalbf(float,float);

/* float versions of fdlibm kernel functions */
float	__kernel_sinf(float,float,int);
float	__kernel_cosf(float,float);
float	__kernel_tanf(float,float,int);
int	__kernel_rem_pio2f(float*,float*,int,int,int,const int*);

#endif /* !_MATH_PRIVATE_H_ */
