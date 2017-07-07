/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/keytab/kt_memory.c */
/*
 * Copyright 2007 by Secure Endpoints Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "k5-int.h"
#include "kt-int.h"
#include <stdio.h>

#ifndef LEAN_CLIENT

#define HEIMDAL_COMPATIBLE

/*
 * Information needed by internal routines of the file-based ticket
 * cache implementation.
 */


/*
 * Constants
 */

/*
 * Types
 */
/* From krb5.h:
 * typedef struct krb5_keytab_entry_st {
 *    krb5_magic magic;
 *    krb5_principal principal;    principal of this key
 *    krb5_timestamp timestamp;    time entry written to keytable
 *    krb5_kvno vno;               key version number
 *    krb5_keyblock key;           the secret key
 *} krb5_keytab_entry;
 */

/* Individual key entries within a table, in a linked list */
typedef struct _krb5_mkt_link {
    struct _krb5_mkt_link *next;
    krb5_keytab_entry *entry;
} krb5_mkt_link, *krb5_mkt_cursor;

/* Per-keytab data header */
typedef struct _krb5_mkt_data {
    char               *name;           /* Name of the keytab */
    k5_mutex_t          lock;           /* Thread-safety - all but link */
    krb5_int32          refcount;
    krb5_mkt_cursor     link;
} krb5_mkt_data;

/* List of memory key tables */
typedef struct _krb5_mkt_list_node {
    struct _krb5_mkt_list_node *next;
    krb5_keytab keytab;
} krb5_mkt_list_node;

/* Iterator over memory key tables */
typedef struct _krb5_mkt_ptcursor_data {
    struct _krb5_mkt_list_node *cur;
} krb5_mkt_ptcursor_data;

/*
 * Globals
 */
static krb5_mkt_list_node * krb5int_mkt_list = NULL;
static k5_mutex_t krb5int_mkt_mutex = K5_MUTEX_PARTIAL_INITIALIZER;

/*
 * Macros
 */
#define KTLOCK(id) k5_mutex_lock(&(((krb5_mkt_data *)(id)->data)->lock))
#define KTUNLOCK(id) k5_mutex_unlock(&(((krb5_mkt_data *)(id)->data)->lock))
#define KTCHECKLOCK(id) k5_mutex_assert_locked(&(((krb5_mkt_data *)(id)->data)->lock))

#define KTGLOCK k5_mutex_lock(&krb5int_mkt_mutex)
#define KTGUNLOCK k5_mutex_unlock(&krb5int_mkt_mutex)
#define KTGCHECKLOCK k5_mutex_assert_locked(&krb5int_mkt_mutex)

#define KTLINK(id) (((krb5_mkt_data *)(id)->data)->link)
#define KTREFCNT(id) (((krb5_mkt_data *)(id)->data)->refcount)
#define KTNAME(id) (((krb5_mkt_data *)(id)->data)->name)

extern const struct _krb5_kt_ops krb5_mkt_ops;

krb5_error_code KRB5_CALLCONV
krb5_mkt_resolve(krb5_context, const char *, krb5_keytab *);

krb5_error_code KRB5_CALLCONV
krb5_mkt_get_name(krb5_context, krb5_keytab, char *, unsigned int);

krb5_error_code KRB5_CALLCONV
krb5_mkt_close(krb5_context, krb5_keytab);

krb5_error_code KRB5_CALLCONV
krb5_mkt_get_entry(krb5_context, krb5_keytab, krb5_const_principal, krb5_kvno,
                   krb5_enctype, krb5_keytab_entry *);

krb5_error_code KRB5_CALLCONV
krb5_mkt_start_seq_get(krb5_context, krb5_keytab, krb5_kt_cursor *);

krb5_error_code KRB5_CALLCONV
krb5_mkt_get_next(krb5_context, krb5_keytab, krb5_keytab_entry *,
                  krb5_kt_cursor *);

krb5_error_code KRB5_CALLCONV
krb5_mkt_end_get(krb5_context, krb5_keytab, krb5_kt_cursor *);

/* routines to be included on extended version (write routines) */
krb5_error_code KRB5_CALLCONV
krb5_mkt_add(krb5_context, krb5_keytab, krb5_keytab_entry *);

krb5_error_code KRB5_CALLCONV
krb5_mkt_remove(krb5_context, krb5_keytab, krb5_keytab_entry *);

int
krb5int_mkt_initialize(void)
{
    return k5_mutex_finish_init(&krb5int_mkt_mutex);
}

