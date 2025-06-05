/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/cc_keyring.c */
/*
 * Copyright (c) 2006
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of The University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use of distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION
 * FROM THE UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY
 * PURPOSE, AND WITHOUT WARRANTY BY THE UNIVERSITY OF
 * MICHIGAN OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING
 * WITHOUT LIMITATION THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE
 * REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE LIABLE
 * FOR ANY DAMAGES, INCLUDING SPECIAL, INDIRECT, INCIDENTAL, OR
 * CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 */
/*
 * Copyright 1990,1991,1992,1993,1994,2000,2004 Massachusetts Institute of
 * Technology.  All Rights Reserved.
 *
 * Original stdio support copyright 1995 by Cygnus Support.
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

/*
 * This file implements a collection-enabled credential cache type where the
 * credentials are stored in the Linux keyring facility.
 *
 * A residual of this type can have three forms:
 *    anchor:collection:subsidiary
 *    anchor:collection
 *    collection
 *
 * The anchor name is "process", "thread", or "legacy" and determines where we
 * search for keyring collections.  In the third form, the anchor name is
 * presumed to be "legacy".  The anchor keyring for legacy caches is the
 * session keyring.
 *
 * If the subsidiary name is present, the residual identifies a single cache
 * within a collection.  Otherwise, the residual identifies the collection
 * itself.  When a residual identifying a collection is resolved, the
 * collection's primary key is looked up (or initialized, using the collection
 * name as the subsidiary name), and the resulting cache's name will use the
 * first name form and will identify the primary cache.
 *
 * Keyring collections are named "_krb_<collection>" and are linked from the
 * anchor keyring.  The keys within a keyring collection are links to cache
 * keyrings, plus a link to one user key named "krb_ccache:primary" which
 * contains a serialized representation of the collection version (currently 1)
 * and the primary name of the collection.
 *
 * Cache keyrings contain one user key per credential which contains a
 * serialized representation of the credential.  There is also one user key
 * named "__krb5_princ__" which contains a serialized representation of the
 * cache's default principal.
 *
 * If the anchor name is "legacy", then the initial primary cache (the one
 * named with the collection name) is also linked to the session keyring, and
 * we look for a cache in that location when initializing the collection.  This
 * extra link allows that cache to be visible to old versions of the KEYRING
 * cache type, and allows us to see caches created by that code.
 */

#include "cc-int.h"

#ifdef USE_KEYRING_CCACHE

#include <errno.h>
#include <keyutils.h>

#ifdef DEBUG
#define KRCC_DEBUG          1
#endif

#if KRCC_DEBUG
void debug_print(char *fmt, ...);       /* prototype to silence warning */
#include <syslog.h>
#define DEBUG_PRINT(x) debug_print x
void
debug_print(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
#ifdef DEBUG_STDERR
    vfprintf(stderr, fmt, ap);
#else
    vsyslog(LOG_ERR, fmt, ap);
#endif
    va_end(ap);
}
#else
#define DEBUG_PRINT(x)
#endif

/*
 * We try to use the big_key key type for credentials except in legacy caches.
 * We fall back to the user key type if the kernel does not support big_key.
 * If the library doesn't support keyctl_get_persistent(), we don't even try
 * big_key since the two features were added at the same time.
 */
#ifdef HAVE_PERSISTENT_KEYRING
#define KRCC_CRED_KEY_TYPE "big_key"
#else
#define KRCC_CRED_KEY_TYPE "user"
#endif

/*
 * We use the "user" key type for collection primary names, for cache principal
 * names, and for credentials in legacy caches.
 */
#define KRCC_KEY_TYPE_USER "user"

/*
 * We create ccaches as separate keyrings
 */
#define KRCC_KEY_TYPE_KEYRING "keyring"

/*
 * Special name of the key within a ccache keyring
 * holding principal information
 */
#define KRCC_SPEC_PRINC_KEYNAME "__krb5_princ__"

/*
 * Special name for the key to communicate the name(s)
 * of credentials caches to be used for requests.
 * This should currently contain a single name, but
 * in the future may contain a list that may be
 * intelligently chosen from.
 */
#define KRCC_SPEC_CCACHE_SET_KEYNAME "__krb5_cc_set__"

/*
 * This name identifies the key containing the name of the current primary
 * cache within a collection.
 */
#define KRCC_COLLECTION_PRIMARY "krb_ccache:primary"

/*
 * If the library context does not specify a keyring collection, unique ccaches
 * will be created within this collection.
 */
#define KRCC_DEFAULT_UNIQUE_COLLECTION "session:__krb5_unique__"

/*
 * Collection keyring names begin with this prefix.  We use a prefix so that a
 * cache keyring with the collection name itself can be linked directly into
 * the anchor, for legacy session keyring compatibility.
 */
#define KRCC_CCCOL_PREFIX "_krb_"

/*
 * For the "persistent" anchor type, we look up or create this fixed keyring
 * name within the per-UID persistent keyring.
 */
#define KRCC_PERSISTENT_KEYRING_NAME "_krb"

/*
 * Name of the key holding time offsets for the individual cache
 */
#define KRCC_TIME_OFFSETS "__krb5_time_offsets__"

/*
 * Keyring name prefix and length of random name part
 */
#define KRCC_NAME_PREFIX "krb_ccache_"
#define KRCC_NAME_RAND_CHARS 8

#define KRCC_COLLECTION_VERSION 1

#define KRCC_PERSISTENT_ANCHOR "persistent"
#define KRCC_PROCESS_ANCHOR "process"
#define KRCC_THREAD_ANCHOR "thread"
#define KRCC_SESSION_ANCHOR "session"
#define KRCC_USER_ANCHOR "user"
#define KRCC_LEGACY_ANCHOR "legacy"

typedef struct _krcc_cursor
{
    int numkeys;
    int currkey;
    key_serial_t princ_id;
    key_serial_t offsets_id;
    key_serial_t *keys;
} *krcc_cursor;

/*
 * This represents a credentials cache "file"
 * where cache_id is the keyring serial number for
 * this credentials cache "file".  Each key
 * in the keyring contains a separate key.
 */
typedef struct _krcc_data
{
    char *name;                 /* Name for this credentials cache */
    k5_cc_mutex lock;           /* synchronization */
    key_serial_t collection_id; /* collection containing this cache keyring */
    key_serial_t cache_id;      /* keyring representing ccache */
    key_serial_t princ_id;      /* key holding principal info */
    krb5_boolean is_legacy_type;
} krcc_data;

/* Global mutex */
k5_cc_mutex krb5int_krcc_mutex = K5_CC_MUTEX_PARTIAL_INITIALIZER;

extern const krb5_cc_ops krb5_krcc_ops;

static const char *KRB5_CALLCONV
krcc_get_name(krb5_context context, krb5_ccache id);

static krb5_error_code KRB5_CALLCONV
krcc_start_seq_get(krb5_context, krb5_ccache id, krb5_cc_cursor *cursor);

