/*
 * Helper for Double-precision vector sincospi function.
 *
 * Copyright (c) 2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "v_math.h"
#include "v_poly_f64.h"

static const struct v_sincospi_data
{
  float64x2_t poly[10], range_val;
} v_sincospi_data = {
  /* Polynomial coefficients generated using Remez algorithm,
     see sinpi.sollya for details.  */
  .poly = { V2 (0x1.921fb54442d184p1), V2 (-0x1.4abbce625be53p2),
	    V2 (0x1.466bc6775ab16p1), V2 (-0x1.32d2cce62dc33p-1),
	    V2 (0x1.507834891188ep-4), V2 (-0x1.e30750a28c88ep-8),
	    V2 (0x1.e8f48308acda4p-12), V2 (-0x1.6fc0032b3c29fp-16),
	    V2 (0x1.af86ae521260bp-21), V2 (-0x1.012a9870eeb7dp-25) },
  .range_val = V2 (0x1p63),
};

/* Double-precision vector function allowing calculation of both sin and cos in
   one function call, using separate argument reduction and shared low-order
   polynomials.
   Approximation for vector double-precision sincospi(x).
   Maximum Error 3.09 ULP:
  _ZGVnN2v_sincospi_sin(0x1.7a41deb4b21e1p+14) got 0x1.fd54d0b327cf1p-1
					      want 0x1.fd54d0b327cf4p-1
   Maximum Error 3.16 ULP:
  _ZGVnN2v_sincospi_cos(-0x1.11e3c7e284adep-5) got 0x1.fd2da484ff3ffp-1
					      want 0x1.fd2da484ff402p-1.  */
static inline float64x2x2_t
v_sincospi_inline (float64x2_t x, const struct v_sincospi_data *d)
{
  /* If r is odd, the sign of the result should be inverted for sinpi
     and reintroduced for cospi.  */
  uint64x2_t cmp = vcgeq_f64 (x, d->range_val);
  uint64x2_t odd = vshlq_n_u64 (
      vbicq_u64 (vreinterpretq_u64_s64 (vcvtaq_s64_f64 (x)), cmp), 63);

  /* r = x - rint(x).  */
  float64x2_t sr = vsubq_f64 (x, vrndaq_f64 (x));
  /* cospi(x) = sinpi(0.5 - abs(x)) for values -1/2 .. 1/2.  */
  float64x2_t cr = vsubq_f64 (v_f64 (0.5), vabsq_f64 (sr));

  /* Pairwise Horner approximation for y = sin(r * pi).  */
  float64x2_t sr2 = vmulq_f64 (sr, sr);
  float64x2_t sr4 = vmulq_f64 (sr2, sr2);
  float64x2_t cr2 = vmulq_f64 (cr, cr);
  float64x2_t cr4 = vmulq_f64 (cr2, cr2);

  float64x2_t ss = vmulq_f64 (v_pw_horner_9_f64 (sr2, sr4, d->poly), sr);
  float64x2_t cc = vmulq_f64 (v_pw_horner_9_f64 (cr2, cr4, d->poly), cr);

  float64x2_t sinpix
      = vreinterpretq_f64_u64 (veorq_u64 (vreinterpretq_u64_f64 (ss), odd));

  float64x2_t cospix
      = vreinterpretq_f64_u64 (veorq_u64 (vreinterpretq_u64_f64 (cc), odd));

  return (float64x2x2_t){ sinpix, cospix };
}
