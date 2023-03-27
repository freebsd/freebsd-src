/*
 * Single-precision vector cbrt(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "mathlib.h"
#include "pl_sig.h"
#include "pl_test.h"

#if V_SUPPORTED

#define AbsMask 0x7fffffff
#define SignMask v_u32 (0x80000000)
#define TwoThirds v_f32 (0x1.555556p-1f)
#define SmallestNormal 0x00800000
#define MantissaMask 0x007fffff
#define HalfExp 0x3f000000

#define C(i) v_f32 (__cbrtf_data.poly[i])
#define T(i) v_lookup_f32 (__cbrtf_data.table, i)

static NOINLINE v_f32_t
specialcase (v_f32_t x, v_f32_t y, v_u32_t special)
{
  return v_call_f32 (cbrtf, x, y, special);
}

/* Approximation for vector single-precision cbrt(x) using Newton iteration with
   initial guess obtained by a low-order polynomial. Greatest error is 1.5 ULP.
   This is observed for every value where the mantissa is 0x1.81410e and the
   exponent is a multiple of 3, for example:
   __v_cbrtf(0x1.81410ep+30) got 0x1.255d96p+10
			    want 0x1.255d92p+10.  */
VPCS_ATTR v_f32_t V_NAME (cbrtf) (v_f32_t x)
{
  v_u32_t ix = v_as_u32_f32 (x);
  v_u32_t iax = ix & AbsMask;

  /* Subnormal, +/-0 and special values.  */
  v_u32_t special = v_cond_u32 ((iax < SmallestNormal) | (iax >= 0x7f800000));

  /* Decompose |x| into m * 2^e, where m is in [0.5, 1.0]. This is a vector
     version of frexpf, which gets subnormal values wrong - these have to be
     special-cased as a result.  */
  v_f32_t m = v_as_f32_u32 ((iax & MantissaMask) | HalfExp);
  v_s32_t e = v_as_s32_u32 (iax >> 23) - 126;

  /* p is a rough approximation for cbrt(m) in [0.5, 1.0]. The better this is,
     the less accurate the next stage of the algorithm needs to be. An order-4
     polynomial is enough for one Newton iteration.  */
  v_f32_t p_01 = v_fma_f32 (C (1), m, C (0));
  v_f32_t p_23 = v_fma_f32 (C (3), m, C (2));
  v_f32_t p = v_fma_f32 (m * m, p_23, p_01);

  /* One iteration of Newton's method for iteratively approximating cbrt.  */
  v_f32_t m_by_3 = m / 3;
  v_f32_t a = v_fma_f32 (TwoThirds, p, m_by_3 / (p * p));

  /* Assemble the result by the following:

     cbrt(x) = cbrt(m) * 2 ^ (e / 3).

     We can get 2 ^ round(e / 3) using ldexp and integer divide, but since e is
     not necessarily a multiple of 3 we lose some information.

     Let q = 2 ^ round(e / 3), then t = 2 ^ (e / 3) / q.

     Then we know t = 2 ^ (i / 3), where i is the remainder from e / 3, which is
     an integer in [-2, 2], and can be looked up in the table T. Hence the
     result is assembled as:

     cbrt(x) = cbrt(m) * t * 2 ^ round(e / 3) * sign.  */

  v_s32_t ey = e / 3;
  v_f32_t my = a * T (v_as_u32_s32 (e % 3 + 2));

  /* Vector version of ldexpf.  */
  v_f32_t y = v_as_f32_u32 ((v_as_u32_s32 (ey + 127) << 23)) * my;
  /* Copy sign.  */
  y = v_as_f32_u32 (v_bsl_u32 (SignMask, ix, v_as_u32_f32 (y)));

  if (unlikely (v_any_u32 (special)))
    return specialcase (x, y, special);
  return y;
}
VPCS_ALIAS

PL_SIG (V, F, 1, cbrt, -10.0, 10.0)
PL_TEST_ULP (V_NAME (cbrtf), 1.03)
PL_TEST_EXPECT_FENV_ALWAYS (V_NAME (cbrtf))
PL_TEST_INTERVAL (V_NAME (cbrtf), 0, inf, 1000000)
PL_TEST_INTERVAL (V_NAME (cbrtf), -0, -inf, 1000000)
#endif
