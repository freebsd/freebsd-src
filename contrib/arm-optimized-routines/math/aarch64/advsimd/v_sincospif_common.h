/*
 * Helper for Single-precision vector sincospi function.
 *
 * Copyright (c) 2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "mathlib.h"
#include "v_math.h"
#include "v_poly_f32.h"

const static struct v_sincospif_data
{
  float32x4_t poly[6], range_val;
} v_sincospif_data = {
  /* Taylor series coefficents for sin(pi * x).  */
  .poly = { V4 (0x1.921fb6p1f), V4 (-0x1.4abbcep2f), V4 (0x1.466bc6p1f),
	    V4 (-0x1.32d2ccp-1f), V4 (0x1.50783p-4f), V4 (-0x1.e30750p-8f) },
  .range_val = V4 (0x1p31f),
};

/* Single-precision vector function allowing calculation of both sinpi and
   cospi in one function call, using shared argument reduction and polynomials.
   Worst-case error for sin is 3.04 ULP:
   _ZGVnN4v_sincospif_sin(0x1.1d341ap-1) got 0x1.f7cd56p-1 want 0x1.f7cd5p-1.
   Worst-case error for cos is 3.18 ULP:
   _ZGVnN4v_sincospif_cos(0x1.d341a8p-5) got 0x1.f7cd56p-1 want 0x1.f7cd5p-1.
 */
static inline float32x4x2_t
v_sincospif_inline (float32x4_t x, const struct v_sincospif_data *d)
{
  /* If r is odd, the sign of the result should be inverted for sinpi and
     reintroduced for cospi.  */
  uint32x4_t cmp = vcgeq_f32 (x, d->range_val);
  uint32x4_t odd = vshlq_n_u32 (
      vbicq_u32 (vreinterpretq_u32_s32 (vcvtaq_s32_f32 (x)), cmp), 31);

  /* r = x - rint(x).  */
  float32x4_t sr = vsubq_f32 (x, vrndaq_f32 (x));
  /* cospi(x) = sinpi(0.5 - abs(x)) for values -1/2 .. 1/2.  */
  float32x4_t cr = vsubq_f32 (v_f32 (0.5f), vabsq_f32 (sr));

  /* Pairwise Horner approximation for y = sin(r * pi).  */
  float32x4_t sr2 = vmulq_f32 (sr, sr);
  float32x4_t sr4 = vmulq_f32 (sr2, sr2);
  float32x4_t cr2 = vmulq_f32 (cr, cr);
  float32x4_t cr4 = vmulq_f32 (cr2, cr2);

  float32x4_t ss = vmulq_f32 (v_pw_horner_5_f32 (sr2, sr4, d->poly), sr);
  float32x4_t cc = vmulq_f32 (v_pw_horner_5_f32 (cr2, cr4, d->poly), cr);

  float32x4_t sinpix
      = vreinterpretq_f32_u32 (veorq_u32 (vreinterpretq_u32_f32 (ss), odd));
  float32x4_t cospix
      = vreinterpretq_f32_u32 (veorq_u32 (vreinterpretq_u32_f32 (cc), odd));

  return (float32x4x2_t){ sinpix, cospix };
}
