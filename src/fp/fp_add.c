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
#include <libecc/fp/fp_add.h>
#include <libecc/nn/nn_add.h>

/*
 * Compute out = in1 + in2 mod p. 'out' parameter must have been initialized
 * by the caller. Returns 0 on success, -1 on error.
 *
 * Aliasing is supported.
 */
int fp_add(fp_t out, fp_src_t in1, fp_src_t in2)
{
	int ret, cmp;

	ret = fp_check_initialized(out); EG(ret, err);
	ret = fp_check_initialized(in1); EG(ret, err);
	ret = fp_check_initialized(in2); EG(ret, err);

	MUST_HAVE(((&(in1->ctx->p)) == (&(in2->ctx->p))), ret, err);
	MUST_HAVE(((&(in1->ctx->p)) == (&(out->ctx->p))), ret, err);
	FORCE_USED_VAR(cmp); /* silence warning when macro results in nothing */
	SHOULD_HAVE(!nn_cmp(&in1->fp_val, &(in1->ctx->p), &cmp) && (cmp < 0), ret, err);
	SHOULD_HAVE(!nn_cmp(&in2->fp_val, &(in2->ctx->p), &cmp) && (cmp < 0), ret, err);

	ret = nn_mod_add(&(out->fp_val), &(in1->fp_val),
			 &(in2->fp_val), &(in1->ctx->p));

err:
	return ret;
}

/*
 * Compute out = in + 1 mod p. 'out' parameter must have been initialized
 * by the caller. Returns 0 on success, -1 on error.
 *
 * Aliasing is supported.
 */
int fp_inc(fp_t out, fp_src_t in)
{
	int ret, cmp;

	ret = fp_check_initialized(in); EG(ret, err);
	ret = fp_check_initialized(out); EG(ret, err);

	MUST_HAVE(((&(in->ctx->p)) == (&(out->ctx->p))), ret, err);
	FORCE_USED_VAR(cmp); /* silence warning when macro results in nothing */
	SHOULD_HAVE(!nn_cmp(&in->fp_val, &(in->ctx->p), &cmp) && (cmp < 0), ret, err);

	ret = nn_mod_inc(&(out->fp_val), &(in->fp_val), &(in->ctx->p));

err:
	return ret;
}

/*
 * Compute out = in1 - in2 mod p. 'out' parameter must have been initialized
 * by the caller. Returns 0 on success, -1 on error.
 *
 * Aliasing is supported.
 */
int fp_sub(fp_t out, fp_src_t in1, fp_src_t in2)
{
	int ret, cmp;

	ret = fp_check_initialized(out); EG(ret, err);
	ret = fp_check_initialized(in1); EG(ret, err);
	ret = fp_check_initialized(in2); EG(ret, err);

	MUST_HAVE(((&(in1->ctx->p)) == (&(in2->ctx->p))), ret, err);
	MUST_HAVE(((&(in1->ctx->p)) == (&(out->ctx->p))), ret, err);
	FORCE_USED_VAR(cmp); /* silence warning when macro results in nothing */
	SHOULD_HAVE(!nn_cmp(&in1->fp_val, &(in1->ctx->p), &cmp) && (cmp < 0), ret, err);
	SHOULD_HAVE(!nn_cmp(&in2->fp_val, &(in2->ctx->p), &cmp) && (cmp < 0), ret, err);

	ret = nn_mod_sub(&(out->fp_val), &(in1->fp_val),
			 &(in2->fp_val), &(in1->ctx->p));

err:
	return ret;
}

/*
 * Compute out = in - 1 mod p. 'out' parameter must have been initialized
 * by the caller. Returns 0 on success, -1 on error.
 *
 * Aliasing is supported.
 */
int fp_dec(fp_t out, fp_src_t in)
{
	int ret, cmp;

	ret = fp_check_initialized(out); EG(ret, err);
	ret = fp_check_initialized(in); EG(ret, err);

	MUST_HAVE(((&(in->ctx->p)) == (&(out->ctx->p))), ret, err);
	FORCE_USED_VAR(cmp); /* silence warning when macro results in nothing */
	SHOULD_HAVE(!nn_cmp(&in->fp_val, &(in->ctx->p), &cmp) && (cmp < 0), ret, err);

	ret = nn_mod_dec(&(out->fp_val), &(in->fp_val), &(in->ctx->p));

err:
	return ret;
}

/*
 * Compute out = -in mod p = (p - in) mod p. 'out' parameter must have been
 * initialized by the caller. Returns 0 on success, -1 on error.
 *
 * Aliasing is supported.
 */
int fp_neg(fp_t out, fp_src_t in)
{
	int ret, cmp;

	ret = fp_check_initialized(in); EG(ret, err);
	ret = fp_check_initialized(out); EG(ret, err);

	MUST_HAVE(((&(in->ctx->p)) == (&(out->ctx->p))), ret, err);
	FORCE_USED_VAR(cmp); /* silence warning when macro results in nothing */
	SHOULD_HAVE(!nn_cmp(&in->fp_val, &(in->ctx->p), &cmp) && (cmp < 0), ret, err);

	ret = nn_sub(&(out->fp_val), &(in->ctx->p), &(in->fp_val));

err:
	return ret;
}
