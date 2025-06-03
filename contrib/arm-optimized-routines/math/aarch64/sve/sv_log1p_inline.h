/*
 * Helper for SVE double-precision routines which calculate log(1 + x) and do
 * not need special-case handling
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#ifndef MATH_SV_LOG1P_INLINE_H
#define MATH_SV_LOG1P_INLINE_H

#include "sv_math.h"
#include "sv_poly_f64.h"

static const struct sv_log1p_data
{
  double poly[19], ln2[2];
  uint64_t hf_rt2_top;
  uint64_t one_m_hf_rt2_top;
  uint32_t bottom_mask;
  int64_t one_top;
} sv_log1p_data = {
  /* Coefficients generated using Remez, deg=20, in [sqrt(2)/2-1, sqrt(2)-1].
   */
  .poly = { -0x1.ffffffffffffbp-2, 0x1.55555555551a9p-2, -0x1.00000000008e3p-2,
	    0x1.9999999a32797p-3, -0x1.555555552fecfp-3, 0x1.249248e071e5ap-3,
	    -0x1.ffffff8bf8482p-4, 0x1.c71c8f07da57ap-4, -0x1.9999ca4ccb617p-4,
	    0x1.7459ad2e1dfa3p-4, -0x1.554d2680a3ff2p-4, 0x1.3b4c54d487455p-4,
	    -0x1.2548a9ffe80e6p-4, 0x1.0f389a24b2e07p-4, -0x1.eee4db15db335p-5,
	    0x1.e95b494d4a5ddp-5, -0x1.15fdf07cb7c73p-4, 0x1.0310b70800fcfp-4,
	    -0x1.cfa7385bdb37ep-6 },
  .ln2 = { 0x1.62e42fefa3800p-1, 0x1.ef35793c76730p-45 },
  .hf_rt2_top = 0x3fe6a09e00000000,
  .one_m_hf_rt2_top = 0x00095f6200000000,
  .bottom_mask = 0xffffffff,
  .one_top = 0x3ff
};

static inline svfloat64_t
sv_log1p_inline (svfloat64_t x, const svbool_t pg)
{
  /* Helper for calculating log(x + 1). Adapted from v_log1p_inline.h, which
     differs from v_log1p_2u5.c by:
     - No special-case handling - this should be dealt with by the caller.
     - Pairwise Horner polynomial evaluation for improved accuracy.
     - Optionally simulate the shortcut for k=0, used in the scalar routine,
       using svsel, for improved accuracy when the argument to log1p is close
     to 0. This feature is enabled by defining WANT_SV_LOG1P_K0_SHORTCUT as 1
     in the source of the caller before including this file.
     See sv_log1p_2u1.c for details of the algorithm.  */
  const struct sv_log1p_data *d = ptr_barrier (&sv_log1p_data);
  svfloat64_t m = svadd_x (pg, x, 1);
  svuint64_t mi = svreinterpret_u64 (m);
  svuint64_t u = svadd_x (pg, mi, d->one_m_hf_rt2_top);

  svint64_t ki
      = svsub_x (pg, svreinterpret_s64 (svlsr_x (pg, u, 52)), d->one_top);
  svfloat64_t k = svcvt_f64_x (pg, ki);

  /* Reduce x to f in [sqrt(2)/2, sqrt(2)].  */
  svuint64_t utop
      = svadd_x (pg, svand_x (pg, u, 0x000fffff00000000), d->hf_rt2_top);
  svuint64_t u_red = svorr_x (pg, utop, svand_x (pg, mi, d->bottom_mask));
  svfloat64_t f = svsub_x (pg, svreinterpret_f64 (u_red), 1);

  /* Correction term c/m.  */
  svfloat64_t c = svsub_x (pg, x, svsub_x (pg, m, 1));
  svfloat64_t cm;

#ifndef WANT_SV_LOG1P_K0_SHORTCUT
# error                                                                       \
      "Cannot use sv_log1p_inline.h without specifying whether you need the k0 shortcut for greater accuracy close to 0"
#elif WANT_SV_LOG1P_K0_SHORTCUT
  /* Shortcut if k is 0 - set correction term to 0 and f to x. The result is
     that the approximation is solely the polynomial.  */
  svbool_t knot0 = svcmpne (pg, k, 0);
  cm = svdiv_z (knot0, c, m);
  if (likely (!svptest_any (pg, knot0)))
    {
      f = svsel (knot0, f, x);
    }
#else
  /* No shortcut.  */
  cm = svdiv_x (pg, c, m);
#endif

  /* Approximate log1p(f) on the reduced input using a polynomial.  */
  svfloat64_t f2 = svmul_x (pg, f, f);
  svfloat64_t p = sv_pw_horner_18_f64_x (pg, f, f2, d->poly);

  /* Assemble log1p(x) = k * log2 + log1p(f) + c/m.  */
  svfloat64_t ylo = svmla_x (pg, cm, k, d->ln2[0]);
  svfloat64_t yhi = svmla_x (pg, f, k, d->ln2[1]);

  return svmla_x (pg, svadd_x (pg, ylo, yhi), f2, p);
}
#endif // MATH_SV_LOG1P_INLINE_H
