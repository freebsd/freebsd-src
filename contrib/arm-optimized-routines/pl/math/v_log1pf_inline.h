/*
 * Helper for single-precision routines which calculate log(1 + x) and do not
 * need special-case handling
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef PL_MATH_V_LOG1PF_INLINE_H
#define PL_MATH_V_LOG1PF_INLINE_H

#include "v_math.h"
#include "poly_advsimd_f32.h"

struct v_log1pf_data
{
  float32x4_t poly[8], ln2;
  uint32x4_t four;
  int32x4_t three_quarters;
};

/* Polynomial generated using FPMinimax in [-0.25, 0.5]. First two coefficients
   (1, -0.5) are not stored as they can be generated more efficiently.  */
#define V_LOG1PF_CONSTANTS_TABLE                                              \
  {                                                                           \
    .poly                                                                     \
	= { V4 (0x1.5555aap-2f),  V4 (-0x1.000038p-2f), V4 (0x1.99675cp-3f),  \
	    V4 (-0x1.54ef78p-3f), V4 (0x1.28a1f4p-3f),	V4 (-0x1.0da91p-3f),  \
	    V4 (0x1.abcb6p-4f),	  V4 (-0x1.6f0d5ep-5f) },                     \
	.ln2 = V4 (0x1.62e43p-1f), .four = V4 (0x40800000),                   \
	.three_quarters = V4 (0x3f400000)                                     \
  }

static inline float32x4_t
eval_poly (float32x4_t m, const float32x4_t *c)
{
  /* Approximate log(1+m) on [-0.25, 0.5] using pairwise Horner (main routine
     uses split Estrin, but this way reduces register pressure in the calling
     routine).  */
  float32x4_t q = vfmaq_f32 (v_f32 (-0.5), m, c[0]);
  float32x4_t m2 = vmulq_f32 (m, m);
  q = vfmaq_f32 (m, m2, q);
  float32x4_t p = v_pw_horner_6_f32 (m, m2, c + 1);
  p = vmulq_f32 (m2, p);
  return vfmaq_f32 (q, m2, p);
}

static inline float32x4_t
log1pf_inline (float32x4_t x, const struct v_log1pf_data d)
{
  /* Helper for calculating log(x + 1). Copied from log1pf_2u1.c, with no
     special-case handling. See that file for details of the algorithm.  */
  float32x4_t m = vaddq_f32 (x, v_f32 (1.0f));
  int32x4_t k
      = vandq_s32 (vsubq_s32 (vreinterpretq_s32_f32 (m), d.three_quarters),
		   v_s32 (0xff800000));
  uint32x4_t ku = vreinterpretq_u32_s32 (k);
  float32x4_t s = vreinterpretq_f32_u32 (vsubq_u32 (d.four, ku));
  float32x4_t m_scale
      = vreinterpretq_f32_u32 (vsubq_u32 (vreinterpretq_u32_f32 (x), ku));
  m_scale = vaddq_f32 (m_scale, vfmaq_f32 (v_f32 (-1.0f), v_f32 (0.25f), s));
  float32x4_t p = eval_poly (m_scale, d.poly);
  float32x4_t scale_back = vmulq_f32 (vcvtq_f32_s32 (k), v_f32 (0x1.0p-23f));
  return vfmaq_f32 (p, scale_back, d.ln2);
}

#endif //  PL_MATH_V_LOG1PF_INLINE_H
