/*
 * Copyright (c) 2006 - 2008 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>

#define HC_DEPRECATED

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <evp.h>
#include <evp-hcrypto.h>

#include <krb5-types.h>

#include <des.h>
#include "camellia.h"
#include <aes.h>

#include <rc2.h>
#include <rc4.h>

#include <sha.h>
#include <md2.h>
#include <md4.h>
#include <md5.h>

/*
 *
 */

static int
aes_init(EVP_CIPHER_CTX *ctx,
	 const unsigned char * key,
	 const unsigned char * iv,
	 int encp)
{
    AES_KEY *k = ctx->cipher_data;
    if (ctx->encrypt)
	AES_set_encrypt_key(key, ctx->cipher->key_len * 8, k);
    else
	AES_set_decrypt_key(key, ctx->cipher->key_len * 8, k);
    return 1;
}

static int
aes_do_cipher(EVP_CIPHER_CTX *ctx,
	      unsigned char *out,
	      const unsigned char *in,
	      unsigned int size)
{
    AES_KEY *k = ctx->cipher_data;
    if (ctx->flags & EVP_CIPH_CFB8_MODE)
        AES_cfb8_encrypt(in, out, size, k, ctx->iv, ctx->encrypt);
    else
        AES_cbc_encrypt(in, out, size, k, ctx->iv, ctx->encrypt);
    return 1;
}

/**
 * The AES-128 cipher type (hcrypto)
 *
 * @return the AES-128 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_hcrypto_aes_128_cbc(void)
{
    static const EVP_CIPHER aes_128_cbc = {
	0,
	16,
	16,
	16,
	EVP_CIPH_CBC_MODE,
	aes_init,
	aes_do_cipher,
	NULL,
	sizeof(AES_KEY),
	NULL,
	NULL,
	NULL,
	NULL
    };

    return &aes_128_cbc;
}

/**
 * The AES-192 cipher type (hcrypto)
 *
 * @return the AES-192 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_hcrypto_aes_192_cbc(void)
{
    static const EVP_CIPHER aes_192_cbc = {
	0,
	16,
	24,
	16,
	EVP_CIPH_CBC_MODE,
	aes_init,
	aes_do_cipher,
	NULL,
	sizeof(AES_KEY),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &aes_192_cbc;
}

/**
 * The AES-256 cipher type (hcrypto)
 *
 * @return the AES-256 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_hcrypto_aes_256_cbc(void)
{
    static const EVP_CIPHER aes_256_cbc = {
	0,
	16,
	32,
	16,
	EVP_CIPH_CBC_MODE,
	aes_init,
	aes_do_cipher,
	NULL,
	sizeof(AES_KEY),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &aes_256_cbc;
}

/**
 * The AES-128 CFB8 cipher type (hcrypto)
 *
 * @return the AES-128 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_hcrypto_aes_128_cfb8(void)
{
    static const EVP_CIPHER aes_128_cfb8 = {
	0,
	1,
	16,
	16,
	EVP_CIPH_CFB8_MODE,
	aes_init,
	aes_do_cipher,
	NULL,
	sizeof(AES_KEY),
	NULL,
	NULL,
	NULL,
	NULL
    };

    return &aes_128_cfb8;
}

/**
 * The AES-192 CFB8 cipher type (hcrypto)
 *
 * @return the AES-192 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_hcrypto_aes_192_cfb8(void)
{
    static const EVP_CIPHER aes_192_cfb8 = {
	0,
	1,
	24,
	16,
	EVP_CIPH_CFB8_MODE,
	aes_init,
	aes_do_cipher,
	NULL,
	sizeof(AES_KEY),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &aes_192_cfb8;
}

/**
 * The AES-256 CFB8 cipher type (hcrypto)
 *
 * @return the AES-256 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_hcrypto_aes_256_cfb8(void)
{
    static const EVP_CIPHER aes_256_cfb8 = {
	0,
	1,
	32,
	16,
	EVP_CIPH_CFB8_MODE,
	aes_init,
	aes_do_cipher,
	NULL,
	sizeof(AES_KEY),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &aes_256_cfb8;
}

/**
 * The message digest SHA256 - hcrypto
 *
 * @return the message digest type.
 *
 * @ingroup hcrypto_evp
 */

