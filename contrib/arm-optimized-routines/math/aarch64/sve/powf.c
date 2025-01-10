/*
 * Single-precision SVE powf function.
 *
 * Copyright (c) 2023-2025, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"

/* The following data is used in the SVE pow core computation
   and special case detection.  */
#define Tinvc __v_powf_data.invc
#define Tlogc __v_powf_data.logc
#define Texp __v_powf_data.scale
#define SignBias (1 << (V_POWF_EXP2_TABLE_BITS + 11))
#define Norm 0x1p23f /* 0x4b000000.  */

/* Overall ULP error bound for pow is 2.6 ulp
   ~ 0.5 + 2^24 (128*Ln2*relerr_log2 + relerr_exp2).  */
static const struct data
{
  double log_poly[4];
  double exp_poly[3];
  float uflow_bound, oflow_bound, small_bound;
  uint32_t sign_bias, subnormal_bias, off;
} data = {
  /* rel err: 1.5 * 2^-30. Each coefficients is multiplied the value of
     V_POWF_EXP2_N.  */
  .log_poly = { -0x1.6ff5daa3b3d7cp+3, 0x1.ec81d03c01aebp+3,
		-0x1.71547bb43f101p+4, 0x1.7154764a815cbp+5 },
  /* rel err: 1.69 * 2^-34.  */
  .exp_poly = {
    0x1.c6af84b912394p-20, /* A0 / V_POWF_EXP2_N^3.  */
    0x1.ebfce50fac4f3p-13, /* A1 / V_POWF_EXP2_N^2.  */
    0x1.62e42ff0c52d6p-6,   /* A3 / V_POWF_EXP2_N.  */
  },
  .uflow_bound = -0x1.2cp+12f, /* -150.0 * V_POWF_EXP2_N.  */
  .oflow_bound = 0x1p+12f, /* 128.0 * V_POWF_EXP2_N.  */
  .small_bound = 0x1p-126f,
  .off = 0x3f35d000,
  .sign_bias = SignBias,
  .subnormal_bias = 0x0b800000, /* 23 << 23.  */
};

#define A(i) sv_f64 (d->log_poly[i])
#define C(i) sv_f64 (d->exp_poly[i])

/* Check if x is an integer.  */
static inline svbool_t
svisint (svbool_t pg, svfloat32_t x)
{
  return svcmpeq (pg, svrintz_z (pg, x), x);
}

/* Check if x is real not integer valued.  */
static inline svbool_t
svisnotint (svbool_t pg, svfloat32_t x)
{
  return svcmpne (pg, svrintz_z (pg, x), x);
}

/* Check if x is an odd integer.  */
static inline svbool_t
svisodd (svbool_t pg, svfloat32_t x)
{
  svfloat32_t y = svmul_x (pg, x, 0.5f);
  return svisnotint (pg, y);
}

/* Check if zero, inf or nan.  */
static inline svbool_t
sv_zeroinfnan (svbool_t pg, svuint32_t i)
{
  return svcmpge (pg, svsub_x (pg, svadd_x (pg, i, i), 1),
		  2u * 0x7f800000 - 1);
}

/* Returns 0 if not int, 1 if odd int, 2 if even int.  The argument is
   the bit representation of a non-zero finite floating-point value.  */
static inline int
checkint (uint32_t iy)
{
  int e = iy >> 23 & 0xff;
  if (e < 0x7f)
    return 0;
  if (e > 0x7f + 23)
    return 2;
  if (iy & ((1 << (0x7f + 23 - e)) - 1))
    return 0;
  if (iy & (1 << (0x7f + 23 - e)))
    return 1;
  return 2;
}

/* Check if zero, inf or nan.  */
static inline int
zeroinfnan (uint32_t ix)
{
  return 2 * ix - 1 >= 2u * 0x7f800000 - 1;
}

