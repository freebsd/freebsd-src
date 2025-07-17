/*
 * Helper for single-precision SVE sincospi
 *
 * Copyright (c) 2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "sv_poly_f32.h"

const static struct sv_sincospif_data
{
  float c0, c2, c4;
  float c1, c3, c5;
  float range_val;
} sv_sincospif_data = {
  /* Taylor series coefficents for sin(pi * x).  */
  .c0 = 0x1.921fb6p1f,
  .c1 = -0x1.4abbcep2f,
  .c2 = 0x1.466bc6p1f,
  .c3 = -0x1.32d2ccp-1f,
  .c4 = 0x1.50783p-4f,
  .c5 = -0x1.e30750p-8f,
  /* Exclusive upper bound for a signed integer.  */
  .range_val = 0x1p31f,
};

/* Single-precision vector function allowing calculation of both sinpi and
   cospi in one function call, using shared argument reduction and polynomials.
   Worst-case error for sin is 3.04 ULP:
   _ZGVsMxvl4l4_sincospif_sin(0x1.b51b8p-2) got 0x1.f28b5ep-1 want
   0x1.f28b58p-1.
   Worst-case error for cos is 3.18 ULP:
   _ZGVsMxvl4l4_sincospif_cos(0x1.d341a8p-5) got 0x1.f7cd56p-1 want
   0x1.f7cd5p-1.  */
static inline svfloat32x2_t
sv_sincospif_inline (svbool_t pg, svfloat32_t x,
		     const struct sv_sincospif_data *d)
{
  const svbool_t pt = svptrue_b32 ();

  /* r = x - rint(x).  */
  svfloat32_t rx = svrinta_x (pg, x);
  svfloat32_t sr = svsub_x (pt, x, rx);

  /* cospi(x) = sinpi(0.5 - abs(r)) for values -1/2 .. 1/2.  */
  svfloat32_t cr = svsubr_x (pt, svabs_x (pg, sr), 0.5f);

  /* Pairwise Horner approximation for y = sin(r * pi).  */
  svfloat32_t sr2 = svmul_x (pt, sr, sr);
  svfloat32_t sr4 = svmul_x (pt, sr2, sr2);
  svfloat32_t cr2 = svmul_x (pt, cr, cr);
  svfloat32_t cr4 = svmul_x (pt, cr2, cr2);

  /* If rint(x) is odd, the sign of the result should be inverted for sinpi and
     re-introduced for cospi. cmp filters rxs that saturate to max sint.  */
  svbool_t cmp = svaclt (pg, x, d->range_val);
  svuint32_t odd = svlsl_x (pt, svreinterpret_u32 (svcvt_s32_z (pg, rx)), 31);
  sr = svreinterpret_f32 (sveor_x (pt, svreinterpret_u32 (sr), odd));
  cr = svreinterpret_f32 (sveor_m (cmp, svreinterpret_u32 (cr), odd));

  svfloat32_t c135 = svld1rq_f32 (svptrue_b32 (), &d->c1);

  svfloat32_t sp01 = svmla_lane (sv_f32 (d->c0), sr2, c135, 0);
  svfloat32_t sp23 = svmla_lane (sv_f32 (d->c2), sr2, c135, 1);
  svfloat32_t sp45 = svmla_lane (sv_f32 (d->c4), sr2, c135, 2);

  svfloat32_t cp01 = svmla_lane (sv_f32 (d->c0), cr2, c135, 0);
  svfloat32_t cp23 = svmla_lane (sv_f32 (d->c2), cr2, c135, 1);
  svfloat32_t cp45 = svmla_lane (sv_f32 (d->c4), cr2, c135, 2);

  svfloat32_t sp = svmla_x (pg, sp23, sr4, sp45);
  svfloat32_t cp = svmla_x (pg, cp23, cr4, cp45);

  sp = svmla_x (pg, sp01, sr4, sp);
  cp = svmla_x (pg, cp01, cr4, cp);

  svfloat32_t sinpix = svmul_x (pt, sp, sr);
  svfloat32_t cospix = svmul_x (pt, cp, cr);

  return svcreate2 (sinpix, cospix);
}