static krb5_error_code KRB5_CALLCONV
krcc_next_cred(krb5_context context, krb5_ccache id, krb5_cc_cursor *cursor,
               krb5_creds *creds);

static krb5_error_code KRB5_CALLCONV
krcc_end_seq_get(krb5_context context, krb5_ccache id, krb5_cc_cursor *cursor);

static krb5_error_code KRB5_CALLCONV
krcc_ptcursor_free(krb5_context context, krb5_cc_ptcursor *cursor);

static krb5_error_code clear_cache_keyring(krb5_context context,
                                           krb5_ccache id);

static krb5_error_code make_krcc_data(const char *anchor_name,
                                      const char *collection_name,
                                      const char *subsidiary_name,
                                      key_serial_t cache_id, key_serial_t
                                      collection_id, krcc_data **datapp);

static krb5_error_code save_principal(krb5_context context, krb5_ccache id,
                                      krb5_principal princ);

static krb5_error_code save_time_offsets(krb5_context context, krb5_ccache id,
                                         int32_t time_offset,
                                         int32_t usec_offset);

static krb5_error_code get_time_offsets(krb5_context context, krb5_ccache id,
                                        int32_t *time_offset,
                                        int32_t *usec_offset);

/* Note the following is a stub function for Linux */
extern krb5_error_code krb5_change_cache(void);

/*
 * GET_PERSISTENT(uid) acquires the persistent keyring for uid, or falls back
 * to the user keyring if uid matches the current effective uid.
 */

static key_serial_t
get_persistent_fallback(uid_t uid)
{
    return (uid == geteuid()) ? KEY_SPEC_USER_KEYRING : -1;
}

#ifdef HAVE_PERSISTENT_KEYRING
#define GET_PERSISTENT get_persistent_real
static key_serial_t
get_persistent_real(uid_t uid)
{
    key_serial_t key;

    key = keyctl_get_persistent(uid, KEY_SPEC_PROCESS_KEYRING);
    return (key == -1 && errno == ENOTSUP) ? get_persistent_fallback(uid) :
        key;
}
#else
#define GET_PERSISTENT get_persistent_fallback
#endif

/*
 * If a process has no explicitly set session keyring, KEY_SPEC_SESSION_KEYRING
 * will resolve to the user session keyring for ID lookup and reading, but in
 * some kernel versions, writing to that special keyring will instead create a
 * new empty session keyring for the process.  We do not want that; the keys we
 * create would be invisible to other processes.  We can work around that
 * behavior by explicitly writing to the user session keyring when it matches
 * the session keyring.  This function returns the keyring we should write to
 * for the session anchor.
 */
static key_serial_t
session_write_anchor()
{
    key_serial_t s, u;

    s = keyctl_get_keyring_ID(KEY_SPEC_SESSION_KEYRING, 0);
    u = keyctl_get_keyring_ID(KEY_SPEC_USER_SESSION_KEYRING, 0);
    return (s == u) ? KEY_SPEC_USER_SESSION_KEYRING : KEY_SPEC_SESSION_KEYRING;
}

/*
 * Find or create a keyring within parent with the given name.  If possess is
 * nonzero, also make sure the key is linked from possess.  This is necessary
 * to ensure that we have possession rights on the key when the parent is the
 * user or persistent keyring.
 */
static krb5_error_code
find_or_create_keyring(key_serial_t parent, key_serial_t possess,
                       const char *name, key_serial_t *key_out)
{
    key_serial_t key;

    *key_out = -1;
    key = keyctl_search(parent, KRCC_KEY_TYPE_KEYRING, name, possess);
    if (key == -1) {
        if (possess != 0) {
            key = add_key(KRCC_KEY_TYPE_KEYRING, name, NULL, 0, possess);
            if (key == -1)
                return errno;
            if (keyctl_link(key, parent) == -1)
                return errno;
        } else {
            key = add_key(KRCC_KEY_TYPE_KEYRING, name, NULL, 0, parent);
            if (key == -1)
                return errno;
        }
    }
    *key_out = key;
    return 0;
}

/* Parse a residual name into an anchor name, a collection name, and possibly a
 * subsidiary name. */
static krb5_error_code
parse_residual(const char *residual, char **anchor_name_out,
               char **collection_name_out, char **subsidiary_name_out)
{
    krb5_error_code ret;
    char *anchor_name = NULL, *collection_name = NULL, *subsidiary_name = NULL;
    const char *sep;

    *anchor_name_out = 0;
    *collection_name_out = NULL;
    *subsidiary_name_out = NULL;

    /* Parse out the anchor name.  Use the legacy anchor if not present. */
    sep = strchr(residual, ':');
    if (sep == NULL) {
        anchor_name = strdup(KRCC_LEGACY_ANCHOR);
        if (anchor_name == NULL)
            goto oom;
    } else {
        anchor_name = k5memdup0(residual, sep - residual, &ret);
        if (anchor_name == NULL)
            goto oom;
        residual = sep + 1;
    }

    /* Parse out the collection and subsidiary name. */
    sep = strchr(residual, ':');
    if (sep == NULL) {
        collection_name = strdup(residual);
        if (collection_name == NULL)
            goto oom;
        subsidiary_name = NULL;
    } else {
        collection_name = k5memdup0(residual, sep - residual, &ret);
        if (collection_name == NULL)
            goto oom;
        subsidiary_name = strdup(sep + 1);
        if (subsidiary_name == NULL)
            goto oom;
    }

    *anchor_name_out = anchor_name;
    *collection_name_out = collection_name;
    *subsidiary_name_out = subsidiary_name;
    return 0;

oom:
    free(anchor_name);
    free(collection_name);
    free(subsidiary_name);
    return ENOMEM;
}

/*
 * Return true if residual identifies a subsidiary cache which should be linked
 * into the anchor so it can be visible to old code.  This is the case if the
 * residual has the legacy anchor and the subsidiary name matches the
 * collection name.
 */
static krb5_boolean
is_legacy_cache_name(const char *residual)
{
    const char *sep, *aname, *cname, *sname;
    size_t alen, clen, legacy_len = sizeof(KRCC_LEGACY_ANCHOR) - 1;

    /* Get pointers to the anchor, collection, and subsidiary names. */
    aname = residual;
    sep = strchr(residual, ':');
    if (sep == NULL)
        return FALSE;
    alen = sep - aname;
    cname = sep + 1;
    sep = strchr(cname, ':');
    if (sep == NULL)
        return FALSE;
    clen = sep - cname;
    sname = sep + 1;

    return alen == legacy_len && clen == strlen(sname) &&
        strncmp(aname, KRCC_LEGACY_ANCHOR, alen) == 0 &&
        strncmp(cname, sname, clen) == 0;
}

/* If the default cache name for context is a KEYRING cache, parse its residual
 * string.  Otherwise set all outputs to NULL. */
