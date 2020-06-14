/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "apr.h"
#include "apr_lib.h"
#include "apu.h"
#include "apu_errno.h"

#include <ctype.h>
#include <assert.h>
#include <stdlib.h>

#include "apr_strings.h"
#include "apr_time.h"
#include "apr_buckets.h"
#include "apr_random.h"

#include "apr_crypto_internal.h"

#if APU_HAVE_CRYPTO

#include <CommonCrypto/CommonCrypto.h>

#define LOG_PREFIX "apr_crypto_commoncrypto: "

struct apr_crypto_t
{
    apr_pool_t *pool;
    const apr_crypto_driver_t *provider;
    apu_err_t *result;
    apr_hash_t *types;
    apr_hash_t *modes;
    apr_random_t *rng;
};

struct apr_crypto_key_t
{
    apr_pool_t *pool;
    const apr_crypto_driver_t *provider;
    const apr_crypto_t *f;
    CCAlgorithm algorithm;
    CCOptions options;
    unsigned char *key;
    int keyLen;
    int ivSize;
    apr_size_t blockSize;
};

struct apr_crypto_block_t
{
    apr_pool_t *pool;
    const apr_crypto_driver_t *provider;
    const apr_crypto_t *f;
    const apr_crypto_key_t *key;
    CCCryptorRef ref;
};

static struct apr_crypto_block_key_type_t key_types[] =
{
{ APR_KEY_3DES_192, 24, 8, 8 },
{ APR_KEY_AES_128, 16, 16, 16 },
{ APR_KEY_AES_192, 24, 16, 16 },
{ APR_KEY_AES_256, 32, 16, 16 } };

static struct apr_crypto_block_key_mode_t key_modes[] =
{
{ APR_MODE_ECB },
{ APR_MODE_CBC } };

/**
 * Fetch the most recent error from this driver.
 */
static apr_status_t crypto_error(const apu_err_t **result,
        const apr_crypto_t *f)
{
    *result = f->result;
    return APR_SUCCESS;
}

/**
 * Shutdown the crypto library and release resources.
 */
static apr_status_t crypto_shutdown(void)
{
    return APR_SUCCESS;
}

static apr_status_t crypto_shutdown_helper(void *data)
{
    return crypto_shutdown();
}

/**
 * Initialise the crypto library and perform one time initialisation.
 */
static apr_status_t crypto_init(apr_pool_t *pool, const char *params,
        const apu_err_t **result)
{

    apr_pool_cleanup_register(pool, pool, crypto_shutdown_helper,
            apr_pool_cleanup_null);

    return APR_SUCCESS;
}

/**
 * @brief Clean encryption / decryption context.
 * @note After cleanup, a context is free to be reused if necessary.
 * @param ctx The block context to use.
 * @return Returns APR_ENOTIMPL if not supported.
 */
static apr_status_t crypto_block_cleanup(apr_crypto_block_t *ctx)
{

    if (ctx->ref) {
        CCCryptorRelease(ctx->ref);
        ctx->ref = NULL;
    }

    return APR_SUCCESS;

}

static apr_status_t crypto_block_cleanup_helper(void *data)
{
    apr_crypto_block_t *block = (apr_crypto_block_t *) data;
    return crypto_block_cleanup(block);
}

/**
 * @brief Clean encryption / decryption context.
 * @note After cleanup, a context is free to be reused if necessary.
 * @param f The context to use.
 * @return Returns APR_ENOTIMPL if not supported.
 */
static apr_status_t crypto_cleanup(apr_crypto_t *f)
{

    return APR_SUCCESS;

}

static apr_status_t crypto_cleanup_helper(void *data)
{
    apr_crypto_t *f = (apr_crypto_t *) data;
    return crypto_cleanup(f);
}

/**
 * @brief Create a context for supporting encryption. Keys, certificates,
 *        algorithms and other parameters will be set per context. More than
 *        one context can be created at one time. A cleanup will be automatically
 *        registered with the given pool to guarantee a graceful shutdown.
 * @param f - context pointer will be written here
 * @param provider - provider to use
 * @param params - array of key parameters
 * @param pool - process pool
 * @return APR_ENOENGINE when the engine specified does not exist. APR_EINITENGINE
 * if the engine cannot be initialised.
 */
