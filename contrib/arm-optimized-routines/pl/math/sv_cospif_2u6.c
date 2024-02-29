/*
 * Single-precision SVE cospi(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "mathlib.h"
#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"
#include "poly_sve_f32.h"

static const struct data
{
  float poly[6];
  float range_val;
} data = {
  /* Taylor series coefficents for sin(pi * x).  */
  .poly = { 0x1.921fb6p1f, -0x1.4abbcep2f, 0x1.466bc6p1f, -0x1.32d2ccp-1f,
	    0x1.50783p-4f, -0x1.e30750p-8f },
  .range_val = 0x1p31f,
};

/* A fast SVE implementation of cospif.
   Maximum error: 2.60 ULP:
   _ZGVsMxv_cospif(+/-0x1.cae664p-4) got 0x1.e09c9ep-1
				    want 0x1.e09c98p-1.  */
svfloat32_t SV_NAME_F1 (cospi) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* Using cospi(x) = sinpi(0.5 - x)
     range reduction and offset into sinpi range -1/2 .. 1/2
     r = 0.5 - |x - rint(x)|.  */
  svfloat32_t n = svrinta_x (pg, x);
  svfloat32_t r = svsub_x (pg, x, n);
  r = svsub_x (pg, sv_f32 (0.5f), svabs_x (pg, r));

  /* Result should be negated based on if n is odd or not.
     If ax >= 2^31, the result will always be positive.  */
  svbool_t cmp = svaclt (pg, x, d->range_val);
  svuint32_t intn = svreinterpret_u32 (svcvt_s32_x (pg, n));
  svuint32_t sign = svlsl_z (cmp, intn, 31);

  /* y = sin(r).  */
  svfloat32_t r2 = svmul_x (pg, r, r);
  svfloat32_t y = sv_horner_5_f32_x (pg, r2, d->poly);
  y = svmul_x (pg, y, r);

  return svreinterpret_f32 (sveor_x (pg, svreinterpret_u32 (y), sign));
}

PL_SIG (SV, F, 1, cospi, -0.9, 0.9)
PL_TEST_ULP (SV_NAME_F1 (cospi), 2.08)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (cospi), 0, 0x1p-31, 5000)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (cospi), 0x1p-31, 0.5, 10000)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (cospi), 0.5, 0x1p31f, 10000)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (cospi), 0x1p31f, inf, 10000)