static krb5_error_code
get_default(krb5_context context, char **anchor_name_out,
            char **collection_name_out, char **subsidiary_name_out)
{
    const char *defname;

    *anchor_name_out = *collection_name_out = *subsidiary_name_out = NULL;
    defname = krb5_cc_default_name(context);
    if (defname == NULL || strncmp(defname, "KEYRING:", 8) != 0)
        return 0;
    return parse_residual(defname + 8, anchor_name_out, collection_name_out,
                          subsidiary_name_out);
}

/* Create a residual identifying a subsidiary cache. */
static krb5_error_code
make_subsidiary_residual(const char *anchor_name, const char *collection_name,
                         const char *subsidiary_name, char **residual_out)
{
    if (asprintf(residual_out, "%s:%s:%s", anchor_name, collection_name,
                 subsidiary_name) < 0) {
        *residual_out = NULL;
        return ENOMEM;
    }
    return 0;
}

/* Retrieve or create a keyring for collection_name within the anchor, and set
 * *collection_id_out to its serial number. */
static krb5_error_code
get_collection(const char *anchor_name, const char *collection_name,
               key_serial_t *collection_id_out)
{
    krb5_error_code ret;
    key_serial_t persistent_id, anchor_id, possess_id = 0;
    char *ckname, *cnend;
    long uidnum;

    *collection_id_out = 0;

    if (strcmp(anchor_name, KRCC_PERSISTENT_ANCHOR) == 0) {
        /*
         * The collection name is a uid (or empty for the current effective
         * uid), and we look up a fixed keyring name within the persistent
         * keyring for that uid.  We link it to the process keyring to ensure
         * that we have possession rights on the collection key.
         */
        if (*collection_name != '\0') {
            errno = 0;
            uidnum = strtol(collection_name, &cnend, 10);
            if (errno || *cnend != '\0')
                return KRB5_KCC_INVALID_UID;
        } else {
            uidnum = geteuid();
        }
        persistent_id = GET_PERSISTENT(uidnum);
        if (persistent_id == -1)
            return KRB5_KCC_INVALID_UID;
        return find_or_create_keyring(persistent_id, KEY_SPEC_PROCESS_KEYRING,
                                      KRCC_PERSISTENT_KEYRING_NAME,
                                      collection_id_out);
    }

    if (strcmp(anchor_name, KRCC_PROCESS_ANCHOR) == 0) {
        anchor_id = KEY_SPEC_PROCESS_KEYRING;
    } else if (strcmp(anchor_name, KRCC_THREAD_ANCHOR) == 0) {
        anchor_id = KEY_SPEC_THREAD_KEYRING;
    } else if (strcmp(anchor_name, KRCC_SESSION_ANCHOR) == 0) {
        anchor_id = session_write_anchor();
    } else if (strcmp(anchor_name, KRCC_USER_ANCHOR) == 0) {
        /* The user keyring does not confer possession, so we need to link the
         * collection to the process keyring to maintain possession rights. */
        anchor_id = KEY_SPEC_USER_KEYRING;
        possess_id = KEY_SPEC_PROCESS_KEYRING;
    } else if (strcmp(anchor_name, KRCC_LEGACY_ANCHOR) == 0) {
        anchor_id = session_write_anchor();
    } else {
        return KRB5_KCC_INVALID_ANCHOR;
    }

    /* Look up the collection keyring name within the anchor keyring. */
    if (asprintf(&ckname, "%s%s", KRCC_CCCOL_PREFIX, collection_name) == -1)
        return ENOMEM;
    ret = find_or_create_keyring(anchor_id, possess_id, ckname,
                                 collection_id_out);
    free(ckname);
    return ret;
}

/* Store subsidiary_name into the primary index key for collection_id. */
static krb5_error_code
set_primary_name(krb5_context context, key_serial_t collection_id,
                 const char *subsidiary_name)
{
    key_serial_t key;
    uint32_t len = strlen(subsidiary_name), plen = 8 + len;
    unsigned char *payload;

    payload = malloc(plen);
    if (payload == NULL)
        return ENOMEM;
    store_32_be(KRCC_COLLECTION_VERSION, payload);
    store_32_be(len, payload + 4);
    memcpy(payload + 8, subsidiary_name, len);
    key = add_key(KRCC_KEY_TYPE_USER, KRCC_COLLECTION_PRIMARY,
                  payload, plen, collection_id);
    free(payload);
    return (key == -1) ? errno : 0;
}

static krb5_error_code
parse_index(krb5_context context, int32_t *version, char **primary,
            const unsigned char *payload, size_t psize)
{
    krb5_error_code ret;
    uint32_t len;

    if (psize < 8)
        return KRB5_CC_END;

    *version = load_32_be(payload);
    len = load_32_be(payload + 4);
    if (len > psize - 8)
        return KRB5_CC_END;
    *primary = k5memdup0(payload + 8, len, &ret);
    return (*primary == NULL) ? ret : 0;
}

/*
 * Get or initialize the primary name within collection_id and set
 * *subsidiary_out to its value.  If initializing a legacy collection, look
 * for a legacy cache and add it to the collection.
 */
static krb5_error_code
get_primary_name(krb5_context context, const char *anchor_name,
                 const char *collection_name, key_serial_t collection_id,
                 char **subsidiary_out)
{
    krb5_error_code ret;
    key_serial_t primary_id, legacy;
    void *payload = NULL;
    int payloadlen;
    int32_t version;
    char *subsidiary_name = NULL;

    *subsidiary_out = NULL;

    primary_id = keyctl_search(collection_id, KRCC_KEY_TYPE_USER,
                               KRCC_COLLECTION_PRIMARY, 0);
    if (primary_id == -1) {
        /* Initialize the primary key using the collection name.  We can't name
         * a key with the empty string, so map that to an arbitrary string. */
        subsidiary_name = strdup((*collection_name == '\0') ? "tkt" :
                                 collection_name);
        if (subsidiary_name == NULL) {
            ret = ENOMEM;
            goto cleanup;
        }
        ret = set_primary_name(context, collection_id, subsidiary_name);
        if (ret)
            goto cleanup;

        if (strcmp(anchor_name, KRCC_LEGACY_ANCHOR) == 0) {
            /* Look for a cache created by old code.  If we find one, add it to
             * the collection. */
            legacy = keyctl_search(KEY_SPEC_SESSION_KEYRING,
                                   KRCC_KEY_TYPE_KEYRING, subsidiary_name, 0);
            if (legacy != -1 && keyctl_link(legacy, collection_id) == -1) {
                ret = errno;
                goto cleanup;
            }
        }
    } else {
        /* Read, parse, and free the primary key's payload. */
        payloadlen = keyctl_read_alloc(primary_id, &payload);
        if (payloadlen == -1) {
            ret = errno;
            goto cleanup;
        }
        ret = parse_index(context, &version, &subsidiary_name, payload,
                          payloadlen);
        if (ret)
            goto cleanup;

        if (version != KRCC_COLLECTION_VERSION) {
            ret = KRB5_KCC_UNKNOWN_VERSION;
            goto cleanup;
        }
    }

    *subsidiary_out = subsidiary_name;
    subsidiary_name = NULL;

cleanup:
    free(payload);
    free(subsidiary_name);
    return ret;
}