const EVP_MD *
EVP_hcrypto_sha256(void)
{
    static const struct hc_evp_md sha256 = {
	32,
	64,
	sizeof(SHA256_CTX),
	(hc_evp_md_init)SHA256_Init,
	(hc_evp_md_update)SHA256_Update,
	(hc_evp_md_final)SHA256_Final,
	NULL
    };
    return &sha256;
}

/**
 * The message digest SHA384 - hcrypto
 *
 * @return the message digest type.
 *
 * @ingroup hcrypto_evp
 */

const EVP_MD *
EVP_hcrypto_sha384(void)
{
    static const struct hc_evp_md sha384 = {
	48,
	128,
	sizeof(SHA384_CTX),
	(hc_evp_md_init)SHA384_Init,
	(hc_evp_md_update)SHA384_Update,
	(hc_evp_md_final)SHA384_Final,
	NULL
    };
    return &sha384;
}

/**
 * The message digest SHA512 - hcrypto
 *
 * @return the message digest type.
 *
 * @ingroup hcrypto_evp
 */

const EVP_MD *
EVP_hcrypto_sha512(void)
{
    static const struct hc_evp_md sha512 = {
	64,
	128,
	sizeof(SHA512_CTX),
	(hc_evp_md_init)SHA512_Init,
	(hc_evp_md_update)SHA512_Update,
	(hc_evp_md_final)SHA512_Final,
	NULL
    };
    return &sha512;
}

/**
 * The message digest SHA1 - hcrypto
 *
 * @return the message digest type.
 *
 * @ingroup hcrypto_evp
 */

const EVP_MD *
EVP_hcrypto_sha1(void)
{
    static const struct hc_evp_md sha1 = {
	20,
	64,
	sizeof(SHA_CTX),
	(hc_evp_md_init)SHA1_Init,
	(hc_evp_md_update)SHA1_Update,
	(hc_evp_md_final)SHA1_Final,
	NULL
    };
    return &sha1;
}

/**
 * The message digest MD5 - hcrypto
 *
 * @return the message digest type.
 *
 * @ingroup hcrypto_evp
 */

const EVP_MD *
EVP_hcrypto_md5(void)
{
    static const struct hc_evp_md md5 = {
	16,
	64,
	sizeof(MD5_CTX),
	(hc_evp_md_init)MD5_Init,
	(hc_evp_md_update)MD5_Update,
	(hc_evp_md_final)MD5_Final,
	NULL
    };
    return &md5;
}

/**
 * The message digest MD4 - hcrypto
 *
 * @return the message digest type.
 *
 * @ingroup hcrypto_evp
 */

const EVP_MD *
EVP_hcrypto_md4(void)
{
    static const struct hc_evp_md md4 = {
	16,
	64,
	sizeof(MD4_CTX),
	(hc_evp_md_init)MD4_Init,
	(hc_evp_md_update)MD4_Update,
	(hc_evp_md_final)MD4_Final,
	NULL
    };
    return &md4;
}

/**
 * The message digest MD2 - hcrypto
 *
 * @return the message digest type.
 *
 * @ingroup hcrypto_evp
 */

const EVP_MD *
EVP_hcrypto_md2(void)
{
    static const struct hc_evp_md md2 = {
	16,
	16,
	sizeof(MD2_CTX),
	(hc_evp_md_init)MD2_Init,
	(hc_evp_md_update)MD2_Update,
	(hc_evp_md_final)MD2_Final,
	NULL
    };
    return &md2;
}

/*
 *
 */

static int
des_cbc_init(EVP_CIPHER_CTX *ctx,
	     const unsigned char * key,
	     const unsigned char * iv,
	     int encp)
{
    DES_key_schedule *k = ctx->cipher_data;
    DES_cblock deskey;
    memcpy(&deskey, key, sizeof(deskey));
    DES_set_key_unchecked(&deskey, k);
    return 1;
}

static int
des_cbc_do_cipher(EVP_CIPHER_CTX *ctx,
		  unsigned char *out,
		  const unsigned char *in,
		  unsigned int size)
{
    DES_key_schedule *k = ctx->cipher_data;
    DES_cbc_encrypt(in, out, size,
		    k, (DES_cblock *)ctx->iv, ctx->encrypt);
    return 1;
}

