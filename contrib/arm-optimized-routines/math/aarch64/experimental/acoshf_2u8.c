/*
 * Single-precision acosh(x) function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"
#include "test_sig.h"
#include "test_defs.h"

#define Ln2 (0x1.62e4p-1f)
#define MinusZero 0x80000000
#define SquareLim 0x5f800000 /* asuint(0x1p64).  */
#define Two 0x40000000

/* acoshf approximation using a variety of approaches on different intervals:

   x >= 2^64: We cannot square x without overflow. For huge x, sqrt(x*x - 1) is
   close enough to x that we can calculate the result by ln(2x) == ln(x) +
   ln(2). The greatest error in the region is 0.94 ULP:
   acoshf(0x1.15f706p+92) got 0x1.022e14p+6 want 0x1.022e16p+6.

   x > 2: Calculate the result directly using definition of asinh(x) = ln(x +
   sqrt(x*x - 1)). Greatest error in this region is 1.30 ULP:
   acoshf(0x1.249d8p+1) got 0x1.77e1aep+0 want 0x1.77e1bp+0.

   0 <= x <= 2: Calculate the result using log1p. For x < 1, acosh(x) is
   undefined. For 1 <= x <= 2, the greatest error is 2.78 ULP:
   acoshf(0x1.07887p+0) got 0x1.ef9e9cp-3 want 0x1.ef9ea2p-3.  */
float
acoshf (float x)
{
  uint32_t ix = asuint (x);

  if (unlikely (ix >= MinusZero))
    return __math_invalidf (x);

  if (unlikely (ix >= SquareLim))
    return logf (x) + Ln2;

  if (ix > Two)
    return logf (x + sqrtf (x * x - 1));

  float xm1 = x - 1;
  return log1pf (xm1 + sqrtf (2 * xm1 + xm1 * xm1));
}

TEST_SIG (S, F, 1, acosh, 1.0, 10.0)
TEST_ULP (acoshf, 2.30)
TEST_INTERVAL (acoshf, 0, 1, 100)
TEST_INTERVAL (acoshf, 1, 2, 10000)
TEST_INTERVAL (acoshf, 2, 0x1p64, 100000)
TEST_INTERVAL (acoshf, 0x1p64, inf, 100000)
TEST_INTERVAL (acoshf, -0, -inf, 10000)
