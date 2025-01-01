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
#include <libecc/fp/fp_mul.h>
#include <libecc/fp/fp_mul_redc1.h>
#include <libecc/fp/fp_montgomery.h>

/* Compute out = in1 + in2 mod p in the Montgomery form.
 * Inputs and outputs are in their Montgomery form.
 * Returns 0 on success, -1 on error.
 *
 * Aliasing is supported.
 */
int fp_add_monty(fp_t out, fp_src_t in1, fp_src_t in2)
{
	return fp_add(out, in1, in2);
}

/* Compute out = in1 - in2 mod p in the Montgomery form.
 * Inputs and outputs are in their Montgomery form.
 * Returns 0 on success, -1 on error.
 *
 * Aliasing is supported.
 */
int fp_sub_monty(fp_t out, fp_src_t in1, fp_src_t in2)
{
	return fp_sub(out, in1, in2);
}

/* Compute out = in1 * in2 mod p in the Montgomery form.
 * Inputs and outputs are in their Montgomery form.
 * Returns 0 on success, -1 on error.
 *
 * Aliasing is supported.
 */
int fp_mul_monty(fp_t out, fp_src_t in1, fp_src_t in2)
{
	return fp_mul_redc1(out, in1, in2);
}

/* Compute out = in * in mod p in the Montgomery form.
 * Inputs and outputs are in their Montgomery form.
 * Returns 0 on success, -1 on error.
 *
 * Aliasing is supported.
 */
int fp_sqr_monty(fp_t out, fp_src_t in)
{
	return fp_sqr_redc1(out, in);
}

/*
 * Compute out such that in1 = out * in2 mod p in the Montgomery form.
 * Inputs and outputs are in their Montgomery form.
 * Returns 0 on success, -1 on error. out must be initialized by the caller.
 *
 * Aliasing is supported.
 */
int fp_div_monty(fp_t out, fp_src_t in1, fp_src_t in2)
{
	int ret, iszero;

	ret = fp_check_initialized(in1); EG(ret, err);
	ret = fp_check_initialized(in2); EG(ret, err);
	ret = fp_check_initialized(out); EG(ret, err);

	MUST_HAVE((out->ctx == in1->ctx), ret, err);
	MUST_HAVE((out->ctx == in2->ctx), ret, err);
        FORCE_USED_VAR(iszero); /* silence warning when macro results in nothing */
	MUST_HAVE(!fp_iszero(in2, &iszero) && (!iszero), ret, err);

	ret = fp_div(out, in1, in2); EG(ret, err);
	ret = fp_redcify(out, out);

err:
	return ret;
}
