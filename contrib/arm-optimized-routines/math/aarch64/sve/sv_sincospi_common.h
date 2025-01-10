/*
 * Core approximation for double-precision SVE sincospi
 *
 * Copyright (c) 2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "sv_poly_f64.h"

static const struct sv_sincospi_data
{
  double c0, c2, c4, c6, c8;
  double c1, c3, c5, c7, c9;
  double range_val;
} sv_sincospi_data = {
  /* Polynomial coefficients generated using Remez algorithm,
     see sinpi.sollya for details.  */
  .c0 = 0x1.921fb54442d184p1,
  .c1 = -0x1.4abbce625be53p2,
  .c2 = 0x1.466bc6775ab16p1,
  .c3 = -0x1.32d2cce62dc33p-1,
  .c4 = 0x1.507834891188ep-4,
  .c5 = -0x1.e30750a28c88ep-8,
  .c6 = 0x1.e8f48308acda4p-12,
  .c7 = -0x1.6fc0032b3c29fp-16,
  .c8 = 0x1.af86ae521260bp-21,
  .c9 = -0x1.012a9870eeb7dp-25,
  /* Exclusive upper bound for a signed integer.  */
  .range_val = 0x1p63
};

/* Double-precision vector function allowing calculation of both sinpi and
   cospi in one function call, using shared argument reduction and polynomials.
    Worst-case error for sin is 3.09 ULP:
    _ZGVsMxvl8l8_sincospi_sin(0x1.7a41deb4b21e1p+14) got 0x1.fd54d0b327cf1p-1
						    want 0x1.fd54d0b327cf4p-1.
   Worst-case error for cos is 3.16 ULP:
    _ZGVsMxvl8l8_sincospi_cos(-0x1.11e3c7e284adep-5) got 0x1.fd2da484ff3ffp-1
						    want 0x1.fd2da484ff402p-1.
 */
static inline svfloat64x2_t
sv_sincospi_inline (svbool_t pg, svfloat64_t x,
		    const struct sv_sincospi_data *d)
{
  const svbool_t pt = svptrue_b64 ();

  /* r = x - rint(x).  */
  /* pt hints unpredicated instruction.  */
  svfloat64_t rx = svrinta_x (pg, x);
  svfloat64_t sr = svsub_x (pt, x, rx);

  /* cospi(x) = sinpi(0.5 - abs(x)) for values -1/2 .. 1/2.  */
  svfloat64_t cr = svsubr_x (pg, svabs_x (pg, sr), 0.5);

  /* Pairwise Horner approximation for y = sin(r * pi).  */
  /* pt hints unpredicated instruction.  */
  svfloat64_t sr2 = svmul_x (pt, sr, sr);
  svfloat64_t cr2 = svmul_x (pt, cr, cr);
  svfloat64_t sr4 = svmul_x (pt, sr2, sr2);
  svfloat64_t cr4 = svmul_x (pt, cr2, cr2);

  /* If rint(x) is odd, the sign of the result should be inverted for sinpi and
    re-introduced for cospi. cmp filters rxs that saturate to max sint.  */
  svbool_t cmp = svaclt (pg, x, d->range_val);
  svuint64_t odd = svlsl_x (pt, svreinterpret_u64 (svcvt_s64_z (pg, rx)), 63);
  sr = svreinterpret_f64 (sveor_x (pt, svreinterpret_u64 (sr), odd));
  cr = svreinterpret_f64 (sveor_m (cmp, svreinterpret_u64 (cr), odd));

  svfloat64_t sinpix = svmul_x (
      pt, sv_lw_pw_horner_9_f64_x (pg, sr2, sr4, &(d->c0), &(d->c1)), sr);
  svfloat64_t cospix = svmul_x (
      pt, sv_lw_pw_horner_9_f64_x (pg, cr2, cr4, &(d->c0), &(d->c1)), cr);

  return svcreate2 (sinpix, cospix);
}
