/*
 * Function wrappers for mathbench.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#if WANT_EXPERIMENTAL_MATH
static double
atan2_wrap (double x)
{
  return atan2 (5.0, x);
}

static float
atan2f_wrap (float x)
{
  return atan2f (5.0f, x);
}

static double
powi_wrap (double x)
{
  return __builtin_powi (x, (int) round (x));
}
#endif /* WANT_EXPERIMENTAL_MATH.  */

#if __aarch64__ && __linux__

__vpcs static float32x4_t
_Z_sincospif_wrap (float32x4_t x)
{
  float s[4], c[4];
  _ZGVnN4vl4l4_sincospif (x, s, c);
  return vld1q_f32 (s) + vld1q_f32 (c);
}

__vpcs static float64x2_t
_Z_sincospi_wrap (float64x2_t x)
{
  double s[2], c[2];
  _ZGVnN2vl8l8_sincospi (x, s, c);
  return vld1q_f64 (s) + vld1q_f64 (c);
}

__vpcs static float64x2_t
_Z_atan2_wrap (float64x2_t x)
{
  return _ZGVnN2vv_atan2 (vdupq_n_f64 (5.0), x);
}

__vpcs static float32x4_t
_Z_atan2f_wrap (float32x4_t x)
{
  return _ZGVnN4vv_atan2f (vdupq_n_f32 (5.0f), x);
}

__vpcs static float32x4_t
_Z_hypotf_wrap (float32x4_t x)
{
  return _ZGVnN4vv_hypotf (vdupq_n_f32 (5.0f), x);
}

__vpcs static float64x2_t
_Z_hypot_wrap (float64x2_t x)
{
  return _ZGVnN2vv_hypot (vdupq_n_f64 (5.0), x);
}

__vpcs static float32x4_t
xy_Z_powf (float32x4_t x)
{
  return _ZGVnN4vv_powf (x, x);
}

__vpcs static float32x4_t
x_Z_powf (float32x4_t x)
{
  return _ZGVnN4vv_powf (x, vdupq_n_f32 (23.4));
}

__vpcs static float32x4_t
y_Z_powf (float32x4_t x)
{
  return _ZGVnN4vv_powf (vdupq_n_f32 (2.34), x);
}

__vpcs static float64x2_t
xy_Z_pow (float64x2_t x)
{
  return _ZGVnN2vv_pow (x, x);
}

__vpcs static float64x2_t
x_Z_pow (float64x2_t x)
{
  return _ZGVnN2vv_pow (x, vdupq_n_f64 (23.4));
}

__vpcs static float64x2_t
y_Z_pow (float64x2_t x)
{
  return _ZGVnN2vv_pow (vdupq_n_f64 (2.34), x);
}

__vpcs static float32x4_t
_Z_modff_wrap (float32x4_t x)
{
  float y[4];
  float32x4_t ret = _ZGVnN4vl4_modff (x, y);
  return ret + vld1q_f32 (y);
}

__vpcs static float64x2_t
_Z_modf_wrap (float64x2_t x)
{
  double y[2];
  float64x2_t ret = _ZGVnN2vl8_modf (x, y);
  return ret + vld1q_f64 (y);
}

__vpcs static float32x4_t
_Z_sincosf_wrap (float32x4_t x)
{
  float s[4], c[4];
  _ZGVnN4vl4l4_sincosf (x, s, c);
  return vld1q_f32 (s) + vld1q_f32 (c);
}

__vpcs static float32x4_t
_Z_cexpif_wrap (float32x4_t x)
{
  float32x4x2_t sc = _ZGVnN4v_cexpif (x);
  return sc.val[0] + sc.val[1];
}

__vpcs static float64x2_t
_Z_sincos_wrap (float64x2_t x)
{
  double s[2], c[2];
  _ZGVnN2vl8l8_sincos (x, s, c);
  return vld1q_f64 (s) + vld1q_f64 (c);
}

__vpcs static float64x2_t
_Z_cexpi_wrap (float64x2_t x)
{
  float64x2x2_t sc = _ZGVnN2v_cexpi (x);
  return sc.val[0] + sc.val[1];
}

#endif

#if WANT_SVE_TESTS

static svfloat32_t
_Z_sv_atan2f_wrap (svfloat32_t x, svbool_t pg)
{
  return _ZGVsMxvv_atan2f (x, svdup_f32 (5.0f), pg);
}

static svfloat64_t
_Z_sv_atan2_wrap (svfloat64_t x, svbool_t pg)
{
  return _ZGVsMxvv_atan2 (x, svdup_f64 (5.0), pg);
}

static svfloat32_t
_Z_sv_hypotf_wrap (svfloat32_t x, svbool_t pg)
{
  return _ZGVsMxvv_hypotf (x, svdup_f32 (5.0), pg);
}

static svfloat64_t
_Z_sv_hypot_wrap (svfloat64_t x, svbool_t pg)
{
  return _ZGVsMxvv_hypot (x, svdup_f64 (5.0), pg);
}