/*
 * Create a keyring with a unique random name within collection_id.  Set
 * *subsidiary to its name and *cache_id_out to its key serial number.
 */
static krb5_error_code
unique_keyring(krb5_context context, key_serial_t collection_id,
               char **subsidiary_out, key_serial_t *cache_id_out)
{
    key_serial_t key;
    krb5_error_code ret;
    char uniquename[sizeof(KRCC_NAME_PREFIX) + KRCC_NAME_RAND_CHARS];
    int prefixlen = sizeof(KRCC_NAME_PREFIX) - 1;
    int tries;

    *subsidiary_out = NULL;
    *cache_id_out = 0;

    memcpy(uniquename, KRCC_NAME_PREFIX, sizeof(KRCC_NAME_PREFIX));
    k5_cc_mutex_lock(context, &krb5int_krcc_mutex);

    /* Loop until we successfully create a new ccache keyring with
     * a unique name, or we get an error. Limit to 100 tries. */
    tries = 100;
    while (tries-- > 0) {
        ret = krb5int_random_string(context, uniquename + prefixlen,
                                    KRCC_NAME_RAND_CHARS);
        if (ret)
            goto cleanup;

        key = keyctl_search(collection_id, KRCC_KEY_TYPE_KEYRING, uniquename,
                            0);
        if (key < 0) {
            /* Name does not already exist.  Create it to reserve the name. */
            key = add_key(KRCC_KEY_TYPE_KEYRING, uniquename, NULL, 0,
                          collection_id);
            if (key < 0) {
                ret = errno;
                goto cleanup;
            }
            break;
        }
    }

    if (tries <= 0) {
        ret = KRB5_CC_BADNAME;
        goto cleanup;
    }

    *subsidiary_out = strdup(uniquename);
    if (*subsidiary_out == NULL) {
        ret = ENOMEM;
        goto cleanup;
    }
    *cache_id_out = key;
    ret = 0;
cleanup:
    k5_cc_mutex_unlock(context, &krb5int_krcc_mutex);
    return ret;
}

static krb5_error_code
add_cred_key(const char *name, const void *payload, size_t plen,
             key_serial_t cache_id, krb5_boolean legacy_type,
             key_serial_t *key_out)
{
    key_serial_t key;

    *key_out = -1;
    if (!legacy_type) {
        /* Try the preferred cred key type; fall back if no kernel support. */
        key = add_key(KRCC_CRED_KEY_TYPE, name, payload, plen, cache_id);
        if (key != -1) {
            *key_out = key;
            return 0;
        } else if (errno != EINVAL && errno != ENODEV) {
            return errno;
        }
    }
    /* Use the user key type. */
    key = add_key(KRCC_KEY_TYPE_USER, name, payload, plen, cache_id);
    if (key == -1)
        return errno;
    *key_out = key;
    return 0;
}

static void
update_keyring_expiration(krb5_context context, krb5_ccache id)
{
    krcc_data *data = id->data;
    krb5_cc_cursor cursor;
    krb5_creds creds;
    krb5_timestamp now, endtime = 0;
    unsigned int timeout;

    /*
     * We have no way to know what is the actual timeout set on the keyring.
     * We also cannot keep track of it in a local variable as another process
     * can always modify the keyring independently, so just always enumerate
     * all keys and find out the highest endtime time.
     */

    /* Find the maximum endtime of all creds in the cache. */
    if (krcc_start_seq_get(context, id, &cursor) != 0)
        return;
    for (;;) {
        if (krcc_next_cred(context, id, &cursor, &creds) != 0)
            break;
        if (ts_after(creds.times.endtime, endtime))
            endtime = creds.times.endtime;
        krb5_free_cred_contents(context, &creds);
    }
    (void)krcc_end_seq_get(context, id, &cursor);

    if (endtime == 0)        /* No creds with end times */
        return;

    if (krb5_timeofday(context, &now) != 0)
        return;

    /* Setting the timeout to zero would reset the timeout, so we set it to one
     * second instead if creds are already expired. */
    timeout = ts_after(endtime, now) ? ts_interval(now, endtime) : 1;
    (void)keyctl_set_timeout(data->cache_id, timeout);
}

/* Create or overwrite the cache keyring, and set the default principal. */
static krb5_error_code KRB5_CALLCONV
krcc_initialize(krb5_context context, krb5_ccache id, krb5_principal princ)
{
    krcc_data *data = (krcc_data *)id->data;
    krb5_os_context os_ctx = &context->os_context;
    krb5_error_code ret;
    const char *cache_name, *p;

    k5_cc_mutex_lock(context, &data->lock);

    ret = clear_cache_keyring(context, id);
    if (ret)
        goto out;

    if (!data->cache_id) {
        /* The key didn't exist at resolve time.  Check again and create the
         * key if it still isn't there. */
        p = strrchr(data->name, ':');
        cache_name = (p != NULL) ? p + 1 : data->name;
        ret = find_or_create_keyring(data->collection_id, 0, cache_name,
                                     &data->cache_id);
        if (ret)
            goto out;
    }

    /* If this is the legacy cache in a legacy session collection, link it
     * directly to the session keyring so that old code can see it. */
    if (is_legacy_cache_name(data->name))
        (void)keyctl_link(data->cache_id, session_write_anchor());

    ret = save_principal(context, id, princ);

    /* Save time offset if it is valid and this is not a legacy cache.  Legacy
     * applications would fail to parse the new key in the cache keyring. */
    if (!is_legacy_cache_name(data->name) &&
        (os_ctx->os_flags & KRB5_OS_TOFFSET_VALID)) {
        ret = save_time_offsets(context, id, os_ctx->time_offset,
                                os_ctx->usec_offset);
    }

    if (ret == 0)
        krb5_change_cache();

out:
    k5_cc_mutex_unlock(context, &data->lock);
    return ret;
}

/* Release the ccache handle. */
static krb5_error_code KRB5_CALLCONV
krcc_close(krb5_context context, krb5_ccache id)
{
    krcc_data *data = id->data;

    k5_cc_mutex_destroy(&data->lock);
    free(data->name);
    free(data);
    free(id);
    return 0;
}

/* Clear out a ccache keyring, unlinking all keys within it.  Call with the
 * mutex locked. */
static krb5_error_code
clear_cache_keyring(krb5_context context, krb5_ccache id)
{
    krcc_data *data = id->data;
    int res;

    k5_cc_mutex_assert_locked(context, &data->lock);

    DEBUG_PRINT(("clear_cache_keyring: cache_id %d, princ_id %d\n",
                 data->cache_id, data->princ_id));

    if (data->cache_id) {
        res = keyctl_clear(data->cache_id);
        if (res != 0)
            return errno;
    }
    data->princ_id = 0;

    return 0;
}

