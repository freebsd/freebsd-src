/*
 *  Copyright (C) 2017 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *      Jean-Pierre FLORI <jean-pierre.flori@ssi.gouv.fr>
 *
 *  Contributors:
 *      Nicolas VIVET <nicolas.vivet@ssi.gouv.fr>
 *      Karim KHALFALLAH <karim.khalfallah@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#ifndef __AFF_PT_H__
#define __AFF_PT_H__

#include <libecc/fp/fp.h>
#include <libecc/fp/fp_sqrt.h>
#include <libecc/curves/ec_shortw.h>
#include <libecc/curves/ec_montgomery.h>
#include <libecc/curves/ec_edwards.h>

typedef struct {
	fp x;
	fp y;
	ec_shortw_crv_src_t crv;
	word_t magic;
} aff_pt;

typedef aff_pt *aff_pt_t;
typedef const aff_pt_t aff_pt_src_t;

ATTRIBUTE_WARN_UNUSED_RET int aff_pt_check_initialized(aff_pt_src_t in);
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_init(aff_pt_t in, ec_shortw_crv_src_t curve);
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_init_from_coords(aff_pt_t in,
			    ec_shortw_crv_src_t curve,
			    fp_src_t xcoord, fp_src_t ycoord);
void aff_pt_uninit(aff_pt_t in);
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_y_from_x(fp_t y1, fp_t y2, fp_src_t x, ec_shortw_crv_src_t curve);
ATTRIBUTE_WARN_UNUSED_RET int is_on_shortw_curve(fp_src_t x, fp_src_t y, ec_shortw_crv_src_t curve, int *on_curve);
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_is_on_curve(aff_pt_src_t pt, int *on_curve);
ATTRIBUTE_WARN_UNUSED_RET int ec_shortw_aff_copy(aff_pt_t out, aff_pt_src_t in);
ATTRIBUTE_WARN_UNUSED_RET int ec_shortw_aff_cmp(aff_pt_src_t in1, aff_pt_src_t in2, int *cmp);
ATTRIBUTE_WARN_UNUSED_RET int ec_shortw_aff_eq_or_opp(aff_pt_src_t in1, aff_pt_src_t in2,
			    int *eq_or_opp);
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_import_from_buf(aff_pt_t pt,
			   const u8 *pt_buf,
			   u16 pt_buf_len, ec_shortw_crv_src_t crv);
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_export_to_buf(aff_pt_src_t pt, u8 *pt_buf, u32 pt_buf_len);

/*** Edwards curves related ***/
typedef struct {
	fp x;
	fp y;
	ec_edwards_crv_src_t crv;
	word_t magic;
} aff_pt_edwards;

typedef aff_pt_edwards *aff_pt_edwards_t;
typedef const aff_pt_edwards_t aff_pt_edwards_src_t;

ATTRIBUTE_WARN_UNUSED_RET int aff_pt_edwards_check_initialized(aff_pt_edwards_src_t in);
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_edwards_init(aff_pt_edwards_t in, ec_edwards_crv_src_t curve);
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_edwards_init_from_coords(aff_pt_edwards_t in,
			     ec_edwards_crv_src_t curve,
			     fp_src_t ucoord, fp_src_t vcoord);
void aff_pt_edwards_uninit(aff_pt_edwards_t in);
ATTRIBUTE_WARN_UNUSED_RET int is_on_edwards_curve(fp_src_t u, fp_src_t v, ec_edwards_crv_src_t curve, int *on_curve);
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_edwards_is_on_curve(aff_pt_edwards_src_t pt, int *on_curve);
ATTRIBUTE_WARN_UNUSED_RET int ec_edwards_aff_copy(aff_pt_edwards_t out, aff_pt_edwards_src_t in);
ATTRIBUTE_WARN_UNUSED_RET int ec_edwards_aff_cmp(aff_pt_edwards_src_t in1, aff_pt_edwards_src_t in2, int *cmp);
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_edwards_import_from_buf(aff_pt_edwards_t pt,
			   const u8 *pt_buf,
			   u16 pt_buf_len, ec_edwards_crv_src_t crv);
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_edwards_export_to_buf(aff_pt_edwards_src_t pt, u8 *pt_buf, u32 pt_buf_len);

ATTRIBUTE_WARN_UNUSED_RET int curve_edwards_to_montgomery(ec_edwards_crv_src_t edwards_crv, ec_montgomery_crv_t montgomery_crv, fp_src_t alpha_edwards);
ATTRIBUTE_WARN_UNUSED_RET int curve_edwards_montgomery_check(ec_edwards_crv_src_t edwards_crv, ec_montgomery_crv_src_t montgomery_crv, fp_src_t alpha_edwards);

ATTRIBUTE_WARN_UNUSED_RET int curve_montgomery_to_edwards(ec_montgomery_crv_src_t montgomery_crv, ec_edwards_crv_t edwards_crv, fp_src_t alpha_edwards);

