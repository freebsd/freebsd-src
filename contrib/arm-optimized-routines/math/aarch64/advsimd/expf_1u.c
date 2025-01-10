/*
 * Single-precision vector e^x function.
 *
 * Copyright (c) 2019-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "v_math.h"
#include "test_defs.h"

static const struct data
{
  float32x4_t shift, inv_ln2;
  uint32x4_t exponent_bias;
  float32x4_t c1, c2, c3, c4;
  float32x4_t special_bound, scale_thresh;
  uint32x4_t special_offset, special_bias;
  float ln2_hi, ln2_lo, c0, nothing;
} data = {
  .ln2_hi = 0x1.62e4p-1f,
  .ln2_lo = 0x1.7f7d1cp-20f,
  .shift = V4 (0x1.8p23f),
  .inv_ln2 = V4 (0x1.715476p+0f),
  .exponent_bias = V4 (0x3f800000),
  .special_bound = V4 (126.0f),
  .scale_thresh = V4 (192.0f),
  .special_offset = V4 (0x83000000),
  .special_bias = V4 (0x7f000000),
  /*  maxerr: 0.36565 +0.5 ulp.  */
  .c0 = 0x1.6a6000p-10f,
  .c1 = V4 (0x1.12718ep-7f),
  .c2 = V4 (0x1.555af0p-5f),
  .c3 = V4 (0x1.555430p-3f),
  .c4 = V4 (0x1.fffff4p-2f),
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
_ZGVnN4v_expf_1u (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);
  float32x4_t ln2_c0 = vld1q_f32 (&d->ln2_hi);

  /* exp(x) = 2^n * poly(r), with poly(r) in [1/sqrt(2),sqrt(2)]
     x = ln2*n + r, with r in [-ln2/2, ln2/2].  */
  float32x4_t z = vmulq_f32 (x, d->inv_ln2);
  float32x4_t n = vrndaq_f32 (z);
  float32x4_t r = vfmsq_laneq_f32 (x, n, ln2_c0, 0);
  r = vfmsq_laneq_f32 (r, n, ln2_c0, 1);
  uint32x4_t e = vshlq_n_u32 (vreinterpretq_u32_s32 (vcvtaq_s32_f32 (z)), 23);
  float32x4_t scale = vreinterpretq_f32_u32 (e + d->exponent_bias);
  uint32x4_t cmp = vcagtq_f32 (n, d->special_bound);
  float32x4_t p = vfmaq_laneq_f32 (d->c1, r, ln2_c0, 2);
  p = vfmaq_f32 (d->c2, p, r);
  p = vfmaq_f32 (d->c3, p, r);
  p = vfmaq_f32 (d->c4, p, r);
  p = vfmaq_f32 (v_f32 (1.0f), p, r);
  p = vfmaq_f32 (v_f32 (1.0f), p, r);
  if (unlikely (v_any_u32 (cmp)))
    return specialcase (p, n, e, d);
  return scale * p;
}

TEST_ULP (_ZGVnN4v_expf_1u, 0.4)
TEST_DISABLE_FENV (_ZGVnN4v_expf_1u)
TEST_INTERVAL (_ZGVnN4v_expf_1u, 0, 0xffff0000, 10000)
TEST_SYM_INTERVAL (_ZGVnN4v_expf_1u, 0x1p-14, 0x1p8, 500000)