/* A scalar subroutine used to fix main power special cases. Similar to the
   preamble of scalar powf except that we do not update ix and sign_bias. This
   is done in the preamble of the SVE powf.  */
static inline float
powf_specialcase (float x, float y, float z)
{
  uint32_t ix = asuint (x);
  uint32_t iy = asuint (y);
  /* Either (x < 0x1p-126 or inf or nan) or (y is 0 or inf or nan).  */
  if (unlikely (zeroinfnan (iy)))
    {
      if (2 * iy == 0)
	return issignalingf_inline (x) ? x + y : 1.0f;
      if (ix == 0x3f800000)
	return issignalingf_inline (y) ? x + y : 1.0f;
      if (2 * ix > 2u * 0x7f800000 || 2 * iy > 2u * 0x7f800000)
	return x + y;
      if (2 * ix == 2 * 0x3f800000)
	return 1.0f;
      if ((2 * ix < 2 * 0x3f800000) == !(iy & 0x80000000))
	return 0.0f; /* |x|<1 && y==inf or |x|>1 && y==-inf.  */
      return y * y;
    }
  if (unlikely (zeroinfnan (ix)))
    {
      float_t x2 = x * x;
      if (ix & 0x80000000 && checkint (iy) == 1)
	x2 = -x2;
      return iy & 0x80000000 ? 1 / x2 : x2;
    }
  /* We need a return here in case x<0 and y is integer, but all other tests
   need to be run.  */
  return z;
}

/* Scalar fallback for special case routines with custom signature.  */
static svfloat32_t NOINLINE
sv_call_powf_sc (svfloat32_t x1, svfloat32_t x2, svfloat32_t y)
{
  /* Special cases of x or y: zero, inf and nan.  */
  svbool_t xspecial = sv_zeroinfnan (svptrue_b32 (), svreinterpret_u32 (x1));
  svbool_t yspecial = sv_zeroinfnan (svptrue_b32 (), svreinterpret_u32 (x2));
  svbool_t cmp = svorr_z (svptrue_b32 (), xspecial, yspecial);

  svbool_t p = svpfirst (cmp, svpfalse ());
  while (svptest_any (cmp, p))
    {
      float sx1 = svclastb (p, 0, x1);
      float sx2 = svclastb (p, 0, x2);
      float elem = svclastb (p, 0, y);
      elem = powf_specialcase (sx1, sx2, elem);
      svfloat32_t y2 = sv_f32 (elem);
      y = svsel (p, y2, y);
      p = svpnext_b32 (cmp, p);
    }
  return y;
}

/* Compute core for half of the lanes in double precision.  */
static inline svfloat64_t
sv_powf_core_ext (const svbool_t pg, svuint64_t i, svfloat64_t z, svint64_t k,
		  svfloat64_t y, svuint64_t sign_bias, svfloat64_t *pylogx,
		  const struct data *d)
{
  svfloat64_t invc = svld1_gather_index (pg, Tinvc, i);
  svfloat64_t logc = svld1_gather_index (pg, Tlogc, i);

  /* log2(x) = log1p(z/c-1)/ln2 + log2(c) + k.  */
  svfloat64_t r = svmla_x (pg, sv_f64 (-1.0), z, invc);
  svfloat64_t y0 = svadd_x (pg, logc, svcvt_f64_x (pg, k));

  /* Polynomial to approximate log1p(r)/ln2.  */
  svfloat64_t logx = A (0);
  logx = svmad_x (pg, r, logx, A (1));
  logx = svmad_x (pg, r, logx, A (2));
  logx = svmad_x (pg, r, logx, A (3));
  logx = svmad_x (pg, r, logx, y0);
  *pylogx = svmul_x (pg, y, logx);

  /* z - kd is in [-1, 1] in non-nearest rounding modes.  */
  svfloat64_t kd = svrinta_x (svptrue_b64 (), *pylogx);
  svuint64_t ki = svreinterpret_u64 (svcvt_s64_x (svptrue_b64 (), kd));

  r = svsub_x (pg, *pylogx, kd);

  /* exp2(x) = 2^(k/N) * 2^r ~= s * (C0*r^3 + C1*r^2 + C2*r + 1).  */
  svuint64_t t = svld1_gather_index (
      svptrue_b64 (), Texp, svand_x (svptrue_b64 (), ki, V_POWF_EXP2_N - 1));
  svuint64_t ski = svadd_x (svptrue_b64 (), ki, sign_bias);
  t = svadd_x (svptrue_b64 (), t,
	       svlsl_x (svptrue_b64 (), ski, 52 - V_POWF_EXP2_TABLE_BITS));
  svfloat64_t s = svreinterpret_f64 (t);

  svfloat64_t p = C (0);
  p = svmla_x (pg, C (1), p, r);
  p = svmla_x (pg, C (2), p, r);
  p = svmla_x (pg, s, p, svmul_x (svptrue_b64 (), s, r));

  return p;
}

