/*
 * Double-precision vector cosh(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "pl_sig.h"
#include "pl_test.h"
#include "v_exp_tail.h"

#define C1 v_f64 (C1_scal)
#define C2 v_f64 (C2_scal)
#define C3 v_f64 (C3_scal)
#define InvLn2 v_f64 (InvLn2_scal)
#define Ln2hi v_f64 (Ln2hi_scal)
#define Ln2lo v_f64 (Ln2lo_scal)
#define IndexMask v_u64 (IndexMask_scal)
#define Shift v_f64 (Shift_scal)
#define Thres v_f64 (Thres_scal)

#define AbsMask 0x7fffffffffffffff
#define Half v_f64 (0.5)
#define SpecialBound                                                           \
  0x4086000000000000 /* 0x1.6p9, above which exp overflows.  */

#if V_SUPPORTED

static inline v_f64_t
exp_inline (v_f64_t x)
{
  /* Helper for approximating exp(x). Copied from v_exp_tail, with no
     special-case handling or tail.  */

  /* n = round(x/(ln2/N)).  */
  v_f64_t z = v_fma_f64 (x, InvLn2, Shift);
  v_u64_t u = v_as_u64_f64 (z);
  v_f64_t n = z - Shift;

  /* r = x - n*ln2/N.  */
  v_f64_t r = x;
  r = v_fma_f64 (-Ln2hi, n, r);
  r = v_fma_f64 (-Ln2lo, n, r);

  v_u64_t e = u << (52 - V_EXP_TAIL_TABLE_BITS);
  v_u64_t i = u & IndexMask;

  /* y = tail + exp(r) - 1 ~= r + C1 r^2 + C2 r^3 + C3 r^4.  */
  v_f64_t y = v_fma_f64 (C3, r, C2);
  y = v_fma_f64 (y, r, C1);
  y = v_fma_f64 (y, r, v_f64 (1)) * r;

  /* s = 2^(n/N).  */
  u = v_lookup_u64 (Tab, i);
  v_f64_t s = v_as_f64_u64 (u + e);

  return v_fma_f64 (y, s, s);
}

/* Approximation for vector double-precision cosh(x) using exp_inline.
   cosh(x) = (exp(x) + exp(-x)) / 2.
   The greatest observed error is in the scalar fall-back region, so is the same
   as the scalar routine, 1.93 ULP:
   __v_cosh(0x1.628af341989dap+9) got 0x1.fdf28623ef921p+1021
				 want 0x1.fdf28623ef923p+1021.

   The greatest observed error in the non-special region is 1.54 ULP:
   __v_cosh(0x1.8e205b6ecacf7p+2) got 0x1.f711dcb0c77afp+7
				 want 0x1.f711dcb0c77b1p+7.  */
VPCS_ATTR v_f64_t V_NAME (cosh) (v_f64_t x)
{
  v_u64_t ix = v_as_u64_f64 (x);
  v_u64_t iax = ix & AbsMask;
  v_u64_t special = v_cond_u64 (iax > SpecialBound);

  /* If any inputs are special, fall back to scalar for all lanes.  */
  if (unlikely (v_any_u64 (special)))
    return v_call_f64 (cosh, x, x, v_u64 (-1));

  v_f64_t ax = v_as_f64_u64 (iax);
  /* Up to the point that exp overflows, we can use it to calculate cosh by
     exp(|x|) / 2 + 1 / (2 * exp(|x|)).  */
  v_f64_t t = exp_inline (ax);
  return t * Half + Half / t;
}
VPCS_ALIAS

PL_SIG (V, D, 1, cosh, -10.0, 10.0)
PL_TEST_ULP (V_NAME (cosh), 1.43)
PL_TEST_EXPECT_FENV_ALWAYS (V_NAME (cosh))
PL_TEST_INTERVAL (V_NAME (cosh), 0, 0x1.6p9, 100000)
PL_TEST_INTERVAL (V_NAME (cosh), -0, -0x1.6p9, 100000)
PL_TEST_INTERVAL (V_NAME (cosh), 0x1.6p9, inf, 1000)
PL_TEST_INTERVAL (V_NAME (cosh), -0x1.6p9, -inf, 1000)
#endif
