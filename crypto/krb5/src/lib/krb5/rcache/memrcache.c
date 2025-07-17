/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/rcache/memrcache.c - in-memory replay cache implementation */
/*
 * Copyright (C) 2019 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "k5-int.h"
#include "k5-queue.h"
#include "k5-hashtab.h"
#include "memrcache.h"

struct entry {
    K5_TAILQ_ENTRY(entry) links;
    krb5_timestamp timestamp;
    krb5_data tag;
};

K5_LIST_HEAD(entry_list, entry);
K5_TAILQ_HEAD(entry_queue, entry);

struct k5_memrcache_st {
    struct k5_hashtab *hash_table;
    struct entry_queue expiration_queue;
};

static krb5_error_code
insert_entry(krb5_context context, k5_memrcache mrc, const krb5_data *tag,
             krb5_timestamp now)
{
    krb5_error_code ret;
    struct entry *entry = NULL;

    entry = calloc(1, sizeof(*entry));
    if (entry == NULL)
        return ENOMEM;
    entry->timestamp = now;

    ret = krb5int_copy_data_contents(context, tag, &entry->tag);
    if (ret)
        goto error;

    ret = k5_hashtab_add(mrc->hash_table, entry->tag.data, entry->tag.length,
                         entry);
    if (ret)
        goto error;
    K5_TAILQ_INSERT_TAIL(&mrc->expiration_queue, entry, links);

    return 0;

error:
    if (entry != NULL) {
        krb5_free_data_contents(context, &entry->tag);
        free(entry);
    }
    return ret;
}


/* Remove entry from its hash bucket and the expiration queue, and free it. */
static void
discard_entry(krb5_context context, k5_memrcache mrc, struct entry *entry)
{
    k5_hashtab_remove(mrc->hash_table, entry->tag.data, entry->tag.length);
    K5_TAILQ_REMOVE(&mrc->expiration_queue, entry, links);
    krb5_free_data_contents(context, &entry->tag);
    free(entry);
}

/* Initialize the lookaside cache structures and randomize the hash seed. */
krb5_error_code
k5_memrcache_create(krb5_context context, k5_memrcache *mrc_out)
{
    krb5_error_code ret;
    k5_memrcache mrc;
    uint8_t seed[K5_HASH_SEED_LEN];
    krb5_data seed_data = make_data(seed, sizeof(seed));

    *mrc_out = NULL;

    ret = krb5_c_random_make_octets(context, &seed_data);
    if (ret)
        return ret;

    mrc = calloc(1, sizeof(*mrc));
    if (mrc == NULL)
        return ENOMEM;
    ret = k5_hashtab_create(seed, 64, &mrc->hash_table);
    if (ret) {
        free(mrc);
        return ret;
    }
    K5_TAILQ_INIT(&mrc->expiration_queue);

    *mrc_out = mrc;
    return 0;
}

krb5_error_code
k5_memrcache_store(krb5_context context, k5_memrcache mrc,
                   const krb5_data *tag)
{
    krb5_error_code ret;
    krb5_timestamp now;
    struct entry *e, *next;

    ret = krb5_timeofday(context, &now);
    if (ret)
        return ret;

    /* Check if we already have a matching entry. */
    e = k5_hashtab_get(mrc->hash_table, tag->data, tag->length);
    if (e != NULL)
        return KRB5KRB_AP_ERR_REPEAT;

    /* Discard stale entries. */
    K5_TAILQ_FOREACH_SAFE(e, &mrc->expiration_queue, links, next) {
        if (!ts_after(now, ts_incr(e->timestamp, context->clockskew)))
            break;
        discard_entry(context, mrc, e);
    }

    /* Add the new entry. */
    return insert_entry(context, mrc, tag, now);
}

/* Free all entries in the lookaside cache. */
void
k5_memrcache_free(krb5_context context, k5_memrcache mrc)
{
    struct entry *e, *next;

    if (mrc == NULL)
        return;
    K5_TAILQ_FOREACH_SAFE(e, &mrc->expiration_queue, links, next) {
        discard_entry(context, mrc, e);
    }
    k5_hashtab_free(mrc->hash_table);
    free(mrc);
}