/* Widen vector to double precision and compute core on both halves of the
   vector. Lower cost of promotion by considering all lanes active.  */
static inline svfloat32_t
sv_powf_core (const svbool_t pg, svuint32_t i, svuint32_t iz, svint32_t k,
	      svfloat32_t y, svuint32_t sign_bias, svfloat32_t *pylogx,
	      const struct data *d)
{
  const svbool_t ptrue = svptrue_b64 ();

  /* Unpack and promote input vectors (pg, y, z, i, k and sign_bias) into two
     in order to perform core computation in double precision.  */
  const svbool_t pg_lo = svunpklo (pg);
  const svbool_t pg_hi = svunpkhi (pg);
  svfloat64_t y_lo
      = svcvt_f64_x (pg, svreinterpret_f32 (svunpklo (svreinterpret_u32 (y))));
  svfloat64_t y_hi
      = svcvt_f64_x (pg, svreinterpret_f32 (svunpkhi (svreinterpret_u32 (y))));
  svfloat64_t z_lo = svcvt_f64_x (pg, svreinterpret_f32 (svunpklo (iz)));
  svfloat64_t z_hi = svcvt_f64_x (pg, svreinterpret_f32 (svunpkhi (iz)));
  svuint64_t i_lo = svunpklo (i);
  svuint64_t i_hi = svunpkhi (i);
  svint64_t k_lo = svunpklo (k);
  svint64_t k_hi = svunpkhi (k);
  svuint64_t sign_bias_lo = svunpklo (sign_bias);
  svuint64_t sign_bias_hi = svunpkhi (sign_bias);

  /* Compute each part in double precision.  */
  svfloat64_t ylogx_lo, ylogx_hi;
  svfloat64_t lo = sv_powf_core_ext (pg_lo, i_lo, z_lo, k_lo, y_lo,
				     sign_bias_lo, &ylogx_lo, d);
  svfloat64_t hi = sv_powf_core_ext (pg_hi, i_hi, z_hi, k_hi, y_hi,
				     sign_bias_hi, &ylogx_hi, d);

  /* Convert back to single-precision and interleave.  */
  svfloat32_t ylogx_lo_32 = svcvt_f32_x (ptrue, ylogx_lo);
  svfloat32_t ylogx_hi_32 = svcvt_f32_x (ptrue, ylogx_hi);
  *pylogx = svuzp1 (ylogx_lo_32, ylogx_hi_32);
  svfloat32_t lo_32 = svcvt_f32_x (ptrue, lo);
  svfloat32_t hi_32 = svcvt_f32_x (ptrue, hi);
  return svuzp1 (lo_32, hi_32);
}

/* Implementation of SVE powf.
   Provides the same accuracy as AdvSIMD powf, since it relies on the same
   algorithm. The theoretical maximum error is under 2.60 ULPs.
   Maximum measured error is 2.57 ULPs:
   SV_NAME_F2 (pow) (0x1.031706p+0, 0x1.ce2ec2p+12) got 0x1.fff868p+127
						   want 0x1.fff862p+127.  */
