/*
 *  Copyright (C) 2021 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#include <libecc/curves/ec_montgomery.h>

#define EC_MONTGOMERY_CRV_MAGIC ((word_t)(0x83734673a0443720ULL))

/* Check if a Montgomery curve is initialized.
 * Returns 0 on success, -1 on error.
 */
int ec_montgomery_crv_check_initialized(ec_montgomery_crv_src_t crv)
{
	int ret;

	MUST_HAVE((crv != NULL) && (crv->magic == EC_MONTGOMERY_CRV_MAGIC), ret, err);
	ret = 0;

err:
	return ret;
}

/*
 * Initialize pointed Montgomery curve structure using given A and B
 * Fp elements representing curve equation (B v^2 = u^3 + A u^2 + u) parameters.
 *
 * The function returns 0 on success, -1 on error.
 */
int ec_montgomery_crv_init(ec_montgomery_crv_t crv, fp_src_t A, fp_src_t B, nn_src_t order)
{
	int ret, iszero;
	fp tmp;
	tmp.magic = WORD(0);

	MUST_HAVE((crv != NULL), ret, err);

	ret = nn_check_initialized(order); EG(ret, err);
	ret = fp_check_initialized(A); EG(ret, err);
	ret = fp_check_initialized(B); EG(ret, err);
	MUST_HAVE(A->ctx == B->ctx, ret, err);

	ret = fp_init(&tmp, A->ctx); EG(ret, err);

	/* A and B elements of Fp, A unequal to (+/-)2 and B non zero */
	ret = fp_set_word_value(&tmp, 2); EG(ret, err);
	ret = fp_add(&tmp, A, &tmp); EG(ret, err);
	MUST_HAVE((!fp_iszero(&tmp, &iszero)) && (!iszero), ret, err);

	ret = fp_set_word_value(&tmp, 2); EG(ret, err);
	ret = fp_sub(&tmp, A, &tmp); EG(ret, err);
	MUST_HAVE((!fp_iszero(&tmp, &iszero)) && (!iszero), ret, err);
	MUST_HAVE((!fp_iszero(B, &iszero)) && (!iszero), ret, err);

	ret = fp_init(&(crv->A), A->ctx); EG(ret, err);
	ret = fp_init(&(crv->B), B->ctx); EG(ret, err);

	ret = fp_copy(&(crv->A), A); EG(ret, err);
	ret = fp_copy(&(crv->B), B); EG(ret, err);

	ret = nn_copy(&(crv->order), order); EG(ret, err);

	crv->magic = EC_MONTGOMERY_CRV_MAGIC;

err:
	fp_uninit(&tmp);

	return ret;
}

/* Uninitialize curve
 */
void ec_montgomery_crv_uninit(ec_montgomery_crv_t crv)
{
	if ((crv != NULL) && (crv->magic == EC_MONTGOMERY_CRV_MAGIC)) {
		crv->magic = WORD(0);
	}

	return;
}
