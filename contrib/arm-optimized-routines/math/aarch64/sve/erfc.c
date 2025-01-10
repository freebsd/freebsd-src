/*
 * Double-precision vector erfc(x) function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"

static const struct data
{
  uint64_t off_idx, off_arr;
  double max, shift;
  double p20, p40, p41, p42;
  double p51, p52;
  double q5, r5;
  double q6, r6;
  double q7, r7;
  double q8, r8;
  double q9, r9;
  uint64_t table_scale;
} data = {
  /* Set an offset so the range of the index used for lookup is 3487, and it
     can be clamped using a saturated add on an offset index.
     Index offset is 0xffffffffffffffff - asuint64(shift) - 3487.  */
  .off_idx = 0xbd3ffffffffff260,
  .off_arr = 0xfffffffffffff260, /* 0xffffffffffffffff - 3487.  */
  .max = 0x1.b3ep+4,		 /* 3487/128.  */
  .shift = 0x1p45,
  .table_scale = 0x37f0000000000000, /* asuint64(0x1p-128).  */
  .p20 = 0x1.5555555555555p-2,	     /* 1/3, used to compute 2/3 and 1/6.  */
  .p40 = -0x1.999999999999ap-4,	     /* 1/10.  */
  .p41 = -0x1.999999999999ap-2,	     /* 2/5.  */
  .p42 = 0x1.1111111111111p-3,	     /* 2/15.  */
  .p51 = -0x1.c71c71c71c71cp-3,	     /* 2/9.  */
  .p52 = 0x1.6c16c16c16c17p-5,	     /* 2/45.  */
  /* Qi = (i+1) / i, for i = 5, ..., 9.  */
  .q5 = 0x1.3333333333333p0,
  .q6 = 0x1.2aaaaaaaaaaabp0,
  .q7 = 0x1.2492492492492p0,
  .q8 = 0x1.2p0,
  .q9 = 0x1.1c71c71c71c72p0,
  /* Ri = -2 * i / ((i+1)*(i+2)), for i = 5, ..., 9.  */
  .r5 = -0x1.e79e79e79e79ep-3,
  .r6 = -0x1.b6db6db6db6dbp-3,
  .r7 = -0x1.8e38e38e38e39p-3,
  .r8 = -0x1.6c16c16c16c17p-3,
  .r9 = -0x1.4f2094f2094f2p-3,
};

/* Optimized double-precision vector erfc(x).
   Approximation based on series expansion near x rounded to
   nearest multiple of 1/128.
   Let d = x - r, and scale = 2 / sqrt(pi) * exp(-r^2). For x near r,

   erfc(x) ~ erfc(r) - scale * d * poly(r, d), with

   poly(r, d) = 1 - r d + (2/3 r^2 - 1/3) d^2 - r (1/3 r^2 - 1/2) d^3
		+ (2/15 r^4 - 2/5 r^2 + 1/10) d^4
		- r * (2/45 r^4 - 2/9 r^2 + 1/6) d^5
		+ p6(r) d^6 + ... + p10(r) d^10

   Polynomials p6(r) to p10(r) are computed using recurrence relation

   2(i+1)p_i + 2r(i+2)p_{i+1} + (i+2)(i+3)p_{i+2} = 0,
   with p0 = 1, and p1(r) = -r.

   Values of erfc(r) and scale are read from lookup tables. Stored values
   are scaled to avoid hitting the subnormal range.

   Note that for x < 0, erfc(x) = 2.0 - erfc(-x).

   Maximum measured error: 1.71 ULP
   _ZGVsMxv_erfc(0x1.46cfe976733p+4) got 0x1.e15fcbea3e7afp-608
				    want 0x1.e15fcbea3e7adp-608.  */
