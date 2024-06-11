/*
 * Single-precision vector log(x + 1) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"
#include "poly_sve_f32.h"

static const struct data
{
  float poly[8];
  float ln2, exp_bias;
  uint32_t four, three_quarters;
} data = {.poly = {/* Do not store first term of polynomial, which is -0.5, as
                      this can be fmov-ed directly instead of including it in
                      the main load-and-mla polynomial schedule.  */
		   0x1.5555aap-2f, -0x1.000038p-2f, 0x1.99675cp-3f,
		   -0x1.54ef78p-3f, 0x1.28a1f4p-3f, -0x1.0da91p-3f,
		   0x1.abcb6p-4f, -0x1.6f0d5ep-5f},
	  .ln2 = 0x1.62e43p-1f,
	  .exp_bias = 0x1p-23f,
	  .four = 0x40800000,
	  .three_quarters = 0x3f400000};

#define SignExponentMask 0xff800000

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svfloat32_t y, svbool_t special)
{
  return sv_call_f32 (log1pf, x, y, special);
}

/* Vector log1pf approximation using polynomial on reduced interval. Worst-case
   error is 1.27 ULP very close to 0.5.
   _ZGVsMxv_log1pf(0x1.fffffep-2) got 0x1.9f324p-2
				 want 0x1.9f323ep-2.  */
svfloat32_t SV_NAME_F1 (log1p) (svfloat32_t x, svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);
  /* x < -1, Inf/Nan.  */
  svbool_t special = svcmpeq (pg, svreinterpret_u32 (x), 0x7f800000);
  special = svorn_z (pg, special, svcmpge (pg, x, -1));

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
  m_scale = svadd_x (pg, m_scale, svmla_x (pg, sv_f32 (-1), s, 0.25));

  /* Evaluate polynomial on reduced interval.  */
  svfloat32_t ms2 = svmul_x (pg, m_scale, m_scale),
	      ms4 = svmul_x (pg, ms2, ms2);
  svfloat32_t p = sv_estrin_7_f32_x (pg, m_scale, ms2, ms4, d->poly);
  p = svmad_x (pg, m_scale, p, -0.5);
  p = svmla_x (pg, m_scale, m_scale, svmul_x (pg, m_scale, p));

  /* The scale factor to be applied back at the end - by multiplying float(k)
     by 2^-23 we get the unbiased exponent of k.  */
  svfloat32_t scale_back = svmul_x (pg, svcvt_f32_x (pg, k), d->exp_bias);

  /* Apply the scaling back.  */
  svfloat32_t y = svmla_x (pg, p, scale_back, d->ln2);

  if (unlikely (svptest_any (pg, special)))
    return special_case (x, y, special);

  return y;
}

PL_SIG (SV, F, 1, log1p, -0.9, 10.0)
PL_TEST_ULP (SV_NAME_F1 (log1p), 0.77)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (log1p), 0, 0x1p-23, 5000)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (log1p), 0x1p-23, 1, 5000)
PL_TEST_INTERVAL (SV_NAME_F1 (log1p), 1, inf, 10000)
PL_TEST_INTERVAL (SV_NAME_F1 (log1p), -1, -inf, 10)