/* Destroy the cache keyring and release the handle. */
static krb5_error_code KRB5_CALLCONV
krcc_destroy(krb5_context context, krb5_ccache id)
{
    krb5_error_code ret = 0;
    krcc_data *data = id->data;
    int res;

    k5_cc_mutex_lock(context, &data->lock);

    clear_cache_keyring(context, id);
    if (data->cache_id) {
        res = keyctl_unlink(data->cache_id, data->collection_id);
        if (res < 0) {
            ret = errno;
            DEBUG_PRINT(("unlinking key %d from ring %d: %s", data->cache_id,
                         data->collection_id, error_message(errno)));
        }
        /* If this is a legacy cache, unlink it from the session anchor. */
        if (is_legacy_cache_name(data->name))
            (void)keyctl_unlink(data->cache_id, session_write_anchor());
    }

    k5_cc_mutex_unlock(context, &data->lock);
    k5_cc_mutex_destroy(&data->lock);
    free(data->name);
    free(data);
    free(id);
    krb5_change_cache();
    return ret;
}

/* Create a cache handle for a cache ID. */
static krb5_error_code
make_cache(krb5_context context, key_serial_t collection_id,
           key_serial_t cache_id, const char *anchor_name,
           const char *collection_name, const char *subsidiary_name,
           krb5_ccache *cache_out)
{
    krb5_error_code ret;
    krb5_os_context os_ctx = &context->os_context;
    krb5_ccache ccache = NULL;
    krcc_data *data;
    key_serial_t pkey = 0;

    /* Determine the key containing principal information, if present. */
    pkey = keyctl_search(cache_id, KRCC_KEY_TYPE_USER, KRCC_SPEC_PRINC_KEYNAME,
                         0);
    if (pkey < 0)
        pkey = 0;

    ccache = malloc(sizeof(struct _krb5_ccache));
    if (!ccache)
        return ENOMEM;

    ret = make_krcc_data(anchor_name, collection_name, subsidiary_name,
                         cache_id, collection_id, &data);
    if (ret) {
        free(ccache);
        return ret;
    }

    data->princ_id = pkey;
    ccache->ops = &krb5_krcc_ops;
    ccache->data = data;
    ccache->magic = KV5M_CCACHE;
    *cache_out = ccache;

    /* Look up time offsets if necessary. */
    if ((context->library_options & KRB5_LIBOPT_SYNC_KDCTIME) &&
        !(os_ctx->os_flags & KRB5_OS_TOFFSET_VALID)) {
        if (get_time_offsets(context, ccache, &os_ctx->time_offset,
                             &os_ctx->usec_offset) == 0) {
            os_ctx->os_flags &= ~KRB5_OS_TOFFSET_TIME;
            os_ctx->os_flags |= KRB5_OS_TOFFSET_VALID;
        }
    }

    return 0;
}

/* Create a keyring ccache handle for the given residual string. */
static krb5_error_code KRB5_CALLCONV
krcc_resolve(krb5_context context, krb5_ccache *id, const char *residual)
{
    krb5_error_code ret;
    key_serial_t collection_id, cache_id;
    char *anchor_name = NULL, *collection_name = NULL, *subsidiary_name = NULL;

    ret = parse_residual(residual, &anchor_name, &collection_name,
                         &subsidiary_name);
    if (ret)
        goto cleanup;
    ret = get_collection(anchor_name, collection_name, &collection_id);
    if (ret)
        goto cleanup;

    if (subsidiary_name == NULL) {
        /* Retrieve or initialize the primary name for the collection. */
        ret = get_primary_name(context, anchor_name, collection_name,
                               collection_id, &subsidiary_name);
        if (ret)
            goto cleanup;
    }

    /* Look up the cache keyring ID, if the cache is already initialized. */
    cache_id = keyctl_search(collection_id, KRCC_KEY_TYPE_KEYRING,
                             subsidiary_name, 0);
    if (cache_id < 0)
        cache_id = 0;

    ret = make_cache(context, collection_id, cache_id, anchor_name,
                     collection_name, subsidiary_name, id);
    if (ret)
        goto cleanup;

cleanup:
    free(anchor_name);
    free(collection_name);
    free(subsidiary_name);
    return ret;
}

/* Prepare for a sequential iteration over the cache keyring. */
static krb5_error_code KRB5_CALLCONV
krcc_start_seq_get(krb5_context context, krb5_ccache id,
                   krb5_cc_cursor *cursor)
{
    krcc_cursor krcursor;
    krcc_data *data = id->data;
    void *keys;
    long size;

    k5_cc_mutex_lock(context, &data->lock);

    if (!data->cache_id) {
        k5_cc_mutex_unlock(context, &data->lock);
        return KRB5_FCC_NOFILE;
    }

    size = keyctl_read_alloc(data->cache_id, &keys);
    if (size == -1) {
        DEBUG_PRINT(("Error getting from keyring: %s\n", strerror(errno)));
        k5_cc_mutex_unlock(context, &data->lock);
        return KRB5_CC_IO;
    }

    krcursor = calloc(1, sizeof(*krcursor));
    if (krcursor == NULL) {
        free(keys);
        k5_cc_mutex_unlock(context, &data->lock);
        return KRB5_CC_NOMEM;
    }

    krcursor->princ_id = data->princ_id;
    krcursor->offsets_id = keyctl_search(data->cache_id, KRCC_KEY_TYPE_USER,
                                         KRCC_TIME_OFFSETS, 0);
    krcursor->numkeys = size / sizeof(key_serial_t);
    krcursor->keys = keys;

    k5_cc_mutex_unlock(context, &data->lock);
    *cursor = krcursor;
    return 0;
}

/* Get the next credential from the cache keyring. */
static krb5_error_code KRB5_CALLCONV
krcc_next_cred(krb5_context context, krb5_ccache id, krb5_cc_cursor *cursor,
               krb5_creds *creds)
{
    krcc_cursor krcursor;
    krb5_error_code ret;
    int psize;
    void *payload = NULL;

    memset(creds, 0, sizeof(krb5_creds));

    /* The cursor has the entire list of keys. */
    krcursor = *cursor;
    if (krcursor == NULL)
        return KRB5_CC_END;

    while (krcursor->currkey < krcursor->numkeys) {
        /* If we're pointing at the entry with the principal, or at the key
         * with the time offsets, skip it. */
        if (krcursor->keys[krcursor->currkey] == krcursor->princ_id ||
            krcursor->keys[krcursor->currkey] == krcursor->offsets_id) {
            krcursor->currkey++;
            continue;
        }

        /* Read the key; the right size buffer will be allocated and
         * returned. */
        psize = keyctl_read_alloc(krcursor->keys[krcursor->currkey],
                                  &payload);
        if (psize != -1) {
            krcursor->currkey++;

            /* Unmarshal the cred using the file ccache version 4 format. */
            ret = k5_unmarshal_cred(payload, psize, 4, creds);
            free(payload);
            return ret;
        } else if (errno != ENOKEY && errno != EACCES) {
            DEBUG_PRINT(("Error reading key %d: %s\n",
                         krcursor->keys[krcursor->currkey], strerror(errno)));
            return KRB5_FCC_NOFILE;
        }

        /* The current key was unlinked, probably by a remove_cred call; move
         * on to the next one. */
        krcursor->currkey++;
    }

    /* No more keys in keyring. */
    return KRB5_CC_END;
}