static apr_status_t crypto_make(apr_crypto_t **ff,
        const apr_crypto_driver_t *provider, const char *params,
        apr_pool_t *pool)
{
    apr_crypto_t *f = apr_pcalloc(pool, sizeof(apr_crypto_t));
    apr_status_t rv;

    if (!f) {
        return APR_ENOMEM;
    }
    *ff = f;
    f->pool = pool;
    f->provider = provider;

    /* seed the secure random number generator */
    f->rng = apr_random_standard_new(pool);
    if (!f->rng) {
        return APR_ENOMEM;
    }
    do {
        unsigned char seed[8];
        rv = apr_generate_random_bytes(seed, sizeof(seed));
        if (rv != APR_SUCCESS) {
            return rv;
        }
        apr_random_add_entropy(f->rng, seed, sizeof(seed));
        rv = apr_random_secure_ready(f->rng);
    } while (rv == APR_ENOTENOUGHENTROPY);

    f->result = apr_pcalloc(pool, sizeof(apu_err_t));
    if (!f->result) {
        return APR_ENOMEM;
    }

    f->types = apr_hash_make(pool);
    if (!f->types) {
        return APR_ENOMEM;
    }
    apr_hash_set(f->types, "3des192", APR_HASH_KEY_STRING, &(key_types[0]));
    apr_hash_set(f->types, "aes128", APR_HASH_KEY_STRING, &(key_types[1]));
    apr_hash_set(f->types, "aes192", APR_HASH_KEY_STRING, &(key_types[2]));
    apr_hash_set(f->types, "aes256", APR_HASH_KEY_STRING, &(key_types[3]));

    f->modes = apr_hash_make(pool);
    if (!f->modes) {
        return APR_ENOMEM;
    }
    apr_hash_set(f->modes, "ecb", APR_HASH_KEY_STRING, &(key_modes[0]));
    apr_hash_set(f->modes, "cbc", APR_HASH_KEY_STRING, &(key_modes[1]));

    apr_pool_cleanup_register(pool, f, crypto_cleanup_helper,
            apr_pool_cleanup_null);

    return APR_SUCCESS;

}

/**
 * @brief Get a hash table of key types, keyed by the name of the type against
 * a pointer to apr_crypto_block_key_type_t.
 *
 * @param types - hashtable of key types keyed to constants.
 * @param f - encryption context
 * @return APR_SUCCESS for success
 */
static apr_status_t crypto_get_block_key_types(apr_hash_t **types,
        const apr_crypto_t *f)
{
    *types = f->types;
    return APR_SUCCESS;
}

/**
 * @brief Get a hash table of key modes, keyed by the name of the mode against
 * a pointer to apr_crypto_block_key_mode_t.
 *
 * @param modes - hashtable of key modes keyed to constants.
 * @param f - encryption context
 * @return APR_SUCCESS for success
 */
static apr_status_t crypto_get_block_key_modes(apr_hash_t **modes,
        const apr_crypto_t *f)
{
    *modes = f->modes;
    return APR_SUCCESS;
}

/*
 * Work out which mechanism to use.
 */
