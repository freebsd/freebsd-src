/*
 * AES-based functions
 *
 * - AES Key Wrap Algorithm (128-bit KEK) (RFC3394)
 * - One-Key CBC MAC (OMAC1) hash with AES-128
 * - AES-128 CTR mode encryption
 * - AES-128 EAX mode encryption/decryption
 * - AES-128 CBC
 *
 * Copyright (c) 2003-2005, Jouni Malinen <jkmaline@cc.hut.fi>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "common.h"
#include "aes_wrap.h"
#include "crypto.h"

#ifndef EAP_TLS_FUNCS
#include "aes.c"
#endif /* EAP_TLS_FUNCS */


/**
 * aes_wrap - Wrap keys with AES Key Wrap Algorithm (128-bit KEK) (RFC3394)
 * @kek: Key encryption key (KEK)
 * @n: Length of the wrapped key in 64-bit units; e.g., 2 = 128-bit = 16 bytes
 * @plain: Plaintext key to be wrapped, n * 64 bit
 * @cipher: Wrapped key, (n + 1) * 64 bit
 * Returns: 0 on success, -1 on failure
 */
int aes_wrap(const u8 *kek, int n, const u8 *plain, u8 *cipher)
{
	u8 *a, *r, b[16];
	int i, j;
	void *ctx;

	a = cipher;
	r = cipher + 8;

	/* 1) Initialize variables. */
	memset(a, 0xa6, 8);
	memcpy(r, plain, 8 * n);

	ctx = aes_encrypt_init(kek, 16);
	if (ctx == NULL)
		return -1;

	/* 2) Calculate intermediate values.
	 * For j = 0 to 5
	 *     For i=1 to n
	 *         B = AES(K, A | R[i])
	 *         A = MSB(64, B) ^ t where t = (n*j)+i
	 *         R[i] = LSB(64, B)
	 */
	for (j = 0; j <= 5; j++) {
		r = cipher + 8;
		for (i = 1; i <= n; i++) {
			memcpy(b, a, 8);
			memcpy(b + 8, r, 8);
			aes_encrypt(ctx, b, b);
			memcpy(a, b, 8);
			a[7] ^= n * j + i;
			memcpy(r, b + 8, 8);
			r += 8;
		}
	}
	aes_encrypt_deinit(ctx);

	/* 3) Output the results.
	 *
	 * These are already in @cipher due to the location of temporary
	 * variables.
	 */

	return 0;
}


/**
 * aes_unwrap - Unwrap key with AES Key Wrap Algorithm (128-bit KEK) (RFC3394)
 * @kek: Key encryption key (KEK)
 * @n: Length of the wrapped key in 64-bit units; e.g., 2 = 128-bit = 16 bytes
 * @cipher: Wrapped key to be unwrapped, (n + 1) * 64 bit
 * @plain: Plaintext key, n * 64 bit
 * Returns: 0 on success, -1 on failure (e.g., integrity verification failed)
 */
int aes_unwrap(const u8 *kek, int n, const u8 *cipher, u8 *plain)
{
	u8 a[8], *r, b[16];
	int i, j;
	void *ctx;

	/* 1) Initialize variables. */
	memcpy(a, cipher, 8);
	r = plain;
	memcpy(r, cipher + 8, 8 * n);

	ctx = aes_decrypt_init(kek, 16);
	if (ctx == NULL)
		return -1;

	/* 2) Compute intermediate values.
	 * For j = 5 to 0
	 *     For i = n to 1
	 *         B = AES-1(K, (A ^ t) | R[i]) where t = n*j+i
	 *         A = MSB(64, B)
	 *         R[i] = LSB(64, B)
	 */
	for (j = 5; j >= 0; j--) {
		r = plain + (n - 1) * 8;
		for (i = n; i >= 1; i--) {
			memcpy(b, a, 8);
			b[7] ^= n * j + i;

			memcpy(b + 8, r, 8);
			aes_decrypt(ctx, b, b);
			memcpy(a, b, 8);
			memcpy(r, b + 8, 8);
			r -= 8;
		}
	}
	aes_decrypt_deinit(ctx);

	/* 3) Output results.
	 *
	 * These are already in @plain due to the location of temporary
	 * variables. Just verify that the IV matches with the expected value.
	 */
	for (i = 0; i < 8; i++) {
		if (a[i] != 0xa6)
			return -1;
	}

	return 0;
}


