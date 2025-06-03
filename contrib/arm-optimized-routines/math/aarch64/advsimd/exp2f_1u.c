/*
 * Single-precision vector 2^x function.
 *
 * Copyright (c) 2019-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_defs.h"

static const struct data
{
  float32x4_t c0, c1, c2, c3, c4, c5, shift;
  uint32x4_t exponent_bias;
  float32x4_t special_bound, scale_thresh;
  uint32x4_t special_offset, special_bias;
} data = {
  .shift = V4 (0x1.8p23f),
  .exponent_bias = V4 (0x3f800000),
  .special_bound = V4 (126.0f),
  .scale_thresh = V4 (192.0f),
  .special_offset = V4 (0x82000000),
  .special_bias = V4 (0x7f000000),
  /*  maxerr: 0.878 ulp.  */
  .c0 = V4 (0x1.416b5ep-13f),
  .c1 = V4 (0x1.5f082ep-10f),
  .c2 = V4 (0x1.3b2dep-7f),
  .c3 = V4 (0x1.c6af7cp-5f),
  .c4 = V4 (0x1.ebfbdcp-3f),
  .c5 = V4 (0x1.62e43p-1f),
};

static float32x4_t VPCS_ATTR NOINLINE
specialcase (float32x4_t p, float32x4_t n, uint32x4_t e, const struct data *d)
{
  /* 2^n may overflow, break it up into s1*s2.  */
  uint32x4_t b = vandq_u32 (vclezq_f32 (n), d->special_offset);
  float32x4_t s1 = vreinterpretq_f32_u32 (vaddq_u32 (b, d->special_bias));
  float32x4_t s2 = vreinterpretq_f32_u32 (vsubq_u32 (e, b));
  uint32x4_t cmp = vcagtq_f32 (n, d->scale_thresh);
  float32x4_t r1 = vmulq_f32 (s1, s1);
  float32x4_t r0 = vmulq_f32 (vmulq_f32 (p, s1), s2);
  return vreinterpretq_f32_u32 ((cmp & vreinterpretq_u32_f32 (r1))
				| (~cmp & vreinterpretq_u32_f32 (r0)));
}

float32x4_t VPCS_ATTR
_ZGVnN4v_exp2f_1u (float32x4_t x)
{
  /* exp2(x) = 2^n * poly(r), with poly(r) in [1/sqrt(2),sqrt(2)]
     x = n + r, with r in [-1/2, 1/2].  */
  const struct data *d = ptr_barrier (&data);
  float32x4_t n = vrndaq_f32 (x);
  float32x4_t r = x - n;
  uint32x4_t e = vreinterpretq_u32_s32 (vcvtaq_s32_f32 (x)) << 23;
  float32x4_t scale = vreinterpretq_f32_u32 (e + d->exponent_bias);
  uint32x4_t cmp = vcagtq_f32 (n, d->special_bound);

  float32x4_t p = vfmaq_f32 (d->c1, d->c0, r);
  p = vfmaq_f32 (d->c2, p, r);
  p = vfmaq_f32 (d->c3, p, r);
  p = vfmaq_f32 (d->c4, p, r);
  p = vfmaq_f32 (d->c5, p, r);
  p = vfmaq_f32 (v_f32 (1.0f), p, r);
  if (unlikely (v_any_u32 (cmp)))
    return specialcase (p, n, e, d);
  return scale * p;
}

TEST_ULP (_ZGVnN4v_exp2f_1u, 0.4)
TEST_DISABLE_FENV (_ZGVnN4v_exp2f_1u)
TEST_INTERVAL (_ZGVnN4v_exp2f_1u, 0, 0xffff0000, 10000)
TEST_SYM_INTERVAL (_ZGVnN4v_exp2f_1u, 0x1p-14, 0x1p8, 500000)
