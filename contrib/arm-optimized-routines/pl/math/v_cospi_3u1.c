/*
 * Double-precision vector cospi function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "mathlib.h"
#include "v_math.h"
#include "poly_advsimd_f64.h"
#include "pl_sig.h"
#include "pl_test.h"

static const struct data
{
  float64x2_t poly[10];
  float64x2_t range_val;
} data = {
  /* Polynomial coefficients generated using Remez algorithm,
     see sinpi.sollya for details.  */
  .poly = { V2 (0x1.921fb54442d184p1), V2 (-0x1.4abbce625be53p2),
	    V2 (0x1.466bc6775ab16p1), V2 (-0x1.32d2cce62dc33p-1),
	    V2 (0x1.507834891188ep-4), V2 (-0x1.e30750a28c88ep-8),
	    V2 (0x1.e8f48308acda4p-12), V2 (-0x1.6fc0032b3c29fp-16),
	    V2 (0x1.af86ae521260bp-21), V2 (-0x1.012a9870eeb7dp-25) },
  .range_val = V2 (0x1p63),
};

static float64x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t x, float64x2_t y, uint64x2_t odd, uint64x2_t cmp)
{
  /* Fall back to scalar code.  */
  y = vreinterpretq_f64_u64 (veorq_u64 (vreinterpretq_u64_f64 (y), odd));
  return v_call_f64 (cospi, x, y, cmp);
}

/* Approximation for vector double-precision cospi(x).
   Maximum Error 3.06 ULP:
  _ZGVnN2v_cospi(0x1.7dd4c0b03cc66p-5) got 0x1.fa854babfb6bep-1
				      want 0x1.fa854babfb6c1p-1.  */
float64x2_t VPCS_ATTR V_NAME_D1 (cospi) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

#if WANT_SIMD_EXCEPT
  float64x2_t r = vabsq_f64 (x);
  uint64x2_t cmp = vcaleq_f64 (v_f64 (0x1p64), x);

  /* When WANT_SIMD_EXCEPT = 1, special lanes should be zero'd
     to avoid them overflowing and throwing exceptions.  */
  r = v_zerofy_f64 (r, cmp);
  uint64x2_t odd = vshlq_n_u64 (vcvtnq_u64_f64 (r), 63);

#else
  float64x2_t r = x;
  uint64x2_t cmp = vcageq_f64 (r, d->range_val);
  uint64x2_t odd
      = vshlq_n_u64 (vreinterpretq_u64_s64 (vcvtaq_s64_f64 (r)), 63);

#endif

  r = vsubq_f64 (r, vrndaq_f64 (r));

  /* cospi(x) = sinpi(0.5 - abs(x)) for values -1/2 .. 1/2.  */
  r = vsubq_f64 (v_f64 (0.5), vabsq_f64 (r));

  /* y = sin(r).  */
  float64x2_t r2 = vmulq_f64 (r, r);
  float64x2_t r4 = vmulq_f64 (r2, r2);
  float64x2_t y = vmulq_f64 (v_pw_horner_9_f64 (r2, r4, d->poly), r);

  /* Fallback to scalar.  */
  if (unlikely (v_any_u64 (cmp)))
    return special_case (x, y, odd, cmp);

  /* Reintroduce the sign bit for inputs which round to odd.  */
  return vreinterpretq_f64_u64 (veorq_u64 (vreinterpretq_u64_f64 (y), odd));
}

PL_SIG (V, D, 1, cospi, -0.9, 0.9)
PL_TEST_ULP (V_NAME_D1 (cospi), 2.56)
PL_TEST_EXPECT_FENV (V_NAME_D1 (cospi), WANT_SIMD_EXCEPT)
PL_TEST_SYM_INTERVAL (V_NAME_D1 (cospi), 0, 0x1p-63, 5000)
PL_TEST_SYM_INTERVAL (V_NAME_D1 (cospi), 0x1p-63, 0.5, 10000)
PL_TEST_SYM_INTERVAL (V_NAME_D1 (cospi), 0.5, 0x1p51, 10000)
PL_TEST_SYM_INTERVAL (V_NAME_D1 (cospi), 0x1p51, inf, 10000)
