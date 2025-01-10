/*
 * Single-precision scalar sincospi function.
 *
 * Copyright (c) 2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"
#include "test_sig.h"
#include "test_defs.h"
#include "poly_scalar_f32.h"

/* Taylor series coefficents for sin(pi * x).  */
const static struct sincospif_data
{
  float poly[6];
} sincospif_data = {
  /* Taylor series coefficents for sin(pi * x).  */
  .poly = { 0x1.921fb6p1f, -0x1.4abbcep2f, 0x1.466bc6p1f, -0x1.32d2ccp-1f,
	    0x1.50783p-4f, -0x1.e30750p-8f },
};

/* Top 12 bits of the float representation with the sign bit cleared.  */
static inline uint32_t
abstop12 (float x)
{
  return (asuint (x) >> 20) & 0x7ff;
}

/* Triages special cases into 4 categories:
     -1 or +1 if iy represents half an integer
       -1 if round(y) is odd.
       +1 if round(y) is even.
     -2 or +2 if iy represents and integer.
       -2 if iy is odd.
       +2 if iy is even.
   The argument is the bit representation of a positive non-zero
   finite floating-point value which is either a half or an integer.  */
static inline int
checkint (uint32_t iy)
{
  int e = iy >> 23;
  if (e > 0x7f + 23)
    return 2;
  if (iy & ((1 << (0x7f + 23 - e)) - 1))
    {
      if ((iy - 1) & 2)
	return -1;
      else
	return 1;
    }
  if (iy & (1 << (0x7f + 23 - e)))
    return -2;
  return 2;
}

/* Approximation for scalar single-precision sincospif(x).
   Maximum error for sin: 3.04 ULP:
      sincospif_sin(0x1.c597ccp-2) got 0x1.f7cd56p-1 want 0x1.f7cd5p-1.
   Maximum error for cos: 3.18 ULP:
      sincospif_cos(0x1.d341a8p-5) got 0x1.f7cd56p-1 want 0x1.f7cd5p-1.  */
void
arm_math_sincospif (float x, float *out_sin, float *out_cos)
{

  const struct sincospif_data *d = ptr_barrier (&sincospif_data);
  uint32_t sign = asuint (x) & 0x80000000;

  /* abs(x) in [0, 0x1p22].  */
  if (likely (abstop12 (x) < abstop12 (0x1p22)))
    {
      /* ar_s = x - n (range reduction into -1/2 .. 1/2).  */
      float ar_s = x - rintf (x);
      /* We know that cospi(x) = sinpi(0.5 - x)
      range reduction and offset into sinpi range -1/2 .. 1/2
      ar_c = 0.5 - |x - n|.  */
      float ar_c = 0.5f - fabsf (ar_s);

      float ar2_s = ar_s * ar_s;
      float ar2_c = ar_c * ar_c;
      float ar4_s = ar2_s * ar2_s;
      float ar4_c = ar2_c * ar2_c;

      uint32_t cc_sign = lrintf (x) << 31;
      uint32_t ss_sign = cc_sign;
      if (ar_s == 0)
	ss_sign = sign;

      /* As all values are reduced to -1/2 .. 1/2, the result of cos(x)
      always be positive, therefore, the sign must be introduced
      based upon if x rounds to odd or even. For sin(x) the sign is
      copied from x.  */
      *out_sin = pw_horner_5_f32 (ar2_s, ar4_s, d->poly)
		 * asfloat (asuint (ar_s) ^ ss_sign);
      *out_cos = pw_horner_5_f32 (ar2_c, ar4_c, d->poly)
		 * asfloat (asuint (ar_c) ^ cc_sign);
      return;
    }
  else
    {
      /* When abs(x) > 0x1p22, the x will be either
	    - Half integer (relevant if abs(x) in [0x1p22, 0x1p23])
	    - Odd integer  (relevant if abs(x) in [0x1p22, 0x1p24])
	    - Even integer (relevant if abs(x) in [0x1p22, inf])
	    - Inf or NaN.  */
      if (abstop12 (x) >= 0x7f8)
	{
	  float inv_result = __math_invalidf (x);
	  *out_sin = inv_result;
	  *out_cos = inv_result;
	  return;
	}
      else
	{
	  uint32_t ax = asuint (x) & 0x7fffffff;
	  int m = checkint (ax);
	  if (m & 1)
	    {
	      *out_sin = sign ? -m : m;
	      *out_cos = 0;
	      return;
	    }
	  else
	    {
	      *out_sin = asfloat (sign);
	      *out_cos = m >> 1;
	      return;
	    }
	}
    }
}

#if WANT_TRIGPI_TESTS
TEST_DISABLE_FENV (arm_math_sincospif_sin)
TEST_DISABLE_FENV (arm_math_sincospif_cos)
TEST_ULP (arm_math_sincospif_sin, 2.54)
TEST_ULP (arm_math_sincospif_cos, 2.68)
#  define SINCOSPIF_INTERVAL(lo, hi, n)                                       \
    TEST_SYM_INTERVAL (arm_math_sincospif_sin, lo, hi, n)                     \
    TEST_SYM_INTERVAL (arm_math_sincospif_cos, lo, hi, n)
SINCOSPIF_INTERVAL (0, 0x1p-31, 10000)
SINCOSPIF_INTERVAL (0x1p-31, 1, 50000)
SINCOSPIF_INTERVAL (1, 0x1p22f, 50000)
SINCOSPIF_INTERVAL (0x1p22f, inf, 10000)
#endif
