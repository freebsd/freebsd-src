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
#ifndef __PRINT_KEYS_H__
#define __PRINT_KEYS_H__

#include <libecc/sig/ec_key.h>
#include <libecc/utils/print_curves.h>

void priv_key_print(const char *msg, const ec_priv_key *priv);

void pub_key_print(const char *msg, const ec_pub_key *pub);

#endif /* __PRINT_KEYS_H__ */
