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
 *  Licensed under dual BSD/GPL v2 license.
 */
#include <libecc/fp/fp.h>
#include <libecc/fp/fp_add.h>
#include <libecc/nn/nn_add.h>
#include <libecc/nn/nn_logical.h>
#include <libecc/nn/nn_mul_redc1.h>
#include "../nn/nn_div.h"

static const word_t FP_CTX_MAGIC = 0x114366fc34955125ULL;
static const word_t FP_MAGIC     = 0x14e96c8ab28221efULL;

/* --- Helper Macros Removed --- */

/* Verify Fp context */
int fp_ctx_check_initialized(fp_ctx_src_t ctx) {
    if (!ctx || ctx->magic != FP_CTX_MAGIC)
        return -1;
    return 0;
}

/* Initialize Fp context from all parameters */
int fp_ctx_init(fp_ctx_t ctx, nn_src_t p, bitcnt_t p_bitlen,
                nn_src_t r, nn_src_t r_square,
                word_t mpinv,
                bitcnt_t p_shift, nn_src_t p_normalized, word_t p_reciprocal) 
{
    if (!ctx || !p || !r || !r_square || !p_normalized)
        return -1;

    if (nn_check_initialized(p) ||
        nn_check_initialized(r) ||
        nn_check_initialized(r_square) ||
        nn_check_initialized(p_normalized))
        return -1;

    if (nn_copy(&ctx->p, p) ||
        nn_copy(&ctx->p_normalized, p_normalized) ||
        nn_copy(&ctx->r, r) ||
        nn_copy(&ctx->r_square, r_square))
        return -1;

    ctx->p_bitlen = p_bitlen;
    ctx->mpinv = mpinv;
    ctx->p_shift = p_shift;
    ctx->p_reciprocal = p_reciprocal;
    ctx->magic = FP_CTX_MAGIC;

    return 0;
}

/* Initialize Fp context from prime only */
int fp_ctx_init_from_p(fp_ctx_t ctx, nn_src_t p_in) {
    nn p = {0}, r = {0}, r_square = {0}, p_norm = {0};
    word_t mpinv, p_shift, p_reciprocal;
    bitcnt_t p_bitlen;

    if (!ctx || !p_in || nn_check_initialized(p_in))
        return -1;

    if (nn_init(&p, 0) ||
        nn_init(&r, 0) ||
        nn_init(&r_square, 0) ||
        nn_init(&p_norm, 0))
        return -1;

    if (nn_copy(&p, p_in))
        goto cleanup;

    if (p.wlen < 2 && nn_set_wlen(&p, 2))
        goto cleanup;

    if (nn_compute_redc1_coefs(&r, &r_square, &p, &mpinv) ||
        nn_compute_div_coefs(&p_norm, &p_shift, &p_reciprocal, &p) ||
        nn_bitlen(p_in, &p_bitlen) ||
        fp_ctx_init(ctx, &p, p_bitlen, &r, &r_square, mpinv, p_shift, &p_norm, p_reciprocal))
        goto cleanup;

    nn_uninit(&p);
    nn_uninit(&r);
    nn_uninit(&r_square);
    nn_uninit(&p_norm);
    return 0;

cleanup:
    nn_uninit(&p);
    nn_uninit(&r);
    nn_uninit(&r_square);
    nn_uninit(&p_norm);
    return -1;
}

/* Verify Fp element */
int fp_check_initialized(fp_src_t in) {
    if (!in || !in->ctx || in->magic != FP_MAGIC || in->ctx->magic != FP_CTX_MAGIC)
        return -1;
    return 0;
}

/* Initialize Fp element with zero value */
int fp_init(fp_t in, fp_ctx_src_t fpctx) {
    if (!in || fp_ctx_check_initialized(fpctx))
        return -1;

    if (nn_init(&in->fp_val, fpctx->p.wlen * WORD_BYTES))
        return -1;

    in->ctx = fpctx;
    in->magic = FP_MAGIC;
    return 0;
}

/* Initialize Fp element from buffer */
int fp_init_from_buf(fp_t in, fp_ctx_src_t ctx, const u8 *buf, u16 buflen) {
    if (fp_init(in, ctx))
        return -1;

    if (fp_import_from_buf(in, buf, buflen))
        return -1;

    return 0;
}

/* Uninitialize Fp element */
void fp_uninit(fp_t in) {
    if (!in || in->magic != FP_MAGIC)
        return;
    nn_uninit(&in->fp_val);
    in->ctx = NULL;
    in->magic = 0;
}

