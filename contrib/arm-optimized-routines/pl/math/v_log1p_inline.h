/*
 * Helper for vector double-precision routines which calculate log(1 + x) and do
 * not need special-case handling
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#ifndef PL_MATH_V_LOG1P_INLINE_H
#define PL_MATH_V_LOG1P_INLINE_H

#include "v_math.h"
#include "poly_advsimd_f64.h"

struct v_log1p_data
{
  float64x2_t poly[19], ln2[2];
  uint64x2_t hf_rt2_top, one_m_hf_rt2_top, umask;
  int64x2_t one_top;
};

/* Coefficients generated using Remez, deg=20, in [sqrt(2)/2-1, sqrt(2)-1].  */
#define V_LOG1P_CONSTANTS_TABLE                                               \
  {                                                                           \
    .poly = { V2 (-0x1.ffffffffffffbp-2), V2 (0x1.55555555551a9p-2),          \
	      V2 (-0x1.00000000008e3p-2), V2 (0x1.9999999a32797p-3),          \
	      V2 (-0x1.555555552fecfp-3), V2 (0x1.249248e071e5ap-3),          \
	      V2 (-0x1.ffffff8bf8482p-4), V2 (0x1.c71c8f07da57ap-4),          \
	      V2 (-0x1.9999ca4ccb617p-4), V2 (0x1.7459ad2e1dfa3p-4),          \
	      V2 (-0x1.554d2680a3ff2p-4), V2 (0x1.3b4c54d487455p-4),          \
	      V2 (-0x1.2548a9ffe80e6p-4), V2 (0x1.0f389a24b2e07p-4),          \
	      V2 (-0x1.eee4db15db335p-5), V2 (0x1.e95b494d4a5ddp-5),          \
	      V2 (-0x1.15fdf07cb7c73p-4), V2 (0x1.0310b70800fcfp-4),          \
	      V2 (-0x1.cfa7385bdb37ep-6) },                                   \
    .ln2 = { V2 (0x1.62e42fefa3800p-1), V2 (0x1.ef35793c76730p-45) },         \
    .hf_rt2_top = V2 (0x3fe6a09e00000000),                                    \
    .one_m_hf_rt2_top = V2 (0x00095f6200000000),                              \
    .umask = V2 (0x000fffff00000000), .one_top = V2 (0x3ff)                   \
  }

#define BottomMask v_u64 (0xffffffff)

static inline float64x2_t
log1p_inline (float64x2_t x, const struct v_log1p_data *d)
{
  /* Helper for calculating log(x + 1). Copied from v_log1p_2u5.c, with several
     modifications:
     - No special-case handling - this should be dealt with by the caller.
     - Pairwise Horner polynomial evaluation for improved accuracy.
     - Optionally simulate the shortcut for k=0, used in the scalar routine,
       using v_sel, for improved accuracy when the argument to log1p is close to
       0. This feature is enabled by defining WANT_V_LOG1P_K0_SHORTCUT as 1 in
       the source of the caller before including this file.
     See v_log1pf_2u1.c for details of the algorithm.  */
  float64x2_t m = vaddq_f64 (x, v_f64 (1));
  uint64x2_t mi = vreinterpretq_u64_f64 (m);
  uint64x2_t u = vaddq_u64 (mi, d->one_m_hf_rt2_top);

  int64x2_t ki
      = vsubq_s64 (vreinterpretq_s64_u64 (vshrq_n_u64 (u, 52)), d->one_top);
  float64x2_t k = vcvtq_f64_s64 (ki);

  /* Reduce x to f in [sqrt(2)/2, sqrt(2)].  */
  uint64x2_t utop = vaddq_u64 (vandq_u64 (u, d->umask), d->hf_rt2_top);
  uint64x2_t u_red = vorrq_u64 (utop, vandq_u64 (mi, BottomMask));
  float64x2_t f = vsubq_f64 (vreinterpretq_f64_u64 (u_red), v_f64 (1));

  /* Correction term c/m.  */
  float64x2_t cm = vdivq_f64 (vsubq_f64 (x, vsubq_f64 (m, v_f64 (1))), m);

#ifndef WANT_V_LOG1P_K0_SHORTCUT
#error                                                                         \
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
  float64x2_t p = v_pw_horner_18_f64 (f, f2, d->poly);

  /* Assemble log1p(x) = k * log2 + log1p(f) + c/m.  */
  float64x2_t ylo = vfmaq_f64 (cm, k, d->ln2[1]);
  float64x2_t yhi = vfmaq_f64 (f, k, d->ln2[0]);
  return vfmaq_f64 (vaddq_f64 (ylo, yhi), f2, p);
}

#endif // PL_MATH_V_LOG1P_INLINE_H
