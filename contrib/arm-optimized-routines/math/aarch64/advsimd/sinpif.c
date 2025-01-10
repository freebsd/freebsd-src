/*
 * Single-precision vector sinpi function.
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
} data = {
  /* Taylor series coefficents for sin(pi * x).  */
  .poly = { V4 (0x1.921fb6p1f), V4 (-0x1.4abbcep2f), V4 (0x1.466bc6p1f),
	    V4 (-0x1.32d2ccp-1f), V4 (0x1.50783p-4f), V4 (-0x1.e30750p-8f) },
};

#if WANT_SIMD_EXCEPT
# define TinyBound v_u32 (0x30000000) /* asuint32(0x1p-31f).  */
# define Thresh v_u32 (0x1f000000)    /* asuint32(0x1p31f) - TinyBound.  */

static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t x, float32x4_t y, uint32x4_t odd, uint32x4_t cmp)
{
  /* Fall back to scalar code.  */
  y = vreinterpretq_f32_u32 (veorq_u32 (vreinterpretq_u32_f32 (y), odd));
  return v_call_f32 (arm_math_sinpif, x, y, cmp);
}
#endif

/* Approximation for vector single-precision sinpi(x)
    Maximum Error 3.03 ULP:
    _ZGVnN4v_sinpif(0x1.c597ccp-2) got 0x1.f7cd56p-1
				  want 0x1.f7cd5p-1.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (sinpi) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

#if WANT_SIMD_EXCEPT
  uint32x4_t ir = vreinterpretq_u32_f32 (vabsq_f32 (x));
  uint32x4_t cmp = vcgeq_u32 (vsubq_u32 (ir, TinyBound), Thresh);

  /* When WANT_SIMD_EXCEPT = 1, special lanes should be set to 0
     to avoid them under/overflowing and throwing exceptions.  */
  float32x4_t r = v_zerofy_f32 (x, cmp);
#else
  float32x4_t r = x;
#endif

  /* If r is odd, the sign of the result should be inverted.  */
  uint32x4_t odd
      = vshlq_n_u32 (vreinterpretq_u32_s32 (vcvtaq_s32_f32 (r)), 31);

  /* r = x - rint(x). Range reduction to -1/2 .. 1/2.  */
  r = vsubq_f32 (r, vrndaq_f32 (r));

  /* Pairwise Horner approximation for y = sin(r * pi).  */
  float32x4_t r2 = vmulq_f32 (r, r);
  float32x4_t r4 = vmulq_f32 (r2, r2);
  float32x4_t y = vmulq_f32 (v_pw_horner_5_f32 (r2, r4, d->poly), r);

#if WANT_SIMD_EXCEPT
  if (unlikely (v_any_u32 (cmp)))
    return special_case (x, y, odd, cmp);
#endif

  return vreinterpretq_f32_u32 (veorq_u32 (vreinterpretq_u32_f32 (y), odd));
}

HALF_WIDTH_ALIAS_F1 (sinpi)

#if WANT_TRIGPI_TESTS
TEST_ULP (V_NAME_F1 (sinpi), 2.54)
TEST_DISABLE_FENV_IF_NOT (V_NAME_F1 (sinpi), WANT_SIMD_EXCEPT)
TEST_SYM_INTERVAL (V_NAME_F1 (sinpi), 0, 0x1p-31, 5000)
TEST_SYM_INTERVAL (V_NAME_F1 (sinpi), 0x1p-31, 0.5, 10000)
TEST_SYM_INTERVAL (V_NAME_F1 (sinpi), 0.5, 0x1p31f, 10000)
TEST_SYM_INTERVAL (V_NAME_F1 (sinpi), 0x1p31f, inf, 10000)
#endif