svfloat32_t SV_NAME_F2 (pow) (svfloat32_t x, svfloat32_t y, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svuint32_t vix0 = svreinterpret_u32 (x);
  svuint32_t viy0 = svreinterpret_u32 (y);

  /* Negative x cases.  */
  svbool_t xisneg = svcmplt (pg, x, sv_f32 (0));

  /* Set sign_bias and ix depending on sign of x and nature of y.  */
  svbool_t yint_or_xpos = pg;
  svuint32_t sign_bias = sv_u32 (0);
  svuint32_t vix = vix0;
  if (unlikely (svptest_any (pg, xisneg)))
    {
      /* Determine nature of y.  */
      yint_or_xpos = svisint (xisneg, y);
      svbool_t yisodd_xisneg = svisodd (xisneg, y);
      /* ix set to abs(ix) if y is integer.  */
      vix = svand_m (yint_or_xpos, vix0, 0x7fffffff);
      /* Set to SignBias if x is negative and y is odd.  */
      sign_bias = svsel (yisodd_xisneg, sv_u32 (d->sign_bias), sv_u32 (0));
    }

  /* Special cases of x or y: zero, inf and nan.  */
  svbool_t xspecial = sv_zeroinfnan (pg, vix0);
  svbool_t yspecial = sv_zeroinfnan (pg, viy0);
  svbool_t cmp = svorr_z (pg, xspecial, yspecial);

  /* Small cases of x: |x| < 0x1p-126.  */
  svbool_t xsmall = svaclt (yint_or_xpos, x, d->small_bound);
  if (unlikely (svptest_any (yint_or_xpos, xsmall)))
    {
      /* Normalize subnormal x so exponent becomes negative.  */
      svuint32_t vix_norm = svreinterpret_u32 (svmul_x (xsmall, x, Norm));
      vix_norm = svand_x (xsmall, vix_norm, 0x7fffffff);
      vix_norm = svsub_x (xsmall, vix_norm, d->subnormal_bias);
      vix = svsel (xsmall, vix_norm, vix);
    }
  /* Part of core computation carried in working precision.  */
  svuint32_t tmp = svsub_x (yint_or_xpos, vix, d->off);
  svuint32_t i = svand_x (
      yint_or_xpos, svlsr_x (yint_or_xpos, tmp, (23 - V_POWF_LOG2_TABLE_BITS)),
      V_POWF_LOG2_N - 1);
  svuint32_t top = svand_x (yint_or_xpos, tmp, 0xff800000);
  svuint32_t iz = svsub_x (yint_or_xpos, vix, top);
  svint32_t k = svasr_x (yint_or_xpos, svreinterpret_s32 (top),
			 (23 - V_POWF_EXP2_TABLE_BITS));

  /* Compute core in extended precision and return intermediate ylogx results
     to handle cases of underflow and underflow in exp.  */
  svfloat32_t ylogx;
  svfloat32_t ret
      = sv_powf_core (yint_or_xpos, i, iz, k, y, sign_bias, &ylogx, d);

  /* Handle exp special cases of underflow and overflow.  */
  svuint32_t sign
      = svlsl_x (yint_or_xpos, sign_bias, 20 - V_POWF_EXP2_TABLE_BITS);
  svfloat32_t ret_oflow
      = svreinterpret_f32 (svorr_x (yint_or_xpos, sign, asuint (INFINITY)));
  svfloat32_t ret_uflow = svreinterpret_f32 (sign);
  ret = svsel (svcmple (yint_or_xpos, ylogx, d->uflow_bound), ret_uflow, ret);
  ret = svsel (svcmpgt (yint_or_xpos, ylogx, d->oflow_bound), ret_oflow, ret);

  /* Cases of finite y and finite negative x.  */
  ret = svsel (yint_or_xpos, ret, sv_f32 (__builtin_nanf ("")));

  if (unlikely (svptest_any (cmp, cmp)))
    return sv_call_powf_sc (x, y, ret);

  return ret;
}

