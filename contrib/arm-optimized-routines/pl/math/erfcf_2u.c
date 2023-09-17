/*
 * Single-precision erfc(x) function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "erfcf.h"
#include "math_config.h"
#include "pl_sig.h"
#include "pl_test.h"

#define P(i) __erfcf_poly_data.poly[i]

/* Approximation of erfcf for |x| > 4.0.  */
static inline float
approx_erfcf_hi (float x, uint32_t sign, const double *coeff)
{
  if (sign)
    {
      return 2.0f;
    }

  /* Polynomial contribution.  */
  double z = (double) fabs (x);
  float p = (float) eval_poly (z, coeff);
  /* Gaussian contribution.  */
  float e_mx2 = (float) eval_exp_mx2 (z);

  return p * e_mx2;
}

/* Approximation of erfcf for |x| < 4.0.  */
static inline float
approx_erfcf_lo (float x, uint32_t sign, const double *coeff)
{
  /* Polynomial contribution.  */
  double z = (double) fabs (x);
  float p = (float) eval_poly (z, coeff);
  /* Gaussian contribution.  */
  float e_mx2 = (float) eval_exp_mx2 (z);

  if (sign)
    return fmaf (-p, e_mx2, 2.0f);
  else
    return p * e_mx2;
}

/* Top 12 bits of a float (sign and exponent bits).  */
static inline uint32_t
abstop12 (float x)
{
  return (asuint (x) >> 20) & 0x7ff;
}

/* Top 12 bits of a float.  */
static inline uint32_t
top12 (float x)
{
  return asuint (x) >> 20;
}

/* Fast erfcf approximation using polynomial approximation
   multiplied by gaussian.
   Most of the computation is carried out in double precision,
   and is very sensitive to accuracy of polynomial and exp
   evaluation.
   Worst-case error is 1.968ulps, obtained for x = 2.0412941.
   erfcf(0x1.05492p+1) got 0x1.fe10f6p-9 want 0x1.fe10f2p-9 ulp
   err 1.46788.  */
float
erfcf (float x)
{
  /* Get top words and sign.  */
  uint32_t ix = asuint (x); /* We need to compare at most 32 bits.  */
  uint32_t sign = ix >> 31;
  uint32_t ia12 = top12 (x) & 0x7ff;

  /* Handle special cases and small values with a single comparison:
       abstop12(x)-abstop12(small) >= abstop12(INFINITY)-abstop12(small)

     Special cases
       erfcf(nan)=nan, erfcf(+inf)=0 and erfcf(-inf)=2

     Errno
       EDOM does not have to be set in case of erfcf(nan).
       Only ERANGE may be set in case of underflow.

     Small values (|x|<small)
       |x|<0x1.0p-26 => accurate to 0.5 ULP (top12(0x1p-26) = 0x328).  */
  if (unlikely (abstop12 (x) - 0x328 >= (abstop12 (INFINITY) & 0x7f8) - 0x328))
    {
      if (abstop12 (x) >= 0x7f8)
	return (float) (sign << 1) + 1.0f / x; /* Special cases.  */
      else
	return 1.0f - x; /* Small case.  */
    }

  /* Normalized numbers divided in 4 intervals
     with bounds: 2.0, 4.0, 8.0 and 10.0. 10 was chosen as the upper bound for
     the interesting region as it is the smallest value, representable as a
     12-bit integer, for which returning 0 gives <1.5 ULP.  */
  if (ia12 < 0x400)
    {
      return approx_erfcf_lo (x, sign, P (0));
    }
  if (ia12 < 0x408)
    {
      return approx_erfcf_lo (x, sign, P (1));
    }
  if (ia12 < 0x410)
    {
      return approx_erfcf_hi (x, sign, P (2));
    }
  if (ia12 < 0x412)
    {
      return approx_erfcf_hi (x, sign, P (3));
    }
  if (sign)
    {
      return 2.0f;
    }
  return __math_uflowf (0);
}

PL_SIG (S, F, 1, erfc, -4.0, 10.0)
PL_TEST_ULP (erfcf, 1.5)
PL_TEST_INTERVAL (erfcf, 0, 0xffff0000, 10000)
PL_TEST_INTERVAL (erfcf, 0x1p-127, 0x1p-26, 40000)
PL_TEST_INTERVAL (erfcf, -0x1p-127, -0x1p-26, 40000)
PL_TEST_INTERVAL (erfcf, 0x1p-26, 0x1p5, 40000)
PL_TEST_INTERVAL (erfcf, -0x1p-26, -0x1p3, 40000)
PL_TEST_INTERVAL (erfcf, 0, inf, 40000)
