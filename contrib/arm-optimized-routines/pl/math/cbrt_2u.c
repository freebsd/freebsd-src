/*
 * Double-precision cbrt(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"
#include "pl_sig.h"
#include "pl_test.h"

PL_SIG (S, D, 1, cbrt, -10.0, 10.0)

#define AbsMask 0x7fffffffffffffff
#define TwoThirds 0x1.5555555555555p-1

#define C(i) __cbrt_data.poly[i]
#define T(i) __cbrt_data.table[i]

/* Approximation for double-precision cbrt(x), using low-order polynomial and
   two Newton iterations. Greatest observed error is 1.79 ULP. Errors repeat
   according to the exponent, for instance an error observed for double value
   m * 2^e will be observed for any input m * 2^(e + 3*i), where i is an
   integer.
   cbrt(0x1.fffff403f0bc6p+1) got 0x1.965fe72821e9bp+0
			     want 0x1.965fe72821e99p+0.  */
double
cbrt (double x)
{
  uint64_t ix = asuint64 (x);
  uint64_t iax = ix & AbsMask;
  uint64_t sign = ix & ~AbsMask;

  if (unlikely (iax == 0 || iax == 0x7ff0000000000000))
    return x;

  /* |x| = m * 2^e, where m is in [0.5, 1.0].
     We can easily decompose x into m and e using frexp.  */
  int e;
  double m = frexp (asdouble (iax), &e);

  /* Calculate rough approximation for cbrt(m) in [0.5, 1.0], starting point for
     Newton iterations.  */
  double p_01 = fma (C (1), m, C (0));
  double p_23 = fma (C (3), m, C (2));
  double p = fma (p_23, m * m, p_01);

  /* Two iterations of Newton's method for iteratively approximating cbrt.  */
  double m_by_3 = m / 3;
  double a = fma (TwoThirds, p, m_by_3 / (p * p));
  a = fma (TwoThirds, a, m_by_3 / (a * a));

  /* Assemble the result by the following:

     cbrt(x) = cbrt(m) * 2 ^ (e / 3).

     Let t = (2 ^ (e / 3)) / (2 ^ round(e / 3)).

     Then we know t = 2 ^ (i / 3), where i is the remainder from e / 3.
     i is an integer in [-2, 2], so t can be looked up in the table T.
     Hence the result is assembled as:

     cbrt(x) = cbrt(m) * t * 2 ^ round(e / 3) * sign.
     Which can be done easily using ldexp.  */
  return asdouble (asuint64 (ldexp (a * T (2 + e % 3), e / 3)) | sign);
}

PL_TEST_ULP (cbrt, 1.30)
PL_TEST_SYM_INTERVAL (cbrt, 0, inf, 1000000)