#define BLOCK_SIZE 16

static void gf_mulx(u8 *pad)
{
	int i, carry;

	carry = pad[0] & 0x80;
	for (i = 0; i < BLOCK_SIZE - 1; i++)
		pad[i] = (pad[i] << 1) | (pad[i + 1] >> 7);
	pad[BLOCK_SIZE - 1] <<= 1;
	if (carry)
		pad[BLOCK_SIZE - 1] ^= 0x87;
}


/**
 * omac1_aes_128 - One-Key CBC MAC (OMAC1) hash with AES-128
 * @key: Key for the hash operation
 * @data: Data buffer for which a MAC is determined
 * @data: Length of data buffer in bytes
 * @mac: Buffer for MAC (128 bits, i.e., 16 bytes)
 * Returns: 0 on success, -1 on failure
 */
int omac1_aes_128(const u8 *key, const u8 *data, size_t data_len, u8 *mac)
{
	void *ctx;
	u8 cbc[BLOCK_SIZE], pad[BLOCK_SIZE];
	const u8 *pos = data;
	int i;
	size_t left = data_len;

	ctx = aes_encrypt_init(key, 16);
	if (ctx == NULL)
		return -1;
	memset(cbc, 0, BLOCK_SIZE);

	while (left >= BLOCK_SIZE) {
		for (i = 0; i < BLOCK_SIZE; i++)
			cbc[i] ^= *pos++;
		if (left > BLOCK_SIZE)
			aes_encrypt(ctx, cbc, cbc);
		left -= BLOCK_SIZE;
	}

	memset(pad, 0, BLOCK_SIZE);
	aes_encrypt(ctx, pad, pad);
	gf_mulx(pad);

	if (left || data_len == 0) {
		for (i = 0; i < left; i++)
			cbc[i] ^= *pos++;
		cbc[left] ^= 0x80;
		gf_mulx(pad);
	}

	for (i = 0; i < BLOCK_SIZE; i++)
		pad[i] ^= cbc[i];
	aes_encrypt(ctx, pad, mac);
	aes_encrypt_deinit(ctx);
	return 0;
}


/**
 * aes_128_encrypt_block - Perform one AES 128-bit block operation
 * @key: Key for AES
 * @in: Input data (16 bytes)
 * @out: Output of the AES block operation (16 bytes)
 * Returns: 0 on success, -1 on failure
 */
int aes_128_encrypt_block(const u8 *key, const u8 *in, u8 *out)
{
	void *ctx;
	ctx = aes_encrypt_init(key, 16);
	if (ctx == NULL)
		return -1;
	aes_encrypt(ctx, in, out);
	aes_encrypt_deinit(ctx);
	return 0;
}


/**
 * aes_128_ctr_encrypt - AES-128 CTR mode encryption
 * @key: Key for encryption (16 bytes)
 * @nonce: Nonce for counter mode (16 bytes)
 * @data: Data to encrypt in-place
 * @data_len: Length of data in bytes
 * Returns: 0 on success, -1 on failure
 */
int aes_128_ctr_encrypt(const u8 *key, const u8 *nonce,
			u8 *data, size_t data_len)
{
	void *ctx;
	size_t len, left = data_len;
	int i;
	u8 *pos = data;
	u8 counter[BLOCK_SIZE], buf[BLOCK_SIZE];

	ctx = aes_encrypt_init(key, 16);
	if (ctx == NULL)
		return -1;
	memcpy(counter, nonce, BLOCK_SIZE);

	while (left > 0) {
		aes_encrypt(ctx, counter, buf);

		len = (left < BLOCK_SIZE) ? left : BLOCK_SIZE;
		for (i = 0; i < len; i++)
			pos[i] ^= buf[i];
		pos += len;
		left -= len;

		for (i = BLOCK_SIZE - 1; i >= 0; i--) {
			counter[i]++;
			if (counter[i])
				break;
		}
	}
	aes_encrypt_deinit(ctx);
	return 0;
}


