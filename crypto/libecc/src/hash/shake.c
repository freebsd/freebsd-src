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
#include <libecc/utils/utils.h>
#include <libecc/hash/shake.h>

/* Init function depending on the digest size */
int _shake_init(shake_context *ctx, u8 digest_size, u8 block_size)
{
	int ret;

	MUST_HAVE((ctx != NULL), ret, err);

        /* Zeroize the internal state */
        ret = local_memset(ctx->shake_state, 0, sizeof(ctx->shake_state)); EG(ret, err);

        ctx->shake_idx = 0;
        ctx->shake_digest_size = digest_size;
        ctx->shake_block_size = block_size;

	/* Detect endianness */
	ctx->shake_endian = arch_is_big_endian() ? SHAKE_BIG : SHAKE_LITTLE;

err:
        return ret;
}

/* Update hash function */
int _shake_update(shake_context *ctx, const u8 *input, u32 ilen)
{
        u32 i;
        u8 *state;
	int ret;

        MUST_HAVE((ctx != NULL) && ((input != NULL) || (ilen == 0)), ret, err);

        state = (u8*)(ctx->shake_state);

        for(i = 0; i < ilen; i++){
                /* Compute the index depending on the endianness */
		u64 idx = (ctx->shake_endian == SHAKE_LITTLE) ? ctx->shake_idx : SWAP64_Idx(ctx->shake_idx);
                ctx->shake_idx++;
                /* Update the state, and adapt endianness order */
                state[idx] ^= input[i];
                if(ctx->shake_idx == ctx->shake_block_size){
                        KECCAKF(ctx->shake_state);
                        ctx->shake_idx = 0;
                }
        }
	ret = 0;

err:
        return ret;
}

/* Finalize hash function */
int _shake_finalize(shake_context *ctx, u8 *output)
{
        unsigned int i;
        u8 *state;
	int ret;

        MUST_HAVE((ctx != NULL) && (output != NULL), ret, err);
        MUST_HAVE((ctx->shake_digest_size <= sizeof(ctx->shake_state)), ret, err);

        state = (u8*)(ctx->shake_state);

        /* Proceed with the padding of the last block */
        /* Compute the index depending on the endianness */
        if(ctx->shake_endian == SHAKE_LITTLE){
                /* Little endian case */
                state[ctx->shake_idx] ^= 0x1f;
                state[ctx->shake_block_size - 1] ^= 0x80;
        }
        else{
                /* Big endian case */
                state[SWAP64_Idx(ctx->shake_idx)] ^= 0x1f;
                state[SWAP64_Idx(ctx->shake_block_size - 1)] ^= 0x80;
        }
	/* Produce the output.
	 * NOTE: we should have a fixed version of SHAKE producing an output size
	 * with size less than the state size.
	 */
	KECCAKF(ctx->shake_state);
        for(i = 0; i < ctx->shake_digest_size; i++){
                output[i] = (ctx->shake_endian == SHAKE_LITTLE) ? state[i] : state[SWAP64_Idx(i)];
	}

        /* Uninit our context magic */
        ctx->magic = WORD(0);

        ret = 0;

err:
        return ret;
}
