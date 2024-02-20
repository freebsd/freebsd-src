/*
 * Single-precision vector log function - inline version
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"

struct v_logf_data
{
  float32x4_t poly[7];
  float32x4_t ln2;
  uint32x4_t off, mantissa_mask;
};

#define V_LOGF_CONSTANTS                                                      \
  {                                                                           \
    .poly                                                                     \
	= { V4 (-0x1.3e737cp-3f), V4 (0x1.5a9aa2p-3f),	V4 (-0x1.4f9934p-3f), \
	    V4 (0x1.961348p-3f),  V4 (-0x1.00187cp-2f), V4 (0x1.555d7cp-2f),  \
	    V4 (-0x1.ffffc8p-2f) },                                           \
	.ln2 = V4 (0x1.62e43p-1f), .off = V4 (0x3f2aaaab),                    \
	.mantissa_mask = V4 (0x007fffff)                                      \
  }

#define P(i) d->poly[7 - i]

static inline float32x4_t
v_logf_inline (float32x4_t x, const struct v_logf_data *d)
{
  float32x4_t n, p, q, r, r2, y;
  uint32x4_t u;

  u = vreinterpretq_u32_f32 (x);

  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3.  */
  u = vsubq_u32 (u, d->off);
  n = vcvtq_f32_s32 (
      vshrq_n_s32 (vreinterpretq_s32_u32 (u), 23)); /* signextend.  */
  u = vandq_u32 (u, d->mantissa_mask);
  u = vaddq_u32 (u, d->off);
  r = vsubq_f32 (vreinterpretq_f32_u32 (u), v_f32 (1.0f));

  /* y = log(1+r) + n*ln2.  */
  r2 = vmulq_f32 (r, r);
  /* n*ln2 + r + r2*(P1 + r*P2 + r2*(P3 + r*P4 + r2*(P5 + r*P6 + r2*P7))).  */
  p = vfmaq_f32 (P (5), P (6), r);
  q = vfmaq_f32 (P (3), P (4), r);
  y = vfmaq_f32 (P (1), P (2), r);
  p = vfmaq_f32 (p, P (7), r2);
  q = vfmaq_f32 (q, p, r2);
  y = vfmaq_f32 (y, q, r2);
  p = vfmaq_f32 (r, d->ln2, n);

  return vfmaq_f32 (p, y, r2);
}

#undef P
