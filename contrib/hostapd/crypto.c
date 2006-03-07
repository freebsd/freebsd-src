/*
 * WPA Supplicant / wrapper functions for libcrypto
 * Copyright (c) 2004-2005, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include <string.h>
#include <sys/types.h>

#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/des.h>
#include <openssl/aes.h>

#include "common.h"
#include "crypto.h"

#if OPENSSL_VERSION_NUMBER < 0x00907000
#define DES_key_schedule des_key_schedule
#define DES_cblock des_cblock
#define DES_set_key(key, schedule) des_set_key((key), *(schedule))
#define DES_ecb_encrypt(input, output, ks, enc) \
	des_ecb_encrypt((input), (output), *(ks), (enc))
#endif /* openssl < 0.9.7 */


void md4_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac)
{
	MD4_CTX ctx;
	int i;

	MD4_Init(&ctx);
	for (i = 0; i < num_elem; i++)
		MD4_Update(&ctx, addr[i], len[i]);
	MD4_Final(mac, &ctx);
}


void des_encrypt(const u8 *clear, const u8 *key, u8 *cypher)
{
	u8 pkey[8], next, tmp;
	int i;
	DES_key_schedule ks;

	/* Add parity bits to the key */
	next = 0;
	for (i = 0; i < 7; i++) {
		tmp = key[i];
		pkey[i] = (tmp >> i) | next | 1;
		next = tmp << (7 - i);
	}
	pkey[i] = next | 1;

	DES_set_key(&pkey, &ks);
	DES_ecb_encrypt((DES_cblock *) clear, (DES_cblock *) cypher, &ks,
			DES_ENCRYPT);
}


#ifdef EAP_TLS_FUNCS
void md5_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac)
{
	MD5_CTX ctx;
	int i;

	MD5_Init(&ctx);
	for (i = 0; i < num_elem; i++)
		MD5_Update(&ctx, addr[i], len[i]);
	MD5_Final(mac, &ctx);
}


void sha1_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac)
{
	SHA_CTX ctx;
	int i;

	SHA1_Init(&ctx);
	for (i = 0; i < num_elem; i++)
		SHA1_Update(&ctx, addr[i], len[i]);
	SHA1_Final(mac, &ctx);
}


void sha1_transform(u8 *state, const u8 data[64])
{
	SHA_CTX context;
	memset(&context, 0, sizeof(context));
	memcpy(&context.h0, state, 5 * 4);
	SHA1_Transform(&context, data);
	memcpy(state, &context.h0, 5 * 4);
}


void * aes_encrypt_init(const u8 *key, size_t len)
{
	AES_KEY *ak;
	ak = malloc(sizeof(*ak));
	if (ak == NULL)
		return NULL;
	if (AES_set_encrypt_key(key, 8 * len, ak) < 0) {
		free(ak);
		return NULL;
	}
	return ak;
}


void aes_encrypt(void *ctx, const u8 *plain, u8 *crypt)
{
	AES_encrypt(plain, crypt, ctx);
}


void aes_encrypt_deinit(void *ctx)
{
	free(ctx);
}


void * aes_decrypt_init(const u8 *key, size_t len)
{
	AES_KEY *ak;
	ak = malloc(sizeof(*ak));
	if (ak == NULL)
		return NULL;
	if (AES_set_decrypt_key(key, 8 * len, ak) < 0) {
		free(ak);
		return NULL;
	}
	return ak;
}


void aes_decrypt(void *ctx, const u8 *crypt, u8 *plain)
{
	AES_decrypt(crypt, plain, ctx);
}


void aes_decrypt_deinit(void *ctx)
{
	free(ctx);
}
#endif /* EAP_TLS_FUNCS */
