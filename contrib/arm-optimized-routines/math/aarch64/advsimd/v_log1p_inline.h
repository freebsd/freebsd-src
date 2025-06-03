/*
 * Helper for vector double-precision routines which calculate log(1 + x) and
 * do not need special-case handling
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#ifndef MATH_V_LOG1P_INLINE_H
#define MATH_V_LOG1P_INLINE_H

#include "v_math.h"

struct v_log1p_data
{
  float64x2_t c0, c2, c4, c6, c8, c10, c12, c14, c16;
  uint64x2_t hf_rt2_top, one_m_hf_rt2_top, umask;
  int64x2_t one_top;
  double c1, c3, c5, c7, c9, c11, c13, c15, c17, c18;
  double ln2[2];
};

/* Coefficients generated using Remez, deg=20, in [sqrt(2)/2-1, sqrt(2)-1].  */
#define V_LOG1P_CONSTANTS_TABLE                                               \
  {                                                                           \
    .c0 = V2 (-0x1.ffffffffffffbp-2), .c1 = 0x1.55555555551a9p-2,             \
    .c2 = V2 (-0x1.00000000008e3p-2), .c3 = 0x1.9999999a32797p-3,             \
    .c4 = V2 (-0x1.555555552fecfp-3), .c5 = 0x1.249248e071e5ap-3,             \
    .c6 = V2 (-0x1.ffffff8bf8482p-4), .c7 = 0x1.c71c8f07da57ap-4,             \
    .c8 = V2 (-0x1.9999ca4ccb617p-4), .c9 = 0x1.7459ad2e1dfa3p-4,             \
    .c10 = V2 (-0x1.554d2680a3ff2p-4), .c11 = 0x1.3b4c54d487455p-4,           \
    .c12 = V2 (-0x1.2548a9ffe80e6p-4), .c13 = 0x1.0f389a24b2e07p-4,           \
    .c14 = V2 (-0x1.eee4db15db335p-5), .c15 = 0x1.e95b494d4a5ddp-5,           \
    .c16 = V2 (-0x1.15fdf07cb7c73p-4), .c17 = 0x1.0310b70800fcfp-4,           \
    .c18 = -0x1.cfa7385bdb37ep-6,                                             \
    .ln2 = { 0x1.62e42fefa3800p-1, 0x1.ef35793c76730p-45 },                   \
    .hf_rt2_top = V2 (0x3fe6a09e00000000),                                    \
    .one_m_hf_rt2_top = V2 (0x00095f6200000000),                              \
    .umask = V2 (0x000fffff00000000), .one_top = V2 (0x3ff)                   \
  }

#define BottomMask v_u64 (0xffffffff)

static inline float64x2_t
eval_poly (float64x2_t m, float64x2_t m2, const struct v_log1p_data *d)
{
  /* Approximate log(1+m) on [-0.25, 0.5] using pairwise Horner.  */
  float64x2_t c13 = vld1q_f64 (&d->c1);
  float64x2_t c57 = vld1q_f64 (&d->c5);
  float64x2_t c911 = vld1q_f64 (&d->c9);
  float64x2_t c1315 = vld1q_f64 (&d->c13);
  float64x2_t c1718 = vld1q_f64 (&d->c17);
  float64x2_t p1617 = vfmaq_laneq_f64 (d->c16, m, c1718, 0);
  float64x2_t p1415 = vfmaq_laneq_f64 (d->c14, m, c1315, 1);
  float64x2_t p1213 = vfmaq_laneq_f64 (d->c12, m, c1315, 0);
  float64x2_t p1011 = vfmaq_laneq_f64 (d->c10, m, c911, 1);
  float64x2_t p89 = vfmaq_laneq_f64 (d->c8, m, c911, 0);
  float64x2_t p67 = vfmaq_laneq_f64 (d->c6, m, c57, 1);
  float64x2_t p45 = vfmaq_laneq_f64 (d->c4, m, c57, 0);
  float64x2_t p23 = vfmaq_laneq_f64 (d->c2, m, c13, 1);
  float64x2_t p01 = vfmaq_laneq_f64 (d->c0, m, c13, 0);
  float64x2_t p = vfmaq_laneq_f64 (p1617, m2, c1718, 1);
  p = vfmaq_f64 (p1415, m2, p);
  p = vfmaq_f64 (p1213, m2, p);
  p = vfmaq_f64 (p1011, m2, p);
  p = vfmaq_f64 (p89, m2, p);
  p = vfmaq_f64 (p67, m2, p);
  p = vfmaq_f64 (p45, m2, p);
  p = vfmaq_f64 (p23, m2, p);
  return vfmaq_f64 (p01, m2, p);
}

static inline float64x2_t
log1p_inline (float64x2_t x, const struct v_log1p_data *d)
{
  /* Helper for calculating log(x + 1):
     - No special-case handling - this should be dealt with by the caller.
     - Optionally simulate the shortcut for k=0, used in the scalar routine,
       using v_sel, for improved accuracy when the argument to log1p is close
       to 0. This feature is enabled by defining WANT_V_LOG1P_K0_SHORTCUT as 1
       in the source of the caller before including this file.  */
  float64x2_t m = vaddq_f64 (x, v_f64 (1.0));
  uint64x2_t mi = vreinterpretq_u64_f64 (m);
  uint64x2_t u = vaddq_u64 (mi, d->one_m_hf_rt2_top);

  int64x2_t ki
      = vsubq_s64 (vreinterpretq_s64_u64 (vshrq_n_u64 (u, 52)), d->one_top);
  float64x2_t k = vcvtq_f64_s64 (ki);

  /* Reduce x to f in [sqrt(2)/2, sqrt(2)].  */
  uint64x2_t utop = vaddq_u64 (vandq_u64 (u, d->umask), d->hf_rt2_top);
  uint64x2_t u_red = vorrq_u64 (utop, vandq_u64 (mi, BottomMask));
  float64x2_t f = vsubq_f64 (vreinterpretq_f64_u64 (u_red), v_f64 (1.0));

  /* Correction term c/m.  */
  float64x2_t cm = vdivq_f64 (vsubq_f64 (x, vsubq_f64 (m, v_f64 (1.0))), m);

#ifndef WANT_V_LOG1P_K0_SHORTCUT
# error                                                                       \
      "Cannot use v_log1p_inline.h without specifying whether you need the k0 shortcut for greater accuracy close to 0"
#elif WANT_V_LOG1P_K0_SHORTCUT
  /* Shortcut if k is 0 - set correction term to 0 and f to x. The result is
     that the approximation is solely the polynomial.  */
  uint64x2_t k0 = vceqzq_f64 (k);
  cm = v_zerofy_f64 (cm, k0);
  f = vbslq_f64 (k0, x, f);
#endif

  /* Approximate log1p(f) on the reduced input using a polynomial.  */
  float64x2_t f2 = vmulq_f64 (f, f);
  float64x2_t p = eval_poly (f, f2, d);

  /* Assemble log1p(x) = k * log2 + log1p(f) + c/m.  */
  float64x2_t ln2 = vld1q_f64 (&d->ln2[0]);
  float64x2_t ylo = vfmaq_laneq_f64 (cm, k, ln2, 1);
  float64x2_t yhi = vfmaq_laneq_f64 (f, k, ln2, 0);
  return vfmaq_f64 (vaddq_f64 (ylo, yhi), f2, p);
}

#endif // MATH_V_LOG1P_INLINE_H
