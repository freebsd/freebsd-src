/*
 * Double-precision vector log(x) function - inline version
 *
 * Copyright (c) 2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "math_config.h"

#ifndef SV_LOG_INLINE_POLY_ORDER
#  error Cannot use inline log helper without specifying poly order (options are 4 or 5)
#endif

#if SV_LOG_INLINE_POLY_ORDER == 4
#  define POLY                                                                \
    {                                                                         \
      -0x1.ffffffffcbad3p-2, 0x1.555555578ed68p-2, -0x1.0000d3a1e7055p-2,     \
	  0x1.999392d02a63ep-3                                                \
    }
#elif SV_LOG_INLINE_POLY_ORDER == 5
#  define POLY                                                                \
    {                                                                         \
      -0x1.ffffffffffff7p-2, 0x1.55555555170d4p-2, -0x1.0000000399c27p-2,     \
	  0x1.999b2e90e94cap-3, -0x1.554e550bd501ep-3                         \
    }
#else
#  error Can only choose order 4 or 5 for log poly
#endif

struct sv_log_inline_data
{
  double poly[SV_LOG_INLINE_POLY_ORDER];
  double ln2;
  uint64_t off, sign_exp_mask;
};

#define SV_LOG_CONSTANTS                                                      \
  {                                                                           \
    .poly = POLY, .ln2 = 0x1.62e42fefa39efp-1,                                \
    .sign_exp_mask = 0xfff0000000000000, .off = 0x3fe6900900000000            \
  }

#define P(i) sv_f64 (d->poly[i])
#define N (1 << V_LOG_TABLE_BITS)

static inline svfloat64_t
sv_log_inline (svbool_t pg, svfloat64_t x, const struct sv_log_inline_data *d)
{
  svuint64_t ix = svreinterpret_u64 (x);

  /* x = 2^k z; where z is in range [Off,2*Off) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  svuint64_t tmp = svsub_x (pg, ix, d->off);
  /* Calculate table index = (tmp >> (52 - V_LOG_TABLE_BITS)) % N.
     The actual value of i is double this due to table layout.  */
  svuint64_t i
      = svand_x (pg, svlsr_x (pg, tmp, (51 - V_LOG_TABLE_BITS)), (N - 1) << 1);
  svint64_t k
      = svasr_x (pg, svreinterpret_s64 (tmp), 52); /* Arithmetic shift.  */
  svuint64_t iz = svsub_x (pg, ix, svand_x (pg, tmp, 0xfffULL << 52));
  svfloat64_t z = svreinterpret_f64 (iz);

  /* Lookup in 2 global lists (length N).  */
  svfloat64_t invc = svld1_gather_index (pg, &__v_log_data.table[0].invc, i);
  svfloat64_t logc = svld1_gather_index (pg, &__v_log_data.table[0].logc, i);

  /* log(x) = log1p(z/c-1) + log(c) + k*Ln2.  */
  svfloat64_t r = svmad_x (pg, invc, z, -1);
  svfloat64_t kd = svcvt_f64_x (pg, k);
  /* hi = r + log(c) + k*Ln2.  */
  svfloat64_t hi = svmla_x (pg, svadd_x (pg, logc, r), kd, __v_log_data.ln2);
  /* y = r2*(A0 + r*A1 + r2*(A2 + r*A3 + r2*A4)) + hi.  */
  svfloat64_t r2 = svmul_x (pg, r, r);
  svfloat64_t y = svmla_x (pg, P (2), r, P (3));
  svfloat64_t p = svmla_x (pg, P (0), r, P (1));
#if SV_LOG_INLINE_POLY_ORDER == 5
  y = svmla_x (pg, P (4), r2);
#endif
  y = svmla_x (pg, p, r2, y);
  return svmla_x (pg, hi, r2, y);
}
