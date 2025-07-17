/*
 * SVE helper for single-precision routines which calculate exp(x) and do
 * not need special-case handling
 *
 * Copyright (c) 2023-2025, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef MATH_SV_EXPF_INLINE_H
#define MATH_SV_EXPF_INLINE_H

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"

struct sv_expf_data
{
  float c1, c3, inv_ln2;
  float ln2_lo, c0, c2, c4;
  float ln2_hi, shift;
};

/* Coefficients copied from the polynomial in AdvSIMD variant, reversed for
   compatibility with polynomial helpers. Shift is 1.5*2^17 + 127.  */
#define SV_EXPF_DATA                                                          \
  {                                                                           \
    /* Coefficients copied from the polynomial in AdvSIMD variant.  */        \
    .c0 = 0x1.ffffecp-1f, .c1 = 0x1.fffdb6p-2f, .c2 = 0x1.555e66p-3f,         \
    .c3 = 0x1.573e2ep-5f, .c4 = 0x1.0e4020p-7f, .inv_ln2 = 0x1.715476p+0f,    \
    .ln2_hi = 0x1.62e4p-1f, .ln2_lo = 0x1.7f7d1cp-20f,                        \
    .shift = 0x1.803f8p17f,                                                   \
  }

#define C(i) sv_f32 (d->poly[i])

static inline svfloat32_t
expf_inline (svfloat32_t x, const svbool_t pg, const struct sv_expf_data *d)
{
  /* exp(x) = 2^n (1 + poly(r)), with 1 + poly(r) in [1/sqrt(2),sqrt(2)]
     x = ln2*n + r, with r in [-ln2/2, ln2/2].  */

  svfloat32_t lane_consts = svld1rq (svptrue_b32 (), &d->ln2_lo);

  /* n = round(x/(ln2/N)).  */
  svfloat32_t z = svmad_x (pg, sv_f32 (d->inv_ln2), x, d->shift);
  svfloat32_t n = svsub_x (pg, z, d->shift);

  /* r = x - n*ln2/N.  */
  svfloat32_t r = svmsb_x (pg, sv_f32 (d->ln2_hi), n, x);
  r = svmls_lane (r, n, lane_consts, 0);

  /* scale = 2^(n/N).  */
  svfloat32_t scale = svexpa (svreinterpret_u32 (z));

  /* poly(r) = exp(r) - 1 ~= C0 r + C1 r^2 + C2 r^3 + C3 r^4 + C4 r^5.  */
  svfloat32_t p12 = svmla_lane (sv_f32 (d->c1), r, lane_consts, 2);
  svfloat32_t p34 = svmla_lane (sv_f32 (d->c3), r, lane_consts, 3);
  svfloat32_t r2 = svmul_x (svptrue_b32 (), r, r);
  svfloat32_t p14 = svmla_x (pg, p12, p34, r2);
  svfloat32_t p0 = svmul_lane (r, lane_consts, 1);
  svfloat32_t poly = svmla_x (pg, p0, r2, p14);

  return svmla_x (pg, scale, scale, poly);
}

#endif // MATH_SV_EXPF_INLINE_H
