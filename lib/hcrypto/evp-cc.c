/*
 * Copyright (c) 2008 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 Apple Inc. All rights reserved.
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

/* CommonCrypto provider */

#ifdef __APPLE__

#include "config.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef HAVE_COMMONCRYPTO_COMMONDIGEST_H
#include <CommonCrypto/CommonDigest.h>
#endif
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
#include <CommonCrypto/CommonCryptor.h>
#endif

#include <evp.h>
#include <evp-cc.h>

/*
 *
 */

#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H

struct cc_key {
    CCCryptorRef href;
};

static int
cc_do_cipher(EVP_CIPHER_CTX *ctx,
	     unsigned char *out,
	     const unsigned char *in,
	     unsigned int size)
{
    struct cc_key *cc = ctx->cipher_data;
    CCCryptorStatus ret;
    size_t moved;

    memcpy(out, in, size);

    ret = CCCryptorUpdate(cc->href, in, size, out, size, &moved);
    if (ret)
	return 0;

    if (moved != size)
	return 0;

    return 1;
}

static int
cc_do_cfb8_cipher(EVP_CIPHER_CTX *ctx,
                  unsigned char *out,
                  const unsigned char *in,
                  unsigned int size)
{
    struct cc_key *cc = ctx->cipher_data;
    CCCryptorStatus ret;
    size_t moved;
    unsigned int i;

    for (i = 0; i < size; i++) {
        unsigned char oiv[EVP_MAX_IV_LENGTH + 1];

        assert(ctx->cipher->iv_len + 1 <= sizeof(oiv));
        memcpy(oiv, ctx->iv, ctx->cipher->iv_len);

        ret = CCCryptorUpdate(cc->href, ctx->iv, ctx->cipher->iv_len,
                              ctx->iv, ctx->cipher->iv_len, &moved);
        if (ret)
            return 0;

        if (moved != ctx->cipher->iv_len)
            return 0;

        if (!ctx->encrypt)
            oiv[ctx->cipher->iv_len] = in[i];
        out[i] = in[i] ^ ctx->iv[0];
        if (ctx->encrypt)
            oiv[ctx->cipher->iv_len] = out[i];

        memcpy(ctx->iv, &oiv[1], ctx->cipher->iv_len);
    }

    return 1;
}

static int
cc_cleanup(EVP_CIPHER_CTX *ctx)
{
    struct cc_key *cc = ctx->cipher_data;
    if (cc->href)
	CCCryptorRelease(cc->href);
    return 1;
}

static int
init_cc_key(int encp, CCAlgorithm alg, CCOptions opts, const void *key,
	    size_t keylen, const void *iv, CCCryptorRef *ref)
{
    CCOperation op = encp ? kCCEncrypt : kCCDecrypt;
    CCCryptorStatus ret;

    if (*ref) {
	if (key == NULL && iv) {
	    CCCryptorReset(*ref, iv);
	    return 1;
	}
	CCCryptorRelease(*ref);
    }

    ret = CCCryptorCreate(op, alg, opts, key, keylen, iv, ref);
    if (ret)
	return 0;
    return 1;
}

static int
cc_des_ede3_cbc_init(EVP_CIPHER_CTX *ctx,
		     const unsigned char * key,
		     const unsigned char * iv,
		     int encp)
{
    struct cc_key *cc = ctx->cipher_data;
    return init_cc_key(encp, kCCAlgorithm3DES, 0, key, kCCKeySize3DES, iv, &cc->href);
}

#endif /* HAVE_COMMONCRYPTO_COMMONCRYPTOR_H */

/**
 * The tripple DES cipher type (Apple CommonCrypto provider)
 *
 * @return the DES-EDE3-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_cc_des_ede3_cbc(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
    static const EVP_CIPHER des_ede3_cbc = {
	0,
	8,
	24,
	8,
	EVP_CIPH_CBC_MODE|EVP_CIPH_ALWAYS_CALL_INIT,
	cc_des_ede3_cbc_init,
	cc_do_cipher,
	cc_cleanup,
	sizeof(struct cc_key),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &des_ede3_cbc;
#else
    return NULL;
#endif
}

#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
/*
 *
 */

static int
cc_des_cbc_init(EVP_CIPHER_CTX *ctx,
		const unsigned char * key,
		const unsigned char * iv,
		int encp)
{
    struct cc_key *cc = ctx->cipher_data;
    return init_cc_key(encp, kCCAlgorithmDES, 0, key, kCCBlockSizeDES, iv, &cc->href);
}
#endif

