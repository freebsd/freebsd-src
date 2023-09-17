/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Stormshield.
 * Copyright (c) 2021 Semihalf.
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

#ifndef __OSSL_CIPHER_H__
#define __OSSL_CIPHER_H__

struct ossl_session_cipher;
struct cryptop;
struct crypto_session_params;

typedef int (ossl_cipher_setkey_t)(const unsigned char*, int, void*);
typedef int (ossl_cipher_process_t)(struct ossl_session_cipher*, struct cryptop*,
    const struct crypto_session_params*);
typedef void (ossl_cipher_encrypt_t)(const unsigned char*, unsigned char*, size_t,
    const void*, unsigned char*, int);

ossl_cipher_encrypt_t ossl_aes_cbc_encrypt;

struct ossl_cipher {
	int			type;
	uint16_t		blocksize;
	uint16_t		ivsize;

	ossl_cipher_setkey_t	*set_encrypt_key;
	ossl_cipher_setkey_t	*set_decrypt_key;
	ossl_cipher_process_t	*process;
};

#endif