/**
 * aes_128_eax_encrypt - AES-128 EAX mode encryption
 * @key: Key for encryption (16 bytes)
 * @nonce: Nonce for counter mode
 * @nonce_len: Nonce length in bytes
 * @hdr: Header data to be authenticity protected
 * @hdr_len: Length of the header data bytes
 * @data: Data to encrypt in-place
 * @data_len: Length of data in bytes
 * @tag: 16-byte tag value
 * Returns: 0 on success, -1 on failure
 */
int aes_128_eax_encrypt(const u8 *key, const u8 *nonce, size_t nonce_len,
			const u8 *hdr, size_t hdr_len,
			u8 *data, size_t data_len, u8 *tag)
{
	u8 *buf;
	size_t buf_len;
	u8 nonce_mac[BLOCK_SIZE], hdr_mac[BLOCK_SIZE], data_mac[BLOCK_SIZE];
	int i;

	if (nonce_len > data_len)
		buf_len = nonce_len;
	else
		buf_len = data_len;
	if (hdr_len > buf_len)
		buf_len = hdr_len;
	buf_len += 16;

	buf = malloc(buf_len);
	if (buf == NULL)
		return -1;

	memset(buf, 0, 15);

	buf[15] = 0;
	memcpy(buf + 16, nonce, nonce_len);
	omac1_aes_128(key, buf, 16 + nonce_len, nonce_mac);

	buf[15] = 1;
	memcpy(buf + 16, hdr, hdr_len);
	omac1_aes_128(key, buf, 16 + hdr_len, hdr_mac);

	aes_128_ctr_encrypt(key, nonce_mac, data, data_len);
	buf[15] = 2;
	memcpy(buf + 16, data, data_len);
	omac1_aes_128(key, buf, 16 + data_len, data_mac);

	free(buf);

	for (i = 0; i < BLOCK_SIZE; i++)
		tag[i] = nonce_mac[i] ^ data_mac[i] ^ hdr_mac[i];

	return 0;
}


/**
 * aes_128_eax_decrypt - AES-128 EAX mode decryption
 * @key: Key for decryption (16 bytes)
 * @nonce: Nonce for counter mode
 * @nonce_len: Nonce length in bytes
 * @hdr: Header data to be authenticity protected
 * @hdr_len: Length of the header data bytes
 * @data: Data to encrypt in-place
 * @data_len: Length of data in bytes
 * @tag: 16-byte tag value
 * Returns: 0 on success, -1 on failure, -2 if tag does not match
 */
int aes_128_eax_decrypt(const u8 *key, const u8 *nonce, size_t nonce_len,
			const u8 *hdr, size_t hdr_len,
			u8 *data, size_t data_len, const u8 *tag)
{
	u8 *buf;
	size_t buf_len;
	u8 nonce_mac[BLOCK_SIZE], hdr_mac[BLOCK_SIZE], data_mac[BLOCK_SIZE];
	int i;

	if (nonce_len > data_len)
		buf_len = nonce_len;
	else
		buf_len = data_len;
	if (hdr_len > buf_len)
		buf_len = hdr_len;
	buf_len += 16;

	buf = malloc(buf_len);
	if (buf == NULL)
		return -1;

	memset(buf, 0, 15);

	buf[15] = 0;
	memcpy(buf + 16, nonce, nonce_len);
	omac1_aes_128(key, buf, 16 + nonce_len, nonce_mac);

	buf[15] = 1;
	memcpy(buf + 16, hdr, hdr_len);
	omac1_aes_128(key, buf, 16 + hdr_len, hdr_mac);

	buf[15] = 2;
	memcpy(buf + 16, data, data_len);
	omac1_aes_128(key, buf, 16 + data_len, data_mac);

	free(buf);

	for (i = 0; i < BLOCK_SIZE; i++) {
		if (tag[i] != (nonce_mac[i] ^ data_mac[i] ^ hdr_mac[i]))
			return -2;
	}

	aes_128_ctr_encrypt(key, nonce_mac, data, data_len);

	return 0;
}