/* Release an iteration cursor. */
static krb5_error_code KRB5_CALLCONV
krcc_end_seq_get(krb5_context context, krb5_ccache id, krb5_cc_cursor *cursor)
{
    krcc_cursor krcursor = *cursor;

    if (krcursor != NULL) {
        free(krcursor->keys);
        free(krcursor);
    }
    *cursor = NULL;
    return 0;
}

/* Create keyring data for a credential cache. */
static krb5_error_code
make_krcc_data(const char *anchor_name, const char *collection_name,
               const char *subsidiary_name, key_serial_t cache_id,
               key_serial_t collection_id, krcc_data **data_out)
{
    krb5_error_code ret;
    krcc_data *data;

    *data_out = NULL;

    data = malloc(sizeof(krcc_data));
    if (data == NULL)
        return KRB5_CC_NOMEM;

    ret = k5_cc_mutex_init(&data->lock);
    if (ret) {
        free(data);
        return ret;
    }

    ret = make_subsidiary_residual(anchor_name, collection_name,
                                   subsidiary_name, &data->name);
    if (ret) {
        k5_cc_mutex_destroy(&data->lock);
        free(data);
        return ret;
    }
    data->princ_id = 0;
    data->cache_id = cache_id;
    data->collection_id = collection_id;
    data->is_legacy_type = (strcmp(anchor_name, KRCC_LEGACY_ANCHOR) == 0);

    *data_out = data;
    return 0;
}

/* Create a new keyring cache with a unique name. */
static krb5_error_code KRB5_CALLCONV
krcc_generate_new(krb5_context context, krb5_ccache *id_out)
{
    krb5_ccache id = NULL;
    krb5_error_code ret;
    char *anchor_name = NULL, *collection_name = NULL, *subsidiary_name = NULL;
    char *new_subsidiary_name = NULL, *new_residual = NULL;
    krcc_data *data;
    key_serial_t collection_id;
    key_serial_t cache_id = 0;

    *id_out = NULL;

    /* Determine the collection in which we will create the cache.*/
    ret = get_default(context, &anchor_name, &collection_name,
                      &subsidiary_name);
    if (ret)
        return ret;
    if (anchor_name == NULL) {
        ret = parse_residual(KRCC_DEFAULT_UNIQUE_COLLECTION, &anchor_name,
                             &collection_name, &subsidiary_name);
        if (ret)
            return ret;
    }
    if (subsidiary_name != NULL) {
        k5_setmsg(context, KRB5_DCC_CANNOT_CREATE,
                  _("Can't create new subsidiary cache because default cache "
                    "is already a subsidiary"));
        ret = KRB5_DCC_CANNOT_CREATE;
        goto cleanup;
    }

    /* Allocate memory */
    id = malloc(sizeof(struct _krb5_ccache));
    if (id == NULL) {
        ret = ENOMEM;
        goto cleanup;
    }

    id->ops = &krb5_krcc_ops;

    /* Make a unique keyring within the chosen collection. */
    ret = get_collection(anchor_name, collection_name, &collection_id);
    if (ret)
        goto cleanup;
    ret = unique_keyring(context, collection_id, &new_subsidiary_name,
                         &cache_id);
    if (ret)
        goto cleanup;

    ret = make_krcc_data(anchor_name, collection_name, new_subsidiary_name,
                         cache_id, collection_id, &data);
    if (ret)
        goto cleanup;

    id->data = data;
    krb5_change_cache();

cleanup:
    free(anchor_name);
    free(collection_name);
    free(subsidiary_name);
    free(new_subsidiary_name);
    free(new_residual);
    if (ret) {
        free(id);
        return ret;
    }
    *id_out = id;
    return 0;
}

/* Return an alias to the residual string of the cache. */
static const char *KRB5_CALLCONV
krcc_get_name(krb5_context context, krb5_ccache id)
{
    return ((krcc_data *)id->data)->name;
}

/* Retrieve a copy of the default principal, if the cache is initialized. */
static krb5_error_code KRB5_CALLCONV
krcc_get_principal(krb5_context context, krb5_ccache id,
                   krb5_principal *princ_out)
{
    krcc_data *data = id->data;
    krb5_error_code ret;
    void *payload = NULL;
    int psize;

    *princ_out = NULL;
    k5_cc_mutex_lock(context, &data->lock);

    if (!data->cache_id || !data->princ_id) {
        ret = KRB5_FCC_NOFILE;
        k5_setmsg(context, ret, _("Credentials cache keyring '%s' not found"),
                  data->name);
        goto errout;
    }

    psize = keyctl_read_alloc(data->princ_id, &payload);
    if (psize == -1) {
        DEBUG_PRINT(("Reading principal key %d: %s\n",
                     data->princ_id, strerror(errno)));
        ret = KRB5_CC_IO;
        goto errout;
    }

    /* Unmarshal the principal using the file ccache version 4 format. */
    ret = k5_unmarshal_princ(payload, psize, 4, princ_out);

errout:
    free(payload);
    k5_cc_mutex_unlock(context, &data->lock);
    return ret;
}

/* Search for a credential within the cache keyring. */
static krb5_error_code KRB5_CALLCONV
krcc_retrieve(krb5_context context, krb5_ccache id,
              krb5_flags whichfields, krb5_creds *mcreds,
              krb5_creds *creds)
{
    return k5_cc_retrieve_cred_default(context, id, whichfields, mcreds,
                                       creds);
}

/* Remove a credential from the cache keyring. */
static krb5_error_code KRB5_CALLCONV
krcc_remove_cred(krb5_context context, krb5_ccache cache,
                 krb5_flags flags, krb5_creds *creds)
{
    krb5_error_code ret;
    krcc_data *data = cache->data;
    krb5_cc_cursor cursor;
    krb5_creds c;
    krcc_cursor krcursor;
    key_serial_t key;
    krb5_boolean match;

    ret = krcc_start_seq_get(context, cache, &cursor);
    if (ret)
        return ret;

    for (;;) {
        ret = krcc_next_cred(context, cache, &cursor, &c);
        if (ret)
            break;
        match = krb5int_cc_creds_match_request(context, flags, creds, &c);
        krb5_free_cred_contents(context, &c);
        if (match) {
            krcursor = cursor;
            key = krcursor->keys[krcursor->currkey - 1];
            if (keyctl_unlink(key, data->cache_id) == -1) {
                ret = errno;
                break;
            }
        }
    }

    krcc_end_seq_get(context, cache, &cursor);
    return (ret == KRB5_CC_END) ? 0 : ret;
}

