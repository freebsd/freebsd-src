/*
 * Single-precision vector e^x function.
 *
 * Copyright (c) 2019-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"
#include "sv_expf_inline.h"

/* Roughly 87.3. For x < -Thres, the result is subnormal and not handled
   correctly by FEXPA.  */
#define Thres 0x1.5d5e2ap+6f

static const struct data
{
  struct sv_expf_data d;
  float thres;
} data = {
  .d = SV_EXPF_DATA,
  .thres = Thres,
};

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svbool_t special, const struct sv_expf_data *d)
{
  return sv_call_f32 (expf, x, expf_inline (x, svptrue_b32 (), d), special);
}

/* Optimised single-precision SVE exp function.
   Worst-case error is 1.04 ulp:
   SV_NAME_F1 (exp)(0x1.a8eda4p+1) got 0x1.ba74bcp+4
				  want 0x1.ba74bap+4.  */
svfloat32_t SV_NAME_F1 (exp) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);
  svbool_t is_special_case = svacgt (pg, x, d->thres);
  if (unlikely (svptest_any (pg, is_special_case)))
    return special_case (x, is_special_case, &d->d);
  return expf_inline (x, pg, &d->d);
}

TEST_SIG (SV, F, 1, exp, -9.9, 9.9)
TEST_ULP (SV_NAME_F1 (exp), 0.55)
TEST_DISABLE_FENV (SV_NAME_F1 (exp))
TEST_SYM_INTERVAL (SV_NAME_F1 (exp), 0, Thres, 50000)
TEST_SYM_INTERVAL (SV_NAME_F1 (exp), Thres, inf, 50000)
CLOSE_SVE_ATTR
