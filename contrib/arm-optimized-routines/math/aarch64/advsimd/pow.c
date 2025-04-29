/*
 * Double-precision vector pow function.
 *
 * Copyright (c) 2020-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_sig.h"
#include "test_defs.h"

/* Defines parameters of the approximation and scalar fallback.  */
#include "finite_pow.h"

#define VecSmallPowX v_u64 (SmallPowX)
#define VecThresPowX v_u64 (ThresPowX)
#define VecSmallPowY v_u64 (SmallPowY)
#define VecThresPowY v_u64 (ThresPowY)

static const struct data
{
  uint64x2_t inf;
  float64x2_t small_powx;
  uint64x2_t offset, mask;
  uint64x2_t mask_sub_0, mask_sub_1;
  float64x2_t log_c0, log_c2, log_c4, log_c5;
  double log_c1, log_c3;
  double ln2_lo, ln2_hi;
  uint64x2_t small_exp, thres_exp;
  double ln2_lo_n, ln2_hi_n;
  double inv_ln2_n, exp_c2;
  float64x2_t exp_c0, exp_c1;
} data = {
  /* Power threshold.  */
  .inf = V2 (0x7ff0000000000000),
  .small_powx = V2 (0x1p-126),
  .offset = V2 (Off),
  .mask = V2 (0xfffULL << 52),
  .mask_sub_0 = V2 (1ULL << 52),
  .mask_sub_1 = V2 (52ULL << 52),
  /* Coefficients copied from v_pow_log_data.c
     relative error: 0x1.11922ap-70 in [-0x1.6bp-8, 0x1.6bp-8]
     Coefficients are scaled to match the scaling during evaluation.  */
  .log_c0 = V2 (0x1.555555555556p-2 * -2),
  .log_c1 = -0x1.0000000000006p-2 * -2,
  .log_c2 = V2 (0x1.999999959554ep-3 * 4),
  .log_c3 = -0x1.555555529a47ap-3 * 4,
  .log_c4 = V2 (0x1.2495b9b4845e9p-3 * -8),
  .log_c5 = V2 (-0x1.0002b8b263fc3p-3 * -8),
  .ln2_hi = 0x1.62e42fefa3800p-1,
  .ln2_lo = 0x1.ef35793c76730p-45,
  /* Polynomial coefficients: abs error: 1.43*2^-58, ulp error: 0.549
     (0.550 without fma) if |x| < ln2/512.  */
  .exp_c0 = V2 (0x1.fffffffffffd4p-2),
  .exp_c1 = V2 (0x1.5555571d6ef9p-3),
  .exp_c2 = 0x1.5555576a5adcep-5,
  .small_exp = V2 (0x3c90000000000000),
  .thres_exp = V2 (0x03f0000000000000),
  .inv_ln2_n = 0x1.71547652b82fep8, /* N/ln2.  */
  .ln2_hi_n = 0x1.62e42fefc0000p-9, /* ln2/N.  */
  .ln2_lo_n = -0x1.c610ca86c3899p-45,
};

/* This version implements an algorithm close to scalar pow but
   - does not implement the trick in the exp's specialcase subroutine to avoid
     double-rounding,
   - does not use a tail in the exponential core computation,
   - and pow's exp polynomial order and table bits might differ.

   Maximum measured error is 1.04 ULPs:
   _ZGVnN2vv_pow(0x1.024a3e56b3c3p-136, 0x1.87910248b58acp-13)
     got 0x1.f71162f473251p-1
    want 0x1.f71162f473252p-1.  */

static inline float64x2_t
v_masked_lookup_f64 (const double *table, uint64x2_t i)
{
  return (float64x2_t){
    table[(i[0] >> (52 - V_POW_LOG_TABLE_BITS)) & (N_LOG - 1)],
    table[(i[1] >> (52 - V_POW_LOG_TABLE_BITS)) & (N_LOG - 1)]
  };
}

/* Compute y+TAIL = log(x) where the rounded result is y and TAIL has about
   additional 15 bits precision.  IX is the bit representation of x, but
   normalized in the subnormal range using the sign bit for the exponent.  */
