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

#include "testutil.h"
#include "apr.h"
#include "apu.h"
#include "apu_errno.h"
#include "apr_pools.h"
#include "apr_dso.h"
#include "apr_crypto.h"
#include "apr_strings.h"

#if APU_HAVE_CRYPTO

#define TEST_STRING "12345"
#define ALIGNED_STRING "123456789012345"

static const apr_crypto_driver_t *get_driver(abts_case *tc, apr_pool_t *pool,
        const char *name, const char *params)
{

    const apr_crypto_driver_t *driver = NULL;
    const apu_err_t *result = NULL;
    apr_status_t rv;

    rv = apr_crypto_init(pool);
    ABTS_ASSERT(tc, "failed to init apr_crypto", rv == APR_SUCCESS);

    rv = apr_crypto_get_driver(&driver, name, params, &result, pool);
    if (APR_ENOTIMPL == rv) {
        ABTS_NOT_IMPL(tc,
                apr_psprintf(pool, "Crypto driver '%s' not implemented", (char *)name));
        return NULL;
    }
    if (APR_EDSOOPEN == rv) {
        ABTS_NOT_IMPL(tc,
                apr_psprintf(pool, "Crypto driver '%s' DSO could not be opened", (char *)name));
        return NULL;
    }
    if (APR_SUCCESS != rv && result) {
        char err[1024];
        apr_strerror(rv, err, sizeof(err) - 1);
        fprintf(stderr, "get_driver error %d: %s: '%s' native error %d: %s (%s),",
                rv, err, name, result->rc, result->reason ? result->reason : "",
                result->msg ? result->msg : "");
    }
    ABTS_ASSERT(tc, apr_psprintf(pool, "failed to apr_crypto_get_driver for '%s' with %d",
                name, rv), rv == APR_SUCCESS);
    ABTS_ASSERT(tc, "apr_crypto_get_driver returned NULL", driver != NULL);
    if (!driver || rv) {
        return NULL;
    }

    return driver;

}

static const apr_crypto_driver_t *get_nss_driver(abts_case *tc,
        apr_pool_t *pool)
{

    /* initialise NSS */
    return get_driver(tc, pool, "nss", "");

}

static const apr_crypto_driver_t *get_openssl_driver(abts_case *tc,
        apr_pool_t *pool)
{

    return get_driver(tc, pool, "openssl", NULL);

}

static const apr_crypto_driver_t *get_commoncrypto_driver(abts_case *tc,
        apr_pool_t *pool)
{

    return get_driver(tc, pool, "commoncrypto", NULL);

}

static apr_crypto_t *make(abts_case *tc, apr_pool_t *pool,
        const apr_crypto_driver_t *driver)
{

    apr_crypto_t *f = NULL;

    if (!driver) {
        return NULL;
    }

    /* get the context */
    apr_crypto_make(&f, driver, "engine=openssl", pool);
    ABTS_ASSERT(tc, "apr_crypto_make returned NULL", f != NULL);

    return f;

}

static const apr_crypto_key_t *keysecret(abts_case *tc, apr_pool_t *pool,
        const apr_crypto_driver_t *driver, const apr_crypto_t *f,
        apr_crypto_block_key_type_e type, apr_crypto_block_key_mode_e mode,
        int doPad, apr_size_t secretLen, const char *description)
{
    apr_crypto_key_t *key = NULL;
    const apu_err_t *result = NULL;
    apr_crypto_key_rec_t *rec = apr_pcalloc(pool, sizeof(apr_crypto_key_rec_t));
    apr_status_t rv;

    if (!f) {
        return NULL;
    }

    rec->ktype = APR_CRYPTO_KTYPE_SECRET;
    rec->type = type;
    rec->mode = mode;
    rec->pad = doPad;
    rec->k.secret.secret = apr_pcalloc(pool, secretLen);
    rec->k.secret.secretLen = secretLen;

    /* init the passphrase */
    rv = apr_crypto_key(&key, rec, f, pool);
    if (APR_ENOCIPHER == rv) {
        apr_crypto_error(&result, f);
        ABTS_NOT_IMPL(tc,
                apr_psprintf(pool, "skipped: %s %s key return APR_ENOCIPHER: error %d: %s (%s)\n", description, apr_crypto_driver_name(driver), result->rc, result->reason ? result->reason : "", result->msg ? result->msg : ""));
        return NULL;
    }
    else {
        if (APR_SUCCESS != rv) {
            apr_crypto_error(&result, f);
            fprintf(stderr, "key: %s %s apr error %d / native error %d: %s (%s)\n",
                    description, apr_crypto_driver_name(driver), rv, result->rc,
                    result->reason ? result->reason : "",
                    result->msg ? result->msg : "");
        }
        ABTS_ASSERT(tc, "apr_crypto_key returned APR_EKEYLENGTH", rv != APR_EKEYLENGTH);
        ABTS_ASSERT(tc, "apr_crypto_key returned APR_ENOKEY", rv != APR_ENOKEY);
        ABTS_ASSERT(tc, "apr_crypto_key returned APR_EPADDING",
                rv != APR_EPADDING);
        ABTS_ASSERT(tc, "apr_crypto_key returned APR_EKEYTYPE",
                rv != APR_EKEYTYPE);
        ABTS_ASSERT(tc, "failed to apr_crypto_key", rv == APR_SUCCESS);
        ABTS_ASSERT(tc, "apr_crypto_key returned NULL context", key != NULL);
    }
    if (rv) {
        return NULL;
    }
    return key;

}

static const apr_crypto_key_t *passphrase(abts_case *tc, apr_pool_t *pool,
        const apr_crypto_driver_t *driver, const apr_crypto_t *f,
        apr_crypto_block_key_type_e type, apr_crypto_block_key_mode_e mode,
        int doPad, const char *description)
{

    apr_crypto_key_t *key = NULL;
    const apu_err_t *result = NULL;
    const char *pass = "secret";
    const char *salt = "salt";
    apr_status_t rv;

    if (!f) {
        return NULL;
    }

    /* init the passphrase */
    rv = apr_crypto_passphrase(&key, NULL, pass, strlen(pass),
            (unsigned char *) salt, strlen(salt), type, mode, doPad, 4096, f,
            pool);
    if (APR_ENOCIPHER == rv) {
        apr_crypto_error(&result, f);
        ABTS_NOT_IMPL(tc, apr_psprintf(pool,
                        "skipped: %s %s passphrase return APR_ENOCIPHER: error %d: %s (%s)\n",
                        description, apr_crypto_driver_name(driver), result->rc,
                        result->reason ? result->reason : "", result->msg ? result->msg : ""));
        return NULL;
    }
    else {
        if (APR_SUCCESS != rv) {
            apr_crypto_error(&result, f);
            fprintf(stderr, "passphrase: %s %s apr error %d / native error %d: %s (%s)\n",
                    description, apr_crypto_driver_name(driver), rv, result->rc,
                    result->reason ? result->reason : "",
                    result->msg ? result->msg : "");
        }
        ABTS_ASSERT(tc, "apr_crypto_passphrase returned APR_ENOKEY", rv != APR_ENOKEY);
        ABTS_ASSERT(tc, "apr_crypto_passphrase returned APR_EPADDING", rv != APR_EPADDING);
        ABTS_ASSERT(tc, "apr_crypto_passphrase returned APR_EKEYTYPE", rv != APR_EKEYTYPE);
        ABTS_ASSERT(tc, "failed to apr_crypto_passphrase", rv == APR_SUCCESS);
        ABTS_ASSERT(tc, "apr_crypto_passphrase returned NULL context", key != NULL);
    }
    if (rv) {
        return NULL;
    }
    return key;

}

