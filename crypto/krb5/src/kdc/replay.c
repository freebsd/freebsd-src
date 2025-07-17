/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/replay.c - Replay lookaside cache for the KDC, to avoid extra work */
/*
 * Copyright 1991 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
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
 * or implied warranty.
 */

#include "k5-int.h"
#include "k5-queue.h"
#include "k5-hashtab.h"
#include "kdc_util.h"
#include "extern.h"

#ifndef NOCACHE

struct entry {
    K5_TAILQ_ENTRY(entry) links;
    int num_hits;
    krb5_timestamp timein;
    krb5_data req_packet;
    krb5_data reply_packet;
};

#ifndef LOOKASIDE_MAX_SIZE
#define LOOKASIDE_MAX_SIZE (10 * 1024 * 1024)
#endif

K5_LIST_HEAD(entry_list, entry);
K5_TAILQ_HEAD(entry_queue, entry);

static struct k5_hashtab *hash_table;
static struct entry_queue expiration_queue;

static int hits = 0;
static int calls = 0;
static int max_hits_per_entry = 0;
static int num_entries = 0;
static size_t total_size = 0;

#define STALE_TIME      (2*60)            /* two minutes */
#define STALE(ptr, now) (ts_after(now, ts_incr((ptr)->timein, STALE_TIME)))

/* Return the rough memory footprint of an entry containing req and rep. */
static size_t
entry_size(const krb5_data *req, const krb5_data *rep)
{
    return sizeof(struct entry) + req->length +
        ((rep == NULL) ? 0 : rep->length);
}

/* Insert an entry into the cache. */
static struct entry *
insert_entry(krb5_context context, krb5_data *req, krb5_data *rep,
             krb5_timestamp time)
{
    krb5_error_code ret;
    struct entry *entry;
    size_t esize = entry_size(req, rep);

    entry = calloc(1, sizeof(*entry));
    if (entry == NULL)
        goto error;
    entry->timein = time;

    ret = krb5int_copy_data_contents(context, req, &entry->req_packet);
    if (ret)
        goto error;

    if (rep != NULL) {
        ret = krb5int_copy_data_contents(context, rep, &entry->reply_packet);
        if (ret)
            goto error;
    }

    ret = k5_hashtab_add(hash_table, entry->req_packet.data,
                         entry->req_packet.length, entry);
    if (ret)
        goto error;
    K5_TAILQ_INSERT_TAIL(&expiration_queue, entry, links);
    num_entries++;
    total_size += esize;

    return entry;

error:
    if (entry != NULL) {
        krb5_free_data_contents(context, &entry->req_packet);
        krb5_free_data_contents(context, &entry->reply_packet);
        free(entry);
    }
    return NULL;
}


/* Remove entry from its hash bucket and the expiration queue, and free it. */
static void
discard_entry(krb5_context context, struct entry *entry)
{
    total_size -= entry_size(&entry->req_packet, &entry->reply_packet);
    num_entries--;
    k5_hashtab_remove(hash_table, entry->req_packet.data,
                      entry->req_packet.length);
    K5_TAILQ_REMOVE(&expiration_queue, entry, links);
    krb5_free_data_contents(context, &entry->req_packet);
    krb5_free_data_contents(context, &entry->reply_packet);
    free(entry);
}

/* Initialize the lookaside cache structures and randomize the hash seed. */
krb5_error_code
kdc_init_lookaside(krb5_context context)
{
    krb5_error_code ret;
    uint8_t seed[K5_HASH_SEED_LEN];
    krb5_data d = make_data(seed, sizeof(seed));

    ret = krb5_c_random_make_octets(context, &d);
    if (ret)
        return ret;
    ret = k5_hashtab_create(seed, 8192, &hash_table);
    if (ret)
        return ret;
    K5_TAILQ_INIT(&expiration_queue);
    return 0;
}

/* Remove the lookaside cache entry for a packet. */
void
kdc_remove_lookaside(krb5_context kcontext, krb5_data *req_packet)
{
    struct entry *e;

    e = k5_hashtab_get(hash_table, req_packet->data, req_packet->length);
    if (e != NULL)
        discard_entry(kcontext, e);
}

/*
 * Return true and fill in reply_packet_out if req_packet is in the lookaside
 * cache; otherwise return false.
 *
 * If the request was inserted with a NULL reply_packet to indicate that a
 * request is still being processed, then return TRUE with reply_packet_out set
 * to NULL.
 */
krb5_boolean
kdc_check_lookaside(krb5_context kcontext, krb5_data *req_packet,
                    krb5_data **reply_packet_out)
{
    struct entry *e;

    *reply_packet_out = NULL;
    calls++;

    e = k5_hashtab_get(hash_table, req_packet->data, req_packet->length);
    if (e == NULL)
        return FALSE;

    e->num_hits++;
    hits++;

    /* Leave *reply_packet_out as NULL for an in-progress entry. */
    if (e->reply_packet.length == 0)
        return TRUE;

    return (krb5_copy_data(kcontext, &e->reply_packet,
                           reply_packet_out) == 0);
}

/*
 * Insert a request and reply into the lookaside cache.  Assumes it's not
 * already there, and can fail silently on memory exhaustion.  Also discard old
 * entries in the cache.
 *
 * The reply_packet may be NULL to indicate a request that is still processing.
 */
void
kdc_insert_lookaside(krb5_context kcontext, krb5_data *req_packet,
                     krb5_data *reply_packet)
{
    struct entry *e, *next;
    krb5_timestamp timenow;
    size_t esize = entry_size(req_packet, reply_packet);

    if (krb5_timeofday(kcontext, &timenow))
        return;

    /* Purge stale entries and limit the total size of the entries. */
    K5_TAILQ_FOREACH_SAFE(e, &expiration_queue, links, next) {
        if (!STALE(e, timenow) && total_size + esize <= LOOKASIDE_MAX_SIZE)
            break;
        max_hits_per_entry = max(max_hits_per_entry, e->num_hits);
        discard_entry(kcontext, e);
    }

    insert_entry(kcontext, req_packet, reply_packet, timenow);
    return;
}

/* Free all entries in the lookaside cache. */
void
kdc_free_lookaside(krb5_context kcontext)
{
    struct entry *e, *next;

    K5_TAILQ_FOREACH_SAFE(e, &expiration_queue, links, next) {
        discard_entry(kcontext, e);
    }
    k5_hashtab_free(hash_table);
}

#endif /* NOCACHE */
