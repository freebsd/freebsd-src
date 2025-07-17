/*
 * Single-precision inverse error function (AdvSIMD variant).
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "v_math.h"
#include "test_sig.h"
#include "test_defs.h"
#include "v_poly_f32.h"
#include "v_logf_inline.h"

const static struct data
{
  /*  We use P_N and Q_N to refer to arrays of coefficients, where P_N is the
      coeffs of the numerator in table N of Blair et al, and Q_N is the coeffs
      of the denominator. Coefficients are stored in various interleaved
      formats to allow for table-based (vector-to-vector) lookup.

      Plo is first two coefficients of P_10 and P_29 interleaved.
      PQ is third coeff of P_10 and first of Q_29 interleaved.
      Qhi is second and third coeffs of Q_29 interleaved.
      P29_3 is a homogenous vector with fourth coeff of P_29.

      P_10 and Q_10 are also stored in homogenous vectors to allow better
      memory access when no lanes are in a tail region.  */
  float Plo[4], PQ[4], Qhi[4];
  float32x4_t P29_3, tailshift;
  float32x4_t P_50[6], Q_50[2];
  float32x4_t P_10[3], Q_10[3];
  uint8_t idxhi[16], idxlo[16];
  struct v_logf_data logf_tbl;
} data = {
  .idxlo = { 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3 },
  .idxhi = { 8, 9, 10, 11, 8, 9, 10, 11, 8, 9, 10, 11, 8, 9, 10, 11 },
  .P29_3 = V4 (0x1.b13626p-2),
  .tailshift = V4 (-0.87890625),
  .Plo = { -0x1.a31268p+3, -0x1.fc0252p-4, 0x1.ac9048p+4, 0x1.119d44p+0 },
  .PQ = { -0x1.293ff6p+3, -0x1.f59ee2p+0, -0x1.8265eep+3, -0x1.69952p-4 },
  .Qhi = { 0x1.ef5eaep+4, 0x1.c7b7d2p-1, -0x1.12665p+4, -0x1.167d7p+1 },
  .P_50 = { V4 (0x1.3d8948p-3), V4 (0x1.61f9eap+0), V4 (0x1.61c6bcp-1),
	    V4 (-0x1.20c9f2p+0), V4 (0x1.5c704cp-1), V4 (-0x1.50c6bep-3) },
  .Q_50 = { V4 (0x1.3d7dacp-3), V4 (0x1.629e5p+0) },
  .P_10 = { V4 (-0x1.a31268p+3), V4 (0x1.ac9048p+4), V4 (-0x1.293ff6p+3) },
  .Q_10 = { V4 (-0x1.8265eep+3), V4 (0x1.ef5eaep+4), V4 (-0x1.12665p+4) },
  .logf_tbl = V_LOGF_CONSTANTS
};

static inline float32x4_t
special (float32x4_t x, const struct data *d)
{
  /* Note erfinvf(inf) should return NaN, and erfinvf(1) should return Inf.
     By using log here, instead of log1p, we return finite values for both
     these inputs, and values outside [-1, 1]. This is non-compliant, but is an
     acceptable optimisation at Ofast. To get correct behaviour for all finite
     values use the log1pf_inline helper on -abs(x) - note that erfinvf(inf)
     will still be finite.  */
  float32x4_t t = vdivq_f32 (
      v_f32 (1), vsqrtq_f32 (vnegq_f32 (v_logf_inline (
		     vsubq_f32 (v_f32 (1), vabsq_f32 (x)), &d->logf_tbl))));
  float32x4_t ts = vbslq_f32 (v_u32 (0x7fffffff), t, x);
  float32x4_t q = vfmaq_f32 (d->Q_50[0], vaddq_f32 (t, d->Q_50[1]), t);
  return vdivq_f32 (v_horner_5_f32 (t, d->P_50), vmulq_f32 (ts, q));
}

static inline float32x4_t
notails (float32x4_t x, const struct data *d)
{
  /* Shortcut when no input is in a tail region - no need to gather shift or
     coefficients.  */
  float32x4_t t = vfmaq_f32 (v_f32 (-0.5625), x, x);
  float32x4_t q = vaddq_f32 (t, d->Q_10[2]);
  q = vfmaq_f32 (d->Q_10[1], t, q);
  q = vfmaq_f32 (d->Q_10[0], t, q);

  return vdivq_f32 (vmulq_f32 (x, v_horner_2_f32 (t, d->P_10)), q);
}

static inline float32x4_t
lookup (float32x4_t tbl, uint8x16_t idx)
{
  return vreinterpretq_f32_u8 (vqtbl1q_u8 (vreinterpretq_u8_f32 (tbl), idx));
}

