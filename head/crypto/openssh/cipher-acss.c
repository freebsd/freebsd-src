/*
 * Copyright (c) 2004 The OpenBSD project
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"

#include <openssl/evp.h>

#include <string.h>

#if !defined(EVP_CTRL_SET_ACSS_MODE) && (OPENSSL_VERSION_NUMBER >= 0x00907000L)

#include "acss.h"
#include "openbsd-compat/openssl-compat.h"

#define data(ctx) ((EVP_ACSS_KEY *)(ctx)->cipher_data)

typedef struct {
	ACSS_KEY ks;
} EVP_ACSS_KEY;

#define EVP_CTRL_SET_ACSS_MODE          0xff06
#define EVP_CTRL_SET_ACSS_SUBKEY        0xff07

static int
acss_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
    const unsigned char *iv, int enc)
{
	acss_setkey(&data(ctx)->ks,key,enc,ACSS_DATA);
	return 1;
}

static int
acss_ciph(EVP_CIPHER_CTX *ctx, unsigned char *out, const unsigned char *in,
    LIBCRYPTO_EVP_INL_TYPE inl)
{
	acss(&data(ctx)->ks,inl,in,out);
	return 1;
}

static int
acss_ctrl(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr)
{
	switch(type) {
	case EVP_CTRL_SET_ACSS_MODE:
		data(ctx)->ks.mode = arg;
		return 1;
	case EVP_CTRL_SET_ACSS_SUBKEY:
		acss_setsubkey(&data(ctx)->ks,(unsigned char *)ptr);
		return 1;
	default:
		return -1;
	}
}

const EVP_CIPHER *
evp_acss(void)
{
	static EVP_CIPHER acss_cipher;

	memset(&acss_cipher, 0, sizeof(EVP_CIPHER));

	acss_cipher.nid = NID_undef;
	acss_cipher.block_size = 1;
	acss_cipher.key_len = 5;
	acss_cipher.init = acss_init_key;
	acss_cipher.do_cipher = acss_ciph;
	acss_cipher.ctx_size = sizeof(EVP_ACSS_KEY);
	acss_cipher.ctrl = acss_ctrl;

	return (&acss_cipher);
}
#endif

