/*
 * Generic helpers for evaluating polynomials with various schemes.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef VTYPE
# error Cannot use poly_generic without defining VTYPE
#endif
#ifndef VWRAP
# error Cannot use poly_generic without defining VWRAP
#endif
#ifndef FMA
# error Cannot use poly_generic without defining FMA
#endif

static inline VTYPE VWRAP (pairwise_poly_3) (VTYPE x, VTYPE x2,
					     const VTYPE *poly)
{
  /* At order 3, Estrin and Pairwise Horner are identical.  */
  VTYPE p01 = FMA (poly[1], x, poly[0]);
  VTYPE p23 = FMA (poly[3], x, poly[2]);
  return FMA (p23, x2, p01);
}

static inline VTYPE VWRAP (estrin_4) (VTYPE x, VTYPE x2, VTYPE x4,
				      const VTYPE *poly)
{
  VTYPE p03 = VWRAP (pairwise_poly_3) (x, x2, poly);
  return FMA (poly[4], x4, p03);
}
static inline VTYPE VWRAP (estrin_5) (VTYPE x, VTYPE x2, VTYPE x4,
				      const VTYPE *poly)
{
  VTYPE p03 = VWRAP (pairwise_poly_3) (x, x2, poly);
  VTYPE p45 = FMA (poly[5], x, poly[4]);
  return FMA (p45, x4, p03);
}
static inline VTYPE VWRAP (estrin_6) (VTYPE x, VTYPE x2, VTYPE x4,
				      const VTYPE *poly)
{
  VTYPE p03 = VWRAP (pairwise_poly_3) (x, x2, poly);
  VTYPE p45 = FMA (poly[5], x, poly[4]);
  VTYPE p46 = FMA (poly[6], x2, p45);
  return FMA (p46, x4, p03);
}
static inline VTYPE VWRAP (estrin_7) (VTYPE x, VTYPE x2, VTYPE x4,
				      const VTYPE *poly)
{
  VTYPE p03 = VWRAP (pairwise_poly_3) (x, x2, poly);
  VTYPE p47 = VWRAP (pairwise_poly_3) (x, x2, poly + 4);
  return FMA (p47, x4, p03);
}
static inline VTYPE VWRAP (estrin_8) (VTYPE x, VTYPE x2, VTYPE x4, VTYPE x8,
				      const VTYPE *poly)
{
  return FMA (poly[8], x8, VWRAP (estrin_7) (x, x2, x4, poly));
}
static inline VTYPE VWRAP (estrin_9) (VTYPE x, VTYPE x2, VTYPE x4, VTYPE x8,
				      const VTYPE *poly)
{
  VTYPE p89 = FMA (poly[9], x, poly[8]);
  return FMA (p89, x8, VWRAP (estrin_7) (x, x2, x4, poly));
}
static inline VTYPE VWRAP (estrin_10) (VTYPE x, VTYPE x2, VTYPE x4, VTYPE x8,
				       const VTYPE *poly)
{
  VTYPE p89 = FMA (poly[9], x, poly[8]);
  VTYPE p8_10 = FMA (poly[10], x2, p89);
  return FMA (p8_10, x8, VWRAP (estrin_7) (x, x2, x4, poly));
}
static inline VTYPE VWRAP (estrin_11) (VTYPE x, VTYPE x2, VTYPE x4, VTYPE x8,
				       const VTYPE *poly)
{
  VTYPE p8_11 = VWRAP (pairwise_poly_3) (x, x2, poly + 8);
  return FMA (p8_11, x8, VWRAP (estrin_7) (x, x2, x4, poly));
}
static inline VTYPE VWRAP (estrin_12) (VTYPE x, VTYPE x2, VTYPE x4, VTYPE x8,
				       const VTYPE *poly)
{
  return FMA (VWRAP (estrin_4) (x, x2, x4, poly + 8), x8,
	      VWRAP (estrin_7) (x, x2, x4, poly));
}
static inline VTYPE VWRAP (estrin_13) (VTYPE x, VTYPE x2, VTYPE x4, VTYPE x8,
				       const VTYPE *poly)
{
  return FMA (VWRAP (estrin_5) (x, x2, x4, poly + 8), x8,
	      VWRAP (estrin_7) (x, x2, x4, poly));
}
static inline VTYPE VWRAP (estrin_14) (VTYPE x, VTYPE x2, VTYPE x4, VTYPE x8,
				       const VTYPE *poly)
{
  return FMA (VWRAP (estrin_6) (x, x2, x4, poly + 8), x8,
	      VWRAP (estrin_7) (x, x2, x4, poly));
}
static inline VTYPE VWRAP (estrin_15) (VTYPE x, VTYPE x2, VTYPE x4, VTYPE x8,
				       const VTYPE *poly)
{
  return FMA (VWRAP (estrin_7) (x, x2, x4, poly + 8), x8,
	      VWRAP (estrin_7) (x, x2, x4, poly));
}
static inline VTYPE VWRAP (estrin_16) (VTYPE x, VTYPE x2, VTYPE x4, VTYPE x8,
				       VTYPE x16, const VTYPE *poly)
{
  return FMA (poly[16], x16, VWRAP (estrin_15) (x, x2, x4, x8, poly));
}
static inline VTYPE VWRAP (estrin_17) (VTYPE x, VTYPE x2, VTYPE x4, VTYPE x8,
				       VTYPE x16, const VTYPE *poly)
{
  VTYPE p16_17 = FMA (poly[17], x, poly[16]);
  return FMA (p16_17, x16, VWRAP (estrin_15) (x, x2, x4, x8, poly));
}
static inline VTYPE VWRAP (estrin_18) (VTYPE x, VTYPE x2, VTYPE x4, VTYPE x8,
				       VTYPE x16, const VTYPE *poly)
{
  VTYPE p16_17 = FMA (poly[17], x, poly[16]);
  VTYPE p16_18 = FMA (poly[18], x2, p16_17);
  return FMA (p16_18, x16, VWRAP (estrin_15) (x, x2, x4, x8, poly));
}
static inline VTYPE VWRAP (estrin_19) (VTYPE x, VTYPE x2, VTYPE x4, VTYPE x8,
				       VTYPE x16, const VTYPE *poly)
{
  VTYPE p16_19 = VWRAP (pairwise_poly_3) (x, x2, poly + 16);
  return FMA (p16_19, x16, VWRAP (estrin_15) (x, x2, x4, x8, poly));
}