static const apr_crypto_key_t *keypassphrase(abts_case *tc, apr_pool_t *pool,
        const apr_crypto_driver_t *driver, const apr_crypto_t *f,
        apr_crypto_block_key_type_e type, apr_crypto_block_key_mode_e mode,
        int doPad, const char *description)
{

    apr_crypto_key_t *key = NULL;
    const apu_err_t *result = NULL;
    const char *pass = "secret";
    const char *salt = "salt";
    apr_crypto_key_rec_t *rec = apr_pcalloc(pool, sizeof(apr_crypto_key_rec_t));
    apr_status_t rv;

    if (!f) {
        return NULL;
    }

    rec->ktype = APR_CRYPTO_KTYPE_PASSPHRASE;
    rec->type = type;
    rec->mode = mode;
    rec->pad = doPad;
    rec->k.passphrase.pass = pass;
    rec->k.passphrase.passLen = strlen(pass);
    rec->k.passphrase.salt = (unsigned char *)salt;
    rec->k.passphrase.saltLen = strlen(salt);
    rec->k.passphrase.iterations = 4096;

    /* init the passphrase */
    rv = apr_crypto_key(&key, rec, f, pool);
    if (APR_ENOCIPHER == rv) {
        apr_crypto_error(&result, f);
        ABTS_NOT_IMPL(tc, apr_psprintf(pool,
                        "skipped: %s %s key passphrase return APR_ENOCIPHER: error %d: %s (%s)\n",
                        description, apr_crypto_driver_name(driver), result->rc,
                        result->reason ? result->reason : "", result->msg ? result->msg : ""));
        return NULL;
    }
    else {
        if (APR_SUCCESS != rv) {
            apr_crypto_error(&result, f);
            fprintf(stderr, "key passphrase: %s %s apr error %d / native error %d: %s (%s)\n",
                    description, apr_crypto_driver_name(driver), rv, result->rc,
                    result->reason ? result->reason : "",
                    result->msg ? result->msg : "");
        }
        ABTS_ASSERT(tc, "apr_crypto_key returned APR_ENOKEY", rv != APR_ENOKEY);
        ABTS_ASSERT(tc, "apr_crypto_key returned APR_EPADDING", rv != APR_EPADDING);
        ABTS_ASSERT(tc, "apr_crypto_key returned APR_EKEYTYPE", rv != APR_EKEYTYPE);
        ABTS_ASSERT(tc, "failed to apr_crypto_key", rv == APR_SUCCESS);
        ABTS_ASSERT(tc, "apr_crypto_key returned NULL context", key != NULL);
    }
    if (rv) {
        return NULL;
    }
    return key;

}

static unsigned char *encrypt_block(abts_case *tc, apr_pool_t *pool,
        const apr_crypto_driver_t *driver, const apr_crypto_t *f,
        const apr_crypto_key_t *key, const unsigned char *in,
        const apr_size_t inlen, unsigned char **cipherText,
        apr_size_t *cipherTextLen, const unsigned char **iv,
        apr_size_t *blockSize, const char *description)
{

    apr_crypto_block_t *block = NULL;
    const apu_err_t *result = NULL;
    apr_size_t len = 0;
    apr_status_t rv;

    if (!driver || !f || !key || !in) {
        return NULL;
    }

    /* init the encryption */
    rv = apr_crypto_block_encrypt_init(&block, iv, key, blockSize, pool);
    if (APR_ENOTIMPL == rv) {
        ABTS_NOT_IMPL(tc, "apr_crypto_block_encrypt_init returned APR_ENOTIMPL");
    }
    else {
        if (APR_SUCCESS != rv) {
            apr_crypto_error(&result, f);
            fprintf(stderr,
                    "encrypt_init: %s %s (APR %d) native error %d: %s (%s)\n",
                    description, apr_crypto_driver_name(driver), rv, result->rc,
                    result->reason ? result->reason : "",
                    result->msg ? result->msg : "");
        }
        ABTS_ASSERT(tc, "apr_crypto_block_encrypt_init returned APR_ENOKEY",
                rv != APR_ENOKEY);
        ABTS_ASSERT(tc, "apr_crypto_block_encrypt_init returned APR_ENOIV",
                rv != APR_ENOIV);
        ABTS_ASSERT(tc, "apr_crypto_block_encrypt_init returned APR_EKEYTYPE",
                rv != APR_EKEYTYPE);
        ABTS_ASSERT(tc, "apr_crypto_block_encrypt_init returned APR_EKEYLENGTH",
                rv != APR_EKEYLENGTH);
        ABTS_ASSERT(tc,
                "apr_crypto_block_encrypt_init returned APR_ENOTENOUGHENTROPY",
                rv != APR_ENOTENOUGHENTROPY);
        ABTS_ASSERT(tc, "failed to apr_crypto_block_encrypt_init",
                rv == APR_SUCCESS);
        ABTS_ASSERT(tc, "apr_crypto_block_encrypt_init returned NULL context",
                block != NULL);
    }
    if (!block || rv) {
        return NULL;
    }

    /* encrypt the block */
    rv = apr_crypto_block_encrypt(cipherText, cipherTextLen, in, inlen, block);
    if (APR_SUCCESS != rv) {
        apr_crypto_error(&result, f);
        fprintf(stderr, "encrypt: %s %s (APR %d) native error %d: %s (%s)\n",
                description, apr_crypto_driver_name(driver), rv, result->rc,
                result->reason ? result->reason : "",
                result->msg ? result->msg : "");
    }
    ABTS_ASSERT(tc, "apr_crypto_block_encrypt returned APR_ECRYPT", rv != APR_ECRYPT);
    ABTS_ASSERT(tc, "failed to apr_crypto_block_encrypt", rv == APR_SUCCESS);
    ABTS_ASSERT(tc, "apr_crypto_block_encrypt failed to allocate buffer", *cipherText != NULL);
    if (rv) {
        return NULL;
    }

    /* finalise the encryption */
    rv = apr_crypto_block_encrypt_finish(*cipherText + *cipherTextLen, &len,
            block);
    if (APR_SUCCESS != rv) {
        apr_crypto_error(&result, f);
        fprintf(stderr,
                "encrypt_finish: %s %s (APR %d) native error %d: %s (%s)\n",
                description, apr_crypto_driver_name(driver), rv, result->rc,
                result->reason ? result->reason : "",
                result->msg ? result->msg : "");
    }
    ABTS_ASSERT(tc, "apr_crypto_block_encrypt_finish returned APR_ECRYPT", rv != APR_ECRYPT);
    ABTS_ASSERT(tc, "apr_crypto_block_encrypt_finish returned APR_EPADDING", rv != APR_EPADDING);
    ABTS_ASSERT(tc, "apr_crypto_block_encrypt_finish returned APR_ENOSPACE", rv != APR_ENOSPACE);
    ABTS_ASSERT(tc, "failed to apr_crypto_block_encrypt_finish", rv == APR_SUCCESS);
    *cipherTextLen += len;
    apr_crypto_block_cleanup(block);
    if (rv) {
        return NULL;
    }

    return *cipherText;

}

