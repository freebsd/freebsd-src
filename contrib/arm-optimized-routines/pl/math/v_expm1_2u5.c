/*
 * Double-precision vector exp(x) - 1 function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#if V_SUPPORTED

#define InvLn2 v_f64 (0x1.71547652b82fep0)
#define MLn2hi v_f64 (-0x1.62e42fefa39efp-1)
#define MLn2lo v_f64 (-0x1.abc9e3b39803fp-56)
#define Shift v_f64 (0x1.8p52)
#define TinyBound                                                              \
  0x3cc0000000000000 /* 0x1p-51, below which expm1(x) is within 2 ULP of x. */
#define SpecialBound                                                           \
  0x40862b7d369a5aa9 /* 0x1.62b7d369a5aa9p+9. For |x| > SpecialBound, the      \
			final stage of the algorithm overflows so fall back to \
			scalar.  */
#define AbsMask 0x7fffffffffffffff
#define One 0x3ff0000000000000

#define C(i) v_f64 (__expm1_poly[i])

static inline v_f64_t
eval_poly (v_f64_t f, v_f64_t f2)
{
  /* Evaluate custom polynomial using Estrin scheme.  */
  v_f64_t p_01 = v_fma_f64 (f, C (1), C (0));
  v_f64_t p_23 = v_fma_f64 (f, C (3), C (2));
  v_f64_t p_45 = v_fma_f64 (f, C (5), C (4));
  v_f64_t p_67 = v_fma_f64 (f, C (7), C (6));
  v_f64_t p_89 = v_fma_f64 (f, C (9), C (8));

  v_f64_t p_03 = v_fma_f64 (f2, p_23, p_01);
  v_f64_t p_47 = v_fma_f64 (f2, p_67, p_45);
  v_f64_t p_8a = v_fma_f64 (f2, C (10), p_89);

  v_f64_t f4 = f2 * f2;
  v_f64_t p_07 = v_fma_f64 (f4, p_47, p_03);
  return v_fma_f64 (f4 * f4, p_8a, p_07);
}

/* Double-precision vector exp(x) - 1 function.
   The maximum error observed error is 2.18 ULP:
   __v_expm1(0x1.634ba0c237d7bp-2) got 0x1.a8b9ea8d66e22p-2
				  want 0x1.a8b9ea8d66e2p-2.  */
VPCS_ATTR
v_f64_t V_NAME (expm1) (v_f64_t x)
{
  v_u64_t ix = v_as_u64_f64 (x);
  v_u64_t ax = ix & AbsMask;

#if WANT_SIMD_EXCEPT
  /* If fp exceptions are to be triggered correctly, fall back to the scalar
     variant for all lanes if any of them should trigger an exception.  */
  v_u64_t special = v_cond_u64 ((ax >= SpecialBound) | (ax <= TinyBound));
  if (unlikely (v_any_u64 (special)))
    return v_call_f64 (expm1, x, x, v_u64 (-1));
#else
  /* Large input, NaNs and Infs.  */
  v_u64_t special
    = v_cond_u64 ((ax >= SpecialBound) | (ix == 0x8000000000000000));
#endif

  /* Reduce argument to smaller range:
     Let i = round(x / ln2)
     and f = x - i * ln2, then f is in [-ln2/2, ln2/2].
     exp(x) - 1 = 2^i * (expm1(f) + 1) - 1
     where 2^i is exact because i is an integer.  */
  v_f64_t j = v_fma_f64 (InvLn2, x, Shift) - Shift;
  v_s64_t i = v_to_s64_f64 (j);
  v_f64_t f = v_fma_f64 (j, MLn2hi, x);
  f = v_fma_f64 (j, MLn2lo, f);

  /* Approximate expm1(f) using polynomial.
     Taylor expansion for expm1(x) has the form:
	 x + ax^2 + bx^3 + cx^4 ....
     So we calculate the polynomial P(f) = a + bf + cf^2 + ...
     and assemble the approximation expm1(f) ~= f + f^2 * P(f).  */
  v_f64_t f2 = f * f;
  v_f64_t p = v_fma_f64 (f2, eval_poly (f, f2), f);

  /* Assemble the result.
     expm1(x) ~= 2^i * (p + 1) - 1
     Let t = 2^i.  */
  v_f64_t t = v_as_f64_u64 (v_as_u64_s64 (i << 52) + One);
  /* expm1(x) ~= p * t + (t - 1).  */
  v_f64_t y = v_fma_f64 (p, t, t - 1);

#if !WANT_SIMD_EXCEPT
  if (unlikely (v_any_u64 (special)))
    return v_call_f64 (expm1, x, y, special);
#endif

  return y;
}
VPCS_ALIAS

PL_SIG (V, D, 1, expm1, -9.9, 9.9)
PL_TEST_ULP (V_NAME (expm1), 1.68)
PL_TEST_EXPECT_FENV (V_NAME (expm1), WANT_SIMD_EXCEPT)
PL_TEST_INTERVAL (V_NAME (expm1), 0, 0x1p-51, 1000)
PL_TEST_INTERVAL (V_NAME (expm1), -0, -0x1p-51, 1000)
PL_TEST_INTERVAL (V_NAME (expm1), 0x1p-51, 0x1.63108c75a1937p+9, 100000)
PL_TEST_INTERVAL (V_NAME (expm1), -0x1p-51, -0x1.740bf7c0d927dp+9, 100000)
PL_TEST_INTERVAL (V_NAME (expm1), 0x1.63108c75a1937p+9, inf, 100)
PL_TEST_INTERVAL (V_NAME (expm1), -0x1.740bf7c0d927dp+9, -inf, 100)
#endif
