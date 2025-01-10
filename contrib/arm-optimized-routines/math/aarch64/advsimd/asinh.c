/*
 * Double-precision vector asinh(x) function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "test_defs.h"
#include "test_sig.h"
#include "v_math.h"

const static struct data
{
  uint64x2_t huge_bound, abs_mask, off, mask;
#if WANT_SIMD_EXCEPT
  float64x2_t tiny_bound;
#endif
  float64x2_t lc0, lc2;
  double lc1, lc3, ln2, lc4;

  float64x2_t c0, c2, c4, c6, c8, c10, c12, c14, c16, c17;
  double c1, c3, c5, c7, c9, c11, c13, c15;

} data = {

#if WANT_SIMD_EXCEPT
  .tiny_bound = V2 (0x1p-26),
#endif
  /* Even terms of polynomial s.t. asinh(x) is approximated by
     asinh(x) ~= x + x^3 * (C0 + C1 * x + C2 * x^2 + C3 * x^3 + ...).
     Generated using Remez, f = (asinh(sqrt(x)) - sqrt(x))/x^(3/2).  */

  .c0 = V2 (-0x1.55555555554a7p-3),
  .c1 = 0x1.3333333326c7p-4,
  .c2 = V2 (-0x1.6db6db68332e6p-5),
  .c3 = 0x1.f1c71b26fb40dp-6,
  .c4 = V2 (-0x1.6e8b8b654a621p-6),
  .c5 = 0x1.1c4daa9e67871p-6,
  .c6 = V2 (-0x1.c9871d10885afp-7),
  .c7 = 0x1.7a16e8d9d2ecfp-7,
  .c8 = V2 (-0x1.3ddca533e9f54p-7),
  .c9 = 0x1.0becef748dafcp-7,
  .c10 = V2 (-0x1.b90c7099dd397p-8),
  .c11 = 0x1.541f2bb1ffe51p-8,
  .c12 = V2 (-0x1.d217026a669ecp-9),
  .c13 = 0x1.0b5c7977aaf7p-9,
  .c14 = V2 (-0x1.e0f37daef9127p-11),
  .c15 = 0x1.388b5fe542a6p-12,
  .c16 = V2 (-0x1.021a48685e287p-14),
  .c17 = V2 (0x1.93d4ba83d34dap-18),

  .lc0 = V2 (-0x1.ffffffffffff7p-2),
  .lc1 = 0x1.55555555170d4p-2,
  .lc2 = V2 (-0x1.0000000399c27p-2),
  .lc3 = 0x1.999b2e90e94cap-3,
  .lc4 = -0x1.554e550bd501ep-3,
  .ln2 = 0x1.62e42fefa39efp-1,

  .off = V2 (0x3fe6900900000000),
  .huge_bound = V2 (0x5fe0000000000000),
  .abs_mask = V2 (0x7fffffffffffffff),
  .mask = V2 (0xfffULL << 52),
};

static float64x2_t NOINLINE VPCS_ATTR
special_case (float64x2_t x, float64x2_t y, uint64x2_t abs_mask,
	      uint64x2_t special)
{
  /* Copy sign.  */
  y = vbslq_f64 (abs_mask, y, x);
  return v_call_f64 (asinh, x, y, special);
}

#define N (1 << V_LOG_TABLE_BITS)
#define IndexMask (N - 1)

struct entry
{
  float64x2_t invc;
  float64x2_t logc;
};

