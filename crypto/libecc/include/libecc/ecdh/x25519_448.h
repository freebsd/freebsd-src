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
#include <libecc/lib_ecc_config.h>
#include <libecc/lib_ecc_types.h>

#if defined(WITH_X25519) || defined(WITH_X448)

#ifndef __X25519_448_H__
#define __X25519_448_H__

#include <libecc/words/words.h>

/* Size of X25519 values */
#define X25519_SIZE 32
/* Size of X448 values   */
#define X448_SIZE   56

#if defined(WITH_X25519)
/* The X25519 function as specified in RFC7748.
 *
 * NOTE: the user of this primitive should be warned and aware that is is not fully compliant with the
 * RFC7748 description as u coordinates on the quadratic twist of the curve are rejected as well as non
 * canonical u.
 * See the explanations in the implementation of the function for more context and explanations.
 */
ATTRIBUTE_WARN_UNUSED_RET int x25519(const u8 k[X25519_SIZE], const u8 u[X25519_SIZE], u8 res[X25519_SIZE]);

ATTRIBUTE_WARN_UNUSED_RET int x25519_gen_priv_key(u8 priv_key[X25519_SIZE]);

ATTRIBUTE_WARN_UNUSED_RET int x25519_init_pub_key(const u8 priv_key[X25519_SIZE], u8 pub_key[X25519_SIZE]);

ATTRIBUTE_WARN_UNUSED_RET int x25519_derive_secret(const u8 priv_key[X25519_SIZE], const u8 peer_pub_key[X25519_SIZE], u8 shared_secret[X25519_SIZE]);
#endif

#if defined(WITH_X448)
/* The X448 function as specified in RFC7748.
 *
 * NOTE: the user of this primitive should be warned and aware that is is not fully compliant with the
 * RFC7748 description as u coordinates on the quadratic twist of the curve are rejected as well as non
 * canonical u.
 * See the explanations in the implementation of the function for more context and explanations.
 */
ATTRIBUTE_WARN_UNUSED_RET int x448(const u8 k[X448_SIZE], const u8 u[X448_SIZE], u8 res[X448_SIZE]);

ATTRIBUTE_WARN_UNUSED_RET int x448_gen_priv_key(u8 priv_key[X448_SIZE]);

ATTRIBUTE_WARN_UNUSED_RET int x448_init_pub_key(const u8 priv_key[X448_SIZE], u8 pub_key[X448_SIZE]);

ATTRIBUTE_WARN_UNUSED_RET int x448_derive_secret(const u8 priv_key[X448_SIZE], const u8 peer_pub_key[X448_SIZE], u8 shared_secret[X448_SIZE]);
#endif

#endif /* __X25519_448_H__ */

#endif /* defined(WITH_X25519) || defined(WITH_X448) */
