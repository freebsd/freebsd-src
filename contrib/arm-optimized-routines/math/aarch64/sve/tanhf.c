/*
 * Single-precision SVE tanh(x) function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"
#include "sv_expm1f_inline.h"

/* Largest value of x for which tanhf(x) rounds to 1 (or -1 for negative).  */
#define BoringBound 0x1.205966p+3f

static const struct data
{
  struct sv_expm1f_data expm1f_consts;
  uint32_t onef, special_bound;
  float boring_bound;
} data = {
  .expm1f_consts = SV_EXPM1F_DATA,
  .onef = 0x3f800000,
  .special_bound = 0x7f800000,
  .boring_bound = BoringBound,
};

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svbool_t pg, svbool_t is_boring,
	      svfloat32_t boring, svfloat32_t q, svbool_t special)
{
  svfloat32_t y
      = svsel_f32 (is_boring, boring, svdiv_x (pg, q, svadd_x (pg, q, 2.0)));
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
  svfloat32_t boring = svreinterpret_f32 (svorr_x (pg, sign, d->onef));
  svbool_t special = svcmpgt (pg, iax, d->special_bound);
  svbool_t is_boring = svacgt (pg, x, d->boring_bound);

  /* tanh(x) = (e^2x - 1) / (e^2x + 1).  */
  svfloat32_t q = expm1f_inline (svmul_x (svptrue_b32 (), x, 2.0), pg,
				 &d->expm1f_consts);

  if (unlikely (svptest_any (pg, special)))
    return special_case (x, pg, is_boring, boring, q, special);
  svfloat32_t y = svdiv_x (pg, q, svadd_x (pg, q, 2.0));
  return svsel_f32 (is_boring, boring, y);
}

TEST_SIG (SV, F, 1, tanh, -10.0, 10.0)
TEST_ULP (SV_NAME_F1 (tanh), 2.07)
TEST_DISABLE_FENV (SV_NAME_F1 (tanh))
TEST_SYM_INTERVAL (SV_NAME_F1 (tanh), 0, 0x1p-23, 1000)
TEST_SYM_INTERVAL (SV_NAME_F1 (tanh), 0x1p-23, BoringBound, 100000)
TEST_SYM_INTERVAL (SV_NAME_F1 (tanh), BoringBound, inf, 100)
CLOSE_SVE_ATTR
