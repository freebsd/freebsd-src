/*
 * Double-precision inverse error function (AdvSIMD variant).
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "v_math.h"
#include "pl_test.h"
#include "mathlib.h"
#include "math_config.h"
#include "pl_sig.h"
#include "poly_advsimd_f64.h"
#define V_LOG_INLINE_POLY_ORDER 4
#include "v_log_inline.h"

const static struct data
{
  /*  We use P_N and Q_N to refer to arrays of coefficients, where P_N is the
      coeffs of the numerator in table N of Blair et al, and Q_N is the coeffs
      of the denominator. P is interleaved P_17 and P_37, similar for Q. P17
      and Q17 are provided as homogenous vectors as well for when the shortcut
      can be taken.  */
  double P[8][2], Q[7][2];
  float64x2_t tailshift;
  uint8x16_t idx;
  struct v_log_inline_data log_tbl;
  float64x2_t P_57[9], Q_57[10], P_17[7], Q_17[6];
} data = { .P = { { 0x1.007ce8f01b2e8p+4, -0x1.f3596123109edp-7 },
		  { -0x1.6b23cc5c6c6d7p+6, 0x1.60b8fe375999ep-2 },
		  { 0x1.74e5f6ceb3548p+7, -0x1.779bb9bef7c0fp+1 },
		  { -0x1.5200bb15cc6bbp+7, 0x1.786ea384470a2p+3 },
		  { 0x1.05d193233a849p+6, -0x1.6a7c1453c85d3p+4 },
		  { -0x1.148c5474ee5e1p+3, 0x1.31f0fc5613142p+4 },
		  { 0x1.689181bbafd0cp-3, -0x1.5ea6c007d4dbbp+2 },
		  { 0, 0x1.e66f265ce9e5p-3 } },
	   .Q = { { 0x1.d8fb0f913bd7bp+3, -0x1.636b2dcf4edbep-7 },
		  { -0x1.6d7f25a3f1c24p+6, 0x1.0b5411e2acf29p-2 },
		  { 0x1.a450d8e7f4cbbp+7, -0x1.3413109467a0bp+1 },
		  { -0x1.bc3480485857p+7, 0x1.563e8136c554ap+3 },
		  { 0x1.ae6b0c504ee02p+6, -0x1.7b77aab1dcafbp+4 },
		  { -0x1.499dfec1a7f5fp+4, 0x1.8a3e174e05ddcp+4 },
		  { 0x1p+0, -0x1.4075c56404eecp+3 } },
	   .P_57 = { V2 (0x1.b874f9516f7f1p-14), V2 (0x1.5921f2916c1c4p-7),
		     V2 (0x1.145ae7d5b8fa4p-2), V2 (0x1.29d6dcc3b2fb7p+1),
		     V2 (0x1.cabe2209a7985p+2), V2 (0x1.11859f0745c4p+3),
		     V2 (0x1.b7ec7bc6a2ce5p+2), V2 (0x1.d0419e0bb42aep+1),
		     V2 (0x1.c5aa03eef7258p-1) },
	   .Q_57 = { V2 (0x1.b8747e12691f1p-14), V2 (0x1.59240d8ed1e0ap-7),
		     V2 (0x1.14aef2b181e2p-2), V2 (0x1.2cd181bcea52p+1),
		     V2 (0x1.e6e63e0b7aa4cp+2), V2 (0x1.65cf8da94aa3ap+3),
		     V2 (0x1.7e5c787b10a36p+3), V2 (0x1.0626d68b6cea3p+3),
		     V2 (0x1.065c5f193abf6p+2), V2 (0x1p+0) },
	   .P_17 = { V2 (0x1.007ce8f01b2e8p+4), V2 (-0x1.6b23cc5c6c6d7p+6),
		     V2 (0x1.74e5f6ceb3548p+7), V2 (-0x1.5200bb15cc6bbp+7),
		     V2 (0x1.05d193233a849p+6), V2 (-0x1.148c5474ee5e1p+3),
		     V2 (0x1.689181bbafd0cp-3) },
	   .Q_17 = { V2 (0x1.d8fb0f913bd7bp+3), V2 (-0x1.6d7f25a3f1c24p+6),
		     V2 (0x1.a450d8e7f4cbbp+7), V2 (-0x1.bc3480485857p+7),
		     V2 (0x1.ae6b0c504ee02p+6), V2 (-0x1.499dfec1a7f5fp+4) },
	   .tailshift = V2 (-0.87890625),
	   .idx = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
	   .log_tbl = V_LOG_CONSTANTS };

