/*
 * Helpers for evaluating polynomials with various schemes - specific to SVE
 * but precision-agnostic.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef VTYPE
# error Cannot use poly_generic without defining VTYPE
#endif
#ifndef STYPE
# error Cannot use poly_generic without defining STYPE
#endif
#ifndef VWRAP
# error Cannot use poly_generic without defining VWRAP
#endif
#ifndef DUP
# error Cannot use poly_generic without defining DUP
#endif

static inline VTYPE VWRAP (pairwise_poly_3) (svbool_t pg, VTYPE x, VTYPE x2,
					     const STYPE *poly)
{
  /* At order 3, Estrin and Pairwise Horner are identical.  */
  VTYPE p01 = svmla_x (pg, DUP (poly[0]), x, poly[1]);
  VTYPE p23 = svmla_x (pg, DUP (poly[2]), x, poly[3]);
  return svmla_x (pg, p01, p23, x2);
}

static inline VTYPE VWRAP (estrin_4) (svbool_t pg, VTYPE x, VTYPE x2, VTYPE x4,
				      const STYPE *poly)
{
  VTYPE p03 = VWRAP (pairwise_poly_3) (pg, x, x2, poly);
  return svmla_x (pg, p03, x4, poly[4]);
}
static inline VTYPE VWRAP (estrin_5) (svbool_t pg, VTYPE x, VTYPE x2, VTYPE x4,
				      const STYPE *poly)
{
  VTYPE p03 = VWRAP (pairwise_poly_3) (pg, x, x2, poly);
  VTYPE p45 = svmla_x (pg, DUP (poly[4]), x, poly[5]);
  return svmla_x (pg, p03, p45, x4);
}
static inline VTYPE VWRAP (estrin_6) (svbool_t pg, VTYPE x, VTYPE x2, VTYPE x4,
				      const STYPE *poly)
{
  VTYPE p03 = VWRAP (pairwise_poly_3) (pg, x, x2, poly);
  VTYPE p45 = svmla_x (pg, DUP (poly[4]), x, poly[5]);
  VTYPE p46 = svmla_x (pg, p45, x, poly[6]);
  return svmla_x (pg, p03, p46, x4);
}
static inline VTYPE VWRAP (estrin_7) (svbool_t pg, VTYPE x, VTYPE x2, VTYPE x4,
				      const STYPE *poly)
{
  VTYPE p03 = VWRAP (pairwise_poly_3) (pg, x, x2, poly);
  VTYPE p47 = VWRAP (pairwise_poly_3) (pg, x, x2, poly + 4);
  return svmla_x (pg, p03, p47, x4);
}
static inline VTYPE VWRAP (estrin_8) (svbool_t pg, VTYPE x, VTYPE x2, VTYPE x4,
				      VTYPE x8, const STYPE *poly)
{
  return svmla_x (pg, VWRAP (estrin_7) (pg, x, x2, x4, poly), x8, poly[8]);
}
static inline VTYPE VWRAP (estrin_9) (svbool_t pg, VTYPE x, VTYPE x2, VTYPE x4,
				      VTYPE x8, const STYPE *poly)
{
  VTYPE p89 = svmla_x (pg, DUP (poly[8]), x, poly[9]);
  return svmla_x (pg, VWRAP (estrin_7) (pg, x, x2, x4, poly), p89, x8);
}
static inline VTYPE VWRAP (estrin_10) (svbool_t pg, VTYPE x, VTYPE x2,
				       VTYPE x4, VTYPE x8, const STYPE *poly)
{
  VTYPE p89 = svmla_x (pg, DUP (poly[8]), x, poly[9]);
  VTYPE p8_10 = svmla_x (pg, p89, x2, poly[10]);
  return svmla_x (pg, VWRAP (estrin_7) (pg, x, x2, x4, poly), p8_10, x8);
}
static inline VTYPE VWRAP (estrin_11) (svbool_t pg, VTYPE x, VTYPE x2,
				       VTYPE x4, VTYPE x8, const STYPE *poly)
{
  VTYPE p8_11 = VWRAP (pairwise_poly_3) (pg, x, x2, poly + 8);
  return svmla_x (pg, VWRAP (estrin_7) (pg, x, x2, x4, poly), p8_11, x8);
}
static inline VTYPE VWRAP (estrin_12) (svbool_t pg, VTYPE x, VTYPE x2,
				       VTYPE x4, VTYPE x8, const STYPE *poly)
{
  return svmla_x (pg, VWRAP (estrin_7) (pg, x, x2, x4, poly),
		  VWRAP (estrin_4) (pg, x, x2, x4, poly + 8), x8);
}
static inline VTYPE VWRAP (estrin_13) (svbool_t pg, VTYPE x, VTYPE x2,
				       VTYPE x4, VTYPE x8, const STYPE *poly)
{
  return svmla_x (pg, VWRAP (estrin_7) (pg, x, x2, x4, poly),
		  VWRAP (estrin_5) (pg, x, x2, x4, poly + 8), x8);
}
static inline VTYPE VWRAP (estrin_14) (svbool_t pg, VTYPE x, VTYPE x2,
				       VTYPE x4, VTYPE x8, const STYPE *poly)
{
  return svmla_x (pg, VWRAP (estrin_7) (pg, x, x2, x4, poly),
		  VWRAP (estrin_6) (pg, x, x2, x4, poly + 8), x8);
}
static inline VTYPE VWRAP (estrin_15) (svbool_t pg, VTYPE x, VTYPE x2,
				       VTYPE x4, VTYPE x8, const STYPE *poly)
{
  return svmla_x (pg, VWRAP (estrin_7) (pg, x, x2, x4, poly),
		  VWRAP (estrin_7) (pg, x, x2, x4, poly + 8), x8);
}
static inline VTYPE VWRAP (estrin_16) (svbool_t pg, VTYPE x, VTYPE x2,
				       VTYPE x4, VTYPE x8, VTYPE x16,
				       const STYPE *poly)
{
  return svmla_x (pg, VWRAP (estrin_15) (pg, x, x2, x4, x8, poly), x16,
		  poly[16]);
}
static inline VTYPE VWRAP (estrin_17) (svbool_t pg, VTYPE x, VTYPE x2,
				       VTYPE x4, VTYPE x8, VTYPE x16,
				       const STYPE *poly)
{
  VTYPE p16_17 = svmla_x (pg, DUP (poly[16]), x, poly[17]);
  return svmla_x (pg, VWRAP (estrin_15) (pg, x, x2, x4, x8, poly), p16_17,
		  x16);
}
static inline VTYPE VWRAP (estrin_18) (svbool_t pg, VTYPE x, VTYPE x2,
				       VTYPE x4, VTYPE x8, VTYPE x16,
				       const STYPE *poly)
{
  VTYPE p16_17 = svmla_x (pg, DUP (poly[16]), x, poly[17]);
  VTYPE p16_18 = svmla_x (pg, p16_17, x2, poly[18]);
  return svmla_x (pg, VWRAP (estrin_15) (pg, x, x2, x4, x8, poly), p16_18,
		  x16);
}
static inline VTYPE VWRAP (estrin_19) (svbool_t pg, VTYPE x, VTYPE x2,
				       VTYPE x4, VTYPE x8, VTYPE x16,
				       const STYPE *poly)
{
  return svmla_x (pg, VWRAP (estrin_15) (pg, x, x2, x4, x8, poly),
		  VWRAP (pairwise_poly_3) (pg, x, x2, poly + 16), x16);
}

