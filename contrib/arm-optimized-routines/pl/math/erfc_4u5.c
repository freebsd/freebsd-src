/*
 * Double-precision erfc(x) function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"
#include "pairwise_horner.h"
#include "pl_sig.h"
#include "pl_test.h"

#define AbsMask (0x7fffffffffffffff)

#define xint __erfc_data.interval_bounds
#define PX __erfc_data.poly

/* Accurate exponential from optimized routines.  */
double
__exp_dd (double x, double xtail);

static inline double
eval_poly_horner (double z, int i)
{
  double z2 = z * z;
#define C(j) PX[i][j]
  return PAIRWISE_HORNER_12 (z, z2, C);
}

/* Accurate evaluation of exp(x^2)
   using compensated product (x^2 ~ x*x + e2)
   and the __exp_dd(y,d) routine, that is the
   computation of exp(y+d) with a small correction d<<y.  */
static inline double
eval_accurate_gaussian (double a)
{
  double e2;
  double a2 = a * a;
  double aa1 = -fma (0x1.0000002p27, a, -a);
  aa1 = fma (0x1.0000002p27, a, aa1);
  double aa2 = a - aa1;
  e2 = fma (-aa1, aa1, a2);
  e2 = fma (-aa1, aa2, e2);
  e2 = fma (-aa2, aa1, e2);
  e2 = fma (-aa2, aa2, e2);
  return __exp_dd (-a2, e2);
}

/* Approximation of erfc for |x| > 6.0.  */
static inline double
approx_erfc_hi (double x, int i)
{
  double a = fabs (x);
  double z = a - xint[i];
  double p = eval_poly_horner (z, i);
  double e_mx2 = eval_accurate_gaussian (a);
  return p * e_mx2;
}

static inline int
get_itv_idx (double x)
{
  /* Interval bounds are a logarithmic scale, i.e. interval n has
     lower bound 2^(n/4) - 1. Use the exponent of (|x|+1)^4 to obtain
     the interval index.  */
  double a = asdouble (asuint64 (x) & AbsMask);
  double z = a + 1.0;
  z = z * z;
  z = z * z;
  return (asuint64 (z) >> 52) - 1023;
}

/* Approximation of erfc for |x| < 6.0.  */
static inline double
approx_erfc_lo (double x, uint32_t sign, int i)
{
  double a = fabs (x);
  double z = a - xint[i];
  double p = eval_poly_horner (z, i);
  double e_mx2 = eval_accurate_gaussian (a);
  if (sign)
    return fma (-p, e_mx2, 2.0);
  else
    return p * e_mx2;
}

/* Top 12 bits of a double (sign and exponent bits).  */
static inline uint32_t
abstop12 (double x)
{
  return (asuint64 (x) >> 52) & 0x7ff;
}

/* Top 32 bits of a double.  */
static inline uint32_t
top32 (double x)
{
  return asuint64 (x) >> 32;
}

/* Fast erfc implementation.
   The approximation uses polynomial approximation of
   exp(x^2) * erfc(x) with fixed orders on 20 intervals.
   Maximum measured error is 4.05 ULPs:.
   erfc(0x1.e8ebf6a2b0801p-2) got 0x1.ff84036f8f0b3p-2
			     want 0x1.ff84036f8f0b7p-2.  */
double
erfc (double x)
{
  /* Get top words.  */
  uint32_t ix = top32 (x); /* We need to compare at most 32 bits.  */
  uint32_t ia = ix & 0x7fffffff;
  uint32_t sign = ix >> 31;

  /* Handle special cases and small values with a single comparison:
     abstop12(x)-abstop12(small) >= abstop12(INFINITY)-abstop12(small)
     Special cases erfc(nan)=nan, erfc(+inf)=0 and erfc(-inf)=2
     Errno EDOM does not have to be set in case of erfc(nan).
     Only ERANGE may be set in case of underflow.
     Small values (|x|<small)
       |x|<0x1.0p-56 => accurate up to 0.5 ULP (top12(0x1p-50) = 0x3c7)
       |x|<0x1.0p-50 => accurate up to 1.0 ULP (top12(0x1p-50) = 0x3cd).  */
  if (unlikely (abstop12 (x) - 0x3cd >= (abstop12 (INFINITY) & 0x7ff) - 0x3cd))
    {
      if (abstop12 (x) >= 0x7ff)
	return (double) (sign << 1) + 1.0 / x; /* special cases.  */
      else
	return 1.0 - x; /* small case.  */
    }
  else if (ia < 0x40180000)
    { /* |x| < 6.0.  */
      return approx_erfc_lo (x, sign, get_itv_idx (x));
    }
  else if (sign)
    { /* x <= -6.0.  */
      return 2.0;
    }
  else if (ia < 0x403c0000)
    { /* 6.0 <= x < 28.  */
      return approx_erfc_hi (x, get_itv_idx (x));
    }
  else
    { /* x > 28.  */
      return __math_uflow (0);
    }
}

PL_SIG (S, D, 1, erfc, -6.0, 28.0)
PL_TEST_ULP (erfc, 3.56)
PL_TEST_INTERVAL (erfc, 0, 0xffff0000, 10000)
PL_TEST_INTERVAL (erfc, 0x1p-1022, 0x1p-26, 40000)
PL_TEST_INTERVAL (erfc, -0x1p-1022, -0x1p-26, 40000)
PL_TEST_INTERVAL (erfc, 0x1p-26, 0x1p5, 40000)
PL_TEST_INTERVAL (erfc, -0x1p-26, -0x1p3, 40000)
PL_TEST_INTERVAL (erfc, 0, inf, 40000)
