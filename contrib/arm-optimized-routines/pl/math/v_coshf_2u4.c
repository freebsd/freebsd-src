/*
 * Single-precision vector cosh(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_expf_inline.h"
#include "v_math.h"
#include "mathlib.h"
#include "pl_sig.h"
#include "pl_test.h"

static const struct data
{
  struct v_expf_data expf_consts;
  uint32x4_t tiny_bound, special_bound;
} data = {
  .expf_consts = V_EXPF_DATA,
  .tiny_bound = V4 (0x20000000), /* 0x1p-63: Round to 1 below this.  */
  /* 0x1.5a92d8p+6: expf overflows above this, so have to use special case.  */
  .special_bound = V4 (0x42ad496c),
};

#if !WANT_SIMD_EXCEPT
static float32x4_t NOINLINE VPCS_ATTR
special_case (float32x4_t x, float32x4_t y, uint32x4_t special)
{
  return v_call_f32 (coshf, x, y, special);
}
#endif

/* Single-precision vector cosh, using vector expf.
   Maximum error is 2.38 ULP:
   _ZGVnN4v_coshf (0x1.e8001ep+1) got 0x1.6a491ep+4
				 want 0x1.6a4922p+4.  */
float32x4_t VPCS_ATTR V_NAME_F1 (cosh) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  float32x4_t ax = vabsq_f32 (x);
  uint32x4_t iax = vreinterpretq_u32_f32 (ax);
  uint32x4_t special = vcgeq_u32 (iax, d->special_bound);

#if WANT_SIMD_EXCEPT
  /* If fp exceptions are to be triggered correctly, fall back to the scalar
     variant for all inputs if any input is a special value or above the bound
     at which expf overflows.  */
  if (unlikely (v_any_u32 (special)))
    return v_call_f32 (coshf, x, x, v_u32 (-1));

  uint32x4_t tiny = vcleq_u32 (iax, d->tiny_bound);
  /* If any input is tiny, avoid underflow exception by fixing tiny lanes of
     input to 0, which will generate no exceptions.  */
  if (unlikely (v_any_u32 (tiny)))
    ax = v_zerofy_f32 (ax, tiny);
#endif

  /* Calculate cosh by exp(x) / 2 + exp(-x) / 2.  */
  float32x4_t t = v_expf_inline (ax, &d->expf_consts);
  float32x4_t half_t = vmulq_n_f32 (t, 0.5);
  float32x4_t half_over_t = vdivq_f32 (v_f32 (0.5), t);

#if WANT_SIMD_EXCEPT
  if (unlikely (v_any_u32 (tiny)))
    return vbslq_f32 (tiny, v_f32 (1), vaddq_f32 (half_t, half_over_t));
#else
  if (unlikely (v_any_u32 (special)))
    return special_case (x, vaddq_f32 (half_t, half_over_t), special);
#endif

  return vaddq_f32 (half_t, half_over_t);
}

PL_SIG (V, F, 1, cosh, -10.0, 10.0)
PL_TEST_ULP (V_NAME_F1 (cosh), 1.89)
PL_TEST_EXPECT_FENV (V_NAME_F1 (cosh), WANT_SIMD_EXCEPT)
PL_TEST_SYM_INTERVAL (V_NAME_F1 (cosh), 0, 0x1p-63, 100)
PL_TEST_SYM_INTERVAL (V_NAME_F1 (cosh), 0, 0x1.5a92d8p+6, 80000)
PL_TEST_SYM_INTERVAL (V_NAME_F1 (cosh), 0x1.5a92d8p+6, inf, 2000)