static inline VTYPE VWRAP (horner_3) (svbool_t pg, VTYPE x, const STYPE *poly)
{
  VTYPE p = svmla_x (pg, DUP (poly[2]), x, poly[3]);
  p = svmad_x (pg, x, p, poly[1]);
  p = svmad_x (pg, x, p, poly[0]);
  return p;
}
static inline VTYPE VWRAP (horner_4) (svbool_t pg, VTYPE x, const STYPE *poly)
{
  VTYPE p = svmla_x (pg, DUP (poly[3]), x, poly[4]);
  p = svmad_x (pg, x, p, poly[2]);
  p = svmad_x (pg, x, p, poly[1]);
  p = svmad_x (pg, x, p, poly[0]);
  return p;
}
static inline VTYPE VWRAP (horner_5) (svbool_t pg, VTYPE x, const STYPE *poly)
{
  return svmad_x (pg, x, VWRAP (horner_4) (pg, x, poly + 1), poly[0]);
}
static inline VTYPE VWRAP (horner_6) (svbool_t pg, VTYPE x, const STYPE *poly)
{
  return svmad_x (pg, x, VWRAP (horner_5) (pg, x, poly + 1), poly[0]);
}
static inline VTYPE VWRAP (horner_7) (svbool_t pg, VTYPE x, const STYPE *poly)
{
  return svmad_x (pg, x, VWRAP (horner_6) (pg, x, poly + 1), poly[0]);
}
static inline VTYPE VWRAP (horner_8) (svbool_t pg, VTYPE x, const STYPE *poly)
{
  return svmad_x (pg, x, VWRAP (horner_7) (pg, x, poly + 1), poly[0]);
}
static inline VTYPE VWRAP (horner_9) (svbool_t pg, VTYPE x, const STYPE *poly)
{
  return svmad_x (pg, x, VWRAP (horner_8) (pg, x, poly + 1), poly[0]);
}
static inline VTYPE
sv_horner_10_f32_x (svbool_t pg, VTYPE x, const STYPE *poly)
{
  return svmad_x (pg, x, VWRAP (horner_9) (pg, x, poly + 1), poly[0]);
}
static inline VTYPE
sv_horner_11_f32_x (svbool_t pg, VTYPE x, const STYPE *poly)
{
  return svmad_x (pg, x, sv_horner_10_f32_x (pg, x, poly + 1), poly[0]);
}
static inline VTYPE
sv_horner_12_f32_x (svbool_t pg, VTYPE x, const STYPE *poly)
{
  return svmad_x (pg, x, sv_horner_11_f32_x (pg, x, poly + 1), poly[0]);
}

