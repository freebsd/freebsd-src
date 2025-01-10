/*
 * Double-precision vector log(x) function - inline version
 *
 * Copyright (c) 2019-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "math_config.h"

#ifndef V_LOG_INLINE_POLY_ORDER
#  error Cannot use inline log helper without specifying poly order (options are 4 or 5)
#endif

#if V_LOG_INLINE_POLY_ORDER == 4
#  define POLY                                                                \
    {                                                                         \
      V2 (-0x1.ffffffffcbad3p-2), V2 (0x1.555555578ed68p-2),                  \
	  V2 (-0x1.0000d3a1e7055p-2), V2 (0x1.999392d02a63ep-3)               \
    }
#elif V_LOG_INLINE_POLY_ORDER == 5
#  define POLY                                                                \
    {                                                                         \
      V2 (-0x1.ffffffffffff7p-2), V2 (0x1.55555555170d4p-2),                  \
	  V2 (-0x1.0000000399c27p-2), V2 (0x1.999b2e90e94cap-3),              \
	  V2 (-0x1.554e550bd501ep-3)                                          \
    }
#else
#  error Can only choose order 4 or 5 for log poly
#endif

struct v_log_inline_data
{
  float64x2_t poly[V_LOG_INLINE_POLY_ORDER];
  float64x2_t ln2;
  uint64x2_t off, sign_exp_mask;
};

#define V_LOG_CONSTANTS                                                       \
  {                                                                           \
    .poly = POLY, .ln2 = V2 (0x1.62e42fefa39efp-1),                           \
    .sign_exp_mask = V2 (0xfff0000000000000), .off = V2 (0x3fe6900900000000)  \
  }

#define A(i) d->poly[i]
#define N (1 << V_LOG_TABLE_BITS)
#define IndexMask (N - 1)

struct entry
{
  float64x2_t invc;
  float64x2_t logc;
};

static inline struct entry
log_lookup (uint64x2_t i)
{
  /* Since N is a power of 2, n % N = n & (N - 1).  */
  struct entry e;
  uint64_t i0 = (vgetq_lane_u64 (i, 0) >> (52 - V_LOG_TABLE_BITS)) & IndexMask;
  uint64_t i1 = (vgetq_lane_u64 (i, 1) >> (52 - V_LOG_TABLE_BITS)) & IndexMask;
  float64x2_t e0 = vld1q_f64 (&__v_log_data.table[i0].invc);
  float64x2_t e1 = vld1q_f64 (&__v_log_data.table[i1].invc);
  e.invc = vuzp1q_f64 (e0, e1);
  e.logc = vuzp2q_f64 (e0, e1);
  return e;
}

static inline float64x2_t
v_log_inline (float64x2_t x, const struct v_log_inline_data *d)
{
  float64x2_t z, r, r2, p, y, kd, hi;
  uint64x2_t ix, iz, tmp;
  int64x2_t k;
  struct entry e;

  ix = vreinterpretq_u64_f64 (x);

  /* x = 2^k z; where z is in range [Off,2*Off) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  tmp = vsubq_u64 (ix, d->off);
  k = vshrq_n_s64 (vreinterpretq_s64_u64 (tmp), 52); /* arithmetic shift.  */
  iz = vsubq_u64 (ix, vandq_u64 (tmp, d->sign_exp_mask));
  z = vreinterpretq_f64_u64 (iz);
  e = log_lookup (tmp);

  /* log(x) = log1p(z/c-1) + log(c) + k*Ln2.  */
  r = vfmaq_f64 (v_f64 (-1.0), z, e.invc);
  kd = vcvtq_f64_s64 (k);

  /* hi = r + log(c) + k*Ln2.  */
  hi = vfmaq_f64 (vaddq_f64 (e.logc, r), kd, d->ln2);
  /* y = r2*(A0 + r*A1 + r2*(A2 + r*A3 + r2*A4)) + hi.  */
  r2 = vmulq_f64 (r, r);
  y = vfmaq_f64 (A (2), A (3), r);
  p = vfmaq_f64 (A (0), A (1), r);
#if V_LOG_POLY_ORDER == 5
  y = vfmaq_f64 (y, A (4), r2);
#endif
  y = vfmaq_f64 (p, y, r2);

  return vfmaq_f64 (hi, y, r2);
}
