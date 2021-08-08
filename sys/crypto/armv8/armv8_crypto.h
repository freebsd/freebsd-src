/*-
 * Copyright (c) 2016 The FreeBSD Foundation
 *
 * This software was developed by Andrew Turner under
 * sponsorship from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _ARMV8_CRYPTO_H_
#define _ARMV8_CRYPTO_H_

#define	AES256_ROUNDS	14
#define	AES_SCHED_LEN	((AES256_ROUNDS + 1) * AES_BLOCK_LEN)

typedef struct {
	uint32_t		aes_key[AES_SCHED_LEN/4];
	int			aes_rounds;
} AES_key_t;

typedef union {
		uint64_t u[2];
		uint32_t d[4];
		uint8_t c[16];
		size_t t[16 / sizeof(size_t)];
} __uint128_val_t;

struct armv8_crypto_session {
	AES_key_t enc_schedule;
	AES_key_t dec_schedule;
	AES_key_t xts_schedule;
	__uint128_val_t Htable[16];
};

/* Prototypes for aesv8-armx.S */
void aes_v8_encrypt(uint8_t *in, uint8_t *out, const AES_key_t *key);
int aes_v8_set_encrypt_key(const unsigned char *userKey, const int bits, const AES_key_t *key);
int aes_v8_set_decrypt_key(const unsigned char *userKey, const int bits, const AES_key_t *key);

/* Prototypes for ghashv8-armx.S */
void gcm_init_v8(__uint128_val_t Htable[16], const uint64_t Xi[2]);
void gcm_gmult_v8(uint64_t Xi[2], const __uint128_val_t Htable[16]);
void gcm_ghash_v8(uint64_t Xi[2], const __uint128_val_t Htable[16], const uint8_t *inp, size_t len);

void armv8_aes_encrypt_cbc(const AES_key_t *, size_t, const uint8_t *,
    uint8_t *, const uint8_t[static AES_BLOCK_LEN]);
void armv8_aes_decrypt_cbc(const AES_key_t *, size_t, uint8_t *,
    const uint8_t[static AES_BLOCK_LEN]);
void armv8_aes_encrypt_gcm(AES_key_t *, size_t, const uint8_t *,
    uint8_t *, size_t, const uint8_t*,
    uint8_t tag[static GMAC_DIGEST_LEN],
    const uint8_t[static AES_BLOCK_LEN],
    const __uint128_val_t *);
int armv8_aes_decrypt_gcm(AES_key_t *, size_t, const uint8_t *,
    uint8_t *, size_t, const uint8_t*,
    const uint8_t tag[static GMAC_DIGEST_LEN],
    const uint8_t[static AES_BLOCK_LEN],
    const __uint128_val_t *);

void armv8_aes_encrypt_xts(AES_key_t *, const void *, size_t,
    const uint8_t *, uint8_t *, const uint8_t[AES_BLOCK_LEN]);
void armv8_aes_decrypt_xts(AES_key_t *, const void *, size_t,
    const uint8_t *, uint8_t *, const uint8_t[AES_BLOCK_LEN]);

#endif /* _ARMV8_CRYPTO_H_ */
