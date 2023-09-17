/*
 * Single-precision vector log2 function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "pairwise_hornerf.h"
#include "pl_sig.h"
#include "pl_test.h"
#if V_SUPPORTED

#define C(i) v_f32 (__v_log2f_data.poly[i])

#define Ln2 v_f32 (0x1.62e43p-1f) /* 0x3f317218 */
#define Min v_u32 (0x00800000)
#define Max v_u32 (0x7f800000)
#define Mask v_u32 (0x007fffff)
#define Off v_u32 (0x3f2aaaab) /* 0.666667 */

VPCS_ATTR
NOINLINE static v_f32_t
specialcase (v_f32_t x, v_f32_t y, v_u32_t cmp)
{
  /* Fall back to scalar code.  */
  return v_call_f32 (log2f, x, y, cmp);
}

/* Fast implementation for single precision log2,
   relies on same argument reduction as Neon logf.
   Maximum error: 2.48 ULPs
   __v_log2f(0x1.558174p+0) got 0x1.a9be84p-2
			   want 0x1.a9be8p-2.  */
VPCS_ATTR
v_f32_t V_NAME (log2f) (v_f32_t x)
{
  v_u32_t u = v_as_u32_f32 (x);
  v_u32_t cmp = v_cond_u32 (u - Min >= Max - Min);

  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3.  */
  u -= Off;
  v_f32_t n = v_to_f32_s32 (v_as_s32_u32 (u) >> 23); /* signextend.  */
  u &= Mask;
  u += Off;
  v_f32_t r = v_as_f32_u32 (u) - v_f32 (1.0f);

  /* y = log2(1+r) + n.  */
  v_f32_t r2 = r * r;
  v_f32_t p = PAIRWISE_HORNER_8 (r, r2, C);
  v_f32_t y = v_fma_f32 (p, r, n);

  if (unlikely (v_any_u32 (cmp)))
    return specialcase (x, y, cmp);
  return y;
}
VPCS_ALIAS

PL_SIG (V, F, 1, log2, 0.01, 11.1)
PL_TEST_ULP (V_NAME (log2f), 1.99)
PL_TEST_EXPECT_FENV_ALWAYS (V_NAME (log2f))
PL_TEST_INTERVAL (V_NAME (log2f), -0.0, -0x1p126, 100)
PL_TEST_INTERVAL (V_NAME (log2f), 0x1p-149, 0x1p-126, 4000)
PL_TEST_INTERVAL (V_NAME (log2f), 0x1p-126, 0x1p-23, 50000)
PL_TEST_INTERVAL (V_NAME (log2f), 0x1p-23, 1.0, 50000)
PL_TEST_INTERVAL (V_NAME (log2f), 1.0, 100, 50000)
PL_TEST_INTERVAL (V_NAME (log2f), 100, inf, 50000)
#endif
