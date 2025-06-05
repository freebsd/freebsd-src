/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/k5-hash.h - hash table interface definitions */
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

/*
 * This file contains declarations for a simple hash table using siphash.  Some
 * limitations which might need to be addressed in the future:
 *
 * - The table does not manage caller memory.  This limitation could be
 *   addressed by adding an optional free callback to k5_hashtab_create(), to
 *   be called by k5_hashtab_free() and k5_hashtab_remove().
 *
 * - There is no way to iterate over a hash table.
 *
 * - k5_hashtab_add() does not check for duplicate entries.
 */

#ifndef K5_HASH_H
#define K5_HASH_H

#define K5_HASH_SEED_LEN 16

struct k5_hashtab;

/*
 * Create a new hash table in *ht_out.  seed must point to random bytes if keys
 * might be under the control of an attacker; otherwise it may be NULL.
 * initial_buckets controls the initial allocation of hash buckets; pass zero
 * to use a default value.  The number of hash buckets will be doubled as the
 * number of entries increases.  Return 0 on success, ENOMEM on failure.
 */
int k5_hashtab_create(const uint8_t seed[K5_HASH_SEED_LEN],
                      size_t initial_buckets, struct k5_hashtab **ht_out);

/* Release the memory used by a hash table.  Keys and values are the caller's
 * responsibility. */
void k5_hashtab_free(struct k5_hashtab *ht);

/* Add an entry to a hash table.  key and val must remain valid until the entry
 * is removed or the hash table is freed.  The caller must avoid duplicates. */
int k5_hashtab_add(struct k5_hashtab *ht, const void *key, size_t klen,
                   void *val);

/* Remove an entry from a hash table by key.  Does not free key or the
 * associated value.  Return 1 if the key was found and removed, 0 if not. */
int k5_hashtab_remove(struct k5_hashtab *ht, const void *key, size_t klen);

/* Retrieve a value from a hash table by key. */
void *k5_hashtab_get(struct k5_hashtab *ht, const void *key, size_t klen);

uint64_t k5_siphash24(const uint8_t *data, size_t len,
                      const uint8_t seed[K5_HASH_SEED_LEN]);

#endif /* K5_HASH_H */