static unsigned char *decrypt_block(abts_case *tc, apr_pool_t *pool,
        const apr_crypto_driver_t *driver, const apr_crypto_t *f,
        const apr_crypto_key_t *key, unsigned char *cipherText,
        apr_size_t cipherTextLen, unsigned char **plainText,
        apr_size_t *plainTextLen, const unsigned char *iv,
        apr_size_t *blockSize, const char *description)
{

    apr_crypto_block_t *block = NULL;
    const apu_err_t *result = NULL;
    apr_size_t len = 0;
    apr_status_t rv;

    if (!driver || !f || !key || !cipherText) {
        return NULL;
    }

    /* init the decryption */
    rv = apr_crypto_block_decrypt_init(&block, blockSize, iv, key, pool);
    if (APR_ENOTIMPL == rv) {
        ABTS_NOT_IMPL(tc, "apr_crypto_block_decrypt_init returned APR_ENOTIMPL");
    }
    else {
        if (APR_SUCCESS != rv) {
            apr_crypto_error(&result, f);
            fprintf(stderr,
                    "decrypt_init: %s %s (APR %d) native error %d: %s (%s)\n",
                    description, apr_crypto_driver_name(driver), rv, result->rc,
                    result->reason ? result->reason : "",
                    result->msg ? result->msg : "");
        }
        ABTS_ASSERT(tc, "apr_crypto_block_decrypt_init returned APR_ENOKEY", rv != APR_ENOKEY);
        ABTS_ASSERT(tc, "apr_crypto_block_decrypt_init returned APR_ENOIV", rv != APR_ENOIV);
        ABTS_ASSERT(tc, "apr_crypto_block_decrypt_init returned APR_EKEYTYPE", rv != APR_EKEYTYPE);
        ABTS_ASSERT(tc, "apr_crypto_block_decrypt_init returned APR_EKEYLENGTH", rv != APR_EKEYLENGTH);
        ABTS_ASSERT(tc, "failed to apr_crypto_block_decrypt_init", rv == APR_SUCCESS);
        ABTS_ASSERT(tc, "apr_crypto_block_decrypt_init returned NULL context", block != NULL);
    }
    if (!block || rv) {
        return NULL;
    }

    /* decrypt the block */
    rv = apr_crypto_block_decrypt(plainText, plainTextLen, cipherText,
            cipherTextLen, block);
    if (APR_SUCCESS != rv) {
        apr_crypto_error(&result, f);
        fprintf(stderr, "decrypt: %s %s (APR %d) native error %d: %s (%s)\n",
                description, apr_crypto_driver_name(driver), rv, result->rc,
                result->reason ? result->reason : "",
                result->msg ? result->msg : "");
    }
    ABTS_ASSERT(tc, "apr_crypto_block_decrypt returned APR_ECRYPT", rv != APR_ECRYPT);
    ABTS_ASSERT(tc, "failed to apr_crypto_block_decrypt", rv == APR_SUCCESS);
    ABTS_ASSERT(tc, "apr_crypto_block_decrypt failed to allocate buffer", *plainText != NULL);
    if (rv) {
        return NULL;
    }

    /* finalise the decryption */
    rv = apr_crypto_block_decrypt_finish(*plainText + *plainTextLen, &len,
            block);
    if (APR_SUCCESS != rv) {
        apr_crypto_error(&result, f);
        fprintf(stderr,
                "decrypt_finish: %s %s (APR %d) native error %d: %s (%s)\n",
                description, apr_crypto_driver_name(driver), rv, result->rc,
                result->reason ? result->reason : "",
                result->msg ? result->msg : "");
    }
    ABTS_ASSERT(tc, "apr_crypto_block_decrypt_finish returned APR_ECRYPT", rv != APR_ECRYPT);
    ABTS_ASSERT(tc, "apr_crypto_block_decrypt_finish returned APR_EPADDING", rv != APR_EPADDING);
    ABTS_ASSERT(tc, "apr_crypto_block_decrypt_finish returned APR_ENOSPACE", rv != APR_ENOSPACE);
    ABTS_ASSERT(tc, "failed to apr_crypto_block_decrypt_finish", rv == APR_SUCCESS);
    if (rv) {
        return NULL;
    }

    *plainTextLen += len;
    apr_crypto_block_cleanup(block);

    return *plainText;

}

/**
 * Interoperability test.
 *
 * data must point at an array of two driver structures. Data will be encrypted
 * with the first driver, and decrypted with the second.
 *
 * If the two drivers interoperate, the test passes.
 */
static void crypto_block_cross(abts_case *tc, apr_pool_t *pool,
        const apr_crypto_driver_t **drivers,
        const apr_crypto_block_key_type_e type,
        const apr_crypto_block_key_mode_e mode, int doPad,
        const unsigned char *in, apr_size_t inlen, apr_size_t secretLen,
        const char *description)
{
    const apr_crypto_driver_t *driver1 = drivers[0];
    const apr_crypto_driver_t *driver2 = drivers[1];
    apr_crypto_t *f1 = NULL;
    apr_crypto_t *f2 = NULL;
    const apr_crypto_key_t *key1 = NULL;
    const apr_crypto_key_t *key2 = NULL;
    const apr_crypto_key_t *key3 = NULL;
    const apr_crypto_key_t *key4 = NULL;
    const apr_crypto_key_t *key5 = NULL;
    const apr_crypto_key_t *key6 = NULL;

    unsigned char *cipherText = NULL;
    apr_size_t cipherTextLen = 0;
    unsigned char *plainText = NULL;
    apr_size_t plainTextLen = 0;
    const unsigned char *iv = NULL;
    apr_size_t blockSize = 0;

    f1 = make(tc, pool, driver1);
    f2 = make(tc, pool, driver2);
    key1 = passphrase(tc, pool, driver1, f1, type, mode, doPad, description);
    key2 = passphrase(tc, pool, driver2, f2, type, mode, doPad, description);

    cipherText = encrypt_block(tc, pool, driver1, f1, key1, in, inlen,
            &cipherText, &cipherTextLen, &iv, &blockSize, description);
    plainText = decrypt_block(tc, pool, driver2, f2, key2, cipherText,
            cipherTextLen, &plainText, &plainTextLen, iv, &blockSize,
            description);

    if (cipherText && plainText) {
        if (memcmp(in, plainText, inlen)) {
            fprintf(stderr, "passphrase cross mismatch: %s %s/%s\n", description,
                    apr_crypto_driver_name(driver1), apr_crypto_driver_name(
                            driver2));
        }
        ABTS_STR_EQUAL(tc, (char *)in, (char *)plainText);
    }

    key3 = keysecret(tc, pool, driver1, f1, type, mode, doPad, secretLen, description);
    key4 = keysecret(tc, pool, driver2, f2, type, mode, doPad, secretLen, description);

    iv = NULL;
    blockSize = 0;
    cipherText = NULL;
    plainText = NULL;
    cipherText = encrypt_block(tc, pool, driver1, f1, key3, in, inlen,
            &cipherText, &cipherTextLen, &iv, &blockSize, description);
    plainText = decrypt_block(tc, pool, driver2, f2, key4, cipherText,
            cipherTextLen, &plainText, &plainTextLen, iv, &blockSize,
            description);

    if (cipherText && plainText) {
        if (memcmp(in, plainText, inlen)) {
            fprintf(stderr, "key secret cross mismatch: %s %s/%s\n", description,
                    apr_crypto_driver_name(driver1), apr_crypto_driver_name(
                            driver2));
        }
        ABTS_STR_EQUAL(tc, (char *)in, (char *)plainText);
    }

    key5 = keypassphrase(tc, pool, driver1, f1, type, mode, doPad, description);
    key6 = keypassphrase(tc, pool, driver2, f2, type, mode, doPad, description);

    iv = NULL;
    blockSize = 0;
    cipherText = NULL;
    plainText = NULL;
    cipherText = encrypt_block(tc, pool, driver1, f1, key5, in, inlen,
            &cipherText, &cipherTextLen, &iv, &blockSize, description);
    plainText = decrypt_block(tc, pool, driver2, f2, key6, cipherText,
            cipherTextLen, &plainText, &plainTextLen, iv, &blockSize,
            description);

    if (cipherText && plainText) {
        if (memcmp(in, plainText, inlen)) {
            fprintf(stderr, "key passphrase cross mismatch: %s %s/%s\n", description,
                    apr_crypto_driver_name(driver1), apr_crypto_driver_name(
                            driver2));
        }
        ABTS_STR_EQUAL(tc, (char *)in, (char *)plainText);
    }

}

