/*
 * Single-precision vector erf(x) function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_sig.h"
#include "test_defs.h"

static const struct data
{
  float32x4_t max, shift, third;
#if WANT_SIMD_EXCEPT
  float32x4_t tiny_bound, scale_minus_one;
#endif
} data = {
  .max = V4 (3.9375), /* 4 - 8/128.  */
  .shift = V4 (0x1p16f),
  .third = V4 (0x1.555556p-2f), /* 1/3.  */
#if WANT_SIMD_EXCEPT
  .tiny_bound = V4 (0x1p-62f),
  .scale_minus_one = V4 (0x1.06eba8p-3f), /* scale - 1.0.  */
#endif
};

#define AbsMask 0x7fffffff

struct entry
{
  float32x4_t erf;
  float32x4_t scale;
};

static inline struct entry
lookup (uint32x4_t i)
{
  struct entry e;
  float32x2_t t0 = vld1_f32 (&__v_erff_data.tab[vgetq_lane_u32 (i, 0)].erf);
  float32x2_t t1 = vld1_f32 (&__v_erff_data.tab[vgetq_lane_u32 (i, 1)].erf);
  float32x2_t t2 = vld1_f32 (&__v_erff_data.tab[vgetq_lane_u32 (i, 2)].erf);
  float32x2_t t3 = vld1_f32 (&__v_erff_data.tab[vgetq_lane_u32 (i, 3)].erf);
  float32x4_t e1 = vcombine_f32 (t0, t1);
  float32x4_t e2 = vcombine_f32 (t2, t3);
  e.erf = vuzp1q_f32 (e1, e2);
  e.scale = vuzp2q_f32 (e1, e2);
  return e;
}

/* Single-precision implementation of vector erf(x).
   Approximation based on series expansion near x rounded to
   nearest multiple of 1/128.
   Let d = x - r, and scale = 2 / sqrt(pi) * exp(-r^2). For x near r,

   erf(x) ~ erf(r) + scale * d * [1 - r * d - 1/3 * d^2]

   Values of erf(r) and scale are read from lookup tables.
   For |x| > 3.9375, erf(|x|) rounds to 1.0f.

   Maximum error: 1.93 ULP
     _ZGVnN4v_erff(0x1.c373e6p-9) got 0x1.fd686cp-9
				 want 0x1.fd6868p-9.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (erf) (float32x4_t x)
{
  const struct data *dat = ptr_barrier (&data);

#if WANT_SIMD_EXCEPT
  /* |x| < 2^-62.  */
  uint32x4_t cmp = vcaltq_f32 (x, dat->tiny_bound);
  float32x4_t xm = x;
  /* If any lanes are special, mask them with 1 and retain a copy of x to allow
     special case handler to fix special lanes later. This is only necessary if
     fenv exceptions are to be triggered correctly.  */
  if (unlikely (v_any_u32 (cmp)))
    x = vbslq_f32 (cmp, v_f32 (1), x);
#endif

  float32x4_t a = vabsq_f32 (x);
  uint32x4_t a_gt_max = vcgtq_f32 (a, dat->max);

  /* Lookup erf(r) and scale(r) in tables, e.g. set erf(r) to 0 and scale to
     2/sqrt(pi), when x reduced to r = 0.  */
  float32x4_t shift = dat->shift;
  float32x4_t z = vaddq_f32 (a, shift);

  uint32x4_t i
      = vsubq_u32 (vreinterpretq_u32_f32 (z), vreinterpretq_u32_f32 (shift));
  i = vminq_u32 (i, v_u32 (512));
  struct entry e = lookup (i);

  float32x4_t r = vsubq_f32 (z, shift);

  /* erf(x) ~ erf(r) + scale * d * (1 - r * d - 1/3 * d^2).  */
  float32x4_t d = vsubq_f32 (a, r);
  float32x4_t d2 = vmulq_f32 (d, d);
  float32x4_t y = vfmaq_f32 (r, dat->third, d);
  y = vfmaq_f32 (e.erf, e.scale, vfmsq_f32 (d, d2, y));

  /* Solves the |x| = inf case.  */
  y = vbslq_f32 (a_gt_max, v_f32 (1.0f), y);

  /* Copy sign.  */
  y = vbslq_f32 (v_u32 (AbsMask), y, x);

#if WANT_SIMD_EXCEPT
  if (unlikely (v_any_u32 (cmp)))
    return vbslq_f32 (cmp, vfmaq_f32 (xm, dat->scale_minus_one, xm), y);
#endif
  return y;
}

HALF_WIDTH_ALIAS_F1 (erf)

TEST_SIG (V, F, 1, erf, -4.0, 4.0)
TEST_ULP (V_NAME_F1 (erf), 1.43)
TEST_DISABLE_FENV_IF_NOT (V_NAME_F1 (erf), WANT_SIMD_EXCEPT)
TEST_SYM_INTERVAL (V_NAME_F1 (erf), 0, 3.9375, 40000)
TEST_SYM_INTERVAL (V_NAME_F1 (erf), 3.9375, inf, 40000)
TEST_SYM_INTERVAL (V_NAME_F1 (erf), 0, inf, 40000)