/* Vector implementation of Blair et al's rational approximation to inverse
   error function in single-precision. Worst-case error is 4.98 ULP, in the
   tail region:
   _ZGVnN4v_erfinvf(0x1.f7dbeep-1) got 0x1.b4793p+0
				  want 0x1.b4793ap+0 .  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (erfinv) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  /* Calculate inverse error using algorithm described in
     J. M. Blair, C. A. Edwards, and J. H. Johnson,
     "Rational Chebyshev approximations for the inverse of the error
      function", Math. Comp. 30, pp. 827--830 (1976).
     https://doi.org/10.1090/S0025-5718-1976-0421040-7.

    Algorithm has 3 intervals:
     - 'Normal' region [-0.75, 0.75]
     - Tail region [0.75, 0.9375] U [-0.9375, -0.75]
     - Extreme tail [-1, -0.9375] U [0.9375, 1]
     Normal and tail are both rational approximation of similar order on
     shifted input - these are typically performed in parallel using gather
     loads to obtain correct coefficients depending on interval.  */
  uint32x4_t is_tail = vcageq_f32 (x, v_f32 (0.75));
  uint32x4_t extreme_tail = vcageq_f32 (x, v_f32 (0.9375));

  if (unlikely (!v_any_u32 (is_tail)))
    /* Shortcut for if all lanes are in [-0.75, 0.75] - can avoid having to
       gather coefficients. If input is uniform in [-1, 1] then likelihood of
       this is 0.75^4 ~= 0.31.  */
    return notails (x, d);

  /* Select requisite shift depending on interval: polynomial is evaluated on
     x * x - shift.
     Normal shift = 0.5625
     Tail shift   = 0.87890625.  */
  float32x4_t t
      = vfmaq_f32 (vbslq_f32 (is_tail, d->tailshift, v_f32 (-0.5625)), x, x);

  /* Calculate indexes for tbl: tbl is byte-wise, so:
     [0, 1, 2, 3, 4, 5, 6, ....] copies the vector
     Add 4 * i to a group of 4 lanes to copy 32-bit lane i. Each vector stores
     two pairs of coeffs, so we need two idx vectors - one for each pair.  */
  uint8x16_t off = vandq_u8 (vreinterpretq_u8_u32 (is_tail), vdupq_n_u8 (4));
  uint8x16_t idx_lo = vaddq_u8 (vld1q_u8 (d->idxlo), off);
  uint8x16_t idx_hi = vaddq_u8 (vld1q_u8 (d->idxhi), off);

  /* Load the tables.  */
  float32x4_t plo = vld1q_f32 (d->Plo);
  float32x4_t pq = vld1q_f32 (d->PQ);
  float32x4_t qhi = vld1q_f32 (d->Qhi);

  /* Do the lookup (and calculate p3 by masking non-tail lanes).  */
  float32x4_t p3 = vreinterpretq_f32_u32 (
      vandq_u32 (is_tail, vreinterpretq_u32_f32 (d->P29_3)));
  float32x4_t p0 = lookup (plo, idx_lo), p1 = lookup (plo, idx_hi),
	      p2 = lookup (pq, idx_lo), q0 = lookup (pq, idx_hi),
	      q1 = lookup (qhi, idx_lo), q2 = lookup (qhi, idx_hi);

  float32x4_t p = vfmaq_f32 (p2, p3, t);
  p = vfmaq_f32 (p1, p, t);
  p = vfmaq_f32 (p0, p, t);
  p = vmulq_f32 (x, p);

  float32x4_t q = vfmaq_f32 (q1, vaddq_f32 (q2, t), t);
  q = vfmaq_f32 (q0, q, t);

  if (unlikely (v_any_u32 (extreme_tail)))
    /* At least one lane is in the extreme tail - if input is uniform in
       [-1, 1] the likelihood of this is ~0.23.  */
    return vbslq_f32 (extreme_tail, special (x, d), vdivq_f32 (p, q));

  return vdivq_f32 (p, q);
}

HALF_WIDTH_ALIAS_F1 (erfinv)

#if USE_MPFR
# warning Not generating tests for _ZGVnN4v_erfinvf, as MPFR has no suitable reference
#else
TEST_SIG (V, F, 1, erfinv, -0.99, 0.99)
TEST_DISABLE_FENV (V_NAME_F1 (erfinv))
TEST_ULP (V_NAME_F1 (erfinv), 4.49)
TEST_SYM_INTERVAL (V_NAME_F1 (erfinv), 0, 0x1.fffffep-1, 40000)
/* Test with control lane in each interval.  */
TEST_CONTROL_VALUE (V_NAME_F1 (erfinv), 0.5)
TEST_CONTROL_VALUE (V_NAME_F1 (erfinv), 0.8)
TEST_CONTROL_VALUE (V_NAME_F1 (erfinv), 0.95)
#endif