static inline VTYPE VWRAP (pw_horner_4) (svbool_t pg, VTYPE x, VTYPE x2,
					 const STYPE *poly)
{
  VTYPE p01 = svmla_x (pg, DUP (poly[0]), x, poly[1]);
  VTYPE p23 = svmla_x (pg, DUP (poly[2]), x, poly[3]);
  VTYPE p;
  p = svmla_x (pg, p23, x2, poly[4]);
  p = svmla_x (pg, p01, x2, p);
  return p;
}
static inline VTYPE VWRAP (pw_horner_5) (svbool_t pg, VTYPE x, VTYPE x2,
					 const STYPE *poly)
{
  VTYPE p01 = svmla_x (pg, DUP (poly[0]), x, poly[1]);
  VTYPE p23 = svmla_x (pg, DUP (poly[2]), x, poly[3]);
  VTYPE p45 = svmla_x (pg, DUP (poly[4]), x, poly[5]);
  VTYPE p;
  p = svmla_x (pg, p23, x2, p45);
  p = svmla_x (pg, p01, x2, p);
  return p;
}
static inline VTYPE VWRAP (pw_horner_6) (svbool_t pg, VTYPE x, VTYPE x2,
					 const STYPE *poly)
{
  VTYPE p26 = VWRAP (pw_horner_4) (pg, x, x2, poly + 2);
  VTYPE p01 = svmla_x (pg, DUP (poly[0]), x, poly[1]);
  return svmla_x (pg, p01, x2, p26);
}
static inline VTYPE VWRAP (pw_horner_7) (svbool_t pg, VTYPE x, VTYPE x2,
					 const STYPE *poly)
{
  VTYPE p27 = VWRAP (pw_horner_5) (pg, x, x2, poly + 2);
  VTYPE p01 = svmla_x (pg, DUP (poly[0]), x, poly[1]);
  return svmla_x (pg, p01, x2, p27);
}
static inline VTYPE VWRAP (pw_horner_8) (svbool_t pg, VTYPE x, VTYPE x2,
					 const STYPE *poly)
{
  VTYPE p28 = VWRAP (pw_horner_6) (pg, x, x2, poly + 2);
  VTYPE p01 = svmla_x (pg, DUP (poly[0]), x, poly[1]);
  return svmla_x (pg, p01, x2, p28);
}
static inline VTYPE VWRAP (pw_horner_9) (svbool_t pg, VTYPE x, VTYPE x2,
					 const STYPE *poly)
{
  VTYPE p29 = VWRAP (pw_horner_7) (pg, x, x2, poly + 2);
  VTYPE p01 = svmla_x (pg, DUP (poly[0]), x, poly[1]);
  return svmla_x (pg, p01, x2, p29);
}
static inline VTYPE VWRAP (pw_horner_10) (svbool_t pg, VTYPE x, VTYPE x2,
					  const STYPE *poly)
{
  VTYPE p2_10 = VWRAP (pw_horner_8) (pg, x, x2, poly + 2);
  VTYPE p01 = svmla_x (pg, DUP (poly[0]), x, poly[1]);
  return svmla_x (pg, p01, x2, p2_10);
}
static inline VTYPE VWRAP (pw_horner_11) (svbool_t pg, VTYPE x, VTYPE x2,
					  const STYPE *poly)
{
  VTYPE p2_11 = VWRAP (pw_horner_9) (pg, x, x2, poly + 2);
  VTYPE p01 = svmla_x (pg, DUP (poly[0]), x, poly[1]);
  return svmla_x (pg, p01, x2, p2_11);
}
static inline VTYPE VWRAP (pw_horner_12) (svbool_t pg, VTYPE x, VTYPE x2,
					  const STYPE *poly)
{
  VTYPE p2_12 = VWRAP (pw_horner_10) (pg, x, x2, poly + 2);
  VTYPE p01 = svmla_x (pg, DUP (poly[0]), x, poly[1]);
  return svmla_x (pg, p01, x2, p2_12);
}
static inline VTYPE VWRAP (pw_horner_13) (svbool_t pg, VTYPE x, VTYPE x2,
					  const STYPE *poly)
{
  VTYPE p2_13 = VWRAP (pw_horner_11) (pg, x, x2, poly + 2);
  VTYPE p01 = svmla_x (pg, DUP (poly[0]), x, poly[1]);
  return svmla_x (pg, p01, x2, p2_13);
}
static inline VTYPE VWRAP (pw_horner_14) (svbool_t pg, VTYPE x, VTYPE x2,
					  const STYPE *poly)
{
  VTYPE p2_14 = VWRAP (pw_horner_12) (pg, x, x2, poly + 2);
  VTYPE p01 = svmla_x (pg, DUP (poly[0]), x, poly[1]);
  return svmla_x (pg, p01, x2, p2_14);
}
static inline VTYPE VWRAP (pw_horner_15) (svbool_t pg, VTYPE x, VTYPE x2,
					  const STYPE *poly)
{
  VTYPE p2_15 = VWRAP (pw_horner_13) (pg, x, x2, poly + 2);
  VTYPE p01 = svmla_x (pg, DUP (poly[0]), x, poly[1]);
  return svmla_x (pg, p01, x2, p2_15);
}
static inline VTYPE VWRAP (pw_horner_16) (svbool_t pg, VTYPE x, VTYPE x2,
					  const STYPE *poly)
{
  VTYPE p2_16 = VWRAP (pw_horner_14) (pg, x, x2, poly + 2);
  VTYPE p01 = svmla_x (pg, DUP (poly[0]), x, poly[1]);
  return svmla_x (pg, p01, x2, p2_16);
}
static inline VTYPE VWRAP (pw_horner_17) (svbool_t pg, VTYPE x, VTYPE x2,
					  const STYPE *poly)
{
  VTYPE p2_17 = VWRAP (pw_horner_15) (pg, x, x2, poly + 2);
  VTYPE p01 = svmla_x (pg, DUP (poly[0]), x, poly[1]);
  return svmla_x (pg, p01, x2, p2_17);
}
static inline VTYPE VWRAP (pw_horner_18) (svbool_t pg, VTYPE x, VTYPE x2,
					  const STYPE *poly)
{
  VTYPE p2_18 = VWRAP (pw_horner_16) (pg, x, x2, poly + 2);
  VTYPE p01 = svmla_x (pg, DUP (poly[0]), x, poly[1]);
  return svmla_x (pg, p01, x2, p2_18);
}

