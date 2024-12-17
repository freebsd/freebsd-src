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
#include <libecc/utils/print_curves.h>

/*
 * Locally convert given projective point to affine representation and
 * print x and y coordinates.
 */
void ec_point_print(const char *msg, prj_pt_src_t pt)
{
	aff_pt y_aff;
	int ret, iszero;
	y_aff.magic = WORD(0);

	MUST_HAVE(msg != NULL, ret, err);
	ret = prj_pt_iszero(pt, &iszero); EG(ret, err);
	if (iszero) {
		ext_printf("%s: infinity\n", msg);
		goto err;
	}

	ret = prj_pt_to_aff(&y_aff, pt); EG(ret, err);
	ext_printf("%s", msg);
	nn_print("x", &(y_aff.x.fp_val));
	ext_printf("%s", msg);
	nn_print("y", &(y_aff.y.fp_val));

err:
	aff_pt_uninit(&y_aff);
	return;
}

void ec_montgomery_point_print(const char *msg, aff_pt_montgomery_src_t pt)
{
	int ret;

	MUST_HAVE(msg != NULL, ret, err);
	ret = aff_pt_montgomery_check_initialized(pt); EG(ret, err);

	ext_printf("%s", msg);
	nn_print("u", &(pt->u.fp_val));
	ext_printf("%s", msg);
	nn_print("v", &(pt->v.fp_val));

err:
	return;
}

void ec_edwards_point_print(const char *msg, aff_pt_edwards_src_t pt)
{
	int ret;

	MUST_HAVE(msg != NULL, ret, err);
	ret = aff_pt_edwards_check_initialized(pt); EG(ret, err);

	ext_printf("%s", msg);
	nn_print("x", &(pt->x.fp_val));
	ext_printf("%s", msg);
	nn_print("y", &(pt->y.fp_val));

err:
	return;
}