static inline struct entry
lookup (uint64x2_t i)
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
log_inline (float64x2_t xm, const struct data *d)
{

  uint64x2_t u = vreinterpretq_u64_f64 (xm);
  uint64x2_t u_off = vsubq_u64 (u, d->off);

  int64x2_t k = vshrq_n_s64 (vreinterpretq_s64_u64 (u_off), 52);
  uint64x2_t iz = vsubq_u64 (u, vandq_u64 (u_off, d->mask));
  float64x2_t z = vreinterpretq_f64_u64 (iz);

  struct entry e = lookup (u_off);

  /* log(x) = log1p(z/c-1) + log(c) + k*Ln2.  */
  float64x2_t r = vfmaq_f64 (v_f64 (-1.0), z, e.invc);
  float64x2_t kd = vcvtq_f64_s64 (k);

  /* hi = r + log(c) + k*Ln2.  */
  float64x2_t ln2_and_lc4 = vld1q_f64 (&d->ln2);
  float64x2_t hi = vfmaq_laneq_f64 (vaddq_f64 (e.logc, r), kd, ln2_and_lc4, 0);

  /* y = r2*(A0 + r*A1 + r2*(A2 + r*A3 + r2*A4)) + hi.  */
  float64x2_t odd_coeffs = vld1q_f64 (&d->lc1);
  float64x2_t r2 = vmulq_f64 (r, r);
  float64x2_t y = vfmaq_laneq_f64 (d->lc2, r, odd_coeffs, 1);
  float64x2_t p = vfmaq_laneq_f64 (d->lc0, r, odd_coeffs, 0);
  y = vfmaq_laneq_f64 (y, r2, ln2_and_lc4, 1);
  y = vfmaq_f64 (p, r2, y);
  return vfmaq_f64 (hi, y, r2);
}

/* Double-precision implementation of vector asinh(x).
   asinh is very sensitive around 1, so it is impractical to devise a single
   low-cost algorithm which is sufficiently accurate on a wide range of input.
   Instead we use two different algorithms:
   asinh(x) = sign(x) * log(|x| + sqrt(x^2 + 1)      if |x| >= 1
	    = sign(x) * (|x| + |x|^3 * P(x^2))       otherwise
   where log(x) is an optimized log approximation, and P(x) is a polynomial
   shared with the scalar routine. The greatest observed error 2.79 ULP, in
   |x| >= 1:
   _ZGVnN2v_asinh(0x1.2cd9d73ea76a6p+0) got 0x1.ffffd003219dap-1
				       want  0x1.ffffd003219ddp-1.  */
VPCS_ATTR float64x2_t V_NAME_D1 (asinh) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);
  float64x2_t ax = vabsq_f64 (x);

  uint64x2_t gt1 = vcgeq_f64 (ax, v_f64 (1));

#if WANT_SIMD_EXCEPT
  uint64x2_t iax = vreinterpretq_u64_f64 (ax);
  uint64x2_t special = vcgeq_u64 (iax, (d->huge_bound));
  uint64x2_t tiny = vcltq_f64 (ax, d->tiny_bound);
  special = vorrq_u64 (special, tiny);
#else
  uint64x2_t special = vcgeq_f64 (ax, vreinterpretq_f64_u64 (d->huge_bound));
