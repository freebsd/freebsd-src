/*
 * Helper for single-precision routines which calculate log(1 + x) and do not
 * need special-case handling
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef PL_MATH_V_LOG1PF_INLINE_H
#define PL_MATH_V_LOG1PF_INLINE_H

#include "v_math.h"
#include "math_config.h"

#define Four 0x40800000
#define Ln2 v_f32 (0x1.62e43p-1f)

#define C(i) v_f32 (__log1pf_data.coeffs[i])

static inline v_f32_t
eval_poly (v_f32_t m)
{
  /* Approximate log(1+m) on [-0.25, 0.5] using Estrin scheme.  */
  v_f32_t p_12 = v_fma_f32 (m, C (1), C (0));
  v_f32_t p_34 = v_fma_f32 (m, C (3), C (2));
  v_f32_t p_56 = v_fma_f32 (m, C (5), C (4));
  v_f32_t p_78 = v_fma_f32 (m, C (7), C (6));

  v_f32_t m2 = m * m;
  v_f32_t p_02 = v_fma_f32 (m2, p_12, m);
  v_f32_t p_36 = v_fma_f32 (m2, p_56, p_34);
  v_f32_t p_79 = v_fma_f32 (m2, C (8), p_78);

  v_f32_t m4 = m2 * m2;
  v_f32_t p_06 = v_fma_f32 (m4, p_36, p_02);

  return v_fma_f32 (m4, m4 * p_79, p_06);
}

static inline v_f32_t
log1pf_inline (v_f32_t x)
{
  /* Helper for calculating log(x + 1). Copied from log1pf_2u1.c, with no
     special-case handling. See that file for details of the algorithm.  */
  v_f32_t m = x + 1.0f;
  v_u32_t k = (v_as_u32_f32 (m) - 0x3f400000) & 0xff800000;
  v_f32_t s = v_as_f32_u32 (v_u32 (Four) - k);
  v_f32_t m_scale = v_as_f32_u32 (v_as_u32_f32 (x) - k)
		    + v_fma_f32 (v_f32 (0.25f), s, v_f32 (-1.0f));
  v_f32_t p = eval_poly (m_scale);
  v_f32_t scale_back = v_to_f32_u32 (k) * 0x1.0p-23f;
  return v_fma_f32 (scale_back, Ln2, p);
}

#endif //  PL_MATH_V_LOG1PF_INLINE_H
