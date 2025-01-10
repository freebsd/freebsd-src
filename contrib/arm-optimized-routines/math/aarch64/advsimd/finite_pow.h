/*
 * Double-precision x^y function.
 *
 * Copyright (c) 2018-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"

/* Scalar version of pow used for fallbacks in vector implementations.  */

/* Data is defined in v_pow_log_data.c.  */
#define N_LOG (1 << V_POW_LOG_TABLE_BITS)
#define Off 0x3fe6955500000000
#define As __v_pow_log_data.poly

/* Data is defined in v_pow_exp_data.c.  */
#define N_EXP (1 << V_POW_EXP_TABLE_BITS)
#define SignBias (0x800 << V_POW_EXP_TABLE_BITS)
#define SmallExp 0x3c9 /* top12(0x1p-54).  */
#define BigExp 0x408   /* top12(512.0).  */
#define ThresExp 0x03f /* BigExp - SmallExp.  */
#define InvLn2N __v_pow_exp_data.n_over_ln2
#define Ln2HiN __v_pow_exp_data.ln2_over_n_hi
#define Ln2LoN __v_pow_exp_data.ln2_over_n_lo
#define SBits __v_pow_exp_data.sbits
#define Cs __v_pow_exp_data.poly

/* Constants associated with pow.  */
#define SmallPowX 0x001 /* top12(0x1p-126).  */
#define BigPowX 0x7ff	/* top12(INFINITY).  */
#define ThresPowX 0x7fe /* BigPowX - SmallPowX.  */
#define SmallPowY 0x3be /* top12(0x1.e7b6p-65).  */
#define BigPowY 0x43e	/* top12(0x1.749p62).  */
#define ThresPowY 0x080 /* BigPowY - SmallPowY.  */

/* Top 12 bits of a double (sign and exponent bits).  */
static inline uint32_t
top12 (double x)
{
  return asuint64 (x) >> 52;
}

/* Compute y+TAIL = log(x) where the rounded result is y and TAIL has about
   additional 15 bits precision.  IX is the bit representation of x, but
   normalized in the subnormal range using the sign bit for the exponent.  */
static inline double
log_inline (uint64_t ix, double *tail)
{
  /* x = 2^k z; where z is in range [Off,2*Off) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  uint64_t tmp = ix - Off;
  int i = (tmp >> (52 - V_POW_LOG_TABLE_BITS)) & (N_LOG - 1);
  int k = (int64_t) tmp >> 52; /* arithmetic shift.  */
  uint64_t iz = ix - (tmp & 0xfffULL << 52);
  double z = asdouble (iz);
  double kd = (double) k;

  /* log(x) = k*Ln2 + log(c) + log1p(z/c-1).  */
  double invc = __v_pow_log_data.invc[i];
  double logc = __v_pow_log_data.logc[i];
  double logctail = __v_pow_log_data.logctail[i];

  /* Note: 1/c is j/N or j/N/2 where j is an integer in [N,2N) and
     |z/c - 1| < 1/N, so r = z/c - 1 is exactly representible.  */
  double r = fma (z, invc, -1.0);

  /* k*Ln2 + log(c) + r.  */
  double t1 = kd * __v_pow_log_data.ln2_hi + logc;
  double t2 = t1 + r;
  double lo1 = kd * __v_pow_log_data.ln2_lo + logctail;
  double lo2 = t1 - t2 + r;

  /* Evaluation is optimized assuming superscalar pipelined execution.  */
  double ar = As[0] * r;
  double ar2 = r * ar;
  double ar3 = r * ar2;
  /* k*Ln2 + log(c) + r + A[0]*r*r.  */
  double hi = t2 + ar2;
  double lo3 = fma (ar, r, -ar2);
  double lo4 = t2 - hi + ar2;
  /* p = log1p(r) - r - A[0]*r*r.  */
  double p = (ar3
	      * (As[1] + r * As[2]
		 + ar2 * (As[3] + r * As[4] + ar2 * (As[5] + r * As[6]))));
  double lo = lo1 + lo2 + lo3 + lo4 + p;
  double y = hi + lo;
  *tail = hi - y + lo;
  return y;
}

/* Handle cases that may overflow or underflow when computing the result that
   is scale*(1+TMP) without intermediate rounding.  The bit representation of
   scale is in SBITS, however it has a computed exponent that may have
   overflown into the sign bit so that needs to be adjusted before using it as
   a double.  (int32_t)KI is the k used in the argument reduction and exponent
   adjustment of scale, positive k here means the result may overflow and
   negative k means the result may underflow.  */
