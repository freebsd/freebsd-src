/*
 * Vector math abstractions.
 *
 * Copyright (c) 2019-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef _V_MATH_H
#define _V_MATH_H

#if !__aarch64__
# error "Cannot build without AArch64"
#endif

#define VPCS_ATTR __attribute__ ((aarch64_vector_pcs))

#define V_NAME_F1(fun) _ZGVnN4v_##fun##f
#define V_NAME_D1(fun) _ZGVnN2v_##fun
#define V_NAME_F2(fun) _ZGVnN4vv_##fun##f
#define V_NAME_D2(fun) _ZGVnN2vv_##fun
#define V_NAME_F1_L1(fun) _ZGVnN4vl4_##fun##f
#define V_NAME_D1_L1(fun) _ZGVnN2vl8_##fun

#if USE_GLIBC_ABI

# define HALF_WIDTH_ALIAS_F1(fun)                                             \
    float32x2_t VPCS_ATTR _ZGVnN2v_##fun##f (float32x2_t x)                   \
    {                                                                         \
      return vget_low_f32 (_ZGVnN4v_##fun##f (vcombine_f32 (x, x)));          \
    }

# define HALF_WIDTH_ALIAS_F2(fun)                                             \
    float32x2_t VPCS_ATTR _ZGVnN2vv_##fun##f (float32x2_t x, float32x2_t y)   \
    {                                                                         \
      return vget_low_f32 (                                                   \
	  _ZGVnN4vv_##fun##f (vcombine_f32 (x, x), vcombine_f32 (y, y)));     \
    }

#else
# define HALF_WIDTH_ALIAS_F1(fun)
# define HALF_WIDTH_ALIAS_F2(fun)
#endif

#include <stdint.h>
#include "math_config.h"
#include <arm_neon.h>

/* Shorthand helpers for declaring constants.  */
#define V2(X)                                                                 \
  {                                                                           \
    X, X                                                                      \
  }
#define V4(X)                                                                 \
  {                                                                           \
    X, X, X, X                                                                \
  }
#define V8(X)                                                                 \
  {                                                                           \
    X, X, X, X, X, X, X, X                                                    \
  }

static inline int
v_any_u16h (uint16x4_t x)
{
  return vget_lane_u64 (vreinterpret_u64_u16 (x), 0) != 0;
}

static inline int
v_lanes32 (void)
{
  return 4;
}

static inline float32x4_t
v_f32 (float x)
{
  return (float32x4_t) V4 (x);
}
static inline uint32x4_t
v_u32 (uint32_t x)
{
  return (uint32x4_t) V4 (x);
}
static inline int32x4_t
v_s32 (int32_t x)
{
  return (int32x4_t) V4 (x);
}

/* true if any elements of a v_cond result is non-zero.  */
static inline int
v_any_u32 (uint32x4_t x)
{
  /* assume elements in x are either 0 or -1u.  */
  return vpaddd_u64 (vreinterpretq_u64_u32 (x)) != 0;
}
static inline int
v_any_u32h (uint32x2_t x)
{
  return vget_lane_u64 (vreinterpret_u64_u32 (x), 0) != 0;
}
static inline float32x4_t
v_lookup_f32 (const float *tab, uint32x4_t idx)
{
  return (float32x4_t){ tab[idx[0]], tab[idx[1]], tab[idx[2]], tab[idx[3]] };
}
static inline uint32x4_t
v_lookup_u32 (const uint32_t *tab, uint32x4_t idx)
{
  return (uint32x4_t){ tab[idx[0]], tab[idx[1]], tab[idx[2]], tab[idx[3]] };
}
static inline float32x4_t
v_call_f32 (float (*f) (float), float32x4_t x, float32x4_t y, uint32x4_t p)
{
  return (float32x4_t){ p[0] ? f (x[0]) : y[0], p[1] ? f (x[1]) : y[1],
			p[2] ? f (x[2]) : y[2], p[3] ? f (x[3]) : y[3] };
}
static inline float32x4_t
v_call2_f32 (float (*f) (float, float), float32x4_t x1, float32x4_t x2,
	     float32x4_t y, uint32x4_t p)
{
  return (float32x4_t){ p[0] ? f (x1[0], x2[0]) : y[0],
			p[1] ? f (x1[1], x2[1]) : y[1],
			p[2] ? f (x1[2], x2[2]) : y[2],
			p[3] ? f (x1[3], x2[3]) : y[3] };
}
static inline float32x4_t
v_zerofy_f32 (float32x4_t x, uint32x4_t mask)
{
  return vreinterpretq_f32_u32 (vbicq_u32 (vreinterpretq_u32_f32 (x), mask));
}

static inline int
v_lanes64 (void)
{
  return 2;
}
static inline float64x2_t
v_f64 (double x)
{
  return (float64x2_t) V2 (x);
}
static inline uint64x2_t
v_u64 (uint64_t x)
{
  return (uint64x2_t) V2 (x);
}
static inline int64x2_t
v_s64 (int64_t x)
{
  return (int64x2_t) V2 (x);
}

/* true if any elements of a v_cond result is non-zero.  */
static inline int
v_any_u64 (uint64x2_t x)
{
  /* assume elements in x are either 0 or -1u.  */
  return vpaddd_u64 (x) != 0;
}
static inline float64x2_t
v_lookup_f64 (const double *tab, uint64x2_t idx)
{
  return (float64x2_t){ tab[idx[0]], tab[idx[1]] };
}
static inline uint64x2_t
v_lookup_u64 (const uint64_t *tab, uint64x2_t idx)
{
  return (uint64x2_t){ tab[idx[0]], tab[idx[1]] };
}
static inline float64x2_t
v_call_f64 (double (*f) (double), float64x2_t x, float64x2_t y, uint64x2_t p)
{
  double p1 = p[1];
  double x1 = x[1];
  if (likely (p[0]))
    y[0] = f (x[0]);
  if (likely (p1))
    y[1] = f (x1);
  return y;
}

static inline float64x2_t
v_call2_f64 (double (*f) (double, double), float64x2_t x1, float64x2_t x2,
	     float64x2_t y, uint64x2_t p)
{
  double p1 = p[1];
  double x1h = x1[1];
  double x2h = x2[1];
  if (likely (p[0]))
    y[0] = f (x1[0], x2[0]);
  if (likely (p1))
    y[1] = f (x1h, x2h);
  return y;
}
static inline float64x2_t
v_zerofy_f64 (float64x2_t x, uint64x2_t mask)
{
  return vreinterpretq_f64_u64 (vbicq_u64 (vreinterpretq_u64_f64 (x), mask));
}

#endif