/**
 * aes_128_cbc_encrypt - AES-128 CBC encryption
 * @key: Encryption key
 * @iv: Encryption IV for CBC mode (16 bytes)
 * @data: Data to encrypt in-place
 * @data_len: Length of data in bytes (must be divisible by 16)
 * Returns: 0 on success, -1 on failure
 */
int aes_128_cbc_encrypt(const u8 *key, const u8 *iv, u8 *data, size_t data_len)
{
	void *ctx;
	u8 cbc[BLOCK_SIZE];
	u8 *pos = data;
	int i, j, blocks;

	ctx = aes_encrypt_init(key, 16);
	if (ctx == NULL)
		return -1;
	memcpy(cbc, iv, BLOCK_SIZE);

	blocks = data_len / BLOCK_SIZE;
	for (i = 0; i < blocks; i++) {
		for (j = 0; j < BLOCK_SIZE; j++)
			cbc[j] ^= pos[j];
		aes_encrypt(ctx, cbc, cbc);
		memcpy(pos, cbc, BLOCK_SIZE);
		pos += BLOCK_SIZE;
	}
	aes_encrypt_deinit(ctx);
	return 0;
}


/**
 * aes_128_cbc_decrypt - AES-128 CBC decryption
 * @key: Decryption key
 * @iv: Decryption IV for CBC mode (16 bytes)
 * @data: Data to decrypt in-place
 * @data_len: Length of data in bytes (must be divisible by 16)
 * Returns: 0 on success, -1 on failure
 */
int aes_128_cbc_decrypt(const u8 *key, const u8 *iv, u8 *data, size_t data_len)
{
	void *ctx;
	u8 cbc[BLOCK_SIZE], tmp[BLOCK_SIZE];
	u8 *pos = data;
	int i, j, blocks;

	ctx = aes_decrypt_init(key, 16);
	if (ctx == NULL)
		return -1;
	memcpy(cbc, iv, BLOCK_SIZE);

	blocks = data_len / BLOCK_SIZE;
	for (i = 0; i < blocks; i++) {
		memcpy(tmp, pos, BLOCK_SIZE);
		aes_decrypt(ctx, pos, pos);
		for (j = 0; j < BLOCK_SIZE; j++)
			pos[j] ^= cbc[j];
		memcpy(cbc, tmp, BLOCK_SIZE);
		pos += BLOCK_SIZE;
	}
	aes_decrypt_deinit(ctx);
	return 0;
}


#ifdef TEST_MAIN

#ifdef __i386__
#define rdtscll(val) \
     __asm__ __volatile__("rdtsc" : "=A" (val))

static void test_aes_perf(void)
{
	const int num_iters = 10;
	int i;
	unsigned int start, end;
	u8 key[16], pt[16], ct[16];
	void *ctx;

	printf("keySetupEnc:");
	for (i = 0; i < num_iters; i++) {
		rdtscll(start);
		ctx = aes_encrypt_init(key, 16);
		rdtscll(end);
		aes_encrypt_deinit(ctx);
		printf(" %d", end - start);
	}
	printf("\n");

	printf("Encrypt:");
	ctx = aes_encrypt_init(key, 16);
	for (i = 0; i < num_iters; i++) {
		rdtscll(start);
		aes_encrypt(ctx, pt, ct);
		rdtscll(end);
		printf(" %d", end - start);
	}
	aes_encrypt_deinit(ctx);
	printf("\n");
}
#endif /* __i386__ */


