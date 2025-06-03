/*
 * Double-precision SVE pow(x, y) function.
 *
 * Copyright (c) 2022-2025, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"

/* This version share a similar algorithm as AOR scalar pow.

   The core computation consists in computing pow(x, y) as

     exp (y * log (x)).

   The algorithms for exp and log are very similar to scalar exp and log.
   The log relies on table lookup for 3 variables and an order 8 polynomial.
   It returns a high and a low contribution that are then passed to the exp,
   to minimise the loss of accuracy in both routines.
   The exp is based on 8-bit table lookup for scale and order-4 polynomial.
   The SVE algorithm drops the tail in the exp computation at the price of
   a lower accuracy, slightly above 1ULP.
   The SVE algorithm also drops the special treatement of small (< 2^-65) and
   large (> 2^63) finite values of |y|, as they only affect non-round to
   nearest modes.

   Maximum measured error is 1.04 ULPs:
   SV_NAME_D2 (pow) (0x1.3d2d45bc848acp+63, -0x1.a48a38b40cd43p-12)
     got 0x1.f7116284221fcp-1
    want 0x1.f7116284221fdp-1.  */

/* Data is defined in v_pow_log_data.c.  */
#define N_LOG (1 << V_POW_LOG_TABLE_BITS)
#define Off 0x3fe6955500000000

/* Data is defined in v_pow_exp_data.c.  */
#define N_EXP (1 << V_POW_EXP_TABLE_BITS)
#define SignBias (0x800 << V_POW_EXP_TABLE_BITS)
#define SmallExp 0x3c9 /* top12(0x1p-54).  */
#define BigExp 0x408   /* top12(512.).  */
#define ThresExp 0x03f /* BigExp - SmallExp.  */
#define HugeExp 0x409  /* top12(1024.).  */

/* Constants associated with pow.  */
#define SmallBoundX 0x1p-126
#define SmallPowX 0x001 /* top12(0x1p-126).  */
#define BigPowX 0x7ff	/* top12(INFINITY).  */
#define ThresPowX 0x7fe /* BigPowX - SmallPowX.  */
#define SmallPowY 0x3be /* top12(0x1.e7b6p-65).  */
#define BigPowY 0x43e	/* top12(0x1.749p62).  */
#define ThresPowY 0x080 /* BigPowY - SmallPowY.  */

static const struct data
{
  double log_c0, log_c2, log_c4, log_c6, ln2_hi, ln2_lo;
  double log_c1, log_c3, log_c5, off;
  double n_over_ln2, exp_c2, ln2_over_n_hi, ln2_over_n_lo;
  double exp_c0, exp_c1;
} data = {
  .log_c0 = -0x1p-1,
  .log_c1 = -0x1.555555555556p-1,
  .log_c2 = 0x1.0000000000006p-1,
  .log_c3 = 0x1.999999959554ep-1,
  .log_c4 = -0x1.555555529a47ap-1,
  .log_c5 = -0x1.2495b9b4845e9p0,
  .log_c6 = 0x1.0002b8b263fc3p0,
  .off = Off,
  .exp_c0 = 0x1.fffffffffffd4p-2,
  .exp_c1 = 0x1.5555571d6ef9p-3,
  .exp_c2 = 0x1.5555576a5adcep-5,
  .ln2_hi = 0x1.62e42fefa3800p-1,
  .ln2_lo = 0x1.ef35793c76730p-45,
  .n_over_ln2 = 0x1.71547652b82fep0 * N_EXP,
  .ln2_over_n_hi = 0x1.62e42fefc0000p-9,
  .ln2_over_n_lo = -0x1.c610ca86c3899p-45,
};

/* Check if x is an integer.  */
static inline svbool_t
sv_isint (svbool_t pg, svfloat64_t x)
{
  return svcmpeq (pg, svrintz_z (pg, x), x);
}

/* Check if x is real not integer valued.  */
static inline svbool_t
sv_isnotint (svbool_t pg, svfloat64_t x)
{
  return svcmpne (pg, svrintz_z (pg, x), x);
}

/* Check if x is an odd integer.  */
static inline svbool_t
sv_isodd (svbool_t pg, svfloat64_t x)
{
  svfloat64_t y = svmul_x (svptrue_b64 (), x, 0.5);
  return sv_isnotint (pg, y);
}

/* Returns 0 if not int, 1 if odd int, 2 if even int.  The argument is
   the bit representation of a non-zero finite floating-point value.  */
