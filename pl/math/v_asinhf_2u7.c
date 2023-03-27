/*
 * Single-precision vector asinh(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "include/mathlib.h"
#include "pl_sig.h"
#include "pl_test.h"

#if V_SUPPORTED

#define SignMask v_u32 (0x80000000)
#define One v_f32 (1.0f)
#define BigBound v_u32 (0x5f800000)  /* asuint(0x1p64).  */
#define TinyBound v_u32 (0x30800000) /* asuint(0x1p-30).  */

#include "v_log1pf_inline.h"

static NOINLINE v_f32_t
specialcase (v_f32_t x, v_f32_t y, v_u32_t special)
{
  return v_call_f32 (asinhf, x, y, special);
}

/* Single-precision implementation of vector asinh(x), using vector log1p.
   Worst-case error is 2.66 ULP, at roughly +/-0.25:
   __v_asinhf(0x1.01b04p-2) got 0x1.fe163ep-3 want 0x1.fe1638p-3.  */
VPCS_ATTR v_f32_t V_NAME (asinhf) (v_f32_t x)
{
  v_u32_t ix = v_as_u32_f32 (x);
  v_u32_t iax = ix & ~SignMask;
  v_u32_t sign = ix & SignMask;
  v_f32_t ax = v_as_f32_u32 (iax);
  v_u32_t special = v_cond_u32 (iax >= BigBound);

#if WANT_SIMD_EXCEPT
  /* Sidestep tiny and large values to avoid inadvertently triggering
     under/overflow.  */
  special |= v_cond_u32 (iax < TinyBound);
  if (unlikely (v_any_u32 (special)))
    ax = v_sel_f32 (special, One, ax);
#endif

  /* asinh(x) = log(x + sqrt(x * x + 1)).
     For positive x, asinh(x) = log1p(x + x * x / (1 + sqrt(x * x + 1))).  */
  v_f32_t d = One + v_sqrt_f32 (ax * ax + One);
  v_f32_t y = log1pf_inline (ax + ax * ax / d);
  y = v_as_f32_u32 (sign | v_as_u32_f32 (y));

  if (unlikely (v_any_u32 (special)))
    return specialcase (x, y, special);
  return y;
}
VPCS_ALIAS

PL_SIG (V, F, 1, asinh, -10.0, 10.0)
PL_TEST_ULP (V_NAME (asinhf), 2.17)
PL_TEST_EXPECT_FENV (V_NAME (asinhf), WANT_SIMD_EXCEPT)
PL_TEST_INTERVAL (V_NAME (asinhf), 0, 0x1p-12, 40000)
PL_TEST_INTERVAL (V_NAME (asinhf), 0x1p-12, 1.0, 40000)
PL_TEST_INTERVAL (V_NAME (asinhf), 1.0, 0x1p11, 40000)
PL_TEST_INTERVAL (V_NAME (asinhf), 0x1p11, inf, 40000)
PL_TEST_INTERVAL (V_NAME (asinhf), 0, -0x1p-12, 20000)
PL_TEST_INTERVAL (V_NAME (asinhf), -0x1p-12, -1.0, 20000)
PL_TEST_INTERVAL (V_NAME (asinhf), -1.0, -0x1p11, 20000)
PL_TEST_INTERVAL (V_NAME (asinhf), -0x1p11, -inf, 20000)
#endif
