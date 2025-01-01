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
#ifndef __GOSTR34_10_94_H__
#define __GOSTR34_10_94_H__

/*
 * NOTE: although we only need libarith for GOSTR34_10_94 as we
 * manipulate a ring of integers, we include libsig for
 * the hash algorithms.
 */
#include <libecc/lib_ecc_config.h>

/* The hash algorithms wrapper */
#include "../../hash/hash.h"

/* The DSA include file as we reuse DSA primitives internally */
#include "../dsa/dsa.h"

/* We define hereafter the types and functions for GOSTR34_10_94.
 */

/* GOSTR34_10_94 public key, composed of:
 *       p        the GOSTR34_10_94 prime modulus
 *       q        the GOSTR34_10_94 prime order (prime divisor of (p-1))
 *       g        the GOSTR34_10_94 generator
 *       y        the public key = g^x (p)
 *
 * NOTE: the GOST public key is mapped to a DSA public key
 * as the parameters are the same.
 */
typedef dsa_pub_key gostr34_10_94_pub_key;

/* GOSTR34_10_94 private key, composed of:
 *       p        the GOSTR34_10_94 prime modulus
 *       q        the GOSTR34_10_94 prime order (prime divisor of (p-1))
 *       g        the GOSTR34_10_94 generator
 *       x        the secret key
 *
 * NOTE: the GOST private key is mapped to a DSA private key
 * as the parameters are the same.
 */
typedef dsa_priv_key gostr34_10_94_priv_key;

ATTRIBUTE_WARN_UNUSED_RET int gostr34_10_94_import_priv_key(gostr34_10_94_priv_key *priv, const u8 *p, u16 plen,
			                          const u8 *q, u16 qlen,
			                          const u8 *g, u16 glen,
			                          const u8 *x, u16 xlen);

ATTRIBUTE_WARN_UNUSED_RET int gostr34_10_94_import_pub_key(gostr34_10_94_pub_key *pub, const u8 *p, u16 plen,
			                         const u8 *q, u16 qlen,
			                         const u8 *g, u16 glen,
			                         const u8 *y, u16 ylen);

ATTRIBUTE_WARN_UNUSED_RET int gostr34_10_94_compute_pub_from_priv(gostr34_10_94_pub_key *pub,
							const gostr34_10_94_priv_key *priv);

ATTRIBUTE_WARN_UNUSED_RET int gostr34_10_94_sign(const gostr34_10_94_priv_key *priv, const u8 *msg, u32 msglen,
			               const u8 *nonce, u16 noncelen,
			               u8 *sig, u16 siglen, gen_hash_alg_type gostr34_10_94_hash);

ATTRIBUTE_WARN_UNUSED_RET int gostr34_10_94_verify(const gostr34_10_94_pub_key *pub, const u8 *msg, u32 msglen,
			                 const u8 *sig, u16 siglen, gen_hash_alg_type gostr34_10_94_hash);

#endif /* __GOSTR34_10_94_H__ */
