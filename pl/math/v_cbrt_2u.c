/*
 * Double-precision vector cbrt(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "mathlib.h"
#include "pl_sig.h"
#include "pl_test.h"

#if V_SUPPORTED

#define AbsMask 0x7fffffffffffffff
#define TwoThirds v_f64 (0x1.5555555555555p-1)
#define TinyBound 0x001 /* top12 (smallest_normal).  */
#define BigBound 0x7ff	/* top12 (infinity).  */
#define MantissaMask v_u64 (0x000fffffffffffff)
#define HalfExp v_u64 (0x3fe0000000000000)

#define C(i) v_f64 (__cbrt_data.poly[i])
#define T(i) v_lookup_f64 (__cbrt_data.table, i)

static NOINLINE v_f64_t
specialcase (v_f64_t x, v_f64_t y, v_u64_t special)
{
  return v_call_f64 (cbrt, x, y, special);
}

/* Approximation for double-precision vector cbrt(x), using low-order polynomial
   and two Newton iterations. Greatest observed error is 1.79 ULP. Errors repeat
   according to the exponent, for instance an error observed for double value
   m * 2^e will be observed for any input m * 2^(e + 3*i), where i is an
   integer.
   __v_cbrt(0x1.fffff403f0bc6p+1) got 0x1.965fe72821e9bp+0
				 want 0x1.965fe72821e99p+0.  */
VPCS_ATTR v_f64_t V_NAME (cbrt) (v_f64_t x)
{
  v_u64_t ix = v_as_u64_f64 (x);
  v_u64_t iax = ix & AbsMask;
  v_u64_t ia12 = iax >> 52;

  /* Subnormal, +/-0 and special values.  */
  v_u64_t special = v_cond_u64 ((ia12 < TinyBound) | (ia12 >= BigBound));

  /* Decompose |x| into m * 2^e, where m is in [0.5, 1.0]. This is a vector
     version of frexp, which gets subnormal values wrong - these have to be
     special-cased as a result.  */
  v_f64_t m = v_as_f64_u64 (v_bsl_u64 (MantissaMask, iax, HalfExp));
  v_s64_t e = v_as_s64_u64 (iax >> 52) - 1022;

  /* Calculate rough approximation for cbrt(m) in [0.5, 1.0], starting point for
     Newton iterations.  */
  v_f64_t p_01 = v_fma_f64 (C (1), m, C (0));
  v_f64_t p_23 = v_fma_f64 (C (3), m, C (2));
  v_f64_t p = v_fma_f64 (m * m, p_23, p_01);

  /* Two iterations of Newton's method for iteratively approximating cbrt.  */
  v_f64_t m_by_3 = m / 3;
  v_f64_t a = v_fma_f64 (TwoThirds, p, m_by_3 / (p * p));
  a = v_fma_f64 (TwoThirds, a, m_by_3 / (a * a));

  /* Assemble the result by the following:

     cbrt(x) = cbrt(m) * 2 ^ (e / 3).

     We can get 2 ^ round(e / 3) using ldexp and integer divide, but since e is
     not necessarily a multiple of 3 we lose some information.

     Let q = 2 ^ round(e / 3), then t = 2 ^ (e / 3) / q.

     Then we know t = 2 ^ (i / 3), where i is the remainder from e / 3, which is
     an integer in [-2, 2], and can be looked up in the table T. Hence the
     result is assembled as:

     cbrt(x) = cbrt(m) * t * 2 ^ round(e / 3) * sign.  */

  v_s64_t ey = e / 3;
  v_f64_t my = a * T (v_as_u64_s64 (e % 3 + 2));

  /* Vector version of ldexp.  */
  v_f64_t y = v_as_f64_u64 ((v_as_u64_s64 (ey + 1023) << 52)) * my;
  /* Copy sign.  */
  y = v_as_f64_u64 (v_bsl_u64 (v_u64 (AbsMask), v_as_u64_f64 (y), ix));

  if (unlikely (v_any_u64 (special)))
    return specialcase (x, y, special);
  return y;
}
VPCS_ALIAS

PL_TEST_ULP (V_NAME (cbrt), 1.30)
PL_SIG (V, D, 1, cbrt, -10.0, 10.0)
PL_TEST_EXPECT_FENV_ALWAYS (V_NAME (cbrt))
PL_TEST_INTERVAL (V_NAME (cbrt), 0, inf, 1000000)
PL_TEST_INTERVAL (V_NAME (cbrt), -0, -inf, 1000000)
#endif
