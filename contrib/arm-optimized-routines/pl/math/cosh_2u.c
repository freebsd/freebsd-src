/*
 * Double-precision cosh(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"
#include "pl_sig.h"
#include "pl_test.h"

#define AbsMask 0x7fffffffffffffff
#define SpecialBound                                                           \
  0x40861da04cbafe44 /* 0x1.61da04cbafe44p+9, above which exp overflows.  */

double
__exp_dd (double, double);

static double
specialcase (double x, uint64_t iax)
{
  if (iax == 0x7ff0000000000000)
    return INFINITY;
  if (iax > 0x7ff0000000000000)
    return __math_invalid (x);
  /* exp overflows above SpecialBound. At this magnitude cosh(x) is dominated by
     exp(x), so we can approximate cosh(x) by (exp(|x|/2)) ^ 2 / 2.  */
  double t = __exp_dd (asdouble (iax) / 2, 0);
  return (0.5 * t) * t;
}

/* Approximation for double-precision cosh(x).
   cosh(x) = (exp(x) + exp(-x)) / 2.
   The greatest observed error is in the special region, 1.93 ULP:
   cosh(0x1.628af341989dap+9) got 0x1.fdf28623ef921p+1021
			     want 0x1.fdf28623ef923p+1021.

   The greatest observed error in the non-special region is 1.03 ULP:
   cosh(0x1.502cd8e56ab3bp+0) got 0x1.fe54962842d0ep+0
			     want 0x1.fe54962842d0fp+0.  */
double
cosh (double x)
{
  uint64_t ix = asuint64 (x);
  uint64_t iax = ix & AbsMask;

  /* exp overflows a little bit before cosh, so use special-case handler for the
     gap, as well as special values.  */
  if (unlikely (iax >= SpecialBound))
    return specialcase (x, iax);

  double ax = asdouble (iax);
  /* Use double-precision exp helper to calculate exp(x), then:
     cosh(x) = exp(|x|) / 2 + 1 / (exp(|x| * 2).  */
  double t = __exp_dd (ax, 0);
  return 0.5 * t + 0.5 / t;
}

PL_SIG (S, D, 1, cosh, -10.0, 10.0)
PL_TEST_ULP (cosh, 1.43)
PL_TEST_INTERVAL (cosh, 0, 0x1.61da04cbafe44p+9, 100000)
PL_TEST_INTERVAL (cosh, -0, -0x1.61da04cbafe44p+9, 100000)
PL_TEST_INTERVAL (cosh, 0x1.61da04cbafe44p+9, 0x1p10, 1000)
PL_TEST_INTERVAL (cosh, -0x1.61da04cbafe44p+9, -0x1p10, 1000)
PL_TEST_INTERVAL (cosh, 0x1p10, inf, 100)
PL_TEST_INTERVAL (cosh, -0x1p10, -inf, 100)
