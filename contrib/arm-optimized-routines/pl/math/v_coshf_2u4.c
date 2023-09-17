/*
 * Single-precision vector cosh(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "mathlib.h"
#include "pl_sig.h"
#include "pl_test.h"

#define AbsMask 0x7fffffff
#define TinyBound 0x20000000 /* 0x1p-63: Round to 1 below this.  */
#define SpecialBound                                                           \
  0x42ad496c /* 0x1.5a92d8p+6: expf overflows above this, so have to use       \
		special case.  */
#define Half v_f32 (0.5)

#if V_SUPPORTED

v_f32_t V_NAME (expf) (v_f32_t);

/* Single-precision vector cosh, using vector expf.
   Maximum error is 2.38 ULP:
   __v_coshf(0x1.e8001ep+1) got 0x1.6a491ep+4 want 0x1.6a4922p+4.  */
VPCS_ATTR v_f32_t V_NAME (coshf) (v_f32_t x)
{
  v_u32_t ix = v_as_u32_f32 (x);
  v_u32_t iax = ix & AbsMask;
  v_f32_t ax = v_as_f32_u32 (iax);
  v_u32_t special = v_cond_u32 (iax >= SpecialBound);

#if WANT_SIMD_EXCEPT
  /* If fp exceptions are to be triggered correctly, fall back to the scalar
     variant for all inputs if any input is a special value or above the bound
     at which expf overflows. */
  if (unlikely (v_any_u32 (special)))
    return v_call_f32 (coshf, x, x, v_u32 (-1));

  v_u32_t tiny = v_cond_u32 (iax <= TinyBound);
  /* If any input is tiny, avoid underflow exception by fixing tiny lanes of
     input to 1, which will generate no exceptions, and then also fixing tiny
     lanes of output to 1 just before return.  */
  if (unlikely (v_any_u32 (tiny)))
    ax = v_sel_f32 (tiny, v_f32 (1), ax);
#endif

  /* Calculate cosh by exp(x) / 2 + exp(-x) / 2.  */
  v_f32_t t = V_NAME (expf) (ax);
  v_f32_t y = t * Half + Half / t;

#if WANT_SIMD_EXCEPT
  if (unlikely (v_any_u32 (tiny)))
    return v_sel_f32 (tiny, v_f32 (1), y);
#else
  if (unlikely (v_any_u32 (special)))
    return v_call_f32 (coshf, x, y, special);
#endif

  return y;
}
VPCS_ALIAS

PL_SIG (V, F, 1, cosh, -10.0, 10.0)
PL_TEST_ULP (V_NAME (coshf), 1.89)
PL_TEST_EXPECT_FENV (V_NAME (coshf), WANT_SIMD_EXCEPT)
PL_TEST_INTERVAL (V_NAME (coshf), 0, 0x1p-63, 100)
PL_TEST_INTERVAL (V_NAME (coshf), 0, 0x1.5a92d8p+6, 80000)
PL_TEST_INTERVAL (V_NAME (coshf), 0x1.5a92d8p+6, inf, 2000)
PL_TEST_INTERVAL (V_NAME (coshf), -0, -0x1p-63, 100)
PL_TEST_INTERVAL (V_NAME (coshf), -0, -0x1.5a92d8p+6, 80000)
PL_TEST_INTERVAL (V_NAME (coshf), -0x1.5a92d8p+6, -inf, 2000)
#endif