static apr_status_t crypto_cipher_mechanism(apr_crypto_key_t *key,
        const apr_crypto_block_key_type_e type,
        const apr_crypto_block_key_mode_e mode, const int doPad, apr_pool_t *p)
{
    /* handle padding */
    key->options = doPad ? kCCOptionPKCS7Padding : 0;

    /* determine the algorithm to be used */
    switch (type) {

    case (APR_KEY_3DES_192):

        /* A 3DES key */
        if (mode == APR_MODE_CBC) {
            key->algorithm = kCCAlgorithm3DES;
            key->keyLen = kCCKeySize3DES;
            key->ivSize = kCCBlockSize3DES;
            key->blockSize = kCCBlockSize3DES;
        }
        else {
            key->algorithm = kCCAlgorithm3DES;
            key->options += kCCOptionECBMode;
            key->keyLen = kCCKeySize3DES;
            key->ivSize = 0;
            key->blockSize = kCCBlockSize3DES;
        }
        break;

    case (APR_KEY_AES_128):

        if (mode == APR_MODE_CBC) {
            key->algorithm = kCCAlgorithmAES128;
            key->keyLen = kCCKeySizeAES128;
            key->ivSize = kCCBlockSizeAES128;
            key->blockSize = kCCBlockSizeAES128;
        }
        else {
            key->algorithm = kCCAlgorithmAES128;
            key->options += kCCOptionECBMode;
            key->keyLen = kCCKeySizeAES128;
            key->ivSize = 0;
            key->blockSize = kCCBlockSizeAES128;
        }
        break;

    case (APR_KEY_AES_192):

        if (mode == APR_MODE_CBC) {
            key->algorithm = kCCAlgorithmAES128;
            key->keyLen = kCCKeySizeAES192;
            key->ivSize = kCCBlockSizeAES128;
            key->blockSize = kCCBlockSizeAES128;
        }
        else {
            key->algorithm = kCCAlgorithmAES128;
            key->options += kCCOptionECBMode;
            key->keyLen = kCCKeySizeAES192;
            key->ivSize = 0;
            key->blockSize = kCCBlockSizeAES128;
        }
        break;

    case (APR_KEY_AES_256):

        if (mode == APR_MODE_CBC) {
            key->algorithm = kCCAlgorithmAES128;
            key->keyLen = kCCKeySizeAES256;
            key->ivSize = kCCBlockSizeAES128;
            key->blockSize = kCCBlockSizeAES128;
        }
        else {
            key->algorithm = kCCAlgorithmAES128;
            key->options += kCCOptionECBMode;
            key->keyLen = kCCKeySizeAES256;
            key->ivSize = 0;
            key->blockSize = kCCBlockSizeAES128;
        }
        break;

    default:

        /* TODO: Support CAST, Blowfish */

        /* unknown key type, give up */
        return APR_EKEYTYPE;

    }

    /* make space for the key */
    key->key = apr_palloc(p, key->keyLen);
    if (!key->key) {
        return APR_ENOMEM;
    }
    apr_crypto_clear(p, key->key, key->keyLen);

    return APR_SUCCESS;
}

/**
 * @brief Create a key from the provided secret or passphrase. The key is cleaned
 *        up when the context is cleaned, and may be reused with multiple encryption
 *        or decryption operations.
 * @note If *key is NULL, a apr_crypto_key_t will be created from a pool. If
 *       *key is not NULL, *key must point at a previously created structure.
 * @param key The key returned, see note.
 * @param rec The key record, from which the key will be derived.
 * @param f The context to use.
 * @param p The pool to use.
 * @return Returns APR_ENOKEY if the pass phrase is missing or empty, or if a backend
 *         error occurred while generating the key. APR_ENOCIPHER if the type or mode
 *         is not supported by the particular backend. APR_EKEYTYPE if the key type is
 *         not known. APR_EPADDING if padding was requested but is not supported.
 *         APR_ENOTIMPL if not implemented.
 */
static apr_status_t crypto_key(apr_crypto_key_t **k,
        const apr_crypto_key_rec_t *rec, const apr_crypto_t *f, apr_pool_t *p)
{
    apr_status_t rv;
    apr_crypto_key_t *key = *k;

    if (!key) {
        *k = key = apr_pcalloc(p, sizeof *key);
    }
    if (!key) {
        return APR_ENOMEM;
    }

    key->f = f;
    key->provider = f->provider;

    /* decide on what cipher mechanism we will be using */
    rv = crypto_cipher_mechanism(key, rec->type, rec->mode, rec->pad, p);
    if (APR_SUCCESS != rv) {
        return rv;
    }

    switch (rec->ktype) {

    case APR_CRYPTO_KTYPE_PASSPHRASE: {

        /* generate the key */
        if ((f->result->rc = CCKeyDerivationPBKDF(kCCPBKDF2,
                rec->k.passphrase.pass, rec->k.passphrase.passLen,
                rec->k.passphrase.salt, rec->k.passphrase.saltLen,
                kCCPRFHmacAlgSHA1, rec->k.passphrase.iterations, key->key,
                key->keyLen)) == kCCParamError) {
            return APR_ENOKEY;
        }

        break;
    }

    case APR_CRYPTO_KTYPE_SECRET: {

        /* sanity check - key correct size? */
        if (rec->k.secret.secretLen != key->keyLen) {
            return APR_EKEYLENGTH;
        }

        /* copy the key */
        memcpy(key->key, rec->k.secret.secret, rec->k.secret.secretLen);

        break;
    }

    default: {

        return APR_ENOKEY;

    }
    }

    return APR_SUCCESS;
}

