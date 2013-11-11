/* $OpenBSD: cipher-3des1.c,v 1.9 2013/11/08 00:39:15 djm Exp $ */
/*
 * Copyright (c) 2003 Markus Friedl.  All rights reserved.
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

#include "includes.h"

#include <sys/types.h>

#include <openssl/evp.h>

#include <stdarg.h>
#include <string.h>

#include "xmalloc.h"
#include "log.h"

#include "openbsd-compat/openssl-compat.h"

/*
 * This is used by SSH1:
 *
 * What kind of triple DES are these 2 routines?
 *
 * Why is there a redundant initialization vector?
 *
 * If only iv3 was used, then, this would till effect have been
 * outer-cbc. However, there is also a private iv1 == iv2 which
 * perhaps makes differential analysis easier. On the other hand, the
 * private iv1 probably makes the CRC-32 attack ineffective. This is a
 * result of that there is no longer any known iv1 to use when
 * choosing the X block.
 */
struct ssh1_3des_ctx
{
	EVP_CIPHER_CTX	k1, k2, k3;
};

const EVP_CIPHER * evp_ssh1_3des(void);
void ssh1_3des_iv(EVP_CIPHER_CTX *, int, u_char *, int);

static int
ssh1_3des_init(EVP_CIPHER_CTX *ctx, const u_char *key, const u_char *iv,
    int enc)
{
	struct ssh1_3des_ctx *c;
	u_char *k1, *k2, *k3;

	if ((c = EVP_CIPHER_CTX_get_app_data(ctx)) == NULL) {
		c = xcalloc(1, sizeof(*c));
		EVP_CIPHER_CTX_set_app_data(ctx, c);
	}
	if (key == NULL)
		return (1);
	if (enc == -1)
		enc = ctx->encrypt;
	k1 = k2 = k3 = (u_char *) key;
	k2 += 8;
	if (EVP_CIPHER_CTX_key_length(ctx) >= 16+8) {
		if (enc)
			k3 += 16;
		else
			k1 += 16;
	}
	EVP_CIPHER_CTX_init(&c->k1);
	EVP_CIPHER_CTX_init(&c->k2);
	EVP_CIPHER_CTX_init(&c->k3);
#ifdef SSH_OLD_EVP
	EVP_CipherInit(&c->k1, EVP_des_cbc(), k1, NULL, enc);
	EVP_CipherInit(&c->k2, EVP_des_cbc(), k2, NULL, !enc);
	EVP_CipherInit(&c->k3, EVP_des_cbc(), k3, NULL, enc);
#else
	if (EVP_CipherInit(&c->k1, EVP_des_cbc(), k1, NULL, enc) == 0 ||
	    EVP_CipherInit(&c->k2, EVP_des_cbc(), k2, NULL, !enc) == 0 ||
	    EVP_CipherInit(&c->k3, EVP_des_cbc(), k3, NULL, enc) == 0) {
		memset(c, 0, sizeof(*c));
		free(c);
		EVP_CIPHER_CTX_set_app_data(ctx, NULL);
		return (0);
	}
#endif
	return (1);
}

static int
ssh1_3des_cbc(EVP_CIPHER_CTX *ctx, u_char *dest, const u_char *src,
    LIBCRYPTO_EVP_INL_TYPE len)
{
	struct ssh1_3des_ctx *c;

	if ((c = EVP_CIPHER_CTX_get_app_data(ctx)) == NULL) {
		error("ssh1_3des_cbc: no context");
		return (0);
	}
#ifdef SSH_OLD_EVP
	EVP_Cipher(&c->k1, dest, (u_char *)src, len);
	EVP_Cipher(&c->k2, dest, dest, len);
	EVP_Cipher(&c->k3, dest, dest, len);
#else
	if (EVP_Cipher(&c->k1, dest, (u_char *)src, len) == 0 ||
	    EVP_Cipher(&c->k2, dest, dest, len) == 0 ||
	    EVP_Cipher(&c->k3, dest, dest, len) == 0)
		return (0);
#endif
	return (1);
}

static int
ssh1_3des_cleanup(EVP_CIPHER_CTX *ctx)
{
	struct ssh1_3des_ctx *c;

	if ((c = EVP_CIPHER_CTX_get_app_data(ctx)) != NULL) {
		EVP_CIPHER_CTX_cleanup(&c->k1);
		EVP_CIPHER_CTX_cleanup(&c->k2);
		EVP_CIPHER_CTX_cleanup(&c->k3);
		memset(c, 0, sizeof(*c));
		free(c);
		EVP_CIPHER_CTX_set_app_data(ctx, NULL);
	}
	return (1);
}

void
ssh1_3des_iv(EVP_CIPHER_CTX *evp, int doset, u_char *iv, int len)
{
	struct ssh1_3des_ctx *c;

	if (len != 24)
		fatal("%s: bad 3des iv length: %d", __func__, len);
	if ((c = EVP_CIPHER_CTX_get_app_data(evp)) == NULL)
		fatal("%s: no 3des context", __func__);
	if (doset) {
		debug3("%s: Installed 3DES IV", __func__);
		memcpy(c->k1.iv, iv, 8);
		memcpy(c->k2.iv, iv + 8, 8);
		memcpy(c->k3.iv, iv + 16, 8);
	} else {
		debug3("%s: Copying 3DES IV", __func__);
		memcpy(iv, c->k1.iv, 8);
		memcpy(iv + 8, c->k2.iv, 8);
		memcpy(iv + 16, c->k3.iv, 8);
	}
}

const EVP_CIPHER *
evp_ssh1_3des(void)
{
	static EVP_CIPHER ssh1_3des;

	memset(&ssh1_3des, 0, sizeof(EVP_CIPHER));
	ssh1_3des.nid = NID_undef;
	ssh1_3des.block_size = 8;
	ssh1_3des.iv_len = 0;
	ssh1_3des.key_len = 16;
	ssh1_3des.init = ssh1_3des_init;
	ssh1_3des.cleanup = ssh1_3des_cleanup;
	ssh1_3des.do_cipher = ssh1_3des_cbc;
#ifndef SSH_OLD_EVP
	ssh1_3des.flags = EVP_CIPH_CBC_MODE | EVP_CIPH_VARIABLE_LENGTH;
#endif
	return (&ssh1_3des);
}