void
krb5int_mkt_finalize(void)
{
    krb5_mkt_list_node *node, *next_node;
    krb5_mkt_cursor cursor, next_cursor;

    k5_mutex_destroy(&krb5int_mkt_mutex);

    for (node = krb5int_mkt_list; node; node = next_node) {
        next_node = node->next;

        /* destroy the contents of node->keytab */
        free(KTNAME(node->keytab));

        /* free the keytab entries */
        for (cursor = KTLINK(node->keytab); cursor; cursor = next_cursor) {
            next_cursor = cursor->next;
            /* the call to krb5_kt_free_entry uses a NULL in place of the
             * krb5_context since we know that the context isn't used by
             * krb5_kt_free_entry or krb5_free_principal. */
            krb5_kt_free_entry(NULL, cursor->entry);
            free(cursor->entry);
            free(cursor);
        }

        /* destroy the lock */
        k5_mutex_destroy(&(((krb5_mkt_data *)node->keytab->data)->lock));

        /* free the private data */
        free(node->keytab->data);

        /* and the keytab */
        free(node->keytab);

        /* and finally the node */
        free(node);
    }
}

static krb5_error_code
create_list_node(const char *name, krb5_mkt_list_node **listp)
{
    krb5_mkt_list_node *list;
    krb5_mkt_data *data = NULL;
    krb5_error_code err;

    *listp = NULL;

    list = calloc(1, sizeof(krb5_mkt_list_node));
    if (list == NULL) {
        err = ENOMEM;
        goto cleanup;
    }

    list->keytab = calloc(1, sizeof(struct _krb5_kt));
    if (list->keytab == NULL) {
        err = ENOMEM;
        goto cleanup;
    }
    list->keytab->ops = &krb5_mkt_ops;

    data = calloc(1, sizeof(krb5_mkt_data));
    if (data == NULL) {
        err = ENOMEM;
        goto cleanup;
    }
    data->link = NULL;
    data->refcount = 0;

    data->name = strdup(name);
    if (data->name == NULL) {
        err = ENOMEM;
        goto cleanup;
    }

    err = k5_mutex_init(&data->lock);
    if (err)
        goto cleanup;

    list->keytab->data = data;
    list->keytab->magic = KV5M_KEYTAB;
    list->next = NULL;
    *listp = list;
    return 0;

cleanup:
    /* data->lock was initialized last, so no need to destroy. */
    if (data)
        free(data->name);
    free(data);
    if (list)
        free(list->keytab);
    free(list);
    return err;
}

/*
 * This is an implementation specific resolver.  It returns a keytab
 * initialized with memory keytab routines.
 */

krb5_error_code KRB5_CALLCONV
krb5_mkt_resolve(krb5_context context, const char *name, krb5_keytab *id)
{
    krb5_mkt_list_node *list;
    krb5_error_code err = 0;

    *id = NULL;

    /* First determine if a memory keytab of this name already exists */
    KTGLOCK;

    for (list = krb5int_mkt_list; list; list = list->next) {
        if (strcmp(name,KTNAME(list->keytab)) == 0)
            break;
    }

    if (!list) {
        /* We will now create the new key table with the specified name.
         * We do not drop the global lock, therefore the name will indeed
         * be unique when we add it.
         */
        err = create_list_node(name, &list);
        if (err)
            goto done;
        list->next = krb5int_mkt_list;
        krb5int_mkt_list = list;
    }

    /* Increment the reference count on the keytab we found or created. */
    KTLOCK(list->keytab);
    KTREFCNT(list->keytab)++;
    KTUNLOCK(list->keytab);
    *id = list->keytab;
done:
    KTGUNLOCK;
    return err;
}


/*
 * "Close" a memory-based keytab.  This is effectively a no-op.
 * We check to see if the keytab exists and that is about it.
 * Closing a file keytab does not destroy the contents.  Closing
 * a memory keytab shouldn't either.
 */