/* Set flags on the cache.  (We don't care about any flags.) */
static krb5_error_code KRB5_CALLCONV
krcc_set_flags(krb5_context context, krb5_ccache id, krb5_flags flags)
{
    return 0;
}

/* Get the current operational flags (of which we have none) for the cache. */
static krb5_error_code KRB5_CALLCONV
krcc_get_flags(krb5_context context, krb5_ccache id, krb5_flags *flags_out)
{
    *flags_out = 0;
    return 0;
}

/* Store a credential in the cache keyring. */
static krb5_error_code KRB5_CALLCONV
krcc_store(krb5_context context, krb5_ccache id, krb5_creds *creds)
{
    krb5_error_code ret;
    krcc_data *data = id->data;
    struct k5buf buf = EMPTY_K5BUF;
    char *keyname = NULL;
    key_serial_t cred_key;
    krb5_timestamp now;

    k5_cc_mutex_lock(context, &data->lock);

    if (!data->cache_id) {
        k5_cc_mutex_unlock(context, &data->lock);
        return KRB5_FCC_NOFILE;
    }

    /* Get the service principal name and use it as the key name */
    ret = krb5_unparse_name(context, creds->server, &keyname);
    if (ret)
        goto errout;

    /* Serialize credential using the file ccache version 4 format. */
    k5_buf_init_dynamic_zap(&buf);
    k5_marshal_cred(&buf, 4, creds);
    ret = k5_buf_status(&buf);
    if (ret)
        goto errout;

    /* Add new key (credentials) into keyring */
    DEBUG_PRINT(("krcc_store: adding new key '%s' to keyring %d\n",
                 keyname, data->cache_id));
    ret = add_cred_key(keyname, buf.data, buf.len, data->cache_id,
                       data->is_legacy_type, &cred_key);
    if (ret)
        goto errout;

    /* Set appropriate timeouts on cache keys. */
    ret = krb5_timeofday(context, &now);
    if (ret)
        goto errout;

    if (ts_after(creds->times.endtime, now)) {
        (void)keyctl_set_timeout(cred_key,
                                 ts_interval(now, creds->times.endtime));
    }

    update_keyring_expiration(context, id);

errout:
    k5_buf_free(&buf);
    krb5_free_unparsed_name(context, keyname);
    k5_cc_mutex_unlock(context, &data->lock);
    return ret;
}

/* Lock the cache handle against other threads.  (This does not lock the cache
 * keyring against other processes.) */
static krb5_error_code KRB5_CALLCONV
krcc_lock(krb5_context context, krb5_ccache id)
{
    krcc_data *data = id->data;

    k5_cc_mutex_lock(context, &data->lock);
    return 0;
}

/* Unlock the cache handle. */
static krb5_error_code KRB5_CALLCONV
krcc_unlock(krb5_context context, krb5_ccache id)
{
    krcc_data *data = id->data;

    k5_cc_mutex_unlock(context, &data->lock);
    return 0;
}

static krb5_error_code
save_principal(krb5_context context, krb5_ccache id, krb5_principal princ)
{
    krcc_data *data = id->data;
    krb5_error_code ret;
    struct k5buf buf;
    key_serial_t newkey;

    k5_cc_mutex_assert_locked(context, &data->lock);

    /* Serialize princ using the file ccache version 4 format. */
    k5_buf_init_dynamic(&buf);
    k5_marshal_princ(&buf, 4, princ);
    if (k5_buf_status(&buf) != 0)
        return ENOMEM;

    /* Add new key into keyring */
#ifdef KRCC_DEBUG
    {
        krb5_error_code rc;
        char *princname = NULL;
        rc = krb5_unparse_name(context, princ, &princname);
        DEBUG_PRINT(("save_principal: adding new key '%s' "
                     "to keyring %d for principal '%s'\n",
                     KRCC_SPEC_PRINC_KEYNAME, data->cache_id,
                     rc ? "<unknown>" : princname));
        if (rc == 0)
            krb5_free_unparsed_name(context, princname);
    }
#endif
    newkey = add_key(KRCC_KEY_TYPE_USER, KRCC_SPEC_PRINC_KEYNAME, buf.data,
                     buf.len, data->cache_id);
    if (newkey < 0) {
        ret = errno;
        DEBUG_PRINT(("Error adding principal key: %s\n", strerror(ret)));
    } else {
        data->princ_id = newkey;
        ret = 0;
    }

    k5_buf_free(&buf);
    return ret;
}

/* Add a key to the cache keyring containing the given time offsets. */
static krb5_error_code
save_time_offsets(krb5_context context, krb5_ccache id, int32_t time_offset,
                  int32_t usec_offset)
{
    krcc_data *data = id->data;
    key_serial_t newkey;
    unsigned char payload[8];

    k5_cc_mutex_assert_locked(context, &data->lock);

    /* Prepare the payload. */
    store_32_be(time_offset, payload);
    store_32_be(usec_offset, payload + 4);

    /* Add new key into keyring. */
    newkey = add_key(KRCC_KEY_TYPE_USER, KRCC_TIME_OFFSETS, payload, 8,
                     data->cache_id);
    if (newkey == -1)
        return errno;
    return 0;
}

/* Retrieve and parse the key in the cache keyring containing time offsets. */
static krb5_error_code
get_time_offsets(krb5_context context, krb5_ccache id, int32_t *time_offset,
                 int32_t *usec_offset)
{
    krcc_data *data = id->data;
    krb5_error_code ret = 0;
    key_serial_t key;
    void *payload = NULL;
    int psize;

    k5_cc_mutex_lock(context, &data->lock);

    if (!data->cache_id) {
        ret = KRB5_FCC_NOFILE;
        goto errout;
    }

    key = keyctl_search(data->cache_id, KRCC_KEY_TYPE_USER, KRCC_TIME_OFFSETS,
                        0);
    if (key == -1) {
        ret = ENOENT;
        goto errout;
    }

    psize = keyctl_read_alloc(key, &payload);
    if (psize == -1) {
        DEBUG_PRINT(("Reading time offsets key %d: %s\n",
                     key, strerror(errno)));
        ret = KRB5_CC_IO;
        goto errout;
    }

    if (psize < 8) {
        ret = KRB5_CC_END;
        goto errout;
    }
    *time_offset = load_32_be(payload);
    *usec_offset = load_32_be((char *)payload + 4);

errout:
    free(payload);
    k5_cc_mutex_unlock(context, &data->lock);
    return ret;
}

