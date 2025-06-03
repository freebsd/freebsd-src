/*
 * Single-precision scalar atan2(x) function.
 *
 * Copyright (c) 2021-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include <stdbool.h>

#include "atanf_common.h"
#include "math_config.h"
#include "test_sig.h"
#include "test_defs.h"

#define Pi (0x1.921fb6p+1f)
#define PiOver2 (0x1.921fb6p+0f)
#define PiOver4 (0x1.921fb6p-1f)
#define SignMask (0x80000000)

/* We calculate atan2f by P(n/d), where n and d are similar to the input
   arguments, and P is a polynomial. The polynomial may underflow.
   POLY_UFLOW_BOUND is the lower bound of the difference in exponents of n and
   d for which P underflows, and is used to special-case such inputs.  */
#define POLY_UFLOW_BOUND 24

static inline int32_t
biased_exponent (float f)
{
  uint32_t fi = asuint (f);
  int32_t ex = (int32_t) ((fi & 0x7f800000) >> 23);
  if (unlikely (ex == 0))
    {
      /* Subnormal case - we still need to get the exponent right for subnormal
	 numbers as division may take us back inside the normal range.  */
      return ex - __builtin_clz (fi << 9);
    }
  return ex;
}

/* Fast implementation of scalar atan2f. Largest observed error is
   2.88ulps in [99.0, 101.0] x [99.0, 101.0]:
   atan2f(0x1.9332d8p+6, 0x1.8cb6c4p+6) got 0x1.964646p-1
				       want 0x1.964640p-1.  */
float
atan2f (float y, float x)
{
  uint32_t ix = asuint (x);
  uint32_t iy = asuint (y);

  uint32_t sign_x = ix & SignMask;
  uint32_t sign_y = iy & SignMask;

  uint32_t iax = ix & ~SignMask;
  uint32_t iay = iy & ~SignMask;

  /* x or y is NaN.  */
  if ((iax > 0x7f800000) || (iay > 0x7f800000))
    return x + y;

  /* m = 2 * sign(x) + sign(y).  */
  uint32_t m = ((iy >> 31) & 1) | ((ix >> 30) & 2);

  /* The following follows glibc ieee754 implementation, except
     that we do not use +-tiny shifts (non-nearest rounding mode).  */

  int32_t exp_diff = biased_exponent (x) - biased_exponent (y);

  /* Special case for (x, y) either on or very close to the x axis. Either y =
     0, or y is tiny and x is huge (difference in exponents >=
     POLY_UFLOW_BOUND). In the second case, we only want to use this special
     case when x is negative (i.e. quadrants 2 or 3).  */
  if (unlikely (iay == 0 || (exp_diff >= POLY_UFLOW_BOUND && m >= 2)))
    {
      switch (m)
	{
	case 0:
	case 1:
	  return y; /* atan(+-0,+anything)=+-0.  */
	case 2:
	  return Pi; /* atan(+0,-anything) = pi.  */
	case 3:
	  return -Pi; /* atan(-0,-anything) =-pi.  */
	}
    }
  /* Special case for (x, y) either on or very close to the y axis. Either x =
     0, or x is tiny and y is huge (difference in exponents >=
     POLY_UFLOW_BOUND).  */
  if (unlikely (iax == 0 || exp_diff <= -POLY_UFLOW_BOUND))
    return sign_y ? -PiOver2 : PiOver2;

  /* x is INF.  */
  if (iax == 0x7f800000)
    {
      if (iay == 0x7f800000)
	{
	  switch (m)
	    {
	    case 0:
	      return PiOver4; /* atan(+INF,+INF).  */
	    case 1:
	      return -PiOver4; /* atan(-INF,+INF).  */
	    case 2:
	      return 3.0f * PiOver4; /* atan(+INF,-INF).  */
	    case 3:
	      return -3.0f * PiOver4; /* atan(-INF,-INF).  */
	    }
	}
      else
	{
	  switch (m)
	    {
	    case 0:
	      return 0.0f; /* atan(+...,+INF).  */
	    case 1:
	      return -0.0f; /* atan(-...,+INF).  */
	    case 2:
	      return Pi; /* atan(+...,-INF).  */
	    case 3:
	      return -Pi; /* atan(-...,-INF).  */
	    }
	}
    }
  /* y is INF.  */
  if (iay == 0x7f800000)
    return sign_y ? -PiOver2 : PiOver2;

  uint32_t sign_xy = sign_x ^ sign_y;

  float ax = asfloat (iax);
  float ay = asfloat (iay);

  bool pred_aygtax = (ay > ax);

  /* Set up z for call to atanf.  */
  float n = pred_aygtax ? -ax : ay;
  float d = pred_aygtax ? ay : ax;
  float z = n / d;

  float ret;
  if (unlikely (m < 2 && exp_diff >= POLY_UFLOW_BOUND))
    {
      /* If (x, y) is very close to x axis and x is positive, the polynomial
	 will underflow and evaluate to z.  */
      ret = z;
    }
  else
    {
      /* Work out the correct shift.  */
      float shift = sign_x ? -2.0f : 0.0f;
      shift = pred_aygtax ? shift + 1.0f : shift;
      shift *= PiOver2;

      ret = eval_poly (z, z, shift);
    }

  /* Account for the sign of x and y.  */
  return asfloat (asuint (ret) ^ sign_xy);
}

/* Arity of 2 means no mathbench entry emitted. See test/mathbench_funcs.h.  */
TEST_SIG (S, F, 2, atan2)
TEST_ULP (atan2f, 2.4)
TEST_INTERVAL (atan2f, -10.0, 10.0, 50000)
TEST_INTERVAL (atan2f, -1.0, 1.0, 40000)
TEST_INTERVAL (atan2f, 0.0, 1.0, 40000)
TEST_INTERVAL (atan2f, 1.0, 100.0, 40000)
TEST_INTERVAL (atan2f, 1e6, 1e32, 40000)