krb5_error_code KRB5_CALLCONV
krb5_mkt_close(krb5_context context, krb5_keytab id)
{
    krb5_mkt_list_node **listp;
#ifdef HEIMDAL_COMPATIBLE
    krb5_mkt_list_node *node;
    krb5_mkt_data * data;
#endif
    krb5_error_code err = 0;

    /* First determine if a memory keytab of this name already exists */
    KTGLOCK;

    for (listp = &krb5int_mkt_list; *listp; listp = &((*listp)->next))
    {
        if (id == (*listp)->keytab) {
            /* Found */
            break;
        }
    }

    if (*listp == NULL) {
        /* The specified keytab could not be found */
        err = KRB5_KT_NOTFOUND;
        goto done;
    }

    /* reduce the refcount and return */
    KTLOCK(id);
    KTREFCNT(id)--;
    KTUNLOCK(id);

#ifdef HEIMDAL_COMPATIBLE
    /* In Heimdal if the refcount hits 0, the MEMORY keytab is
     * destroyed since there is no krb5_kt_destroy function.
     * There is no need to lock the entry while performing
     * these operations as the refcount will be 0 and we are
     * holding the global lock.
     */
    data = (krb5_mkt_data *)id->data;
    if (data->refcount == 0) {
        krb5_mkt_cursor cursor, next_cursor;

        node = *listp;
        *listp = node->next;

        /* destroy the contents of node->keytab (aka id) */
        free(data->name);

        /* free the keytab entries */
        for (cursor = KTLINK(node->keytab); cursor; cursor = next_cursor) {
            next_cursor = cursor->next;

            krb5_kt_free_entry(context, cursor->entry);
            free(cursor->entry);
            free(cursor);
        }

        /* destroy the lock */
        k5_mutex_destroy(&(data->lock));

        /* free the private data */
        free(data);

        /* and the keytab */
        free(node->keytab);

        /* and finally the node */
        free(node);
    }
#endif /* HEIMDAL_COMPATIBLE */

done:
    KTGUNLOCK;
    return(err);
}

/*
 * This is the get_entry routine for the memory based keytab implementation.
 * It either retrieves the entry or returns an error.
 */

krb5_error_code KRB5_CALLCONV
krb5_mkt_get_entry(krb5_context context, krb5_keytab id,
                   krb5_const_principal principal, krb5_kvno kvno,
                   krb5_enctype enctype, krb5_keytab_entry *out_entry)
{
    krb5_mkt_cursor   cursor;
    krb5_keytab_entry *entry, *match = NULL;
    krb5_error_code err = 0;
    int found_wrong_kvno = 0;
    krb5_boolean similar = 0;

    KTLOCK(id);

    for (cursor = KTLINK(id); cursor && cursor->entry; cursor = cursor->next) {
        entry = cursor->entry;

        /* if the principal isn't the one requested, continue to the next. */

        if (!krb5_principal_compare(context, principal, entry->principal))
            continue;

        /* if the enctype is not ignored and doesn't match,
           and continue to the next */
        if (enctype != IGNORE_ENCTYPE) {
            if ((err = krb5_c_enctype_compare(context, enctype,
                                              entry->key.enctype,
                                              &similar))) {
                /* we can't determine the enctype of the entry */
                continue;
            }

            if (!similar)
                continue;
        }

        if (kvno == IGNORE_VNO) {
            if (match == NULL)
                match = entry;
            else if (entry->vno > match->vno)
                match = entry;
        } else {
            if (entry->vno == kvno) {
                match = entry;
                break;
            } else {
                found_wrong_kvno++;
            }
        }
    }

    /* if we found an entry that matches, ... */
    if (match) {
        out_entry->magic = match->magic;
        out_entry->timestamp = match->timestamp;
        out_entry->vno = match->vno;
        out_entry->key = match->key;
        err = krb5_copy_keyblock_contents(context, &(match->key),
                                          &(out_entry->key));
        /*
         * Coerce the enctype of the output keyblock in case we
         * got an inexact match on the enctype.
         */
        if(enctype != IGNORE_ENCTYPE)
            out_entry->key.enctype = enctype;
        if(!err) {
            err = krb5_copy_principal(context,
                                      match->principal,
                                      &(out_entry->principal));
        }
    } else {
        if (!err)
            err = found_wrong_kvno ? KRB5_KT_KVNONOTFOUND : KRB5_KT_NOTFOUND;
    }

    KTUNLOCK(id);
    return(err);
}

/*
 * Get the name of the memory-based keytab.
 */

krb5_error_code KRB5_CALLCONV
krb5_mkt_get_name(krb5_context context, krb5_keytab id, char *name, unsigned int len)
{
    int result;

    memset(name, 0, len);
    result = snprintf(name, len, "%s:%s", id->ops->prefix, KTNAME(id));
    if (SNPRINTF_OVERFLOW(result, len))
        return(KRB5_KT_NAME_TOOLONG);
    return(0);
}

/*
 * krb5_mkt_start_seq_get()
 */

krb5_error_code KRB5_CALLCONV
krb5_mkt_start_seq_get(krb5_context context, krb5_keytab id, krb5_kt_cursor *cursorp)
{
    KTLOCK(id);
    *cursorp = (krb5_kt_cursor)KTLINK(id);
    KTUNLOCK(id);

    return(0);
}

/*
 * krb5_mkt_get_next()
 */

