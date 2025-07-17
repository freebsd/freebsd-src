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
#ifndef __SSS_PRIVATE_H__
#define __SSS_PRIVATE_H__

/* NOTE: this is a header that is private to SSS
 * and should not be included elsewhare to avoid exposing
 * unnecessary APIs and symbols when only using SSS.
 */

/* NOTE: we need the arithmetic library for SSS as all
 * operations will take place in Fp with p a public known
 * prime number.
 */
#include <libecc/lib_ecc_config.h>
#include <libecc/libarith.h>
/* We use HMAC */
#include <libecc/hash/hmac.h>
/* We generate random */
#include <libecc/external_deps/rand.h>

#endif /* __SSS_PRIVATE_H__ */