static inline VTYPE VWRAP (horner_2) (VTYPE x, const VTYPE *poly)
{
  VTYPE p = FMA (poly[2], x, poly[1]);
  return FMA (x, p, poly[0]);
}
static inline VTYPE VWRAP (horner_3) (VTYPE x, const VTYPE *poly)
{
  VTYPE p = FMA (poly[3], x, poly[2]);
  p = FMA (x, p, poly[1]);
  p = FMA (x, p, poly[0]);
  return p;
}
static inline VTYPE VWRAP (horner_4) (VTYPE x, const VTYPE *poly)
{
  VTYPE p = FMA (poly[4], x, poly[3]);
  p = FMA (x, p, poly[2]);
  p = FMA (x, p, poly[1]);
  p = FMA (x, p, poly[0]);
  return p;
}
static inline VTYPE VWRAP (horner_5) (VTYPE x, const VTYPE *poly)
{
  return FMA (x, VWRAP (horner_4) (x, poly + 1), poly[0]);
}
static inline VTYPE VWRAP (horner_6) (VTYPE x, const VTYPE *poly)
{
  return FMA (x, VWRAP (horner_5) (x, poly + 1), poly[0]);
}
static inline VTYPE VWRAP (horner_7) (VTYPE x, const VTYPE *poly)
{
  return FMA (x, VWRAP (horner_6) (x, poly + 1), poly[0]);
}
static inline VTYPE VWRAP (horner_8) (VTYPE x, const VTYPE *poly)
{
  return FMA (x, VWRAP (horner_7) (x, poly + 1), poly[0]);
}
static inline VTYPE VWRAP (horner_9) (VTYPE x, const VTYPE *poly)
{
  return FMA (x, VWRAP (horner_8) (x, poly + 1), poly[0]);
}
static inline VTYPE VWRAP (horner_10) (VTYPE x, const VTYPE *poly)
{
  return FMA (x, VWRAP (horner_9) (x, poly + 1), poly[0]);
}
static inline VTYPE VWRAP (horner_11) (VTYPE x, const VTYPE *poly)
{
  return FMA (x, VWRAP (horner_10) (x, poly + 1), poly[0]);
}
static inline VTYPE VWRAP (horner_12) (VTYPE x, const VTYPE *poly)
{
  return FMA (x, VWRAP (horner_11) (x, poly + 1), poly[0]);
}

