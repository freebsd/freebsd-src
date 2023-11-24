/*
 * Single-precision vector atan(x) function.
 *
 * Copyright (c) 2021-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#if V_SUPPORTED

#include "atanf_common.h"

#define PiOver2 v_f32 (0x1.921fb6p+0f)
#define AbsMask v_u32 (0x7fffffff)
#define TinyBound 0x308 /* top12(asuint(0x1p-30)).  */
#define BigBound 0x4e8	/* top12(asuint(0x1p30)).  */

#if WANT_SIMD_EXCEPT
static NOINLINE v_f32_t
specialcase (v_f32_t x, v_f32_t y, v_u32_t special)
{
  return v_call_f32 (atanf, x, y, special);
}
#endif

/* Fast implementation of vector atanf based on
   atan(x) ~ shift + z + z^3 * P(z^2) with reduction to [0,1]
   using z=-1/x and shift = pi/2. Maximum observed error is 2.9ulps:
   v_atanf(0x1.0468f6p+0) got 0x1.967f06p-1 want 0x1.967fp-1.  */
VPCS_ATTR
v_f32_t V_NAME (atanf) (v_f32_t x)
{
  /* Small cases, infs and nans are supported by our approximation technique,
     but do not set fenv flags correctly. Only trigger special case if we need
     fenv.  */
  v_u32_t ix = v_as_u32_f32 (x);
  v_u32_t sign = ix & ~AbsMask;

#if WANT_SIMD_EXCEPT
  v_u32_t ia12 = (ix >> 20) & 0x7ff;
  v_u32_t special = v_cond_u32 (ia12 - TinyBound > BigBound - TinyBound);
  /* If any lane is special, fall back to the scalar routine for all lanes.  */
  if (unlikely (v_any_u32 (special)))
    return specialcase (x, x, v_u32 (-1));
#endif

  /* Argument reduction:
     y := arctan(x) for x < 1
     y := pi/2 + arctan(-1/x) for x > 1
     Hence, use z=-1/a if x>=1, otherwise z=a.  */
  v_u32_t red = v_cagt_f32 (x, v_f32 (1.0));
  /* Avoid dependency in abs(x) in division (and comparison).  */
  v_f32_t z = v_sel_f32 (red, v_div_f32 (v_f32 (-1.0f), x), x);
  v_f32_t shift = v_sel_f32 (red, PiOver2, v_f32 (0.0f));
  /* Use absolute value only when needed (odd powers of z).  */
  v_f32_t az = v_abs_f32 (z);
  az = v_sel_f32 (red, -az, az);

  /* Calculate the polynomial approximation.  */
  v_f32_t y = eval_poly (z, az, shift);

  /* y = atan(x) if x>0, -atan(-x) otherwise.  */
  y = v_as_f32_u32 (v_as_u32_f32 (y) ^ sign);

  return y;
}
VPCS_ALIAS

PL_SIG (V, F, 1, atan, -10.0, 10.0)
PL_TEST_ULP (V_NAME (atanf), 2.5)
PL_TEST_EXPECT_FENV (V_NAME (atanf), WANT_SIMD_EXCEPT)
PL_TEST_INTERVAL (V_NAME (atanf), 0, 0x1p-30, 5000)
PL_TEST_INTERVAL (V_NAME (atanf), -0, -0x1p-30, 5000)
PL_TEST_INTERVAL (V_NAME (atanf), 0x1p-30, 1, 40000)
PL_TEST_INTERVAL (V_NAME (atanf), -0x1p-30, -1, 40000)
PL_TEST_INTERVAL (V_NAME (atanf), 1, 0x1p30, 40000)
PL_TEST_INTERVAL (V_NAME (atanf), -1, -0x1p30, 40000)
PL_TEST_INTERVAL (V_NAME (atanf), 0x1p30, inf, 1000)
PL_TEST_INTERVAL (V_NAME (atanf), -0x1p30, -inf, 1000)
#endif
