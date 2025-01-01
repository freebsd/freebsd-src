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
#include <libecc/curves/ec_shortw.h>

#define EC_SHORTW_CRV_MAGIC ((word_t)(0x9c7c46a1a04c6720ULL))

/*
 * Check pointed short Weierstrass curve structure has already been
 * initialized. Returns -1 on error, 0 on success.
 */
int ec_shortw_crv_check_initialized(ec_shortw_crv_src_t crv)
{
	int ret;

	MUST_HAVE((crv != NULL) && (crv->magic == EC_SHORTW_CRV_MAGIC), ret, err);
	ret = 0;

err:
	return ret;
}

/*
 * Initialize pointed short Weierstrass curve structure using given a and b
 * Fp elements representing curve equation (y^2 = x^3 + ax + b) parameters.
 * 'order' parameter is the generator point order. The function returns 0
 * on success, -1 on error.
 */
int ec_shortw_crv_init(ec_shortw_crv_t crv, fp_src_t a, fp_src_t b, nn_src_t order)
{
	fp tmp, tmp2;
	int ret, iszero;
	tmp.magic = tmp2.magic = WORD(0);

	ret = nn_check_initialized(order); EG(ret, err);
	ret = fp_check_initialized(a); EG(ret, err);
	ret = fp_check_initialized(b); EG(ret, err);
	MUST_HAVE((a->ctx == b->ctx), ret, err);
	MUST_HAVE((crv != NULL), ret, err);

	/* The discriminant (4 a^3 + 27 b^2) must be non zero */
	ret = fp_init(&tmp, a->ctx); EG(ret, err);
	ret = fp_init(&tmp2, a->ctx); EG(ret, err);
	ret = fp_sqr(&tmp, a); EG(ret, err);
	ret = fp_mul(&tmp, &tmp, a); EG(ret, err);
	ret = fp_set_word_value(&tmp2, WORD(4)); EG(ret, err);
	ret = fp_mul(&tmp, &tmp, &tmp2); EG(ret, err);

	ret = fp_set_word_value(&tmp2, WORD(27)); EG(ret, err);
	ret = fp_mul(&tmp2, &tmp2, b); EG(ret, err);
	ret = fp_mul(&tmp2, &tmp2, b); EG(ret, err);

	ret = fp_add(&tmp, &tmp, &tmp2); EG(ret, err);
	ret = fp_iszero(&tmp, &iszero); EG(ret, err);
	MUST_HAVE((!iszero), ret, err);

	ret = fp_init(&(crv->a), a->ctx); EG(ret, err);
	ret = fp_init(&(crv->b), b->ctx); EG(ret, err);
	ret = fp_init(&(crv->a_monty), a->ctx); EG(ret, err);

	ret = fp_copy(&(crv->a), a); EG(ret, err);
	ret = fp_copy(&(crv->b), b); EG(ret, err);
	ret = fp_redcify(&(crv->a_monty), a); EG(ret, err);

	ret = nn_copy(&(crv->order), order); EG(ret, err);

#ifndef NO_USE_COMPLETE_FORMULAS
	ret = fp_init(&(crv->b3), b->ctx); EG(ret, err);
	ret = fp_init(&(crv->b_monty), b->ctx); EG(ret, err);
	ret = fp_init(&(crv->b3_monty), b->ctx); EG(ret, err);

	ret = fp_add(&(crv->b3), b, b); EG(ret, err);
	ret = fp_add(&(crv->b3), &(crv->b3), b); EG(ret, err);
	ret = fp_redcify(&(crv->b_monty), b); EG(ret, err);
	ret = fp_redcify(&(crv->b3_monty), &(crv->b3)); EG(ret, err);
#endif

	crv->magic = EC_SHORTW_CRV_MAGIC;

err:
	fp_uninit(&tmp);
	fp_uninit(&tmp2);

	return ret;
}

/* Uninitialize curve */
void ec_shortw_crv_uninit(ec_shortw_crv_t crv)
{
	if((crv != NULL) && (crv->magic == EC_SHORTW_CRV_MAGIC)){
		crv->magic = WORD(0);
	}

	return;
}
