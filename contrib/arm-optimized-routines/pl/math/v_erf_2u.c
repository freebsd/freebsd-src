/*
 * Double-precision vector erf(x) function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "include/mathlib.h"
#include "math_config.h"
#include "pl_sig.h"
#include "pl_test.h"

#if V_SUPPORTED

#define AbsMask v_u64 (0x7fffffffffffffff)
#define AbsXMax v_f64 (0x1.8p+2)
#define Scale v_f64 (0x1p+3)

/* Special cases (fall back to scalar calls).  */
VPCS_ATTR
NOINLINE static v_f64_t
specialcase (v_f64_t x, v_f64_t y, v_u64_t cmp)
{
  return v_call_f64 (erf, x, y, cmp);
}

/* A structure to perform look-up in coeffs and other parameter tables.  */
struct entry
{
  v_f64_t P[V_ERF_NCOEFFS];
  v_f64_t shift;
};

static inline struct entry
lookup (v_u64_t i)
{
  struct entry e;
#ifdef SCALAR
  for (int j = 0; j < V_ERF_NCOEFFS; ++j)
    e.P[j] = __v_erf_data.coeffs[j][i];
  e.shift = __v_erf_data.shifts[i];
#else
  for (int j = 0; j < V_ERF_NCOEFFS; ++j)
    {
      e.P[j][0] = __v_erf_data.coeffs[j][i[0]];
      e.P[j][1] = __v_erf_data.coeffs[j][i[1]];
    }
  e.shift[0] = __v_erf_data.shifts[i[0]];
  e.shift[1] = __v_erf_data.shifts[i[1]];
#endif
  return e;
}

/* Optimized double precision vector error function erf. Maximum
   observed error is 1.75 ULP, in [0.110, 0.111]:
   verf(0x1.c5e0c2d5d0543p-4) got 0x1.fe0ed62a54987p-4
			     want 0x1.fe0ed62a54985p-4.  */
VPCS_ATTR
v_f64_t V_NAME (erf) (v_f64_t x)
{
  /* Handle both inf/nan as well as small values (|x|<2^-28)
     If any condition in the lane is true then a loop over
     scalar calls will be performed.  */
  v_u64_t ix = v_as_u64_f64 (x);
  v_u64_t atop = (ix >> 48) & v_u64 (0x7fff);
  v_u64_t special_case
    = v_cond_u64 (atop - v_u64 (0x3e30) >= v_u64 (0x7ff0 - 0x3e30));

  /* Get sign and absolute value.  */
  v_u64_t sign = v_as_u64_f64 (x) & ~AbsMask;
  v_f64_t a = v_min_f64 (v_abs_f64 (x), AbsXMax);

  /* Compute index by truncating 8 * a with a=|x| saturated to 6.0.  */

#ifdef SCALAR
  v_u64_t i = v_trunc_u64 (a * Scale);
#else
  v_u64_t i = vcvtq_n_u64_f64 (a, 3);
#endif
  /* Get polynomial coefficients and shift parameter using lookup.  */
  struct entry dat = lookup (i);

  /* Evaluate polynomial on transformed argument.  */
  v_f64_t z = v_fma_f64 (a, Scale, dat.shift);

  v_f64_t r1 = v_fma_f64 (z, dat.P[1], dat.P[0]);
  v_f64_t r2 = v_fma_f64 (z, dat.P[3], dat.P[2]);
  v_f64_t r3 = v_fma_f64 (z, dat.P[5], dat.P[4]);
  v_f64_t r4 = v_fma_f64 (z, dat.P[7], dat.P[6]);
  v_f64_t r5 = v_fma_f64 (z, dat.P[9], dat.P[8]);

  v_f64_t z2 = z * z;
  v_f64_t y = v_fma_f64 (z2, r5, r4);
  y = v_fma_f64 (z2, y, r3);
  y = v_fma_f64 (z2, y, r2);
  y = v_fma_f64 (z2, y, r1);

  /* y=erf(x) if x>0, -erf(-x) otherwise.  */
  y = v_as_f64_u64 (v_as_u64_f64 (y) ^ sign);

  if (unlikely (v_any_u64 (special_case)))
    return specialcase (x, y, special_case);
  return y;
}
VPCS_ALIAS

PL_SIG (V, D, 1, erf, -6.0, 6.0)
PL_TEST_ULP (V_NAME (erf), 1.26)
PL_TEST_INTERVAL (V_NAME (erf), 0, 0xffff0000, 10000)
PL_TEST_INTERVAL (V_NAME (erf), 0x1p-127, 0x1p-26, 40000)
PL_TEST_INTERVAL (V_NAME (erf), -0x1p-127, -0x1p-26, 40000)
PL_TEST_INTERVAL (V_NAME (erf), 0x1p-26, 0x1p3, 40000)
PL_TEST_INTERVAL (V_NAME (erf), -0x1p-26, -0x1p3, 40000)
PL_TEST_INTERVAL (V_NAME (erf), 0, inf, 40000)
#endif
