/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/krb/prng_fortuna.c - Fortuna PRNG implementation */
/*
 * Copyright (c) 2005 Marko Kreen
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.      IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Copyright (C) 2010, 2011 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 *
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 *  or implied warranty.
 */

/*
 * This file implements the generator and accumulator parts of the Fortuna PRNG
 * as described in chapter 9 of _Cryptography Engineering_ by Ferguson,
 * Schneier, and Kohno.
 *
 * The generator, once seeded with an unguessable value, produces an unlimited
 * number of pseudo-random outputs which cannot be used to determine the
 * internal state of the generator (without an unreasonable amount of
 * computational power).  The generator protects against the case where the OS
 * random number generator is not cryptographically secure, but can produce an
 * unguessable initial seed.  Successive reseeds of the generator will not make
 * the internal state any more guessable than it was before.
 *
 * The accumulator is layered on top of the generator, and seeks to eventually
 * recover from the case where the OS random number generator did not produce
 * an unguessable initial seed.  Unreliable entropy inputs are collected into
 * 32 pools, which are used to reseed the generator when enough entropy has
 * been collected.  Each pool collects twice as much entropy between reseeds as
 * the previous one; eventually a reseed will occur involving a pool with
 * enough entropy that an attacker cannot maintain knowledge of the generator's
 * internal state.  The accumulator is only helpful for a long-running process
 * such as a KDC which can submit periodic entropy inputs to the PRNG.
 */

#include "crypto_int.h"

/* The accumulator's number of pools. */
#define NUM_POOLS 32

/* Minimum reseed interval in microseconds. */
#define RESEED_INTERVAL 100000  /* 0.1 sec */

/* For one big request, change the key after this many bytes. */
#define MAX_BYTES_PER_KEY (1 << 20)

/* Reseed if pool 0 has had this many bytes added since last reseed. */
#define MIN_POOL_LEN 64

/* AES-256 key size in bytes. */
#define AES256_KEYSIZE (256/8)

/* AES-256 block size in bytes. */
#define AES256_BLOCKSIZE (128/8)

/* SHA-256 block size in bytes. */
#define SHA256_BLOCKSIZE (512/8)

/* SHA-256 result size in bytes. */
#define SHA256_HASHSIZE (256/8)

/* Genarator - block cipher in CTR mode */
struct fortuna_state
{
    /* Generator state. */
    unsigned char counter[AES256_BLOCKSIZE];
    unsigned char key[AES256_KEYSIZE];
    aes_ctx ciph;

    /* Accumulator state. */
    SHA256_CTX pool[NUM_POOLS];
    unsigned int pool_index;
    unsigned int reseed_count;
    struct timeval last_reseed_time;
    unsigned int pool0_bytes;
};

/*
 * SHA[d]-256(m) is defined as SHA-256(SHA-256(0^512||m))--that is, hash a
 * block full of zeros followed by the input data, then re-hash the result.
 * These functions implement the SHA[d]-256 function on incremental inputs.
 */

static void
shad256_init(SHA256_CTX *ctx)
{
    unsigned char zero[SHA256_BLOCKSIZE];

    /* Initialize the inner SHA-256 context and update it with a zero block. */
    memset(zero, 0, sizeof(zero));
    k5_sha256_init(ctx);
    k5_sha256_update(ctx, zero, sizeof(zero));
}

static void
shad256_update(SHA256_CTX *ctx, const unsigned char *data, int len)
{
    /* Feed the input to the inner SHA-256 context. */
    k5_sha256_update(ctx, data, len);
}

static void
shad256_result(SHA256_CTX *ctx, unsigned char *dst)
{
    /* Finalize the inner context, then feed the result back through SHA256. */
    k5_sha256_final(dst, ctx);
    k5_sha256_init(ctx);
    k5_sha256_update(ctx, dst, SHA256_HASHSIZE);
    k5_sha256_final(dst, ctx);
}

/* Initialize state. */
static void
init_state(struct fortuna_state *st)
{
    unsigned int i;

    memset(st, 0, sizeof(*st));
    for (i = 0; i < NUM_POOLS; i++)
        shad256_init(&st->pool[i]);
}

/* Increment st->counter using least significant byte first. */
static void
inc_counter(struct fortuna_state *st)
{
    uint64_t val;

    val = load_64_le(st->counter) + 1;
    store_64_le(val, st->counter);
    if (val == 0) {
        val = load_64_le(st->counter + 8) + 1;
        store_64_le(val, st->counter + 8);
    }
}