/**
 * @brief Create a key from the given passphrase. By default, the PBKDF2
 *        algorithm is used to generate the key from the passphrase. It is expected
 *        that the same pass phrase will generate the same key, regardless of the
 *        backend crypto platform used. The key is cleaned up when the context
 *        is cleaned, and may be reused with multiple encryption or decryption
 *        operations.
 * @note If *key is NULL, a apr_crypto_key_t will be created from a pool. If
 *       *key is not NULL, *key must point at a previously created structure.
 * @param key The key returned, see note.
 * @param ivSize The size of the initialisation vector will be returned, based
 *               on whether an IV is relevant for this type of crypto.
 * @param pass The passphrase to use.
 * @param passLen The passphrase length in bytes
 * @param salt The salt to use.
 * @param saltLen The salt length in bytes
 * @param type 3DES_192, AES_128, AES_192, AES_256.
 * @param mode Electronic Code Book / Cipher Block Chaining.
 * @param doPad Pad if necessary.
 * @param iterations Iteration count
 * @param f The context to use.
 * @param p The pool to use.
 * @return Returns APR_ENOKEY if the pass phrase is missing or empty, or if a backend
 *         error occurred while generating the key. APR_ENOCIPHER if the type or mode
 *         is not supported by the particular backend. APR_EKEYTYPE if the key type is
 *         not known. APR_EPADDING if padding was requested but is not supported.
 *         APR_ENOTIMPL if not implemented.
 */
static apr_status_t crypto_passphrase(apr_crypto_key_t **k, apr_size_t *ivSize,
        const char *pass, apr_size_t passLen, const unsigned char * salt,
        apr_size_t saltLen, const apr_crypto_block_key_type_e type,
        const apr_crypto_block_key_mode_e mode, const int doPad,
        const int iterations, const apr_crypto_t *f, apr_pool_t *p)
{
    apr_status_t rv;
    apr_crypto_key_t *key = *k;

    if (!key) {
        *k = key = apr_pcalloc(p, sizeof *key);
        if (!key) {
            return APR_ENOMEM;
        }
    }

    key->f = f;
    key->provider = f->provider;

    /* decide on what cipher mechanism we will be using */
    rv = crypto_cipher_mechanism(key, type, mode, doPad, p);
    if (APR_SUCCESS != rv) {
        return rv;
    }

    /* generate the key */
    if ((f->result->rc = CCKeyDerivationPBKDF(kCCPBKDF2, pass, passLen, salt,
            saltLen, kCCPRFHmacAlgSHA1, iterations, key->key, key->keyLen))
            == kCCParamError) {
        return APR_ENOKEY;
    }

    if (ivSize) {
        *ivSize = key->ivSize;
    }

    return APR_SUCCESS;
}

/**
 * @brief Initialise a context for encrypting arbitrary data using the given key.
 * @note If *ctx is NULL, a apr_crypto_block_t will be created from a pool. If
 *       *ctx is not NULL, *ctx must point at a previously created structure.
 * @param ctx The block context returned, see note.
 * @param iv Optional initialisation vector. If the buffer pointed to is NULL,
 *           an IV will be created at random, in space allocated from the pool.
 *           If the buffer pointed to is not NULL, the IV in the buffer will be
 *           used.
 * @param key The key structure.
 * @param blockSize The block size of the cipher.
 * @param p The pool to use.
 * @return Returns APR_ENOIV if an initialisation vector is required but not specified.
 *         Returns APR_EINIT if the backend failed to initialise the context. Returns
 *         APR_ENOTIMPL if not implemented.
 */
