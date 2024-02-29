/*
 * Double-precision SVE log(1+x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "poly_sve_f64.h"
#include "pl_sig.h"
#include "pl_test.h"

static const struct data
{
  double poly[19];
  double ln2_hi, ln2_lo;
  uint64_t hfrt2_top, onemhfrt2_top, inf, mone;
} data = {
  /* Generated using Remez in [ sqrt(2)/2 - 1, sqrt(2) - 1]. Order 20
     polynomial, however first 2 coefficients are 0 and 1 so are not stored.  */
  .poly = { -0x1.ffffffffffffbp-2, 0x1.55555555551a9p-2, -0x1.00000000008e3p-2,
	    0x1.9999999a32797p-3, -0x1.555555552fecfp-3, 0x1.249248e071e5ap-3,
	    -0x1.ffffff8bf8482p-4, 0x1.c71c8f07da57ap-4, -0x1.9999ca4ccb617p-4,
	    0x1.7459ad2e1dfa3p-4, -0x1.554d2680a3ff2p-4, 0x1.3b4c54d487455p-4,
	    -0x1.2548a9ffe80e6p-4, 0x1.0f389a24b2e07p-4, -0x1.eee4db15db335p-5,
	    0x1.e95b494d4a5ddp-5, -0x1.15fdf07cb7c73p-4, 0x1.0310b70800fcfp-4,
	    -0x1.cfa7385bdb37ep-6, },
  .ln2_hi = 0x1.62e42fefa3800p-1,
  .ln2_lo = 0x1.ef35793c76730p-45,
  /* top32(asuint64(sqrt(2)/2)) << 32.  */
  .hfrt2_top = 0x3fe6a09e00000000,
  /* (top32(asuint64(1)) - top32(asuint64(sqrt(2)/2))) << 32.  */
  .onemhfrt2_top = 0x00095f6200000000,
  .inf = 0x7ff0000000000000,
  .mone = 0xbff0000000000000,
};

#define AbsMask 0x7fffffffffffffff
#define BottomMask 0xffffffff

static svfloat64_t NOINLINE
special_case (svbool_t special, svfloat64_t x, svfloat64_t y)
{
  return sv_call_f64 (log1p, x, y, special);
}

/* Vector approximation for log1p using polynomial on reduced interval. Maximum
   observed error is 2.46 ULP:
   _ZGVsMxv_log1p(0x1.654a1307242a4p+11) got 0x1.fd5565fb590f4p+2
					want 0x1.fd5565fb590f6p+2.  */
svfloat64_t SV_NAME_D1 (log1p) (svfloat64_t x, svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);
  svuint64_t ix = svreinterpret_u64 (x);
  svuint64_t ax = svand_x (pg, ix, AbsMask);
  svbool_t special
      = svorr_z (pg, svcmpge (pg, ax, d->inf), svcmpge (pg, ix, d->mone));

  /* With x + 1 = t * 2^k (where t = f + 1 and k is chosen such that f
			   is in [sqrt(2)/2, sqrt(2)]):
     log1p(x) = k*log(2) + log1p(f).

     f may not be representable exactly, so we need a correction term:
     let m = round(1 + x), c = (1 + x) - m.
     c << m: at very small x, log1p(x) ~ x, hence:
     log(1+x) - log(m) ~ c/m.

     We therefore calculate log1p(x) by k*log2 + log1p(f) + c/m.  */

  /* Obtain correctly scaled k by manipulation in the exponent.
     The scalar algorithm casts down to 32-bit at this point to calculate k and
     u_red. We stay in double-width to obtain f and k, using the same constants
     as the scalar algorithm but shifted left by 32.  */
  svfloat64_t m = svadd_x (pg, x, 1);
  svuint64_t mi = svreinterpret_u64 (m);
  svuint64_t u = svadd_x (pg, mi, d->onemhfrt2_top);

  svint64_t ki = svsub_x (pg, svreinterpret_s64 (svlsr_x (pg, u, 52)), 0x3ff);
  svfloat64_t k = svcvt_f64_x (pg, ki);

  /* Reduce x to f in [sqrt(2)/2, sqrt(2)].  */
  svuint64_t utop
      = svadd_x (pg, svand_x (pg, u, 0x000fffff00000000), d->hfrt2_top);
  svuint64_t u_red = svorr_x (pg, utop, svand_x (pg, mi, BottomMask));
  svfloat64_t f = svsub_x (pg, svreinterpret_f64 (u_red), 1);

  /* Correction term c/m.  */
  svfloat64_t cm = svdiv_x (pg, svsub_x (pg, x, svsub_x (pg, m, 1)), m);

  /* Approximate log1p(x) on the reduced input using a polynomial. Because
     log1p(0)=0 we choose an approximation of the form:
	x + C0*x^2 + C1*x^3 + C2x^4 + ...
     Hence approximation has the form f + f^2 * P(f)
     where P(x) = C0 + C1*x + C2x^2 + ...
     Assembling this all correctly is dealt with at the final step.  */
  svfloat64_t f2 = svmul_x (pg, f, f), f4 = svmul_x (pg, f2, f2),
	      f8 = svmul_x (pg, f4, f4), f16 = svmul_x (pg, f8, f8);
  svfloat64_t p = sv_estrin_18_f64_x (pg, f, f2, f4, f8, f16, d->poly);

  svfloat64_t ylo = svmla_x (pg, cm, k, d->ln2_lo);
  svfloat64_t yhi = svmla_x (pg, f, k, d->ln2_hi);
  svfloat64_t y = svmla_x (pg, svadd_x (pg, ylo, yhi), f2, p);

  if (unlikely (svptest_any (pg, special)))
    return special_case (special, x, y);

  return y;
}

PL_SIG (SV, D, 1, log1p, -0.9, 10.0)
PL_TEST_ULP (SV_NAME_D1 (log1p), 1.97)
PL_TEST_SYM_INTERVAL (SV_NAME_D1 (log1p), 0.0, 0x1p-23, 50000)
PL_TEST_SYM_INTERVAL (SV_NAME_D1 (log1p), 0x1p-23, 0.001, 50000)
PL_TEST_SYM_INTERVAL (SV_NAME_D1 (log1p), 0.001, 1.0, 50000)
PL_TEST_INTERVAL (SV_NAME_D1 (log1p), 1, inf, 10000)
PL_TEST_INTERVAL (SV_NAME_D1 (log1p), -1, -inf, 10)