/* Encrypt and increment st->counter in the current cipher context. */
static void
encrypt_counter(struct fortuna_state *st, unsigned char *dst)
{
    krb5int_aes_enc_blk(st->counter, dst, &st->ciph);
    inc_counter(st);
}

/* Reseed the generator based on hopefully non-guessable input. */
static void
generator_reseed(struct fortuna_state *st, const unsigned char *data,
                 size_t len)
{
    SHA256_CTX ctx;

    /* Calculate SHA[d]-256(key||s) and make that the new key.  Depend on the
     * SHA-256 hash size being the AES-256 key size. */
    shad256_init(&ctx);
    shad256_update(&ctx, st->key, AES256_KEYSIZE);
    shad256_update(&ctx, data, len);
    shad256_result(&ctx, st->key);
    zap(&ctx, sizeof(ctx));
    krb5int_aes_enc_key(st->key, AES256_KEYSIZE, &st->ciph);

    /* Increment counter. */
    inc_counter(st);
}

/* Generate two blocks in counter mode and replace the key with the result. */
static void
change_key(struct fortuna_state *st)
{
    encrypt_counter(st, st->key);
    encrypt_counter(st, st->key + AES256_BLOCKSIZE);
    krb5int_aes_enc_key(st->key, AES256_KEYSIZE, &st->ciph);
}

/* Output pseudo-random data from the generator. */
static void
generator_output(struct fortuna_state *st, unsigned char *dst, size_t len)
{
    unsigned char result[AES256_BLOCKSIZE];
    size_t n, count = 0;

    while (len > 0) {
        /* Produce bytes and copy the result into dst. */
        encrypt_counter(st, result);
        n = (len < AES256_BLOCKSIZE) ? len : AES256_BLOCKSIZE;
        memcpy(dst, result, n);
        dst += n;
        len -= n;

        /* Each time we reach MAX_BYTES_PER_KEY bytes, change the key. */
        count += AES256_BLOCKSIZE;
        if (count >= MAX_BYTES_PER_KEY) {
            change_key(st);
            count = 0;
        }
    }
    zap(result, sizeof(result));

    /* Change the key after each request. */
    change_key(st);
}

/* Reseed the generator using the accumulator pools. */
static void
accumulator_reseed(struct fortuna_state *st)
{
    unsigned int i, n;
    SHA256_CTX ctx;
    unsigned char hash_result[SHA256_HASHSIZE];

    n = ++st->reseed_count;

    /*
     * Collect entropy from pools.  We use the i-th pool only 1/(2^i) of the
     * time so that each pool collects twice as much entropy between uses as
     * the last.
     */
    shad256_init(&ctx);
    for (i = 0; i < NUM_POOLS; i++) {
        if (n % (1 << i) != 0)
            break;

        /* Harvest this pool's hash result into ctx, then reset the pool. */
        shad256_result(&st->pool[i], hash_result);
        shad256_init(&st->pool[i]);
        shad256_update(&ctx, hash_result, SHA256_HASHSIZE);
    }
    shad256_result(&ctx, hash_result);
    generator_reseed(st, hash_result, SHA256_HASHSIZE);
    zap(hash_result, SHA256_HASHSIZE);
    zap(&ctx, sizeof(ctx));

    /* Reset the count of bytes added to pool 0. */
    st->pool0_bytes = 0;
}

/* Add possibly unguessable data to the next accumulator pool. */
static void
accumulator_add_event(struct fortuna_state *st, const unsigned char *data,
                      size_t len)
{
    unsigned char lenbuf[2];
    SHA256_CTX *pool;

    /* Track how many bytes have been added to pool 0. */
    if (st->pool_index == 0 && st->pool0_bytes < MIN_POOL_LEN)
        st->pool0_bytes += len;

    /* Hash events into successive accumulator pools. */
    pool = &st->pool[st->pool_index];
    st->pool_index = (st->pool_index + 1) % NUM_POOLS;

    /*
     * Fortuna specifies that events are encoded with a source identifier byte,
     * a length byte, and the event data itself.  We do not have source
     * identifiers and they're not really important, so just encode the
     * length in two bytes instead.
     */
    store_16_be(len, lenbuf);
    shad256_update(pool, lenbuf, 2);
    shad256_update(pool, data, len);
}

/* Limit dependencies for test program. */
#ifndef TEST

/* Return true if RESEED_INTERVAL microseconds have passed since the last
 * reseed. */