/**
 * Test initialisation.
 */
static void test_crypto_init(abts_case *tc, void *data)
{
    apr_pool_t *pool = NULL;
    apr_status_t rv;

    apr_pool_create(&pool, NULL);

    rv = apr_crypto_init(pool);
    ABTS_ASSERT(tc, "failed to init apr_crypto", rv == APR_SUCCESS);

    apr_pool_destroy(pool);

}

/**
 * Simple test of OpenSSL key.
 */
static void test_crypto_key_openssl(abts_case *tc, void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *driver;
    apr_crypto_t *f = NULL;

    apr_pool_create(&pool, NULL);
    driver = get_openssl_driver(tc, pool);

    f = make(tc, pool, driver);
    keysecret(tc, pool, driver, f, APR_KEY_AES_256, APR_MODE_CBC, 1, 32,
            "KEY_AES_256/MODE_CBC");
    apr_pool_destroy(pool);

}

/**
 * Simple test of NSS key.
 */
static void test_crypto_key_nss(abts_case *tc, void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *driver;
    apr_crypto_t *f = NULL;

    apr_pool_create(&pool, NULL);
    driver = get_nss_driver(tc, pool);

    f = make(tc, pool, driver);
    keysecret(tc, pool, driver, f, APR_KEY_AES_256, APR_MODE_CBC, 1, 32,
            "KEY_AES_256/MODE_CBC");
    apr_pool_destroy(pool);

}

/**
 * Simple test of CommonCrypto key.
 */
static void test_crypto_key_commoncrypto(abts_case *tc, void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *driver;
    apr_crypto_t *f = NULL;

    apr_pool_create(&pool, NULL);
    driver = get_commoncrypto_driver(tc, pool);

    f = make(tc, pool, driver);
    keysecret(tc, pool, driver, f, APR_KEY_AES_256, APR_MODE_CBC, 1, 32,
            "KEY_AES_256/MODE_CBC");
    apr_pool_destroy(pool);

}

/**
 * Simple test of OpenSSL block crypt.
 */
static void test_crypto_block_openssl(abts_case *tc, void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *drivers[] = { NULL, NULL };

    const unsigned char *in = (const unsigned char *) ALIGNED_STRING;
    apr_size_t inlen = sizeof(ALIGNED_STRING);

    apr_pool_create(&pool, NULL);
    drivers[0] = get_openssl_driver(tc, pool);
    drivers[1] = get_openssl_driver(tc, pool);
    crypto_block_cross(tc, pool, drivers, APR_KEY_3DES_192, APR_MODE_CBC, 0,
            in, inlen, 24, "KEY_3DES_192/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_3DES_192, APR_MODE_ECB, 0,
            in, inlen, 24, "KEY_3DES_192/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_CBC, 0, in,
            inlen, 32, "KEY_AES_256/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_ECB, 0, in,
            inlen, 32, "KEY_AES_256/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_CBC, 0, in,
            inlen, 24, "KEY_AES_192/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_ECB, 0, in,
            inlen, 24, "KEY_AES_192/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_CBC, 0, in,
            inlen, 16, "KEY_AES_128/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_ECB, 0, in,
            inlen, 16, "KEY_AES_128/MODE_ECB");
    apr_pool_destroy(pool);

}

/**
 * Simple test of NSS block crypt.
 */
static void test_crypto_block_nss(abts_case *tc, void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *drivers[] = { NULL, NULL };

    const unsigned char *in = (const unsigned char *) ALIGNED_STRING;
    apr_size_t inlen = sizeof(ALIGNED_STRING);

    apr_pool_create(&pool, NULL);
    drivers[0] = get_nss_driver(tc, pool);
    drivers[1] = get_nss_driver(tc, pool);
    crypto_block_cross(tc, pool, drivers, APR_KEY_3DES_192, APR_MODE_CBC, 0,
            in, inlen, 24, "KEY_3DES_192/MODE_CBC");
    /* KEY_3DES_192 / MODE_ECB doesn't work on NSS */
    /* crypto_block_cross(tc, pool, drivers, KEY_3DES_192, MODE_ECB, 0, in, inlen, "KEY_3DES_192/MODE_ECB"); */
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_CBC, 0, in,
            inlen, 32, "KEY_AES_256/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_ECB, 0, in,
            inlen, 32, "KEY_AES_256/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_CBC, 0, in,
            inlen, 24, "KEY_AES_192/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_ECB, 0, in,
            inlen, 24, "KEY_AES_192/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_CBC, 0, in,
            inlen, 16, "KEY_AES_128/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_ECB, 0, in,
            inlen, 16, "KEY_AES_128/MODE_ECB");
    apr_pool_destroy(pool);

}

/**
 * Simple test of Common Crypto block crypt.
 */
static void test_crypto_block_commoncrypto(abts_case *tc, void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *drivers[] = { NULL, NULL };

    const unsigned char *in = (const unsigned char *) ALIGNED_STRING;
    apr_size_t inlen = sizeof(ALIGNED_STRING);

    apr_pool_create(&pool, NULL);
    drivers[0] = get_commoncrypto_driver(tc, pool);
    drivers[1] = get_commoncrypto_driver(tc, pool);
    crypto_block_cross(tc, pool, drivers, APR_KEY_3DES_192, APR_MODE_CBC, 0,
            in, inlen, 24, "KEY_3DES_192/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_3DES_192, APR_MODE_ECB, 0,
            in, inlen, 24, "KEY_3DES_192/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_CBC, 0, in,
            inlen, 32, "KEY_AES_256/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_ECB, 0, in,
            inlen, 32, "KEY_AES_256/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_CBC, 0, in,
            inlen, 24, "KEY_AES_192/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_ECB, 0, in,
            inlen, 24, "KEY_AES_192/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_CBC, 0, in,
            inlen, 16, "KEY_AES_128/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_ECB, 0, in,
            inlen, 16, "KEY_AES_128/MODE_ECB");
    apr_pool_destroy(pool);

}

/**
 * Encrypt NSS, decrypt OpenSSL.
 */
static void test_crypto_block_nss_openssl(abts_case *tc, void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *drivers[] = { NULL, NULL };

    const unsigned char *in = (const unsigned char *) ALIGNED_STRING;
    apr_size_t inlen = sizeof(ALIGNED_STRING);

    apr_pool_create(&pool, NULL);
    drivers[0] = get_nss_driver(tc, pool);
    drivers[1] = get_openssl_driver(tc, pool);

    crypto_block_cross(tc, pool, drivers, APR_KEY_3DES_192, APR_MODE_CBC, 0,
            in, inlen, 24, "KEY_3DES_192/MODE_CBC");

    /* KEY_3DES_192 / MODE_ECB doesn't work on NSS */
    /* crypto_block_cross(tc, pool, drivers, KEY_3DES_192, MODE_ECB, 0, in, inlen, 24, "KEY_3DES_192/MODE_ECB"); */
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_CBC, 0, in,
            inlen, 32, "KEY_AES_256/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_ECB, 0, in,
            inlen, 32, "KEY_AES_256/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_CBC, 0, in,
            inlen, 24, "KEY_AES_192/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_ECB, 0, in,
            inlen, 24, "KEY_AES_192/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_CBC, 0, in,
            inlen, 16, "KEY_AES_128/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_ECB, 0, in,
            inlen, 16, "KEY_AES_128/MODE_ECB");
    apr_pool_destroy(pool);

}