static inline VTYPE VWRAP (lw_pw_horner_5) (svbool_t pg, VTYPE x, VTYPE x2,
					    const STYPE *poly_even,
					    const STYPE *poly_odd)
{
  VTYPE c13 = svld1rq (pg, poly_odd);

  VTYPE p01 = svmla_lane (DUP (poly_even[0]), x, c13, 0);
  VTYPE p23 = svmla_lane (DUP (poly_even[1]), x, c13, 1);
  VTYPE p45 = svmla_x (pg, DUP (poly_even[2]), x, poly_odd[2]);

  VTYPE p;
  p = svmla_x (pg, p23, x2, p45);
  p = svmla_x (pg, p01, x2, p);
  return p;
}
static inline VTYPE VWRAP (lw_pw_horner_9) (svbool_t pg, VTYPE x, VTYPE x2,
					    const STYPE *poly_even,
					    const STYPE *poly_odd)
{
  VTYPE c13 = svld1rq (pg, poly_odd);

  VTYPE p49 = VWRAP (lw_pw_horner_5) (pg, x, x2, poly_even + 2, poly_odd + 2);
  VTYPE p23 = svmla_lane (DUP (poly_even[1]), x, c13, 1);

  VTYPE p29 = svmla_x (pg, p23, x2, p49);
  VTYPE p01 = svmla_lane (DUP (poly_even[0]), x, c13, 0);

  return svmla_x (pg, p01, x2, p29);
}
