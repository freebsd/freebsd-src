/*
 * Single-precision vector log function - inline version
 *
 * Copyright (c) 2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"

struct sv_logf_data
{
  float p1, p3, p5, p6, p0, p2, p4;
  float ln2;
  uint32_t off, mantissa_mask;
};

#define SV_LOGF_CONSTANTS                                                     \
  {                                                                           \
    .p0 = -0x1.ffffc8p-2f, .p1 = 0x1.555d7cp-2f, .p2 = -0x1.00187cp-2f,       \
    .p3 = 0x1.961348p-3f, .p4 = -0x1.4f9934p-3f, .p5 = 0x1.5a9aa2p-3f,        \
    .p6 = -0x1.3e737cp-3f, .ln2 = 0x1.62e43p-1f, .off = 0x3f2aaaab,           \
    .mantissa_mask = 0x007fffff                                               \
  }

static inline svfloat32_t
sv_logf_inline (svbool_t pg, svfloat32_t x, const struct sv_logf_data *d)
{
  svuint32_t u = svreinterpret_u32 (x);

  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3.  */
  u = svsub_x (pg, u, d->off);
  svfloat32_t n = svcvt_f32_s32_x (
      pg, svasr_x (pg, svreinterpret_s32_u32 (u), 23)); /* signextend.  */
  u = svand_x (pg, u, d->mantissa_mask);
  u = svadd_x (pg, u, d->off);
  svfloat32_t r = svsub_x (pg, svreinterpret_f32 (u), 1.0f);

  /* y = log(1+r) + n*ln2.  */
  svfloat32_t r2 = svmul_x (pg, r, r);
  /* n*ln2 + r + r2*(P1 + r*P2 + r2*(P3 + r*P4 + r2*(P5 + r*P6 + r2*P7))).  */
  svfloat32_t p1356 = svld1rq_f32 (svptrue_b32 (), &d->p1);
  svfloat32_t p = svmla_lane (sv_f32 (d->p4), r, p1356, 2);
  svfloat32_t q = svmla_lane (sv_f32 (d->p2), r, p1356, 1);
  svfloat32_t y = svmla_lane (sv_f32 (d->p0), r, p1356, 0);
  p = svmla_lane (p, r2, p1356, 3);
  q = svmla_x (pg, q, p, r2);
  y = svmla_x (pg, y, q, r2);
  p = svmla_x (pg, r, n, d->ln2);

  return svmla_x (pg, p, y, r2);
}
