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
#include <libecc/fp/fp.h>
#include <libecc/fp/fp_add.h>
#include <libecc/nn/nn_add.h>
#include <libecc/nn/nn_logical.h>
#include <libecc/nn/nn_mul_redc1.h>
/* Include the "internal" header as we use non public API here */
#include "../nn/nn_div.h"

#define FP_CTX_MAGIC ((word_t)(0x114366fc34955125ULL))

/*
 * Verify given Fp context has been correctly initialized, by checking
 * given pointer is valid and structure's magic has expected value.
 * Returns 0 on success, -1 on error.
 */
int fp_ctx_check_initialized(fp_ctx_src_t ctx)
{
	int ret = 0;

	MUST_HAVE(((ctx != NULL) && (ctx->magic == FP_CTX_MAGIC)), ret, err);

err:
	return ret;
}

/*
 * Initialize pointed Fp context structure from given parameters:
 *  - p: pointer to the prime defining Fp
 *  - p_bitlen: the bit length of p
 *  - r, r_square, mpinv: pointers to the Montgomery parameters r,
 *    (2^|p|) mod p), r^2 mod p and -p^-1 mod B (where B is the
 *    size in bits of words, as defined for the project, 16, 32
 *    or 64).
 *  - p_shift, p_normalized and p_reciprocal are precomputed
 *    division parameters (see ec_params_external.h for details).
 *
 * Returns 0 on success, -1 on error.
 */
int fp_ctx_init(fp_ctx_t ctx, nn_src_t p, bitcnt_t p_bitlen,
		nn_src_t r, nn_src_t r_square,
		word_t mpinv,
		bitcnt_t p_shift, nn_src_t p_normalized, word_t p_reciprocal)
{
	int ret;

	MUST_HAVE((ctx != NULL), ret, err);
	ret = nn_check_initialized(p); EG(ret, err);
	ret = nn_check_initialized(r); EG(ret, err);
	ret = nn_check_initialized(r_square); EG(ret, err);
	ret = nn_check_initialized(p_normalized); EG(ret, err);

	ret = nn_copy(&(ctx->p), p); EG(ret, err);
	ctx->p_bitlen = p_bitlen;
	ctx->mpinv = mpinv;
	ctx->p_shift = p_shift;
	ctx->p_reciprocal = p_reciprocal;
	ret = nn_copy(&(ctx->p_normalized), p_normalized); EG(ret, err);
	ret = nn_copy(&(ctx->r), r); EG(ret, err);
	ret = nn_copy(&(ctx->r_square), r_square); EG(ret, err);
	ctx->magic = FP_CTX_MAGIC;

err:
	return ret;
}

/*
 * Initialize pointed Fp context structure only from the prime p.
 * The Montgomery related parameters are dynamically computed
 * using our redc1 helpers from the NN layer. Returns 0 on success,
 * -1 on error.
 */
int fp_ctx_init_from_p(fp_ctx_t ctx, nn_src_t p_in)
{
	nn p, r, r_square, p_normalized;
	word_t mpinv, p_shift, p_reciprocal;
	bitcnt_t p_bitlen;
	int ret;
	p.magic = r.magic = r_square.magic = p_normalized.magic = WORD(0);

	MUST_HAVE((ctx != NULL), ret, err);
	ret = nn_check_initialized(p_in); EG(ret, err);

	ret = nn_init(&p, 0); EG(ret, err);
	ret = nn_copy(&p, p_in); EG(ret, err);
	ret = nn_init(&r, 0); EG(ret, err);
	ret = nn_init(&r_square, 0); EG(ret, err);
	ret = nn_init(&p_normalized, 0); EG(ret, err);

	/*
	 * In order for our reciprocal division routines to work, it is
	 * expected that the bit length (including leading zeroes) of
	 * input prime p is >= 2 * wlen where wlen is the number of bits
	 * of a word size.
	 */
	if (p.wlen < 2) {
		ret = nn_set_wlen(&p, 2); EG(ret, err);
	}

	ret = nn_compute_redc1_coefs(&r, &r_square, &p, &mpinv); EG(ret, err);
	ret = nn_compute_div_coefs(&p_normalized, &p_shift, &p_reciprocal, &p); EG(ret, err);
	ret = nn_bitlen(p_in, &p_bitlen); EG(ret, err);
	ret = fp_ctx_init(ctx, &p, p_bitlen, &r, &r_square,
			mpinv, (bitcnt_t)p_shift, &p_normalized, p_reciprocal);

err:
	nn_uninit(&p);
	nn_uninit(&r);
	nn_uninit(&r_square);
	nn_uninit(&p_normalized);

	return ret;
}

