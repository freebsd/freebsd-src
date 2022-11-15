/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 The FreeBSD Foundation
 *
 * This software was developed by Ararat River Consulting, LLC under
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/endian.h>
#include <crypto/chacha20_poly1305.h>

#include <sodium/crypto_aead_chacha20poly1305.h>
#include <sodium/crypto_aead_xchacha20poly1305.h>

/*
 * libsodium's chacha20poly1305 AEAD cipher does not construct the
 * Poly1305 digest in the same method as the IETF AEAD construct.
 * Specifically, libsodium does not pad the AAD and cipher text with
 * zeroes to a 16 byte boundary, and libsodium inserts the AAD and
 * cipher text lengths as inputs into the digest after each data
 * segment rather than appending both data lengths after the padded
 * cipher text.
 *
 * Instead, always use libsodium's chacha20poly1305 IETF AEAD cipher.
 * This cipher uses a 96-bit nonce with a 32-bit counter.  The data
 * encrypted here should never be large enough to overflow the counter
 * to the second word, so just pass zeros as the first word of the
 * nonce to mimic a 64-bit nonce and 64-bit counter.
 */
void
chacha20_poly1305_encrypt(uint8_t *dst, const uint8_t *src,
    const size_t src_len, const uint8_t *aad, const size_t aad_len,
    const uint8_t *nonce, const size_t nonce_len, const uint8_t *key)
{
	char local_nonce[12];

	MPASS(aad_len + src_len <=
	    crypto_aead_chacha20poly1305_ietf_MESSAGEBYTES_MAX);
	if (nonce_len == crypto_aead_chacha20poly1305_ietf_NPUBBYTES)
		memcpy(local_nonce, nonce, sizeof(local_nonce));
	else {
		memset(local_nonce, 0, 4);
		memcpy(local_nonce + 4, nonce, 8);
	}
	crypto_aead_chacha20poly1305_ietf_encrypt(dst, NULL, src, src_len,
	    aad, aad_len, NULL, local_nonce, key);
}

bool
chacha20_poly1305_decrypt(uint8_t *dst, const uint8_t *src,
    const size_t src_len, const uint8_t *aad, const size_t aad_len,
    const uint8_t *nonce, const size_t nonce_len, const uint8_t *key)
{
	char local_nonce[12];
	int ret;

	MPASS(aad_len + src_len <=
	    crypto_aead_chacha20poly1305_ietf_MESSAGEBYTES_MAX);
	if (nonce_len == crypto_aead_chacha20poly1305_ietf_NPUBBYTES)
		memcpy(local_nonce, nonce, sizeof(local_nonce));
	else {
		memset(local_nonce, 0, 4);
		memcpy(local_nonce + 4, nonce, 8);
	}
	ret = crypto_aead_chacha20poly1305_ietf_decrypt(dst, NULL, NULL,
	    src, src_len, aad, aad_len, local_nonce, key);
	return (ret == 0);
}

void
xchacha20_poly1305_encrypt(uint8_t *dst, const uint8_t *src,
    const size_t src_len, const uint8_t *aad, const size_t aad_len,
    const uint8_t *nonce, const uint8_t *key)
{
	crypto_aead_xchacha20poly1305_ietf_encrypt(dst, NULL, src, src_len,
	    aad, aad_len, NULL, nonce, key);
}

bool
xchacha20_poly1305_decrypt(uint8_t *dst, const uint8_t *src,
    const size_t src_len, const uint8_t *aad, const size_t aad_len,
    const uint8_t *nonce, const uint8_t *key)
{
	return (crypto_aead_xchacha20poly1305_ietf_decrypt(dst, NULL, NULL,
	    src, src_len, aad, aad_len, nonce, key) == 0);
}