krb5_error_code KRB5_CALLCONV
krb5_mkt_get_next(krb5_context context, krb5_keytab id, krb5_keytab_entry *entry, krb5_kt_cursor *cursor)
{
    krb5_mkt_cursor mkt_cursor = (krb5_mkt_cursor)*cursor;
    krb5_error_code err = 0;

    KTLOCK(id);

    if (mkt_cursor == NULL) {
        KTUNLOCK(id);
        return KRB5_KT_END;
    }

    entry->magic = mkt_cursor->entry->magic;
    entry->timestamp = mkt_cursor->entry->timestamp;
    entry->vno = mkt_cursor->entry->vno;
    entry->key = mkt_cursor->entry->key;
    err = krb5_copy_keyblock_contents(context, &(mkt_cursor->entry->key),
                                      &(entry->key));
    if (!err)
        err = krb5_copy_principal(context, mkt_cursor->entry->principal,
                                  &(entry->principal));
    if (!err)
        *cursor = (krb5_kt_cursor *)mkt_cursor->next;
    KTUNLOCK(id);
    return(err);
}

/*
 * krb5_mkt_end_get()
 */

krb5_error_code KRB5_CALLCONV
krb5_mkt_end_get(krb5_context context, krb5_keytab id, krb5_kt_cursor *cursor)
{
    *cursor = NULL;
    return(0);
}


/*
 * krb5_mkt_add()
 */

krb5_error_code KRB5_CALLCONV
krb5_mkt_add(krb5_context context, krb5_keytab id, krb5_keytab_entry *entry)
{
    krb5_error_code err = 0;
    krb5_mkt_cursor cursor;

    KTLOCK(id);

    cursor = (krb5_mkt_cursor)malloc(sizeof(krb5_mkt_link));
    if (cursor == NULL) {
        err = ENOMEM;
        goto done;
    }
    cursor->entry = (krb5_keytab_entry *)malloc(sizeof(krb5_keytab_entry));
    if (cursor->entry == NULL) {
        free(cursor);
        err = ENOMEM;
        goto done;
    }
    cursor->entry->magic = entry->magic;
    cursor->entry->timestamp = entry->timestamp;
    cursor->entry->vno = entry->vno;
    err = krb5_copy_keyblock_contents(context, &(entry->key),
                                      &(cursor->entry->key));
    if (err) {
        free(cursor->entry);
        free(cursor);
        goto done;
    }

    err = krb5_copy_principal(context, entry->principal, &(cursor->entry->principal));
    if (err) {
        krb5_free_keyblock_contents(context, &(cursor->entry->key));
        free(cursor->entry);
        free(cursor);
        goto done;
    }

    if (KTLINK(id) == NULL) {
        cursor->next = NULL;
        KTLINK(id) = cursor;
    } else {
        cursor->next = KTLINK(id);
        KTLINK(id) = cursor;
    }

done:
    KTUNLOCK(id);
    return err;
}

/*
 * krb5_mkt_remove()
 */

krb5_error_code KRB5_CALLCONV
krb5_mkt_remove(krb5_context context, krb5_keytab id, krb5_keytab_entry *entry)
{
    krb5_mkt_cursor *pcursor, next;
    krb5_error_code err = 0;

    KTLOCK(id);

    if ( KTLINK(id) == NULL ) {
        err = KRB5_KT_NOTFOUND;
        goto done;
    }

    for ( pcursor = &KTLINK(id); *pcursor; pcursor = &(*pcursor)->next ) {
        if ( (*pcursor)->entry->vno == entry->vno &&
             (*pcursor)->entry->key.enctype == entry->key.enctype &&
             krb5_principal_compare(context, (*pcursor)->entry->principal, entry->principal))
            break;
    }

    if (!*pcursor) {
        err = KRB5_KT_NOTFOUND;
        goto done;
    }

    krb5_kt_free_entry(context, (*pcursor)->entry);
    free((*pcursor)->entry);
    next = (*pcursor)->next;
    free(*pcursor);
    (*pcursor) = next;

done:
    KTUNLOCK(id);
    return err;
}


/*
 * krb5_mkt_ops
 */

const struct _krb5_kt_ops krb5_mkt_ops = {
    0,
    "MEMORY",   /* Prefix -- this string should not appear anywhere else! */
    krb5_mkt_resolve,
    krb5_mkt_get_name,
    krb5_mkt_close,
    krb5_mkt_get_entry,
    krb5_mkt_start_seq_get,
    krb5_mkt_get_next,
    krb5_mkt_end_get,
    krb5_mkt_add,
    krb5_mkt_remove,
    NULL
};

#endif /* LEAN_CLIENT */
