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
#ifndef __DSA_H__
#define __DSA_H__

/*
 * NOTE: although we only need libarith for DSA as we
 * manipulate a ring of integers, we include libsig for
 * the hash algorithms.
 */
#include <libecc/lib_ecc_config.h>

/* The hash algorithms wrapper */
#include "../../hash/hash.h"

/* We define hereafter the types and functions for DSA.
 * The notations are taken from NIST FIPS 186-4 and should be
 * compliant with it.
 */

/* DSA public key, composed of:
 *       p        the DSA prime modulus
 *       q        the DSA prime order (prime divisor of (p-1))
 *       g        the DSA generator
 *       y        the public key = g^x (p)
 */
typedef struct {
	nn p;
	nn q;
	nn g;
	nn y;
} dsa_pub_key;

/* DSA private key, composed of:
 *       p        the DSA prime modulus
 *       q        the DSA prime order (prime divisor of (p-1))
 *       g        the DSA generator
 *       x        the secret key
 */
typedef struct {
	nn p;
	nn q;
	nn g;
	nn x;
} dsa_priv_key;

ATTRIBUTE_WARN_UNUSED_RET int dsa_import_priv_key(dsa_priv_key *priv, const u8 *p, u16 plen,
			                          const u8 *q, u16 qlen,
			                          const u8 *g, u16 glen,
			                          const u8 *x, u16 xlen);

ATTRIBUTE_WARN_UNUSED_RET int dsa_import_pub_key(dsa_pub_key *pub, const u8 *p, u16 plen,
			                         const u8 *q, u16 qlen,
			                         const u8 *g, u16 glen,
			                         const u8 *y, u16 ylen);

ATTRIBUTE_WARN_UNUSED_RET int dsa_compute_pub_from_priv(dsa_pub_key *pub,
							const dsa_priv_key *priv);

ATTRIBUTE_WARN_UNUSED_RET int dsa_sign(const dsa_priv_key *priv, const u8 *msg, u32 msglen,
			               const u8 *nonce, u16 noncelen,
			               u8 *sig, u16 siglen, gen_hash_alg_type dsa_hash);

ATTRIBUTE_WARN_UNUSED_RET int dsa_verify(const dsa_pub_key *pub, const u8 *msg, u32 msglen,
			                 const u8 *sig, u16 siglen, gen_hash_alg_type dsa_hash);

#endif /* __DSA_H__ */
