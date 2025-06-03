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
#ifndef __PRJ_PT_H__
#define __PRJ_PT_H__

#include <libecc/nn/nn_mul_public.h>
#include <libecc/fp/fp.h>
#include <libecc/fp/fp_mul.h>
#include <libecc/fp/fp_mul_redc1.h>
#include <libecc/curves/ec_shortw.h>
#include <libecc/curves/aff_pt.h>

typedef struct {
	fp X;
	fp Y;
	fp Z;
	ec_shortw_crv_src_t crv;
	word_t magic;
} prj_pt;

typedef prj_pt *prj_pt_t;
typedef const prj_pt *prj_pt_src_t;

typedef enum {
	PUBLIC_PT = 0,
	PRIVATE_PT = 1
} prj_pt_sensitivity;

ATTRIBUTE_WARN_UNUSED_RET int prj_pt_check_initialized(prj_pt_src_t in);
ATTRIBUTE_WARN_UNUSED_RET int prj_pt_init(prj_pt_t in, ec_shortw_crv_src_t curve);
ATTRIBUTE_WARN_UNUSED_RET int prj_pt_init_from_coords(prj_pt_t in,
			    ec_shortw_crv_src_t curve,
			    fp_src_t xcoord,
			    fp_src_t ycoord, fp_src_t zcoord);
void prj_pt_uninit(prj_pt_t in);
ATTRIBUTE_WARN_UNUSED_RET int prj_pt_zero(prj_pt_t out);
ATTRIBUTE_WARN_UNUSED_RET int prj_pt_iszero(prj_pt_src_t in, int *iszero);
ATTRIBUTE_WARN_UNUSED_RET int prj_pt_is_on_curve(prj_pt_src_t in, int *on_curve);
ATTRIBUTE_WARN_UNUSED_RET int prj_pt_copy(prj_pt_t out, prj_pt_src_t in);
ATTRIBUTE_WARN_UNUSED_RET int prj_pt_to_aff(aff_pt_t out, prj_pt_src_t in);
ATTRIBUTE_WARN_UNUSED_RET int prj_pt_unique(prj_pt_t out, prj_pt_src_t in);
ATTRIBUTE_WARN_UNUSED_RET int ec_shortw_aff_to_prj(prj_pt_t out, aff_pt_src_t in);
ATTRIBUTE_WARN_UNUSED_RET int prj_pt_cmp(prj_pt_src_t in1, prj_pt_src_t in2, int *cmp);
ATTRIBUTE_WARN_UNUSED_RET int prj_pt_eq_or_opp(prj_pt_src_t in1, prj_pt_src_t in2, int *eq_or_opp);
ATTRIBUTE_WARN_UNUSED_RET int prj_pt_neg(prj_pt_t out, prj_pt_src_t in);
ATTRIBUTE_WARN_UNUSED_RET int prj_pt_add(prj_pt_t sum, prj_pt_src_t in1, prj_pt_src_t in2);
ATTRIBUTE_WARN_UNUSED_RET int prj_pt_dbl(prj_pt_t dbl, prj_pt_src_t in);
ATTRIBUTE_WARN_UNUSED_RET int prj_pt_mul(prj_pt_t out, nn_src_t m, prj_pt_src_t in);
ATTRIBUTE_WARN_UNUSED_RET int prj_pt_mul_blind(prj_pt_t out, nn_src_t m, prj_pt_src_t in);
/* XXX: WARNING: this function must only be used on public points! */
ATTRIBUTE_WARN_UNUSED_RET int _prj_pt_unprotected_mult(prj_pt_t out, nn_src_t cofactor, prj_pt_src_t public_in);
ATTRIBUTE_WARN_UNUSED_RET int check_prj_pt_order(prj_pt_src_t in_shortw, nn_src_t in_isorder, prj_pt_sensitivity s, int *check);
ATTRIBUTE_WARN_UNUSED_RET int prj_pt_import_from_buf(prj_pt_t pt,
			   const u8 *pt_buf,
			   u16 pt_buf_len, ec_shortw_crv_src_t crv);
ATTRIBUTE_WARN_UNUSED_RET int prj_pt_import_from_aff_buf(prj_pt_t pt,
			   const u8 *pt_buf,
			   u16 pt_buf_len, ec_shortw_crv_src_t crv);
ATTRIBUTE_WARN_UNUSED_RET int prj_pt_export_to_buf(prj_pt_src_t pt, u8 *pt_buf, u32 pt_buf_len);
ATTRIBUTE_WARN_UNUSED_RET int prj_pt_export_to_aff_buf(prj_pt_src_t pt, u8 *pt_buf, u32 pt_buf_len);
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_edwards_to_prj_pt_shortw(aff_pt_edwards_src_t in_edwards,
				    ec_shortw_crv_src_t shortw_crv,
				    prj_pt_t out_shortw, fp_src_t alpha);
ATTRIBUTE_WARN_UNUSED_RET int aff_pt_montgomery_to_prj_pt_shortw(aff_pt_montgomery_src_t in_montgomery,
				       ec_shortw_crv_src_t shortw_crv,
				       prj_pt_t out_shortw);
ATTRIBUTE_WARN_UNUSED_RET int prj_pt_shortw_to_aff_pt_edwards(prj_pt_src_t in_shortw,
				    ec_edwards_crv_src_t edwards_crv,
				    aff_pt_edwards_t out_edwards,
				    fp_src_t alpha);
ATTRIBUTE_WARN_UNUSED_RET int prj_pt_shortw_to_aff_pt_montgomery(prj_pt_src_t in_shortw,
				       ec_montgomery_crv_src_t montgomery_crv,
				       aff_pt_montgomery_t out_montgomery);

#endif /* __PRJ_PT_H__ */
