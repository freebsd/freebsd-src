/*
 * Helper for single-precision routines which calculate exp(x) - 1 and do not
 * need special-case handling
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef PL_MATH_V_EXPM1F_INLINE_H
#define PL_MATH_V_EXPM1F_INLINE_H

#include "v_math.h"
#include "math_config.h"
#include "estrinf.h"

#define One 0x3f800000
#define Shift v_f32 (0x1.8p23f)
#define InvLn2 v_f32 (0x1.715476p+0f)
#define MLn2hi v_f32 (-0x1.62e4p-1f)
#define MLn2lo v_f32 (-0x1.7f7d1cp-20f)

#define C(i) v_f32 (__expm1f_poly[i])

static inline v_f32_t
expm1f_inline (v_f32_t x)
{
  /* Helper routine for calculating exp(x) - 1.
     Copied from v_expm1f_1u6.c, with all special-case handling removed - the
     calling routine should handle special values if required.  */

  /* Reduce argument: f in [-ln2/2, ln2/2], i is exact.  */
  v_f32_t j = v_fma_f32 (InvLn2, x, Shift) - Shift;
  v_s32_t i = v_to_s32_f32 (j);
  v_f32_t f = v_fma_f32 (j, MLn2hi, x);
  f = v_fma_f32 (j, MLn2lo, f);

  /* Approximate expm1(f) with polynomial P, expm1(f) ~= f + f^2 * P(f).
     Uses Estrin scheme, where the main __v_expm1f routine uses Horner.  */
  v_f32_t f2 = f * f;
  v_f32_t p = ESTRIN_4 (f, f2, f2 * f2, C);
  p = v_fma_f32 (f2, p, f);

  /* t = 2^i.  */
  v_f32_t t = v_as_f32_u32 (v_as_u32_s32 (i << 23) + One);
  /* expm1(x) ~= p * t + (t - 1).  */
  return v_fma_f32 (p, t, t - 1);
}

#endif // PL_MATH_V_EXPM1F_INLINE_H