static inline int
checkint (uint64_t iy)
{
  int e = iy >> 52 & 0x7ff;
  if (e < 0x3ff)
    return 0;
  if (e > 0x3ff + 52)
    return 2;
  if (iy & ((1ULL << (0x3ff + 52 - e)) - 1))
    return 0;
  if (iy & (1ULL << (0x3ff + 52 - e)))
    return 1;
  return 2;
}

/* Top 12 bits (sign and exponent of each double float lane).  */
static inline svuint64_t
sv_top12 (svfloat64_t x)
{
  return svlsr_x (svptrue_b64 (), svreinterpret_u64 (x), 52);
}

/* Returns 1 if input is the bit representation of 0, infinity or nan.  */
static inline int
zeroinfnan (uint64_t i)
{
  return 2 * i - 1 >= 2 * asuint64 (INFINITY) - 1;
}

/* Returns 1 if input is the bit representation of 0, infinity or nan.  */
static inline svbool_t
sv_zeroinfnan (svbool_t pg, svuint64_t i)
{
  return svcmpge (pg, svsub_x (pg, svadd_x (pg, i, i), 1),
		  2 * asuint64 (INFINITY) - 1);
}

/* Handle cases that may overflow or underflow when computing the result that
   is scale*(1+TMP) without intermediate rounding.  The bit representation of
   scale is in SBITS, however it has a computed exponent that may have
   overflown into the sign bit so that needs to be adjusted before using it as
   a double.  (int32_t)KI is the k used in the argument reduction and exponent
   adjustment of scale, positive k here means the result may overflow and
   negative k means the result may underflow.  */
static inline double
specialcase (double tmp, uint64_t sbits, uint64_t ki)
{
  double scale;
  if ((ki & 0x80000000) == 0)
    {
      /* k > 0, the exponent of scale might have overflowed by <= 460.  */
      sbits -= 1009ull << 52;
      scale = asdouble (sbits);
      return 0x1p1009 * (scale + scale * tmp);
    }
  /* k < 0, need special care in the subnormal range.  */
  sbits += 1022ull << 52;
  /* Note: sbits is signed scale.  */
  scale = asdouble (sbits);
  double y = scale + scale * tmp;
  return 0x1p-1022 * y;
}

/* Scalar fallback for special cases of SVE pow's exp.  */
static inline svfloat64_t
sv_call_specialcase (svfloat64_t x1, svuint64_t u1, svuint64_t u2,
		     svfloat64_t y, svbool_t cmp)
{
  svbool_t p = svpfirst (cmp, svpfalse ());
  while (svptest_any (cmp, p))
    {
      double sx1 = svclastb (p, 0, x1);
      uint64_t su1 = svclastb (p, 0, u1);
      uint64_t su2 = svclastb (p, 0, u2);
      double elem = specialcase (sx1, su1, su2);
      svfloat64_t y2 = sv_f64 (elem);
      y = svsel (p, y2, y);
      p = svpnext_b64 (cmp, p);
    }
  return y;
}

/* Compute y+TAIL = log(x) where the rounded result is y and TAIL has about
   additional 15 bits precision.  IX is the bit representation of x, but
   normalized in the subnormal range using the sign bit for the exponent.  */
