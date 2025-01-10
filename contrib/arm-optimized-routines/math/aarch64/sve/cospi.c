/*
 * Double-precision SVE cospi(x) function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "mathlib.h"
#include "test_sig.h"
#include "test_defs.h"
#include "sv_poly_f64.h"

static const struct data
{
  double poly[10];
  double range_val;
} data = {
  /* Polynomial coefficients generated using Remez algorithm,
     see sinpi.sollya for details.  */
  .poly = { 0x1.921fb54442d184p1, -0x1.4abbce625be53p2, 0x1.466bc6775ab16p1,
	    -0x1.32d2cce62dc33p-1, 0x1.507834891188ep-4, -0x1.e30750a28c88ep-8,
	    0x1.e8f48308acda4p-12, -0x1.6fc0032b3c29fp-16,
	    0x1.af86ae521260bp-21, -0x1.012a9870eeb7dp-25 },
  .range_val = 0x1p53,
};

/* A fast SVE implementation of cospi.
   Maximum error 3.20 ULP:
   _ZGVsMxv_cospi(0x1.f18ba32c63159p-6) got 0x1.fdabf595f9763p-1
				       want 0x1.fdabf595f9766p-1.  */
svfloat64_t SV_NAME_D1 (cospi) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* Using cospi(x) = sinpi(0.5 - x)
     range reduction and offset into sinpi range -1/2 .. 1/2
     r = 0.5 - |x - rint(x)|.  */
  svfloat64_t n = svrinta_x (pg, x);
  svfloat64_t r = svsub_x (pg, x, n);
  r = svsub_x (pg, sv_f64 (0.5), svabs_x (pg, r));

  /* Result should be negated based on if n is odd or not.
     If ax >= 2^53, the result will always be positive.  */
  svbool_t cmp = svaclt (pg, x, d->range_val);
  svuint64_t intn = svreinterpret_u64 (svcvt_s64_z (pg, n));
  svuint64_t sign = svlsl_z (cmp, intn, 63);

  /* y = sin(r).  */
  svfloat64_t r2 = svmul_x (pg, r, r);
  svfloat64_t r4 = svmul_x (pg, r2, r2);
  svfloat64_t y = sv_pw_horner_9_f64_x (pg, r2, r4, d->poly);
  y = svmul_x (pg, y, r);

  return svreinterpret_f64 (sveor_x (pg, svreinterpret_u64 (y), sign));
}

#if WANT_TRIGPI_TESTS
TEST_ULP (SV_NAME_D1 (cospi), 2.71)
TEST_DISABLE_FENV (SV_NAME_D1 (cospi))
TEST_SYM_INTERVAL (SV_NAME_D1 (cospi), 0, 0x1p-63, 5000)
TEST_SYM_INTERVAL (SV_NAME_D1 (cospi), 0x1p-63, 0.5, 10000)
TEST_SYM_INTERVAL (SV_NAME_D1 (cospi), 0.5, 0x1p51, 10000)
TEST_SYM_INTERVAL (SV_NAME_D1 (cospi), 0x1p51, inf, 100000)
#endif
CLOSE_SVE_ATTR