/**
 * Encrypt OpenSSL, decrypt NSS.
 */
static void test_crypto_block_openssl_nss(abts_case *tc, void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *drivers[] = { NULL, NULL };

    const unsigned char *in = (const unsigned char *) ALIGNED_STRING;
    apr_size_t inlen = sizeof(ALIGNED_STRING);

    apr_pool_create(&pool, NULL);
    drivers[0] = get_openssl_driver(tc, pool);
    drivers[1] = get_nss_driver(tc, pool);
    crypto_block_cross(tc, pool, drivers, APR_KEY_3DES_192, APR_MODE_CBC, 0,
            in, inlen, 24, "KEY_3DES_192/MODE_CBC");

    /* KEY_3DES_192 / MODE_ECB doesn't work on NSS */
    /* crypto_block_cross(tc, pool, drivers, KEY_3DES_192, MODE_ECB, 0, in, inlen, 24, "KEY_3DES_192/MODE_ECB"); */

    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_CBC, 0, in,
            inlen, 32, "KEY_AES_256/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_ECB, 0, in,
            inlen, 32, "KEY_AES_256/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_CBC, 0, in,
            inlen, 24, "KEY_AES_192/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_ECB, 0, in,
            inlen, 24, "KEY_AES_192/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_CBC, 0, in,
            inlen, 16, "KEY_AES_128/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_ECB, 0, in,
            inlen, 16, "KEY_AES_128/MODE_ECB");
    apr_pool_destroy(pool);

}

/**
 * Encrypt OpenSSL, decrypt CommonCrypto.
 */
static void test_crypto_block_openssl_commoncrypto(abts_case *tc, void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *drivers[] =
    { NULL, NULL };

    const unsigned char *in = (const unsigned char *) ALIGNED_STRING;
    apr_size_t inlen = sizeof(ALIGNED_STRING);

    apr_pool_create(&pool, NULL);
    drivers[0] = get_openssl_driver(tc, pool);
    drivers[1] = get_commoncrypto_driver(tc, pool);

    crypto_block_cross(tc, pool, drivers, APR_KEY_3DES_192, APR_MODE_CBC, 0, in,
            inlen, 24, "KEY_3DES_192/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_3DES_192, APR_MODE_ECB, 0, in,
            inlen, 24, "KEY_3DES_192/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_CBC, 0, in,
            inlen, 32, "KEY_AES_256/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_ECB, 0, in,
            inlen, 32, "KEY_AES_256/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_CBC, 0, in,
            inlen, 24, "KEY_AES_192/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_ECB, 0, in,
            inlen, 24, "KEY_AES_192/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_CBC, 0, in,
            inlen, 16, "KEY_AES_128/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_ECB, 0, in,
            inlen, 16, "KEY_AES_128/MODE_ECB");
    apr_pool_destroy(pool);

}

/**
 * Encrypt OpenSSL, decrypt CommonCrypto.
 */
static void test_crypto_block_commoncrypto_openssl(abts_case *tc, void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *drivers[] =
    { NULL, NULL };

    const unsigned char *in = (const unsigned char *) ALIGNED_STRING;
    apr_size_t inlen = sizeof(ALIGNED_STRING);

    apr_pool_create(&pool, NULL);
    drivers[0] = get_commoncrypto_driver(tc, pool);
    drivers[1] = get_openssl_driver(tc, pool);

    crypto_block_cross(tc, pool, drivers, APR_KEY_3DES_192, APR_MODE_CBC, 0, in,
            inlen, 24, "KEY_3DES_192/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_3DES_192, APR_MODE_ECB, 0, in,
            inlen, 24, "KEY_3DES_192/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_CBC, 0, in,
            inlen, 32, "KEY_AES_256/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_ECB, 0, in,
            inlen, 32, "KEY_AES_256/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_CBC, 0, in,
            inlen, 24, "KEY_AES_192/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_ECB, 0, in,
            inlen, 24, "KEY_AES_192/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_CBC, 0, in,
            inlen, 16, "KEY_AES_128/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_ECB, 0, in,
            inlen, 16, "KEY_AES_128/MODE_ECB");
    apr_pool_destroy(pool);

}

/**
 * Simple test of OpenSSL block crypt.
 */
static void test_crypto_block_openssl_pad(abts_case *tc, void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *drivers[] = { NULL, NULL };

    const unsigned char *in = (const unsigned char *) TEST_STRING;
    apr_size_t inlen = sizeof(TEST_STRING);

    apr_pool_create(&pool, NULL);
    drivers[0] = get_openssl_driver(tc, pool);
    drivers[1] = get_openssl_driver(tc, pool);

    crypto_block_cross(tc, pool, drivers, APR_KEY_3DES_192, APR_MODE_CBC, 1,
            in, inlen, 24, "KEY_3DES_192/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_3DES_192, APR_MODE_ECB, 1,
            in, inlen, 24, "KEY_3DES_192/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_CBC, 1, in,
            inlen, 32, "KEY_AES_256/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_ECB, 1, in,
            inlen, 32, "KEY_AES_256/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_CBC, 1, in,
            inlen, 24, "KEY_AES_192/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_ECB, 1, in,
            inlen, 24, "KEY_AES_192/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_CBC, 1, in,
            inlen, 16, "KEY_AES_128/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_ECB, 1, in,
            inlen, 16, "KEY_AES_128/MODE_ECB");

    apr_pool_destroy(pool);

}

/**
 * Simple test of NSS block crypt.
 */
static void test_crypto_block_nss_pad(abts_case *tc, void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *drivers[] =
    { NULL, NULL };

    const unsigned char *in = (const unsigned char *) TEST_STRING;
    apr_size_t inlen = sizeof(TEST_STRING);

    apr_pool_create(&pool, NULL);
    drivers[0] = get_nss_driver(tc, pool);
    drivers[1] = get_nss_driver(tc, pool);

    crypto_block_cross(tc, pool, drivers, APR_KEY_3DES_192, APR_MODE_CBC, 1,
            in, inlen, 24, "KEY_3DES_192/MODE_CBC");
    /* KEY_3DES_192 / MODE_ECB doesn't work on NSS */
    /* crypto_block_cross(tc, pool, drivers, KEY_3DES_192, MODE_ECB, 1, in, inlen, 24, "KEY_3DES_192/MODE_ECB"); */

    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_CBC, 1, in,
            inlen, 32, "KEY_AES_256/MODE_CBC");

    /* KEY_AES_256 / MODE_ECB doesn't support padding on NSS */
    /*crypto_block_cross(tc, pool, drivers, KEY_AES_256, MODE_ECB, 1, in, inlen, 32, "KEY_AES_256/MODE_ECB");*/

    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_CBC, 1, in,
            inlen, 24, "KEY_AES_192/MODE_CBC");

    /* KEY_AES_256 / MODE_ECB doesn't support padding on NSS */
    /*crypto_block_cross(tc, pool, drivers, KEY_AES_192, MODE_ECB, 1, in, inlen, 24, "KEY_AES_192/MODE_ECB");*/

    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_CBC, 1, in,
            inlen, 16, "KEY_AES_128/MODE_CBC");

    /* KEY_AES_256 / MODE_ECB doesn't support padding on NSS */
    /*crypto_block_cross(tc, pool, drivers, KEY_AES_128, MODE_ECB, 1, in, inlen, 16, "KEY_AES_128/MODE_ECB");*/

    apr_pool_destroy(pool);

}

