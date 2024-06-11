/*
 * Function wrappers for mathbench.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

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

#if __aarch64__ && defined(__vpcs)

__vpcs static v_double
_Z_atan2_wrap (v_double x)
{
  return _ZGVnN2vv_atan2 (v_double_dup (5.0), x);
}

__vpcs static v_float
_Z_atan2f_wrap (v_float x)
{
  return _ZGVnN4vv_atan2f (v_float_dup (5.0f), x);
}

__vpcs static v_float
_Z_hypotf_wrap (v_float x)
{
  return _ZGVnN4vv_hypotf (v_float_dup (5.0f), x);
}

__vpcs static v_double
_Z_hypot_wrap (v_double x)
{
  return _ZGVnN2vv_hypot (v_double_dup (5.0), x);
}

__vpcs static v_double
xy_Z_pow (v_double x)
{
  return _ZGVnN2vv_pow (x, x);
}

__vpcs static v_double
x_Z_pow (v_double x)
{
  return _ZGVnN2vv_pow (x, v_double_dup (23.4));
}

__vpcs static v_double
y_Z_pow (v_double x)
{
  return _ZGVnN2vv_pow (v_double_dup (2.34), x);
}

__vpcs static v_float
_Z_sincosf_wrap (v_float x)
{
  v_float s, c;
  _ZGVnN4vl4l4_sincosf (x, &s, &c);
  return s + c;
}

__vpcs static v_float
_Z_cexpif_wrap (v_float x)
{
  __f32x4x2_t sc = _ZGVnN4v_cexpif (x);
  return sc.val[0] + sc.val[1];
}

__vpcs static v_double
_Z_sincos_wrap (v_double x)
{
  v_double s, c;
  _ZGVnN2vl8l8_sincos (x, &s, &c);
  return s + c;
}

__vpcs static v_double
_Z_cexpi_wrap (v_double x)
{
  __f64x2x2_t sc = _ZGVnN2v_cexpi (x);
  return sc.val[0] + sc.val[1];
}

#endif // __arch64__ && __vpcs

#if WANT_SVE_MATH

static sv_float
_Z_sv_atan2f_wrap (sv_float x, sv_bool pg)
{
  return _ZGVsMxvv_atan2f (x, svdup_f32 (5.0f), pg);
}

static sv_double
_Z_sv_atan2_wrap (sv_double x, sv_bool pg)
{
  return _ZGVsMxvv_atan2 (x, svdup_f64 (5.0), pg);
}

static sv_float
_Z_sv_hypotf_wrap (sv_float x, sv_bool pg)
{
  return _ZGVsMxvv_hypotf (x, svdup_f32 (5.0), pg);
}

static sv_double
_Z_sv_hypot_wrap (sv_double x, sv_bool pg)
{
  return _ZGVsMxvv_hypot (x, svdup_f64 (5.0), pg);
}

static sv_float
_Z_sv_powi_wrap (sv_float x, sv_bool pg)
{
  return _ZGVsMxvv_powi (x, svcvt_s32_f32_x (pg, x), pg);
}

static sv_double
_Z_sv_powk_wrap (sv_double x, sv_bool pg)
{
  return _ZGVsMxvv_powk (x, svcvt_s64_f64_x (pg, x), pg);
}

static sv_float
xy_Z_sv_powf (sv_float x, sv_bool pg)
{
  return _ZGVsMxvv_powf (x, x, pg);
}

static sv_float
x_Z_sv_powf (sv_float x, sv_bool pg)
{
  return _ZGVsMxvv_powf (x, svdup_f32 (23.4f), pg);
}

static sv_float
y_Z_sv_powf (sv_float x, sv_bool pg)
{
  return _ZGVsMxvv_powf (svdup_f32 (2.34f), x, pg);
}

static sv_double
xy_Z_sv_pow (sv_double x, sv_bool pg)
{
  return _ZGVsMxvv_pow (x, x, pg);
}

static sv_double
x_Z_sv_pow (sv_double x, sv_bool pg)
{
  return _ZGVsMxvv_pow (x, svdup_f64 (23.4), pg);
}

static sv_double
y_Z_sv_pow (sv_double x, sv_bool pg)
{
  return _ZGVsMxvv_pow (svdup_f64 (2.34), x, pg);
}

static sv_float
_Z_sv_sincosf_wrap (sv_float x, sv_bool pg)
{
  float s[svcntw ()], c[svcntw ()];
  _ZGVsMxvl4l4_sincosf (x, s, c, pg);
  return svadd_x (pg, svld1 (pg, s), svld1 (pg, s));
}

static sv_float
_Z_sv_cexpif_wrap (sv_float x, sv_bool pg)
{
  svfloat32x2_t sc = _ZGVsMxv_cexpif (x, pg);
  return svadd_x (pg, svget2 (sc, 0), svget2 (sc, 1));
}

static sv_double
_Z_sv_sincos_wrap (sv_double x, sv_bool pg)
{
  double s[svcntd ()], c[svcntd ()];
  _ZGVsMxvl8l8_sincos (x, s, c, pg);
  return svadd_x (pg, svld1 (pg, s), svld1 (pg, s));
}

static sv_double
_Z_sv_cexpi_wrap (sv_double x, sv_bool pg)
{
  svfloat64x2_t sc = _ZGVsMxv_cexpi (x, pg);
  return svadd_x (pg, svget2 (sc, 0), svget2 (sc, 1));
}

#endif // WANT_SVE_MATH
