/*
 * Double-precision vector log10(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "include/mathlib.h"
#include "pl_sig.h"
#include "pl_test.h"

#if V_SUPPORTED

#define A(i) v_f64 (__v_log10_data.poly[i])
#define T(s, i) __v_log10_data.tab[i].s
#define Ln2 v_f64 (0x1.62e42fefa39efp-1)
#define N (1 << V_LOG10_TABLE_BITS)
#define OFF v_u64 (0x3fe6900900000000)

struct entry
{
  v_f64_t invc;
  v_f64_t log10c;
};

static inline struct entry
lookup (v_u64_t i)
{
  struct entry e;
#ifdef SCALAR
  e.invc = T (invc, i);
  e.log10c = T (log10c, i);
#else
  e.invc[0] = T (invc, i[0]);
  e.log10c[0] = T (log10c, i[0]);
  e.invc[1] = T (invc, i[1]);
  e.log10c[1] = T (log10c, i[1]);
#endif
  return e;
}

VPCS_ATTR
inline static v_f64_t
specialcase (v_f64_t x, v_f64_t y, v_u64_t cmp)
{
  return v_call_f64 (log10, x, y, cmp);
}

/* Our implementation of v_log10 is a slight modification of v_log (1.660ulps).
   Max ULP error: < 2.5 ulp (nearest rounding.)
   Maximum measured at 2.46 ulp for x in [0.96, 0.97]
     __v_log10(0x1.13192407fcb46p+0) got 0x1.fff6be3cae4bbp-6
				    want 0x1.fff6be3cae4b9p-6
     -0.459999 ulp err 1.96.  */
VPCS_ATTR
v_f64_t V_NAME (log10) (v_f64_t x)
{
  v_f64_t z, r, r2, p, y, kd, hi;
  v_u64_t ix, iz, tmp, top, i, cmp;
  v_s64_t k;
  struct entry e;

  ix = v_as_u64_f64 (x);
  top = ix >> 48;
  cmp = v_cond_u64 (top - v_u64 (0x0010) >= v_u64 (0x7ff0 - 0x0010));

  /* x = 2^k z; where z is in range [OFF,2*OFF) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  tmp = ix - OFF;
  i = (tmp >> (52 - V_LOG10_TABLE_BITS)) % N;
  k = v_as_s64_u64 (tmp) >> 52; /* arithmetic shift.  */
  iz = ix - (tmp & v_u64 (0xfffULL << 52));
  z = v_as_f64_u64 (iz);
  e = lookup (i);

  /* log10(x) = log1p(z/c-1)/log(10) + log10(c) + k*log10(2).  */
  r = v_fma_f64 (z, e.invc, v_f64 (-1.0));
  kd = v_to_f64_s64 (k);

  /* hi = r / log(10) + log10(c) + k*log10(2).
     Constants in `v_log10_data.c` are computed (in extended precision) as
     e.log10c := e.logc * ivln10.  */
  v_f64_t w = v_fma_f64 (r, v_f64 (__v_log10_data.invln10), e.log10c);

  /* y = log10(1+r) + n * log10(2).  */
  hi = v_fma_f64 (kd, v_f64 (__v_log10_data.log10_2), w);

  /* y = r2*(A0 + r*A1 + r2*(A2 + r*A3 + r2*A4)) + hi.  */
  r2 = r * r;
  y = v_fma_f64 (A (3), r, A (2));
  p = v_fma_f64 (A (1), r, A (0));
  y = v_fma_f64 (A (4), r2, y);
  y = v_fma_f64 (y, r2, p);
  y = v_fma_f64 (y, r2, hi);

  if (unlikely (v_any_u64 (cmp)))
    return specialcase (x, y, cmp);
  return y;
}
VPCS_ALIAS

PL_SIG (V, D, 1, log10, 0.01, 11.1)
PL_TEST_ULP (V_NAME (log10), 1.97)
PL_TEST_EXPECT_FENV_ALWAYS (V_NAME (log10))
PL_TEST_INTERVAL (V_NAME (log10), 0, 0xffff000000000000, 10000)
PL_TEST_INTERVAL (V_NAME (log10), 0x1p-4, 0x1p4, 400000)
PL_TEST_INTERVAL (V_NAME (log10), 0, inf, 400000)
#endif
