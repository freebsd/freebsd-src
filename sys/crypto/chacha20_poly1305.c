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

void
chacha20_poly1305_encrypt(uint8_t *dst, const uint8_t *src,
    const size_t src_len, const uint8_t *aad, const size_t aad_len,
    const uint8_t *nonce, const size_t nonce_len, const uint8_t *key)
{
	if (nonce_len == crypto_aead_chacha20poly1305_ietf_NPUBBYTES)
		crypto_aead_chacha20poly1305_ietf_encrypt(dst, NULL, src,
		    src_len, aad, aad_len, NULL, nonce, key);
	else
		crypto_aead_chacha20poly1305_encrypt(dst, NULL, src,
		    src_len, aad, aad_len, NULL, nonce, key);
}

bool
chacha20_poly1305_decrypt(uint8_t *dst, const uint8_t *src,
    const size_t src_len, const uint8_t *aad, const size_t aad_len,
    const uint8_t *nonce, const size_t nonce_len, const uint8_t *key)
{
	int ret;

	if (nonce_len == crypto_aead_chacha20poly1305_ietf_NPUBBYTES)
		ret = crypto_aead_chacha20poly1305_ietf_decrypt(dst, NULL,
		    NULL, src, src_len, aad, aad_len, nonce, key);
	else
		ret = crypto_aead_chacha20poly1305_decrypt(dst, NULL,
		    NULL, src, src_len, aad, aad_len, nonce, key);
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
