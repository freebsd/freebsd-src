/*
 * Wrapper functions for SVE ACLE.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef SV_MATH_H
#define SV_MATH_H

#ifndef WANT_VMATH
/* Enable the build of vector math code.  */
#define WANT_VMATH 1
#endif
#if WANT_VMATH

#if WANT_SVE_MATH
#define SV_SUPPORTED 1

#include <arm_sve.h>
#include <stdbool.h>

#include "math_config.h"

typedef float f32_t;
typedef uint32_t u32_t;
typedef int32_t s32_t;
typedef double f64_t;
typedef uint64_t u64_t;
typedef int64_t s64_t;

typedef svfloat64_t sv_f64_t;
typedef svuint64_t sv_u64_t;
typedef svint64_t sv_s64_t;

typedef svfloat32_t sv_f32_t;
typedef svuint32_t sv_u32_t;
typedef svint32_t sv_s32_t;

/* Double precision.  */
static inline sv_s64_t
sv_s64 (s64_t x)
{
  return svdup_n_s64 (x);
}

static inline sv_u64_t
sv_u64 (u64_t x)
{
  return svdup_n_u64 (x);
}

static inline sv_f64_t
sv_f64 (f64_t x)
{
  return svdup_n_f64 (x);
}

static inline sv_f64_t
sv_fma_f64_x (svbool_t pg, sv_f64_t x, sv_f64_t y, sv_f64_t z)
{
  return svmla_f64_x (pg, z, x, y);
}

/* res = z + x * y with x scalar. */
static inline sv_f64_t
sv_fma_n_f64_x (svbool_t pg, f64_t x, sv_f64_t y, sv_f64_t z)
{
  return svmla_n_f64_x (pg, z, y, x);
}

static inline sv_s64_t
sv_as_s64_u64 (sv_u64_t x)
{
  return svreinterpret_s64_u64 (x);
}

static inline sv_u64_t
sv_as_u64_f64 (sv_f64_t x)
{
  return svreinterpret_u64_f64 (x);
}

static inline sv_f64_t
sv_as_f64_u64 (sv_u64_t x)
{
  return svreinterpret_f64_u64 (x);
}

static inline sv_f64_t
sv_to_f64_s64_x (svbool_t pg, sv_s64_t s)
{
  return svcvt_f64_x (pg, s);
}

static inline sv_f64_t
sv_call_f64 (f64_t (*f) (f64_t), sv_f64_t x, sv_f64_t y, svbool_t cmp)
{
  svbool_t p = svpfirst (cmp, svpfalse ());
  while (svptest_any (cmp, p))
    {
      f64_t elem = svclastb_n_f64 (p, 0, x);
      elem = (*f) (elem);
      sv_f64_t y2 = svdup_n_f64 (elem);
      y = svsel_f64 (p, y2, y);
      p = svpnext_b64 (cmp, p);
    }
  return y;
}

static inline sv_f64_t
sv_call2_f64 (f64_t (*f) (f64_t, f64_t), sv_f64_t x1, sv_f64_t x2, sv_f64_t y,
	      svbool_t cmp)
{
  svbool_t p = svpfirst (cmp, svpfalse ());
  while (svptest_any (cmp, p))
    {
      f64_t elem1 = svclastb_n_f64 (p, 0, x1);
      f64_t elem2 = svclastb_n_f64 (p, 0, x2);
      f64_t ret = (*f) (elem1, elem2);
      sv_f64_t y2 = svdup_n_f64 (ret);
      y = svsel_f64 (p, y2, y);
      p = svpnext_b64 (cmp, p);
    }
  return y;
}

/* Load array of uint64_t into svuint64_t.  */
static inline sv_u64_t
sv_lookup_u64_x (svbool_t pg, const u64_t *tab, sv_u64_t idx)
{
  return svld1_gather_u64index_u64 (pg, tab, idx);
}

/* Load array of double into svfloat64_t.  */
static inline sv_f64_t
sv_lookup_f64_x (svbool_t pg, const f64_t *tab, sv_u64_t idx)
{
  return svld1_gather_u64index_f64 (pg, tab, idx);
}

static inline sv_u64_t
sv_mod_n_u64_x (svbool_t pg, sv_u64_t x, u64_t y)
{
  sv_u64_t q = svdiv_n_u64_x (pg, x, y);
  return svmls_n_u64_x (pg, x, q, y);
}

/* Single precision.  */
static inline sv_s32_t
sv_s32 (s32_t x)
{
  return svdup_n_s32 (x);
}

static inline sv_u32_t
sv_u32 (u32_t x)
{
  return svdup_n_u32 (x);
}

static inline sv_f32_t
sv_f32 (f32_t x)
{
  return svdup_n_f32 (x);
}

static inline sv_f32_t
sv_fma_f32_x (svbool_t pg, sv_f32_t x, sv_f32_t y, sv_f32_t z)
{
  return svmla_f32_x (pg, z, x, y);
}

/* res = z + x * y with x scalar.  */
static inline sv_f32_t
sv_fma_n_f32_x (svbool_t pg, f32_t x, sv_f32_t y, sv_f32_t z)
{
  return svmla_n_f32_x (pg, z, y, x);
}

static inline sv_u32_t
sv_as_u32_f32 (sv_f32_t x)
{
  return svreinterpret_u32_f32 (x);
}

static inline sv_f32_t
sv_as_f32_u32 (sv_u32_t x)
{
  return svreinterpret_f32_u32 (x);
}

static inline sv_s32_t
sv_as_s32_u32 (sv_u32_t x)
{
  return svreinterpret_s32_u32 (x);
}

static inline sv_f32_t
sv_to_f32_s32_x (svbool_t pg, sv_s32_t s)
{
  return svcvt_f32_x (pg, s);
}

static inline sv_s32_t
sv_to_s32_f32_x (svbool_t pg, sv_f32_t x)
{
  return svcvt_s32_f32_x (pg, x);
}

static inline sv_f32_t
sv_call_f32 (f32_t (*f) (f32_t), sv_f32_t x, sv_f32_t y, svbool_t cmp)
{
  svbool_t p = svpfirst (cmp, svpfalse ());
  while (svptest_any (cmp, p))
    {
      f32_t elem = svclastb_n_f32 (p, 0, x);
      elem = (*f) (elem);
      sv_f32_t y2 = svdup_n_f32 (elem);
      y = svsel_f32 (p, y2, y);
      p = svpnext_b32 (cmp, p);
    }
  return y;
}

static inline sv_f32_t
sv_call2_f32 (f32_t (*f) (f32_t, f32_t), sv_f32_t x1, sv_f32_t x2, sv_f32_t y,
	      svbool_t cmp)
{
  svbool_t p = svpfirst (cmp, svpfalse ());
  while (svptest_any (cmp, p))
    {
      f32_t elem1 = svclastb_n_f32 (p, 0, x1);
      f32_t elem2 = svclastb_n_f32 (p, 0, x2);
      f32_t ret = (*f) (elem1, elem2);
      sv_f32_t y2 = svdup_n_f32 (ret);
      y = svsel_f32 (p, y2, y);
      p = svpnext_b32 (cmp, p);
    }
  return y;
}

#endif
#endif
#endif
