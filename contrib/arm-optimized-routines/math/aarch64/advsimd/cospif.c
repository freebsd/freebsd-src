/*
 * Single-precision vector cospi function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "mathlib.h"
#include "v_math.h"
#include "v_poly_f32.h"
#include "test_sig.h"
#include "test_defs.h"

static const struct data
{
  float32x4_t poly[6];
  float32x4_t range_val;
} data = {
  /* Taylor series coefficents for sin(pi * x).  */
  .poly = { V4 (0x1.921fb6p1f), V4 (-0x1.4abbcep2f), V4 (0x1.466bc6p1f),
	    V4 (-0x1.32d2ccp-1f), V4 (0x1.50783p-4f), V4 (-0x1.e30750p-8f) },
  .range_val = V4 (0x1p31f),
};

static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t x, float32x4_t y, uint32x4_t odd, uint32x4_t cmp)
{
  y = vreinterpretq_f32_u32 (veorq_u32 (vreinterpretq_u32_f32 (y), odd));
  return v_call_f32 (arm_math_cospif, x, y, cmp);
}

/* Approximation for vector single-precision cospi(x)
    Maximum Error: 3.17 ULP:
    _ZGVnN4v_cospif(0x1.d341a8p-5) got 0x1.f7cd56p-1
				  want 0x1.f7cd5p-1.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (cospi) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

#if WANT_SIMD_EXCEPT
  float32x4_t r = vabsq_f32 (x);
  uint32x4_t cmp = vcaleq_f32 (v_f32 (0x1p32f), x);

  /* When WANT_SIMD_EXCEPT = 1, special lanes should be zero'd
     to avoid them overflowing and throwing exceptions.  */
  r = v_zerofy_f32 (r, cmp);
  uint32x4_t odd = vshlq_n_u32 (vcvtnq_u32_f32 (r), 31);

#else
  float32x4_t r = x;
  uint32x4_t cmp = vcageq_f32 (r, d->range_val);

  uint32x4_t odd
      = vshlq_n_u32 (vreinterpretq_u32_s32 (vcvtaq_s32_f32 (r)), 31);

#endif

  /* r = x - rint(x).  */
  r = vsubq_f32 (r, vrndaq_f32 (r));

  /* cospi(x) = sinpi(0.5 - abs(x)) for values -1/2 .. 1/2.  */
  r = vsubq_f32 (v_f32 (0.5f), vabsq_f32 (r));

  /* Pairwise Horner approximation for y = sin(r * pi).  */
  float32x4_t r2 = vmulq_f32 (r, r);
  float32x4_t r4 = vmulq_f32 (r2, r2);
  float32x4_t y = vmulq_f32 (v_pw_horner_5_f32 (r2, r4, d->poly), r);

  /* Fallback to scalar.  */
  if (unlikely (v_any_u32 (cmp)))
    return special_case (x, y, odd, cmp);

  /* Reintroduce the sign bit for inputs which round to odd.  */
  return vreinterpretq_f32_u32 (veorq_u32 (vreinterpretq_u32_f32 (y), odd));
}

HALF_WIDTH_ALIAS_F1 (cospi)

#if WANT_TRIGPI_TESTS
TEST_ULP (V_NAME_F1 (cospi), 2.67)
TEST_DISABLE_FENV_IF_NOT (V_NAME_F1 (cospi), WANT_SIMD_EXCEPT)
TEST_SYM_INTERVAL (V_NAME_F1 (cospi), 0, 0x1p-31, 5000)
TEST_SYM_INTERVAL (V_NAME_F1 (cospi), 0x1p-31, 0.5, 10000)
TEST_SYM_INTERVAL (V_NAME_F1 (cospi), 0.5, 0x1p32f, 10000)
TEST_SYM_INTERVAL (V_NAME_F1 (cospi), 0x1p32f, inf, 10000)
#endif
