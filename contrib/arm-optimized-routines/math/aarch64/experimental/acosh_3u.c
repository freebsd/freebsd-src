/*
 * Double-precision acosh(x) function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "mathlib.h"
#include "math_config.h"
#include "test_sig.h"
#include "test_defs.h"

#define Ln2 (0x1.62e42fefa39efp-1)
#define MinusZero (0x8000000000000000)
#define SquareLim (0x5fe0000000000000) /* asuint64(0x1.0p511).  */
#define Two (0x4000000000000000)       /* asuint64(2.0).  */

/* acosh approximation using a variety of approaches on different intervals:

   acosh(x) = ln(x + sqrt(x * x - 1)).

   x >= 2^511: We cannot square x without overflow. For huge x, sqrt(x*x - 1)
   is close enough to x that we can calculate the result by ln(2x) == ln(x) +
   ln(2). The greatest observed error in this region is 0.98 ULP:
   acosh(0x1.1b9bf42923d1dp+853) got 0x1.28066a11a7c7fp+9
				want 0x1.28066a11a7c8p+9.

   x > 2: Calculate the result directly using definition of acosh(x). Greatest
   observed error in this region is 1.33 ULP:
   acosh(0x1.1e45d14bfcfa2p+1) got 0x1.71a06f50c34b5p+0
			      want 0x1.71a06f50c34b6p+0.

   0 <= x <= 2: Calculate the result using log1p. For x < 1, acosh(x) is
   undefined. For 1 <= x <= 2, the largest observed error is 2.69 ULP:
   acosh(0x1.073528248093p+0) got 0x1.e4d9bd20684f3p-3
			     want 0x1.e4d9bd20684f6p-3.  */
double
acosh (double x)
{
  uint64_t ix = asuint64 (x);

  if (unlikely (ix >= MinusZero))
    return __math_invalid (x);

  if (unlikely (ix >= SquareLim))
    return log (x) + Ln2;

  if (ix >= Two)
    return log (x + sqrt (x * x - 1));

  double xm1 = x - 1;
  return log1p (xm1 + sqrt (2 * xm1 + xm1 * xm1));
}

TEST_SIG (S, D, 1, acosh, 1.0, 10.0)
TEST_ULP (acosh, 2.19)
TEST_INTERVAL (acosh, 0, 1, 10000)
TEST_INTERVAL (acosh, 1, 2, 100000)
TEST_INTERVAL (acosh, 2, 0x1p511, 100000)
TEST_INTERVAL (acosh, 0x1p511, inf, 100000)
TEST_INTERVAL (acosh, -0, -inf, 10000)
