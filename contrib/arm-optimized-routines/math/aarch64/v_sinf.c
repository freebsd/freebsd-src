/*
 * Single-precision vector sin function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "mathlib.h"
#include "v_math.h"

static const struct data
{
  float32x4_t poly[4];
  float32x4_t range_val, inv_pi, shift, pi_1, pi_2, pi_3;
} data = {
  /* 1.886 ulp error.  */
  .poly = { V4 (-0x1.555548p-3f), V4 (0x1.110df4p-7f), V4 (-0x1.9f42eap-13f),
	    V4 (0x1.5b2e76p-19f) },

  .pi_1 = V4 (0x1.921fb6p+1f),
  .pi_2 = V4 (-0x1.777a5cp-24f),
  .pi_3 = V4 (-0x1.ee59dap-49f),

  .inv_pi = V4 (0x1.45f306p-2f),
  .shift = V4 (0x1.8p+23f),
  .range_val = V4 (0x1p20f)
};

#if WANT_SIMD_EXCEPT
# define TinyBound v_u32 (0x21000000) /* asuint32(0x1p-61f).  */
# define Thresh v_u32 (0x28800000)    /* RangeVal - TinyBound.  */
#endif

#define C(i) d->poly[i]

static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t x, float32x4_t y, uint32x4_t odd, uint32x4_t cmp)
{
  /* Fall back to scalar code.  */
  y = vreinterpretq_f32_u32 (veorq_u32 (vreinterpretq_u32_f32 (y), odd));
  return v_call_f32 (sinf, x, y, cmp);
}

float32x4_t VPCS_ATTR V_NAME_F1 (sin) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);
  float32x4_t n, r, r2, y;
  uint32x4_t odd, cmp;

#if WANT_SIMD_EXCEPT
  uint32x4_t ir = vreinterpretq_u32_f32 (vabsq_f32 (x));
  cmp = vcgeq_u32 (vsubq_u32 (ir, TinyBound), Thresh);
  /* If fenv exceptions are to be triggered correctly, set any special lanes
     to 1 (which is neutral w.r.t. fenv). These lanes will be fixed by
     special-case handler later.  */
  r = vbslq_f32 (cmp, vreinterpretq_f32_u32 (cmp), x);
#else
  r = x;
  cmp = vcageq_f32 (x, d->range_val);
#endif

  /* n = rint(|x|/pi) */
  n = vfmaq_f32 (d->shift, d->inv_pi, r);
  odd = vshlq_n_u32 (vreinterpretq_u32_f32 (n), 31);
  n = vsubq_f32 (n, d->shift);

  /* r = |x| - n*pi  (range reduction into -pi/2 .. pi/2) */
  r = vfmsq_f32 (r, d->pi_1, n);
  r = vfmsq_f32 (r, d->pi_2, n);
  r = vfmsq_f32 (r, d->pi_3, n);

  /* y = sin(r) */
  r2 = vmulq_f32 (r, r);
  y = vfmaq_f32 (C (2), C (3), r2);
  y = vfmaq_f32 (C (1), y, r2);
  y = vfmaq_f32 (C (0), y, r2);
  y = vfmaq_f32 (r, vmulq_f32 (y, r2), r);

  if (unlikely (v_any_u32 (cmp)))
    return special_case (x, y, odd, cmp);
  return vreinterpretq_f32_u32 (veorq_u32 (vreinterpretq_u32_f32 (y), odd));
}
