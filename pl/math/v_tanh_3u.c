/*
 * Double-precision vector tanh(x) function.
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "estrin.h"
#include "mathlib.h"
#include "pl_sig.h"
#include "pl_test.h"

#if V_SUPPORTED

#define AbsMask v_u64 (0x7fffffffffffffff)
#define InvLn2 v_f64 (0x1.71547652b82fep0)
#define MLn2hi v_f64 (-0x1.62e42fefa39efp-1)
#define MLn2lo v_f64 (-0x1.abc9e3b39803fp-56)
#define Shift v_f64 (0x1.8p52)
#define C(i) v_f64 (__expm1_poly[i])

#define BoringBound 0x403241bf835f9d5f /* asuint64 (0x1.241bf835f9d5fp+4).  */
#define TinyBound 0x3e40000000000000   /* asuint64 (0x1p-27).  */
#define One v_u64 (0x3ff0000000000000)

static inline v_f64_t
expm1_inline (v_f64_t x)
{
  /* Helper routine for calculating exp(x) - 1. Vector port of the helper from
     the scalar variant of tanh.  */

  /* Reduce argument: f in [-ln2/2, ln2/2], i is exact.  */
  v_f64_t j = v_fma_f64 (InvLn2, x, Shift) - Shift;
  v_s64_t i = v_to_s64_f64 (j);
  v_f64_t f = v_fma_f64 (j, MLn2hi, x);
  f = v_fma_f64 (j, MLn2lo, f);

  /* Approximate expm1(f) using polynomial.  */
  v_f64_t f2 = f * f;
  v_f64_t f4 = f2 * f2;
  v_f64_t p = v_fma_f64 (f2, ESTRIN_10 (f, f2, f4, f4 * f4, C), f);

  /* t = 2 ^ i.  */
  v_f64_t t = v_as_f64_u64 (v_as_u64_s64 (i << 52) + One);
  /* expm1(x) = p * t + (t - 1).  */
  return v_fma_f64 (p, t, t - 1);
}

static NOINLINE v_f64_t
special_case (v_f64_t x, v_f64_t y, v_u64_t special)
{
  return v_call_f64 (tanh, x, y, special);
}

/* Vector approximation for double-precision tanh(x), using a simplified
   version of expm1. The greatest observed error is 2.75 ULP:
   __v_tanh(-0x1.c143c3a44e087p-3) got -0x1.ba31ba4691ab7p-3
				  want -0x1.ba31ba4691ab4p-3.  */
VPCS_ATTR v_f64_t V_NAME (tanh) (v_f64_t x)
{
  v_u64_t ix = v_as_u64_f64 (x);
  v_u64_t ia = ix & AbsMask;

  /* Trigger special-cases for tiny, boring and infinity/NaN.  */
  v_u64_t special = v_cond_u64 ((ia - TinyBound) > (BoringBound - TinyBound));
  v_f64_t u;

  /* To trigger fp exceptions correctly, set special lanes to a neutral value.
     They will be fixed up later by the special-case handler.  */
  if (unlikely (v_any_u64 (special)))
    u = v_sel_f64 (special, v_f64 (1), x) * 2;
  else
    u = x * 2;

  /* tanh(x) = (e^2x - 1) / (e^2x + 1).  */
  v_f64_t q = expm1_inline (u);
  v_f64_t y = q / (q + 2);

  if (unlikely (v_any_u64 (special)))
    return special_case (x, y, special);
  return y;
}
VPCS_ALIAS

PL_SIG (V, D, 1, tanh, -10.0, 10.0)
PL_TEST_ULP (V_NAME (tanh), 2.26)
PL_TEST_EXPECT_FENV_ALWAYS (V_NAME (tanh))
PL_TEST_INTERVAL (V_NAME (tanh), 0, TinyBound, 1000)
PL_TEST_INTERVAL (V_NAME (tanh), -0, -TinyBound, 1000)
PL_TEST_INTERVAL (V_NAME (tanh), TinyBound, BoringBound, 100000)
PL_TEST_INTERVAL (V_NAME (tanh), -TinyBound, -BoringBound, 100000)
PL_TEST_INTERVAL (V_NAME (tanh), BoringBound, inf, 1000)
PL_TEST_INTERVAL (V_NAME (tanh), -BoringBound, -inf, 1000)
#endif
