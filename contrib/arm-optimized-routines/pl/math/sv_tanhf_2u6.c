/*
 * Single-precision SVE tanh(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#include "sv_expm1f_inline.h"

static const struct data
{
  struct sv_expm1f_data expm1f_consts;
  uint32_t boring_bound, onef;
} data = {
  .expm1f_consts = SV_EXPM1F_DATA,
  /* 0x1.205966p+3, above which tanhf rounds to 1 (or -1 for negative).  */
  .boring_bound = 0x41102cb3,
  .onef = 0x3f800000,
};

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svfloat32_t y, svbool_t special)
{
  return sv_call_f32 (tanhf, x, y, special);
}

/* Approximation for single-precision SVE tanh(x), using a simplified
   version of expm1f. The maximum error is 2.57 ULP:
   _ZGVsMxv_tanhf (0x1.fc1832p-5) got 0x1.fb71a4p-5
				 want 0x1.fb71aap-5.  */
svfloat32_t SV_NAME_F1 (tanh) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svfloat32_t ax = svabs_x (pg, x);
  svuint32_t iax = svreinterpret_u32 (ax);
  svuint32_t sign = sveor_x (pg, svreinterpret_u32 (x), iax);
  svbool_t is_boring = svcmpgt (pg, iax, d->boring_bound);
  svfloat32_t boring = svreinterpret_f32 (svorr_x (pg, sign, d->onef));

  svbool_t special = svcmpgt (pg, iax, 0x7f800000);

  /* tanh(x) = (e^2x - 1) / (e^2x + 1).  */
  svfloat32_t q = expm1f_inline (svmul_x (pg, x, 2.0), pg, &d->expm1f_consts);
  svfloat32_t y = svdiv_x (pg, q, svadd_x (pg, q, 2.0));
  if (unlikely (svptest_any (pg, special)))
    return special_case (x, svsel_f32 (is_boring, boring, y), special);
  return svsel_f32 (is_boring, boring, y);
}

PL_SIG (SV, F, 1, tanh, -10.0, 10.0)
PL_TEST_ULP (SV_NAME_F1 (tanh), 2.07)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (tanh), 0, 0x1p-23, 1000)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (tanh), 0x1p-23, 0x1.205966p+3, 100000)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (tanh), 0x1.205966p+3, inf, 100)