static apr_status_t crypto_block_encrypt_init(apr_crypto_block_t **ctx,
        const unsigned char **iv, const apr_crypto_key_t *key,
        apr_size_t *blockSize, apr_pool_t *p)
{
    unsigned char *usedIv;
    apr_crypto_block_t *block = *ctx;
    if (!block) {
        *ctx = block = apr_pcalloc(p, sizeof(apr_crypto_block_t));
    }
    if (!block) {
        return APR_ENOMEM;
    }
    block->f = key->f;
    block->pool = p;
    block->provider = key->provider;
    block->key = key;

    apr_pool_cleanup_register(p, block, crypto_block_cleanup_helper,
            apr_pool_cleanup_null);

    /* generate an IV, if necessary */
    usedIv = NULL;
    if (key->ivSize) {
        if (iv == NULL) {
            return APR_ENOIV;
        }
        if (*iv == NULL) {
            apr_status_t status;
            usedIv = apr_pcalloc(p, key->ivSize);
            if (!usedIv) {
                return APR_ENOMEM;
            }
            apr_crypto_clear(p, usedIv, key->ivSize);
            status = apr_random_secure_bytes(block->f->rng, usedIv,
                    key->ivSize);
            if (APR_SUCCESS != status) {
                return status;
            }
            *iv = usedIv;
        }
        else {
            usedIv = (unsigned char *) *iv;
        }
    }

    /* create a new context for encryption */
    switch ((block->f->result->rc = CCCryptorCreate(kCCEncrypt, key->algorithm,
            key->options, key->key, key->keyLen, usedIv, &block->ref))) {
    case kCCSuccess: {
        break;
    }
    case kCCParamError: {
        return APR_EINIT;
    }
    case kCCMemoryFailure: {
        return APR_ENOMEM;
    }
    case kCCAlignmentError: {
        return APR_EPADDING;
    }
    case kCCUnimplemented: {
        return APR_ENOTIMPL;
    }
    default: {
        return APR_EINIT;
    }
    }

    if (blockSize) {
        *blockSize = key->blockSize;
    }

    return APR_SUCCESS;

}

/**
 * @brief Encrypt data provided by in, write it to out.
 * @note The number of bytes written will be written to outlen. If
 *       out is NULL, outlen will contain the maximum size of the
 *       buffer needed to hold the data, including any data
 *       generated by apr_crypto_block_encrypt_finish below. If *out points
 *       to NULL, a buffer sufficiently large will be created from
 *       the pool provided. If *out points to a not-NULL value, this
 *       value will be used as a buffer instead.
 * @param out Address of a buffer to which data will be written,
 *        see note.
 * @param outlen Length of the output will be written here.
 * @param in Address of the buffer to read.
 * @param inlen Length of the buffer to read.
 * @param ctx The block context to use.
 * @return APR_ECRYPT if an error occurred. Returns APR_ENOTIMPL if
 *         not implemented.
 */
static apr_status_t crypto_block_encrypt(unsigned char **out,
        apr_size_t *outlen, const unsigned char *in, apr_size_t inlen,
        apr_crypto_block_t *ctx)
{
    apr_size_t outl = *outlen;
    unsigned char *buffer;

    /* are we after the maximum size of the out buffer? */
    if (!out) {
        *outlen = CCCryptorGetOutputLength(ctx->ref, inlen, 1);
        return APR_SUCCESS;
    }

    /* must we allocate the output buffer from a pool? */
    if (!*out) {
        outl = CCCryptorGetOutputLength(ctx->ref, inlen, 1);
        buffer = apr_palloc(ctx->pool, outl);
        if (!buffer) {
            return APR_ENOMEM;
        }
        apr_crypto_clear(ctx->pool, buffer, outl);
        *out = buffer;
    }

    switch ((ctx->f->result->rc = CCCryptorUpdate(ctx->ref, in, inlen, (*out),
            outl, &outl))) {
    case kCCSuccess: {
        break;
    }
    case kCCBufferTooSmall: {
        return APR_ENOSPACE;
    }
    default: {
        return APR_ECRYPT;
    }
    }
    *outlen = outl;

    return APR_SUCCESS;

}

