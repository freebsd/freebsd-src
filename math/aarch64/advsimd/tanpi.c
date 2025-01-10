/*
 * Double-precision vector tanpi(x) function.
 *
 * Copyright (c) 2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_sig.h"
#include "test_defs.h"

const static struct v_tanpi_data
{
  float64x2_t c0, c2, c4, c6, c8, c10, c12;
  double c1, c3, c5, c7, c9, c11, c13, c14;
} tanpi_data = {
  /* Coefficents for tan(pi * x) computed with fpminimax
     on [ 0x1p-1022 0x1p-2 ]
     approx rel error: 0x1.7eap-55
     approx abs error: 0x1.7eap-55.  */
  .c0 = V2 (0x1.921fb54442d18p1), /* pi.  */
  .c1 = 0x1.4abbce625be52p3,	  .c2 = V2 (0x1.466bc6775b0f9p5),
  .c3 = 0x1.45fff9b426f5ep7,	  .c4 = V2 (0x1.45f4730dbca5cp9),
  .c5 = 0x1.45f3265994f85p11,	  .c6 = V2 (0x1.45f4234b330cap13),
  .c7 = 0x1.45dca11be79ebp15,	  .c8 = V2 (0x1.47283fc5eea69p17),
  .c9 = 0x1.3a6d958cdefaep19,	  .c10 = V2 (0x1.927896baee627p21),
  .c11 = -0x1.89333f6acd922p19,	  .c12 = V2 (0x1.5d4e912bb8456p27),
  .c13 = -0x1.a854d53ab6874p29,	  .c14 = 0x1.1b76de7681424p32,
};

/* Approximation for double-precision vector tanpi(x)
   The maximum error is 3.06 ULP:
   _ZGVnN2v_tanpi(0x1.0a4a07dfcca3ep-1) got -0x1.fa30112702c98p+3
				       want -0x1.fa30112702c95p+3.  */
float64x2_t VPCS_ATTR V_NAME_D1 (tanpi) (float64x2_t x)
{
  const struct v_tanpi_data *d = ptr_barrier (&tanpi_data);

  float64x2_t n = vrndnq_f64 (x);

  /* inf produces nan that propagates.  */
  float64x2_t xr = vsubq_f64 (x, n);
  float64x2_t ar = vabdq_f64 (x, n);
  uint64x2_t flip = vcgtq_f64 (ar, v_f64 (0.25));
  float64x2_t r = vbslq_f64 (flip, vsubq_f64 (v_f64 (0.5), ar), ar);

  /* Order-14 pairwise Horner.  */
  float64x2_t r2 = vmulq_f64 (r, r);
  float64x2_t r4 = vmulq_f64 (r2, r2);

  float64x2_t c_1_3 = vld1q_f64 (&d->c1);
  float64x2_t c_5_7 = vld1q_f64 (&d->c5);
  float64x2_t c_9_11 = vld1q_f64 (&d->c9);
  float64x2_t c_13_14 = vld1q_f64 (&d->c13);
  float64x2_t p01 = vfmaq_laneq_f64 (d->c0, r2, c_1_3, 0);
  float64x2_t p23 = vfmaq_laneq_f64 (d->c2, r2, c_1_3, 1);
  float64x2_t p45 = vfmaq_laneq_f64 (d->c4, r2, c_5_7, 0);
  float64x2_t p67 = vfmaq_laneq_f64 (d->c6, r2, c_5_7, 1);
  float64x2_t p89 = vfmaq_laneq_f64 (d->c8, r2, c_9_11, 0);
  float64x2_t p1011 = vfmaq_laneq_f64 (d->c10, r2, c_9_11, 1);
  float64x2_t p1213 = vfmaq_laneq_f64 (d->c12, r2, c_13_14, 0);

  float64x2_t p = vfmaq_laneq_f64 (p1213, r4, c_13_14, 1);
  p = vfmaq_f64 (p1011, r4, p);
  p = vfmaq_f64 (p89, r4, p);
  p = vfmaq_f64 (p67, r4, p);
  p = vfmaq_f64 (p45, r4, p);
  p = vfmaq_f64 (p23, r4, p);
  p = vfmaq_f64 (p01, r4, p);
  p = vmulq_f64 (r, p);

  float64x2_t p_recip = vdivq_f64 (v_f64 (1.0), p);
  float64x2_t y = vbslq_f64 (flip, p_recip, p);

  uint64x2_t sign
      = veorq_u64 (vreinterpretq_u64_f64 (xr), vreinterpretq_u64_f64 (ar));
  return vreinterpretq_f64_u64 (vorrq_u64 (vreinterpretq_u64_f64 (y), sign));
}

#if WANT_TRIGPI_TESTS
TEST_DISABLE_FENV (V_NAME_D1 (tanpi))
TEST_ULP (V_NAME_D1 (tanpi), 2.57)
TEST_SYM_INTERVAL (V_NAME_D1 (tanpi), 0, 0x1p-31, 50000)
TEST_SYM_INTERVAL (V_NAME_D1 (tanpi), 0x1p-31, 0.5, 50000)
TEST_SYM_INTERVAL (V_NAME_D1 (tanpi), 0.5, 1.0, 200000)
TEST_SYM_INTERVAL (V_NAME_D1 (tanpi), 1.0, 0x1p23, 50000)
TEST_SYM_INTERVAL (V_NAME_D1 (tanpi), 0x1p23, inf, 50000)
#endif
