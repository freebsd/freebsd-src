/*
 * Single-precision vector tanh(x) function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_sig.h"
#include "test_defs.h"
#include "v_expm1f_inline.h"

static const struct data
{
  struct v_expm1f_data expm1f_consts;
  uint32x4_t boring_bound, large_bound, onef;
} data = {
  .expm1f_consts = V_EXPM1F_DATA,
  /* 0x1.205966p+3, above which tanhf rounds to 1 (or -1 for  negative).  */
  .boring_bound = V4 (0x41102cb3),
  .large_bound = V4 (0x7f800000),
};

static float32x4_t NOINLINE VPCS_ATTR
special_case (float32x4_t x, uint32x4_t is_boring, float32x4_t boring,
	      float32x4_t q, uint32x4_t special)
{
  return v_call_f32 (
      tanhf, x,
      vbslq_f32 (is_boring, boring, vdivq_f32 (q, vaddq_f32 (q, v_f32 (2.0)))),
      special);
}

/* Approximation for single-precision vector tanh(x), using a simplified
   version of expm1f. The maximum error is 2.58 ULP:
   _ZGVnN4v_tanhf (0x1.fa5eep-5) got 0x1.f9ba02p-5
				want 0x1.f9ba08p-5.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (tanh) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  uint32x4_t ix = vreinterpretq_u32_f32 (x);
  float32x4_t ax = vabsq_f32 (x);
  uint32x4_t iax = vreinterpretq_u32_f32 (ax);
  uint32x4_t sign = veorq_u32 (ix, iax);
  uint32x4_t is_boring = vcgtq_u32 (iax, d->boring_bound);
  /* expm1 exponent bias is 1.0f reinterpreted to int.  */
  float32x4_t boring = vreinterpretq_f32_u32 (vorrq_u32 (
      sign, vreinterpretq_u32_s32 (d->expm1f_consts.exponent_bias)));

#if WANT_SIMD_EXCEPT
  /* If fp exceptions are to be triggered properly, set all special and boring
     lanes to 0, which will trigger no exceptions, and fix them up later.  */
  uint32x4_t special = vorrq_u32 (vcgtq_u32 (iax, d->large_bound),
				  vcltq_u32 (iax, v_u32 (0x34000000)));
  x = v_zerofy_f32 (x, is_boring);
  if (unlikely (v_any_u32 (special)))
    x = v_zerofy_f32 (x, special);
#else
  uint32x4_t special = vcgtq_u32 (iax, d->large_bound);
#endif

  /* tanh(x) = (e^2x - 1) / (e^2x + 1).  */
  float32x4_t q = expm1f_inline (vmulq_n_f32 (x, 2), &d->expm1f_consts);

  if (unlikely (v_any_u32 (special)))
    return special_case (vreinterpretq_f32_u32 (ix), is_boring, boring, q,
			 special);

  float32x4_t y = vdivq_f32 (q, vaddq_f32 (q, v_f32 (2.0)));
  return vbslq_f32 (is_boring, boring, y);
}

HALF_WIDTH_ALIAS_F1 (tanh)

TEST_SIG (V, F, 1, tanh, -10.0, 10.0)
TEST_ULP (V_NAME_F1 (tanh), 2.09)
TEST_DISABLE_FENV_IF_NOT (V_NAME_F1 (tanh), WANT_SIMD_EXCEPT)
TEST_SYM_INTERVAL (V_NAME_F1 (tanh), 0, 0x1p-23, 1000)
TEST_SYM_INTERVAL (V_NAME_F1 (tanh), 0x1p-23, 0x1.205966p+3, 100000)
TEST_SYM_INTERVAL (V_NAME_F1 (tanh), 0x1.205966p+3, inf, 100)
