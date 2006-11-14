/*
 * WPA Supplicant / wrapper functions for libgcrypt
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

#include <stdio.h>
#include <sys/types.h>
#include <gcrypt.h>

#include "common.h"
#include "crypto.h"

void md4_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac)
{
	gcry_md_hd_t hd;
	unsigned char *p;
	int i;

	if (gcry_md_open(&hd, GCRY_MD_MD4, 0) != GPG_ERR_NO_ERROR)
		return;
	for (i = 0; i < num_elem; i++)
		gcry_md_write(hd, addr[i], len[i]);
	p = gcry_md_read(hd, GCRY_MD_MD4);
	if (p)
		memcpy(mac, p, gcry_md_get_algo_dlen(GCRY_MD_MD4));
	gcry_md_close(hd);
}


void des_encrypt(const u8 *clear, const u8 *key, u8 *cypher)
{
	gcry_cipher_hd_t hd;
	u8 pkey[8], next, tmp;
	int i;

	/* Add parity bits to the key */
	next = 0;
	for (i = 0; i < 7; i++) {
		tmp = key[i];
		pkey[i] = (tmp >> i) | next | 1;
		next = tmp << (7 - i);
	}
	pkey[i] = next | 1;

	gcry_cipher_open(&hd, GCRY_CIPHER_DES, GCRY_CIPHER_MODE_ECB, 0);
	gcry_err_code(gcry_cipher_setkey(hd, pkey, 8));
	gcry_cipher_encrypt(hd, cypher, 8, clear, 8);
	gcry_cipher_close(hd);
}


#ifdef EAP_TLS_FUNCS
void md5_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac)
{
	gcry_md_hd_t hd;
	unsigned char *p;
	int i;

	if (gcry_md_open(&hd, GCRY_MD_MD5, 0) != GPG_ERR_NO_ERROR)
		return;
	for (i = 0; i < num_elem; i++)
		gcry_md_write(hd, addr[i], len[i]);
	p = gcry_md_read(hd, GCRY_MD_MD5);
	if (p)
		memcpy(mac, p, gcry_md_get_algo_dlen(GCRY_MD_MD5));
	gcry_md_close(hd);
}


void sha1_vector(size_t num_elem, const u8 *addr[], const size_t *len, u8 *mac)
{
	gcry_md_hd_t hd;
	unsigned char *p;
	int i;

	if (gcry_md_open(&hd, GCRY_MD_SHA1, 0) != GPG_ERR_NO_ERROR)
		return;
	for (i = 0; i < num_elem; i++)
		gcry_md_write(hd, addr[i], len[i]);
	p = gcry_md_read(hd, GCRY_MD_SHA1);
	if (p)
		memcpy(mac, p, gcry_md_get_algo_dlen(GCRY_MD_SHA1));
	gcry_md_close(hd);
}


void sha1_transform(u8 *state, const u8 data[64])
{
	/* FIX: how to do this with libgcrypt? */
}


void * aes_encrypt_init(const u8 *key, size_t len)
{
	gcry_cipher_hd_t hd;

	if (gcry_cipher_open(&hd, GCRY_CIPHER_AES, GCRY_CIPHER_MODE_ECB, 0) !=
	    GPG_ERR_NO_ERROR) {
		printf("cipher open failed\n");
		return NULL;
	}
	if (gcry_cipher_setkey(hd, key, len) != GPG_ERR_NO_ERROR) {
		printf("setkey failed\n");
		gcry_cipher_close(hd);
		return NULL;
	}

	return hd;
}


void aes_encrypt(void *ctx, const u8 *plain, u8 *crypt)
{
	gcry_cipher_hd_t hd = ctx;
	gcry_cipher_encrypt(hd, crypt, 16, plain, 16);
}


void aes_encrypt_deinit(void *ctx)
{
	gcry_cipher_hd_t hd = ctx;
	gcry_cipher_close(hd);
}


void * aes_decrypt_init(const u8 *key, size_t len)
{
	gcry_cipher_hd_t hd;

	if (gcry_cipher_open(&hd, GCRY_CIPHER_AES, GCRY_CIPHER_MODE_ECB, 0) !=
	    GPG_ERR_NO_ERROR)
		return NULL;
	if (gcry_cipher_setkey(hd, key, len) != GPG_ERR_NO_ERROR) {
		gcry_cipher_close(hd);
		return NULL;
	}

	return hd;
}


void aes_decrypt(void *ctx, const u8 *crypt, u8 *plain)
{
	gcry_cipher_hd_t hd = ctx;
	gcry_cipher_decrypt(hd, plain, 16, crypt, 16);
}


void aes_decrypt_deinit(void *ctx)
{
	gcry_cipher_hd_t hd = ctx;
	gcry_cipher_close(hd);
}
#endif /* EAP_TLS_FUNCS */