static inline svfloat64_t
sv_log_inline (svbool_t pg, svuint64_t ix, svfloat64_t *tail,
	       const struct data *d)
{
  /* x = 2^k z; where z is in range [Off,2*Off) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  svuint64_t tmp = svsub_x (pg, ix, d->off);
  svuint64_t i = svand_x (pg, svlsr_x (pg, tmp, 52 - V_POW_LOG_TABLE_BITS),
			  sv_u64 (N_LOG - 1));
  svint64_t k = svasr_x (pg, svreinterpret_s64 (tmp), 52);
  svuint64_t iz = svsub_x (pg, ix, svlsl_x (pg, svreinterpret_u64 (k), 52));
  svfloat64_t z = svreinterpret_f64 (iz);
  svfloat64_t kd = svcvt_f64_x (pg, k);

  /* log(x) = k*Ln2 + log(c) + log1p(z/c-1).  */
  /* SVE lookup requires 3 separate lookup tables, as opposed to scalar version
     that uses array of structures. We also do the lookup earlier in the code
     to make sure it finishes as early as possible.  */
  svfloat64_t invc = svld1_gather_index (pg, __v_pow_log_data.invc, i);
  svfloat64_t logc = svld1_gather_index (pg, __v_pow_log_data.logc, i);
  svfloat64_t logctail = svld1_gather_index (pg, __v_pow_log_data.logctail, i);

  /* Note: 1/c is j/N or j/N/2 where j is an integer in [N,2N) and
     |z/c - 1| < 1/N, so r = z/c - 1 is exactly representible.  */
  svfloat64_t r = svmad_x (pg, z, invc, -1.0);
  /* k*Ln2 + log(c) + r.  */

  svfloat64_t ln2_hilo = svld1rq_f64 (svptrue_b64 (), &d->ln2_hi);
  svfloat64_t t1 = svmla_lane_f64 (logc, kd, ln2_hilo, 0);
  svfloat64_t t2 = svadd_x (pg, t1, r);
  svfloat64_t lo1 = svmla_lane_f64 (logctail, kd, ln2_hilo, 1);
  svfloat64_t lo2 = svadd_x (pg, svsub_x (pg, t1, t2), r);

  /* Evaluation is optimized assuming superscalar pipelined execution.  */

  svfloat64_t log_c02 = svld1rq_f64 (svptrue_b64 (), &d->log_c0);
  svfloat64_t ar = svmul_lane_f64 (r, log_c02, 0);
  svfloat64_t ar2 = svmul_x (svptrue_b64 (), r, ar);
  svfloat64_t ar3 = svmul_x (svptrue_b64 (), r, ar2);
  /* k*Ln2 + log(c) + r + A[0]*r*r.  */
  svfloat64_t hi = svadd_x (pg, t2, ar2);
  svfloat64_t lo3 = svmls_x (pg, ar2, ar, r);
  svfloat64_t lo4 = svadd_x (pg, svsub_x (pg, t2, hi), ar2);
  /* p = log1p(r) - r - A[0]*r*r.  */
  /* p = (ar3 * (A[1] + r * A[2] + ar2 * (A[3] + r * A[4] + ar2 * (A[5] + r *
     A[6])))).  */

  svfloat64_t log_c46 = svld1rq_f64 (svptrue_b64 (), &d->log_c4);
  svfloat64_t a56 = svmla_lane_f64 (sv_f64 (d->log_c5), r, log_c46, 1);
  svfloat64_t a34 = svmla_lane_f64 (sv_f64 (d->log_c3), r, log_c46, 0);
  svfloat64_t a12 = svmla_lane_f64 (sv_f64 (d->log_c1), r, log_c02, 1);
  svfloat64_t p = svmla_x (pg, a34, ar2, a56);
  p = svmla_x (pg, a12, ar2, p);
  p = svmul_x (svptrue_b64 (), ar3, p);
  svfloat64_t lo = svadd_x (
      pg, svadd_x (pg, svsub_x (pg, svadd_x (pg, lo1, lo2), lo3), lo4), p);
  svfloat64_t y = svadd_x (pg, hi, lo);
  *tail = svadd_x (pg, svsub_x (pg, hi, y), lo);
  return y;
}