static svfloat32_t
xy_Z_sv_powf (svfloat32_t x, svbool_t pg)
{
  return _ZGVsMxvv_powf (x, x, pg);
}

static svfloat32_t
x_Z_sv_powf (svfloat32_t x, svbool_t pg)
{
  return _ZGVsMxvv_powf (x, svdup_f32 (23.4f), pg);
}

static svfloat32_t
y_Z_sv_powf (svfloat32_t x, svbool_t pg)
{
  return _ZGVsMxvv_powf (svdup_f32 (2.34f), x, pg);
}

static svfloat64_t
xy_Z_sv_pow (svfloat64_t x, svbool_t pg)
{
  return _ZGVsMxvv_pow (x, x, pg);
}

static svfloat64_t
x_Z_sv_pow (svfloat64_t x, svbool_t pg)
{
  return _ZGVsMxvv_pow (x, svdup_f64 (23.4), pg);
}

static svfloat64_t
y_Z_sv_pow (svfloat64_t x, svbool_t pg)
{
  return _ZGVsMxvv_pow (svdup_f64 (2.34), x, pg);
}

static svfloat32_t
_Z_sv_sincospif_wrap (svfloat32_t x, svbool_t pg)
{
  float s[svcntw ()], c[svcntw ()];
  _ZGVsMxvl4l4_sincospif (x, s, c, pg);
  return svadd_x (pg, svld1 (pg, s), svld1 (pg, c));
}

static svfloat64_t
_Z_sv_sincospi_wrap (svfloat64_t x, svbool_t pg)
{
  double s[svcntd ()], c[svcntd ()];
  _ZGVsMxvl8l8_sincospi (x, s, c, pg);
  return svadd_x (pg, svld1 (pg, s), svld1 (pg, c));
}

static svfloat32_t
_Z_sv_modff_wrap (svfloat32_t x, svbool_t pg)
{
  float i[svcntw ()];
  svfloat32_t r = _ZGVsMxvl4_modff (x, i, pg);
  return svadd_x (pg, r, svld1 (pg, i));
}

static svfloat64_t
_Z_sv_modf_wrap (svfloat64_t x, svbool_t pg)
{
  double i[svcntd ()];
  svfloat64_t r = _ZGVsMxvl8_modf (x, i, pg);
  return svadd_x (pg, r, svld1 (pg, i));
}

static svfloat32_t
_Z_sv_sincosf_wrap (svfloat32_t x, svbool_t pg)
{
  float s[svcntw ()], c[svcntw ()];
  _ZGVsMxvl4l4_sincosf (x, s, c, pg);
  return svadd_x (pg, svld1 (pg, s), svld1 (pg, s));
}

static svfloat32_t
_Z_sv_cexpif_wrap (svfloat32_t x, svbool_t pg)
{
  svfloat32x2_t sc = _ZGVsMxv_cexpif (x, pg);
  return svadd_x (pg, svget2 (sc, 0), svget2 (sc, 1));
}

static svfloat64_t
_Z_sv_sincos_wrap (svfloat64_t x, svbool_t pg)
{
  double s[svcntd ()], c[svcntd ()];
  _ZGVsMxvl8l8_sincos (x, s, c, pg);
  return svadd_x (pg, svld1 (pg, s), svld1 (pg, s));
}

static svfloat64_t
_Z_sv_cexpi_wrap (svfloat64_t x, svbool_t pg)
{
  svfloat64x2_t sc = _ZGVsMxv_cexpi (x, pg);
  return svadd_x (pg, svget2 (sc, 0), svget2 (sc, 1));
}

# if WANT_EXPERIMENTAL_MATH

static svfloat32_t
_Z_sv_powi_wrap (svfloat32_t x, svbool_t pg)
{
  return _ZGVsMxvv_powi (x, svcvt_s32_f32_x (pg, x), pg);
}

static svfloat64_t
_Z_sv_powk_wrap (svfloat64_t x, svbool_t pg)
{
  return _ZGVsMxvv_powk (x, svcvt_s64_f64_x (pg, x), pg);
}

# endif

#endif

#if __aarch64__
static float
sincospif_wrap (float x)
{
  float s, c;
  arm_math_sincospif (x, &s, &c);
  return s + c;
}

static double
sincospi_wrap (double x)
{
  double s, c;
  arm_math_sincospi (x, &s, &c);
  return s + c;
}
#endif

static double
xypow (double x)
{
  return pow (x, x);
}

static float
xypowf (float x)
{
  return powf (x, x);
}

static double
xpow (double x)
{
  return pow (x, 23.4);
}

static float
xpowf (float x)
{
  return powf (x, 23.4f);
}

static double
ypow (double x)
{
  return pow (2.34, x);
}

static float
ypowf (float x)
{
  return powf (2.34f, x);
}

static float
sincosf_wrap (float x)
{
  float s, c;
  sincosf (x, &s, &c);
  return s + c;
}