/**
 * The DES cipher type (Apple CommonCrypto provider)
 *
 * @return the DES-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_cc_des_cbc(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
    static const EVP_CIPHER des_ede3_cbc = {
	0,
	kCCBlockSizeDES,
	kCCBlockSizeDES,
	kCCBlockSizeDES,
	EVP_CIPH_CBC_MODE|EVP_CIPH_ALWAYS_CALL_INIT,
	cc_des_cbc_init,
	cc_do_cipher,
	cc_cleanup,
	sizeof(struct cc_key),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &des_ede3_cbc;
#else
    return NULL;
#endif
}

#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
/*
 *
 */

static int
cc_aes_cbc_init(EVP_CIPHER_CTX *ctx,
		const unsigned char * key,
		const unsigned char * iv,
		int encp)
{
    struct cc_key *cc = ctx->cipher_data;
    return init_cc_key(encp, kCCAlgorithmAES128, 0, key, ctx->cipher->key_len, iv, &cc->href);
}
#endif

/**
 * The AES-128 cipher type (Apple CommonCrypto provider)
 *
 * @return the AES-128-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_cc_aes_128_cbc(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
    static const EVP_CIPHER c = {
	0,
	kCCBlockSizeAES128,
	kCCKeySizeAES128,
	kCCBlockSizeAES128,
	EVP_CIPH_CBC_MODE|EVP_CIPH_ALWAYS_CALL_INIT,
	cc_aes_cbc_init,
	cc_do_cipher,
	cc_cleanup,
	sizeof(struct cc_key),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &c;
#else
    return NULL;
#endif
}

/**
 * The AES-192 cipher type (Apple CommonCrypto provider)
 *
 * @return the AES-192-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_cc_aes_192_cbc(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
    static const EVP_CIPHER c = {
	0,
	kCCBlockSizeAES128,
	kCCKeySizeAES192,
	kCCBlockSizeAES128,
	EVP_CIPH_CBC_MODE|EVP_CIPH_ALWAYS_CALL_INIT,
	cc_aes_cbc_init,
	cc_do_cipher,
	cc_cleanup,
	sizeof(struct cc_key),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &c;
#else
    return NULL;
#endif
}

/**
 * The AES-256 cipher type (Apple CommonCrypto provider)
 *
 * @return the AES-256-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_cc_aes_256_cbc(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
    static const EVP_CIPHER c = {
	0,
	kCCBlockSizeAES128,
	kCCKeySizeAES256,
	kCCBlockSizeAES128,
	EVP_CIPH_CBC_MODE|EVP_CIPH_ALWAYS_CALL_INIT,
	cc_aes_cbc_init,
	cc_do_cipher,
	cc_cleanup,
	sizeof(struct cc_key),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &c;
#else
    return NULL;
#endif
}

#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
/*
 *
 */

static int
cc_aes_cfb8_init(EVP_CIPHER_CTX *ctx,
		const unsigned char * key,
		const unsigned char * iv,
		int encp)
{
    struct cc_key *cc = ctx->cipher_data;
    memcpy(ctx->iv, iv, ctx->cipher->iv_len);
    return init_cc_key(1, kCCAlgorithmAES128, kCCOptionECBMode,
		       key, ctx->cipher->key_len, NULL, &cc->href);
}
#endif

/**
 * The AES-128 CFB8 cipher type (Apple CommonCrypto provider)
 *
 * @return the AES-128-CFB8 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_cc_aes_128_cfb8(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
    static const EVP_CIPHER c = {
	0,
	1,
	kCCKeySizeAES128,
	kCCBlockSizeAES128,
	EVP_CIPH_CFB8_MODE|EVP_CIPH_ALWAYS_CALL_INIT,
	cc_aes_cfb8_init,
	cc_do_cfb8_cipher,
	cc_cleanup,
	sizeof(struct cc_key),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &c;
#else
    return NULL;
#endif
}

/**
 * The AES-192 CFB8 cipher type (Apple CommonCrypto provider)
 *
 * @return the AES-192-CFB8 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_cc_aes_192_cfb8(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
    static const EVP_CIPHER c = {
	0,
	1,
	kCCKeySizeAES192,
	kCCBlockSizeAES128,
	EVP_CIPH_CFB8_MODE|EVP_CIPH_ALWAYS_CALL_INIT,
	cc_aes_cfb8_init,
	cc_do_cfb8_cipher,
	cc_cleanup,
	sizeof(struct cc_key),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &c;
#else
    return NULL;
#endif
}

/**
 * The AES-256 CFB8 cipher type (Apple CommonCrypto provider)
 *
 * @return the AES-256-CFB8 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_cc_aes_256_cfb8(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
    static const EVP_CIPHER c = {
	0,
	kCCBlockSizeAES128,
	kCCKeySizeAES256,
	kCCBlockSizeAES128,
	EVP_CIPH_CFB8_MODE|EVP_CIPH_ALWAYS_CALL_INIT,
	cc_aes_cfb8_init,
	cc_do_cfb8_cipher,
	cc_cleanup,
	sizeof(struct cc_key),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &c;
#else
    return NULL;
#endif
}

/*
 *
 */

