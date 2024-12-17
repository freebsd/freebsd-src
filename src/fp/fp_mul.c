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
#include <libecc/fp/fp_mul.h>
#include <libecc/fp/fp_pow.h>
#include <libecc/nn/nn_add.h>
#include <libecc/nn/nn_mul_public.h>
#include <libecc/nn/nn_modinv.h>
/* Include the "internal" header as we use non public API here */
#include "../nn/nn_div.h"

/*
 * Compute out = in1 * in2 mod p. 'out' parameter must have been initialized
 * by the caller. Returns 0 on success, -1 on error.
 *
 * Aliasing is supported.
 */
int fp_mul(fp_t out, fp_src_t in1, fp_src_t in2)
{
	int ret;

	ret = fp_check_initialized(in1); EG(ret, err);
	ret = fp_check_initialized(in2); EG(ret, err);
	ret = fp_check_initialized(out); EG(ret, err);

	MUST_HAVE(out->ctx == in1->ctx, ret, err);
	MUST_HAVE(out->ctx == in2->ctx, ret, err);

	ret = nn_mul(&(out->fp_val), &(in1->fp_val), &(in2->fp_val)); EG(ret, err);
	ret = nn_mod_unshifted(&(out->fp_val), &(out->fp_val), &(in1->ctx->p_normalized),
                         in1->ctx->p_reciprocal, in1->ctx->p_shift);

err:
	return ret;
}

/*
 * Compute out = in * in mod p. 'out' parameter must have been initialized
 * by the caller. Returns 0 on success, -1 on error.
 *
 * Aliasing is supported.
 */
int fp_sqr(fp_t out, fp_src_t in)
{
	return fp_mul(out, in, in);
}

/* We use Fermat's little theorem for our inversion in Fp:
 *    x^(p-1) = 1 mod (p) means that x^(p-2) mod(p) is the modular
 *    inverse of x mod (p)
 *
 * Aliasing is supported.
 */
int fp_inv(fp_t out, fp_src_t in)
{
	/* Use our lower layer Fermat modular inversion with precomputed
	 * Montgomery coefficients.
	 */
	int ret;

	ret = fp_check_initialized(in); EG(ret, err);
	ret = fp_check_initialized(out); EG(ret, err);

	MUST_HAVE(out->ctx == in->ctx, ret, err);

	/* We can use the Fermat inversion as p is surely prime here */
	ret = nn_modinv_fermat_redc(&(out->fp_val), &(in->fp_val), &(in->ctx->p), &(in->ctx->r), &(in->ctx->r_square), in->ctx->mpinv);

err:
	return ret;
}

/*
 * Compute out = w^-1 mod p. 'out' parameter must have been initialized
 * by the caller. Returns 0 on success, -1 on error.
 */
int fp_inv_word(fp_t out, word_t w)
{
	int ret;

	ret = fp_check_initialized(out); EG(ret, err);

	ret = nn_modinv_word(&(out->fp_val), w, &(out->ctx->p));

err:
	return ret;
}

/*
 * Compute out such that num = out * den mod p. 'out' parameter must have been initialized
 * by the caller. Returns 0 on success, -1 on error.
 *
 * Aliasing is supported.
 */
int fp_div(fp_t out, fp_src_t num, fp_src_t den)
{
	int ret;

	ret = fp_check_initialized(num); EG(ret, err);
 	ret = fp_check_initialized(den); EG(ret, err);
	ret = fp_check_initialized(out); EG(ret, err);

	MUST_HAVE(out->ctx == num->ctx, ret, err);
	MUST_HAVE(out->ctx == den->ctx, ret, err);

	if(out == num){
		/* Handle aliasing of out and num */
		fp _num;
		_num.magic = WORD(0);

		ret = fp_copy(&_num, num); EG(ret, err1);
		ret = fp_inv(out, den); EG(ret, err1);
		ret = fp_mul(out, &_num, out);

err1:
		fp_uninit(&_num);
		EG(ret, err);
	}
	else{
		ret = fp_inv(out, den); EG(ret, err);
		ret = fp_mul(out, num, out);
	}

err:
	return ret;
}
