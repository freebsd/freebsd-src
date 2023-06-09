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

#if WANT_VMATH
#if __aarch64__

static double
__s_atan2_wrap (double x)
{
  return __s_atan2 (5.0, x);
}

static float
__s_atan2f_wrap (float x)
{
  return __s_atan2f (5.0f, x);
}

static v_double
__v_atan2_wrap (v_double x)
{
  return __v_atan2 (v_double_dup (5.0), x);
}

static v_float
__v_atan2f_wrap (v_float x)
{
  return __v_atan2f (v_float_dup (5.0f), x);
}

#ifdef __vpcs

__vpcs static v_double
__vn_atan2_wrap (v_double x)
{
  return __vn_atan2 (v_double_dup (5.0), x);
}

__vpcs static v_float
__vn_atan2f_wrap (v_float x)
{
  return __vn_atan2f (v_float_dup (5.0f), x);
}

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

#endif // __vpcs
#endif // __arch64__
#endif // WANT_VMATH

#if WANT_SVE_MATH

static sv_float
__sv_atan2f_wrap (sv_float x, sv_bool pg)
{
  return __sv_atan2f_x (x, svdup_n_f32 (5.0f), pg);
}

static sv_float
_Z_sv_atan2f_wrap (sv_float x, sv_bool pg)
{
  return _ZGVsMxvv_atan2f (x, svdup_n_f32 (5.0f), pg);
}

static sv_double
__sv_atan2_wrap (sv_double x, sv_bool pg)
{
  return __sv_atan2_x (x, svdup_n_f64 (5.0), pg);
}

static sv_double
_Z_sv_atan2_wrap (sv_double x, sv_bool pg)
{
  return _ZGVsMxvv_atan2 (x, svdup_n_f64 (5.0), pg);
}

static sv_float
_Z_sv_powi_wrap (sv_float x, sv_bool pg)
{
  return _ZGVsMxvv_powi (x, svcvt_s32_f32_x (pg, x), pg);
}

static sv_float
__sv_powif_wrap (sv_float x, sv_bool pg)
{
  return __sv_powif_x (x, svcvt_s32_f32_x (pg, x), pg);
}

static sv_double
_Z_sv_powk_wrap (sv_double x, sv_bool pg)
{
  return _ZGVsMxvv_powk (x, svcvt_s64_f64_x (pg, x), pg);
}

static sv_double
__sv_powi_wrap (sv_double x, sv_bool pg)
{
  return __sv_powi_x (x, svcvt_s64_f64_x (pg, x), pg);
}

#endif // WANT_SVE_MATH