#define FP_MAGIC ((word_t)(0x14e96c8ab28221efULL))

/*
 * Verify given Fp element has been correctly intialized, by checking
 * given pointer is valid and structure magic has expected value.
 * Returns 0 on success, -1 on error.
 */
int fp_check_initialized(fp_src_t in)
{
	int ret = 0;

	MUST_HAVE(((in != NULL) && (in->magic == FP_MAGIC) && (in->ctx != NULL) && (in->ctx->magic == FP_CTX_MAGIC)), ret, err);

err:
	return ret;
}

/*
 * Initialilize pointed Fp element structure with given Fp context. Initial
 * value of Fp element is set to 0.Returns 0 on success, -1 on error.
 */
int fp_init(fp_t in, fp_ctx_src_t fpctx)
{
	int ret;

	MUST_HAVE((in != NULL), ret, err);

	ret = fp_ctx_check_initialized(fpctx); EG(ret, err);
	ret = nn_init(&(in->fp_val), (u16)((fpctx->p.wlen) * WORD_BYTES)); EG(ret, err);

	in->ctx = fpctx;
	in->magic = FP_MAGIC;

err:
	return ret;
}

/*
 * Same as above but providing the element an initial value given by 'buf'
 * content (in big endian order) of size 'buflen'. Content of 'buf' must
 * be less than p. Returns 0 on success, -1 on error.
 */
int fp_init_from_buf(fp_t in, fp_ctx_src_t fpctx, const u8 *buf, u16 buflen)
{
	int ret;

	ret = fp_ctx_check_initialized(fpctx); EG(ret, err);
	ret = fp_init(in, fpctx); EG(ret, err);
	ret = fp_import_from_buf(in, buf, buflen);

err:
	return ret;
}

/*
 * Uninitialize pointed Fp element to prevent further use (magic field
 * in the structure is zeroized) and zeroize associated storage space.
 * Note that the Fp context pointed to by Fp element (passed during
 * init) is left untouched.
 */
void fp_uninit(fp_t in)
{
	if((in != NULL) && (in->magic == FP_MAGIC) && (in->ctx != NULL)){
		nn_uninit(&in->fp_val);

		in->ctx = NULL;
		in->magic = WORD(0);
	}

	return;
}

/*
 * Set value of given Fp element to that of given nn. The value of
 * given nn must be less than that of p, i.e. no reduction modulo
 * p is performed by the function. Returns 0 on success, -1 on error.
 */
int fp_set_nn(fp_t out, nn_src_t in)
{
	int ret, cmp;

	ret = fp_check_initialized(out); EG(ret, err);
	ret = nn_check_initialized(in); EG(ret, err);
	ret = nn_copy(&(out->fp_val), in); EG(ret, err);
	ret = nn_cmp(&(out->fp_val), &(out->ctx->p), &cmp); EG(ret, err);

	MUST_HAVE((cmp < 0), ret, err);

	/* Set the wlen to the length of p */
	ret = nn_set_wlen(&(out->fp_val), out->ctx->p.wlen);

err:
	return ret;
}

/*
 * Set 'out' to the element 0 of Fp (neutral element for addition). Returns 0
 * on success, -1 on error.
 */
