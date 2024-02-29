/*
 * Double-precision scalar cospi function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "mathlib.h"
#include "math_config.h"
#include "pl_sig.h"
#include "pl_test.h"
#include "poly_scalar_f64.h"

/* Taylor series coefficents for sin(pi * x).
   C2 coefficient (orginally ~=5.16771278) has been split into two parts:
   C2_hi = 4, C2_lo = C2 - C2_hi (~=1.16771278)
   This change in magnitude reduces floating point rounding errors.
   C2_hi is then reintroduced after the polynomial approxmation.  */
static const double poly[]
    = { 0x1.921fb54442d184p1,  -0x1.2aef39896f94bp0,   0x1.466bc6775ab16p1,
	-0x1.32d2cce62dc33p-1, 0x1.507834891188ep-4,   -0x1.e30750a28c88ep-8,
	0x1.e8f48308acda4p-12, -0x1.6fc0032b3c29fp-16, 0x1.af86ae521260bp-21,
	-0x1.012a9870eeb7dp-25 };

#define Shift 0x1.8p+52

/* Approximation for scalar double-precision cospi(x).
   Maximum error: 3.13 ULP:
   cospi(0x1.160b129300112p-21) got 0x1.fffffffffd16bp-1
			       want 0x1.fffffffffd16ep-1.  */
double
cospi (double x)
{
  if (isinf (x))
    return __math_invalid (x);

  double ax = asdouble (asuint64 (x) & ~0x8000000000000000);

  /* Edge cases for when cospif should be exactly 1. (Integers)
     0x1p53 is the limit for single precision to store any decimal places.  */
  if (ax >= 0x1p53)
    return 1;

  /* If x is an integer, return +- 1, based upon if x is odd.  */
  uint64_t m = (uint64_t) ax;
  if (m == ax)
    return (m & 1) ? -1 : 1;

  /* For very small inputs, squaring r causes underflow.
     Values below this threshold can be approximated via
     cospi(x) ~= 1.  */
  if (ax < 0x1p-63)
    return 1;

  /* Any non-integer values >= 0x1x51 will be int +0.5.
     These values should return exactly 0.  */
  if (ax >= 0x1p51)
    return 0;

  /* n = rint(|x|).  */
  double n = ax + Shift;
  uint64_t sign = asuint64 (n) << 63;
  n = n - Shift;

  /* We know that cospi(x) = sinpi(0.5 - x)
     range reduction and offset into sinpi range -1/2 .. 1/2
     r = 0.5 - |x - rint(x)|.  */
  double r = 0.5 - fabs (ax - n);

  /* y = sin(r).  */
  double r2 = r * r;
  double y = horner_9_f64 (r2, poly);
  y = y * r;

  /* Reintroduce C2_hi.  */
  y = fma (-4 * r2, r, y);

  /* As all values are reduced to -1/2 .. 1/2, the result of cos(x) always be
     positive, therefore, the sign must be introduced based upon if x rounds to
     odd or even.  */
  return asdouble (asuint64 (y) ^ sign);
}

PL_SIG (S, D, 1, cospi, -0.9, 0.9)
PL_TEST_ULP (cospi, 2.63)
PL_TEST_SYM_INTERVAL (cospi, 0, 0x1p-63, 5000)
PL_TEST_SYM_INTERVAL (cospi, 0x1p-63, 0.5, 10000)
PL_TEST_SYM_INTERVAL (cospi, 0.5, 0x1p51f, 10000)
PL_TEST_SYM_INTERVAL (cospi, 0x1p51f, inf, 10000)
