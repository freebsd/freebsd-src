/*
 * Single-precision vector asinh(x) function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_sig.h"
#include "test_defs.h"
#include "v_log1pf_inline.h"

const static struct data
{
  struct v_log1pf_data log1pf_consts;
  float32x4_t one;
  uint32x4_t big_bound;
#if WANT_SIMD_EXCEPT
  uint32x4_t tiny_bound;
#endif
} data = {
  .one = V4 (1),
  .log1pf_consts = V_LOG1PF_CONSTANTS_TABLE,
  .big_bound = V4 (0x5f800000), /* asuint(0x1p64).  */
#if WANT_SIMD_EXCEPT
  .tiny_bound = V4 (0x30800000) /* asuint(0x1p-30).  */
#endif
};

static float32x4_t NOINLINE VPCS_ATTR
special_case (float32x4_t x, uint32x4_t sign, float32x4_t y,
	      uint32x4_t special, const struct data *d)
{
  return v_call_f32 (
      asinhf, x,
      vreinterpretq_f32_u32 (veorq_u32 (
	  sign, vreinterpretq_u32_f32 (log1pf_inline (y, &d->log1pf_consts)))),
      special);
}

/* Single-precision implementation of vector asinh(x), using vector log1p.
   Worst-case error is 2.59 ULP:
   _ZGVnN4v_asinhf(0x1.d86124p-3) got 0x1.d449bep-3
				 want 0x1.d449c4p-3.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (asinh) (float32x4_t x)
{
  const struct data *dat = ptr_barrier (&data);
  float32x4_t ax = vabsq_f32 (x);
  uint32x4_t iax = vreinterpretq_u32_f32 (ax);
  uint32x4_t special = vcgeq_u32 (iax, dat->big_bound);
  uint32x4_t sign = veorq_u32 (vreinterpretq_u32_f32 (x), iax);
  float32x4_t special_arg = x;

#if WANT_SIMD_EXCEPT
  /* Sidestep tiny and large values to avoid inadvertently triggering
     under/overflow.  */
  special = vorrq_u32 (special, vcltq_u32 (iax, dat->tiny_bound));
  if (unlikely (v_any_u32 (special)))
    {
      ax = v_zerofy_f32 (ax, special);
      x = v_zerofy_f32 (x, special);
    }
#endif

  /* asinh(x) = log(x + sqrt(x * x + 1)).
     For positive x, asinh(x) = log1p(x + x * x / (1 + sqrt(x * x + 1))).  */
  float32x4_t d
      = vaddq_f32 (v_f32 (1), vsqrtq_f32 (vfmaq_f32 (dat->one, ax, ax)));
  float32x4_t y = vaddq_f32 (ax, vdivq_f32 (vmulq_f32 (ax, ax), d));

  if (unlikely (v_any_u32 (special)))
    return special_case (special_arg, sign, y, special, dat);
  return vreinterpretq_f32_u32 (veorq_u32 (
      sign, vreinterpretq_u32_f32 (log1pf_inline (y, &dat->log1pf_consts))));
}

HALF_WIDTH_ALIAS_F1 (asinh)

TEST_SIG (V, F, 1, asinh, -10.0, 10.0)
TEST_ULP (V_NAME_F1 (asinh), 2.10)
TEST_DISABLE_FENV_IF_NOT (V_NAME_F1 (asinh), WANT_SIMD_EXCEPT)
TEST_INTERVAL (V_NAME_F1 (asinh), 0, 0x1p-12, 40000)
TEST_INTERVAL (V_NAME_F1 (asinh), 0x1p-12, 1.0, 40000)
TEST_INTERVAL (V_NAME_F1 (asinh), 1.0, 0x1p11, 40000)
TEST_INTERVAL (V_NAME_F1 (asinh), 0x1p11, inf, 40000)
TEST_INTERVAL (V_NAME_F1 (asinh), -0, -0x1p-12, 20000)
TEST_INTERVAL (V_NAME_F1 (asinh), -0x1p-12, -1.0, 20000)
TEST_INTERVAL (V_NAME_F1 (asinh), -1.0, -0x1p11, 20000)
TEST_INTERVAL (V_NAME_F1 (asinh), -0x1p11, -inf, 20000)
