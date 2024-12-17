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
#include <libecc/utils/print_keys.h>

void priv_key_print(const char *msg, const ec_priv_key *priv)
{
	int ret;

	MUST_HAVE(msg != NULL, ret, err);
	ret = priv_key_check_initialized(priv); EG(ret, err);

	nn_print(msg, &(priv->x));

err:
	return;
}

void pub_key_print(const char *msg, const ec_pub_key *pub)
{
	int ret;

	MUST_HAVE(msg != NULL, ret, err);
	ret = pub_key_check_initialized(pub); EG(ret, err);

	ec_point_print(msg, &(pub->y));

err:
	return;
}
