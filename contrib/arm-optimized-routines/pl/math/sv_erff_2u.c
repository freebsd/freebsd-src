/*
 * Single-precision vector erf(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

static const struct data
{
  float min, max, scale, shift, third;
} data = {
  .min = 0x1.cp-7f,	   /* 1/64 - 1/512.  */
  .max = 3.9375,	   /* 4 - 8/128.  */
  .scale = 0x1.20dd76p+0f, /* 2/sqrt(pi).  */
  .shift = 0x1p16f,
  .third = 0x1.555556p-2f, /* 1/3.  */
};

#define SignMask (0x80000000)

/* Single-precision implementation of vector erf(x).
   Approximation based on series expansion near x rounded to
   nearest multiple of 1/128.
   Let d = x - r, and scale = 2 / sqrt(pi) * exp(-r^2). For x near r,

   erf(x) ~ erf(r) + scale * d * [1 - r * d - 1/3 * d^2]

   Values of erf(r) and scale are read from lookup tables.
   For |x| < 0x1.cp-7, the algorithm sets r = 0, erf(r) = 0, and scale = 2 /
   sqrt(pi), so it simply boils down to a Taylor series expansion near 0. For
   |x| > 3.9375, erf(|x|) rounds to 1.0f.

   Maximum error on each interval:
   - [0, 0x1.cp-7]: 1.93 ULP
     _ZGVsMxv_erff(0x1.c373e6p-9) got 0x1.fd686cp-9 want 0x1.fd6868p-9
   - [0x1.cp-7, 4.0]: 1.26 ULP
     _ZGVsMxv_erff(0x1.1d002ep+0) got 0x1.c4eb9ap-1 want 0x1.c4eb98p-1.  */
svfloat32_t SV_NAME_F1 (erf) (svfloat32_t x, const svbool_t pg)
{
  const struct data *dat = ptr_barrier (&data);

  /* |x| > 1/64 - 1/512.  */
  svbool_t a_gt_min = svacgt (pg, x, dat->min);

  /* |x| >= 4.0 - 8/128.  */
  svbool_t a_ge_max = svacge (pg, x, dat->max);
  svfloat32_t a = svabs_x (pg, x);

  svfloat32_t shift = sv_f32 (dat->shift);
  svfloat32_t z = svadd_x (pg, a, shift);
  svuint32_t i
      = svsub_x (pg, svreinterpret_u32 (z), svreinterpret_u32 (shift));

  /* Saturate lookup index.  */
  i = svsel (a_ge_max, sv_u32 (512), i);

  /* r and erf(r) set to 0 for |x| below min.  */
  svfloat32_t r = svsub_z (a_gt_min, z, shift);
  svfloat32_t erfr = svld1_gather_index (a_gt_min, __sv_erff_data.erf, i);

  /* scale set to 2/sqrt(pi) for |x| below min.  */
  svfloat32_t scale = svld1_gather_index (a_gt_min, __sv_erff_data.scale, i);
  scale = svsel (a_gt_min, scale, sv_f32 (dat->scale));

  /* erf(x) ~ erf(r) + scale * d * (1 - r * d + 1/3 * d^2).  */
  svfloat32_t d = svsub_x (pg, a, r);
  svfloat32_t d2 = svmul_x (pg, d, d);
  svfloat32_t y = svmla_x (pg, r, d, dat->third);
  y = svmla_x (pg, erfr, scale, svmls_x (pg, d, d2, y));

  /* Solves the |x| = inf case.  */
  y = svsel (a_ge_max, sv_f32 (1.0f), y);

  /* Copy sign.  */
  svuint32_t ix = svreinterpret_u32 (x);
  svuint32_t iy = svreinterpret_u32 (y);
  svuint32_t sign = svand_x (pg, ix, SignMask);
  return svreinterpret_f32 (svorr_x (pg, sign, iy));
}

PL_SIG (SV, F, 1, erf, -4.0, 4.0)
PL_TEST_ULP (SV_NAME_F1 (erf), 1.43)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (erf), 0, 0x1.cp-7, 40000)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (erf), 0x1.cp-7, 3.9375, 40000)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (erf), 3.9375, inf, 40000)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (erf), 0, inf, 4000)
