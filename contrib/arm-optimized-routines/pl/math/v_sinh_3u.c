/*
 * Double-precision vector sinh(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "estrin.h"
#include "pl_sig.h"
#include "pl_test.h"

#define AbsMask 0x7fffffffffffffff
#define Half 0x3fe0000000000000
#define BigBound                                                               \
  0x4080000000000000 /* 2^9. expm1 helper overflows for large input.  */
#define TinyBound                                                              \
  0x3e50000000000000 /* 2^-26, below which sinh(x) rounds to x.  */
#define InvLn2 v_f64 (0x1.71547652b82fep0)
#define MLn2hi v_f64 (-0x1.62e42fefa39efp-1)
#define MLn2lo v_f64 (-0x1.abc9e3b39803fp-56)
#define Shift v_f64 (0x1.8p52)
#define One 0x3ff0000000000000
#define C(i) v_f64 (__expm1_poly[i])

#if V_SUPPORTED

static inline v_f64_t
expm1_inline (v_f64_t x)
{
  /* Reduce argument:
     exp(x) - 1 = 2^i * (expm1(f) + 1) - 1
     where i = round(x / ln2)
     and   f = x - i * ln2 (f in [-ln2/2, ln2/2]).  */
  v_f64_t j = v_fma_f64 (InvLn2, x, Shift) - Shift;
  v_s64_t i = v_to_s64_f64 (j);
  v_f64_t f = v_fma_f64 (j, MLn2hi, x);
  f = v_fma_f64 (j, MLn2lo, f);
  /* Approximate expm1(f) using polynomial.  */
  v_f64_t f2 = f * f, f4 = f2 * f2, f8 = f4 * f4;
  v_f64_t p = v_fma_f64 (f2, ESTRIN_10 (f, f2, f4, f8, C), f);
  /* t = 2^i.  */
  v_f64_t t = v_as_f64_u64 (v_as_u64_s64 (i << 52) + One);
  /* expm1(x) ~= p * t + (t - 1).  */
  return v_fma_f64 (p, t, t - 1);
}

static NOINLINE VPCS_ATTR v_f64_t
special_case (v_f64_t x)
{
  return v_call_f64 (sinh, x, x, v_u64 (-1));
}

/* Approximation for vector double-precision sinh(x) using expm1.
   sinh(x) = (exp(x) - exp(-x)) / 2.
   The greatest observed error is 2.57 ULP:
   sinh(0x1.9fb1d49d1d58bp-2) got 0x1.ab34e59d678dcp-2
			     want 0x1.ab34e59d678d9p-2.  */
VPCS_ATTR v_f64_t V_NAME (sinh) (v_f64_t x)
{
  v_u64_t ix = v_as_u64_f64 (x);
  v_u64_t iax = ix & AbsMask;
  v_f64_t ax = v_as_f64_u64 (iax);
  v_u64_t sign = ix & ~AbsMask;
  v_f64_t halfsign = v_as_f64_u64 (sign | Half);

#if WANT_SIMD_EXCEPT
  v_u64_t special = v_cond_u64 ((iax - TinyBound) >= (BigBound - TinyBound));
#else
  v_u64_t special = v_cond_u64 (iax >= BigBound);
#endif

  /* Fall back to scalar variant for all lanes if any of them are special.  */
  if (unlikely (v_any_u64 (special)))
    return special_case (x);

  /* Up to the point that expm1 overflows, we can use it to calculate sinh
     using a slight rearrangement of the definition of sinh. This allows us to
     retain acceptable accuracy for very small inputs.  */
  v_f64_t t = expm1_inline (ax);
  return (t + t / (t + 1)) * halfsign;
}
VPCS_ALIAS

PL_SIG (V, D, 1, sinh, -10.0, 10.0)
PL_TEST_ULP (V_NAME (sinh), 2.08)
PL_TEST_EXPECT_FENV (V_NAME (sinh), WANT_SIMD_EXCEPT)
PL_TEST_INTERVAL (V_NAME (sinh), 0, TinyBound, 1000)
PL_TEST_INTERVAL (V_NAME (sinh), -0, -TinyBound, 1000)
PL_TEST_INTERVAL (V_NAME (sinh), TinyBound, BigBound, 500000)
PL_TEST_INTERVAL (V_NAME (sinh), -TinyBound, -BigBound, 500000)
PL_TEST_INTERVAL (V_NAME (sinh), BigBound, inf, 1000)
PL_TEST_INTERVAL (V_NAME (sinh), -BigBound, -inf, 1000)
#endif