struct krcc_ptcursor_data {
    key_serial_t collection_id;
    char *anchor_name;
    char *collection_name;
    char *subsidiary_name;
    char *primary_name;
    krb5_boolean first;
    long num_keys;
    long next_key;
    key_serial_t *keys;
};

static krb5_error_code KRB5_CALLCONV
krcc_ptcursor_new(krb5_context context, krb5_cc_ptcursor *cursor_out)
{
    struct krcc_ptcursor_data *ptd;
    krb5_cc_ptcursor cursor;
    krb5_error_code ret;
    void *keys;
    long size;

    *cursor_out = NULL;

    cursor = k5alloc(sizeof(*cursor), &ret);
    if (cursor == NULL)
        return ENOMEM;
    ptd = k5alloc(sizeof(*ptd), &ret);
    if (ptd == NULL)
        goto error;
    cursor->ops = &krb5_krcc_ops;
    cursor->data = ptd;
    ptd->first = TRUE;

    ret = get_default(context, &ptd->anchor_name, &ptd->collection_name,
                      &ptd->subsidiary_name);
    if (ret)
        goto error;

    /* If there is no default collection, return an empty cursor. */
    if (ptd->anchor_name == NULL) {
        *cursor_out = cursor;
        return 0;
    }

    ret = get_collection(ptd->anchor_name, ptd->collection_name,
                         &ptd->collection_id);
    if (ret)
        goto error;

    if (ptd->subsidiary_name == NULL) {
        ret = get_primary_name(context, ptd->anchor_name,
                               ptd->collection_name, ptd->collection_id,
                               &ptd->primary_name);
        if (ret)
            goto error;

        size = keyctl_read_alloc(ptd->collection_id, &keys);
        if (size == -1) {
            ret = errno;
            goto error;
        }
        ptd->keys = keys;
        ptd->num_keys = size / sizeof(key_serial_t);
    }

    *cursor_out = cursor;
    return 0;

error:
    krcc_ptcursor_free(context, &cursor);
    return ret;
}

static krb5_error_code KRB5_CALLCONV
krcc_ptcursor_next(krb5_context context, krb5_cc_ptcursor cursor,
                   krb5_ccache *cache_out)
{
    krb5_error_code ret;
    struct krcc_ptcursor_data *ptd = cursor->data;
    key_serial_t key, cache_id = 0;
    const char *first_name, *keytype, *sep, *subsidiary_name;
    size_t keytypelen;
    char *description = NULL;

    *cache_out = NULL;

    /* No keyring available */
    if (ptd->collection_id == 0)
        return 0;

    if (ptd->first) {
        /* Look for the primary cache for a collection cursor, or the
         * subsidiary cache for a subsidiary cursor. */
        ptd->first = FALSE;
        first_name = (ptd->primary_name != NULL) ? ptd->primary_name :
            ptd->subsidiary_name;
        cache_id = keyctl_search(ptd->collection_id, KRCC_KEY_TYPE_KEYRING,
                                 first_name, 0);
        if (cache_id != -1) {
            return make_cache(context, ptd->collection_id, cache_id,
                              ptd->anchor_name, ptd->collection_name,
                              first_name, cache_out);
        }
    }

    /* A subsidiary cursor yields at most the first cache. */
    if (ptd->subsidiary_name != NULL)
        return 0;

    keytype = KRCC_KEY_TYPE_KEYRING ";";
    keytypelen = strlen(keytype);

    for (; ptd->next_key < ptd->num_keys; ptd->next_key++) {
        /* Free any previously retrieved key description. */
        free(description);
        description = NULL;

        /*
         * Get the key description, which should have the form:
         *   typename;UID;GID;permissions;description
         */
        key = ptd->keys[ptd->next_key];
        if (keyctl_describe_alloc(key, &description) < 0)
            continue;
        sep = strrchr(description, ';');
        if (sep == NULL)
            continue;
        subsidiary_name = sep + 1;

        /* Skip this key if it isn't a keyring. */
        if (strncmp(description, keytype, keytypelen) != 0)
            continue;

        /* Don't repeat the primary cache. */
        if (strcmp(subsidiary_name, ptd->primary_name) == 0)
            continue;

        /* We found a valid key */
        ptd->next_key++;
        ret = make_cache(context, ptd->collection_id, key, ptd->anchor_name,
                         ptd->collection_name, subsidiary_name, cache_out);
        free(description);
        return ret;
    }

    free(description);
    return 0;
}

static krb5_error_code KRB5_CALLCONV
krcc_ptcursor_free(krb5_context context, krb5_cc_ptcursor *cursor)
{
    struct krcc_ptcursor_data *ptd = (*cursor)->data;

    if (ptd != NULL) {
        free(ptd->anchor_name);
        free(ptd->collection_name);
        free(ptd->subsidiary_name);
        free(ptd->primary_name);
        free(ptd->keys);
        free(ptd);
    }
    free(*cursor);
    *cursor = NULL;
    return 0;
}

static krb5_error_code KRB5_CALLCONV
krcc_switch_to(krb5_context context, krb5_ccache cache)
{
    krcc_data *data = cache->data;
    krb5_error_code ret;
    char *anchor_name = NULL, *collection_name = NULL, *subsidiary_name = NULL;
    key_serial_t collection_id;

    ret = parse_residual(data->name, &anchor_name, &collection_name,
                         &subsidiary_name);
    if (ret)
        goto cleanup;
    ret = get_collection(anchor_name, collection_name, &collection_id);
    if (ret)
        goto cleanup;
    ret = set_primary_name(context, collection_id, subsidiary_name);

cleanup:
    free(anchor_name);
    free(collection_name);
    free(subsidiary_name);
    return ret;
}

/*
 * ccache implementation storing credentials in the Linux keyring facility
 * The default is to put them at the session keyring level.
 * If "KEYRING:process:" or "KEYRING:thread:" is specified, then they will
 * be stored at the process or thread level respectively.
 */
const krb5_cc_ops krb5_krcc_ops = {
    0,
    "KEYRING",
    krcc_get_name,
    krcc_resolve,
    krcc_generate_new,
    krcc_initialize,
    krcc_destroy,
    krcc_close,
    krcc_store,
    krcc_retrieve,
    krcc_get_principal,
    krcc_start_seq_get,
    krcc_next_cred,
    krcc_end_seq_get,
    krcc_remove_cred,
    krcc_set_flags,
    krcc_get_flags,        /* added after 1.4 release */
    krcc_ptcursor_new,
    krcc_ptcursor_next,
    krcc_ptcursor_free,
    NULL, /* move */
    NULL, /* wasdefault */
    krcc_lock,
    krcc_unlock,
    krcc_switch_to,
};

#else /* !USE_KEYRING_CCACHE */

/*
 * Export this, but it shouldn't be used.
 */
const krb5_cc_ops krb5_krcc_ops = {
    0,
    "KEYRING",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,                       /* added after 1.4 release */
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};
#endif  /* USE_KEYRING_CCACHE */