static inline double
special_case (double tmp, uint64_t sbits, uint64_t ki)
{
  double scale, y;

  if ((ki & 0x80000000) == 0)
    {
      /* k > 0, the exponent of scale might have overflowed by <= 460.  */
      sbits -= 1009ull << 52;
      scale = asdouble (sbits);
      y = 0x1p1009 * (scale + scale * tmp);
      return y;
    }
  /* k < 0, need special care in the subnormal range.  */
  sbits += 1022ull << 52;
  /* Note: sbits is signed scale.  */
  scale = asdouble (sbits);
  y = scale + scale * tmp;
#if WANT_SIMD_EXCEPT
  if (fabs (y) < 1.0)
    {
      /* Round y to the right precision before scaling it into the subnormal
	 range to avoid double rounding that can cause 0.5+E/2 ulp error where
	 E is the worst-case ulp error outside the subnormal range.  So this
	 is only useful if the goal is better than 1 ulp worst-case error.  */
      double hi, lo, one = 1.0;
      if (y < 0.0)
	one = -1.0;
      lo = scale - y + scale * tmp;
      hi = one + y;
      lo = one - hi + y + lo;
      y = (hi + lo) - one;
      /* Fix the sign of 0.  */
      if (y == 0.0)
	y = asdouble (sbits & 0x8000000000000000);
      /* The underflow exception needs to be signaled explicitly.  */
      force_eval_double (opt_barrier_double (0x1p-1022) * 0x1p-1022);
    }
#endif
  y = 0x1p-1022 * y;
  return y;
}

/* Computes sign*exp(x+xtail) where |xtail| < 2^-8/N and |xtail| <= |x|.
   The sign_bias argument is SignBias or 0 and sets the sign to -1 or 1.  */
static inline double
exp_inline (double x, double xtail, uint32_t sign_bias)
{
  uint32_t abstop = top12 (x) & 0x7ff;
  if (unlikely (abstop - SmallExp >= ThresExp))
    {
      if (abstop - SmallExp >= 0x80000000)
	{
	  /* Avoid spurious underflow for tiny x.  */
	  /* Note: 0 is common input.  */
	  return sign_bias ? -1.0 : 1.0;
	}
      if (abstop >= top12 (1024.0))
	{
	  /* Note: inf and nan are already handled.  */
	  /* Skip errno handling.  */
#if WANT_SIMD_EXCEPT
	  return asuint64 (x) >> 63 ? __math_uflow (sign_bias)
				    : __math_oflow (sign_bias);
#else
	  double res_uoflow = asuint64 (x) >> 63 ? 0.0 : INFINITY;
	  return sign_bias ? -res_uoflow : res_uoflow;
#endif
	}
      /* Large x is special cased below.  */
      abstop = 0;
    }

  /* exp(x) = 2^(k/N) * exp(r), with exp(r) in [2^(-1/2N),2^(1/2N)].  */
  /* x = ln2/N*k + r, with int k and r in [-ln2/2N, ln2/2N].  */
  double z = InvLn2N * x;
  double kd = round (z);
  uint64_t ki = lround (z);
  double r = x - kd * Ln2HiN - kd * Ln2LoN;
  /* The code assumes 2^-200 < |xtail| < 2^-8/N.  */
  r += xtail;
  /* 2^(k/N) ~= scale.  */
  uint64_t idx = ki & (N_EXP - 1);
  uint64_t top = (ki + sign_bias) << (52 - V_POW_EXP_TABLE_BITS);
  /* This is only a valid scale when -1023*N < k < 1024*N.  */
  uint64_t sbits = SBits[idx] + top;
  /* exp(x) = 2^(k/N) * exp(r) ~= scale + scale * (exp(r) - 1).  */
  /* Evaluation is optimized assuming superscalar pipelined execution.  */
  double r2 = r * r;
  double tmp = r + r2 * Cs[0] + r * r2 * (Cs[1] + r * Cs[2]);
  if (unlikely (abstop == 0))
    return special_case (tmp, sbits, ki);
  double scale = asdouble (sbits);
  /* Note: tmp == 0 or |tmp| > 2^-200 and scale > 2^-739, so there
     is no spurious underflow here even without fma.  */
  return scale + scale * tmp;
}

/* Computes exp(x+xtail) where |xtail| < 2^-8/N and |xtail| <= |x|.
   A version of exp_inline that is not inlined and for which sign_bias is
   equal to 0.  */