/**
 * @brief Encrypt final data block, write it to out.
 * @note If necessary the final block will be written out after being
 *       padded. Typically the final block will be written to the
 *       same buffer used by apr_crypto_block_encrypt, offset by the
 *       number of bytes returned as actually written by the
 *       apr_crypto_block_encrypt() call. After this call, the context
 *       is cleaned and can be reused by apr_crypto_block_encrypt_init().
 * @param out Address of a buffer to which data will be written. This
 *            buffer must already exist, and is usually the same
 *            buffer used by apr_evp_crypt(). See note.
 * @param outlen Length of the output will be written here.
 * @param ctx The block context to use.
 * @return APR_ECRYPT if an error occurred.
 * @return APR_EPADDING if padding was enabled and the block was incorrectly
 *         formatted.
 * @return APR_ENOTIMPL if not implemented.
 */
static apr_status_t crypto_block_encrypt_finish(unsigned char *out,
        apr_size_t *outlen, apr_crypto_block_t *ctx)
{
    apr_size_t len = *outlen;

    ctx->f->result->rc = CCCryptorFinal(ctx->ref, out,
            CCCryptorGetOutputLength(ctx->ref, 0, 1), &len);

    /* always clean up */
    crypto_block_cleanup(ctx);

    switch (ctx->f->result->rc) {
    case kCCSuccess: {
        break;
    }
    case kCCBufferTooSmall: {
        return APR_ENOSPACE;
    }
    case kCCAlignmentError: {
        return APR_EPADDING;
    }
    case kCCDecodeError: {
        return APR_ECRYPT;
    }
    default: {
        return APR_ECRYPT;
    }
    }
    *outlen = len;

    return APR_SUCCESS;

}

/**
 * @brief Initialise a context for decrypting arbitrary data using the given key.
 * @note If *ctx is NULL, a apr_crypto_block_t will be created from a pool. If
 *       *ctx is not NULL, *ctx must point at a previously created structure.
 * @param ctx The block context returned, see note.
 * @param blockSize The block size of the cipher.
 * @param iv Optional initialisation vector. If the buffer pointed to is NULL,
 *           an IV will be created at random, in space allocated from the pool.
 *           If the buffer is not NULL, the IV in the buffer will be used.
 * @param key The key structure.
 * @param p The pool to use.
 * @return Returns APR_ENOIV if an initialisation vector is required but not specified.
 *         Returns APR_EINIT if the backend failed to initialise the context. Returns
 *         APR_ENOTIMPL if not implemented.
 */
static apr_status_t crypto_block_decrypt_init(apr_crypto_block_t **ctx,
        apr_size_t *blockSize, const unsigned char *iv,
        const apr_crypto_key_t *key, apr_pool_t *p)
{
    apr_crypto_block_t *block = *ctx;
    if (!block) {
        *ctx = block = apr_pcalloc(p, sizeof(apr_crypto_block_t));
    }
    if (!block) {
        return APR_ENOMEM;
    }
    block->f = key->f;
    block->pool = p;
    block->provider = key->provider;

    apr_pool_cleanup_register(p, block, crypto_block_cleanup_helper,
            apr_pool_cleanup_null);

    /* generate an IV, if necessary */
    if (key->ivSize) {
        if (iv == NULL) {
            return APR_ENOIV;
        }
    }

    /* create a new context for decryption */
    switch ((block->f->result->rc = CCCryptorCreate(kCCDecrypt, key->algorithm,
            key->options, key->key, key->keyLen, iv, &block->ref))) {
    case kCCSuccess: {
        break;
    }
    case kCCParamError: {
        return APR_EINIT;
    }
    case kCCMemoryFailure: {
        return APR_ENOMEM;
    }
    case kCCAlignmentError: {
        return APR_EPADDING;
    }
    case kCCUnimplemented: {
        return APR_ENOTIMPL;
    }
    default: {
        return APR_EINIT;
    }
    }

    if (blockSize) {
        *blockSize = key->blockSize;
    }

    return APR_SUCCESS;

}

