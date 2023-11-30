/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2023 Stormshield
 * Copyright (c) 2023 Semihalf
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#ifndef __OSSL_ARM__
#define __OSSL_ARM__

#include <crypto/openssl/ossl.h>
#include <crypto/openssl/ossl_cipher.h>

#include <opencrypto/cryptodev.h>

struct bsaes_key {
	struct ossl_aes_keysched ks;
	int converted;
#define	BSAES_KEY_SIZE	(128 * (RIJNDAEL_MAXNR - 1) + 2 * AES_BLOCK_LEN)
	uint8_t bitslice[BSAES_KEY_SIZE] __aligned(8);
} __aligned(8);

ossl_cipher_encrypt_t ossl_bsaes_cbc_encrypt;

void AES_encrypt(const void *, void *, const void *);

static inline void
AES_CBC_ENCRYPT(const unsigned char *in, unsigned char *out,
    size_t length, const void *key, unsigned char *iv, int encrypt)
{
	struct bsaes_key bsks;
	uint32_t iv32[4], scratch[4];

	/*
	 * bsaes_cbc_encrypt has some special requirements w.r.t input data.
	 * The key buffer, that normally holds round keys is used as a scratch
	 * space. 128 bytes per round of extra space is required.
	 * Another thing is that only decryption is supported.
	 * In the case of encryption block chaining has to be done in C.
	 */
	if (!encrypt) {
		memcpy(&bsks.ks, key, sizeof(bsks.ks));
		bsks.converted = 0;
		ossl_bsaes_cbc_encrypt(in, out, length, &bsks, iv, false);
		return;
	}

	length /= AES_BLOCK_LEN;
	memcpy(iv32, iv, AES_BLOCK_LEN);

	while (length-- > 0) {
		memcpy(scratch, in, AES_BLOCK_LEN);

		/* XOR plaintext with IV. */
		scratch[0] ^= iv32[0];
		scratch[1] ^= iv32[1];
		scratch[2] ^= iv32[2];
		scratch[3] ^= iv32[3];

		AES_encrypt(scratch, out, key);

		memcpy(iv32, out, AES_BLOCK_LEN);
		in += AES_BLOCK_LEN;
		out += AES_BLOCK_LEN;
	}

	memcpy(iv, iv32, AES_BLOCK_LEN);
}

#endif /* __OSSL_ARM__ */
