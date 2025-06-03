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
#include <libecc/nn/nn_logical.h>
#include <libecc/nn/nn_mod_pow.h>
#include <libecc/fp/fp_pow.h>
#include <libecc/fp/fp.h>

/*
 * NOT constant time with regard to the bitlength of exp.
 * Aliasing not supported. Expects caller to check parameters
 * have been initialized. This is an internal helper.
 *
 * Returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _fp_pow(fp_t out, fp_src_t base, nn_src_t exp)
{
	/* Use the lower layer modular exponentiation */
	return nn_mod_pow_redc(&(out->fp_val), &(base->fp_val), exp, &(out->ctx->p), &(out->ctx->r), &(out->ctx->r_square), out->ctx->mpinv);
}

/*
 * Same purpose as above but handles aliasing of 'base' and 'out', i.e.
 * base is passed via 'out'.  Expects caller to check parameters
 * have been initialized. This is an internal helper.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _fp_pow_aliased(fp_t out, nn_src_t exp)
{
	fp base;
	int ret;
	base.magic = WORD(0);

	ret = fp_init(&base, out->ctx); EG(ret, err);
	ret = fp_copy(&base, out); EG(ret, err);
	ret = _fp_pow(out, &base, exp); EG(ret, err);

err:
	fp_uninit(&base);

	return ret;
}

/*
 * Compute out = base^exp (p). 'base', 'exp' and 'out' are supposed to be initialized.
 * Aliased version of previous one.
 *
 * Aliasing is supported.
 */
int fp_pow(fp_t out, fp_src_t base, nn_src_t exp)
{
	int ret;

	ret = fp_check_initialized(base); EG(ret, err);
	ret = nn_check_initialized(exp); EG(ret, err);
	ret = fp_check_initialized(out); EG(ret, err);
	MUST_HAVE(((&(out->ctx->p)) == (&(base->ctx->p))), ret, err);

	/* Handle output aliasing */
	if (out == base) {
		ret = _fp_pow_aliased(out, exp);
	} else {
		ret = _fp_pow(out, base, exp);
	}

err:
	return ret;
}
