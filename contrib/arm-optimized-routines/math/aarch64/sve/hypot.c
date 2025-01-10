/*
 * Double-precision SVE hypot(x) function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"

static const struct data
{
  uint64_t tiny_bound, thres;
} data = {
  .tiny_bound = 0x0c80000000000000, /* asuint (0x1p-102).  */
  .thres = 0x7300000000000000,	    /* asuint (inf) - tiny_bound.  */
};

static svfloat64_t NOINLINE
special_case (svfloat64_t sqsum, svfloat64_t x, svfloat64_t y, svbool_t pg,
	      svbool_t special)
{
  return sv_call2_f64 (hypot, x, y, svsqrt_x (pg, sqsum), special);
}

/* SVE implementation of double-precision hypot.
   Maximum error observed is 1.21 ULP:
   _ZGVsMxvv_hypot (-0x1.6a22d0412cdd3p+352, 0x1.d3d89bd66fb1ap+330)
    got 0x1.6a22d0412cfp+352
   want 0x1.6a22d0412cf01p+352.  */
svfloat64_t SV_NAME_D2 (hypot) (svfloat64_t x, svfloat64_t y, svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svfloat64_t sqsum = svmla_x (pg, svmul_x (pg, x, x), y, y);

  svbool_t special = svcmpge (
      pg, svsub_x (pg, svreinterpret_u64 (sqsum), d->tiny_bound), d->thres);

  if (unlikely (svptest_any (pg, special)))
    return special_case (sqsum, x, y, pg, special);
  return svsqrt_x (pg, sqsum);
}

TEST_SIG (SV, D, 2, hypot, -10.0, 10.0)
TEST_ULP (SV_NAME_D2 (hypot), 0.71)
TEST_DISABLE_FENV (SV_NAME_D2 (hypot))
TEST_INTERVAL2 (SV_NAME_D2 (hypot), 0, inf, 0, inf, 10000)
TEST_INTERVAL2 (SV_NAME_D2 (hypot), 0, inf, -0, -inf, 10000)
TEST_INTERVAL2 (SV_NAME_D2 (hypot), -0, -inf, 0, inf, 10000)
TEST_INTERVAL2 (SV_NAME_D2 (hypot), -0, -inf, -0, -inf, 10000)
CLOSE_SVE_ATTR
