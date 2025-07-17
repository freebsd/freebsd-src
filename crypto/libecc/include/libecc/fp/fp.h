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
#ifndef __FP_H__
#define __FP_H__

#include <libecc/nn/nn.h>
#include <libecc/nn/nn_div_public.h>
#include <libecc/nn/nn_modinv.h>
#include <libecc/nn/nn_mul_public.h>
#include <libecc/nn/nn_mul_redc1.h>
#include <libecc/fp/fp_config.h>

/*
 * First, definition of our Fp context, containing all the elements
 * needed to efficiently implement Fp operations.
 */

typedef struct {
	/*
	 * Value of p (extended by one word to handle
	 * overflows in Fp). p_bitlen provides its
	 * length in bit.
	 */
	nn p;
	bitcnt_t p_bitlen;

	/* -p^-1 mod 2^(bitsizeof(word_t)) */
	word_t mpinv;

	/* 2^bitsizeof(p) mod p */
	nn r;

	/* 2^(2*bitsizeof(p)) mod p */
	nn r_square;

	/* clz(p) */
	bitcnt_t p_shift;
	/* p << p_shift */
	nn p_normalized;
	/* floor(B^3/(DMSW(p_normalized) + 1)) - B */
	word_t p_reciprocal;

	word_t magic;
} fp_ctx;

typedef fp_ctx *fp_ctx_t;
typedef const fp_ctx *fp_ctx_src_t;

ATTRIBUTE_WARN_UNUSED_RET int fp_ctx_check_initialized(fp_ctx_src_t ctx);
ATTRIBUTE_WARN_UNUSED_RET int fp_ctx_init(fp_ctx_t ctx, nn_src_t p, bitcnt_t p_bitlen,
		nn_src_t r, nn_src_t r_square,
		word_t mpinv,
		bitcnt_t p_shift, nn_src_t p_normalized, word_t p_reciprocal);
ATTRIBUTE_WARN_UNUSED_RET int fp_ctx_init_from_p(fp_ctx_t ctx, nn_src_t p);

/*
 * Then the definition of our Fp elements
 */

typedef struct {
	nn fp_val;
	fp_ctx_src_t ctx;
	word_t magic;
} fp;

typedef fp *fp_t;
typedef const fp *fp_src_t;

ATTRIBUTE_WARN_UNUSED_RET int fp_check_initialized(fp_src_t in);
ATTRIBUTE_WARN_UNUSED_RET int fp_init(fp_t A, fp_ctx_src_t fpctx);
ATTRIBUTE_WARN_UNUSED_RET int fp_init_from_buf(fp_t A, fp_ctx_src_t fpctx, const u8 *buf, u16 buflen);
void fp_uninit(fp_t A);
ATTRIBUTE_WARN_UNUSED_RET int fp_set_nn(fp_t out, nn_src_t in);
ATTRIBUTE_WARN_UNUSED_RET int fp_zero(fp_t out);
ATTRIBUTE_WARN_UNUSED_RET int fp_one(fp_t out);
ATTRIBUTE_WARN_UNUSED_RET int fp_set_word_value(fp_t out, word_t val);
ATTRIBUTE_WARN_UNUSED_RET int fp_cmp(fp_src_t in1, fp_src_t in2, int *cmp);
ATTRIBUTE_WARN_UNUSED_RET int fp_iszero(fp_src_t in, int *iszero);
ATTRIBUTE_WARN_UNUSED_RET int fp_copy(fp_t out, fp_src_t in);
ATTRIBUTE_WARN_UNUSED_RET int fp_tabselect(fp_t out, u8 idx, fp_src_t *tab, u8 tabsize);
ATTRIBUTE_WARN_UNUSED_RET int fp_eq_or_opp(fp_src_t in1, fp_src_t in2, int *eq_or_opp);
ATTRIBUTE_WARN_UNUSED_RET int fp_import_from_buf(fp_t out_fp, const u8 *buf, u16 buflen);
ATTRIBUTE_WARN_UNUSED_RET int fp_export_to_buf(u8 *buf, u16 buflen, fp_src_t in_fp);

#endif /* __FP_H__ */
