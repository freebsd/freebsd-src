/*
 * Double-precision vector log(1+x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "poly_advsimd_f64.h"
#include "pl_sig.h"
#include "pl_test.h"

const static struct data
{
  float64x2_t poly[19], ln2[2];
  uint64x2_t hf_rt2_top, one_m_hf_rt2_top, umask, inf, minus_one;
  int64x2_t one_top;
} data = {
  /* Generated using Remez, deg=20, in [sqrt(2)/2-1, sqrt(2)-1].  */
  .poly = { V2 (-0x1.ffffffffffffbp-2), V2 (0x1.55555555551a9p-2),
	    V2 (-0x1.00000000008e3p-2), V2 (0x1.9999999a32797p-3),
	    V2 (-0x1.555555552fecfp-3), V2 (0x1.249248e071e5ap-3),
	    V2 (-0x1.ffffff8bf8482p-4), V2 (0x1.c71c8f07da57ap-4),
	    V2 (-0x1.9999ca4ccb617p-4), V2 (0x1.7459ad2e1dfa3p-4),
	    V2 (-0x1.554d2680a3ff2p-4), V2 (0x1.3b4c54d487455p-4),
	    V2 (-0x1.2548a9ffe80e6p-4), V2 (0x1.0f389a24b2e07p-4),
	    V2 (-0x1.eee4db15db335p-5), V2 (0x1.e95b494d4a5ddp-5),
	    V2 (-0x1.15fdf07cb7c73p-4), V2 (0x1.0310b70800fcfp-4),
	    V2 (-0x1.cfa7385bdb37ep-6) },
  .ln2 = { V2 (0x1.62e42fefa3800p-1), V2 (0x1.ef35793c76730p-45) },
  /* top32(asuint64(sqrt(2)/2)) << 32.  */
  .hf_rt2_top = V2 (0x3fe6a09e00000000),
  /* (top32(asuint64(1)) - top32(asuint64(sqrt(2)/2))) << 32.  */
  .one_m_hf_rt2_top = V2 (0x00095f6200000000),
  .umask = V2 (0x000fffff00000000),
  .one_top = V2 (0x3ff),
  .inf = V2 (0x7ff0000000000000),
  .minus_one = V2 (0xbff0000000000000)
};

#define BottomMask v_u64 (0xffffffff)

static float64x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t x, float64x2_t y, uint64x2_t special)
{
  return v_call_f64 (log1p, x, y, special);
}

/* Vector log1p approximation using polynomial on reduced interval. Routine is
   a modification of the algorithm used in scalar log1p, with no shortcut for
   k=0 and no narrowing for f and k. Maximum observed error is 2.45 ULP:
   _ZGVnN2v_log1p(0x1.658f7035c4014p+11) got 0x1.fd61d0727429dp+2
					want 0x1.fd61d0727429fp+2 .  */
VPCS_ATTR float64x2_t V_NAME_D1 (log1p) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);
  uint64x2_t ix = vreinterpretq_u64_f64 (x);
  uint64x2_t ia = vreinterpretq_u64_f64 (vabsq_f64 (x));
  uint64x2_t special = vcgeq_u64 (ia, d->inf);

#if WANT_SIMD_EXCEPT
  special = vorrq_u64 (special,
		       vcgeq_u64 (ix, vreinterpretq_u64_f64 (v_f64 (-1))));
  if (unlikely (v_any_u64 (special)))
    x = v_zerofy_f64 (x, special);
#else
  special = vorrq_u64 (special, vcleq_f64 (x, v_f64 (-1)));
#endif

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
  float64x2_t m = vaddq_f64 (x, v_f64 (1));
  uint64x2_t mi = vreinterpretq_u64_f64 (m);
  uint64x2_t u = vaddq_u64 (mi, d->one_m_hf_rt2_top);

  int64x2_t ki
      = vsubq_s64 (vreinterpretq_s64_u64 (vshrq_n_u64 (u, 52)), d->one_top);
  float64x2_t k = vcvtq_f64_s64 (ki);

  /* Reduce x to f in [sqrt(2)/2, sqrt(2)].  */
  uint64x2_t utop = vaddq_u64 (vandq_u64 (u, d->umask), d->hf_rt2_top);
  uint64x2_t u_red = vorrq_u64 (utop, vandq_u64 (mi, BottomMask));
  float64x2_t f = vsubq_f64 (vreinterpretq_f64_u64 (u_red), v_f64 (1));

  /* Correction term c/m.  */
  float64x2_t cm = vdivq_f64 (vsubq_f64 (x, vsubq_f64 (m, v_f64 (1))), m);

  /* Approximate log1p(x) on the reduced input using a polynomial. Because
     log1p(0)=0 we choose an approximation of the form:
       x + C0*x^2 + C1*x^3 + C2x^4 + ...
     Hence approximation has the form f + f^2 * P(f)
      where P(x) = C0 + C1*x + C2x^2 + ...
     Assembling this all correctly is dealt with at the final step.  */
  float64x2_t f2 = vmulq_f64 (f, f);
  float64x2_t p = v_pw_horner_18_f64 (f, f2, d->poly);

  float64x2_t ylo = vfmaq_f64 (cm, k, d->ln2[1]);
  float64x2_t yhi = vfmaq_f64 (f, k, d->ln2[0]);
  float64x2_t y = vaddq_f64 (ylo, yhi);

  if (unlikely (v_any_u64 (special)))
    return special_case (vreinterpretq_f64_u64 (ix), vfmaq_f64 (y, f2, p),
			 special);

  return vfmaq_f64 (y, f2, p);
}

PL_SIG (V, D, 1, log1p, -0.9, 10.0)
PL_TEST_ULP (V_NAME_D1 (log1p), 1.97)
PL_TEST_EXPECT_FENV (V_NAME_D1 (log1p), WANT_SIMD_EXCEPT)
PL_TEST_SYM_INTERVAL (V_NAME_D1 (log1p), 0.0, 0x1p-23, 50000)
PL_TEST_SYM_INTERVAL (V_NAME_D1 (log1p), 0x1p-23, 0.001, 50000)
PL_TEST_SYM_INTERVAL (V_NAME_D1 (log1p), 0.001, 1.0, 50000)
PL_TEST_INTERVAL (V_NAME_D1 (log1p), 1, inf, 40000)
PL_TEST_INTERVAL (V_NAME_D1 (log1p), -1.0, -inf, 500)
