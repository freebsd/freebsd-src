/*
 * Double-precision scalar tanpi(x) function.
 *
 * Copyright (c) 2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "mathlib.h"
#include "math_config.h"
#include "test_sig.h"
#include "test_defs.h"
#include "poly_scalar_f64.h"

#define SIGN_MASK 0x8000000000000000

const static struct tanpi_data
{
  double tan_poly[14], cot_poly[9], pi, invpi;
} tanpi_data = {
  /* Coefficents for tan(pi * x).  */
  .tan_poly = {
    0x1.4abbce625be52p3,
    0x1.466bc6775b0f9p5,
    0x1.45fff9b426f5ep7,
    0x1.45f4730dbca5cp9,
    0x1.45f3265994f85p11,
    0x1.45f4234b330cap13,
    0x1.45dca11be79ebp15,
    0x1.47283fc5eea69p17,
    0x1.3a6d958cdefaep19,
    0x1.927896baee627p21,
    -0x1.89333f6acd922p19,
    0x1.5d4e912bb8456p27,
    -0x1.a854d53ab6874p29,
    0x1.1b76de7681424p32,
  },
  /* Coefficents for cot(pi * x).  */
  .cot_poly = {
    -0x1.0c152382d7366p0,
    -0x1.60c8539c1d316p-1,
    -0x1.4b9a2f3516354p-1,
    -0x1.47474060b6ba8p-1,
    -0x1.464633ad9dcb1p-1,
    -0x1.45ff229d7edd6p-1,
    -0x1.46d8dbf492923p-1,
    -0x1.3873892311c6bp-1,
    -0x1.b2f3d0ff96d73p-1,
  },
  .pi = 0x1.921fb54442d18p1,
  .invpi = 0x1.45f306dc9c883p-2,
};

/* Double-precision scalar tanpi(x) implementation.
   Maximum error 2.19 ULP:
   tanpi(0x1.68847e177a855p-2) got 0x1.fe9a0ff9bb9d7p+0
			      want 0x1.fe9a0ff9bb9d5p+0.  */
double
arm_math_tanpi (double x)
{
  uint64_t xabs_12 = asuint64 (x) >> 52 & 0x7ff;

  /* x >= 0x1p54.  */
  if (unlikely (xabs_12 >= 0x434))
    {
      /* tanpi(+/-inf) and tanpi(+/-nan) = nan.  */
      if (unlikely (xabs_12 == 0x7ff))
	{
	  return __math_invalid (x);
	}

      uint64_t x_sign = asuint64 (x) & SIGN_MASK;
      return asdouble (x_sign);
    }

  const struct tanpi_data *d = ptr_barrier (&tanpi_data);

  double rounded = round (x);
  if (unlikely (rounded == x))
    {
      /* If x == 0, return with sign.  */
      if (x == 0)
	{
	  return x;
	}
      /* Otherwise, return zero with alternating sign.  */
      int64_t m = (int64_t) rounded;
      if (x < 0)
	{
	  return m & 1 ? 0.0 : -0.0;
	}
      else
	{
	  return m & 1 ? -0.0 : 0.0;
	}
    }

  double x_reduced = x - rounded;
  double abs_x_reduced = 0.5 - fabs (x_reduced);

  /* Prevent underflow exceptions. x <= 0x1p-63.  */
  if (unlikely (xabs_12 < 0x3c0))
    {
      return d->pi * x;
    }

  double result, offset, scale;

  /* Test  0.25 < abs_x < 0.5 independent from abs_x_reduced.  */
  double x2 = x + x;
  int64_t rounded_x2 = (int64_t) round (x2);
  if (rounded_x2 & 1)
    {
      double r_x = abs_x_reduced;

      double r_x2 = r_x * r_x;
      double r_x4 = r_x2 * r_x2;

      uint64_t sign = asuint64 (x_reduced) & SIGN_MASK;
      r_x = asdouble (asuint64 (r_x) ^ sign);

      // calculate sign for half-fractional inf values
      uint64_t is_finite = asuint64 (abs_x_reduced);
      uint64_t is_odd = (rounded_x2 & 2) << 62;
      uint64_t is_neg = rounded_x2 & SIGN_MASK;
      uint64_t keep_sign = is_finite | (is_odd ^ is_neg);
      offset = d->invpi / (keep_sign ? r_x : -r_x);
      scale = r_x;

      result = pw_horner_8_f64 (r_x2, r_x4, d->cot_poly);
    }
  else
    {
      double r_x2 = x_reduced * x_reduced;
      double r_x4 = r_x2 * r_x2;

      offset = d->pi * x_reduced;
      scale = x_reduced * r_x2;

      result = pw_horner_13_f64 (r_x2, r_x4, d->tan_poly);
    }

  return fma (scale, result, offset);
}

#if WANT_EXPERIMENTAL_MATH
double
tanpi (double x)
{
  return arm_math_tanpi (x);
}
#endif

#if WANT_TRIGPI_TESTS
TEST_ULP (arm_math_tanpi, 1.69)
TEST_SYM_INTERVAL (arm_math_tanpi, 0, 0x1p-63, 50000)
TEST_SYM_INTERVAL (arm_math_tanpi, 0x1p-63, 0.5, 100000)
TEST_SYM_INTERVAL (arm_math_tanpi, 0.5, 0x1p53, 100000)
TEST_SYM_INTERVAL (arm_math_tanpi, 0x1p53, inf, 100000)
#endif