static inline svfloat64_t
sv_exp_core (svbool_t pg, svfloat64_t x, svfloat64_t xtail,
	     svuint64_t sign_bias, svfloat64_t *tmp, svuint64_t *sbits,
	     svuint64_t *ki, const struct data *d)
{
  /* exp(x) = 2^(k/N) * exp(r), with exp(r) in [2^(-1/2N),2^(1/2N)].  */
  /* x = ln2/N*k + r, with int k and r in [-ln2/2N, ln2/2N].  */
  svfloat64_t n_over_ln2_and_c2 = svld1rq_f64 (svptrue_b64 (), &d->n_over_ln2);
  svfloat64_t z = svmul_lane_f64 (x, n_over_ln2_and_c2, 0);
  /* z - kd is in [-1, 1] in non-nearest rounding modes.  */
  svfloat64_t kd = svrinta_x (pg, z);
  *ki = svreinterpret_u64 (svcvt_s64_x (pg, kd));

  svfloat64_t ln2_over_n_hilo
      = svld1rq_f64 (svptrue_b64 (), &d->ln2_over_n_hi);
  svfloat64_t r = x;
  r = svmls_lane_f64 (r, kd, ln2_over_n_hilo, 0);
  r = svmls_lane_f64 (r, kd, ln2_over_n_hilo, 1);
  /* The code assumes 2^-200 < |xtail| < 2^-8/N.  */
  r = svadd_x (pg, r, xtail);
  /* 2^(k/N) ~= scale.  */
  svuint64_t idx = svand_x (pg, *ki, N_EXP - 1);
  svuint64_t top
      = svlsl_x (pg, svadd_x (pg, *ki, sign_bias), 52 - V_POW_EXP_TABLE_BITS);
  /* This is only a valid scale when -1023*N < k < 1024*N.  */
  *sbits = svld1_gather_index (pg, __v_pow_exp_data.sbits, idx);
  *sbits = svadd_x (pg, *sbits, top);
  /* exp(x) = 2^(k/N) * exp(r) ~= scale + scale * (exp(r) - 1).  */
  svfloat64_t r2 = svmul_x (svptrue_b64 (), r, r);
  *tmp = svmla_lane_f64 (sv_f64 (d->exp_c1), r, n_over_ln2_and_c2, 1);
  *tmp = svmla_x (pg, sv_f64 (d->exp_c0), r, *tmp);
  *tmp = svmla_x (pg, r, r2, *tmp);
  svfloat64_t scale = svreinterpret_f64 (*sbits);
  /* Note: tmp == 0 or |tmp| > 2^-200 and scale > 2^-739, so there
     is no spurious underflow here even without fma.  */
  z = svmla_x (pg, scale, scale, *tmp);
  return z;
}

/* Computes sign*exp(x+xtail) where |xtail| < 2^-8/N and |xtail| <= |x|.
   The sign_bias argument is SignBias or 0 and sets the sign to -1 or 1.  */
static inline svfloat64_t
sv_exp_inline (svbool_t pg, svfloat64_t x, svfloat64_t xtail,
	       svuint64_t sign_bias, const struct data *d)
{
  /* 3 types of special cases: tiny (uflow and spurious uflow), huge (oflow)
     and other cases of large values of x (scale * (1 + TMP) oflow).  */
  svuint64_t abstop = svand_x (pg, sv_top12 (x), 0x7ff);
  /* |x| is large (|x| >= 512) or tiny (|x| <= 0x1p-54).  */
  svbool_t uoflow = svcmpge (pg, svsub_x (pg, abstop, SmallExp), ThresExp);

  svfloat64_t tmp;
  svuint64_t sbits, ki;
  if (unlikely (svptest_any (pg, uoflow)))
    {
      svfloat64_t z
	  = sv_exp_core (pg, x, xtail, sign_bias, &tmp, &sbits, &ki, d);

      /* |x| is tiny (|x| <= 0x1p-54).  */
      svbool_t uflow
	  = svcmpge (pg, svsub_x (pg, abstop, SmallExp), 0x80000000);
      uflow = svand_z (pg, uoflow, uflow);
      /* |x| is huge (|x| >= 1024).  */
      svbool_t oflow = svcmpge (pg, abstop, HugeExp);
      oflow = svand_z (pg, uoflow, svbic_z (pg, oflow, uflow));

      /* For large |x| values (512 < |x| < 1024) scale * (1 + TMP) can overflow
    or underflow.  */
      svbool_t special = svbic_z (pg, uoflow, svorr_z (pg, uflow, oflow));

      /* Update result with special and large cases.  */
      z = sv_call_specialcase (tmp, sbits, ki, z, special);

      /* Handle underflow and overflow.  */
      svbool_t x_is_neg = svcmplt (pg, x, 0);
      svuint64_t sign_mask
	  = svlsl_x (pg, sign_bias, 52 - V_POW_EXP_TABLE_BITS);
      svfloat64_t res_uoflow
	  = svsel (x_is_neg, sv_f64 (0.0), sv_f64 (INFINITY));
      res_uoflow = svreinterpret_f64 (
	  svorr_x (pg, svreinterpret_u64 (res_uoflow), sign_mask));
      /* Avoid spurious underflow for tiny x.  */
      svfloat64_t res_spurious_uflow
	  = svreinterpret_f64 (svorr_x (pg, sign_mask, 0x3ff0000000000000));

      z = svsel (oflow, res_uoflow, z);
      z = svsel (uflow, res_spurious_uflow, z);
      return z;
    }

  return sv_exp_core (pg, x, xtail, sign_bias, &tmp, &sbits, &ki, d);
}