/**
 * The DES cipher type
 *
 * @return the DES-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_hcrypto_des_cbc(void)
{
    static const EVP_CIPHER des_cbc = {
	0,
	8,
	8,
	8,
	EVP_CIPH_CBC_MODE,
	des_cbc_init,
	des_cbc_do_cipher,
	NULL,
	sizeof(DES_key_schedule),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &des_cbc;
}

/*
 *
 */

struct des_ede3_cbc {
    DES_key_schedule ks[3];
};

static int
des_ede3_cbc_init(EVP_CIPHER_CTX *ctx,
		  const unsigned char * key,
		  const unsigned char * iv,
		  int encp)
{
    struct des_ede3_cbc *k = ctx->cipher_data;
    DES_cblock deskey;

    memcpy(&deskey, key, sizeof(deskey));
    DES_set_odd_parity(&deskey);
    DES_set_key_unchecked(&deskey, &k->ks[0]);

    memcpy(&deskey, key + 8, sizeof(deskey));
    DES_set_odd_parity(&deskey);
    DES_set_key_unchecked(&deskey, &k->ks[1]);

    memcpy(&deskey, key + 16, sizeof(deskey));
    DES_set_odd_parity(&deskey);
    DES_set_key_unchecked(&deskey, &k->ks[2]);

    return 1;
}

static int
des_ede3_cbc_do_cipher(EVP_CIPHER_CTX *ctx,
		       unsigned char *out,
		       const unsigned char *in,
		       unsigned int size)
{
    struct des_ede3_cbc *k = ctx->cipher_data;
    DES_ede3_cbc_encrypt(in, out, size,
			 &k->ks[0], &k->ks[1], &k->ks[2],
			 (DES_cblock *)ctx->iv, ctx->encrypt);
    return 1;
}

/**
 * The tripple DES cipher type - hcrypto
 *
 * @return the DES-EDE3-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_hcrypto_des_ede3_cbc(void)
{
    static const EVP_CIPHER des_ede3_cbc = {
	0,
	8,
	24,
	8,
	EVP_CIPH_CBC_MODE,
	des_ede3_cbc_init,
	des_ede3_cbc_do_cipher,
	NULL,
	sizeof(struct des_ede3_cbc),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &des_ede3_cbc;
}

/*
 *
 */

struct rc2_cbc {
    unsigned int maximum_effective_key;
    RC2_KEY key;
};

static int
rc2_init(EVP_CIPHER_CTX *ctx,
	 const unsigned char * key,
	 const unsigned char * iv,
	 int encp)
{
    struct rc2_cbc *k = ctx->cipher_data;
    k->maximum_effective_key = EVP_CIPHER_CTX_key_length(ctx) * 8;
    RC2_set_key(&k->key,
		EVP_CIPHER_CTX_key_length(ctx),
		key,
		k->maximum_effective_key);
    return 1;
}

static int
rc2_do_cipher(EVP_CIPHER_CTX *ctx,
	      unsigned char *out,
	      const unsigned char *in,
	      unsigned int size)
{
    struct rc2_cbc *k = ctx->cipher_data;
    RC2_cbc_encrypt(in, out, size, &k->key, ctx->iv, ctx->encrypt);
    return 1;
}