/* Set Fp element to given nn value */
int fp_set_nn(fp_t out, nn_src_t in) {
    int cmp;
    if (fp_check_initialized(out) || nn_check_initialized(in))
        return -1;

    if (nn_copy(&out->fp_val, in) ||
        nn_cmp(&out->fp_val, &out->ctx->p, &cmp) || cmp >= 0)
        return -1;

    nn_set_wlen(&out->fp_val, out->ctx->p.wlen);
    return 0;
}

/* Set zero */
int fp_zero(fp_t out) {
    if (fp_check_initialized(out))
        return -1;
    nn_set_word_value(&out->fp_val, 0);
    nn_set_wlen(&out->fp_val, out->ctx->p.wlen);
    return 0;
}

/* Set one */
int fp_one(fp_t out) {
    int isone;
    word_t val;
    if (fp_check_initialized(out))
        return -1;

    if (nn_isone(&out->ctx->p, &isone))
        return -1;

    val = isone ? 0 : 1;
    nn_set_word_value(&out->fp_val, val);
    nn_set_wlen(&out->fp_val, out->ctx->p.wlen);
    return 0;
}

/* Compare Fp elements */
int fp_cmp(fp_src_t in1, fp_src_t in2, int *cmp) {
    if (fp_check_initialized(in1) || fp_check_initialized(in2))
        return -1;

    if (in1->ctx != in2->ctx)
        return -1;

    return nn_cmp(&in1->fp_val, &in2->fp_val, cmp);
}

/* Check if Fp element is zero */
int fp_iszero(fp_src_t in, int *iszero) {
    if (fp_check_initialized(in))
        return -1;
    return nn_iszero(&in->fp_val, iszero);
}

/* Copy Fp element */
int fp_copy(fp_t out, fp_src_t in) {
    if (fp_check_initialized(in))
        return -1;

    if (!out->ctx) {
        if (fp_init(out, in->ctx))
            return -1;
    } else if (out->ctx != in->ctx) {
        return -1;
    }

    return nn_copy(&out->fp_val, &in->fp_val);
}

/* Tab select using masking */
int fp_tabselect(fp_t out, u8 idx, fp_src_t *tab, u8 tabsize) {
    if (!tab || idx >= tabsize || fp_check_initialized(out))
        return -1;

    nn_zero(&out->fp_val);
    out->fp_val.wlen = out->ctx->p.wlen;

    for (u8 k = 0; k < tabsize; k++) {
        if (fp_check_initialized(tab[k]) || tab[k]->ctx != out->ctx)
            return -1;

        word_t mask = (word_t)-(idx == k);
        for (u8 i = 0; i < out->fp_val.wlen; i++)
            out->fp_val.val[i] |= (tab[k]->fp_val.val[i] & mask);
    }
    return 0;
}

/* Equality or opposite check */
int fp_eq_or_opp(fp_src_t in1, fp_src_t in2, int *eq_or_opp) {
    if (!eq_or_opp || fp_check_initialized(in1) || fp_check_initialized(in2))
        return -1;
    if (in1->ctx != in2->ctx)
        return -1;

    fp neg;
    if (fp_init(&neg, in1->ctx))
        return -1;

    if (fp_neg(&neg, in2))
        goto cleanup;

    int cmp_eq, cmp_opp;
    if (nn_cmp(&in1->fp_val, &in2->fp_val, &cmp_eq) ||
        nn_cmp(&in1->fp_val, &neg.fp_val, &cmp_opp))
        goto cleanup;

    *eq_or_opp = (cmp_eq == 0 || cmp_opp == 0);

cleanup:
    fp_uninit(&neg);
    return 0;
}

/* Import from buffer */
int fp_import_from_buf(fp_t out_fp, const u8 *buf, u16 buflen) {
    if (fp_check_initialized(out_fp))
        return -1;
    int cmp;
    if (nn_init_from_buf(&out_fp->fp_val, buf, buflen) ||
        nn_cmp(&out_fp->fp_val, &out_fp->ctx->p, &cmp) || cmp >= 0)
        return -1;
    return 0;
}

/* Export to buffer */
int fp_export_to_buf(u8 *buf, u16 buflen, fp_src_t in_fp) {
    if (fp_check_initialized(in_fp))
        return -1;
    return nn_export_to_buf(buf, buflen, &in_fp->fp_val);
}