static inline double
pow_sc (double x, double y)
{
  uint64_t ix = asuint64 (x);
  uint64_t iy = asuint64 (y);
  /* Special cases: |x| or |y| is 0, inf or nan.  */
  if (unlikely (zeroinfnan (iy)))
    {
      if (2 * iy == 0)
	return issignaling_inline (x) ? x + y : 1.0;
      if (ix == asuint64 (1.0))
	return issignaling_inline (y) ? x + y : 1.0;
      if (2 * ix > 2 * asuint64 (INFINITY) || 2 * iy > 2 * asuint64 (INFINITY))
	return x + y;
      if (2 * ix == 2 * asuint64 (1.0))
	return 1.0;
      if ((2 * ix < 2 * asuint64 (1.0)) == !(iy >> 63))
	return 0.0; /* |x|<1 && y==inf or |x|>1 && y==-inf.  */
      return y * y;
    }
  if (unlikely (zeroinfnan (ix)))
    {
      double_t x2 = x * x;
      if (ix >> 63 && checkint (iy) == 1)
	x2 = -x2;
      return (iy >> 63) ? 1 / x2 : x2;
    }
  return x;
}

svfloat64_t SV_NAME_D2 (pow) (svfloat64_t x, svfloat64_t y, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* This preamble handles special case conditions used in the final scalar
     fallbacks. It also updates ix and sign_bias, that are used in the core
     computation too, i.e., exp( y * log (x) ).  */
  svuint64_t vix0 = svreinterpret_u64 (x);
  svuint64_t viy0 = svreinterpret_u64 (y);

  /* Negative x cases.  */
  svbool_t xisneg = svcmplt (pg, x, 0);

  /* Set sign_bias and ix depending on sign of x and nature of y.  */
  svbool_t yint_or_xpos = pg;
  svuint64_t sign_bias = sv_u64 (0);
  svuint64_t vix = vix0;
  if (unlikely (svptest_any (pg, xisneg)))
    {
      /* Determine nature of y.  */
      yint_or_xpos = sv_isint (xisneg, y);
      svbool_t yisodd_xisneg = sv_isodd (xisneg, y);
      /* ix set to abs(ix) if y is integer.  */
      vix = svand_m (yint_or_xpos, vix0, 0x7fffffffffffffff);
      /* Set to SignBias if x is negative and y is odd.  */
      sign_bias = svsel (yisodd_xisneg, sv_u64 (SignBias), sv_u64 (0));
    }

  /* Small cases of x: |x| < 0x1p-126.  */
  svbool_t xsmall = svaclt (yint_or_xpos, x, SmallBoundX);
  if (unlikely (svptest_any (yint_or_xpos, xsmall)))
    {
      /* Normalize subnormal x so exponent becomes negative.  */
      svuint64_t vtopx = svlsr_x (svptrue_b64 (), vix, 52);
      svbool_t topx_is_null = svcmpeq (xsmall, vtopx, 0);

      svuint64_t vix_norm = svreinterpret_u64 (svmul_m (xsmall, x, 0x1p52));
      vix_norm = svand_m (xsmall, vix_norm, 0x7fffffffffffffff);
      vix_norm = svsub_m (xsmall, vix_norm, 52ULL << 52);
      vix = svsel (topx_is_null, vix_norm, vix);
    }

  /* y_hi = log(ix, &y_lo).  */
  svfloat64_t vlo;
  svfloat64_t vhi = sv_log_inline (yint_or_xpos, vix, &vlo, d);

  /* z = exp(y_hi, y_lo, sign_bias).  */
  svfloat64_t vehi = svmul_x (svptrue_b64 (), y, vhi);
  svfloat64_t vemi = svmls_x (yint_or_xpos, vehi, y, vhi);
  svfloat64_t velo = svnmls_x (yint_or_xpos, vemi, y, vlo);
  svfloat64_t vz = sv_exp_inline (yint_or_xpos, vehi, velo, sign_bias, d);

  /* Cases of finite y and finite negative x.  */
  vz = svsel (yint_or_xpos, vz, sv_f64 (__builtin_nan ("")));

  /* Special cases of x or y: zero, inf and nan.  */
  svbool_t xspecial = sv_zeroinfnan (svptrue_b64 (), vix0);
  svbool_t yspecial = sv_zeroinfnan (svptrue_b64 (), viy0);
  svbool_t special = svorr_z (svptrue_b64 (), xspecial, yspecial);

  /* Cases of zero/inf/nan x or y.  */
  if (unlikely (svptest_any (svptrue_b64 (), special)))
    vz = sv_call2_f64 (pow_sc, x, y, vz, special);

  return vz;
}

