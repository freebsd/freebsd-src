/*
 * Copyright (c) 2003 Markus Friedl <markus@openbsd.org>
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
RCSID("$OpenBSD: cipher-ctr.c,v 1.4 2004/02/06 23:41:13 dtucker Exp $");

#include <openssl/evp.h>

#include "log.h"
#include "xmalloc.h"

#if OPENSSL_VERSION_NUMBER < 0x00906000L
#define SSH_OLD_EVP
#endif

#if OPENSSL_VERSION_NUMBER < 0x00907000L
#include "rijndael.h"
#define AES_KEY rijndael_ctx
#define AES_BLOCK_SIZE 16
#define AES_encrypt(a, b, c) rijndael_encrypt(c, a, b)
#define AES_set_encrypt_key(a, b, c) rijndael_set_key(c, (char *)a, b, 1)
#else
#include <openssl/aes.h>
#endif

const EVP_CIPHER *evp_aes_128_ctr(void);
void ssh_aes_ctr_iv(EVP_CIPHER_CTX *, int, u_char *, u_int);

struct ssh_aes_ctr_ctx
{
	AES_KEY		aes_ctx;
	u_char		aes_counter[AES_BLOCK_SIZE];
};

/*
 * increment counter 'ctr',
 * the counter is of size 'len' bytes and stored in network-byte-order.
 * (LSB at ctr[len-1], MSB at ctr[0])
 */
static void
ssh_ctr_inc(u_char *ctr, u_int len)
{
	int i;

	for (i = len - 1; i >= 0; i--)
		if (++ctr[i])	/* continue on overflow */
			return;
}

static int
ssh_aes_ctr(EVP_CIPHER_CTX *ctx, u_char *dest, const u_char *src,
    u_int len)
{
	struct ssh_aes_ctr_ctx *c;
	u_int n = 0;
	u_char buf[AES_BLOCK_SIZE];

	if (len == 0)
		return (1);
	if ((c = EVP_CIPHER_CTX_get_app_data(ctx)) == NULL)
		return (0);

	while ((len--) > 0) {
		if (n == 0) {
			AES_encrypt(c->aes_counter, buf, &c->aes_ctx);
			ssh_ctr_inc(c->aes_counter, AES_BLOCK_SIZE);
		}
		*(dest++) = *(src++) ^ buf[n];
		n = (n + 1) % AES_BLOCK_SIZE;
	}
	return (1);
}

static int
ssh_aes_ctr_init(EVP_CIPHER_CTX *ctx, const u_char *key, const u_char *iv,
    int enc)
{
	struct ssh_aes_ctr_ctx *c;

	if ((c = EVP_CIPHER_CTX_get_app_data(ctx)) == NULL) {
		c = xmalloc(sizeof(*c));
		EVP_CIPHER_CTX_set_app_data(ctx, c);
	}
	if (key != NULL)
		AES_set_encrypt_key(key, EVP_CIPHER_CTX_key_length(ctx) * 8,
		     &c->aes_ctx);
	if (iv != NULL)
		memcpy(c->aes_counter, iv, AES_BLOCK_SIZE);
	return (1);
}

static int
ssh_aes_ctr_cleanup(EVP_CIPHER_CTX *ctx)
{
	struct ssh_aes_ctr_ctx *c;

	if ((c = EVP_CIPHER_CTX_get_app_data(ctx)) != NULL) {
		memset(c, 0, sizeof(*c));
		xfree(c);
		EVP_CIPHER_CTX_set_app_data(ctx, NULL);
	}
	return (1);
}

void
ssh_aes_ctr_iv(EVP_CIPHER_CTX *evp, int doset, u_char * iv, u_int len)
{
	struct ssh_aes_ctr_ctx *c;

	if ((c = EVP_CIPHER_CTX_get_app_data(evp)) == NULL)
		fatal("ssh_aes_ctr_iv: no context");
	if (doset)
		memcpy(c->aes_counter, iv, len);
	else
		memcpy(iv, c->aes_counter, len);
}

const EVP_CIPHER *
evp_aes_128_ctr(void)
{
	static EVP_CIPHER aes_ctr;

	memset(&aes_ctr, 0, sizeof(EVP_CIPHER));
	aes_ctr.nid = NID_undef;
	aes_ctr.block_size = AES_BLOCK_SIZE;
	aes_ctr.iv_len = AES_BLOCK_SIZE;
	aes_ctr.key_len = 16;
	aes_ctr.init = ssh_aes_ctr_init;
	aes_ctr.cleanup = ssh_aes_ctr_cleanup;
	aes_ctr.do_cipher = ssh_aes_ctr;
#ifndef SSH_OLD_EVP
	aes_ctr.flags = EVP_CIPH_CBC_MODE | EVP_CIPH_VARIABLE_LENGTH |
	    EVP_CIPH_ALWAYS_CALL_INIT | EVP_CIPH_CUSTOM_IV;
#endif
	return (&aes_ctr);
}