#ifdef COMMONCRYPTO_SUPPORTS_RC2
static int
cc_rc2_cbc_init(EVP_CIPHER_CTX *ctx,
		const unsigned char * key,
		const unsigned char * iv,
		int encp)
{
    struct cc_key *cc = ctx->cipher_data;
    return init_cc_key(encp, kCCAlgorithmRC2, 0, key, ctx->cipher->key_len, iv, &cc->href);
}
#endif

/**
 * The RC2 cipher type - common crypto
 *
 * @return the RC2 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */


const EVP_CIPHER *
EVP_cc_rc2_cbc(void)
{
#ifdef COMMONCRYPTO_SUPPORTS_RC2
    static const EVP_CIPHER rc2_cbc = {
	0,
	kCCBlockSizeRC2,
	16,
	kCCBlockSizeRC2,
	EVP_CIPH_CBC_MODE|EVP_CIPH_ALWAYS_CALL_INIT,
	cc_rc2_cbc_init,
	cc_do_cipher,
	cc_cleanup,
	sizeof(struct cc_key),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &rc2_cbc;
#else
    return NULL;
#endif
}

/**
 * The RC2-40 cipher type - common crypto
 *
 * @return the RC2-40 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */


const EVP_CIPHER *
EVP_cc_rc2_40_cbc(void)
{
#ifdef COMMONCRYPTO_SUPPORTS_RC2
    static const EVP_CIPHER rc2_40_cbc = {
	0,
	kCCBlockSizeRC2,
	5,
	kCCBlockSizeRC2,
	EVP_CIPH_CBC_MODE|EVP_CIPH_ALWAYS_CALL_INIT,
	cc_rc2_cbc_init,
	cc_do_cipher,
	cc_cleanup,
	sizeof(struct cc_key),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &rc2_40_cbc;
#else
    return NULL;
#endif
}


/**
 * The RC2-64 cipher type - common crypto
 *
 * @return the RC2-64 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */


const EVP_CIPHER *
EVP_cc_rc2_64_cbc(void)
{
#ifdef COMMONCRYPTO_SUPPORTS_RC2
    static const EVP_CIPHER rc2_64_cbc = {
	0,
	kCCBlockSizeRC2,
	8,
	kCCBlockSizeRC2,
	EVP_CIPH_CBC_MODE|EVP_CIPH_ALWAYS_CALL_INIT,
	cc_rc2_cbc_init,
	cc_do_cipher,
	cc_cleanup,
	sizeof(struct cc_key),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &rc2_64_cbc;
#else
    return NULL;
#endif
}

/**
 * The CommonCrypto md2 provider
 *
 * @ingroup hcrypto_evp
 */

const EVP_MD *
EVP_cc_md2(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONDIGEST_H
    static const struct hc_evp_md md2 = {
	CC_MD2_DIGEST_LENGTH,
	CC_MD2_BLOCK_BYTES,
	sizeof(CC_MD2_CTX),
	(hc_evp_md_init)CC_MD2_Init,
	(hc_evp_md_update)CC_MD2_Update,
	(hc_evp_md_final)CC_MD2_Final,
	(hc_evp_md_cleanup)NULL
    };
    return &md2;
#else
    return NULL;
#endif
}

/**
 * The CommonCrypto md4 provider
 *
 * @ingroup hcrypto_evp
 */

const EVP_MD *
EVP_cc_md4(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONDIGEST_H
    static const struct hc_evp_md md4 = {
	CC_MD4_DIGEST_LENGTH,
	CC_MD4_BLOCK_BYTES,
	sizeof(CC_MD4_CTX),
	(hc_evp_md_init)CC_MD4_Init,
	(hc_evp_md_update)CC_MD4_Update,
	(hc_evp_md_final)CC_MD4_Final,
	(hc_evp_md_cleanup)NULL
    };
    return &md4;
#else
    return NULL;
#endif
}

/**
 * The CommonCrypto md5 provider
 *
 * @ingroup hcrypto_evp
 */

const EVP_MD *
EVP_cc_md5(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONDIGEST_H
    static const struct hc_evp_md md5 = {
	CC_MD5_DIGEST_LENGTH,
	CC_MD5_BLOCK_BYTES,
	sizeof(CC_MD5_CTX),
	(hc_evp_md_init)CC_MD5_Init,
	(hc_evp_md_update)CC_MD5_Update,
	(hc_evp_md_final)CC_MD5_Final,
	(hc_evp_md_cleanup)NULL
    };
    return &md5;
#else
    return NULL;
#endif
}

/**
 * The CommonCrypto sha1 provider
 *
 * @ingroup hcrypto_evp
 */

const EVP_MD *
EVP_cc_sha1(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONDIGEST_H
    static const struct hc_evp_md sha1 = {
	CC_SHA1_DIGEST_LENGTH,
	CC_SHA1_BLOCK_BYTES,
	sizeof(CC_SHA1_CTX),
	(hc_evp_md_init)CC_SHA1_Init,
	(hc_evp_md_update)CC_SHA1_Update,
	(hc_evp_md_final)CC_SHA1_Final,
	(hc_evp_md_cleanup)NULL
    };
    return &sha1;
#else
    return NULL;
#endif
}

/**
 * The CommonCrypto sha256 provider
 *
 * @ingroup hcrypto_evp
 */

const EVP_MD *
EVP_cc_sha256(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONDIGEST_H
    static const struct hc_evp_md sha256 = {
	CC_SHA256_DIGEST_LENGTH,
	CC_SHA256_BLOCK_BYTES,
	sizeof(CC_SHA256_CTX),
	(hc_evp_md_init)CC_SHA256_Init,
	(hc_evp_md_update)CC_SHA256_Update,
	(hc_evp_md_final)CC_SHA256_Final,
	(hc_evp_md_cleanup)NULL
    };
    return &sha256;
#else
    return NULL;
#endif
}

/**
 * The Camellia-128 cipher type - CommonCrypto
 *
 * @return the Camellia-128 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_cc_camellia_128_cbc(void)
{
    return NULL;
}

/**
 * The Camellia-198 cipher type - CommonCrypto
 *
 * @return the Camellia-198 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_cc_camellia_192_cbc(void)
{
    return NULL;
}

/**
 * The Camellia-256 cipher type - CommonCrypto
 *
 * @return the Camellia-256 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_cc_camellia_256_cbc(void)
{
    return NULL;
}

#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H

/*
 *
 */

static int
cc_rc4_init(EVP_CIPHER_CTX *ctx,
	    const unsigned char * key,
	    const unsigned char * iv,
	    int encp)
{
    struct cc_key *cc = ctx->cipher_data;
    return init_cc_key(encp, kCCAlgorithmRC4, 0, key, ctx->key_len, iv, &cc->href);
}

#endif

/**

 * The RC4 cipher type (Apple CommonCrypto provider)
 *
 * @return the RC4 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_cc_rc4(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
    static const EVP_CIPHER rc4 = {
	0,
	1,
	16,
	0,
	EVP_CIPH_STREAM_CIPHER|EVP_CIPH_VARIABLE_LENGTH,
	cc_rc4_init,
	cc_do_cipher,
	cc_cleanup,
	sizeof(struct cc_key),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &rc4;
#else
    return NULL;
#endif
}


/**
 * The RC4-40 cipher type (Apple CommonCrypto provider)
 *
 * @return the RC4 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_cc_rc4_40(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
    static const EVP_CIPHER rc4_40 = {
	0,
	1,
	5,
	0,
	EVP_CIPH_STREAM_CIPHER|EVP_CIPH_VARIABLE_LENGTH,
	cc_rc4_init,
	cc_do_cipher,
	cc_cleanup,
	sizeof(struct cc_key),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &rc4_40;
#else
    return NULL;
#endif
}

#endif /* __APPLE__ */

