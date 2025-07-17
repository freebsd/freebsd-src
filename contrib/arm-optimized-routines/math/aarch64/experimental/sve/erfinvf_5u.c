/*
 * Single-precision inverse error function (SVE variant).
 *
 * Copyright (c) 2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"
#include "sv_poly_f32.h"
#include "sv_logf_inline.h"

const static struct data
{
  /*  We use P_N and Q_N to refer to arrays of coefficients, where P_N
      is the coeffs of the numerator in table N of Blair et al, and
      Q_N is the coeffs of the denominator. Coefficients stored in
      interleaved format to support lookup scheme.  */
  float P10_2, P29_3, Q10_2, Q29_2;
  float P10_0, P29_1, P10_1, P29_2;
  float Q10_0, Q29_0, Q10_1, Q29_1;
  float P29_0, P_50[6], Q_50[2], tailshift;
  struct sv_logf_data logf_tbl;
} data = { .P10_0 = -0x1.a31268p+3,
	   .P10_1 = 0x1.ac9048p+4,
	   .P10_2 = -0x1.293ff6p+3,
	   .P29_0 = -0x1.fc0252p-4,
	   .P29_1 = 0x1.119d44p+0,
	   .P29_2 = -0x1.f59ee2p+0,
	   .P29_3 = 0x1.b13626p-2,
	   .Q10_0 = -0x1.8265eep+3,
	   .Q10_1 = 0x1.ef5eaep+4,
	   .Q10_2 = -0x1.12665p+4,
	   .Q29_0 = -0x1.69952p-4,
	   .Q29_1 = 0x1.c7b7d2p-1,
	   .Q29_2 = -0x1.167d7p+1,
	   .P_50 = { 0x1.3d8948p-3, 0x1.61f9eap+0, 0x1.61c6bcp-1,
		     -0x1.20c9f2p+0, 0x1.5c704cp-1, -0x1.50c6bep-3 },
	   .Q_50 = { 0x1.3d7dacp-3, 0x1.629e5p+0 },
	   .tailshift = -0.87890625,
	   .logf_tbl = SV_LOGF_CONSTANTS };

static inline svfloat32_t
special (svbool_t pg, svfloat32_t x, const struct data *d)
{
  svfloat32_t ax = svabs_x (pg, x);
  svfloat32_t t = svdivr_x (
      pg,
      svsqrt_x (pg, svneg_x (pg, sv_logf_inline (pg, svsubr_x (pg, ax, 1),
						 &d->logf_tbl))),
      1);
  svuint32_t sign
      = sveor_x (pg, svreinterpret_u32 (ax), svreinterpret_u32 (x));
  svfloat32_t ts
      = svreinterpret_f32 (svorr_x (pg, sign, svreinterpret_u32 (t)));
  svfloat32_t q
      = svmla_x (pg, sv_f32 (d->Q_50[0]), svadd_x (pg, t, d->Q_50[1]), t);
  return svdiv_x (pg, sv_horner_5_f32_x (pg, t, d->P_50), svmul_x (pg, ts, q));
}

static inline svfloat32_t
notails (svbool_t pg, svfloat32_t x, const struct data *d)
{
  /* Shortcut when no input is in a tail region - no need to gather shift or
     coefficients.  */
  svfloat32_t t = svmad_x (pg, x, x, -0.5625);
  svfloat32_t q = svadd_x (pg, t, d->Q10_2);
  q = svmad_x (pg, t, q, d->Q10_1);
  q = svmad_x (pg, t, q, d->Q10_0);

  svfloat32_t p = svmla_x (pg, sv_f32 (d->P10_1), t, d->P10_2);
  p = svmad_x (pg, p, t, d->P10_0);

  return svdiv_x (pg, svmul_x (pg, x, p), q);
}

/* Vector implementation of Blair et al's rational approximation to inverse
   error function in single-precision. Worst-case error is 4.71 ULP, in the
   tail region:
   _ZGVsMxv_erfinvf(0x1.f84e9ap-1) got 0x1.b8326ap+0
				  want 0x1.b83274p+0.  */
