/*
 * Double-precision scalar sinpi function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#define _GNU_SOURCE
#include <math.h>
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

/* Approximation for scalar double-precision sinpi(x).
   Maximum error: 3.03 ULP:
   sinpi(0x1.a90da2818f8b5p+7) got 0x1.fe358f255a4b3p-1
			      want 0x1.fe358f255a4b6p-1.  */
double
sinpi (double x)
{
  if (isinf (x))
    return __math_invalid (x);

  double r = asdouble (asuint64 (x) & ~0x8000000000000000);
  uint64_t sign = asuint64 (x) & 0x8000000000000000;

  /* Edge cases for when sinpif should be exactly 0. (Integers)
     0x1p53 is the limit for single precision to store any decimal places.  */
  if (r >= 0x1p53)
    return 0;

  /* If x is an integer, return 0.  */
  uint64_t m = (uint64_t) r;
  if (r == m)
    return 0;

  /* For very small inputs, squaring r causes underflow.
     Values below this threshold can be approximated via sinpi(x) ≈ pi*x.  */
  if (r < 0x1p-63)
    return M_PI * x;

  /* Any non-integer values >= 0x1x51 will be int + 0.5.
     These values should return exactly 1 or -1.  */
  if (r >= 0x1p51)
    {
      uint64_t iy = ((m & 1) << 63) ^ asuint64 (1.0);
      return asdouble (sign ^ iy);
    }

  /* n = rint(|x|).  */
  double n = r + Shift;
  sign ^= (asuint64 (n) << 63);
  n = n - Shift;

  /* r = |x| - n (range reduction into -1/2 .. 1/2).  */
  r = r - n;

  /* y = sin(r).  */
  double r2 = r * r;
  double y = horner_9_f64 (r2, poly);
  y = y * r;

  /* Reintroduce C2_hi.  */
  y = fma (-4 * r2, r, y);

  /* Copy sign of x to sin(|x|).  */
  return asdouble (asuint64 (y) ^ sign);
}

PL_SIG (S, D, 1, sinpi, -0.9, 0.9)
PL_TEST_ULP (sinpi, 2.53)
PL_TEST_SYM_INTERVAL (sinpi, 0, 0x1p-63, 5000)
PL_TEST_SYM_INTERVAL (sinpi, 0x1p-63, 0.5, 10000)
PL_TEST_SYM_INTERVAL (sinpi, 0.5, 0x1p51, 10000)
PL_TEST_SYM_INTERVAL (sinpi, 0x1p51, inf, 10000)