/**
 * The RC2 cipher type - hcrypto
 *
 * @return the RC2 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_hcrypto_rc2_cbc(void)
{
    static const EVP_CIPHER rc2_cbc = {
	0,
	RC2_BLOCK_SIZE,
	RC2_KEY_LENGTH,
	RC2_BLOCK_SIZE,
	EVP_CIPH_CBC_MODE|EVP_CIPH_VARIABLE_LENGTH,
	rc2_init,
	rc2_do_cipher,
	NULL,
	sizeof(struct rc2_cbc),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &rc2_cbc;
}

/**
 * The RC2-40 cipher type
 *
 * @return the RC2-40 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_hcrypto_rc2_40_cbc(void)
{
    static const EVP_CIPHER rc2_40_cbc = {
	0,
	RC2_BLOCK_SIZE,
	5,
	RC2_BLOCK_SIZE,
	EVP_CIPH_CBC_MODE,
	rc2_init,
	rc2_do_cipher,
	NULL,
	sizeof(struct rc2_cbc),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &rc2_40_cbc;
}

/**
 * The RC2-64 cipher type
 *
 * @return the RC2-64 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_hcrypto_rc2_64_cbc(void)
{
    static const EVP_CIPHER rc2_64_cbc = {
	0,
	RC2_BLOCK_SIZE,
	8,
	RC2_BLOCK_SIZE,
	EVP_CIPH_CBC_MODE,
	rc2_init,
	rc2_do_cipher,
	NULL,
	sizeof(struct rc2_cbc),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &rc2_64_cbc;
}

static int
camellia_init(EVP_CIPHER_CTX *ctx,
	 const unsigned char * key,
	 const unsigned char * iv,
	 int encp)
{
    CAMELLIA_KEY *k = ctx->cipher_data;
    k->bits = ctx->cipher->key_len * 8;
    CAMELLIA_set_key(key, ctx->cipher->key_len * 8, k);
    return 1;
}

static int
camellia_do_cipher(EVP_CIPHER_CTX *ctx,
	      unsigned char *out,
	      const unsigned char *in,
	      unsigned int size)
{
    CAMELLIA_KEY *k = ctx->cipher_data;
    CAMELLIA_cbc_encrypt(in, out, size, k, ctx->iv, ctx->encrypt);
    return 1;
}

/**
 * The Camellia-128 cipher type - hcrypto
 *
 * @return the Camellia-128 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_hcrypto_camellia_128_cbc(void)
{
    static const EVP_CIPHER cipher = {
	0,
	16,
	16,
	16,
	EVP_CIPH_CBC_MODE,
	camellia_init,
	camellia_do_cipher,
	NULL,
	sizeof(CAMELLIA_KEY),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &cipher;
}

/**
 * The Camellia-198 cipher type - hcrypto
 *
 * @return the Camellia-198 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_hcrypto_camellia_192_cbc(void)
{
    static const EVP_CIPHER cipher = {
	0,
	16,
	24,
	16,
	EVP_CIPH_CBC_MODE,
	camellia_init,
	camellia_do_cipher,
	NULL,
	sizeof(CAMELLIA_KEY),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &cipher;
}

/**
 * The Camellia-256 cipher type - hcrypto
 *
 * @return the Camellia-256 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_hcrypto_camellia_256_cbc(void)
{
    static const EVP_CIPHER cipher = {
	0,
	16,
	32,
	16,
	EVP_CIPH_CBC_MODE,
	camellia_init,
	camellia_do_cipher,
	NULL,
	sizeof(CAMELLIA_KEY),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &cipher;
}

static int
rc4_init(EVP_CIPHER_CTX *ctx,
	 const unsigned char *key,
	 const unsigned char *iv,
	 int enc)
{
    RC4_KEY *k = ctx->cipher_data;
    RC4_set_key(k, ctx->key_len, key);
    return 1;
}

static int
rc4_do_cipher(EVP_CIPHER_CTX *ctx,
	      unsigned char *out,
	      const unsigned char *in,
	      unsigned int size)
{
    RC4_KEY *k = ctx->cipher_data;
    RC4(k, size, in, out);
    return 1;
}

const EVP_CIPHER *
EVP_hcrypto_rc4(void)
{
    static const EVP_CIPHER rc4 = {
	0,
	1,
	16,
	0,
	EVP_CIPH_STREAM_CIPHER|EVP_CIPH_VARIABLE_LENGTH,
	rc4_init,
	rc4_do_cipher,
	NULL,
	sizeof(RC4_KEY),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &rc4;
}


const EVP_CIPHER *
EVP_hcrypto_rc4_40(void)
{
    static const EVP_CIPHER rc4_40 = {
	0,
	1,
	5,
	0,
	EVP_CIPH_STREAM_CIPHER|EVP_CIPH_VARIABLE_LENGTH,
	rc4_init,
	rc4_do_cipher,
	NULL,
	sizeof(RC4_KEY),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &rc4_40;
}
