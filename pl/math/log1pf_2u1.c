/*
 * Single-precision log(1+x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "hornerf.h"
#include "math_config.h"
#include "pl_sig.h"
#include "pl_test.h"

#define Ln2 (0x1.62e43p-1f)
#define SignMask (0x80000000)

/* Biased exponent of the largest float m for which m^8 underflows.  */
#define M8UFLOW_BOUND_BEXP 112
/* Biased exponent of the largest float for which we just return x.  */
#define TINY_BOUND_BEXP 103

#define C(i) __log1pf_data.coeffs[i]

static inline float
eval_poly (float m, uint32_t e)
{
#ifdef LOG1PF_2U5

  /* 2.5 ulp variant. Approximate log(1+m) on [-0.25, 0.5] using
     slightly modified Estrin scheme (no x^0 term, and x term is just x).  */
  float p_12 = fmaf (m, C (1), C (0));
  float p_34 = fmaf (m, C (3), C (2));
  float p_56 = fmaf (m, C (5), C (4));
  float p_78 = fmaf (m, C (7), C (6));

  float m2 = m * m;
  float p_02 = fmaf (m2, p_12, m);
  float p_36 = fmaf (m2, p_56, p_34);
  float p_79 = fmaf (m2, C (8), p_78);

  float m4 = m2 * m2;
  float p_06 = fmaf (m4, p_36, p_02);

  if (unlikely (e < M8UFLOW_BOUND_BEXP))
    return p_06;

  float m8 = m4 * m4;
  return fmaf (m8, p_79, p_06);

#elif defined(LOG1PF_1U3)

  /* 1.3 ulp variant. Approximate log(1+m) on [-0.25, 0.5] using Horner
     scheme. Our polynomial approximation for log1p has the form
     x + C1 * x^2 + C2 * x^3 + C3 * x^4 + ...
     Hence approximation has the form m + m^2 * P(m)
       where P(x) = C1 + C2 * x + C3 * x^2 + ... .  */
  return fmaf (m, m * HORNER_8 (m, C), m);

#else
#error No log1pf approximation exists with the requested precision. Options are 13 or 25.
#endif
}

static inline uint32_t
biased_exponent (uint32_t ix)
{
  return (ix & 0x7f800000) >> 23;
}

/* log1pf approximation using polynomial on reduced interval. Worst-case error
   when using Estrin is roughly 2.02 ULP:
   log1pf(0x1.21e13ap-2) got 0x1.fe8028p-3 want 0x1.fe802cp-3.  */
float
log1pf (float x)
{
  uint32_t ix = asuint (x);
  uint32_t ia = ix & ~SignMask;
  uint32_t ia12 = ia >> 20;
  uint32_t e = biased_exponent (ix);

  /* Handle special cases first.  */
  if (unlikely (ia12 >= 0x7f8 || ix >= 0xbf800000 || ix == 0x80000000
		|| e <= TINY_BOUND_BEXP))
    {
      if (ix == 0xff800000)
	{
	  /* x == -Inf => log1pf(x) =  NaN.  */
	  return NAN;
	}
      if ((ix == 0x7f800000 || e <= TINY_BOUND_BEXP) && ia12 <= 0x7f8)
	{
	  /* |x| < TinyBound => log1p(x)  =  x.
	      x ==       Inf => log1pf(x) = Inf.  */
	  return x;
	}
      if (ix == 0xbf800000)
	{
	  /* x == -1.0 => log1pf(x) = -Inf.  */
	  return __math_divzerof (-1);
	}
      if (ia12 >= 0x7f8)
	{
	  /* x == +/-NaN => log1pf(x) = NaN.  */
	  return __math_invalidf (asfloat (ia));
	}
      /* x <    -1.0 => log1pf(x) = NaN.  */
      return __math_invalidf (x);
    }

  /* With x + 1 = t * 2^k (where t = m + 1 and k is chosen such that m
			   is in [-0.25, 0.5]):
     log1p(x) = log(t) + log(2^k) = log1p(m) + k*log(2).

     We approximate log1p(m) with a polynomial, then scale by
     k*log(2). Instead of doing this directly, we use an intermediate
     scale factor s = 4*k*log(2) to ensure the scale is representable
     as a normalised fp32 number.  */

  if (ix <= 0x3f000000 || ia <= 0x3e800000)
    {
      /* If x is in [-0.25, 0.5] then we can shortcut all the logic
	 below, as k = 0 and m = x.  All we need is to return the
	 polynomial.  */
      return eval_poly (x, e);
    }

  float m = x + 1.0f;

  /* k is used scale the input. 0x3f400000 is chosen as we are trying to
     reduce x to the range [-0.25, 0.5]. Inside this range, k is 0.
     Outside this range, if k is reinterpreted as (NOT CONVERTED TO) float:
	 let k = sign * 2^p      where sign = -1 if x < 0
					       1 otherwise
	 and p is a negative integer whose magnitude increases with the
	 magnitude of x.  */
  int k = (asuint (m) - 0x3f400000) & 0xff800000;

  /* By using integer arithmetic, we obtain the necessary scaling by
     subtracting the unbiased exponent of k from the exponent of x.  */
  float m_scale = asfloat (asuint (x) - k);

  /* Scale up to ensure that the scale factor is representable as normalised
     fp32 number (s in [2**-126,2**26]), and scale m down accordingly.  */
  float s = asfloat (asuint (4.0f) - k);
  m_scale = m_scale + fmaf (0.25f, s, -1.0f);

  float p = eval_poly (m_scale, biased_exponent (asuint (m_scale)));

  /* The scale factor to be applied back at the end - by multiplying float(k)
     by 2^-23 we get the unbiased exponent of k.  */
  float scale_back = (float) k * 0x1.0p-23f;

  /* Apply the scaling back.  */
  return fmaf (scale_back, Ln2, p);
}

PL_SIG (S, F, 1, log1p, -0.9, 10.0)
PL_TEST_ULP (log1pf, 1.52)
PL_TEST_INTERVAL (log1pf, -10.0, 10.0, 10000)
PL_TEST_INTERVAL (log1pf, 0.0, 0x1p-23, 50000)
PL_TEST_INTERVAL (log1pf, 0x1p-23, 0.001, 50000)
PL_TEST_INTERVAL (log1pf, 0.001, 1.0, 50000)
PL_TEST_INTERVAL (log1pf, 0.0, -0x1p-23, 50000)
PL_TEST_INTERVAL (log1pf, -0x1p-23, -0.001, 50000)
PL_TEST_INTERVAL (log1pf, -0.001, -1.0, 50000)
PL_TEST_INTERVAL (log1pf, -1.0, inf, 5000)