static inline float64x2_t
v_log_inline (uint64x2_t ix, float64x2_t *tail, const struct data *d)
{
  /* x = 2^k z; where z is in range [OFF,2*OFF) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  uint64x2_t tmp = vsubq_u64 (ix, d->offset);
  int64x2_t k = vshrq_n_s64 (vreinterpretq_s64_u64 (tmp), 52);
  uint64x2_t iz = vsubq_u64 (ix, vandq_u64 (tmp, d->mask));
  float64x2_t z = vreinterpretq_f64_u64 (iz);
  float64x2_t kd = vcvtq_f64_s64 (k);
  /* log(x) = k*Ln2 + log(c) + log1p(z/c-1).  */
  float64x2_t invc = v_masked_lookup_f64 (__v_pow_log_data.invc, tmp);
  float64x2_t logc = v_masked_lookup_f64 (__v_pow_log_data.logc, tmp);
  float64x2_t logctail = v_masked_lookup_f64 (__v_pow_log_data.logctail, tmp);
  /* Note: 1/c is j/N or j/N/2 where j is an integer in [N,2N) and
     |z/c - 1| < 1/N, so r = z/c - 1 is exactly representible.  */
  float64x2_t r = vfmaq_f64 (v_f64 (-1.0), z, invc);
  /* k*Ln2 + log(c) + r.  */
  float64x2_t ln2 = vld1q_f64 (&d->ln2_lo);
  float64x2_t t1 = vfmaq_laneq_f64 (logc, kd, ln2, 1);
  float64x2_t t2 = vaddq_f64 (t1, r);
  float64x2_t lo1 = vfmaq_laneq_f64 (logctail, kd, ln2, 0);
  float64x2_t lo2 = vaddq_f64 (vsubq_f64 (t1, t2), r);
  /* Evaluation is optimized assuming superscalar pipelined execution.  */
  float64x2_t ar = vmulq_f64 (v_f64 (-0.5), r);
  float64x2_t ar2 = vmulq_f64 (r, ar);
  float64x2_t ar3 = vmulq_f64 (r, ar2);
  /* k*Ln2 + log(c) + r + A[0]*r*r.  */
  float64x2_t hi = vaddq_f64 (t2, ar2);
  float64x2_t lo3 = vfmaq_f64 (vnegq_f64 (ar2), ar, r);
  float64x2_t lo4 = vaddq_f64 (vsubq_f64 (t2, hi), ar2);
  /* p = log1p(r) - r - A[0]*r*r.  */
  float64x2_t odd_coeffs = vld1q_f64 (&d->log_c1);
  float64x2_t a56 = vfmaq_f64 (d->log_c4, r, d->log_c5);
  float64x2_t a34 = vfmaq_laneq_f64 (d->log_c2, r, odd_coeffs, 1);
  float64x2_t a12 = vfmaq_laneq_f64 (d->log_c0, r, odd_coeffs, 0);
  float64x2_t p = vfmaq_f64 (a34, ar2, a56);
  p = vfmaq_f64 (a12, ar2, p);
  p = vmulq_f64 (ar3, p);
  float64x2_t lo
      = vaddq_f64 (vaddq_f64 (vaddq_f64 (vaddq_f64 (lo1, lo2), lo3), lo4), p);
  float64x2_t y = vaddq_f64 (hi, lo);
  *tail = vaddq_f64 (vsubq_f64 (hi, y), lo);
  return y;
}

static float64x2_t VPCS_ATTR NOINLINE
exp_special_case (float64x2_t x, float64x2_t xtail)
{
  return (float64x2_t){ exp_nosignbias (x[0], xtail[0]),
			exp_nosignbias (x[1], xtail[1]) };
}