int fp_zero(fp_t out)
{
	int ret;

	ret = fp_check_initialized(out); EG(ret, err);
	ret = nn_set_word_value(&(out->fp_val), 0); EG(ret, err);
	/* Set the wlen to the length of p */
	ret = nn_set_wlen(&(out->fp_val), out->ctx->p.wlen);

err:
	return ret;
}

/*
 * Set out to the element 1 of Fp (neutral element for multiplication). Returns
 * 0 on success, -1 on error.
 */
int fp_one(fp_t out)
{
	int ret, isone;
	word_t val;

	ret = fp_check_initialized(out); EG(ret, err);
	/* One is indeed 1 except if p = 1 where it is 0 */
	ret = nn_isone(&(out->ctx->p), &isone); EG(ret, err);

	val = isone ? WORD(0) : WORD(1);

	ret = nn_set_word_value(&(out->fp_val), val); EG(ret, err);

	/* Set the wlen to the length of p */
	ret = nn_set_wlen(&(out->fp_val), out->ctx->p.wlen);

err:
	return ret;
}

/* Set out to the asked word: the value must be < p */
int fp_set_word_value(fp_t out, word_t val)
{
	int ret, cmp;

	ret = fp_check_initialized(out); EG(ret, err);

	/* Check that our value is indeed < p */
	ret = nn_cmp_word(&(out->ctx->p), val, &cmp); EG(ret, err);
	MUST_HAVE((cmp > 0), ret, err);

	/* Set the word in the NN layer */
	ret = nn_set_word_value(&(out->fp_val), val); EG(ret, err);

	/* Set the wlen to the length of p */
	ret = nn_set_wlen(&(out->fp_val), out->ctx->p.wlen);

err:
	return ret;
}


/*
 * Compare given Fp elements. The function returns -1 if the value of in1 is
 * less than that of in2, 0 if they are equal and 1 if the value of in2 is
 * more than that of in1. Obviously, both parameters must be initialized and
 * belong to the same field (i.e. must have been initialized from the same
 * context). Returns 0 on success, -1 on error.
 */
int fp_cmp(fp_src_t in1, fp_src_t in2, int *cmp)
{
	int ret;

	ret = fp_check_initialized(in1); EG(ret, err);
	ret = fp_check_initialized(in2); EG(ret, err);

	MUST_HAVE((in1->ctx == in2->ctx), ret, err);

	ret = nn_cmp(&(in1->fp_val), &(in2->fp_val), cmp);

err:
	return ret;
}

/* Check if given Fp element has value 0. Returns 0 on success, -1 on error. */
int fp_iszero(fp_src_t in, int *iszero)
{
	int ret;

	ret = fp_check_initialized(in); EG(ret, err);
	ret = nn_iszero(&(in->fp_val), iszero);

err:
	return ret;
}


/*
 * Copy value of pointed Fp element (in) into pointed Fp element (out). If
 * output is already initialized, check that the Fp contexts are consistent.
 * Else, output is initialized with the same field context as input. Returns 0
 * on success, -1 on error.
 *
 * Aliasing of input and output is supported.
 */
int fp_copy(fp_t out, fp_src_t in)
{
	int ret;

	ret = fp_check_initialized(in); EG(ret, err);

	MUST_HAVE((out != NULL), ret, err);

	if ((out->magic == FP_MAGIC) && (out->ctx != NULL)) {
		MUST_HAVE((out->ctx == in->ctx), ret, err);
	} else {
		ret = fp_init(out, in->ctx); EG(ret, err);
	}

	ret = nn_copy(&(out->fp_val), &(in->fp_val));

err:
	return ret;
}


/*
 * Given a table 'tab' pointing to a set of 'tabsize' Fp elements, the
 * function copies the value of element at position idx (idx < tabsize)
 * in 'out' parameters. Masking is used to avoid leaking which element
 * was copied.
 *
 * Note that the main copying loop is done on the |p| bits for all
 * Fp elements and not based on the specific effective size of each
 * Fp elements in 'tab'
 *
 * Returns 0 on success, -1 on error.
 *
 * Aliasing of out and the selected element inside the tab is NOT supported.
 *
 */
