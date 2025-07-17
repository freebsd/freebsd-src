/*
 * Copyright (c) 2018-2019 iXsystems Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/endian.h>
#include <opencrypto/cbc_mac.h>
#include <opencrypto/xform_auth.h>

/*
 * Given two CCM_CBC_BLOCK_LEN blocks, xor
 * them into dst, and then encrypt dst.
 */
static void
xor_and_encrypt(struct aes_cbc_mac_ctx *ctx,
		const uint8_t *src, uint8_t *dst)
{
#define	NWORDS	(CCM_CBC_BLOCK_LEN / sizeof(uint64_t))
	uint64_t b1[NWORDS], b2[NWORDS], temp[NWORDS];

	memcpy(b1, src, CCM_CBC_BLOCK_LEN);
	memcpy(b2, dst, CCM_CBC_BLOCK_LEN);

	for (size_t count = 0; count < NWORDS; count++)
		temp[count] = b1[count] ^ b2[count];
	rijndaelEncrypt(ctx->keysched, ctx->rounds, (void *)temp, dst);
#undef NWORDS
}

void
AES_CBC_MAC_Init(void *vctx)
{
	struct aes_cbc_mac_ctx *ctx;

	ctx = vctx;
	bzero(ctx, sizeof(*ctx));
}

void
AES_CBC_MAC_Setkey(void *vctx, const uint8_t *key, u_int klen)
{
	struct aes_cbc_mac_ctx *ctx;

	ctx = vctx;
	ctx->rounds = rijndaelKeySetupEnc(ctx->keysched, key, klen * 8);
}

/*
 * This is called to set the nonce, aka IV.
 *
 * Note that the caller is responsible for constructing b0 as well
 * as the length and padding around the AAD and passing that data
 * to _Update.
 */
void
AES_CBC_MAC_Reinit(void *vctx, const uint8_t *nonce, u_int nonceLen)
{
	struct aes_cbc_mac_ctx *ctx = vctx;

	ctx->nonce = nonce;
	ctx->nonceLength = nonceLen;

	ctx->blockIndex = 0;

	/* XOR b0 with all 0's on first call to _Update. */
	memset(ctx->block, 0, CCM_CBC_BLOCK_LEN);
}

int
AES_CBC_MAC_Update(void *vctx, const void *vdata, u_int length)
{
	struct aes_cbc_mac_ctx *ctx;
	const uint8_t *data;
	size_t copy_amt;
	
	ctx = vctx;
	data = vdata;

	/*
	 * _Update can be called with non-aligned update lengths.  Use
	 * the staging block when necessary.
	 */
	while (length != 0) {
		uint8_t *ptr;

		/*
		 * If there is no partial block and the length is at
		 * least a full block, encrypt the full block without
		 * copying to the staging block.
		 */
		if (ctx->blockIndex == 0 && length >= CCM_CBC_BLOCK_LEN) {
			xor_and_encrypt(ctx, data, ctx->block);
			length -= CCM_CBC_BLOCK_LEN;
			data += CCM_CBC_BLOCK_LEN;
			continue;
		}

		copy_amt = MIN(sizeof(ctx->staging_block) - ctx->blockIndex,
		    length);
		ptr = ctx->staging_block + ctx->blockIndex;
		bcopy(data, ptr, copy_amt);
		data += copy_amt;
		ctx->blockIndex += copy_amt;
		length -= copy_amt;
		if (ctx->blockIndex == sizeof(ctx->staging_block)) {
			/* We've got a full block */
			xor_and_encrypt(ctx, ctx->staging_block, ctx->block);
			ctx->blockIndex = 0;
		}
	}
	return (0);
}

void
AES_CBC_MAC_Final(uint8_t *buf, void *vctx)
{
	struct aes_cbc_mac_ctx *ctx;
	uint8_t s0[CCM_CBC_BLOCK_LEN];

	ctx = vctx;

	/*
	 * We first need to check to see if we've got any data
	 * left over to encrypt.
	 */
	if (ctx->blockIndex != 0) {
		memset(ctx->staging_block + ctx->blockIndex, 0,
		    CCM_CBC_BLOCK_LEN - ctx->blockIndex);
		xor_and_encrypt(ctx, ctx->staging_block, ctx->block);
	}
	explicit_bzero(ctx->staging_block, sizeof(ctx->staging_block));

	bzero(s0, sizeof(s0));
	s0[0] = (15 - ctx->nonceLength) - 1;
	bcopy(ctx->nonce, s0 + 1, ctx->nonceLength);
	rijndaelEncrypt(ctx->keysched, ctx->rounds, s0, s0);
	for (size_t indx = 0; indx < AES_CBC_MAC_HASH_LEN; indx++)
		buf[indx] = ctx->block[indx] ^ s0[indx];
	explicit_bzero(s0, sizeof(s0));
}