/**
 * Simple test of Common Crypto block crypt.
 */
static void test_crypto_block_commoncrypto_pad(abts_case *tc, void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *drivers[] = { NULL, NULL };

    const unsigned char *in = (const unsigned char *) TEST_STRING;
    apr_size_t inlen = sizeof(TEST_STRING);

    apr_pool_create(&pool, NULL);
    drivers[0] = get_commoncrypto_driver(tc, pool);
    drivers[1] = get_commoncrypto_driver(tc, pool);

    crypto_block_cross(tc, pool, drivers, APR_KEY_3DES_192, APR_MODE_CBC, 1,
            in, inlen, 24, "KEY_3DES_192/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_3DES_192, APR_MODE_ECB, 1,
            in, inlen, 24, "KEY_3DES_192/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_CBC, 1, in,
            inlen, 32, "KEY_AES_256/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_ECB, 1, in,
            inlen, 32, "KEY_AES_256/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_CBC, 1, in,
            inlen, 24, "KEY_AES_192/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_ECB, 1, in,
            inlen, 24, "KEY_AES_192/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_CBC, 1, in,
            inlen, 16, "KEY_AES_128/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_ECB, 1, in,
            inlen, 16, "KEY_AES_128/MODE_ECB");

    apr_pool_destroy(pool);

}

/**
 * Encrypt NSS, decrypt OpenSSL.
 */
static void test_crypto_block_nss_openssl_pad(abts_case *tc, void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *drivers[] = { NULL, NULL };

    const unsigned char *in = (const unsigned char *) TEST_STRING;
    apr_size_t inlen = sizeof(TEST_STRING);

    apr_pool_create(&pool, NULL);
    drivers[0] = get_nss_driver(tc, pool);
    drivers[1] = get_openssl_driver(tc, pool);

    crypto_block_cross(tc, pool, drivers, APR_KEY_3DES_192, APR_MODE_CBC, 1,
            in, inlen, 24, "KEY_3DES_192/MODE_CBC");

    /* KEY_3DES_192 / MODE_ECB doesn't work on NSS */
    /* crypto_block_cross(tc, pool, drivers, KEY_3DES_192, MODE_ECB, 1, in, inlen, 24, "KEY_3DES_192/MODE_ECB"); */

    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_CBC, 1, in,
            inlen, 32, "KEY_AES_256/MODE_CBC");

    /* KEY_AES_256 / MODE_ECB doesn't support padding on NSS */
    /*crypto_block_cross(tc, pool, drivers, KEY_AES_256, MODE_ECB, 1, in, inlen, 32, "KEY_AES_256/MODE_ECB");*/

    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_CBC, 1, in,
            inlen, 24, "KEY_AES_192/MODE_CBC");

    /* KEY_AES_192 / MODE_ECB doesn't support padding on NSS */
    /*crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_ECB, 1, in,
            inlen, 24, "KEY_AES_192/MODE_ECB");*/

    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_CBC, 1, in,
            inlen, 16, "KEY_AES_128/MODE_CBC");

    /* KEY_AES_192 / MODE_ECB doesn't support padding on NSS */
    /*crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_ECB, 1, in,
            inlen, 16, "KEY_AES_128/MODE_ECB");*/

    apr_pool_destroy(pool);

}

/**
 * Encrypt OpenSSL, decrypt NSS.
 */
static void test_crypto_block_openssl_nss_pad(abts_case *tc, void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *drivers[] = { NULL, NULL };

    const unsigned char *in = (const unsigned char *) TEST_STRING;
    apr_size_t inlen = sizeof(TEST_STRING);

    apr_pool_create(&pool, NULL);
    drivers[0] = get_openssl_driver(tc, pool);
    drivers[1] = get_nss_driver(tc, pool);
    crypto_block_cross(tc, pool, drivers, APR_KEY_3DES_192, APR_MODE_CBC, 1,
            in, inlen, 24, "KEY_3DES_192/MODE_CBC");

    /* KEY_3DES_192 / MODE_ECB doesn't work on NSS */
    /* crypto_block_cross(tc, pool, drivers, KEY_3DES_192, MODE_ECB, 1, in, inlen, 24, "KEY_3DES_192/MODE_ECB"); */

    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_CBC, 1, in,
            inlen, 32, "KEY_AES_256/MODE_CBC");

    /* KEY_AES_256 / MODE_ECB doesn't support padding on NSS */
    /*crypto_block_cross(tc, pool, drivers, KEY_AES_256, MODE_ECB, 1, in, inlen, 32, "KEY_AES_256/MODE_ECB");*/

    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_CBC, 1, in, inlen,
            24, "KEY_AES_192/MODE_CBC");

    /* KEY_AES_192 / MODE_ECB doesn't support padding on NSS */
    /*crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_ECB, 1, in, inlen,
            24, "KEY_AES_192/MODE_ECB");*/

    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_CBC, 1, in, inlen,
            16, "KEY_AES_128/MODE_CBC");

    /* KEY_AES_128 / MODE_ECB doesn't support padding on NSS */
    /*crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_ECB, 1, in, inlen,
            16, "KEY_AES_128/MODE_ECB");*/

    apr_pool_destroy(pool);

}

/**
 * Encrypt CommonCrypto, decrypt OpenSSL.
 */
static void test_crypto_block_commoncrypto_openssl_pad(abts_case *tc,
        void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *drivers[] =
    { NULL, NULL };

    const unsigned char *in = (const unsigned char *) TEST_STRING;
    apr_size_t inlen = sizeof(TEST_STRING);

    apr_pool_create(&pool, NULL);
    drivers[0] = get_commoncrypto_driver(tc, pool);
    drivers[1] = get_openssl_driver(tc, pool);

    crypto_block_cross(tc, pool, drivers, APR_KEY_3DES_192, APR_MODE_CBC, 1, in,
            inlen, 24, "KEY_3DES_192/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_3DES_192, APR_MODE_ECB, 1, in,
            inlen, 24, "KEY_3DES_192/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_CBC, 1, in,
            inlen, 32, "KEY_AES_256/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_ECB, 1, in,
            inlen, 32, "KEY_AES_256/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_CBC, 1, in,
            inlen, 24, "KEY_AES_192/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_ECB, 1, in,
            inlen, 24, "KEY_AES_192/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_CBC, 1, in,
            inlen, 16, "KEY_AES_128/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_ECB, 1, in,
            inlen, 16, "KEY_AES_128/MODE_ECB");

    apr_pool_destroy(pool);

}

/**
 * Encrypt OpenSSL, decrypt CommonCrypto.
 */
static void test_crypto_block_openssl_commoncrypto_pad(abts_case *tc,
        void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *drivers[] =
    { NULL, NULL };

    const unsigned char *in = (const unsigned char *) TEST_STRING;
    apr_size_t inlen = sizeof(TEST_STRING);

    apr_pool_create(&pool, NULL);
    drivers[0] = get_openssl_driver(tc, pool);
    drivers[1] = get_commoncrypto_driver(tc, pool);

    crypto_block_cross(tc, pool, drivers, APR_KEY_3DES_192, APR_MODE_CBC, 1, in,
            inlen, 24, "KEY_3DES_192/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_3DES_192, APR_MODE_ECB, 1, in,
            inlen, 24, "KEY_3DES_192/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_CBC, 1, in,
            inlen, 32, "KEY_AES_256/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_256, APR_MODE_ECB, 1, in,
            inlen, 32, "KEY_AES_256/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_CBC, 1, in,
            inlen, 24, "KEY_AES_192/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_192, APR_MODE_ECB, 1, in,
            inlen, 24, "KEY_AES_192/MODE_ECB");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_CBC, 1, in,
            inlen, 16, "KEY_AES_128/MODE_CBC");
    crypto_block_cross(tc, pool, drivers, APR_KEY_AES_128, APR_MODE_ECB, 1, in,
            inlen, 16, "KEY_AES_128/MODE_ECB");

    apr_pool_destroy(pool);

}

