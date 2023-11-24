/*
 * Single-precision vector erf(x) function.
 *
 * Copyright (c) 2020-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "include/mathlib.h"
#include "math_config.h"
#include "pl_sig.h"
#include "pl_test.h"

#if V_SUPPORTED

VPCS_ATTR v_f32_t V_NAME (expf) (v_f32_t);

#define AbsMask v_u32 (0x7fffffff)

/* Special cases (fall back to scalar calls).  */
VPCS_ATTR
NOINLINE static v_f32_t
specialcase (v_f32_t x, v_f32_t y, v_u32_t cmp)
{
  return v_call_f32 (erff, x, y, cmp);
}

/* A structure to perform look-up in coeffs and other parameter tables.  */
struct entry
{
  v_f32_t P[V_ERFF_NCOEFFS];
};

static inline struct entry
lookup (v_u32_t i)
{
  struct entry e;
#ifdef SCALAR
  for (int j = 0; j < V_ERFF_NCOEFFS; ++j)
    e.P[j] = __v_erff_data.coeffs[j][i];
#else
  for (int j = 0; j < V_ERFF_NCOEFFS; ++j)
    {
      e.P[j][0] = __v_erff_data.coeffs[j][i[0]];
      e.P[j][1] = __v_erff_data.coeffs[j][i[1]];
      e.P[j][2] = __v_erff_data.coeffs[j][i[2]];
      e.P[j][3] = __v_erff_data.coeffs[j][i[3]];
    }
#endif
  return e;
}

/* Optimized single precision vector error function erf.
   Maximum measured at +/- 0.931, 1.25ULP:
   v_erff(-0x1.dc59fap-1) got -0x1.9f9c88p-1
			 want -0x1.9f9c8ap-1.  */
VPCS_ATTR
v_f32_t V_NAME (erff) (v_f32_t x)
{
  /* Handle both inf/nan as well as small values (|x|<2^-28). If any condition
     in the lane is true then a loop over scalar calls will be performed.  */
  v_u32_t ix = v_as_u32_f32 (x);
  v_u32_t atop = (ix >> 16) & v_u32 (0x7fff);
  v_u32_t cmp = v_cond_u32 (atop - v_u32 (0x3180) >= v_u32 (0x7ff0 - 0x3180));

  /* Get sign and absolute value.  */
  v_u32_t sign = ix & ~AbsMask;
  /* |x| < 0.921875.  */
  v_u32_t red = v_calt_f32 (x, v_f32 (0.921875f));
  /* |x| > 4.0.  */
  v_u32_t bor = v_cagt_f32 (x, v_f32 (4.0f));
  /* Avoid dependency in abs(x) in division (and comparison).  */
  v_u32_t i = v_sel_u32 (red, v_u32 (0), v_u32 (1));

  /* Get polynomial coefficients.  */
  struct entry dat = lookup (i);

  v_f32_t a = v_abs_f32 (x);
  v_f32_t z = v_sel_f32 (red, x * x, a);

  /* Evaluate Polynomial of |x| or x^2.  */
  v_f32_t r = dat.P[6];
  r = v_fma_f32 (z, r, dat.P[5]);
  r = v_fma_f32 (z, r, dat.P[4]);
  r = v_fma_f32 (z, r, dat.P[3]);
  r = v_fma_f32 (z, r, dat.P[2]);
  r = v_fma_f32 (z, r, dat.P[1]);
  r = v_sel_f32 (red, r, v_fma_f32 (z, r, dat.P[0]));
  r = v_fma_f32 (a, r, a);

  /* y = |x| + |x|*P(|x|)        if |x| < 0.921875
     1 - exp (-(|x|+|x|*P(x^2))) otherwise.  */
  v_f32_t y = v_sel_f32 (red, r, v_f32 (1.0f) - V_NAME (expf) (-r));

  /* Boring domain (absolute value is required to get the sign of erf(-nan)
     right).  */
  y = v_sel_f32 (bor, v_f32 (1.0f), v_abs_f32 (y));

  /* y=erf(x) if x>0, -erf(-x) otherwise.  */
  y = v_as_f32_u32 (v_as_u32_f32 (y) ^ sign);

  if (unlikely (v_any_u32 (cmp)))
    return specialcase (x, y, cmp);
  return y;
}
VPCS_ALIAS

PL_SIG (V, F, 1, erf, -4.0, 4.0)
PL_TEST_ULP (V_NAME (erff), 0.76)
PL_TEST_INTERVAL (V_NAME (erff), 0, 0xffff0000, 10000)
PL_TEST_INTERVAL (V_NAME (erff), 0x1p-127, 0x1p-26, 40000)
PL_TEST_INTERVAL (V_NAME (erff), -0x1p-127, -0x1p-26, 40000)
PL_TEST_INTERVAL (V_NAME (erff), 0x1p-26, 0x1p3, 40000)
PL_TEST_INTERVAL (V_NAME (erff), -0x1p-26, -0x1p3, 40000)
PL_TEST_INTERVAL (V_NAME (erff), 0, inf, 40000)
#endif
