/*
 * Single-precision vector log2 function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_sig.h"
#include "test_defs.h"

static const struct data
{
  float32x4_t c0, c2, c4, c6, c8;
  uint32x4_t off, offset_lower_bound;
  uint16x8_t special_bound;
  uint32x4_t mantissa_mask;
  float c1, c3, c5, c7;
} data = {
  /* Coefficients generated using Remez algorithm approximate
     log2(1+r)/r for r in [ -1/3, 1/3 ].
     rel error: 0x1.c4c4b0cp-26.  */
  .c0 = V4 (0x1.715476p0f), /* (float)(1 / ln(2)).  */
  .c1 = -0x1.715458p-1f,
  .c2 = V4 (0x1.ec701cp-2f),
  .c3 = -0x1.7171a4p-2f,
  .c4 = V4 (0x1.27a0b8p-2f),
  .c5 = -0x1.e5143ep-3f,
  .c6 = V4 (0x1.9d8ecap-3f),
  .c7 = -0x1.c675bp-3f,
  .c8 = V4 (0x1.9e495p-3f),
  /* Lower bound is the smallest positive normal float 0x00800000. For
     optimised register use subnormals are detected after offset has been
     subtracted, so lower bound is 0x0080000 - offset (which wraps around).  */
  .offset_lower_bound = V4 (0x00800000 - 0x3f2aaaab),
  .special_bound = V8 (0x7f00), /* top16(asuint32(inf) - 0x00800000).  */
  .off = V4 (0x3f2aaaab),	/* 0.666667.  */
  .mantissa_mask = V4 (0x007fffff),
};

static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t n, uint32x4_t u_off, float32x4_t p, float32x4_t r,
	      uint16x4_t cmp, const struct data *d)
{
  /* Fall back to scalar code.  */
  return v_call_f32 (log2f, vreinterpretq_f32_u32 (vaddq_u32 (u_off, d->off)),
		     vfmaq_f32 (n, p, r), vmovl_u16 (cmp));
}

/* Fast implementation for single precision AdvSIMD log2,
   relies on same argument reduction as AdvSIMD logf.
   Maximum error: 2.48 ULPs
   _ZGVnN4v_log2f(0x1.558174p+0) got 0x1.a9be84p-2
				want 0x1.a9be8p-2.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (log2) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  /* To avoid having to mov x out of the way, keep u after offset has been
     applied, and recover x by adding the offset back in the special-case
     handler.  */
  uint32x4_t u_off = vreinterpretq_u32_f32 (x);

  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3.  */
  u_off = vsubq_u32 (u_off, d->off);
  float32x4_t n = vcvtq_f32_s32 (
      vshrq_n_s32 (vreinterpretq_s32_u32 (u_off), 23)); /* signextend.  */

  uint16x4_t special = vcge_u16 (vsubhn_u32 (u_off, d->offset_lower_bound),
				 vget_low_u16 (d->special_bound));

  uint32x4_t u = vaddq_u32 (vandq_u32 (u_off, d->mantissa_mask), d->off);
  float32x4_t r = vsubq_f32 (vreinterpretq_f32_u32 (u), v_f32 (1.0f));

  /* y = log2(1+r) + n.  */
  float32x4_t r2 = vmulq_f32 (r, r);

  float32x4_t c1357 = vld1q_f32 (&d->c1);
  float32x4_t c01 = vfmaq_laneq_f32 (d->c0, r, c1357, 0);
  float32x4_t c23 = vfmaq_laneq_f32 (d->c2, r, c1357, 1);
  float32x4_t c45 = vfmaq_laneq_f32 (d->c4, r, c1357, 2);
  float32x4_t c67 = vfmaq_laneq_f32 (d->c6, r, c1357, 3);
  float32x4_t p68 = vfmaq_f32 (c67, r2, d->c8);
  float32x4_t p48 = vfmaq_f32 (c45, r2, p68);
  float32x4_t p28 = vfmaq_f32 (c23, r2, p48);
  float32x4_t p = vfmaq_f32 (c01, r2, p28);

  if (unlikely (v_any_u16h (special)))
    return special_case (n, u_off, p, r, special, d);
  return vfmaq_f32 (n, p, r);
}

HALF_WIDTH_ALIAS_F1 (log2)

TEST_SIG (V, F, 1, log2, 0.01, 11.1)
TEST_ULP (V_NAME_F1 (log2), 1.99)
TEST_INTERVAL (V_NAME_F1 (log2), -0.0, -0x1p126, 100)
TEST_INTERVAL (V_NAME_F1 (log2), 0x1p-149, 0x1p-126, 4000)
TEST_INTERVAL (V_NAME_F1 (log2), 0x1p-126, 0x1p-23, 50000)
TEST_INTERVAL (V_NAME_F1 (log2), 0x1p-23, 1.0, 50000)
TEST_INTERVAL (V_NAME_F1 (log2), 1.0, 100, 50000)
TEST_INTERVAL (V_NAME_F1 (log2), 100, inf, 50000)