static krb5_boolean
enough_time_passed(struct fortuna_state *st)
{
    struct timeval tv, *last = &st->last_reseed_time;
    krb5_boolean ok = FALSE;

    gettimeofday(&tv, NULL);

    /* Check how much time has passed. */
    if (tv.tv_sec > last->tv_sec + 1)
        ok = TRUE;
    else if (tv.tv_sec == last->tv_sec + 1) {
        if (1000000 + tv.tv_usec - last->tv_usec >= RESEED_INTERVAL)
            ok = TRUE;
    } else if (tv.tv_usec - last->tv_usec >= RESEED_INTERVAL)
        ok = TRUE;

    /* Update last_reseed_time if we're returning success. */
    if (ok)
        memcpy(last, &tv, sizeof(tv));

    return ok;
}

static void
accumulator_output(struct fortuna_state *st, unsigned char *dst, size_t len)
{
    /* Reseed the generator with data from pools if we have accumulated enough
     * data and enough time has passed since the last accumulator reseed. */
    if (st->pool0_bytes >= MIN_POOL_LEN && enough_time_passed(st))
        accumulator_reseed(st);

    generator_output(st, dst, len);
}

static k5_mutex_t fortuna_lock = K5_MUTEX_PARTIAL_INITIALIZER;
static struct fortuna_state main_state;
#ifdef _WIN32
static DWORD last_pid;
#else
static pid_t last_pid;
#endif
static krb5_boolean have_entropy = FALSE;

int
k5_prng_init(void)
{
    krb5_error_code ret = 0;
    unsigned char osbuf[64];

    ret = k5_mutex_finish_init(&fortuna_lock);
    if (ret)
        return ret;

    init_state(&main_state);
#ifdef _WIN32
    last_pid = GetCurrentProcessId();
#else
    last_pid = getpid();
#endif
    if (k5_get_os_entropy(osbuf, sizeof(osbuf), 0)) {
        generator_reseed(&main_state, osbuf, sizeof(osbuf));
        have_entropy = TRUE;
    }

    return 0;
}

void
k5_prng_cleanup(void)
{
    have_entropy = FALSE;
    zap(&main_state, sizeof(main_state));
    k5_mutex_destroy(&fortuna_lock);
}

krb5_error_code KRB5_CALLCONV
krb5_c_random_add_entropy(krb5_context context, unsigned int randsource,
                          const krb5_data *indata)
{
    krb5_error_code ret;

    ret = krb5int_crypto_init();
    if (ret)
        return ret;
    k5_mutex_lock(&fortuna_lock);
    if (randsource == KRB5_C_RANDSOURCE_OSRAND ||
        randsource == KRB5_C_RANDSOURCE_TRUSTEDPARTY) {
        /* These sources contain enough entropy that we should use them
         * immediately, so that they benefit the next request. */
        generator_reseed(&main_state, (unsigned char *)indata->data,
                         indata->length);
        have_entropy = TRUE;
    } else {
        /* Other sources should just go into the pools and be used according to
         * the accumulator logic. */
        accumulator_add_event(&main_state, (unsigned char *)indata->data,
                              indata->length);
    }
    k5_mutex_unlock(&fortuna_lock);
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_c_random_make_octets(krb5_context context, krb5_data *outdata)
{
#ifdef _WIN32
    DWORD pid = GetCurrentProcessId();
#else
    pid_t pid = getpid();
#endif
    unsigned char pidbuf[4];

    k5_mutex_lock(&fortuna_lock);

    if (!have_entropy) {
        k5_mutex_unlock(&fortuna_lock);
        if (context != NULL) {
            k5_set_error(&context->err, KRB5_CRYPTO_INTERNAL,
                         _("Random number generator could not be seeded"));
        }
        return KRB5_CRYPTO_INTERNAL;
    }

    if (pid != last_pid) {
        /* We forked; make sure child's PRNG stream differs from parent's. */
        store_32_be(pid, pidbuf);
        generator_reseed(&main_state, pidbuf, 4);
        last_pid = pid;
    }

    accumulator_output(&main_state, (unsigned char *)outdata->data,
                       outdata->length);
    k5_mutex_unlock(&fortuna_lock);
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_c_random_os_entropy(krb5_context context, int strong, int *success)
{
    krb5_error_code ret;
    krb5_data data;
    uint8_t buf[64];
    int status = 0;

    if (!k5_get_os_entropy(buf, sizeof(buf), strong))
        goto done;

    data = make_data(buf, sizeof(buf));
    ret = krb5_c_random_add_entropy(context, KRB5_C_RANDSOURCE_OSRAND, &data);
    if (ret)
        goto done;

    status = 1;

done:
    if (success != NULL)
        *success = status;
    return 0;
}

#endif /* not TEST */