ATTRIBUTE_WARN_UNUSED_RET int curve_edwards_to_shortw(ec_edwards_crv_src_t edwards_crv, ec_shortw_crv_t shortw_crv, fp_src_t alpha_edwards);
ATTRIBUTE_WARN_UNUSED_RET int curve_edwards_shortw_check(ec_edwards_crv_src_t edwards_crv, ec_shortw_crv_src_t shortw_crv, fp_src_t alpha_edwards);
ATTRIBUTE_WARN_UNUSED_RET int curve_shortw_to_edwards(ec_shortw_crv_src_t shortw_crv, ec_edwards_crv_t edwards_crv, fp_src_t alpha_montgomery, fp_src_t gamma_montgomery, fp_src_t alpha_edwards);

ATTRIBUTE_WARN_UNUSED_RET int aff_pt_edwards_to_shortw(aff_pt_edwards_src_t in_edwards, ec_shortw_crv_src_t shortw_crv, aff_pt_t out_shortw, fp_src_t alpha_edwards);
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_shortw_to_edwards(aff_pt_src_t in_shortw, ec_edwards_crv_src_t edwards_crv, aff_pt_edwards_t out_edwards, fp_src_t alpha_edwards);

ATTRIBUTE_WARN_UNUSED_RET int aff_pt_edwards_y_from_x(fp_t y1, fp_t y2, fp_src_t x, ec_edwards_crv_src_t crv);
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_edwards_x_from_y(fp_t x1, fp_t x2, fp_src_t y, ec_edwards_crv_src_t crv);

/*** Montgomery curves related ***/
typedef struct {
	fp u;
	fp v;
	ec_montgomery_crv_src_t crv;
	word_t magic;
} aff_pt_montgomery;

typedef aff_pt_montgomery *aff_pt_montgomery_t;
typedef const aff_pt_montgomery_t aff_pt_montgomery_src_t;

ATTRIBUTE_WARN_UNUSED_RET int aff_pt_montgomery_check_initialized(aff_pt_montgomery_src_t in);
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_montgomery_init(aff_pt_montgomery_t in, ec_montgomery_crv_src_t curve);
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_montgomery_init_from_coords(aff_pt_montgomery_t in,
                             ec_montgomery_crv_src_t curve,
                             fp_src_t ucoord, fp_src_t vcoord);
void aff_pt_montgomery_uninit(aff_pt_montgomery_t in);
ATTRIBUTE_WARN_UNUSED_RET int is_on_montgomery_curve(fp_src_t u, fp_src_t v, ec_montgomery_crv_src_t curve, int *on_curve);
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_montgomery_is_on_curve(aff_pt_montgomery_src_t pt, int *on_curve);
ATTRIBUTE_WARN_UNUSED_RET int ec_montgomery_aff_copy(aff_pt_montgomery_t out, aff_pt_montgomery_src_t in);
ATTRIBUTE_WARN_UNUSED_RET int ec_montgomery_aff_cmp(aff_pt_montgomery_src_t in1, aff_pt_montgomery_src_t in2, int *cmp);
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_montgomery_import_from_buf(aff_pt_montgomery_t pt,
                           const u8 *pt_buf,
                           u16 pt_buf_len, ec_montgomery_crv_src_t crv);
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_montgomery_export_to_buf(aff_pt_montgomery_src_t pt, u8 *pt_buf, u32 pt_buf_len);

ATTRIBUTE_WARN_UNUSED_RET int curve_montgomery_to_shortw(ec_montgomery_crv_src_t montgomery_crv, ec_shortw_crv_t shortw_crv);

ATTRIBUTE_WARN_UNUSED_RET int curve_montgomery_shortw_check(ec_montgomery_crv_src_t montgomery_crv, ec_shortw_crv_src_t shortw_crv);
ATTRIBUTE_WARN_UNUSED_RET int curve_shortw_to_montgomery(ec_shortw_crv_src_t shortw_crv, ec_montgomery_crv_t montgomery_crv, fp_src_t alpha_montgomery, fp_src_t gamma_montgomery);


ATTRIBUTE_WARN_UNUSED_RET int aff_pt_montgomery_to_shortw(aff_pt_montgomery_src_t in_montgomery, ec_shortw_crv_src_t shortw_crv, aff_pt_t out_shortw);
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_shortw_to_montgomery(aff_pt_src_t in_shortw, ec_montgomery_crv_src_t montgomery_crv, aff_pt_montgomery_t out_montgomery);


/*****/
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_edwards_to_montgomery(aff_pt_edwards_src_t in_edwards, ec_montgomery_crv_src_t montgomery_crv, aff_pt_montgomery_t out_montgomery, fp_src_t alpha);
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_montgomery_to_edwards(aff_pt_montgomery_src_t in_montgomery, ec_edwards_crv_src_t edwards_crv, aff_pt_edwards_t out_edwards, fp_src_t alpha);

ATTRIBUTE_WARN_UNUSED_RET int aff_pt_montgomery_v_from_u(fp_t v1, fp_t v2, fp_src_t u, ec_montgomery_crv_src_t crv);

 #endif /* __AFF_PT_H__ */
