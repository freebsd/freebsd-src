/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/hash.c - hash table implementation */
/*
 * Copyright (C) 2018 by the Massachusetts Institute of Technology.
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

#include "k5-platform.h"
#include "k5-hashtab.h"
#include "k5-queue.h"

struct entry {
    const void *key;
    size_t klen;
    void *val;
    K5_SLIST_ENTRY(entry) next;
};

struct k5_hashtab {
    uint64_t k0;
    uint64_t k1;
    size_t nbuckets;
    size_t nentries;
    K5_SLIST_HEAD(bucket_list, entry) *buckets;
};

/* Return x rotated to the left by r bits. */
static inline uint64_t
rotl64(uint64_t x, int r)
{
    return (x << r) | (x >> (64 - r));
}

static inline void
sipround(uint64_t *v0, uint64_t *v1, uint64_t *v2, uint64_t *v3)
{
    *v0 += *v1;
    *v2 += *v3;
    *v1 = rotl64(*v1, 13) ^ *v0;
    *v3 = rotl64(*v3, 16) ^ *v2;
    *v0 = rotl64(*v0, 32);
    *v2 += *v1;
    *v0 += *v3;
    *v1 = rotl64(*v1, 17) ^ *v2;
    *v3 = rotl64(*v3, 21) ^ *v0;
    *v2 = rotl64(*v2, 32);
}

/* SipHash-2-4 from https://131002.net/siphash/siphash.pdf (Jean-Philippe
 * Aumasson and Daniel J. Bernstein) */
static uint64_t
siphash24(const uint8_t *data, size_t len, uint64_t k0, uint64_t k1)
{
    uint64_t v0 = k0 ^ 0x736F6D6570736575;
    uint64_t v1 = k1 ^ 0x646F72616E646F6D;
    uint64_t v2 = k0 ^ 0x6C7967656E657261;
    uint64_t v3 = k1 ^ 0x7465646279746573;
    uint64_t mi;
    const uint8_t *p, *end = data + (len - len % 8);
    uint8_t last[8] = { 0 };

    /* Process each full 8-byte chunk of data. */
    for (p = data; p < end; p += 8) {
        mi = load_64_le(p);
        v3 ^= mi;
        sipround(&v0, &v1, &v2, &v3);
        sipround(&v0, &v1, &v2, &v3);
        v0 ^= mi;
    }

    /* Process the last 0-7 bytes followed by the length mod 256. */
    memcpy(last, end, len % 8);
    last[7] = len & 0xFF;
    mi = load_64_le(last);
    v3 ^= mi;
    sipround(&v0, &v1, &v2, &v3);
    sipround(&v0, &v1, &v2, &v3);
    v0 ^= mi;

    /* Finalize. */
    v2 ^= 0xFF;
    sipround(&v0, &v1, &v2, &v3);
    sipround(&v0, &v1, &v2, &v3);
    sipround(&v0, &v1, &v2, &v3);
    sipround(&v0, &v1, &v2, &v3);
    return v0 ^ v1 ^ v2 ^ v3;
}

uint64_t
k5_siphash24(const uint8_t *data, size_t len,
             const uint8_t seed[K5_HASH_SEED_LEN])
{
    uint64_t k0 = load_64_le(seed), k1 = load_64_le(seed + 8);

    return siphash24(data, len, k0, k1);
}

int
k5_hashtab_create(const uint8_t seed[K5_HASH_SEED_LEN], size_t initial_buckets,
                  struct k5_hashtab **ht_out)
{
    struct k5_hashtab *ht;

    *ht_out = NULL;

    ht = malloc(sizeof(*ht));
    if (ht == NULL)
        return ENOMEM;

    if (seed != NULL) {
        ht->k0 = load_64_le(seed);
        ht->k1 = load_64_le(seed + 8);
    } else {
        ht->k0 = ht->k1 = 0;
    }
    ht->nbuckets = (initial_buckets > 0) ? initial_buckets : 64;
    ht->nentries = 0;
    ht->buckets = calloc(ht->nbuckets, sizeof(*ht->buckets));
    if (ht->buckets == NULL) {
        free(ht);
        return ENOMEM;
    }

    *ht_out = ht;
    return 0;
}

void
k5_hashtab_free(struct k5_hashtab *ht)
{
    size_t i;
    struct entry *ent;

    for (i = 0; i < ht->nbuckets; i++) {
        while (!K5_SLIST_EMPTY(&ht->buckets[i])) {
            ent = K5_SLIST_FIRST(&ht->buckets[i]);
            K5_SLIST_REMOVE_HEAD(&ht->buckets[i], next);
            free(ent);
        }
    }
    free(ht->buckets);
    free(ht);
}

static int
resize_table(struct k5_hashtab *ht)
{
    size_t i, j, newsize = ht->nbuckets * 2;
    struct bucket_list *newbuckets;
    struct entry *ent;

    newbuckets = calloc(newsize, sizeof(*newbuckets));
    if (newbuckets == NULL)
        return ENOMEM;

    /* Rehash all the entries into the new buckets. */
    for (i = 0; i < ht->nbuckets; i++) {
        while (!K5_SLIST_EMPTY(&ht->buckets[i])) {
            ent = K5_SLIST_FIRST(&ht->buckets[i]);
            j = siphash24(ent->key, ent->klen, ht->k0, ht->k1) % newsize;
            K5_SLIST_REMOVE_HEAD(&ht->buckets[i], next);
            K5_SLIST_INSERT_HEAD(&newbuckets[j], ent, next);
        }
    }

    free(ht->buckets);
    ht->buckets = newbuckets;
    ht->nbuckets = newsize;
    return 0;
}

int
k5_hashtab_add(struct k5_hashtab *ht, const void *key, size_t klen, void *val)
{
    size_t i;
    struct entry *ent;

    if (ht->nentries == ht->nbuckets) {
        if (resize_table(ht) != 0)
            return ENOMEM;
    }

    ent = malloc(sizeof(*ent));
    if (ent == NULL)
        return ENOMEM;
    ent->key = key;
    ent->klen = klen;
    ent->val = val;

    i = siphash24(key, klen, ht->k0, ht->k1) % ht->nbuckets;
    K5_SLIST_INSERT_HEAD(&ht->buckets[i], ent, next);

    ht->nentries++;
    return 0;
}

int
k5_hashtab_remove(struct k5_hashtab *ht, const void *key, size_t klen)
{
    size_t i;
    struct entry *ent;

    i = siphash24(key, klen, ht->k0, ht->k1) % ht->nbuckets;
    K5_SLIST_FOREACH(ent, &ht->buckets[i], next) {
        if (ent->klen == klen && memcmp(ent->key, key, klen) == 0) {
            K5_SLIST_REMOVE(&ht->buckets[i], ent, entry, next);
            free(ent);
            ht->nentries--;
            return 1;
        }
    }
    return 0;
}

void *
k5_hashtab_get(struct k5_hashtab *ht, const void *key, size_t klen)
{
    size_t i;
    struct entry *ent;

    i = siphash24(key, klen, ht->k0, ht->k1) % ht->nbuckets;
    K5_SLIST_FOREACH(ent, &ht->buckets[i], next) {
        if (ent->klen == klen && memcmp(ent->key, key, klen) == 0)
            return ent->val;
    }
    return NULL;
}
