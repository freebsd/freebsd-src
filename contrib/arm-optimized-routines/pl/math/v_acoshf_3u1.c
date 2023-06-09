/*
 * Single-precision vector acosh(x) function.
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#define SignMask 0x80000000
#define One 0x3f800000
#define SquareLim 0x5f800000 /* asuint(0x1p64).  */

#if V_SUPPORTED

#include "v_log1pf_inline.h"

static NOINLINE VPCS_ATTR v_f32_t
special_case (v_f32_t x, v_f32_t y, v_u32_t special)
{
  return v_call_f32 (acoshf, x, y, special);
}

/* Vector approximation for single-precision acosh, based on log1p. Maximum
   error depends on WANT_SIMD_EXCEPT. With SIMD fp exceptions enabled, it
   is 2.78 ULP:
   __v_acoshf(0x1.07887p+0) got 0x1.ef9e9cp-3
			   want 0x1.ef9ea2p-3.
   With exceptions disabled, we can compute u with a shorter dependency chain,
   which gives maximum error of 3.07 ULP:
  __v_acoshf(0x1.01f83ep+0) got 0x1.fbc7fap-4
			   want 0x1.fbc7f4p-4.  */

VPCS_ATTR v_f32_t V_NAME (acoshf) (v_f32_t x)
{
  v_u32_t ix = v_as_u32_f32 (x);
  v_u32_t special = v_cond_u32 ((ix - One) >= (SquareLim - One));

#if WANT_SIMD_EXCEPT
  /* Mask special lanes with 1 to side-step spurious invalid or overflow. Use
     only xm1 to calculate u, as operating on x will trigger invalid for NaN. */
  v_f32_t xm1 = v_sel_f32 (special, v_f32 (1), x - 1);
  v_f32_t u = v_fma_f32 (xm1, xm1, 2 * xm1);
#else
  v_f32_t xm1 = x - 1;
  v_f32_t u = xm1 * (x + 1.0f);
#endif
  v_f32_t y = log1pf_inline (xm1 + v_sqrt_f32 (u));

  if (unlikely (v_any_u32 (special)))
    return special_case (x, y, special);
  return y;
}
VPCS_ALIAS

PL_SIG (V, F, 1, acosh, 1.0, 10.0)
#if WANT_SIMD_EXCEPT
PL_TEST_ULP (V_NAME (acoshf), 2.29)
#else
PL_TEST_ULP (V_NAME (acoshf), 2.58)
#endif
PL_TEST_EXPECT_FENV (V_NAME (acoshf), WANT_SIMD_EXCEPT)
PL_TEST_INTERVAL (V_NAME (acoshf), 0, 1, 500)
PL_TEST_INTERVAL (V_NAME (acoshf), 1, SquareLim, 100000)
PL_TEST_INTERVAL (V_NAME (acoshf), SquareLim, inf, 1000)
PL_TEST_INTERVAL (V_NAME (acoshf), -0, -inf, 1000)
#endif
