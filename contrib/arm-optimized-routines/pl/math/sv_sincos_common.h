/*
 * Core approximation for double-precision vector sincos
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "poly_sve_f64.h"

static const struct sv_sincos_data
{
  double sin_poly[7], cos_poly[6], pio2[3];
  double inv_pio2, shift, range_val;
} sv_sincos_data = {
  .inv_pio2 = 0x1.45f306dc9c882p-1,
  .pio2 = { 0x1.921fb50000000p+0, 0x1.110b460000000p-26,
	    0x1.1a62633145c07p-54 },
  .shift = 0x1.8p52,
  .sin_poly = { /* Computed using Remez in [-pi/2, pi/2].  */
	        -0x1.555555555547bp-3, 0x1.1111111108a4dp-7,
		-0x1.a01a019936f27p-13, 0x1.71de37a97d93ep-19,
		-0x1.ae633919987c6p-26, 0x1.60e277ae07cecp-33,
		-0x1.9e9540300a1p-41 },
  .cos_poly = { /* Computed using Remez in [-pi/4, pi/4].  */
	        0x1.555555555554cp-5, -0x1.6c16c16c1521fp-10,
		0x1.a01a019cbf62ap-16, -0x1.27e4f812b681ep-22,
		0x1.1ee9f152a57cdp-29, -0x1.8fb131098404bp-37 },
  .range_val = 0x1p23, };

static inline svbool_t
check_ge_rangeval (svbool_t pg, svfloat64_t x, const struct sv_sincos_data *d)
{
  svbool_t in_bounds = svaclt (pg, x, d->range_val);
  return svnot_z (pg, in_bounds);
}

/* Double-precision vector function allowing calculation of both sin and cos in
   one function call, using shared argument reduction and separate polynomials.
   Largest observed error is for sin, 3.22 ULP:
   v_sincos_sin (0x1.d70eef40f39b1p+12) got -0x1.ffe9537d5dbb7p-3
				       want -0x1.ffe9537d5dbb4p-3.  */
static inline svfloat64x2_t
sv_sincos_inline (svbool_t pg, svfloat64_t x, const struct sv_sincos_data *d)
{
  /* q = nearest integer to 2 * x / pi.  */
  svfloat64_t q = svsub_x (pg, svmla_x (pg, sv_f64 (d->shift), x, d->inv_pio2),
			   d->shift);
  svint64_t n = svcvt_s64_x (pg, q);

  /* Reduce x such that r is in [ -pi/4, pi/4 ].  */
  svfloat64_t r = x;
  r = svmls_x (pg, r, q, d->pio2[0]);
  r = svmls_x (pg, r, q, d->pio2[1]);
  r = svmls_x (pg, r, q, d->pio2[2]);

  svfloat64_t r2 = svmul_x (pg, r, r), r3 = svmul_x (pg, r2, r),
	      r4 = svmul_x (pg, r2, r2);

  /* Approximate sin(r) ~= r + r^3 * poly_sin(r^2).  */
  svfloat64_t s = sv_pw_horner_6_f64_x (pg, r2, r4, d->sin_poly);
  s = svmla_x (pg, r, r3, s);

  /* Approximate cos(r) ~= 1 - (r^2)/2 + r^4 * poly_cos(r^2).  */
  svfloat64_t c = sv_pw_horner_5_f64_x (pg, r2, r4, d->cos_poly);
  c = svmad_x (pg, c, r2, -0.5);
  c = svmad_x (pg, c, r2, 1);

  svuint64_t un = svreinterpret_u64 (n);
  /* If odd quadrant, swap cos and sin.  */
  svbool_t swap = svcmpeq (pg, svlsl_x (pg, un, 63), 0);
  svfloat64_t ss = svsel (swap, s, c);
  svfloat64_t cc = svsel (swap, c, s);

  /* Fix signs according to quadrant.
     ss = asdouble(asuint64(ss) ^ ((n       & 2) << 62))
     cc = asdouble(asuint64(cc) & (((n + 1) & 2) << 62)).  */
  svuint64_t sin_sign = svlsl_x (pg, svand_x (pg, un, 2), 62);
  svuint64_t cos_sign = svlsl_x (
      pg, svand_x (pg, svreinterpret_u64 (svadd_x (pg, n, 1)), 2), 62);
  ss = svreinterpret_f64 (sveor_x (pg, svreinterpret_u64 (ss), sin_sign));
  cc = svreinterpret_f64 (sveor_x (pg, svreinterpret_u64 (cc), cos_sign));

  return svcreate2 (ss, cc);
}