static inline float64x2_t
special (float64x2_t x, const struct data *d)
{
  /* Note erfinv(inf) should return NaN, and erfinv(1) should return Inf.
     By using log here, instead of log1p, we return finite values for both
     these inputs, and values outside [-1, 1]. This is non-compliant, but is an
     acceptable optimisation at Ofast. To get correct behaviour for all finite
     values use the log1p_inline helper on -abs(x) - note that erfinv(inf)
     will still be finite.  */
  float64x2_t t = vnegq_f64 (
      v_log_inline (vsubq_f64 (v_f64 (1), vabsq_f64 (x)), &d->log_tbl));
  t = vdivq_f64 (v_f64 (1), vsqrtq_f64 (t));
  float64x2_t ts = vbslq_f64 (v_u64 (0x7fffffffffffffff), t, x);
  return vdivq_f64 (v_horner_8_f64 (t, d->P_57),
		    vmulq_f64 (ts, v_horner_9_f64 (t, d->Q_57)));
}

static inline float64x2_t
lookup (const double *c, uint8x16_t idx)
{
  float64x2_t x = vld1q_f64 (c);
  return vreinterpretq_f64_u8 (vqtbl1q_u8 (vreinterpretq_u8_f64 (x), idx));
}

static inline float64x2_t VPCS_ATTR
notails (float64x2_t x, const struct data *d)
{
  /* Shortcut when no input is in a tail region - no need to gather shift or
     coefficients.  */
  float64x2_t t = vfmaq_f64 (v_f64 (-0.5625), x, x);
  float64x2_t p = vmulq_f64 (v_horner_6_f64 (t, d->P_17), x);
  float64x2_t q = vaddq_f64 (d->Q_17[5], t);
  for (int i = 4; i >= 0; i--)
    q = vfmaq_f64 (d->Q_17[i], q, t);
  return vdivq_f64 (p, q);
}

/* Vector implementation of Blair et al's rational approximation to inverse
   error function in single-precision. Largest observed error is 24.75 ULP:
   _ZGVnN2v_erfinv(0x1.fc861d81c2ba8p-1) got 0x1.ea05472686625p+0
					want 0x1.ea0547268660cp+0.  */
float64x2_t VPCS_ATTR V_NAME_D1 (erfinv) (float64x2_t x)
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
  uint64x2_t is_tail = vcagtq_f64 (x, v_f64 (0.75));

  if (unlikely (!v_any_u64 (is_tail)))
    /* If input is normally distributed in [-1, 1] then likelihood of this is
       0.75^2 ~= 0.56.  */
    return notails (x, d);

  uint64x2_t extreme_tail = vcagtq_f64 (x, v_f64 (0.9375));

  uint8x16_t off = vandq_u8 (vreinterpretq_u8_u64 (is_tail), vdupq_n_u8 (8));
  uint8x16_t idx = vaddq_u8 (d->idx, off);

  float64x2_t t = vbslq_f64 (is_tail, d->tailshift, v_f64 (-0.5625));
  t = vfmaq_f64 (t, x, x);

  float64x2_t p = lookup (&d->P[7][0], idx);
  /* Last coeff of q is either 0 or 1 - use mask instead of load.  */
  float64x2_t q = vreinterpretq_f64_u64 (
      vandq_u64 (is_tail, vreinterpretq_u64_f64 (v_f64 (1))));
  for (int i = 6; i >= 0; i--)
    {
      p = vfmaq_f64 (lookup (&d->P[i][0], idx), p, t);
      q = vfmaq_f64 (lookup (&d->Q[i][0], idx), q, t);
    }
  p = vmulq_f64 (p, x);

  if (unlikely (v_any_u64 (extreme_tail)))
    return vbslq_f64 (extreme_tail, special (x, d), vdivq_f64 (p, q));

  return vdivq_f64 (p, q);
}

PL_SIG (V, D, 1, erfinv, -0.99, 0.99)
PL_TEST_ULP (V_NAME_D1 (erfinv), 24.8)
/* Test with control lane in each interval.  */
PL_TEST_SYM_INTERVAL_C (V_NAME_D1 (erfinv), 0, 0x1.fffffffffffffp-1, 100000,
			0.5)
PL_TEST_SYM_INTERVAL_C (V_NAME_D1 (erfinv), 0, 0x1.fffffffffffffp-1, 100000,
			0.8)
PL_TEST_SYM_INTERVAL_C (V_NAME_D1 (erfinv), 0, 0x1.fffffffffffffp-1, 100000,
			0.95)
