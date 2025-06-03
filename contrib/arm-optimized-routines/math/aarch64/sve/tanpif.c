/*
 * Single-precision vector tanpif(x) function.
 *
 * Copyright (c) 2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_defs.h"
#include "test_sig.h"

const static struct v_tanpif_data
{
  float c0, c2, c4, c6;
  float c1, c3, c5, c7;
} tanpif_data = {
  /* Coefficients for tan(pi * x).  */
  .c0 = 0x1.921fb4p1f,	.c1 = 0x1.4abbcep3f,  .c2 = 0x1.466b8p5f,
  .c3 = 0x1.461c72p7f,	.c4 = 0x1.42e9d4p9f,  .c5 = 0x1.69e2c4p11f,
  .c6 = 0x1.e85558p11f, .c7 = 0x1.a52e08p16f,
};

/* Approximation for single-precision vector tanpif(x)
   The maximum error is 3.34 ULP:
   _ZGVsMxv_tanpif(0x1.d6c09ap-2) got 0x1.f70aacp+2
				 want 0x1.f70aa6p+2.  */
svfloat32_t SV_NAME_F1 (tanpi) (svfloat32_t x, const svbool_t pg)
{
  const struct v_tanpif_data *d = ptr_barrier (&tanpif_data);
  svfloat32_t odd_coeffs = svld1rq (pg, &d->c1);
  svfloat32_t n = svrintn_x (pg, x);

  /* inf produces nan that propagates.  */
  svfloat32_t xr = svsub_x (pg, x, n);
  svfloat32_t ar = svabd_x (pg, x, n);
  svbool_t flip = svcmpgt (pg, ar, 0.25f);
  svfloat32_t r = svsel (flip, svsub_x (pg, sv_f32 (0.5f), ar), ar);

  svfloat32_t r2 = svmul_x (pg, r, r);
  svfloat32_t r4 = svmul_x (pg, r2, r2);

  /* Order-7 Pairwise Horner.  */
  svfloat32_t p01 = svmla_lane (sv_f32 (d->c0), r2, odd_coeffs, 0);
  svfloat32_t p23 = svmla_lane (sv_f32 (d->c2), r2, odd_coeffs, 1);
  svfloat32_t p45 = svmla_lane (sv_f32 (d->c4), r2, odd_coeffs, 2);
  svfloat32_t p67 = svmla_lane (sv_f32 (d->c6), r2, odd_coeffs, 3);
  svfloat32_t p = svmad_x (pg, p67, r4, p45);
  p = svmad_x (pg, p, r4, p23);
  p = svmad_x (pg, p, r4, p01);
  svfloat32_t poly = svmul_x (pg, r, p);

  svfloat32_t poly_recip = svdiv_x (pg, sv_f32 (1.0), poly);
  svfloat32_t y = svsel (flip, poly_recip, poly);

  svuint32_t sign
      = sveor_x (pg, svreinterpret_u32 (xr), svreinterpret_u32 (ar));
  return svreinterpret_f32 (svorr_x (pg, svreinterpret_u32 (y), sign));
}

#if WANT_TRIGPI_TESTS
TEST_DISABLE_FENV (SV_NAME_F1 (tanpi))
TEST_ULP (SV_NAME_F1 (tanpi), 2.84)
TEST_SYM_INTERVAL (SV_NAME_F1 (tanpi), 0, 0x1p-31, 50000)
TEST_SYM_INTERVAL (SV_NAME_F1 (tanpi), 0x1p-31, 0.5, 100000)
TEST_SYM_INTERVAL (SV_NAME_F1 (tanpi), 0.5, 0x1p23f, 100000)
TEST_SYM_INTERVAL (SV_NAME_F1 (tanpi), 0x1p23f, inf, 100000)
#endif
CLOSE_SVE_ATTR
