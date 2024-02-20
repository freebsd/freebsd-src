/*
 * Double-precision SVE cosh(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

static const struct data
{
  float64_t poly[3];
  float64_t inv_ln2, ln2_hi, ln2_lo, shift, thres;
  uint64_t index_mask, special_bound;
} data = {
  .poly = { 0x1.fffffffffffd4p-2, 0x1.5555571d6b68cp-3,
	    0x1.5555576a59599p-5, },

  .inv_ln2 = 0x1.71547652b82fep8, /* N/ln2.  */
  /* -ln2/N.  */
  .ln2_hi = -0x1.62e42fefa39efp-9,
  .ln2_lo = -0x1.abc9e3b39803f3p-64,
  .shift = 0x1.8p+52,
  .thres = 704.0,

  .index_mask = 0xff,
  /* 0x1.6p9, above which exp overflows.  */
  .special_bound = 0x4086000000000000,
};

static svfloat64_t NOINLINE
special_case (svfloat64_t x, svfloat64_t y, svbool_t special)
{
  return sv_call_f64 (cosh, x, y, special);
}

/* Helper for approximating exp(x). Copied from sv_exp_tail, with no
   special-case handling or tail.  */
static inline svfloat64_t
exp_inline (svfloat64_t x, const svbool_t pg, const struct data *d)
{
  /* Calculate exp(x).  */
  svfloat64_t z = svmla_x (pg, sv_f64 (d->shift), x, d->inv_ln2);
  svfloat64_t n = svsub_x (pg, z, d->shift);

  svfloat64_t r = svmla_x (pg, x, n, d->ln2_hi);
  r = svmla_x (pg, r, n, d->ln2_lo);

  svuint64_t u = svreinterpret_u64 (z);
  svuint64_t e = svlsl_x (pg, u, 52 - V_EXP_TAIL_TABLE_BITS);
  svuint64_t i = svand_x (pg, u, d->index_mask);

  svfloat64_t y = svmla_x (pg, sv_f64 (d->poly[1]), r, d->poly[2]);
  y = svmla_x (pg, sv_f64 (d->poly[0]), r, y);
  y = svmla_x (pg, sv_f64 (1.0), r, y);
  y = svmul_x (pg, r, y);

  /* s = 2^(n/N).  */
  u = svld1_gather_index (pg, __v_exp_tail_data, i);
  svfloat64_t s = svreinterpret_f64 (svadd_x (pg, u, e));

  return svmla_x (pg, s, s, y);
}

/* Approximation for SVE double-precision cosh(x) using exp_inline.
   cosh(x) = (exp(x) + exp(-x)) / 2.
   The greatest observed error is in the scalar fall-back region, so is the
   same as the scalar routine, 1.93 ULP:
   _ZGVsMxv_cosh (0x1.628ad45039d2fp+9) got 0x1.fd774e958236dp+1021
				       want 0x1.fd774e958236fp+1021.

   The greatest observed error in the non-special region is 1.54 ULP:
   _ZGVsMxv_cosh (0x1.ba5651dd4486bp+2) got 0x1.f5e2bb8d5c98fp+8
				       want 0x1.f5e2bb8d5c991p+8.  */
svfloat64_t SV_NAME_D1 (cosh) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svfloat64_t ax = svabs_x (pg, x);
  svbool_t special = svcmpgt (pg, svreinterpret_u64 (ax), d->special_bound);

  /* Up to the point that exp overflows, we can use it to calculate cosh by
     exp(|x|) / 2 + 1 / (2 * exp(|x|)).  */
  svfloat64_t t = exp_inline (ax, pg, d);
  svfloat64_t half_t = svmul_x (pg, t, 0.5);
  svfloat64_t half_over_t = svdivr_x (pg, t, 0.5);

  /* Fall back to scalar for any special cases.  */
  if (unlikely (svptest_any (pg, special)))
    return special_case (x, svadd_x (pg, half_t, half_over_t), special);

  return svadd_x (pg, half_t, half_over_t);
}

PL_SIG (SV, D, 1, cosh, -10.0, 10.0)
PL_TEST_ULP (SV_NAME_D1 (cosh), 1.43)
PL_TEST_SYM_INTERVAL (SV_NAME_D1 (cosh), 0, 0x1.6p9, 100000)
PL_TEST_SYM_INTERVAL (SV_NAME_D1 (cosh), 0x1.6p9, inf, 1000)
