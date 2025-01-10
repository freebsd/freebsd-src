/*
 * Single-precision SVE sinpi(x) function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "mathlib.h"
#include "test_sig.h"
#include "test_defs.h"
#include "sv_poly_f32.h"

static const struct data
{
  float poly[6], range_val;
} data = {
  /* Taylor series coefficents for sin(pi * x).  */
  .poly = { 0x1.921fb6p1f, -0x1.4abbcep2f, 0x1.466bc6p1f, -0x1.32d2ccp-1f,
	    0x1.50783p-4f, -0x1.e30750p-8f },
  .range_val = 0x1p31,
};

/* A fast SVE implementation of sinpif.
   Maximum error 2.48 ULP:
   _ZGVsMxv_sinpif(0x1.d062b6p-2) got 0x1.fa8c06p-1
				 want 0x1.fa8c02p-1.  */
svfloat32_t SV_NAME_F1 (sinpi) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* range reduction into -1/2 .. 1/2
     with n = rint(x) and r = r - n.  */
  svfloat32_t n = svrinta_x (pg, x);
  svfloat32_t r = svsub_x (pg, x, n);

  /* Result should be negated based on if n is odd or not.  */
  svbool_t cmp = svaclt (pg, x, d->range_val);
  svuint32_t intn = svreinterpret_u32 (svcvt_s32_z (pg, n));
  svuint32_t sign = svlsl_z (cmp, intn, 31);

  /* y = sin(r).  */
  svfloat32_t r2 = svmul_x (pg, r, r);
  svfloat32_t y = sv_horner_5_f32_x (pg, r2, d->poly);
  y = svmul_x (pg, y, r);

  return svreinterpret_f32 (sveor_x (pg, svreinterpret_u32 (y), sign));
}

#if WANT_TRIGPI_TESTS
TEST_ULP (SV_NAME_F1 (sinpi), 1.99)
TEST_DISABLE_FENV (SV_NAME_F1 (sinpi))
TEST_SYM_INTERVAL (SV_NAME_F1 (sinpi), 0, 0x1p-31, 5000)
TEST_SYM_INTERVAL (SV_NAME_F1 (sinpi), 0x1p-31, 0.5, 10000)
TEST_SYM_INTERVAL (SV_NAME_F1 (sinpi), 0.5, 0x1p22f, 10000)
TEST_SYM_INTERVAL (SV_NAME_F1 (sinpi), 0x1p22f, inf, 10000)
#endif
CLOSE_SVE_ATTR
