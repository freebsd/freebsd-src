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
#include <libecc/lib_ecc_config.h>
#include <libecc/lib_ecc_types.h>
#if defined(WITH_SIG_ECDSA) && defined(USE_CRYPTOFUZZ)

#ifndef __FUZZING_ECDSA_H__
#define __FUZZING_ECDSA_H__

ATTRIBUTE_WARN_UNUSED_RET int ecdsa_sign_raw(struct ec_sign_context *ctx, const u8 *input, u8 inputlen, u8 *sig, u8 siglen, const u8 *nonce, u8 noncelen);

ATTRIBUTE_WARN_UNUSED_RET int ecdsa_verify_raw(struct ec_verify_context *ctx, const u8 *input, u8 inputlen);

#endif /* __FUZZING_ECDSA_H__ */
#endif /* WITH_SIG_ECDSA && USE_CRYPTOFUZZ */
