/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2023 Stormshield
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

#ifndef _OSSL_AES_GCM_H_
#define	_OSSL_AES_GCM_H_

#include <crypto/openssl/ossl_cipher.h>

struct ossl_gcm_context;

struct ossl_aes_gcm_ops {
	void (*init)(struct ossl_gcm_context *ctx, const void *key,
	    size_t keylen);
	void (*setiv)(struct ossl_gcm_context *ctx, const unsigned char *iv,
	    size_t ivlen);
	int (*aad)(struct ossl_gcm_context *ctx, const unsigned char *aad,
	    size_t len);
	int (*encrypt)(struct ossl_gcm_context *ctx, const unsigned char *in,
	    unsigned char *out, size_t len);
	int (*decrypt)(struct ossl_gcm_context *ctx, const unsigned char *in,
	    unsigned char *out, size_t len);
	int (*finish)(struct ossl_gcm_context *ctx, const unsigned char *tag,
	    size_t len);
	void (*tag)(struct ossl_gcm_context *ctx, unsigned char *tag,
	    size_t len);
};

#ifndef __SIZEOF_INT128__
typedef	struct { uint64_t v[2]; } __uint128_t;
#endif

struct ossl_gcm_context {
	struct {
		union {
			uint64_t u[2];
			uint32_t d[4];
			uint8_t c[16];
		} Yi, EKi, EK0, len, Xi, H;
		__uint128_t Htable[16];
		unsigned int mres, ares;
	} gcm;

	struct ossl_aes_keysched aes_ks;

	const struct ossl_aes_gcm_ops *ops;
};

#endif /* !_OSSL_AES_GCM_H_ */
