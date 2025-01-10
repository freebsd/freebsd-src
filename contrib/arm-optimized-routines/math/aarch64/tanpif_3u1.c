/*
 * Single-precision scalar tanpi(x) function.
 *
 * Copyright (c) 2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "mathlib.h"
#include "math_config.h"
#include "test_sig.h"
#include "test_defs.h"
#include "poly_scalar_f32.h"

const static struct tanpif_data
{
  float tan_poly[6], cot_poly[4], pi, invpi;
} tanpif_data = {
  /* Coefficents for tan(pi * x).  */
  .tan_poly = {
    0x1.4abbc8p3,
    0x1.467284p5,
    0x1.44cf12p7,
    0x1.596b5p9,
    0x1.753858p10,
    0x1.76ff52p14,
  },
  /* Coefficents for cot(pi * x).  */
  .cot_poly = {
    -0x1.0c1522p0,
    -0x1.60ce32p-1,
    -0x1.49cd42p-1,
    -0x1.73f786p-1,
  },
  .pi = 0x1.921fb6p1f,
  .invpi = 0x1.45f308p-2f,
};

/* Single-precision scalar tanpi(x) implementation.
   Maximum error 2.56 ULP:
   tanpif(0x1.4bf948p-1) got -0x1.fcc9ep+0
			want -0x1.fcc9e6p+0.  */
float
arm_math_tanpif (float x)
{
  uint32_t xabs_12 = asuint (x) >> 20 & 0x7f8;

  /* x >= 0x1p24f.  */
  if (unlikely (xabs_12 >= 0x4b1))
    {
      /* tanpif(+/-inf) and tanpif(+/-nan) = nan.  */
      if (unlikely (xabs_12 == 0x7f8))
	{
	  return __math_invalidf (x);
	}

      uint32_t x_sign = asuint (x) & 0x80000000;
      return asfloat (x_sign);
    }

  const struct tanpif_data *d = ptr_barrier (&tanpif_data);

  /* Prevent underflow exceptions. x <= 0x1p-31.  */
  if (unlikely (xabs_12 < 0x300))
    {
      return d->pi * x;
    }

  float rounded = roundf (x);
  if (unlikely (rounded == x))
    {
      /* If x == 0, return with sign.  */
      if (x == 0)
	{
	  return x;
	}
      /* Otherwise, return zero with alternating sign.  */
      int32_t m = (int32_t) rounded;
      if (x < 0)
	{
	  return m & 1 ? 0.0f : -0.0f;
	}
      else
	{
	  return m & 1 ? -0.0f : 0.0f;
	}
    }

  float x_reduced = x - rounded;
  float abs_x_reduced = 0.5f - asfloat (asuint (x_reduced) & 0x7fffffff);

  float result, offset, scale;

  /* Test  0.25 < abs_x < 0.5 independent from abs_x_reduced.  */
  float x2 = x + x;
  int32_t rounded_x2 = (int32_t) roundf (x2);
  if (rounded_x2 & 1)
    {
      float r_x = abs_x_reduced;

      float r_x2 = r_x * r_x;
      float r_x4 = r_x2 * r_x2;

      uint32_t sign = asuint (x_reduced) & 0x80000000;
      r_x = asfloat (asuint (r_x) ^ sign);

      // calculate sign for half-fractional inf values
      uint32_t is_finite = asuint (abs_x_reduced);
      uint32_t is_odd = (rounded_x2 & 2) << 30;
      uint32_t is_neg = rounded_x2 & 0x80000000;
      uint32_t keep_sign = is_finite | (is_odd ^ is_neg);
      offset = d->invpi / (keep_sign ? r_x : -r_x);
      scale = r_x;

      result = pairwise_poly_3_f32 (r_x2, r_x4, d->cot_poly);
    }
  else
    {
      float r_x = x_reduced;

      float r_x2 = r_x * r_x;
      float r_x4 = r_x2 * r_x2;

      offset = d->pi * r_x;
      scale = r_x * r_x2;

      result = pw_horner_5_f32 (r_x2, r_x4, d->tan_poly);
    }

  return fmaf (scale, result, offset);
}

#if WANT_EXPERIMENTAL_MATH
float
tanpif (float x)
{
  return arm_math_tanpif (x);
}
#endif

#if WANT_TRIGPI_TESTS
TEST_ULP (arm_math_tanpif, 2.57)
TEST_SYM_INTERVAL (arm_math_tanpif, 0, 0x1p-31f, 50000)
TEST_SYM_INTERVAL (arm_math_tanpif, 0x1p-31f, 0.5, 100000)
TEST_SYM_INTERVAL (arm_math_tanpif, 0.5, 0x1p23f, 100000)
TEST_SYM_INTERVAL (arm_math_tanpif, 0x1p23f, inf, 100000)
#endif
