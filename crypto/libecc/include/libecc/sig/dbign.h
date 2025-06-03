/*
 *  Copyright (C) 2022 - This file is part of libecc project
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
#ifdef WITH_SIG_DBIGN

#ifndef __DBIGN_H__
#define __DBIGN_H__

#include <libecc/sig/bign_common.h>

ATTRIBUTE_WARN_UNUSED_RET int dbign_init_pub_key(ec_pub_key *out_pub, const ec_priv_key *in_priv);

ATTRIBUTE_WARN_UNUSED_RET int dbign_siglen(u16 p_bit_len, u16 q_bit_len, u8 hsize, u8 blocksize, u8 *siglen);

ATTRIBUTE_WARN_UNUSED_RET int _dbign_sign_init(struct ec_sign_context *ctx);

ATTRIBUTE_WARN_UNUSED_RET int _dbign_sign_update(struct ec_sign_context *ctx,
		       const u8 *chunk, u32 chunklen);

ATTRIBUTE_WARN_UNUSED_RET int _dbign_sign_finalize(struct ec_sign_context *ctx, u8 *sig, u8 siglen);

ATTRIBUTE_WARN_UNUSED_RET int _dbign_verify_init(struct ec_verify_context *ctx,
		       const u8 *sig, u8 siglen);

ATTRIBUTE_WARN_UNUSED_RET int _dbign_verify_update(struct ec_verify_context *ctx,
			 const u8 *chunk, u32 chunklen);

ATTRIBUTE_WARN_UNUSED_RET int _dbign_verify_finalize(struct ec_verify_context *ctx);

#endif /* __DBIGN_H__ */
#endif /* WITH_SIG_DBIGN */
