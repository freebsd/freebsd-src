/*
 * Single-precision vector log(1+x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#if V_SUPPORTED

#define AbsMask 0x7fffffff
#define TinyBound 0x340 /* asuint32(0x1p-23). ulp=0.5 at 0x1p-23.  */
#define MinusOne 0xbf800000
#define Ln2 (0x1.62e43p-1f)
#define Four 0x40800000
#define ThreeQuarters v_u32 (0x3f400000)

#define C(i) v_f32 (__log1pf_data.coeffs[i])

static inline v_f32_t
eval_poly (v_f32_t m)
{
#ifdef V_LOG1PF_1U3

  /* Approximate log(1+m) on [-0.25, 0.5] using Horner scheme.  */
  v_f32_t p = v_fma_f32 (C (8), m, C (7));
  p = v_fma_f32 (p, m, C (6));
  p = v_fma_f32 (p, m, C (5));
  p = v_fma_f32 (p, m, C (4));
  p = v_fma_f32 (p, m, C (3));
  p = v_fma_f32 (p, m, C (2));
  p = v_fma_f32 (p, m, C (1));
  p = v_fma_f32 (p, m, C (0));
  return v_fma_f32 (m, m * p, m);

#elif defined(V_LOG1PF_2U5)

  /* Approximate log(1+m) on [-0.25, 0.5] using Estrin scheme.  */
  v_f32_t p_12 = v_fma_f32 (m, C (1), C (0));
  v_f32_t p_34 = v_fma_f32 (m, C (3), C (2));
  v_f32_t p_56 = v_fma_f32 (m, C (5), C (4));
  v_f32_t p_78 = v_fma_f32 (m, C (7), C (6));

  v_f32_t m2 = m * m;
  v_f32_t p_02 = v_fma_f32 (m2, p_12, m);
  v_f32_t p_36 = v_fma_f32 (m2, p_56, p_34);
  v_f32_t p_79 = v_fma_f32 (m2, C (8), p_78);

  v_f32_t m4 = m2 * m2;
  v_f32_t p_06 = v_fma_f32 (m4, p_36, p_02);

  return v_fma_f32 (m4, m4 * p_79, p_06);

#else
#error No precision specified for v_log1pf
#endif
}

static inline float
handle_special (float x)
{
  uint32_t ix = asuint (x);
  uint32_t ia = ix & AbsMask;
  if (ix == 0xff800000 || ia > 0x7f800000 || ix > 0xbf800000)
    {
      /* x == -Inf   => log1pf(x) = NaN.
	 x <  -1.0   => log1pf(x) = NaN.
	 x == +/-NaN => log1pf(x) = NaN.  */
#if WANT_SIMD_EXCEPT
      return __math_invalidf (asfloat (ia));
#else
      return NAN;
#endif
    }
  if (ix == 0xbf800000)
    {
      /* x == -1.0 => log1pf(x) = -Inf.  */
#if WANT_SIMD_EXCEPT
      return __math_divzerof (ix);
#else
      return -INFINITY;
#endif
    }
  /* |x| < TinyBound => log1p(x)  =  x.  */
  return x;
}

/* Vector log1pf approximation using polynomial on reduced interval. Accuracy is
   the same as for the scalar algorithm, i.e. worst-case error when using Estrin
   is roughly 2.02 ULP:
   log1pf(0x1.21e13ap-2) got 0x1.fe8028p-3 want 0x1.fe802cp-3.  */
VPCS_ATTR v_f32_t V_NAME (log1pf) (v_f32_t x)
{
  v_u32_t ix = v_as_u32_f32 (x);
  v_u32_t ia12 = (ix >> 20) & v_u32 (0x7f8);
  v_u32_t special_cases
    = v_cond_u32 (ia12 - v_u32 (TinyBound) >= (0x7f8 - TinyBound))
      | v_cond_u32 (ix >= MinusOne);
  v_f32_t special_arg = x;

#if WANT_SIMD_EXCEPT
  if (unlikely (v_any_u32 (special_cases)))
    /* Side-step special lanes so fenv exceptions are not triggered
       inadvertently.  */
    x = v_sel_f32 (special_cases, v_f32 (1), x);
#endif

  /* With x + 1 = t * 2^k (where t = m + 1 and k is chosen such that m
			   is in [-0.25, 0.5]):
     log1p(x) = log(t) + log(2^k) = log1p(m) + k*log(2).

     We approximate log1p(m) with a polynomial, then scale by
     k*log(2). Instead of doing this directly, we use an intermediate
     scale factor s = 4*k*log(2) to ensure the scale is representable
     as a normalised fp32 number.  */

  v_f32_t m = x + v_f32 (1.0f);

  /* Choose k to scale x to the range [-1/4, 1/2].  */
  v_s32_t k = (v_as_s32_f32 (m) - ThreeQuarters) & v_u32 (0xff800000);

  /* Scale x by exponent manipulation.  */
  v_f32_t m_scale = v_as_f32_u32 (v_as_u32_f32 (x) - v_as_u32_s32 (k));

  /* Scale up to ensure that the scale factor is representable as normalised
     fp32 number, and scale m down accordingly.  */
  v_f32_t s = v_as_f32_u32 (v_u32 (Four) - k);
  m_scale = m_scale + v_fma_f32 (v_f32 (0.25f), s, v_f32 (-1.0f));

  /* Evaluate polynomial on the reduced interval.  */
  v_f32_t p = eval_poly (m_scale);

  /* The scale factor to be applied back at the end - by multiplying float(k)
     by 2^-23 we get the unbiased exponent of k.  */
  v_f32_t scale_back = v_to_f32_s32 (k) * v_f32 (0x1p-23f);

  /* Apply the scaling back.  */
  v_f32_t y = v_fma_f32 (scale_back, v_f32 (Ln2), p);

  if (unlikely (v_any_u32 (special_cases)))
    return v_call_f32 (handle_special, special_arg, y, special_cases);
  return y;
}
VPCS_ALIAS

PL_SIG (V, F, 1, log1p, -0.9, 10.0)
PL_TEST_ULP (V_NAME (log1pf), 1.53)
PL_TEST_EXPECT_FENV (V_NAME (log1pf), WANT_SIMD_EXCEPT)
PL_TEST_INTERVAL (V_NAME (log1pf), -10.0, 10.0, 10000)
PL_TEST_INTERVAL (V_NAME (log1pf), 0.0, 0x1p-23, 30000)
PL_TEST_INTERVAL (V_NAME (log1pf), 0x1p-23, 0.001, 50000)
PL_TEST_INTERVAL (V_NAME (log1pf), 0.001, 1.0, 50000)
PL_TEST_INTERVAL (V_NAME (log1pf), 0.0, -0x1p-23, 30000)
PL_TEST_INTERVAL (V_NAME (log1pf), -0x1p-23, -0.001, 30000)
PL_TEST_INTERVAL (V_NAME (log1pf), -0.001, -1.0, 50000)
PL_TEST_INTERVAL (V_NAME (log1pf), -1.0, inf, 1000)
#endif
