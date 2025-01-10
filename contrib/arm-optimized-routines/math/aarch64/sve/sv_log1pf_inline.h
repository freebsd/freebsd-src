/*
 * Helper for SVE routines which calculate log(1 + x) and do not
 * need special-case handling
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef MATH_SV_LOG1PF_INLINE_H
#define MATH_SV_LOG1PF_INLINE_H

#define SignExponentMask 0xff800000

static const struct sv_log1pf_data
{
  float c0, c2, c4, c6;
  float c1, c3, c5, c7;
  float ln2, exp_bias, quarter;
  uint32_t four, three_quarters;
} sv_log1pf_data = {
  /* Do not store first term of polynomial, which is -0.5, as
     this can be fmov-ed directly instead of including it in
     the main load-and-mla polynomial schedule.  */
  .c0 = 0x1.5555aap-2f,		.c1 = -0x1.000038p-2f, .c2 = 0x1.99675cp-3f,
  .c3 = -0x1.54ef78p-3f,	.c4 = 0x1.28a1f4p-3f,  .c5 = -0x1.0da91p-3f,
  .c6 = 0x1.abcb6p-4f,		.c7 = -0x1.6f0d5ep-5f, .ln2 = 0x1.62e43p-1f,
  .exp_bias = 0x1p-23f,		.quarter = 0x1p-2f,    .four = 0x40800000,
  .three_quarters = 0x3f400000,
};

static inline svfloat32_t
sv_log1pf_inline (svfloat32_t x, svbool_t pg)
{
  const struct sv_log1pf_data *d = ptr_barrier (&sv_log1pf_data);

  /* With x + 1 = t * 2^k (where t = m + 1 and k is chosen such that m
			 is in [-0.25, 0.5]):
   log1p(x) = log(t) + log(2^k) = log1p(m) + k*log(2).

   We approximate log1p(m) with a polynomial, then scale by
   k*log(2). Instead of doing this directly, we use an intermediate
   scale factor s = 4*k*log(2) to ensure the scale is representable
   as a normalised fp32 number.  */
  svfloat32_t m = svadd_x (pg, x, 1);

  /* Choose k to scale x to the range [-1/4, 1/2].  */
  svint32_t k
      = svand_x (pg, svsub_x (pg, svreinterpret_s32 (m), d->three_quarters),
		 sv_s32 (SignExponentMask));

  /* Scale x by exponent manipulation.  */
  svfloat32_t m_scale = svreinterpret_f32 (
      svsub_x (pg, svreinterpret_u32 (x), svreinterpret_u32 (k)));

  /* Scale up to ensure that the scale factor is representable as normalised
     fp32 number, and scale m down accordingly.  */
  svfloat32_t s = svreinterpret_f32 (svsubr_x (pg, k, d->four));
  svfloat32_t fconst = svld1rq_f32 (svptrue_b32 (), &d->ln2);
  m_scale = svadd_x (pg, m_scale, svmla_lane_f32 (sv_f32 (-1), s, fconst, 2));

  /* Evaluate polynomial on reduced interval.  */
  svfloat32_t ms2 = svmul_x (svptrue_b32 (), m_scale, m_scale);

  svfloat32_t c1357 = svld1rq_f32 (svptrue_b32 (), &d->c1);
  svfloat32_t p01 = svmla_lane_f32 (sv_f32 (d->c0), m_scale, c1357, 0);
  svfloat32_t p23 = svmla_lane_f32 (sv_f32 (d->c2), m_scale, c1357, 1);
  svfloat32_t p45 = svmla_lane_f32 (sv_f32 (d->c4), m_scale, c1357, 2);
  svfloat32_t p67 = svmla_lane_f32 (sv_f32 (d->c6), m_scale, c1357, 3);

  svfloat32_t p = svmla_x (pg, p45, p67, ms2);
  p = svmla_x (pg, p23, p, ms2);
  p = svmla_x (pg, p01, p, ms2);

  p = svmad_x (pg, m_scale, p, -0.5);
  p = svmla_x (pg, m_scale, m_scale, svmul_x (pg, m_scale, p));

  /* The scale factor to be applied back at the end - by multiplying float(k)
   by 2^-23 we get the unbiased exponent of k.  */
  svfloat32_t scale_back = svmul_lane_f32 (svcvt_f32_x (pg, k), fconst, 1);
  return svmla_lane_f32 (p, scale_back, fconst, 0);
}

#endif //  SV_LOG1PF_INLINE_H
