/*
 * Single-precision vector log10 function.
 *
 * Copyright (c) 2020-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "mathlib.h"
#include "pl_sig.h"
#include "pl_test.h"

#if V_SUPPORTED

#define P(i) v_f32 (__v_log10f_poly[i])

#define Ln2 v_f32 (0x1.62e43p-1f) /* 0x3f317218.  */
#define InvLn10 v_f32 (0x1.bcb7b2p-2f)
#define Min v_u32 (0x00800000)
#define Max v_u32 (0x7f800000)
#define Mask v_u32 (0x007fffff)
#define Off v_u32 (0x3f2aaaab) /* 0.666667.  */

VPCS_ATTR
NOINLINE static v_f32_t
specialcase (v_f32_t x, v_f32_t y, v_u32_t cmp)
{
  /* Fall back to scalar code.  */
  return v_call_f32 (log10f, x, y, cmp);
}

/* Our fast implementation of v_log10f uses a similar approach as v_logf.
   With the same offset as v_logf (i.e., 2/3) it delivers about 3.3ulps with
   order 9. This is more efficient than using a low order polynomial computed in
   double precision.
   Maximum error: 3.305ulps (nearest rounding.)
   __v_log10f(0x1.555c16p+0) got 0x1.ffe2fap-4
			    want 0x1.ffe2f4p-4 -0.304916 ulp err 2.80492.  */
VPCS_ATTR
v_f32_t V_NAME (log10f) (v_f32_t x)
{
  v_f32_t n, o, p, q, r, r2, y;
  v_u32_t u, cmp;

  u = v_as_u32_f32 (x);
  cmp = v_cond_u32 (u - Min >= Max - Min);

  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3.  */
  u -= Off;
  n = v_to_f32_s32 (v_as_s32_u32 (u) >> 23); /* signextend.  */
  u &= Mask;
  u += Off;
  r = v_as_f32_u32 (u) - v_f32 (1.0f);

  /* y = log10(1+r) + n*log10(2).  */
  r2 = r * r;
  /* (n*ln2 + r)*InvLn10 + r2*(P0 + r*P1 + r2*(P2 + r*P3 + r2*(P4 + r*P5 +
     r2*(P6+r*P7))).  */
  o = v_fma_f32 (P (7), r, P (6));
  p = v_fma_f32 (P (5), r, P (4));
  q = v_fma_f32 (P (3), r, P (2));
  y = v_fma_f32 (P (1), r, P (0));
  p = v_fma_f32 (o, r2, p);
  q = v_fma_f32 (p, r2, q);
  y = v_fma_f32 (q, r2, y);
  /* Using p = Log10(2)*n + r*InvLn(10) is slightly faster
     but less accurate.  */
  p = v_fma_f32 (Ln2, n, r);
  y = v_fma_f32 (y, r2, p * InvLn10);

  if (unlikely (v_any_u32 (cmp)))
    return specialcase (x, y, cmp);
  return y;
}
VPCS_ALIAS

PL_SIG (V, F, 1, log10, 0.01, 11.1)
PL_TEST_ULP (V_NAME (log10f), 2.81)
PL_TEST_EXPECT_FENV_ALWAYS (V_NAME (log10f))
PL_TEST_INTERVAL (V_NAME (log10f), 0, 0xffff0000, 10000)
PL_TEST_INTERVAL (V_NAME (log10f), 0x1p-4, 0x1p4, 500000)
#endif
