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
#ifndef __KCDSA_H__
#define __KCDSA_H__

/*
 * NOTE: although we only need libarith for KCDSA as we
 * manipulate a ring of integers, we include libsig for
 * the hash algorithms.
 */
#include <libecc/lib_ecc_config.h>

/* The hash algorithms wrapper */
#include "../../hash/hash.h"

/* The DSA include file as we reuse DSA primitives internally */
#include "../dsa/dsa.h"

/* We define hereafter the types and functions for KCDSA.
 */

/* KCDSA public key, composed of:
 *       p        the KCDSA prime modulus
 *       q        the KCDSA prime order (prime divisor of (p-1))
 *       g        the KCDSA generator
 *       y        the public key = g^x (p)
 *
 * NOTE: the KCDSA (Schnorr DSA) public key is mapped to a DSA public key
 * as the parameters are the same.
 */
typedef dsa_pub_key kcdsa_pub_key;

/* KCDSA private key, composed of:
 *       p        the KCDSA prime modulus
 *       q        the KCDSA prime order (prime divisor of (p-1))
 *       g        the KCDSA generator
 *       x        the secret key
 *
 * NOTE: the KCDSA (Schnorr DSA) private key is mapped to a DSA private key
 * as the parameters are the same.
 */
typedef dsa_priv_key kcdsa_priv_key;

ATTRIBUTE_WARN_UNUSED_RET int kcdsa_import_priv_key(kcdsa_priv_key *priv, const u8 *p, u16 plen,
			                          const u8 *q, u16 qlen,
			                          const u8 *g, u16 glen,
			                          const u8 *x, u16 xlen);

ATTRIBUTE_WARN_UNUSED_RET int kcdsa_import_pub_key(kcdsa_pub_key *pub, const u8 *p, u16 plen,
			                         const u8 *q, u16 qlen,
			                         const u8 *g, u16 glen,
			                         const u8 *y, u16 ylen);

ATTRIBUTE_WARN_UNUSED_RET int kcdsa_compute_pub_from_priv(kcdsa_pub_key *pub,
							const kcdsa_priv_key *priv);

ATTRIBUTE_WARN_UNUSED_RET int kcdsa_sign(const kcdsa_priv_key *priv, const u8 *msg, u32 msglen,
			               const u8 *nonce, u16 noncelen,
			               u8 *sig, u16 siglen, gen_hash_alg_type kcdsa_hash);

ATTRIBUTE_WARN_UNUSED_RET int kcdsa_verify(const kcdsa_pub_key *pub, const u8 *msg, u32 msglen,
			                 const u8 *sig, u16 siglen, gen_hash_alg_type kcdsa_hash);

#endif /* __KCDSA_H__ */