static inline VTYPE VWRAP (pw_horner_4) (VTYPE x, VTYPE x2, const VTYPE *poly)
{
  VTYPE p01 = FMA (poly[1], x, poly[0]);
  VTYPE p23 = FMA (poly[3], x, poly[2]);
  VTYPE p;
  p = FMA (x2, poly[4], p23);
  p = FMA (x2, p, p01);
  return p;
}
static inline VTYPE VWRAP (pw_horner_5) (VTYPE x, VTYPE x2, const VTYPE *poly)
{
  VTYPE p01 = FMA (poly[1], x, poly[0]);
  VTYPE p23 = FMA (poly[3], x, poly[2]);
  VTYPE p45 = FMA (poly[5], x, poly[4]);
  VTYPE p;
  p = FMA (x2, p45, p23);
  p = FMA (x2, p, p01);
  return p;
}
static inline VTYPE VWRAP (pw_horner_6) (VTYPE x, VTYPE x2, const VTYPE *poly)
{
  VTYPE p26 = VWRAP (pw_horner_4) (x, x2, poly + 2);
  VTYPE p01 = FMA (poly[1], x, poly[0]);
  return FMA (x2, p26, p01);
}
static inline VTYPE VWRAP (pw_horner_7) (VTYPE x, VTYPE x2, const VTYPE *poly)
{
  VTYPE p27 = VWRAP (pw_horner_5) (x, x2, poly + 2);
  VTYPE p01 = FMA (poly[1], x, poly[0]);
  return FMA (x2, p27, p01);
}
static inline VTYPE VWRAP (pw_horner_8) (VTYPE x, VTYPE x2, const VTYPE *poly)
{
  VTYPE p28 = VWRAP (pw_horner_6) (x, x2, poly + 2);
  VTYPE p01 = FMA (poly[1], x, poly[0]);
  return FMA (x2, p28, p01);
}
static inline VTYPE VWRAP (pw_horner_9) (VTYPE x, VTYPE x2, const VTYPE *poly)
{
  VTYPE p29 = VWRAP (pw_horner_7) (x, x2, poly + 2);
  VTYPE p01 = FMA (poly[1], x, poly[0]);
  return FMA (x2, p29, p01);
}
static inline VTYPE VWRAP (pw_horner_10) (VTYPE x, VTYPE x2, const VTYPE *poly)
{
  VTYPE p2_10 = VWRAP (pw_horner_8) (x, x2, poly + 2);
  VTYPE p01 = FMA (poly[1], x, poly[0]);
  return FMA (x2, p2_10, p01);
}
static inline VTYPE VWRAP (pw_horner_11) (VTYPE x, VTYPE x2, const VTYPE *poly)
{
  VTYPE p2_11 = VWRAP (pw_horner_9) (x, x2, poly + 2);
  VTYPE p01 = FMA (poly[1], x, poly[0]);
  return FMA (x2, p2_11, p01);
}
static inline VTYPE VWRAP (pw_horner_12) (VTYPE x, VTYPE x2, const VTYPE *poly)
{
  VTYPE p2_12 = VWRAP (pw_horner_10) (x, x2, poly + 2);
  VTYPE p01 = FMA (poly[1], x, poly[0]);
  return FMA (x2, p2_12, p01);
}
static inline VTYPE VWRAP (pw_horner_13) (VTYPE x, VTYPE x2, const VTYPE *poly)
{
  VTYPE p2_13 = VWRAP (pw_horner_11) (x, x2, poly + 2);
  VTYPE p01 = FMA (poly[1], x, poly[0]);
  return FMA (x2, p2_13, p01);
}
static inline VTYPE VWRAP (pw_horner_14) (VTYPE x, VTYPE x2, const VTYPE *poly)
{
  VTYPE p2_14 = VWRAP (pw_horner_12) (x, x2, poly + 2);
  VTYPE p01 = FMA (poly[1], x, poly[0]);
  return FMA (x2, p2_14, p01);
}
static inline VTYPE VWRAP (pw_horner_15) (VTYPE x, VTYPE x2, const VTYPE *poly)
{
  VTYPE p2_15 = VWRAP (pw_horner_13) (x, x2, poly + 2);
  VTYPE p01 = FMA (poly[1], x, poly[0]);
  return FMA (x2, p2_15, p01);
}
static inline VTYPE VWRAP (pw_horner_16) (VTYPE x, VTYPE x2, const VTYPE *poly)
{
  VTYPE p2_16 = VWRAP (pw_horner_14) (x, x2, poly + 2);
  VTYPE p01 = FMA (poly[1], x, poly[0]);
  return FMA (x2, p2_16, p01);
}
static inline VTYPE VWRAP (pw_horner_17) (VTYPE x, VTYPE x2, const VTYPE *poly)
{
  VTYPE p2_17 = VWRAP (pw_horner_15) (x, x2, poly + 2);
  VTYPE p01 = FMA (poly[1], x, poly[0]);
  return FMA (x2, p2_17, p01);
}
static inline VTYPE VWRAP (pw_horner_18) (VTYPE x, VTYPE x2, const VTYPE *poly)
{
  VTYPE p2_18 = VWRAP (pw_horner_16) (x, x2, poly + 2);
  VTYPE p01 = FMA (poly[1], x, poly[0]);
  return FMA (x2, p2_18, p01);
}
