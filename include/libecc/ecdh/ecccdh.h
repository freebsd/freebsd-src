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
#if defined(WITH_ECCCDH)


#ifndef __ECCCDH_H__
#define __ECCCDH_H__

#include <libecc/curves/curves.h>
#include <libecc/sig/ec_key.h>
#include <libecc/utils/utils.h>

/* Get the size of the shared secret associated to the curve parameters.
 *
 */
ATTRIBUTE_WARN_UNUSED_RET int ecccdh_shared_secret_size(const ec_params *params, u8 *size);

/* Get the size of the serialized public key associated to the curve parameters.
 *
 */
ATTRIBUTE_WARN_UNUSED_RET int ecccdh_serialized_pub_key_size(const ec_params *params, u8 *size);

/* Initialize ECCCDH public key from an initialized private key.
 *
 */
ATTRIBUTE_WARN_UNUSED_RET int ecccdh_init_pub_key(ec_pub_key *out_pub, const ec_priv_key *in_priv);

/* Generate a key pair for ECCCDH given curve parameters as input.
 *
 */
ATTRIBUTE_WARN_UNUSED_RET int ecccdh_gen_key_pair(ec_key_pair *kp, const ec_params *params);

/* Create a key pair from a serialized private key.
 *
 */
ATTRIBUTE_WARN_UNUSED_RET int ecccdh_import_key_pair_from_priv_key_buf(ec_key_pair *kp, const ec_params *params, const u8 *priv_key_buf, u8 priv_key_buf_len);

/* Serialize our public key in a buffer.
 *
 */
ATTRIBUTE_WARN_UNUSED_RET int ecccdh_serialize_pub_key(const ec_pub_key *our_pub_key, u8 *buff, u8 buff_len);

/* Derive the ECCCDH shared secret and store it in a buffer given the peer
 * public key and our private key.
 *
 * The shared_secret_len length MUST be exactly equal to the expected shared secret size:
 * the function fails otherwise.
 */
ATTRIBUTE_WARN_UNUSED_RET int ecccdh_derive_secret(const ec_priv_key *our_priv_key, const u8 *peer_pub_key, u8 peer_pub_key_len, u8 *shared_secret, u8 shared_secret_len);

#endif /* __ECCCDH_H__ */

#endif /* WITH_ECCCDH */
