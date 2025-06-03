/*
 * Double-precision SVE sinh(x) function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "sv_poly_f64.h"
#include "test_sig.h"
#include "test_defs.h"

static const struct data
{
  float64_t poly[11];
  float64_t inv_ln2, m_ln2_hi, m_ln2_lo, shift;
  uint64_t halff;
  int64_t onef;
  uint64_t large_bound;
} data = {
  /* Generated using Remez, deg=12 in [-log(2)/2, log(2)/2].  */
  .poly = { 0x1p-1, 0x1.5555555555559p-3, 0x1.555555555554bp-5,
	    0x1.111111110f663p-7, 0x1.6c16c16c1b5f3p-10,
	    0x1.a01a01affa35dp-13, 0x1.a01a018b4ecbbp-16,
	    0x1.71ddf82db5bb4p-19, 0x1.27e517fc0d54bp-22,
	    0x1.af5eedae67435p-26, 0x1.1f143d060a28ap-29, },

  .inv_ln2 = 0x1.71547652b82fep0,
  .m_ln2_hi = -0x1.62e42fefa39efp-1,
  .m_ln2_lo = -0x1.abc9e3b39803fp-56,
  .shift = 0x1.8p52,

  .halff = 0x3fe0000000000000,
  .onef = 0x3ff0000000000000,
  /* 2^9. expm1 helper overflows for large input.  */
  .large_bound = 0x4080000000000000,
};

static inline svfloat64_t
expm1_inline (svfloat64_t x, svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* Reduce argument:
     exp(x) - 1 = 2^i * (expm1(f) + 1) - 1
     where i = round(x / ln2)
     and   f = x - i * ln2 (f in [-ln2/2, ln2/2]).  */
  svfloat64_t j
      = svsub_x (pg, svmla_x (pg, sv_f64 (d->shift), x, d->inv_ln2), d->shift);
  svint64_t i = svcvt_s64_x (pg, j);
  svfloat64_t f = svmla_x (pg, x, j, d->m_ln2_hi);
  f = svmla_x (pg, f, j, d->m_ln2_lo);
  /* Approximate expm1(f) using polynomial.  */
  svfloat64_t f2 = svmul_x (pg, f, f);
  svfloat64_t f4 = svmul_x (pg, f2, f2);
  svfloat64_t f8 = svmul_x (pg, f4, f4);
  svfloat64_t p
      = svmla_x (pg, f, f2, sv_estrin_10_f64_x (pg, f, f2, f4, f8, d->poly));
  /* t = 2^i.  */
  svfloat64_t t = svscale_x (pg, sv_f64 (1), i);
  /* expm1(x) ~= p * t + (t - 1).  */
  return svmla_x (pg, svsub_x (pg, t, 1.0), p, t);
}

static svfloat64_t NOINLINE
special_case (svfloat64_t x, svbool_t pg)
{
  return sv_call_f64 (sinh, x, x, pg);
}

/* Approximation for SVE double-precision sinh(x) using expm1.
   sinh(x) = (exp(x) - exp(-x)) / 2.
   The greatest observed error is 2.57 ULP:
   _ZGVsMxv_sinh (0x1.a008538399931p-2) got 0x1.ab929fc64bd66p-2
				       want 0x1.ab929fc64bd63p-2.  */
svfloat64_t SV_NAME_D1 (sinh) (svfloat64_t x, svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svfloat64_t ax = svabs_x (pg, x);
  svuint64_t sign
      = sveor_x (pg, svreinterpret_u64 (x), svreinterpret_u64 (ax));
  svfloat64_t halfsign = svreinterpret_f64 (svorr_x (pg, sign, d->halff));

  svbool_t special = svcmpge (pg, svreinterpret_u64 (ax), d->large_bound);

  /* Fall back to scalar variant for all lanes if any are special.  */
  if (unlikely (svptest_any (pg, special)))
    return special_case (x, pg);

  /* Up to the point that expm1 overflows, we can use it to calculate sinh
     using a slight rearrangement of the definition of sinh. This allows us to
     retain acceptable accuracy for very small inputs.  */
  svfloat64_t t = expm1_inline (ax, pg);
  t = svadd_x (pg, t, svdiv_x (pg, t, svadd_x (pg, t, 1.0)));
  return svmul_x (pg, t, halfsign);
}

TEST_SIG (SV, D, 1, sinh, -10.0, 10.0)
TEST_ULP (SV_NAME_D1 (sinh), 2.08)
TEST_DISABLE_FENV (SV_NAME_D1 (sinh))
TEST_SYM_INTERVAL (SV_NAME_D1 (sinh), 0, 0x1p-26, 1000)
TEST_SYM_INTERVAL (SV_NAME_D1 (sinh), 0x1p-26, 0x1p9, 500000)
TEST_SYM_INTERVAL (SV_NAME_D1 (sinh), 0x1p9, inf, 1000)
CLOSE_SVE_ATTR
