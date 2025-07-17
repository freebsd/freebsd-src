/*
 * Wrapper functions for SVE ACLE.
 *
 * Copyright (c) 2019-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef SV_MATH_H
#define SV_MATH_H

/* Enable SVE in this translation unit. Note, because this is 'pushed' in
   clang, any file including sv_math.h will have to pop it back off again by
   ending the source file with CLOSE_SVE_ATTR. It is important that sv_math.h
   is included first so that all functions have the target attribute.  */
#ifdef __clang__
# pragma clang attribute push(__attribute__((target("sve"))),                \
			       apply_to = any(function))
# define CLOSE_SVE_ATTR _Pragma("clang attribute pop")
#else
# pragma GCC target("+sve")
# define CLOSE_SVE_ATTR
#endif

#include <arm_sve.h>
#include <stdbool.h>

#include "math_config.h"

#define SV_NAME_F1(fun) _ZGVsMxv_##fun##f
#define SV_NAME_D1(fun) _ZGVsMxv_##fun
#define SV_NAME_F2(fun) _ZGVsMxvv_##fun##f
#define SV_NAME_D2(fun) _ZGVsMxvv_##fun
#define SV_NAME_F1_L1(fun) _ZGVsMxvl4_##fun##f
#define SV_NAME_D1_L1(fun) _ZGVsMxvl8_##fun
#define SV_NAME_F1_L2(fun) _ZGVsMxvl4l4_##fun##f

/* Double precision.  */
static inline svint64_t
sv_s64 (int64_t x)
{
  return svdup_s64 (x);
}

static inline svuint64_t
sv_u64 (uint64_t x)
{
  return svdup_u64 (x);
}

static inline svfloat64_t
sv_f64 (double x)
{
  return svdup_f64 (x);
}

static inline svfloat64_t
sv_call_f64 (double (*f) (double), svfloat64_t x, svfloat64_t y, svbool_t cmp)
{
  svbool_t p = svpfirst (cmp, svpfalse ());
  while (svptest_any (cmp, p))
    {
      double elem = svclastb (p, 0, x);
      elem = (*f) (elem);
      svfloat64_t y2 = sv_f64 (elem);
      y = svsel (p, y2, y);
      p = svpnext_b64 (cmp, p);
    }
  return y;
}

static inline svfloat64_t
sv_call2_f64 (double (*f) (double, double), svfloat64_t x1, svfloat64_t x2,
	      svfloat64_t y, svbool_t cmp)
{
  svbool_t p = svpfirst (cmp, svpfalse ());
  while (svptest_any (cmp, p))
    {
      double elem1 = svclastb (p, 0, x1);
      double elem2 = svclastb (p, 0, x2);
      double ret = (*f) (elem1, elem2);
      svfloat64_t y2 = sv_f64 (ret);
      y = svsel (p, y2, y);
      p = svpnext_b64 (cmp, p);
    }
  return y;
}

static inline svuint64_t
sv_mod_n_u64_x (svbool_t pg, svuint64_t x, uint64_t y)
{
  svuint64_t q = svdiv_x (pg, x, y);
  return svmls_x (pg, x, q, y);
}

/* Single precision.  */
static inline svint32_t
sv_s32 (int32_t x)
{
  return svdup_s32 (x);
}

static inline svuint32_t
sv_u32 (uint32_t x)
{
  return svdup_u32 (x);
}

static inline svfloat32_t
sv_f32 (float x)
{
  return svdup_f32 (x);
}

static inline svfloat32_t
sv_call_f32 (float (*f) (float), svfloat32_t x, svfloat32_t y, svbool_t cmp)
{
  svbool_t p = svpfirst (cmp, svpfalse ());
  while (svptest_any (cmp, p))
    {
      float elem = svclastb (p, 0, x);
      elem = (*f) (elem);
      svfloat32_t y2 = sv_f32 (elem);
      y = svsel (p, y2, y);
      p = svpnext_b32 (cmp, p);
    }
  return y;
}

static inline svfloat32_t
sv_call2_f32 (float (*f) (float, float), svfloat32_t x1, svfloat32_t x2,
	      svfloat32_t y, svbool_t cmp)
{
  svbool_t p = svpfirst (cmp, svpfalse ());
  while (svptest_any (cmp, p))
    {
      float elem1 = svclastb (p, 0, x1);
      float elem2 = svclastb (p, 0, x2);
      float ret = (*f) (elem1, elem2);
      svfloat32_t y2 = sv_f32 (ret);
      y = svsel (p, y2, y);
      p = svpnext_b32 (cmp, p);
    }
  return y;
}
#endif