static int test_eax(void)
{
	u8 msg[] = { 0xF7, 0xFB };
	u8 key[] = { 0x91, 0x94, 0x5D, 0x3F, 0x4D, 0xCB, 0xEE, 0x0B,
		     0xF4, 0x5E, 0xF5, 0x22, 0x55, 0xF0, 0x95, 0xA4 };
	u8 nonce[] = { 0xBE, 0xCA, 0xF0, 0x43, 0xB0, 0xA2, 0x3D, 0x84,
		       0x31, 0x94, 0xBA, 0x97, 0x2C, 0x66, 0xDE, 0xBD };
	u8 hdr[] = { 0xFA, 0x3B, 0xFD, 0x48, 0x06, 0xEB, 0x53, 0xFA };
	u8 cipher[] = { 0x19, 0xDD, 0x5C, 0x4C, 0x93, 0x31, 0x04, 0x9D,
			0x0B, 0xDA, 0xB0, 0x27, 0x74, 0x08, 0xF6, 0x79,
			0x67, 0xE5 };
	u8 data[sizeof(msg)], tag[BLOCK_SIZE];

	memcpy(data, msg, sizeof(msg));
	if (aes_128_eax_encrypt(key, nonce, sizeof(nonce), hdr, sizeof(hdr),
				data, sizeof(data), tag)) {
		printf("AES-128 EAX mode encryption failed\n");
		return 1;
	}
	if (memcmp(data, cipher, sizeof(data)) != 0) {
		printf("AES-128 EAX mode encryption returned invalid cipher "
		       "text\n");
		return 1;
	}
	if (memcmp(tag, cipher + sizeof(data), BLOCK_SIZE) != 0) {
		printf("AES-128 EAX mode encryption returned invalid tag\n");
		return 1;
	}

	if (aes_128_eax_decrypt(key, nonce, sizeof(nonce), hdr, sizeof(hdr),
				data, sizeof(data), tag)) {
		printf("AES-128 EAX mode decryption failed\n");
		return 1;
	}
	if (memcmp(data, msg, sizeof(data)) != 0) {
		printf("AES-128 EAX mode decryption returned invalid plain "
		       "text\n");
		return 1;
	}

	return 0;
}


static int test_cbc(void)
{
	struct cbc_test_vector {
		u8 key[16];
		u8 iv[16];
		u8 plain[32];
		u8 cipher[32];
		size_t len;
	} vectors[] = {
		{
			{ 0x06, 0xa9, 0x21, 0x40, 0x36, 0xb8, 0xa1, 0x5b,
			  0x51, 0x2e, 0x03, 0xd5, 0x34, 0x12, 0x00, 0x06 },
			{ 0x3d, 0xaf, 0xba, 0x42, 0x9d, 0x9e, 0xb4, 0x30,
			  0xb4, 0x22, 0xda, 0x80, 0x2c, 0x9f, 0xac, 0x41 },
			"Single block msg",
			{ 0xe3, 0x53, 0x77, 0x9c, 0x10, 0x79, 0xae, 0xb8,
			  0x27, 0x08, 0x94, 0x2d, 0xbe, 0x77, 0x18, 0x1a },
			16
		},
		{
			{ 0xc2, 0x86, 0x69, 0x6d, 0x88, 0x7c, 0x9a, 0xa0,
			  0x61, 0x1b, 0xbb, 0x3e, 0x20, 0x25, 0xa4, 0x5a },
			{ 0x56, 0x2e, 0x17, 0x99, 0x6d, 0x09, 0x3d, 0x28,
			  0xdd, 0xb3, 0xba, 0x69, 0x5a, 0x2e, 0x6f, 0x58 },
			{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
			  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
			  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f },
			{ 0xd2, 0x96, 0xcd, 0x94, 0xc2, 0xcc, 0xcf, 0x8a,
			  0x3a, 0x86, 0x30, 0x28, 0xb5, 0xe1, 0xdc, 0x0a,
			  0x75, 0x86, 0x60, 0x2d, 0x25, 0x3c, 0xff, 0xf9,
			  0x1b, 0x82, 0x66, 0xbe, 0xa6, 0xd6, 0x1a, 0xb1 },
			32
		}
	};
	int i, ret = 0;
	u8 *buf;

	for (i = 0; i < sizeof(vectors) / sizeof(vectors[0]); i++) {
		struct cbc_test_vector *tv = &vectors[i];
		buf = malloc(tv->len);
		if (buf == NULL) {
			ret++;
			break;
		}
		memcpy(buf, tv->plain, tv->len);
		aes_128_cbc_encrypt(tv->key, tv->iv, buf, tv->len);
		if (memcmp(buf, tv->cipher, tv->len) != 0) {
			printf("AES-CBC encrypt %d failed\n", i);
			ret++;
		}
		memcpy(buf, tv->cipher, tv->len);
		aes_128_cbc_decrypt(tv->key, tv->iv, buf, tv->len);
		if (memcmp(buf, tv->plain, tv->len) != 0) {
			printf("AES-CBC decrypt %d failed\n", i);
			ret++;
		}
		free(buf);
	}

	return ret;
}


