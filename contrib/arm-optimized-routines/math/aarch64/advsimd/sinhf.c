/*
 * Single-precision vector sinh(x) function.
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
#if WANT_SIMD_EXCEPT
  uint32x4_t tiny_bound, thresh;
#else
  float32x4_t oflow_bound;
#endif
} data = {
  .expm1f_consts = V_EXPM1F_DATA,
#if WANT_SIMD_EXCEPT
  /* 0x1.6a09e8p-32, below which expm1f underflows.  */
  .tiny_bound = V4 (0x2fb504f4),
  /* asuint(oflow_bound) - asuint(tiny_bound).  */
  .thresh = V4 (0x12fbbbb3),
#else
  /* 0x1.61814ep+6, above which expm1f helper overflows.  */
  .oflow_bound = V4 (0x1.61814ep+6),
#endif
};

static float32x4_t NOINLINE VPCS_ATTR
special_case (float32x4_t x, float32x4_t t, float32x4_t halfsign,
	      uint32x4_t special)
{
  return v_call_f32 (sinhf, x, vmulq_f32 (t, halfsign), special);
}

/* Approximation for vector single-precision sinh(x) using expm1.
   sinh(x) = (exp(x) - exp(-x)) / 2.
   The maximum error is 2.26 ULP:
   _ZGVnN4v_sinhf (0x1.e34a9ep-4) got 0x1.e469ep-4
				 want 0x1.e469e4p-4.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (sinh) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  uint32x4_t ix = vreinterpretq_u32_f32 (x);
  float32x4_t ax = vabsq_f32 (x);
  float32x4_t halfsign = vreinterpretq_f32_u32 (
      vbslq_u32 (v_u32 (0x80000000), ix, vreinterpretq_u32_f32 (v_f32 (0.5))));

#if WANT_SIMD_EXCEPT
  uint32x4_t special = vcgeq_u32 (
      vsubq_u32 (vreinterpretq_u32_f32 (ax), d->tiny_bound), d->thresh);
  ax = v_zerofy_f32 (ax, special);
#else
  uint32x4_t special = vcageq_f32 (x, d->oflow_bound);
#endif

  /* Up to the point that expm1f overflows, we can use it to calculate sinhf
       using a slight rearrangement of the definition of asinh. This allows us
     to retain acceptable accuracy for very small inputs.  */
  float32x4_t t = expm1f_inline (ax, &d->expm1f_consts);
  t = vaddq_f32 (t, vdivq_f32 (t, vaddq_f32 (t, v_f32 (1.0))));

  /* Fall back to the scalar variant for any lanes that should trigger an
     exception.  */
  if (unlikely (v_any_u32 (special)))
    return special_case (x, t, halfsign, special);

  return vmulq_f32 (t, halfsign);
}

HALF_WIDTH_ALIAS_F1 (sinh)

TEST_SIG (V, F, 1, sinh, -10.0, 10.0)
TEST_ULP (V_NAME_F1 (sinh), 1.76)
TEST_DISABLE_FENV_IF_NOT (V_NAME_F1 (sinh), WANT_SIMD_EXCEPT)
TEST_SYM_INTERVAL (V_NAME_F1 (sinh), 0, 0x2fb504f4, 1000)
TEST_SYM_INTERVAL (V_NAME_F1 (sinh), 0x2fb504f4, 0x42b0c0a7, 100000)
TEST_SYM_INTERVAL (V_NAME_F1 (sinh), 0x42b0c0a7, inf, 1000)