/**
 * @brief Decrypt data provided by in, write it to out.
 * @note The number of bytes written will be written to outlen. If
 *       out is NULL, outlen will contain the maximum size of the
 *       buffer needed to hold the data, including any data
 *       generated by apr_crypto_block_decrypt_finish below. If *out points
 *       to NULL, a buffer sufficiently large will be created from
 *       the pool provided. If *out points to a not-NULL value, this
 *       value will be used as a buffer instead.
 * @param out Address of a buffer to which data will be written,
 *        see note.
 * @param outlen Length of the output will be written here.
 * @param in Address of the buffer to read.
 * @param inlen Length of the buffer to read.
 * @param ctx The block context to use.
 * @return APR_ECRYPT if an error occurred. Returns APR_ENOTIMPL if
 *         not implemented.
 */
static apr_status_t crypto_block_decrypt(unsigned char **out,
        apr_size_t *outlen, const unsigned char *in, apr_size_t inlen,
        apr_crypto_block_t *ctx)
{
    apr_size_t outl = *outlen;
    unsigned char *buffer;

    /* are we after the maximum size of the out buffer? */
    if (!out) {
        *outlen = CCCryptorGetOutputLength(ctx->ref, inlen, 1);
        return APR_SUCCESS;
    }

    /* must we allocate the output buffer from a pool? */
    if (!*out) {
        outl = CCCryptorGetOutputLength(ctx->ref, inlen, 1);
        buffer = apr_palloc(ctx->pool, outl);
        if (!buffer) {
            return APR_ENOMEM;
        }
        apr_crypto_clear(ctx->pool, buffer, outl);
        *out = buffer;
    }

    switch ((ctx->f->result->rc = CCCryptorUpdate(ctx->ref, in, inlen, (*out),
            outl, &outl))) {
    case kCCSuccess: {
        break;
    }
    case kCCBufferTooSmall: {
        return APR_ENOSPACE;
    }
    default: {
        return APR_ECRYPT;
    }
    }
    *outlen = outl;

    return APR_SUCCESS;

}

/**
 * @brief Decrypt final data block, write it to out.
 * @note If necessary the final block will be written out after being
 *       padded. Typically the final block will be written to the
 *       same buffer used by apr_crypto_block_decrypt, offset by the
 *       number of bytes returned as actually written by the
 *       apr_crypto_block_decrypt() call. After this call, the context
 *       is cleaned and can be reused by apr_crypto_block_decrypt_init().
 * @param out Address of a buffer to which data will be written. This
 *            buffer must already exist, and is usually the same
 *            buffer used by apr_evp_crypt(). See note.
 * @param outlen Length of the output will be written here.
 * @param ctx The block context to use.
 * @return APR_ECRYPT if an error occurred.
 * @return APR_EPADDING if padding was enabled and the block was incorrectly
 *         formatted.
 * @return APR_ENOTIMPL if not implemented.
 */
static apr_status_t crypto_block_decrypt_finish(unsigned char *out,
        apr_size_t *outlen, apr_crypto_block_t *ctx)
{
    apr_size_t len = *outlen;

    ctx->f->result->rc = CCCryptorFinal(ctx->ref, out,
            CCCryptorGetOutputLength(ctx->ref, 0, 1), &len);

    /* always clean up */
    crypto_block_cleanup(ctx);

    switch (ctx->f->result->rc) {
    case kCCSuccess: {
        break;
    }
    case kCCBufferTooSmall: {
        return APR_ENOSPACE;
    }
    case kCCAlignmentError: {
        return APR_EPADDING;
    }
    case kCCDecodeError: {
        return APR_ECRYPT;
    }
    default: {
        return APR_ECRYPT;
    }
    }
    *outlen = len;

    return APR_SUCCESS;

}

/**
 * OSX Common Crypto module.
 */
APU_MODULE_DECLARE_DATA const apr_crypto_driver_t apr_crypto_commoncrypto_driver =
{
        "commoncrypto", crypto_init, crypto_make, crypto_get_block_key_types,
        crypto_get_block_key_modes, crypto_passphrase,
        crypto_block_encrypt_init, crypto_block_encrypt,
        crypto_block_encrypt_finish, crypto_block_decrypt_init,
        crypto_block_decrypt, crypto_block_decrypt_finish, crypto_block_cleanup,
        crypto_cleanup, crypto_shutdown, crypto_error, crypto_key
};

#endif