/**
 * Get Types, OpenSSL.
 */
static void test_crypto_get_block_key_types_openssl(abts_case *tc, void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *driver;
    apr_crypto_t *f;
    apr_hash_t *types;
    int *key_3des_192;
    int *key_aes_128;
    int *key_aes_192;
    int *key_aes_256;

    apr_pool_create(&pool, NULL);
    driver = get_openssl_driver(tc, pool);
    if (driver) {

        f = make(tc, pool, driver);
        apr_crypto_get_block_key_types(&types, f);

        key_3des_192 = apr_hash_get(types, "3des192", APR_HASH_KEY_STRING);
        ABTS_PTR_NOTNULL(tc, key_3des_192);
        ABTS_INT_EQUAL(tc, *key_3des_192, APR_KEY_3DES_192);

        key_aes_128 = apr_hash_get(types, "aes128", APR_HASH_KEY_STRING);
        ABTS_PTR_NOTNULL(tc, key_aes_128);
        ABTS_INT_EQUAL(tc, *key_aes_128, APR_KEY_AES_128);

        key_aes_192 = apr_hash_get(types, "aes192", APR_HASH_KEY_STRING);
        ABTS_PTR_NOTNULL(tc, key_aes_192);
        ABTS_INT_EQUAL(tc, *key_aes_192, APR_KEY_AES_192);

        key_aes_256 = apr_hash_get(types, "aes256", APR_HASH_KEY_STRING);
        ABTS_PTR_NOTNULL(tc, key_aes_256);
        ABTS_INT_EQUAL(tc, *key_aes_256, APR_KEY_AES_256);

    }

    apr_pool_destroy(pool);

}

/**
 * Get Types, NSS.
 */
static void test_crypto_get_block_key_types_nss(abts_case *tc, void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *driver;
    apr_crypto_t *f;
    apr_hash_t *types;
    int *key_3des_192;
    int *key_aes_128;
    int *key_aes_192;
    int *key_aes_256;

    apr_pool_create(&pool, NULL);
    driver = get_nss_driver(tc, pool);
    if (driver) {

        f = make(tc, pool, driver);
        apr_crypto_get_block_key_types(&types, f);

        key_3des_192 = apr_hash_get(types, "3des192", APR_HASH_KEY_STRING);
        ABTS_PTR_NOTNULL(tc, key_3des_192);
        ABTS_INT_EQUAL(tc, *key_3des_192, APR_KEY_3DES_192);

        key_aes_128 = apr_hash_get(types, "aes128", APR_HASH_KEY_STRING);
        ABTS_PTR_NOTNULL(tc, key_aes_128);
        ABTS_INT_EQUAL(tc, *key_aes_128, APR_KEY_AES_128);

        key_aes_192 = apr_hash_get(types, "aes192", APR_HASH_KEY_STRING);
        ABTS_PTR_NOTNULL(tc, key_aes_192);
        ABTS_INT_EQUAL(tc, *key_aes_192, APR_KEY_AES_192);

        key_aes_256 = apr_hash_get(types, "aes256", APR_HASH_KEY_STRING);
        ABTS_PTR_NOTNULL(tc, key_aes_256);
        ABTS_INT_EQUAL(tc, *key_aes_256, APR_KEY_AES_256);

    }

    apr_pool_destroy(pool);

}

/**
 * Get Types, Common Crypto.
 */
static void test_crypto_get_block_key_types_commoncrypto(abts_case *tc, void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *driver;
    apr_crypto_t *f;
    apr_hash_t *types;
    int *key_3des_192;
    int *key_aes_128;
    int *key_aes_192;
    int *key_aes_256;

    apr_pool_create(&pool, NULL);
    driver = get_commoncrypto_driver(tc, pool);
    if (driver) {

        f = make(tc, pool, driver);
        apr_crypto_get_block_key_types(&types, f);

        key_3des_192 = apr_hash_get(types, "3des192", APR_HASH_KEY_STRING);
        ABTS_PTR_NOTNULL(tc, key_3des_192);
        ABTS_INT_EQUAL(tc, *key_3des_192, APR_KEY_3DES_192);

        key_aes_128 = apr_hash_get(types, "aes128", APR_HASH_KEY_STRING);
        ABTS_PTR_NOTNULL(tc, key_aes_128);
        ABTS_INT_EQUAL(tc, *key_aes_128, APR_KEY_AES_128);

        key_aes_192 = apr_hash_get(types, "aes192", APR_HASH_KEY_STRING);
        ABTS_PTR_NOTNULL(tc, key_aes_192);
        ABTS_INT_EQUAL(tc, *key_aes_192, APR_KEY_AES_192);

        key_aes_256 = apr_hash_get(types, "aes256", APR_HASH_KEY_STRING);
        ABTS_PTR_NOTNULL(tc, key_aes_256);
        ABTS_INT_EQUAL(tc, *key_aes_256, APR_KEY_AES_256);

    }

    apr_pool_destroy(pool);

}

/**
 * Get Modes, OpenSSL.
 */
static void test_crypto_get_block_key_modes_openssl(abts_case *tc, void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *driver;
    apr_crypto_t *f;
    apr_hash_t *modes;
    int *mode_ecb;
    int *mode_cbc;

    apr_pool_create(&pool, NULL);
    driver = get_openssl_driver(tc, pool);
    if (driver) {

        f = make(tc, pool, driver);
        apr_crypto_get_block_key_modes(&modes, f);

        mode_ecb = apr_hash_get(modes, "ecb", APR_HASH_KEY_STRING);
        ABTS_PTR_NOTNULL(tc, mode_ecb);
        ABTS_INT_EQUAL(tc, *mode_ecb, APR_MODE_ECB);

        mode_cbc = apr_hash_get(modes, "cbc", APR_HASH_KEY_STRING);
        ABTS_PTR_NOTNULL(tc, mode_cbc);
        ABTS_INT_EQUAL(tc, *mode_cbc, APR_MODE_CBC);

    }

    apr_pool_destroy(pool);

}

/**
 * Get Modes, NSS.
 */
static void test_crypto_get_block_key_modes_nss(abts_case *tc, void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *driver;
    apr_crypto_t *f;
    apr_hash_t *modes;
    int *mode_ecb;
    int *mode_cbc;

    apr_pool_create(&pool, NULL);
    driver = get_nss_driver(tc, pool);
    if (driver) {

        f = make(tc, pool, driver);
        apr_crypto_get_block_key_modes(&modes, f);

        mode_ecb = apr_hash_get(modes, "ecb", APR_HASH_KEY_STRING);
        ABTS_PTR_NOTNULL(tc, mode_ecb);
        ABTS_INT_EQUAL(tc, *mode_ecb, APR_MODE_ECB);

        mode_cbc = apr_hash_get(modes, "cbc", APR_HASH_KEY_STRING);
        ABTS_PTR_NOTNULL(tc, mode_cbc);
        ABTS_INT_EQUAL(tc, *mode_cbc, APR_MODE_CBC);

    }

    apr_pool_destroy(pool);

}

/**
 * Get Modes, Common Crypto.
 */