int fp_tabselect(fp_t out, u8 idx, fp_src_t *tab, u8 tabsize)
{
	u8 i, k, p_wlen;
	word_t mask;
	nn_src_t p;
	int ret;

	/* Basic sanity checks */
	MUST_HAVE(((tab != NULL) && (idx < tabsize)), ret, err);

	ret = fp_check_initialized(out); EG(ret, err);

	/* Make things more readable */
	p = &(out->ctx->p);
	MUST_HAVE((p != NULL), ret, err);
	p_wlen = p->wlen;

	/* Zeroize out and enforce its size. */
	ret = nn_zero(&(out->fp_val)); EG(ret, err);
	out->fp_val.wlen = p_wlen;

	for (k = 0; k < tabsize; k++) {
		/* Check current element is initialized and from Fp */
		ret = fp_check_initialized(tab[k]); EG(ret, err);

		MUST_HAVE(((&(tab[k]->ctx->p)) == p), ret, err);

		mask = WORD_MASK_IFNOTZERO(idx == k);

		for (i = 0; i < p_wlen; i++) {
			out->fp_val.val[i] |= (tab[k]->fp_val.val[i] & mask);
		}
	}

err:
	return ret;
}

/*
 * The function tests if in1 and in2 parameters are equal or opposite in
 * Fp. In that case, 'eq_or_opp' out parameter is set to 1. When in1 and
 * in2 are not equal or opposite, 'eq_or_opp' is set to 0. The function
 * returns 0 on success and -1 on error. 'eq_or_opp' is only meaningful
 * on success, i.e. if the return value is 0.
 *
 * Aliasing of inputs is supported.
 */
int fp_eq_or_opp(fp_src_t in1, fp_src_t in2, int *eq_or_opp)
{
	int ret, cmp_eq, cmp_opp;
	fp opp;
	opp.magic = WORD(0);

	MUST_HAVE((eq_or_opp != NULL), ret, err);
	ret = fp_check_initialized(in1); EG(ret, err);
	ret = fp_check_initialized(in2); EG(ret, err);
	MUST_HAVE((in1->ctx == in2->ctx), ret, err);

	ret = fp_init(&opp, in1->ctx); EG(ret, err);
	ret = fp_neg(&opp, in2); EG(ret, err);
	ret = nn_cmp(&(in1->fp_val), &(in2->fp_val), &cmp_eq); EG(ret, err);
	ret = nn_cmp(&(in1->fp_val), &(opp.fp_val), &cmp_opp); EG(ret, err);

	(*eq_or_opp) = ((cmp_eq == 0) | (cmp_opp == 0));

err:
	fp_uninit(&opp);

	return ret;
}

/*
 * Import given buffer of length buflen as a value for out_fp. Buffer is
 * expected to be in big endian format. out_fp is expected to be already
 * initialized w/ a proper Fp context, providing a value for p. The value
 * in buf is also expected to be less than the one of p. The function
 * returns 0 on success and -1 on error.
 */
int fp_import_from_buf(fp_t out_fp, const u8 *buf, u16 buflen)
{
	int ret, cmp;

	ret = fp_check_initialized(out_fp); EG(ret, err);
	ret = nn_init_from_buf(&(out_fp->fp_val), buf, buflen); EG(ret, err);
	ret = nn_cmp(&(out_fp->fp_val), &(out_fp->ctx->p), &cmp); EG(ret, err);
	MUST_HAVE((cmp < 0), ret, err);

err:
	return ret;
}

/*
 * Export an element from Fp to a buffer using the underlying NN export
 * primitive. The function returns 0 on sucess, -1 on error.
 */
int fp_export_to_buf(u8 *buf, u16 buflen, fp_src_t in_fp)
{
	int ret;

	ret = fp_check_initialized(in_fp); EG(ret, err);
	ret = nn_export_to_buf(buf, buflen, &(in_fp->fp_val));

err:
	return ret;
}