/* Computes sign*exp(x+xtail) where |xtail| < 2^-8/N and |xtail| <= |x|.  */
static inline float64x2_t
v_exp_inline (float64x2_t x, float64x2_t neg_xtail, const struct data *d)
{
  /* Fallback to scalar exp_inline for all lanes if any lane
     contains value of x s.t. |x| <= 2^-54 or >= 512.  */
  uint64x2_t uoflowx = vcgeq_u64 (
      vsubq_u64 (vreinterpretq_u64_f64 (vabsq_f64 (x)), d->small_exp),
      d->thres_exp);
  if (unlikely (v_any_u64 (uoflowx)))
    return exp_special_case (x, vnegq_f64 (neg_xtail));

  /* exp(x) = 2^(k/N) * exp(r), with exp(r) in [2^(-1/2N),2^(1/2N)].  */
  /* x = ln2/N*k + r, with k integer and r in [-ln2/2N, ln2/2N].  */
  /* z - kd is in [-1, 1] in non-nearest rounding modes.  */
  float64x2_t exp_consts = vld1q_f64 (&d->inv_ln2_n);
  float64x2_t z = vmulq_laneq_f64 (x, exp_consts, 0);
  float64x2_t kd = vrndnq_f64 (z);
  uint64x2_t ki = vreinterpretq_u64_s64 (vcvtaq_s64_f64 (z));
  float64x2_t ln2_n = vld1q_f64 (&d->ln2_lo_n);
  float64x2_t r = vfmsq_laneq_f64 (x, kd, ln2_n, 1);
  r = vfmsq_laneq_f64 (r, kd, ln2_n, 0);
  /* The code assumes 2^-200 < |xtail| < 2^-8/N.  */
  r = vsubq_f64 (r, neg_xtail);
  /* 2^(k/N) ~= scale.  */
  uint64x2_t idx = vandq_u64 (ki, v_u64 (N_EXP - 1));
  uint64x2_t top = vshlq_n_u64 (ki, 52 - V_POW_EXP_TABLE_BITS);
  /* This is only a valid scale when -1023*N < k < 1024*N.  */
  uint64x2_t sbits = v_lookup_u64 (SBits, idx);
  sbits = vaddq_u64 (sbits, top);
  /* exp(x) = 2^(k/N) * exp(r) ~= scale + scale * (exp(r) - 1).  */
  float64x2_t r2 = vmulq_f64 (r, r);
  float64x2_t tmp = vfmaq_laneq_f64 (d->exp_c1, r, exp_consts, 1);
  tmp = vfmaq_f64 (d->exp_c0, r, tmp);
  tmp = vfmaq_f64 (r, r2, tmp);
  float64x2_t scale = vreinterpretq_f64_u64 (sbits);
  /* Note: tmp == 0 or |tmp| > 2^-200 and scale > 2^-739, so there
     is no spurious underflow here even without fma.  */
  return vfmaq_f64 (scale, scale, tmp);
}

static float64x2_t NOINLINE VPCS_ATTR
scalar_fallback (float64x2_t x, float64x2_t y)
{
  return (float64x2_t){ pow_scalar_special_case (x[0], y[0]),
			pow_scalar_special_case (x[1], y[1]) };
}

float64x2_t VPCS_ATTR V_NAME_D2 (pow) (float64x2_t x, float64x2_t y)
{
  const struct data *d = ptr_barrier (&data);
  /* Case of x <= 0 is too complicated to be vectorised efficiently here,
     fallback to scalar pow for all lanes if any x < 0 detected.  */
  if (v_any_u64 (vclezq_s64 (vreinterpretq_s64_f64 (x))))
    return scalar_fallback (x, y);

  uint64x2_t vix = vreinterpretq_u64_f64 (x);
  uint64x2_t viy = vreinterpretq_u64_f64 (y);
  uint64x2_t iay = vandq_u64 (viy, d->inf);

  /* Special cases of x or y.  */
#if WANT_SIMD_EXCEPT
  /* Small or large.  */
  uint64x2_t vtopx = vshrq_n_u64 (vix, 52);
  uint64x2_t vabstopy = vshrq_n_u64 (iay, 52);
  uint64x2_t specialx
      = vcgeq_u64 (vsubq_u64 (vtopx, VecSmallPowX), VecThresPowX);
  uint64x2_t specialy
      = vcgeq_u64 (vsubq_u64 (vabstopy, VecSmallPowY), VecThresPowY);
#else
  /* The case y==0 does not trigger a special case, since in this case it is
     necessary to fix the result only if x is a signalling nan, which already
     triggers a special case. We test y==0 directly in the scalar fallback.  */
  uint64x2_t iax = vandq_u64 (vix, d->inf);
  uint64x2_t specialx = vcgeq_u64 (iax, d->inf);
  uint64x2_t specialy = vcgeq_u64 (iay, d->inf);
#endif
  uint64x2_t special = vorrq_u64 (specialx, specialy);
  /* Fallback to scalar on all lanes if any lane is inf or nan.  */
  if (unlikely (v_any_u64 (special)))
    return scalar_fallback (x, y);

  /* Small cases of x: |x| < 0x1p-126.  */
  uint64x2_t smallx = vcaltq_f64 (x, d->small_powx);
  if (unlikely (v_any_u64 (smallx)))
    {
      /* Update ix if top 12 bits of x are 0.  */
      uint64x2_t sub_x = vceqzq_u64 (vshrq_n_u64 (vix, 52));
      if (unlikely (v_any_u64 (sub_x)))
	{
	  /* Normalize subnormal x so exponent becomes negative.  */
	  uint64x2_t vix_norm = vreinterpretq_u64_f64 (
	      vabsq_f64 (vmulq_f64 (x, vcvtq_f64_u64 (d->mask_sub_0))));
	  vix_norm = vsubq_u64 (vix_norm, d->mask_sub_1);
	  vix = vbslq_u64 (sub_x, vix_norm, vix);
	}
    }

  /* Vector Log(ix, &lo).  */
  float64x2_t vlo;
  float64x2_t vhi = v_log_inline (vix, &vlo, d);

  /* Vector Exp(y_loghi, y_loglo).  */
  float64x2_t vehi = vmulq_f64 (y, vhi);
  float64x2_t vemi = vfmsq_f64 (vehi, y, vhi);
  float64x2_t neg_velo = vfmsq_f64 (vemi, y, vlo);
  return v_exp_inline (vehi, neg_velo, d);
}