static void test_crypto_get_block_key_modes_commoncrypto(abts_case *tc, void *data)
{
    apr_pool_t *pool = NULL;
    const apr_crypto_driver_t *driver;
    apr_crypto_t *f;
    apr_hash_t *modes;
    int *mode_ecb;
    int *mode_cbc;

    apr_pool_create(&pool, NULL);
    driver = get_commoncrypto_driver(tc, pool);
    if (driver) {

        f = make(tc, pool, driver);
        apr_crypto_get_block_key_modes(&modes, f);

        mode_ecb = apr_hash_get(modes, "ecb", APR_HASH_KEY_STRING);
        ABTS_PTR_NOTNULL(tc, mode_ecb);
        ABTS_INT_EQUAL(tc, *mode_ecb, APR_MODE_ECB);

        mode_cbc = apr_hash_get(modes, "cbc", APR_HASH_KEY_STRING);
        ABTS_PTR_NOTNULL(tc, mode_cbc);
        ABTS_INT_EQUAL(tc, *mode_cbc, APR_MODE_CBC);

    }

    apr_pool_destroy(pool);

}

static void test_crypto_memzero(abts_case *tc, void *data)
{
    /* Aligned message */
    struct {
        char buf[7 * sizeof(int)];
        int untouched;
    } msg;
    /* A bit of type punning such that 'msg' might look unused
     * after the call to apr_crypto_memzero().
     */
    int *ptr = (int *)&msg;
    int i;

    /* Fill buf with non-zeros (odds) */
    for (i = 1; i < 2 * sizeof(msg.buf); i += 2) {
        msg.buf[i / 2] = (char)i;
        ABTS_ASSERT(tc, "test_crypto_memzero() barrier", msg.buf[i / 2] != 0);
    }

    /* Zero out the whole, and check it */
    apr_crypto_memzero(&msg, sizeof msg);
    for (i = 0; i < sizeof(msg) / sizeof(*ptr); ++i) {
        ABTS_ASSERT(tc, "test_crypto_memzero() optimized out", ptr[i] == 0);
    }
}

static void test_crypto_equals(abts_case *tc, void *data)
{
    /* Buffers of each type of scalar */
    union {
        char c;
        short s;
        int i;
        long l;
        float f;
        double d;
        void *p;
    } buf0[7], buf1[7], buf[7];
    char *ptr = (char *)buf;
    int i;

#define TEST_SCALAR_MATCH(i, x, r) \
    ABTS_ASSERT(tc, "test_crypto_equals(" APR_STRINGIFY(x) ")" \
                                   " != " APR_STRINGIFY(r), \
                apr_crypto_equals(&buf##r[i].x, &buf[i].x, \
                                  sizeof(buf[i].x)) == r)

    /* Fill buf with non-zeros (odds) */
    for (i = 1; i < 2 * sizeof(buf); i += 2) {
        ptr[i / 2] = (char)i;
    }
    /* Set buf1 = buf */
    memcpy(buf1, buf, sizeof buf);
    /* Set buf0 = {0} */
    memset(buf0, 0, sizeof buf0);

    /* Check that buf1 == buf for each scalar */
    TEST_SCALAR_MATCH(0, c, 1);
    TEST_SCALAR_MATCH(1, s, 1);
    TEST_SCALAR_MATCH(2, i, 1);
    TEST_SCALAR_MATCH(3, l, 1);
    TEST_SCALAR_MATCH(4, f, 1);
    TEST_SCALAR_MATCH(5, d, 1);
    TEST_SCALAR_MATCH(6, p, 1);

    /* Check that buf0 != buf for each scalar */
    TEST_SCALAR_MATCH(0, c, 0);
    TEST_SCALAR_MATCH(1, s, 0);
    TEST_SCALAR_MATCH(2, i, 0);
    TEST_SCALAR_MATCH(3, l, 0);
    TEST_SCALAR_MATCH(4, f, 0);
    TEST_SCALAR_MATCH(5, d, 0);
    TEST_SCALAR_MATCH(6, p, 0);
}

abts_suite *testcrypto(abts_suite *suite)
{
    suite = ADD_SUITE(suite);

    /* test simple init and shutdown */
    abts_run_test(suite, test_crypto_init, NULL);

    /* test key parsing - openssl */
    abts_run_test(suite, test_crypto_key_openssl, NULL);

    /* test key parsing - nss */
    abts_run_test(suite, test_crypto_key_nss, NULL);

    /* test key parsing - commoncrypto */
    abts_run_test(suite, test_crypto_key_commoncrypto, NULL);

    /* test a simple encrypt / decrypt operation - openssl */
    abts_run_test(suite, test_crypto_block_openssl, NULL);

    /* test a padded encrypt / decrypt operation - openssl */
    abts_run_test(suite, test_crypto_block_openssl_pad, NULL);

    /* test a simple encrypt / decrypt operation - nss */
    abts_run_test(suite, test_crypto_block_nss, NULL);

    /* test a padded encrypt / decrypt operation - nss */
    abts_run_test(suite, test_crypto_block_nss_pad, NULL);

    /* test a simple encrypt / decrypt operation - commoncrypto */
    abts_run_test(suite, test_crypto_block_commoncrypto, NULL);

    /* test a padded encrypt / decrypt operation - commoncrypto */
    abts_run_test(suite, test_crypto_block_commoncrypto_pad, NULL);

    /* test encrypt nss / decrypt openssl */
    abts_run_test(suite, test_crypto_block_nss_openssl, NULL);

    /* test padded encrypt nss / decrypt openssl */
    abts_run_test(suite, test_crypto_block_nss_openssl_pad, NULL);

    /* test encrypt openssl / decrypt nss */
    abts_run_test(suite, test_crypto_block_openssl_nss, NULL);

    /* test padded encrypt openssl / decrypt nss */
    abts_run_test(suite, test_crypto_block_openssl_nss_pad, NULL);

    /* test encrypt openssl / decrypt commoncrypto */
    abts_run_test(suite, test_crypto_block_openssl_commoncrypto, NULL);

    /* test padded encrypt openssl / decrypt commoncrypto */
    abts_run_test(suite, test_crypto_block_openssl_commoncrypto_pad, NULL);

    /* test encrypt commoncrypto / decrypt openssl */
    abts_run_test(suite, test_crypto_block_commoncrypto_openssl, NULL);

    /* test padded encrypt commoncrypto / decrypt openssl */
    abts_run_test(suite, test_crypto_block_commoncrypto_openssl_pad, NULL);

    /* test block key types openssl */
    abts_run_test(suite, test_crypto_get_block_key_types_openssl, NULL);

    /* test block key types nss */
    abts_run_test(suite, test_crypto_get_block_key_types_nss, NULL);

    /* test block key types commoncrypto */
    abts_run_test(suite, test_crypto_get_block_key_types_commoncrypto, NULL);

    /* test block key modes openssl */
    abts_run_test(suite, test_crypto_get_block_key_modes_openssl, NULL);

    /* test block key modes nss */
    abts_run_test(suite, test_crypto_get_block_key_modes_nss, NULL);

    /* test block key modes commoncrypto */
    abts_run_test(suite, test_crypto_get_block_key_modes_commoncrypto, NULL);

    abts_run_test(suite, test_crypto_memzero, NULL);
    abts_run_test(suite, test_crypto_equals, NULL);

    return suite;
}

#else

/**
 * Dummy test suite when crypto is turned off.
 */
abts_suite *testcrypto(abts_suite *suite)
{
    return ADD_SUITE(suite);
}

#endif /* APU_HAVE_CRYPTO */