#endif

  /* Option 1: |x| >= 1.
     Compute asinh(x) according by asinh(x) = log(x + sqrt(x^2 + 1)).
     If WANT_SIMD_EXCEPT is enabled, sidestep special values, which will
     overflow, by setting special lanes to 1. These will be fixed later.  */
  float64x2_t option_1 = v_f64 (0);
  if (likely (v_any_u64 (gt1)))
    {
#if WANT_SIMD_EXCEPT
      float64x2_t xm = v_zerofy_f64 (ax, special);
#else
      float64x2_t xm = ax;
#endif
      option_1 = log_inline (
	  vaddq_f64 (xm, vsqrtq_f64 (vfmaq_f64 (v_f64 (1), xm, xm))), d);
    }

  /* Option 2: |x| < 1.
     Compute asinh(x) using a polynomial.
     If WANT_SIMD_EXCEPT is enabled, sidestep special lanes, which will
     overflow, and tiny lanes, which will underflow, by setting them to 0. They
     will be fixed later, either by selecting x or falling back to the scalar
     special-case. The largest observed error in this region is 1.47 ULPs:
     _ZGVnN2v_asinh(0x1.fdfcd00cc1e6ap-1) got 0x1.c1d6bf874019bp-1
					 want 0x1.c1d6bf874019cp-1.  */
  float64x2_t option_2 = v_f64 (0);

  if (likely (v_any_u64 (vceqzq_u64 (gt1))))
    {

#if WANT_SIMD_EXCEPT
      ax = v_zerofy_f64 (ax, vorrq_u64 (tiny, gt1));
#endif
      float64x2_t x2 = vmulq_f64 (ax, ax), z2 = vmulq_f64 (x2, x2);
      /* Order-17 Pairwise Horner scheme.  */
      float64x2_t c13 = vld1q_f64 (&d->c1);
      float64x2_t c57 = vld1q_f64 (&d->c5);
      float64x2_t c911 = vld1q_f64 (&d->c9);
      float64x2_t c1315 = vld1q_f64 (&d->c13);

      float64x2_t p01 = vfmaq_laneq_f64 (d->c0, x2, c13, 0);
      float64x2_t p23 = vfmaq_laneq_f64 (d->c2, x2, c13, 1);
      float64x2_t p45 = vfmaq_laneq_f64 (d->c4, x2, c57, 0);
      float64x2_t p67 = vfmaq_laneq_f64 (d->c6, x2, c57, 1);
      float64x2_t p89 = vfmaq_laneq_f64 (d->c8, x2, c911, 0);
      float64x2_t p1011 = vfmaq_laneq_f64 (d->c10, x2, c911, 1);
      float64x2_t p1213 = vfmaq_laneq_f64 (d->c12, x2, c1315, 0);
      float64x2_t p1415 = vfmaq_laneq_f64 (d->c14, x2, c1315, 1);
      float64x2_t p1617 = vfmaq_f64 (d->c16, x2, d->c17);

      float64x2_t p = vfmaq_f64 (p1415, z2, p1617);
      p = vfmaq_f64 (p1213, z2, p);
      p = vfmaq_f64 (p1011, z2, p);
      p = vfmaq_f64 (p89, z2, p);

      p = vfmaq_f64 (p67, z2, p);
      p = vfmaq_f64 (p45, z2, p);

      p = vfmaq_f64 (p23, z2, p);

      p = vfmaq_f64 (p01, z2, p);
      option_2 = vfmaq_f64 (ax, p, vmulq_f64 (ax, x2));
#if WANT_SIMD_EXCEPT
      option_2 = vbslq_f64 (tiny, x, option_2);
#endif
    }

  /* Choose the right option for each lane.  */
  float64x2_t y = vbslq_f64 (gt1, option_1, option_2);
  if (unlikely (v_any_u64 (special)))
    {
      return special_case (x, y, d->abs_mask, special);
    }
  /* Copy sign.  */
  return vbslq_f64 (d->abs_mask, y, x);
}

TEST_SIG (V, D, 1, asinh, -10.0, 10.0)
TEST_ULP (V_NAME_D1 (asinh), 2.29)
TEST_DISABLE_FENV_IF_NOT (V_NAME_D1 (asinh), WANT_SIMD_EXCEPT)
TEST_SYM_INTERVAL (V_NAME_D1 (asinh), 0, 0x1p-26, 50000)
TEST_SYM_INTERVAL (V_NAME_D1 (asinh), 0x1p-26, 1, 50000)
TEST_SYM_INTERVAL (V_NAME_D1 (asinh), 1, 0x1p511, 50000)
TEST_SYM_INTERVAL (V_NAME_D1 (asinh), 0x1p511, inf, 40000)
/* Test vector asinh 3 times, with control lane < 1, > 1 and special.
   Ensures the v_sel is choosing the right option in all cases.  */
TEST_CONTROL_VALUE (V_NAME_D1 (asinh), 0.5)
TEST_CONTROL_VALUE (V_NAME_D1 (asinh), 2)
TEST_CONTROL_VALUE (V_NAME_D1 (asinh), 0x1p600)