/* OMAC1 AES-128 test vectors from
 * http://csrc.nist.gov/CryptoToolkit/modes/proposedmodes/omac/omac-ad.pdf
 */

struct omac1_test_vector {
	u8 k[16];
	u8 msg[64];
	int msg_len;
	u8 tag[16];
};

static struct omac1_test_vector test_vectors[] =
{
	{
		{ 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
		  0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c },
		{ },
		0,
		{ 0xbb, 0x1d, 0x69, 0x29, 0xe9, 0x59, 0x37, 0x28,
		  0x7f, 0xa3, 0x7d, 0x12, 0x9b, 0x75, 0x67, 0x46 }
	},
	{
		{ 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
		  0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c },
		{ 0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
		  0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a},
		16,
		{ 0x07, 0x0a, 0x16, 0xb4, 0x6b, 0x4d, 0x41, 0x44,
		  0xf7, 0x9b, 0xdd, 0x9d, 0xd0, 0x4a, 0x28, 0x7c }
	},
	{
		{ 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
		  0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c },
		{ 0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
		  0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
		  0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
		  0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
		  0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11 },
		40,
		{ 0xdf, 0xa6, 0x67, 0x47, 0xde, 0x9a, 0xe6, 0x30,
		  0x30, 0xca, 0x32, 0x61, 0x14, 0x97, 0xc8, 0x27 }
	},
	{
		{ 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
		  0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c },
		{ 0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
		  0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
		  0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
		  0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
		  0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
		  0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
		  0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
		  0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10 },
		64,
		{ 0x51, 0xf0, 0xbe, 0xbf, 0x7e, 0x3b, 0x9d, 0x92,
		  0xfc, 0x49, 0x74, 0x17, 0x79, 0x36, 0x3c, 0xfe }
	},
};


int main(int argc, char *argv[])
{
	u8 kek[] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
	};
	u8 plain[] = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
	};
	u8 crypt[] = {
		0x1F, 0xA6, 0x8B, 0x0A, 0x81, 0x12, 0xB4, 0x47,
		0xAE, 0xF3, 0x4B, 0xD8, 0xFB, 0x5A, 0x7B, 0x82,
		0x9D, 0x3E, 0x86, 0x23, 0x71, 0xD2, 0xCF, 0xE5
	};
	u8 result[24];
	int ret = 0, i;
	struct omac1_test_vector *tv;

	if (aes_wrap(kek, 2, plain, result)) {
		printf("AES-WRAP-128-128 reported failure\n");
		ret++;
	}
	if (memcmp(result, crypt, 24) != 0) {
		printf("AES-WRAP-128-128 failed\n");
		ret++;
	}
	if (aes_unwrap(kek, 2, crypt, result)) {
		printf("AES-UNWRAP-128-128 reported failure\n");
		ret++;
	}
	if (memcmp(result, plain, 16) != 0) {
		int i;
		printf("AES-UNWRAP-128-128 failed\n");
		ret++;
		for (i = 0; i < 16; i++)
			printf(" %02x", result[i]);
		printf("\n");
	}

#ifdef __i386__
	test_aes_perf();
#endif /* __i386__ */

	for (i = 0; i < sizeof(test_vectors) / sizeof(test_vectors[0]); i++) {
		tv = &test_vectors[i];
		omac1_aes_128(tv->k, tv->msg, tv->msg_len, result);
		if (memcmp(result, tv->tag, 16) != 0) {
			printf("OMAC1-AES-128 test vector %d failed\n", i);
			ret++;
		}
	}

	ret += test_eax();

	ret += test_cbc();

	if (ret)
		printf("FAILED!\n");

	return ret;
}
#endif /* TEST_MAIN */
