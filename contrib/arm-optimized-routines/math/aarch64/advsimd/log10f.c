/*
 * Single-precision vector log10 function.
 *
 * Copyright (c) 2020-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_sig.h"
#include "test_defs.h"

static const struct data
{
  float32x4_t c0, c2, c4, c6, inv_ln10, ln2;
  uint32x4_t off, offset_lower_bound;
  uint16x8_t special_bound;
  uint32x4_t mantissa_mask;
  float c1, c3, c5, c7;
} data = {
  /* Use order 9 for log10(1+x), i.e. order 8 for log10(1+x)/x, with x in
      [-1/3, 1/3] (offset=2/3). Max. relative error: 0x1.068ee468p-25.  */
  .c0 = V4 (-0x1.bcb79cp-3f),
  .c1 = 0x1.2879c8p-3f,
  .c2 = V4 (-0x1.bcd472p-4f),
  .c3 = 0x1.6408f8p-4f,
  .c4 = V4 (-0x1.246f8p-4f),
  .c5 = 0x1.f0e514p-5f,
  .c6 = V4 (-0x1.0fc92cp-4f),
  .c7 = 0x1.f5f76ap-5f,
  .ln2 = V4 (0x1.62e43p-1f),
  .inv_ln10 = V4 (0x1.bcb7b2p-2f),
  /* Lower bound is the smallest positive normal float 0x00800000. For
     optimised register use subnormals are detected after offset has been
     subtracted, so lower bound is 0x0080000 - offset (which wraps around).  */
  .offset_lower_bound = V4 (0x00800000 - 0x3f2aaaab),
  .special_bound = V8 (0x7f00), /* top16(asuint32(inf) - 0x00800000).  */
  .off = V4 (0x3f2aaaab),	/* 0.666667.  */
  .mantissa_mask = V4 (0x007fffff),
};

static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t y, uint32x4_t u_off, float32x4_t p, float32x4_t r2,
	      uint16x4_t cmp, const struct data *d)
{
  /* Fall back to scalar code.  */
  return v_call_f32 (log10f, vreinterpretq_f32_u32 (vaddq_u32 (u_off, d->off)),
		     vfmaq_f32 (y, p, r2), vmovl_u16 (cmp));
}

/* Fast implementation of AdvSIMD log10f,
   uses a similar approach as AdvSIMD logf with the same offset (i.e., 2/3) and
   an order 9 polynomial.
   Maximum error: 3.305ulps (nearest rounding.)
   _ZGVnN4v_log10f(0x1.555c16p+0) got 0x1.ffe2fap-4
				 want 0x1.ffe2f4p-4.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (log10) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);
  float32x4_t c1357 = vld1q_f32 (&d->c1);
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

  /* y = log10(1+r) + n * log10(2).  */
  float32x4_t r2 = vmulq_f32 (r, r);

  float32x4_t c01 = vfmaq_laneq_f32 (d->c0, r, c1357, 0);
  float32x4_t c23 = vfmaq_laneq_f32 (d->c2, r, c1357, 1);
  float32x4_t c45 = vfmaq_laneq_f32 (d->c4, r, c1357, 2);
  float32x4_t c67 = vfmaq_laneq_f32 (d->c6, r, c1357, 3);

  float32x4_t p47 = vfmaq_f32 (c45, r2, c67);
  float32x4_t p27 = vfmaq_f32 (c23, r2, p47);
  float32x4_t poly = vfmaq_f32 (c01, r2, p27);

  /* y = Log10(2) * n + poly * InvLn(10).  */
  float32x4_t y = vfmaq_f32 (r, d->ln2, n);
  y = vmulq_f32 (y, d->inv_ln10);

  if (unlikely (v_any_u16h (special)))
    return special_case (y, u_off, poly, r2, special, d);
  return vfmaq_f32 (y, poly, r2);
}

HALF_WIDTH_ALIAS_F1 (log10)

TEST_SIG (V, F, 1, log10, 0.01, 11.1)
TEST_ULP (V_NAME_F1 (log10), 2.81)
TEST_INTERVAL (V_NAME_F1 (log10), -0.0, -inf, 100)
TEST_INTERVAL (V_NAME_F1 (log10), 0, 0x1p-126, 100)
TEST_INTERVAL (V_NAME_F1 (log10), 0x1p-126, 0x1p-23, 50000)
TEST_INTERVAL (V_NAME_F1 (log10), 0x1p-23, 1.0, 50000)
TEST_INTERVAL (V_NAME_F1 (log10), 1.0, 100, 50000)
TEST_INTERVAL (V_NAME_F1 (log10), 100, inf, 50000)
