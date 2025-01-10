/*
 * Double-precision vector tanpi(x) function.
 *
 * Copyright (c) 2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"

const static struct v_tanpi_data
{
  double c0, c2, c4, c6, c8, c10, c12;
  double c1, c3, c5, c7, c9, c11, c13, c14;
} tanpi_data = {
  /* Coefficents for tan(pi * x) computed with fpminimax
     on [ 0x1p-1022 0x1p-2 ]
     approx rel error: 0x1.7eap-55
     approx abs error: 0x1.7eap-55.  */
  .c0 = 0x1.921fb54442d18p1, /* pi.  */
  .c1 = 0x1.4abbce625be52p3,	.c2 = 0x1.466bc6775b0f9p5,
  .c3 = 0x1.45fff9b426f5ep7,	.c4 = 0x1.45f4730dbca5cp9,
  .c5 = 0x1.45f3265994f85p11,	.c6 = 0x1.45f4234b330cap13,
  .c7 = 0x1.45dca11be79ebp15,	.c8 = 0x1.47283fc5eea69p17,
  .c9 = 0x1.3a6d958cdefaep19,	.c10 = 0x1.927896baee627p21,
  .c11 = -0x1.89333f6acd922p19, .c12 = 0x1.5d4e912bb8456p27,
  .c13 = -0x1.a854d53ab6874p29, .c14 = 0x1.1b76de7681424p32,
};

/* Approximation for double-precision vector tanpi(x)
   The maximum error is 3.06 ULP:
   _ZGVsMxv_tanpi(0x1.0a4a07dfcca3ep-1) got -0x1.fa30112702c98p+3
				       want -0x1.fa30112702c95p+3.  */
svfloat64_t SV_NAME_D1 (tanpi) (svfloat64_t x, const svbool_t pg)
{
  const struct v_tanpi_data *d = ptr_barrier (&tanpi_data);

  svfloat64_t n = svrintn_x (pg, x);

  /* inf produces nan that propagates.  */
  svfloat64_t xr = svsub_x (pg, x, n);
  svfloat64_t ar = svabd_x (pg, x, n);
  svbool_t flip = svcmpgt (pg, ar, 0.25);
  svfloat64_t r = svsel (flip, svsubr_x (pg, ar, 0.5), ar);

  /* Order-14 pairwise Horner.  */
  svfloat64_t r2 = svmul_x (pg, r, r);
  svfloat64_t r4 = svmul_x (pg, r2, r2);

  svfloat64_t c_1_3 = svld1rq (pg, &d->c1);
  svfloat64_t c_5_7 = svld1rq (pg, &d->c5);
  svfloat64_t c_9_11 = svld1rq (pg, &d->c9);
  svfloat64_t c_13_14 = svld1rq (pg, &d->c13);
  svfloat64_t p01 = svmla_lane (sv_f64 (d->c0), r2, c_1_3, 0);
  svfloat64_t p23 = svmla_lane (sv_f64 (d->c2), r2, c_1_3, 1);
  svfloat64_t p45 = svmla_lane (sv_f64 (d->c4), r2, c_5_7, 0);
  svfloat64_t p67 = svmla_lane (sv_f64 (d->c6), r2, c_5_7, 1);
  svfloat64_t p89 = svmla_lane (sv_f64 (d->c8), r2, c_9_11, 0);
  svfloat64_t p1011 = svmla_lane (sv_f64 (d->c10), r2, c_9_11, 1);
  svfloat64_t p1213 = svmla_lane (sv_f64 (d->c12), r2, c_13_14, 0);

  svfloat64_t p = svmla_lane (p1213, r4, c_13_14, 1);
  p = svmad_x (pg, p, r4, p1011);
  p = svmad_x (pg, p, r4, p89);
  p = svmad_x (pg, p, r4, p67);
  p = svmad_x (pg, p, r4, p45);
  p = svmad_x (pg, p, r4, p23);
  p = svmad_x (pg, p, r4, p01);
  p = svmul_x (pg, r, p);

  svfloat64_t p_recip = svdivr_x (pg, p, 1.0);
  svfloat64_t y = svsel (flip, p_recip, p);

  svuint64_t sign
      = sveor_x (pg, svreinterpret_u64 (xr), svreinterpret_u64 (ar));
  return svreinterpret_f64 (svorr_x (pg, svreinterpret_u64 (y), sign));
}

#if WANT_TRIGPI_TESTS
TEST_DISABLE_FENV (SV_NAME_D1 (tanpi))
TEST_ULP (SV_NAME_D1 (tanpi), 2.57)
TEST_SYM_INTERVAL (SV_NAME_D1 (tanpi), 0, 0x1p-31, 50000)
TEST_SYM_INTERVAL (SV_NAME_D1 (tanpi), 0x1p-31, 0.5, 50000)
TEST_SYM_INTERVAL (SV_NAME_D1 (tanpi), 0.5, 1.0, 200000)
TEST_SYM_INTERVAL (SV_NAME_D1 (tanpi), 1.0, 0x1p23, 50000)
TEST_SYM_INTERVAL (SV_NAME_D1 (tanpi), 0x1p23, inf, 50000)
#endif
CLOSE_SVE_ATTR