svfloat32_t SV_NAME_F1 (erfinv) (svfloat32_t x, svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* Calculate inverse error using algorithm described in
     J. M. Blair, C. A. Edwards, and J. H. Johnson,
     "Rational Chebyshev approximations for the inverse of the error function",
     Math. Comp. 30, pp. 827--830 (1976).
     https://doi.org/10.1090/S0025-5718-1976-0421040-7.  */

  /* Algorithm has 3 intervals:
     - 'Normal' region [-0.75, 0.75]
     - Tail region [0.75, 0.9375] U [-0.9375, -0.75]
     - Extreme tail [-1, -0.9375] U [0.9375, 1]
     Normal and tail are both rational approximation of similar order on
     shifted input - these are typically performed in parallel using gather
     loads to obtain correct coefficients depending on interval.  */
  svbool_t is_tail = svacge (pg, x, 0.75);
  svbool_t extreme_tail = svacge (pg, x, 0.9375);

  if (likely (!svptest_any (pg, is_tail)))
    return notails (pg, x, d);

  /* Select requisite shift depending on interval: polynomial is evaluated on
     x * x - shift.
     Normal shift = 0.5625
     Tail shift   = 0.87890625.  */
  svfloat32_t t = svmla_x (
      pg, svsel (is_tail, sv_f32 (d->tailshift), sv_f32 (-0.5625)), x, x);

  svuint32_t idx = svdup_u32_z (is_tail, 1);
  svuint32_t idxhi = svadd_x (pg, idx, 2);

  /* Load coeffs in quadwords and select them according to interval.  */
  svfloat32_t pqhi = svld1rq (svptrue_b32 (), &d->P10_2);
  svfloat32_t plo = svld1rq (svptrue_b32 (), &d->P10_0);
  svfloat32_t qlo = svld1rq (svptrue_b32 (), &d->Q10_0);

  svfloat32_t p2 = svtbl (pqhi, idx);
  svfloat32_t p1 = svtbl (plo, idxhi);
  svfloat32_t p0 = svtbl (plo, idx);
  svfloat32_t q0 = svtbl (qlo, idx);
  svfloat32_t q1 = svtbl (qlo, idxhi);
  svfloat32_t q2 = svtbl (pqhi, idxhi);

  svfloat32_t p = svmla_x (pg, p1, p2, t);
  p = svmla_x (pg, p0, p, t);
  /* Tail polynomial has higher order - merge with normal lanes.  */
  p = svmad_m (is_tail, p, t, d->P29_0);
  svfloat32_t y = svmul_x (pg, x, p);

  /* Least significant term of both Q polynomials is 1, so no need to generate
     it.  */
  svfloat32_t q = svadd_x (pg, t, q2);
  q = svmla_x (pg, q1, q, t);
  q = svmla_x (pg, q0, q, t);

  if (unlikely (svptest_any (pg, extreme_tail)))
    return svsel (extreme_tail, special (extreme_tail, x, d),
		  svdiv_x (pg, y, q));
  return svdiv_x (pg, y, q);
}

#if USE_MPFR
# warning Not generating tests for _ZGVsMxv_erfinvf, as MPFR has no suitable reference
#else
TEST_SIG (SV, F, 1, erfinv, -0.99, 0.99)
TEST_ULP (SV_NAME_F1 (erfinv), 4.09)
TEST_DISABLE_FENV (SV_NAME_F1 (erfinv))
TEST_SYM_INTERVAL (SV_NAME_F1 (erfinv), 0, 1, 40000)
TEST_CONTROL_VALUE (SV_NAME_F1 (erfinv), 0.5)
TEST_CONTROL_VALUE (SV_NAME_F1 (erfinv), 0.8)
TEST_CONTROL_VALUE (SV_NAME_F1 (erfinv), 0.95)
#endif
CLOSE_SVE_ATTR
