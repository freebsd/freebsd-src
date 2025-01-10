/*
 * Helper for single-precision routines which calculate exp(x) - 1 and do not
 * need special-case handling
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef MATH_V_EXPM1F_INLINE_H
#define MATH_V_EXPM1F_INLINE_H

#include "v_math.h"

struct v_expm1f_data
{
  float32x4_t c0, c2;
  int32x4_t exponent_bias;
  float c1, c3, inv_ln2, c4;
  float ln2_hi, ln2_lo;
};

/* Coefficients generated using fpminimax with degree=5 in [-log(2)/2,
   log(2)/2]. Exponent bias is asuint(1.0f).  */
#define V_EXPM1F_DATA                                                         \
  {                                                                           \
    .c0 = V4 (0x1.fffffep-2), .c1 = 0x1.5554aep-3, .c2 = V4 (0x1.555736p-5),  \
    .c3 = 0x1.12287cp-7, .c4 = 0x1.6b55a2p-10,                                \
    .exponent_bias = V4 (0x3f800000), .inv_ln2 = 0x1.715476p+0f,              \
    .ln2_hi = 0x1.62e4p-1f, .ln2_lo = 0x1.7f7d1cp-20f,                        \
  }

static inline float32x4_t
expm1f_inline (float32x4_t x, const struct v_expm1f_data *d)
{
  /* Helper routine for calculating exp(x) - 1.  */

  float32x2_t ln2 = vld1_f32 (&d->ln2_hi);
  float32x4_t lane_consts = vld1q_f32 (&d->c1);

  /* Reduce argument: f in [-ln2/2, ln2/2], i is exact.  */
  float32x4_t j = vrndaq_f32 (vmulq_laneq_f32 (x, lane_consts, 2));
  int32x4_t i = vcvtq_s32_f32 (j);
  float32x4_t f = vfmsq_lane_f32 (x, j, ln2, 0);
  f = vfmsq_lane_f32 (f, j, ln2, 1);

  /* Approximate expm1(f) with polynomial P, expm1(f) ~= f + f^2 * P(f).  */
  float32x4_t f2 = vmulq_f32 (f, f);
  float32x4_t f4 = vmulq_f32 (f2, f2);
  float32x4_t p01 = vfmaq_laneq_f32 (d->c0, f, lane_consts, 0);
  float32x4_t p23 = vfmaq_laneq_f32 (d->c2, f, lane_consts, 1);
  float32x4_t p = vfmaq_f32 (p01, f2, p23);
  p = vfmaq_laneq_f32 (p, f4, lane_consts, 3);
  p = vfmaq_f32 (f, f2, p);

  /* t = 2^i.  */
  int32x4_t u = vaddq_s32 (vshlq_n_s32 (i, 23), d->exponent_bias);
  float32x4_t t = vreinterpretq_f32_s32 (u);
  /* expm1(x) ~= p * t + (t - 1).  */
  return vfmaq_f32 (vsubq_f32 (t, v_f32 (1.0f)), p, t);
}

#endif // MATH_V_EXPM1F_INLINE_H
