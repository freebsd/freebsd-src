/*
 * Single-precision scalar tan(x) function.
 *
 * Copyright (c) 2021-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "math_config.h"
#include "test_sig.h"
#include "test_defs.h"
#include "poly_scalar_f32.h"

/* Useful constants.  */
#define NegPio2_1 (-0x1.921fb6p+0f)
#define NegPio2_2 (0x1.777a5cp-25f)
#define NegPio2_3 (0x1.ee59dap-50f)
/* Reduced from 0x1p20 to 0x1p17 to ensure 3.5ulps.  */
#define RangeVal (0x1p17f)
#define InvPio2 ((0x1.45f306p-1f))
#define Shift (0x1.8p+23f)
#define AbsMask (0x7fffffff)
#define Pio4 (0x1.921fb6p-1)
/* 2PI * 2^-64.  */
#define Pio2p63 (0x1.921FB54442D18p-62)

static inline float
eval_P (float z)
{
  return pw_horner_5_f32 (z, z * z, __tanf_poly_data.poly_tan);
}

static inline float
eval_Q (float z)
{
  return pairwise_poly_3_f32 (z, z * z, __tanf_poly_data.poly_cotan);
}

/* Reduction of the input argument x using Cody-Waite approach, such that x = r
   + n * pi/2 with r lives in [-pi/4, pi/4] and n is a signed integer.  */
static inline float
reduce (float x, int32_t *in)
{
  /* n = rint(x/(pi/2)).  */
  float r = x;
  float q = fmaf (InvPio2, r, Shift);
  float n = q - Shift;
  /* There is no rounding here, n is representable by a signed integer.  */
  *in = (int32_t) n;
  /* r = x - n * (pi/2)  (range reduction into -pi/4 .. pi/4).  */
  r = fmaf (NegPio2_1, n, r);
  r = fmaf (NegPio2_2, n, r);
  r = fmaf (NegPio2_3, n, r);
  return r;
}

/* Reduce the range of XI to a multiple of PI/2 using fast integer arithmetic.
   XI is a reinterpreted float and must be >= 2.0f (the sign bit is ignored).
   Return the modulo between -PI/4 and PI/4 and store the quadrant in NP.
   Reduction uses a table of 4/PI with 192 bits of precision.  A 32x96->128 bit
   multiply computes the exact 2.62-bit fixed-point modulo.  Since the result
   can have at most 29 leading zeros after the binary point, the double
   precision result is accurate to 33 bits.  */
static inline double
reduce_large (uint32_t xi, int *np)
{
  const uint32_t *arr = &__inv_pio4[(xi >> 26) & 15];
  int shift = (xi >> 23) & 7;
  uint64_t n, res0, res1, res2;

  xi = (xi & 0xffffff) | 0x800000;
  xi <<= shift;

  res0 = xi * arr[0];
  res1 = (uint64_t) xi * arr[4];
  res2 = (uint64_t) xi * arr[8];
  res0 = (res2 >> 32) | (res0 << 32);
  res0 += res1;

  n = (res0 + (1ULL << 61)) >> 62;
  res0 -= n << 62;
  double x = (int64_t) res0;
  *np = n;
  return x * Pio2p63;
}

/* Top 12 bits of the float representation with the sign bit cleared.  */
static inline uint32_t
top12 (float x)
{
  return (asuint (x) >> 20);
}

/* Fast single-precision tan implementation.
   Maximum ULP error: 3.293ulps.
   tanf(0x1.c849eap+16) got -0x1.fe8d98p-1 want -0x1.fe8d9ep-1.  */
float
tanf (float x)
{
  /* Get top words.  */
  uint32_t ix = asuint (x);
  uint32_t ia = ix & AbsMask;
  uint32_t ia12 = ia >> 20;

  /* Dispatch between no reduction (small numbers), fast reduction and
     slow large numbers reduction. The reduction step determines r float
     (|r| < pi/4) and n signed integer such that x = r + n * pi/2.  */
  int32_t n;
  float r;
  if (ia12 < top12 (Pio4))
    {
      /* Optimize small values.  */
      if (unlikely (ia12 < top12 (0x1p-12f)))
	{
	  if (unlikely (ia12 < top12 (0x1p-126f)))
	    /* Force underflow for tiny x.  */
	    force_eval_float (x * x);
	  return x;
	}

      /* tan (x) ~= x + x^3 * P(x^2).  */
      float x2 = x * x;
      float y = eval_P (x2);
      return fmaf (x2, x * y, x);
    }
  /* Similar to other trigonometric routines, fast inaccurate reduction is
     performed for values of x from pi/4 up to RangeVal. In order to keep
     errors below 3.5ulps, we set the value of RangeVal to 2^17. This might
     differ for other trigonometric routines. Above this value more advanced
     but slower reduction techniques need to be implemented to reach a similar
     accuracy.  */
  else if (ia12 < top12 (RangeVal))
    {
      /* Fast inaccurate reduction.  */
      r = reduce (x, &n);
    }
  else if (ia12 < 0x7f8)
    {
      /* Slow accurate reduction.  */
      uint32_t sign = ix & ~AbsMask;
      double dar = reduce_large (ia, &n);
      float ar = (float) dar;
      r = asfloat (asuint (ar) ^ sign);
    }
  else
    {
      /* tan(Inf or NaN) is NaN.  */
      return __math_invalidf (x);
    }

  /* If x lives in an interval where |tan(x)|
     - is finite then use an approximation of tangent in the form
       tan(r) ~ r + r^3 * P(r^2) = r + r * r^2 * P(r^2).
     - grows to infinity then use an approximation of cotangent in the form
       cotan(z) ~ 1/z + z * Q(z^2), where the reciprocal can be computed early.
       Using symmetries of tangent and the identity tan(r) = cotan(pi/2 - r),
       we only need to change the sign of r to obtain tan(x) from cotan(r).
     This 2-interval approach requires 2 different sets of coefficients P and
     Q, where Q is a lower order polynomial than P.  */

  /* Determine if x lives in an interval where |tan(x)| grows to infinity.  */
  uint32_t alt = (uint32_t) n & 1;

  /* Perform additional reduction if required.  */
  float z = alt ? -r : r;

  /* Prepare backward transformation.  */
  float z2 = r * r;
  float offset = alt ? 1.0f / z : z;
  float scale = alt ? z : z * z2;

  /* Evaluate polynomial approximation of tan or cotan.  */
  float p = alt ? eval_Q (z2) : eval_P (z2);

  /* A unified way of assembling the result on both interval types.  */
  return fmaf (scale, p, offset);
}

TEST_SIG (S, F, 1, tan, -3.1, 3.1)
TEST_ULP (tanf, 2.80)
TEST_INTERVAL (tanf, 0, 0xffff0000, 10000)
TEST_SYM_INTERVAL (tanf, 0x1p-127, 0x1p-14, 50000)
TEST_SYM_INTERVAL (tanf, 0x1p-14, 0.7, 50000)
TEST_SYM_INTERVAL (tanf, 0.7, 1.5, 50000)
TEST_SYM_INTERVAL (tanf, 1.5, 0x1p17, 50000)
TEST_SYM_INTERVAL (tanf, 0x1p17, 0x1p54, 50000)
TEST_SYM_INTERVAL (tanf, 0x1p54, inf, 50000)
