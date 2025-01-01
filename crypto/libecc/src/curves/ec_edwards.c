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
#include <libecc/curves/ec_edwards.h>

#define EC_EDWARDS_CRV_MAGIC ((word_t)(0x9c7349a1837c6794ULL))

/*
 * Check pointed Edwards curve structure has already been
 * initialized.
 *
 * Returns 0 on success, -1 on error.
 */
int ec_edwards_crv_check_initialized(ec_edwards_crv_src_t crv)
{
	int ret;

	MUST_HAVE((crv != NULL) && (crv->magic == EC_EDWARDS_CRV_MAGIC), ret, err);
	ret = 0;

err:
	return ret;
}

/*
 * Initialize pointed Edwards curve structure using given a and d
 * Fp elements representing curve equation (a x^2 + y^2 = 1 + d x^2 y^2) parameters.
 *
 * Returns 0 on success, -1 on error.
 */
int ec_edwards_crv_init(ec_edwards_crv_t crv, fp_src_t a, fp_src_t d, nn_src_t order)
{
	int ret, iszero, cmp;

	ret = nn_check_initialized(order); EG(ret, err);
	ret = fp_check_initialized(a); EG(ret, err);
	ret = fp_check_initialized(d); EG(ret, err);
	MUST_HAVE((a->ctx == d->ctx), ret, err);
	MUST_HAVE((crv != NULL), ret, err);

	/* a and d in Fp, must be distinct and non zero */
	MUST_HAVE((!fp_iszero(a, &iszero)) && (!iszero), ret, err);
	MUST_HAVE((!fp_iszero(d, &iszero)) && (!iszero), ret, err);
	MUST_HAVE((!fp_cmp(a, d, &cmp)) && cmp, ret, err);

	ret = fp_init(&(crv->a), a->ctx); EG(ret, err);
	ret = fp_init(&(crv->d), d->ctx); EG(ret, err);
	ret = fp_copy(&(crv->a), a); EG(ret, err);
	ret = fp_copy(&(crv->d), d); EG(ret, err);
	ret = nn_copy(&(crv->order), order); EG(ret, err);

	crv->magic = EC_EDWARDS_CRV_MAGIC;

err:
	return ret;
}


/* Uninitialize curve */
void ec_edwards_crv_uninit(ec_edwards_crv_t crv)
{
	if ((crv != NULL) && (crv->magic == EC_EDWARDS_CRV_MAGIC)) {
		crv->magic = WORD(0);
	}

	return;
}
