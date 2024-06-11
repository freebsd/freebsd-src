/*
 * Single-precision cbrt(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "poly_scalar_f32.h"
#include "math_config.h"
#include "pl_sig.h"
#include "pl_test.h"

#define AbsMask 0x7fffffff
#define SignMask 0x80000000
#define TwoThirds 0x1.555556p-1f

#define T(i) __cbrtf_data.table[i]

/* Approximation for single-precision cbrt(x), using low-order polynomial and
   one Newton iteration on a reduced interval. Greatest error is 1.5 ULP. This
   is observed for every value where the mantissa is 0x1.81410e and the exponent
   is a multiple of 3, for example:
   cbrtf(0x1.81410ep+30) got 0x1.255d96p+10
			want 0x1.255d92p+10.  */
float
cbrtf (float x)
{
  uint32_t ix = asuint (x);
  uint32_t iax = ix & AbsMask;
  uint32_t sign = ix & SignMask;

  if (unlikely (iax == 0 || iax == 0x7f800000))
    return x;

  /* |x| = m * 2^e, where m is in [0.5, 1.0].
     We can easily decompose x into m and e using frexpf.  */
  int e;
  float m = frexpf (asfloat (iax), &e);

  /* p is a rough approximation for cbrt(m) in [0.5, 1.0]. The better this is,
     the less accurate the next stage of the algorithm needs to be. An order-4
     polynomial is enough for one Newton iteration.  */
  float p = pairwise_poly_3_f32 (m, m * m, __cbrtf_data.poly);

  /* One iteration of Newton's method for iteratively approximating cbrt.  */
  float m_by_3 = m / 3;
  float a = fmaf (TwoThirds, p, m_by_3 / (p * p));

  /* Assemble the result by the following:

     cbrt(x) = cbrt(m) * 2 ^ (e / 3).

     Let t = (2 ^ (e / 3)) / (2 ^ round(e / 3)).

     Then we know t = 2 ^ (i / 3), where i is the remainder from e / 3.
     i is an integer in [-2, 2], so t can be looked up in the table T.
     Hence the result is assembled as:

     cbrt(x) = cbrt(m) * t * 2 ^ round(e / 3) * sign.
     Which can be done easily using ldexpf.  */
  return asfloat (asuint (ldexpf (a * T (2 + e % 3), e / 3)) | sign);
}

PL_SIG (S, F, 1, cbrt, -10.0, 10.0)
PL_TEST_ULP (cbrtf, 1.03)
PL_TEST_SYM_INTERVAL (cbrtf, 0, inf, 1000000)
