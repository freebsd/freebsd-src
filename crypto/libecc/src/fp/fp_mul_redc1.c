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
#include <libecc/fp/fp_mul_redc1.h>

/*
 * Internal helper performing Montgomery multiplication. The function returns
 * 0 on success, -1 on error.
 *
 * CAUTION: the function does not check input parameters. Those checks MUST be
 * performed by the caller.
 */
ATTRIBUTE_WARN_UNUSED_RET static inline int _fp_mul_redc1(nn_t out, nn_src_t in1, nn_src_t in2,
				 fp_ctx_src_t ctx)
{
	return nn_mul_redc1(out, in1, in2, &(ctx->p), ctx->mpinv);
}

/*
 * Compute out = in1 * in2 mod (p) in redcified form.
 *
 * Exported version based on previous one, that sanity checks input parameters.
 * The function returns 0 on success, -1 on error.
 *
 * Aliasing is supported.
 */
int fp_mul_redc1(fp_t out, fp_src_t in1, fp_src_t in2)
{
	int ret;

	ret = fp_check_initialized(in1); EG(ret, err);
	ret = fp_check_initialized(in2); EG(ret, err);
	ret = fp_check_initialized(out); EG(ret, err);

	MUST_HAVE((out->ctx == in1->ctx), ret, err);
	MUST_HAVE((out->ctx == in2->ctx), ret, err);

	ret = _fp_mul_redc1(&(out->fp_val), &(in1->fp_val), &(in2->fp_val),
			    out->ctx);

err:
	return ret;
}

/*
 * Compute out = in * in mod (p) in redcified form.
 *
 * Aliasing is supported.
 */
int fp_sqr_redc1(fp_t out, fp_src_t in)
{
	return fp_mul_redc1(out, in, in);
}

/*
 * Compute out = redcified form of in.
 * redcify could be done by shifting and division by p. The function returns 0
 * on success, -1 on error.
 *
 * Aliasing is supported.
 */
int fp_redcify(fp_t out, fp_src_t in)
{
	int ret;

	ret = fp_check_initialized(in); EG(ret, err);
	ret = fp_check_initialized(out); EG(ret, err);

	MUST_HAVE((out->ctx == in->ctx), ret, err);

	ret = _fp_mul_redc1(&(out->fp_val), &(in->fp_val), &(out->ctx->r_square),
			    out->ctx);

err:
	return ret;
}

/*
 * Compute out = unredcified form of in.
 * The function returns 0 on success, -1 on error.
 *
 * Aliasing is supported.
 */
int fp_unredcify(fp_t out, fp_src_t in)
{
	int ret;
	nn one;
	one.magic = WORD(0);

	ret = fp_check_initialized(in); EG(ret, err);
	ret = fp_check_initialized(out); EG(ret, err);
	ret = nn_init(&one, 0);  EG(ret, err);
	ret = nn_one(&one); EG(ret, err);
	ret = _fp_mul_redc1(&(out->fp_val), &(in->fp_val), &one, out->ctx);

err:
	nn_uninit(&one);

	return ret;
}
