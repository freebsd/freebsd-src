/*
 * Double-precision vector atanh(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "pairwise_horner.h"
#include "pl_sig.h"
#include "pl_test.h"

#if V_SUPPORTED

#define WANT_V_LOG1P_K0_SHORTCUT 0
#include "v_log1p_inline.h"

#define AbsMask 0x7fffffffffffffff
#define Half 0x3fe0000000000000
#define One 0x3ff0000000000000

VPCS_ATTR
NOINLINE static v_f64_t
specialcase (v_f64_t x, v_f64_t y, v_u64_t special)
{
  return v_call_f64 (atanh, x, y, special);
}

/* Approximation for vector double-precision atanh(x) using modified log1p.
   The greatest observed error is 3.31 ULP:
   __v_atanh(0x1.ffae6288b601p-6) got 0x1.ffd8ff31b5019p-6
				 want 0x1.ffd8ff31b501cp-6.  */
VPCS_ATTR
v_f64_t V_NAME (atanh) (v_f64_t x)
{
  v_u64_t ix = v_as_u64_f64 (x);
  v_u64_t sign = ix & ~AbsMask;
  v_u64_t ia = ix & AbsMask;
  v_u64_t special = v_cond_u64 (ia >= One);
  v_f64_t halfsign = v_as_f64_u64 (sign | Half);

  /* Mask special lanes with 0 to prevent spurious underflow.  */
  v_f64_t ax = v_sel_f64 (special, v_f64 (0), v_as_f64_u64 (ia));
  v_f64_t y = halfsign * log1p_inline ((2 * ax) / (1 - ax));

  if (unlikely (v_any_u64 (special)))
    return specialcase (x, y, special);
  return y;
}
VPCS_ALIAS

PL_SIG (V, D, 1, atanh, -1.0, 1.0)
PL_TEST_EXPECT_FENV_ALWAYS (V_NAME (atanh))
PL_TEST_ULP (V_NAME (atanh), 3.32)
PL_TEST_INTERVAL_C (V_NAME (atanh), 0, 0x1p-23, 10000, 0)
PL_TEST_INTERVAL_C (V_NAME (atanh), -0, -0x1p-23, 10000, 0)
PL_TEST_INTERVAL_C (V_NAME (atanh), 0x1p-23, 1, 90000, 0)
PL_TEST_INTERVAL_C (V_NAME (atanh), -0x1p-23, -1, 90000, 0)
PL_TEST_INTERVAL_C (V_NAME (atanh), 1, inf, 100, 0)
PL_TEST_INTERVAL_C (V_NAME (atanh), -1, -inf, 100, 0)
#endif