svfloat64_t SV_NAME_D1 (erfc) (svfloat64_t x, const svbool_t pg)
{
  const struct data *dat = ptr_barrier (&data);

  svfloat64_t a = svabs_x (pg, x);

  /* Clamp input at |x| <= 3487/128.  */
  a = svmin_x (pg, a, dat->max);

  /* Reduce x to the nearest multiple of 1/128.  */
  svfloat64_t shift = sv_f64 (dat->shift);
  svfloat64_t z = svadd_x (pg, a, shift);

  /* Saturate index for the NaN case.  */
  svuint64_t i = svqadd (svreinterpret_u64 (z), dat->off_idx);

  /* Lookup erfc(r) and 2/sqrt(pi)*exp(-r^2) in tables.  */
  i = svadd_x (pg, i, i);
  const float64_t *p = &__v_erfc_data.tab[0].erfc - 2 * dat->off_arr;
  svfloat64_t erfcr = svld1_gather_index (pg, p, i);
  svfloat64_t scale = svld1_gather_index (pg, p + 1, i);

  /* erfc(x) ~ erfc(r) - scale * d * poly(r, d).  */
  svfloat64_t r = svsub_x (pg, z, shift);
  svfloat64_t d = svsub_x (pg, a, r);
  svfloat64_t d2 = svmul_x (pg, d, d);
  svfloat64_t r2 = svmul_x (pg, r, r);

  /* poly (d, r) = 1 + p1(r) * d + p2(r) * d^2 + ... + p9(r) * d^9.  */
  svfloat64_t p1 = r;
  svfloat64_t third = sv_f64 (dat->p20);
  svfloat64_t twothird = svmul_x (pg, third, 2.0);
  svfloat64_t sixth = svmul_x (pg, third, 0.5);
  svfloat64_t p2 = svmls_x (pg, third, r2, twothird);
  svfloat64_t p3 = svmad_x (pg, r2, third, -0.5);
  p3 = svmul_x (pg, r, p3);
  svfloat64_t p4 = svmla_x (pg, sv_f64 (dat->p41), r2, dat->p42);
  p4 = svmls_x (pg, sv_f64 (dat->p40), r2, p4);
  svfloat64_t p5 = svmla_x (pg, sv_f64 (dat->p51), r2, dat->p52);
  p5 = svmla_x (pg, sixth, r2, p5);
  p5 = svmul_x (pg, r, p5);
  /* Compute p_i using recurrence relation:
     p_{i+2} = (p_i + r * Q_{i+1} * p_{i+1}) * R_{i+1}.  */
  svfloat64_t qr5 = svld1rq (svptrue_b64 (), &dat->q5);
  svfloat64_t qr6 = svld1rq (svptrue_b64 (), &dat->q6);
  svfloat64_t qr7 = svld1rq (svptrue_b64 (), &dat->q7);
  svfloat64_t qr8 = svld1rq (svptrue_b64 (), &dat->q8);
  svfloat64_t qr9 = svld1rq (svptrue_b64 (), &dat->q9);
  svfloat64_t p6 = svmla_x (pg, p4, p5, svmul_lane (r, qr5, 0));
  p6 = svmul_lane (p6, qr5, 1);
  svfloat64_t p7 = svmla_x (pg, p5, p6, svmul_lane (r, qr6, 0));
  p7 = svmul_lane (p7, qr6, 1);
  svfloat64_t p8 = svmla_x (pg, p6, p7, svmul_lane (r, qr7, 0));
  p8 = svmul_lane (p8, qr7, 1);
  svfloat64_t p9 = svmla_x (pg, p7, p8, svmul_lane (r, qr8, 0));
  p9 = svmul_lane (p9, qr8, 1);
  svfloat64_t p10 = svmla_x (pg, p8, p9, svmul_lane (r, qr9, 0));
  p10 = svmul_lane (p10, qr9, 1);
  /* Compute polynomial in d using pairwise Horner scheme.  */
  svfloat64_t p90 = svmla_x (pg, p9, d, p10);
  svfloat64_t p78 = svmla_x (pg, p7, d, p8);
  svfloat64_t p56 = svmla_x (pg, p5, d, p6);
  svfloat64_t p34 = svmla_x (pg, p3, d, p4);
  svfloat64_t p12 = svmla_x (pg, p1, d, p2);
  svfloat64_t y = svmla_x (pg, p78, d2, p90);
  y = svmla_x (pg, p56, d2, y);
  y = svmla_x (pg, p34, d2, y);
  y = svmla_x (pg, p12, d2, y);

  y = svmls_x (pg, erfcr, scale, svmls_x (pg, d, d2, y));

  /* Offset equals 2.0 if sign, else 0.0.  */
  svuint64_t sign = svand_x (pg, svreinterpret_u64 (x), 0x8000000000000000);
  svfloat64_t off = svreinterpret_f64 (svlsr_x (pg, sign, 1));
  /* Handle sign and scale back in a single fma.  */
  svfloat64_t fac = svreinterpret_f64 (svorr_x (pg, sign, dat->table_scale));

  return svmla_x (pg, off, fac, y);
}

TEST_SIG (SV, D, 1, erfc, -6.0, 28.0)
TEST_ULP (SV_NAME_D1 (erfc), 1.21)
TEST_DISABLE_FENV (SV_NAME_D1 (erfc))
TEST_SYM_INTERVAL (SV_NAME_D1 (erfc), 0.0, 0x1p-26, 40000)
TEST_INTERVAL (SV_NAME_D1 (erfc), 0x1p-26, 28.0, 40000)
TEST_INTERVAL (SV_NAME_D1 (erfc), -0x1p-26, -6.0, 40000)
TEST_INTERVAL (SV_NAME_D1 (erfc), 28.0, inf, 40000)
TEST_INTERVAL (SV_NAME_D1 (erfc), 6.0, -inf, 40000)
CLOSE_SVE_ATTR
