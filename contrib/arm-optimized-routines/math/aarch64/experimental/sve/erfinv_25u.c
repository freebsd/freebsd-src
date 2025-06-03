/*
 * Double-precision inverse error function (SVE variant).
 *
 * Copyright (c) 2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "sv_math.h"
#include "test_defs.h"
#include "math_config.h"
#include "test_sig.h"
#include "sv_poly_f64.h"
#define SV_LOG_INLINE_POLY_ORDER 4
#include "sv_log_inline.h"

const static struct data
{
  /*  We use P_N and Q_N to refer to arrays of coefficients, where P_N is the
      coeffs of the numerator in table N of Blair et al, and Q_N is the coeffs
      of the denominator. P is interleaved P_17 and P_37, similar for Q.  */
  double P[7][2], Q[7][2];
  double P_57[9], Q_57[9], tailshift, P37_0;
  struct sv_log_inline_data log_tbl;
} data = {
  .P37_0 = -0x1.f3596123109edp-7,
  .tailshift = -0.87890625,
  .P = { { 0x1.007ce8f01b2e8p+4, 0x1.60b8fe375999ep-2 },
	 { -0x1.6b23cc5c6c6d7p+6, -0x1.779bb9bef7c0fp+1 },
	 { 0x1.74e5f6ceb3548p+7, 0x1.786ea384470a2p+3 },
	 { -0x1.5200bb15cc6bbp+7, -0x1.6a7c1453c85d3p+4 },
	 { 0x1.05d193233a849p+6, 0x1.31f0fc5613142p+4 },
	 { -0x1.148c5474ee5e1p+3, -0x1.5ea6c007d4dbbp+2 },
	 { 0x1.689181bbafd0cp-3, 0x1.e66f265ce9e5p-3 } },
  .Q = { { 0x1.d8fb0f913bd7bp+3, -0x1.636b2dcf4edbep-7 },
	 { -0x1.6d7f25a3f1c24p+6, 0x1.0b5411e2acf29p-2 },
	 { 0x1.a450d8e7f4cbbp+7, -0x1.3413109467a0bp+1 },
	 { -0x1.bc3480485857p+7, 0x1.563e8136c554ap+3 },
	 { 0x1.ae6b0c504ee02p+6, -0x1.7b77aab1dcafbp+4 },
	 { -0x1.499dfec1a7f5fp+4, 0x1.8a3e174e05ddcp+4 },
	 { 0x1p+0, -0x1.4075c56404eecp+3 } },
  .P_57 = { 0x1.b874f9516f7f1p-14, 0x1.5921f2916c1c4p-7, 0x1.145ae7d5b8fa4p-2,
	    0x1.29d6dcc3b2fb7p+1, 0x1.cabe2209a7985p+2, 0x1.11859f0745c4p+3,
	    0x1.b7ec7bc6a2ce5p+2, 0x1.d0419e0bb42aep+1, 0x1.c5aa03eef7258p-1 },
  .Q_57 = { 0x1.b8747e12691f1p-14, 0x1.59240d8ed1e0ap-7, 0x1.14aef2b181e2p-2,
	    0x1.2cd181bcea52p+1, 0x1.e6e63e0b7aa4cp+2, 0x1.65cf8da94aa3ap+3,
	    0x1.7e5c787b10a36p+3, 0x1.0626d68b6cea3p+3, 0x1.065c5f193abf6p+2 },
  .log_tbl = SV_LOG_CONSTANTS
};

static inline svfloat64_t
special (svbool_t pg, svfloat64_t x, const struct data *d)
{
  /* Note erfinv(inf) should return NaN, and erfinv(1) should return Inf.
     By using log here, instead of log1p, we return finite values for both
     these inputs, and values outside [-1, 1]. This is non-compliant, but is an
     acceptable optimisation at Ofast. To get correct behaviour for all finite
     values use the log1p_inline helper on -abs(x) - note that erfinv(inf)
     will still be finite.  */
  svfloat64_t ax = svabs_x (pg, x);
  svfloat64_t t
      = svneg_x (pg, sv_log_inline (pg, svsubr_x (pg, ax, 1), &d->log_tbl));
  t = svdivr_x (pg, svsqrt_x (pg, t), 1);
  svuint64_t sign
      = sveor_x (pg, svreinterpret_u64 (ax), svreinterpret_u64 (x));
  svfloat64_t ts
      = svreinterpret_f64 (svorr_x (pg, sign, svreinterpret_u64 (t)));

  svfloat64_t q = svadd_x (pg, t, d->Q_57[8]);
  for (int i = 7; i >= 0; i--)
    q = svmad_x (pg, q, t, d->Q_57[i]);

  return svdiv_x (pg, sv_horner_8_f64_x (pg, t, d->P_57), svmul_x (pg, ts, q));
}

