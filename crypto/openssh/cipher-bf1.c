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
RCSID("$OpenBSD: cipher-bf1.c,v 1.1 2003/05/15 03:08:29 markus Exp $");

#include <openssl/evp.h>
#include "xmalloc.h"
#include "log.h"

#if OPENSSL_VERSION_NUMBER < 0x00906000L
#define SSH_OLD_EVP
#endif

/*
 * SSH1 uses a variation on Blowfish, all bytes must be swapped before
 * and after encryption/decryption. Thus the swap_bytes stuff (yuk).
 */

const EVP_CIPHER * evp_ssh1_bf(void);

static void
swap_bytes(const u_char *src, u_char *dst, int n)
{
	u_char c[4];

	/* Process 4 bytes every lap. */
	for (n = n / 4; n > 0; n--) {
		c[3] = *src++;
		c[2] = *src++;
		c[1] = *src++;
		c[0] = *src++;

		*dst++ = c[0];
		*dst++ = c[1];
		*dst++ = c[2];
		*dst++ = c[3];
	}
}

#ifdef SSH_OLD_EVP
static void bf_ssh1_init (EVP_CIPHER_CTX * ctx, const unsigned char *key,
			  const unsigned char *iv, int enc)
{
	if (iv != NULL)
		memcpy (&(ctx->oiv[0]), iv, 8);
	memcpy (&(ctx->iv[0]), &(ctx->oiv[0]), 8);
	if (key != NULL)
		BF_set_key (&(ctx->c.bf_ks), EVP_CIPHER_CTX_key_length (ctx),
			    key);
}
#endif

static int (*orig_bf)(EVP_CIPHER_CTX *, u_char *, const u_char *, u_int) = NULL;

static int
bf_ssh1_cipher(EVP_CIPHER_CTX *ctx, u_char *out, const u_char *in, u_int len)
{
	int ret;

	swap_bytes(in, out, len);
	ret = (*orig_bf)(ctx, out, out, len);
	swap_bytes(out, out, len);
	return (ret);
}

const EVP_CIPHER *
evp_ssh1_bf(void)
{
	static EVP_CIPHER ssh1_bf;

	memcpy(&ssh1_bf, EVP_bf_cbc(), sizeof(EVP_CIPHER));
	orig_bf = ssh1_bf.do_cipher;
	ssh1_bf.nid = NID_undef;
#ifdef SSH_OLD_EVP
	ssh1_bf.init = bf_ssh1_init;
#endif
	ssh1_bf.do_cipher = bf_ssh1_cipher;
	ssh1_bf.key_len = 32;
	return (&ssh1_bf);
}