TEST_SIG (V, D, 2, pow)
TEST_ULP (V_NAME_D2 (pow), 0.55)
TEST_DISABLE_FENV_IF_NOT (V_NAME_D2 (pow), WANT_SIMD_EXCEPT)
/* Wide intervals spanning the whole domain but shared between x and y.  */
#define V_POW_INTERVAL2(xlo, xhi, ylo, yhi, n)                                \
  TEST_INTERVAL2 (V_NAME_D2 (pow), xlo, xhi, ylo, yhi, n)                     \
  TEST_INTERVAL2 (V_NAME_D2 (pow), xlo, xhi, -ylo, -yhi, n)                   \
  TEST_INTERVAL2 (V_NAME_D2 (pow), -xlo, -xhi, ylo, yhi, n)                   \
  TEST_INTERVAL2 (V_NAME_D2 (pow), -xlo, -xhi, -ylo, -yhi, n)
#define EXPAND(str) str##000000000
#define SHL52(str) EXPAND (str)
V_POW_INTERVAL2 (0, SHL52 (SmallPowX), 0, inf, 40000)
V_POW_INTERVAL2 (SHL52 (SmallPowX), SHL52 (BigPowX), 0, inf, 40000)
V_POW_INTERVAL2 (SHL52 (BigPowX), inf, 0, inf, 40000)
V_POW_INTERVAL2 (0, inf, 0, SHL52 (SmallPowY), 40000)
V_POW_INTERVAL2 (0, inf, SHL52 (SmallPowY), SHL52 (BigPowY), 40000)
V_POW_INTERVAL2 (0, inf, SHL52 (BigPowY), inf, 40000)
V_POW_INTERVAL2 (0, inf, 0, inf, 1000)
/* x~1 or y~1.  */
V_POW_INTERVAL2 (0x1p-1, 0x1p1, 0x1p-10, 0x1p10, 10000)
V_POW_INTERVAL2 (0x1p-500, 0x1p500, 0x1p-1, 0x1p1, 10000)
V_POW_INTERVAL2 (0x1.ep-1, 0x1.1p0, 0x1p8, 0x1p16, 10000)
/* around argmaxs of ULP error.  */
V_POW_INTERVAL2 (0x1p-300, 0x1p-200, 0x1p-20, 0x1p-10, 10000)
V_POW_INTERVAL2 (0x1p50, 0x1p100, 0x1p-20, 0x1p-10, 10000)
/* x is negative, y is odd or even integer, or y is real not integer.  */
TEST_INTERVAL2 (V_NAME_D2 (pow), -0.0, -10.0, 3.0, 3.0, 10000)
TEST_INTERVAL2 (V_NAME_D2 (pow), -0.0, -10.0, 4.0, 4.0, 10000)
TEST_INTERVAL2 (V_NAME_D2 (pow), -0.0, -10.0, 0.0, 10.0, 10000)
TEST_INTERVAL2 (V_NAME_D2 (pow), 0.0, 10.0, -0.0, -10.0, 10000)
/* 1.0^y.  */
TEST_INTERVAL2 (V_NAME_D2 (pow), 1.0, 1.0, 0.0, 0x1p-50, 1000)
TEST_INTERVAL2 (V_NAME_D2 (pow), 1.0, 1.0, 0x1p-50, 1.0, 1000)
TEST_INTERVAL2 (V_NAME_D2 (pow), 1.0, 1.0, 1.0, 0x1p100, 1000)
TEST_INTERVAL2 (V_NAME_D2 (pow), 1.0, 1.0, -1.0, -0x1p120, 1000)
