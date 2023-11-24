/*
 * Single-precision vector tanh(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#if V_SUPPORTED

#include "v_expm1f_inline.h"

#define BoringBound                                                            \
  0x41102cb3 /* 0x1.205966p+3, above which tanhf rounds to 1 (or -1 for        \
		negative).  */
#define AbsMask 0x7fffffff

static NOINLINE v_f32_t
special_case (v_f32_t x, v_f32_t y, v_u32_t special)
{
  return v_call_f32 (tanhf, x, y, special);
}

/* Approximation for single-precision vector tanh(x), using a simplified version
   of expm1f. The maximum error is 2.58 ULP:
   __v_tanhf(0x1.fa5eep-5) got 0x1.f9ba02p-5
			  want 0x1.f9ba08p-5.  */
VPCS_ATTR v_f32_t V_NAME (tanhf) (v_f32_t x)
{
  v_u32_t ix = v_as_u32_f32 (x);
  v_u32_t iax = ix & AbsMask;
  v_u32_t sign = ix & ~AbsMask;
  v_u32_t is_boring = v_cond_u32 (iax > BoringBound);
  v_f32_t boring = v_as_f32_u32 (sign | One);

#if WANT_SIMD_EXCEPT
  /* If fp exceptions are to be triggered properly, set all special and boring
     lanes to 1, which will trigger no exceptions, and fix them up later.  */
  v_u32_t special = v_cond_u32 ((iax > 0x7f800000) | (iax < 0x34000000));
  ix = v_sel_u32 (is_boring, v_u32 (One), ix);
  if (unlikely (v_any_u32 (special)))
    ix = v_sel_u32 (special, v_u32 (One), ix);
#else
  v_u32_t special = v_cond_u32 ((iax > 0x7f800000) | (iax == 0));
#endif

  /* tanh(x) = (e^2x - 1) / (e^2x + 1).  */
  v_f32_t q = expm1f_inline (2 * v_as_f32_u32 (ix));
  v_f32_t y = q / (q + 2);
  y = v_sel_f32 (is_boring, boring, y);
  if (unlikely (v_any_u32 (special)))
    return special_case (x, y, special);
  return y;
}
VPCS_ALIAS

PL_SIG (V, F, 1, tanh, -10.0, 10.0)
PL_TEST_ULP (V_NAME (tanhf), 2.09)
PL_TEST_EXPECT_FENV (V_NAME (tanhf), WANT_SIMD_EXCEPT)
PL_TEST_INTERVAL (V_NAME (tanhf), 0, 0x1p-23, 1000)
PL_TEST_INTERVAL (V_NAME (tanhf), -0, -0x1p-23, 1000)
PL_TEST_INTERVAL (V_NAME (tanhf), 0x1p-23, 0x1.205966p+3, 100000)
PL_TEST_INTERVAL (V_NAME (tanhf), -0x1p-23, -0x1.205966p+3, 100000)
PL_TEST_INTERVAL (V_NAME (tanhf), 0x1.205966p+3, inf, 100)
PL_TEST_INTERVAL (V_NAME (tanhf), -0x1.205966p+3, -inf, 100)
#endif
