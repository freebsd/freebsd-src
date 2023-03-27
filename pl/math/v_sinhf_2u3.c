/*
 * Single-precision vector sinh(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#if V_SUPPORTED

#include "v_expm1f_inline.h"

#define AbsMask 0x7fffffff
#define Half 0x3f000000
#define BigBound                                                               \
  0x42b0c0a7 /* 0x1.61814ep+6, above which expm1f helper overflows.  */
#define TinyBound                                                              \
  0x2fb504f4 /* 0x1.6a09e8p-32, below which expm1f underflows.  */

static NOINLINE VPCS_ATTR v_f32_t
special_case (v_f32_t x)
{
  return v_call_f32 (sinhf, x, x, v_u32 (-1));
}

/* Approximation for vector single-precision sinh(x) using expm1.
   sinh(x) = (exp(x) - exp(-x)) / 2.
   The maximum error is 2.26 ULP:
   __v_sinhf(0x1.e34a9ep-4) got 0x1.e469ep-4 want 0x1.e469e4p-4.  */
VPCS_ATTR v_f32_t V_NAME (sinhf) (v_f32_t x)
{
  v_u32_t ix = v_as_u32_f32 (x);
  v_u32_t iax = ix & AbsMask;
  v_f32_t ax = v_as_f32_u32 (iax);
  v_u32_t sign = ix & ~AbsMask;
  v_f32_t halfsign = v_as_f32_u32 (sign | Half);

#if WANT_SIMD_EXCEPT
  v_u32_t special = v_cond_u32 ((iax - TinyBound) >= (BigBound - TinyBound));
#else
  v_u32_t special = v_cond_u32 (iax >= BigBound);
#endif

  /* Fall back to the scalar variant for all lanes if any of them should trigger
     an exception.  */
  if (unlikely (v_any_u32 (special)))
    return special_case (x);

  /* Up to the point that expm1f overflows, we can use it to calculate sinhf
     using a slight rearrangement of the definition of asinh. This allows us to
     retain acceptable accuracy for very small inputs.  */
  v_f32_t t = expm1f_inline (ax);
  return (t + t / (t + 1)) * halfsign;
}
VPCS_ALIAS

PL_SIG (V, F, 1, sinh, -10.0, 10.0)
PL_TEST_ULP (V_NAME (sinhf), 1.76)
PL_TEST_EXPECT_FENV (V_NAME (sinhf), WANT_SIMD_EXCEPT)
PL_TEST_INTERVAL (V_NAME (sinhf), 0, TinyBound, 1000)
PL_TEST_INTERVAL (V_NAME (sinhf), -0, -TinyBound, 1000)
PL_TEST_INTERVAL (V_NAME (sinhf), TinyBound, BigBound, 100000)
PL_TEST_INTERVAL (V_NAME (sinhf), -TinyBound, -BigBound, 100000)
PL_TEST_INTERVAL (V_NAME (sinhf), BigBound, inf, 1000)
PL_TEST_INTERVAL (V_NAME (sinhf), -BigBound, -inf, 1000)
#endif