TEST_SIG (SV, D, 2, pow)
TEST_ULP (SV_NAME_D2 (pow), 0.55)
TEST_DISABLE_FENV (SV_NAME_D2 (pow))
/* Wide intervals spanning the whole domain but shared between x and y.  */
#define SV_POW_INTERVAL2(xlo, xhi, ylo, yhi, n)                               \
  TEST_INTERVAL2 (SV_NAME_D2 (pow), xlo, xhi, ylo, yhi, n)                    \
  TEST_INTERVAL2 (SV_NAME_D2 (pow), xlo, xhi, -ylo, -yhi, n)                  \
  TEST_INTERVAL2 (SV_NAME_D2 (pow), -xlo, -xhi, ylo, yhi, n)                  \
  TEST_INTERVAL2 (SV_NAME_D2 (pow), -xlo, -xhi, -ylo, -yhi, n)
#define EXPAND(str) str##000000000
#define SHL52(str) EXPAND (str)
SV_POW_INTERVAL2 (0, SHL52 (SmallPowX), 0, inf, 40000)
SV_POW_INTERVAL2 (SHL52 (SmallPowX), SHL52 (BigPowX), 0, inf, 40000)
SV_POW_INTERVAL2 (SHL52 (BigPowX), inf, 0, inf, 40000)
SV_POW_INTERVAL2 (0, inf, 0, SHL52 (SmallPowY), 40000)
SV_POW_INTERVAL2 (0, inf, SHL52 (SmallPowY), SHL52 (BigPowY), 40000)
SV_POW_INTERVAL2 (0, inf, SHL52 (BigPowY), inf, 40000)
SV_POW_INTERVAL2 (0, inf, 0, inf, 1000)
/* x~1 or y~1.  */
SV_POW_INTERVAL2 (0x1p-1, 0x1p1, 0x1p-10, 0x1p10, 10000)
SV_POW_INTERVAL2 (0x1.ep-1, 0x1.1p0, 0x1p8, 0x1p16, 10000)
SV_POW_INTERVAL2 (0x1p-500, 0x1p500, 0x1p-1, 0x1p1, 10000)
/* around estimated argmaxs of ULP error.  */
SV_POW_INTERVAL2 (0x1p-300, 0x1p-200, 0x1p-20, 0x1p-10, 10000)
SV_POW_INTERVAL2 (0x1p50, 0x1p100, 0x1p-20, 0x1p-10, 10000)
/* x is negative, y is odd or even integer, or y is real not integer.  */
TEST_INTERVAL2 (SV_NAME_D2 (pow), -0.0, -10.0, 3.0, 3.0, 10000)
TEST_INTERVAL2 (SV_NAME_D2 (pow), -0.0, -10.0, 4.0, 4.0, 10000)
TEST_INTERVAL2 (SV_NAME_D2 (pow), -0.0, -10.0, 0.0, 10.0, 10000)
TEST_INTERVAL2 (SV_NAME_D2 (pow), 0.0, 10.0, -0.0, -10.0, 10000)
/* |x| is inf, y is odd or even integer, or y is real not integer.  */
SV_POW_INTERVAL2 (inf, inf, 0.5, 0.5, 1)
SV_POW_INTERVAL2 (inf, inf, 1.0, 1.0, 1)
SV_POW_INTERVAL2 (inf, inf, 2.0, 2.0, 1)
SV_POW_INTERVAL2 (inf, inf, 3.0, 3.0, 1)
/* 0.0^y.  */
SV_POW_INTERVAL2 (0.0, 0.0, 0.0, 0x1p120, 1000)
/* 1.0^y.  */
TEST_INTERVAL2 (SV_NAME_D2 (pow), 1.0, 1.0, 0.0, 0x1p-50, 1000)
TEST_INTERVAL2 (SV_NAME_D2 (pow), 1.0, 1.0, 0x1p-50, 1.0, 1000)
TEST_INTERVAL2 (SV_NAME_D2 (pow), 1.0, 1.0, 1.0, 0x1p100, 1000)
TEST_INTERVAL2 (SV_NAME_D2 (pow), 1.0, 1.0, -1.0, -0x1p120, 1000)
CLOSE_SVE_ATTR