static inline svfloat64_t
lookup (const double *c, svuint64_t idx)
{
  svfloat64_t x = svld1rq_f64 (svptrue_b64 (), c);
  return svtbl (x, idx);
}

static inline svfloat64_t
notails (svbool_t pg, svfloat64_t x, const struct data *d)
{
  svfloat64_t t = svmad_x (pg, x, x, -0.5625);
  svfloat64_t p = svmla_x (pg, sv_f64 (d->P[5][0]), t, d->P[6][0]);
  svfloat64_t q = svadd_x (pg, t, d->Q[5][0]);
  for (int i = 4; i >= 0; i--)
    {
      p = svmad_x (pg, t, p, d->P[i][0]);
      q = svmad_x (pg, t, q, d->Q[i][0]);
    }
  p = svmul_x (pg, p, x);
  return svdiv_x (pg, p, q);
}

/* Vector implementation of Blair et al's rational approximation to inverse
   error function in double precision. Largest observed error is 24.75 ULP:
   _ZGVsMxv_erfinv(0x1.fc861d81c2ba8p-1) got 0x1.ea05472686625p+0
					want 0x1.ea0547268660cp+0.  */
svfloat64_t SV_NAME_D1 (erfinv) (svfloat64_t x, svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);
  /* Calculate inverse error using algorithm described in
     J. M. Blair, C. A. Edwards, and J. H. Johnson,
     "Rational Chebyshev approximations for the inverse of the error function",
     Math. Comp. 30, pp. 827--830 (1976).
     https://doi.org/10.1090/S0025-5718-1976-0421040-7.

     Algorithm has 3 intervals:
     - 'Normal' region [-0.75, 0.75]
     - Tail region [0.75, 0.9375] U [-0.9375, -0.75]
     - Extreme tail [-1, -0.9375] U [0.9375, 1]
     Normal and tail are both rational approximation of similar order on
     shifted input - these are typically performed in parallel using gather
     loads to obtain correct coefficients depending on interval.  */

  svbool_t no_tail = svacle (pg, x, 0.75);
  if (unlikely (!svptest_any (pg, svnot_z (pg, no_tail))))
    return notails (pg, x, d);

  svbool_t is_tail = svnot_z (pg, no_tail);
  svbool_t extreme_tail = svacgt (pg, x, 0.9375);
  svuint64_t idx = svdup_n_u64_z (is_tail, 1);

  svfloat64_t t = svsel_f64 (is_tail, sv_f64 (d->tailshift), sv_f64 (-0.5625));
  t = svmla_x (pg, t, x, x);

  svfloat64_t p = lookup (&d->P[6][0], idx);
  svfloat64_t q
      = svmla_x (pg, lookup (&d->Q[6][0], idx), svdup_n_f64_z (is_tail, 1), t);
  for (int i = 5; i >= 0; i--)
    {
      p = svmla_x (pg, lookup (&d->P[i][0], idx), p, t);
      q = svmla_x (pg, lookup (&d->Q[i][0], idx), q, t);
    }
  p = svmad_m (is_tail, p, t, d->P37_0);
  p = svmul_x (pg, p, x);

  if (likely (svptest_any (pg, extreme_tail)))
    return svsel (extreme_tail, special (pg, x, d), svdiv_x (pg, p, q));
  return svdiv_x (pg, p, q);
}

#if USE_MPFR
# warning Not generating tests for _ZGVsMxv_erfinv, as MPFR has no suitable reference
#else
TEST_SIG (SV, D, 1, erfinv, -0.99, 0.99)
TEST_ULP (SV_NAME_D1 (erfinv), 24.5)
TEST_DISABLE_FENV (SV_NAME_D1 (erfinv))
/* Test with control lane in each interval.  */
TEST_SYM_INTERVAL (SV_NAME_F1 (erfinv), 0, 1, 100000)
TEST_CONTROL_VALUE (SV_NAME_F1 (erfinv), 0.5)
TEST_CONTROL_VALUE (SV_NAME_F1 (erfinv), 0.8)
TEST_CONTROL_VALUE (SV_NAME_F1 (erfinv), 0.95)
#endif
CLOSE_SVE_ATTR
