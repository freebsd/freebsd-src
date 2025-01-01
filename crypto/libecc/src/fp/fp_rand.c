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

#include <libecc/fp/fp_rand.h>
#include <libecc/nn/nn_rand.h>

/*
 * Initialize given Fp element in 'out' storage space to a Fp value chosen
 * uniformly at random in [1, p-1] where p is provided by 'ctx'. The function
 * returns 0 on success, -1 on error.
 */
int fp_get_random(fp_t out, fp_ctx_src_t ctx)
{
	int ret;

	ret = fp_init(out, ctx); EG(ret, err);
	ret = nn_get_random_mod(&(out->fp_val), &(ctx->p));

err:
	return ret;
}
