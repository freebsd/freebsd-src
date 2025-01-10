/*
 * Single-precision vector 2^x function.
 *
 * Copyright (c) 2019-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_defs.h"
#include "test_sig.h"

static const struct data
{
  float32x4_t c1, c3;
  uint32x4_t exponent_bias, special_offset, special_bias;
#if !WANT_SIMD_EXCEPT
  float32x4_t scale_thresh, special_bound;
#endif
  float c0, c2, c4, zero;
} data = {
  /* maxerr: 1.962 ulp.  */
  .c0 = 0x1.59977ap-10f,
  .c1 = V4 (0x1.3ce9e4p-7f),
  .c2 = 0x1.c6bd32p-5f,
  .c3 = V4 (0x1.ebf9bcp-3f),
  .c4 = 0x1.62e422p-1f,
  .exponent_bias = V4 (0x3f800000),
  .special_offset = V4 (0x82000000),
  .special_bias = V4 (0x7f000000),
#if !WANT_SIMD_EXCEPT
  .special_bound = V4 (126.0f),
  .scale_thresh = V4 (192.0f),
#endif
};

#if WANT_SIMD_EXCEPT

# define TinyBound v_u32 (0x20000000)	  /* asuint (0x1p-63).  */
# define BigBound v_u32 (0x42800000)	  /* asuint (0x1p6).  */
# define SpecialBound v_u32 (0x22800000) /* BigBound - TinyBound.  */

static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t x, float32x4_t y, uint32x4_t cmp)
{
  /* If fenv exceptions are to be triggered correctly, fall back to the scalar
     routine for special lanes.  */
  return v_call_f32 (exp2f, x, y, cmp);
}

#else

static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t poly, float32x4_t n, uint32x4_t e, uint32x4_t cmp1,
	      float32x4_t scale, const struct data *d)
{
  /* 2^n may overflow, break it up into s1*s2.  */
  uint32x4_t b = vandq_u32 (vclezq_f32 (n), d->special_offset);
  float32x4_t s1 = vreinterpretq_f32_u32 (vaddq_u32 (b, d->special_bias));
  float32x4_t s2 = vreinterpretq_f32_u32 (vsubq_u32 (e, b));
  uint32x4_t cmp2 = vcagtq_f32 (n, d->scale_thresh);
  float32x4_t r2 = vmulq_f32 (s1, s1);
  float32x4_t r1 = vmulq_f32 (vfmaq_f32 (s2, poly, s2), s1);
  /* Similar to r1 but avoids double rounding in the subnormal range.  */
  float32x4_t r0 = vfmaq_f32 (scale, poly, scale);
  float32x4_t r = vbslq_f32 (cmp1, r1, r0);
  return vbslq_f32 (cmp2, r2, r);
}

#endif

float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (exp2) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

#if WANT_SIMD_EXCEPT
  /* asuint(|x|) - TinyBound >= BigBound - TinyBound.  */
  uint32x4_t ia = vreinterpretq_u32_f32 (vabsq_f32 (x));
  uint32x4_t cmp = vcgeq_u32 (vsubq_u32 (ia, TinyBound), SpecialBound);
  float32x4_t xm = x;
  /* If any lanes are special, mask them with 1 and retain a copy of x to allow
     special_case to fix special lanes later. This is only necessary if fenv
     exceptions are to be triggered correctly.  */
  if (unlikely (v_any_u32 (cmp)))
    x = vbslq_f32 (cmp, v_f32 (1), x);
#endif

  /* exp2(x) = 2^n (1 + poly(r)), with 1 + poly(r) in [1/sqrt(2),sqrt(2)]
     x = n + r, with r in [-1/2, 1/2].  */
  float32x4_t n = vrndaq_f32 (x);
  float32x4_t r = vsubq_f32 (x, n);
  uint32x4_t e = vshlq_n_u32 (vreinterpretq_u32_s32 (vcvtaq_s32_f32 (x)), 23);
  float32x4_t scale = vreinterpretq_f32_u32 (vaddq_u32 (e, d->exponent_bias));

#if !WANT_SIMD_EXCEPT
  uint32x4_t cmp = vcagtq_f32 (n, d->special_bound);
#endif

  float32x4_t c024 = vld1q_f32 (&d->c0);
  float32x4_t r2 = vmulq_f32 (r, r);
  float32x4_t p = vfmaq_laneq_f32 (d->c1, r, c024, 0);
  float32x4_t q = vfmaq_laneq_f32 (d->c3, r, c024, 1);
  q = vfmaq_f32 (q, p, r2);
  p = vmulq_laneq_f32 (r, c024, 2);
  float32x4_t poly = vfmaq_f32 (p, q, r2);

  if (unlikely (v_any_u32 (cmp)))
#if WANT_SIMD_EXCEPT
    return special_case (xm, vfmaq_f32 (scale, poly, scale), cmp);
#else
    return special_case (poly, n, e, cmp, scale, d);
#endif

  return vfmaq_f32 (scale, poly, scale);
}

HALF_WIDTH_ALIAS_F1 (exp2)

TEST_SIG (V, F, 1, exp2, -9.9, 9.9)
TEST_ULP (V_NAME_F1 (exp2), 1.49)
TEST_DISABLE_FENV_IF_NOT (V_NAME_F1 (exp2), WANT_SIMD_EXCEPT)
TEST_INTERVAL (V_NAME_F1 (exp2), 0, 0xffff0000, 10000)
TEST_SYM_INTERVAL (V_NAME_F1 (exp2), 0x1p-14, 0x1p8, 500000)
