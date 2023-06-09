/*
 * Double-precision vector atan(x) function.
 *
 * Copyright (c) 2021-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#if V_SUPPORTED

#include "atan_common.h"

#define PiOver2 v_f64 (0x1.921fb54442d18p+0)
#define AbsMask v_u64 (0x7fffffffffffffff)
#define TinyBound 0x3e1 /* top12(asuint64(0x1p-30)).  */
#define BigBound 0x434	/* top12(asuint64(0x1p53)).  */

/* Fast implementation of vector atan.
   Based on atan(x) ~ shift + z + z^3 * P(z^2) with reduction to [0,1] using
   z=1/x and shift = pi/2. Maximum observed error is 2.27 ulps:
   __v_atan(0x1.0005af27c23e9p+0) got 0x1.9225645bdd7c1p-1
				 want 0x1.9225645bdd7c3p-1.  */
VPCS_ATTR
v_f64_t V_NAME (atan) (v_f64_t x)
{
  /* Small cases, infs and nans are supported by our approximation technique,
     but do not set fenv flags correctly. Only trigger special case if we need
     fenv.  */
  v_u64_t ix = v_as_u64_f64 (x);
  v_u64_t sign = ix & ~AbsMask;

#if WANT_SIMD_EXCEPT
  v_u64_t ia12 = (ix >> 52) & 0x7ff;
  v_u64_t special = v_cond_u64 (ia12 - TinyBound > BigBound - TinyBound);
  /* If any lane is special, fall back to the scalar routine for all lanes.  */
  if (unlikely (v_any_u64 (special)))
    return v_call_f64 (atan, x, v_f64 (0), v_u64 (-1));
#endif

  /* Argument reduction:
     y := arctan(x) for x < 1
     y := pi/2 + arctan(-1/x) for x > 1
     Hence, use z=-1/a if x>=1, otherwise z=a.  */
  v_u64_t red = v_cagt_f64 (x, v_f64 (1.0));
  /* Avoid dependency in abs(x) in division (and comparison).  */
  v_f64_t z = v_sel_f64 (red, v_div_f64 (v_f64 (-1.0), x), x);
  v_f64_t shift = v_sel_f64 (red, PiOver2, v_f64 (0.0));
  /* Use absolute value only when needed (odd powers of z).  */
  v_f64_t az = v_abs_f64 (z);
  az = v_sel_f64 (red, -az, az);

  /* Calculate the polynomial approximation.  */
  v_f64_t y = eval_poly (z, az, shift);

  /* y = atan(x) if x>0, -atan(-x) otherwise.  */
  y = v_as_f64_u64 (v_as_u64_f64 (y) ^ sign);
  return y;
}
VPCS_ALIAS

PL_SIG (V, D, 1, atan, -10.0, 10.0)
PL_TEST_ULP (V_NAME (atan), 1.78)
PL_TEST_EXPECT_FENV (V_NAME (atan), WANT_SIMD_EXCEPT)
PL_TEST_INTERVAL (V_NAME (atan), 0, 0x1p-30, 10000)
PL_TEST_INTERVAL (V_NAME (atan), -0, -0x1p-30, 1000)
PL_TEST_INTERVAL (V_NAME (atan), 0x1p-30, 0x1p53, 900000)
PL_TEST_INTERVAL (V_NAME (atan), -0x1p-30, -0x1p53, 90000)
PL_TEST_INTERVAL (V_NAME (atan), 0x1p53, inf, 10000)
PL_TEST_INTERVAL (V_NAME (atan), -0x1p53, -inf, 1000)

#endif