TEST_SIG (SV, F, 2, pow)
TEST_ULP (SV_NAME_F2 (pow), 2.08)
TEST_DISABLE_FENV (SV_NAME_F2 (pow))
/* Wide intervals spanning the whole domain but shared between x and y.  */
#define SV_POWF_INTERVAL2(xlo, xhi, ylo, yhi, n)                              \
  TEST_INTERVAL2 (SV_NAME_F2 (pow), xlo, xhi, ylo, yhi, n)                    \
  TEST_INTERVAL2 (SV_NAME_F2 (pow), xlo, xhi, -ylo, -yhi, n)                  \
  TEST_INTERVAL2 (SV_NAME_F2 (pow), -xlo, -xhi, ylo, yhi, n)                  \
  TEST_INTERVAL2 (SV_NAME_F2 (pow), -xlo, -xhi, -ylo, -yhi, n)
SV_POWF_INTERVAL2 (0, 0x1p-126, 0, inf, 40000)
SV_POWF_INTERVAL2 (0x1p-126, 1, 0, inf, 50000)
SV_POWF_INTERVAL2 (1, inf, 0, inf, 50000)
/* x~1 or y~1.  */
SV_POWF_INTERVAL2 (0x1p-1, 0x1p1, 0x1p-10, 0x1p10, 10000)
SV_POWF_INTERVAL2 (0x1.ep-1, 0x1.1p0, 0x1p8, 0x1p16, 10000)
SV_POWF_INTERVAL2 (0x1p-500, 0x1p500, 0x1p-1, 0x1p1, 10000)
/* around estimated argmaxs of ULP error.  */
SV_POWF_INTERVAL2 (0x1p-300, 0x1p-200, 0x1p-20, 0x1p-10, 10000)
SV_POWF_INTERVAL2 (0x1p50, 0x1p100, 0x1p-20, 0x1p-10, 10000)
/* x is negative, y is odd or even integer, or y is real not integer.  */
TEST_INTERVAL2 (SV_NAME_F2 (pow), -0.0, -10.0, 3.0, 3.0, 10000)
TEST_INTERVAL2 (SV_NAME_F2 (pow), -0.0, -10.0, 4.0, 4.0, 10000)
TEST_INTERVAL2 (SV_NAME_F2 (pow), -0.0, -10.0, 0.0, 10.0, 10000)
TEST_INTERVAL2 (SV_NAME_F2 (pow), 0.0, 10.0, -0.0, -10.0, 10000)
/* |x| is inf, y is odd or even integer, or y is real not integer.  */
SV_POWF_INTERVAL2 (inf, inf, 0.5, 0.5, 1)
SV_POWF_INTERVAL2 (inf, inf, 1.0, 1.0, 1)
SV_POWF_INTERVAL2 (inf, inf, 2.0, 2.0, 1)
SV_POWF_INTERVAL2 (inf, inf, 3.0, 3.0, 1)
/* 0.0^y.  */
SV_POWF_INTERVAL2 (0.0, 0.0, 0.0, 0x1p120, 1000)
/* 1.0^y.  */
TEST_INTERVAL2 (SV_NAME_F2 (pow), 1.0, 1.0, 0.0, 0x1p-50, 1000)
TEST_INTERVAL2 (SV_NAME_F2 (pow), 1.0, 1.0, 0x1p-50, 1.0, 1000)
TEST_INTERVAL2 (SV_NAME_F2 (pow), 1.0, 1.0, 1.0, 0x1p100, 1000)
TEST_INTERVAL2 (SV_NAME_F2 (pow), 1.0, 1.0, -1.0, -0x1p120, 1000)
CLOSE_SVE_ATTR