static double NOINLINE
exp_nosignbias (double x, double xtail)
{
  uint32_t abstop = top12 (x) & 0x7ff;
  if (unlikely (abstop - SmallExp >= ThresExp))
    {
      /* Avoid spurious underflow for tiny x.  */
      if (abstop - SmallExp >= 0x80000000)
	return 1.0;
      /* Note: inf and nan are already handled.  */
      if (abstop >= top12 (1024.0))
#if WANT_SIMD_EXCEPT
	return asuint64 (x) >> 63 ? __math_uflow (0) : __math_oflow (0);
#else
	return asuint64 (x) >> 63 ? 0.0 : INFINITY;
#endif
      /* Large x is special cased below.  */
      abstop = 0;
    }

  /* exp(x) = 2^(k/N) * exp(r), with exp(r) in [2^(-1/2N),2^(1/2N)].  */
  /* x = ln2/N*k + r, with k integer and r in [-ln2/2N, ln2/2N].  */
  double z = InvLn2N * x;
  double kd = round (z);
  uint64_t ki = lround (z);
  double r = x - kd * Ln2HiN - kd * Ln2LoN;
  /* The code assumes 2^-200 < |xtail| < 2^-8/N.  */
  r += xtail;
  /* 2^(k/N) ~= scale.  */
  uint64_t idx = ki & (N_EXP - 1);
  uint64_t top = ki << (52 - V_POW_EXP_TABLE_BITS);
  /* This is only a valid scale when -1023*N < k < 1024*N.  */
  uint64_t sbits = SBits[idx] + top;
  /* exp(x) = 2^(k/N) * exp(r) ~= scale + scale * (tail + exp(r) - 1).  */
  double r2 = r * r;
  double tmp = r + r2 * Cs[0] + r * r2 * (Cs[1] + r * Cs[2]);
  if (unlikely (abstop == 0))
    return special_case (tmp, sbits, ki);
  double scale = asdouble (sbits);
  /* Note: tmp == 0 or |tmp| > 2^-200 and scale > 2^-739, so there
     is no spurious underflow here even without fma.  */
  return scale + scale * tmp;
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

/* Returns 1 if input is the bit representation of 0, infinity or nan.  */
static inline int
zeroinfnan (uint64_t i)
{
  return 2 * i - 1 >= 2 * asuint64 (INFINITY) - 1;
}

static double NOINLINE
pow_scalar_special_case (double x, double y)
{
  uint32_t sign_bias = 0;
  uint64_t ix, iy;
  uint32_t topx, topy;

  ix = asuint64 (x);
  iy = asuint64 (y);
  topx = top12 (x);
  topy = top12 (y);
  if (unlikely (topx - SmallPowX >= ThresPowX
		|| (topy & 0x7ff) - SmallPowY >= ThresPowY))
    {
      /* Note: if |y| > 1075 * ln2 * 2^53 ~= 0x1.749p62 then pow(x,y) = inf/0
	 and if |y| < 2^-54 / 1075 ~= 0x1.e7b6p-65 then pow(x,y) = +-1.  */
      /* Special cases: (x < 0x1p-126 or inf or nan) or
	 (|y| < 0x1p-65 or |y| >= 0x1p63 or nan).  */
      if (unlikely (zeroinfnan (iy)))
	{
	  if (2 * iy == 0)
	    return issignaling_inline (x) ? x + y : 1.0;
	  if (ix == asuint64 (1.0))
	    return issignaling_inline (y) ? x + y : 1.0;
	  if (2 * ix > 2 * asuint64 (INFINITY)
	      || 2 * iy > 2 * asuint64 (INFINITY))
	    return x + y;
	  if (2 * ix == 2 * asuint64 (1.0))
	    return 1.0;
	  if ((2 * ix < 2 * asuint64 (1.0)) == !(iy >> 63))
	    return 0.0; /* |x|<1 && y==inf or |x|>1 && y==-inf.  */
	  return y * y;
	}
      if (unlikely (zeroinfnan (ix)))
	{
	  double x2 = x * x;
	  if (ix >> 63 && checkint (iy) == 1)
	    {
	      x2 = -x2;
	      sign_bias = 1;
	    }
#if WANT_SIMD_EXCEPT
	  if (2 * ix == 0 && iy >> 63)
	    return __math_divzero (sign_bias);
#endif
	  return iy >> 63 ? 1 / x2 : x2;
	}
      /* Here x and y are non-zero finite.  */
      if (ix >> 63)
	{
	  /* Finite x < 0.  */
	  int yint = checkint (iy);
	  if (yint == 0)
#if WANT_SIMD_EXCEPT
	    return __math_invalid (x);
#else
	    return __builtin_nan ("");
#endif
	  if (yint == 1)
	    sign_bias = SignBias;
	  ix &= 0x7fffffffffffffff;
	  topx &= 0x7ff;
	}
      if ((topy & 0x7ff) - SmallPowY >= ThresPowY)
	{
	  /* Note: sign_bias == 0 here because y is not odd.  */
	  if (ix == asuint64 (1.0))
	    return 1.0;
	  /* |y| < 2^-65, x^y ~= 1 + y*log(x).  */
	  if ((topy & 0x7ff) < SmallPowY)
	    return 1.0;
#if WANT_SIMD_EXCEPT
	  return (ix > asuint64 (1.0)) == (topy < 0x800) ? __math_oflow (0)
							 : __math_uflow (0);
#else
	  return (ix > asuint64 (1.0)) == (topy < 0x800) ? INFINITY : 0;
#endif
	}
      if (topx == 0)
	{
	  /* Normalize subnormal x so exponent becomes negative.  */
	  ix = asuint64 (x * 0x1p52);
	  ix &= 0x7fffffffffffffff;
	  ix -= 52ULL << 52;
	}
    }

  double lo;
  double hi = log_inline (ix, &lo);
  double ehi = y * hi;
  double elo = y * lo + fma (y, hi, -ehi);
  return exp_inline (ehi, elo, sign_bias);
}
