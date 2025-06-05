/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/keytab/kt_file.c */
/*
 * Copyright 1990,1991,1995,2007,2008 by the Massachusetts Institute of Technology.
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
/*
 * Copyright (c) Hewlett-Packard Company 1991
 * Released to the Massachusetts Institute of Technology for inclusion
 * in the Kerberos source code distribution.
 *
 * Copyright 1990,1991 by the Massachusetts Institute of Technology.
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

#ifndef LEAN_CLIENT

#include "k5-int.h"
#include "../os/os-proto.h"
#include <stdio.h>

/*
 * Information needed by internal routines of the file-based ticket
 * cache implementation.
 */


/*
 * Constants
 */

#define KRB5_KT_VNO_1   0x0501  /* krb v5, keytab version 1 (DCE compat) */
#define KRB5_KT_VNO     0x0502  /* krb v5, keytab version 2 (standard)  */

#define KRB5_KT_DEFAULT_VNO KRB5_KT_VNO

/*
 * Types
 */
typedef struct _krb5_ktfile_data {
    char *name;                 /* Name of the file */
    FILE *openf;                /* open file, if any. */
    char iobuf[BUFSIZ];         /* so we can zap it later */
    int version;                /* Version number of keytab */
    unsigned int iter_count;    /* Number of active iterators */
    long start_offset;          /* Starting offset after version */
    k5_mutex_t lock;            /* Protect openf, version */
} krb5_ktfile_data;

/*
 * Some limitations:
 *
 * If the file OPENF is left open between calls, we have an iterator
 * active, and OPENF is opened in read-only mode.  So, no changes
 * can be made via that handle.
 *
 * An advisory file lock is used while the file is open.  Thus,
 * multiple handles on the same underlying file cannot be used without
 * disrupting the locking in effect.
 *
 * The start_offset field is only valid if the file is open.  It will
 * almost certainly always be the same constant.  It's used so that
 * if an iterator is active, and we start another one, we don't have
 * to seek back to the start and re-read the version number to set
 * the position for the iterator.
 */

/*
 * Macros
 */
#define KTPRIVATE(id) ((krb5_ktfile_data *)(id)->data)
#define KTFILENAME(id) (((krb5_ktfile_data *)(id)->data)->name)
#define KTFILEP(id) (((krb5_ktfile_data *)(id)->data)->openf)
#define KTFILEBUFP(id) (((krb5_ktfile_data *)(id)->data)->iobuf)
#define KTVERSION(id) (((krb5_ktfile_data *)(id)->data)->version)
#define KTITERS(id) (((krb5_ktfile_data *)(id)->data)->iter_count)
#define KTSTARTOFF(id) (((krb5_ktfile_data *)(id)->data)->start_offset)
#define KTLOCK(id) k5_mutex_lock(&((krb5_ktfile_data *)(id)->data)->lock)
#define KTUNLOCK(id) k5_mutex_unlock(&((krb5_ktfile_data *)(id)->data)->lock)
#define KTCHECKLOCK(id) k5_mutex_assert_locked(&((krb5_ktfile_data *)(id)->data)->lock)

extern const struct _krb5_kt_ops krb5_ktf_ops;
extern const struct _krb5_kt_ops krb5_ktf_writable_ops;

static krb5_error_code KRB5_CALLCONV
krb5_ktfile_resolve(krb5_context, const char *, krb5_keytab *);

static krb5_error_code KRB5_CALLCONV
krb5_ktfile_get_name(krb5_context, krb5_keytab, char *, unsigned int);

static krb5_error_code KRB5_CALLCONV
krb5_ktfile_close(krb5_context, krb5_keytab);

static krb5_error_code KRB5_CALLCONV
krb5_ktfile_get_entry(krb5_context, krb5_keytab, krb5_const_principal,
                      krb5_kvno, krb5_enctype, krb5_keytab_entry *);

static krb5_error_code KRB5_CALLCONV
krb5_ktfile_start_seq_get(krb5_context, krb5_keytab, krb5_kt_cursor *);

static krb5_error_code KRB5_CALLCONV
krb5_ktfile_get_next(krb5_context, krb5_keytab, krb5_keytab_entry *,
                     krb5_kt_cursor *);

static krb5_error_code KRB5_CALLCONV
krb5_ktfile_end_get(krb5_context, krb5_keytab, krb5_kt_cursor *);

/* routines to be included on extended version (write routines) */
static krb5_error_code KRB5_CALLCONV
krb5_ktfile_add(krb5_context, krb5_keytab, krb5_keytab_entry *);

static krb5_error_code KRB5_CALLCONV
krb5_ktfile_remove(krb5_context, krb5_keytab, krb5_keytab_entry *);

static krb5_error_code
krb5_ktfileint_openr(krb5_context, krb5_keytab);

static krb5_error_code
krb5_ktfileint_openw(krb5_context, krb5_keytab);

static krb5_error_code
krb5_ktfileint_close(krb5_context, krb5_keytab);

static krb5_error_code
krb5_ktfileint_read_entry(krb5_context, krb5_keytab, krb5_keytab_entry *);

static krb5_error_code
krb5_ktfileint_write_entry(krb5_context, krb5_keytab, krb5_keytab_entry *);

static krb5_error_code
krb5_ktfileint_delete_entry(krb5_context, krb5_keytab, krb5_int32);

static krb5_error_code
krb5_ktfileint_internal_read_entry(krb5_context, krb5_keytab,
                                   krb5_keytab_entry *, krb5_int32 *);

static krb5_error_code
krb5_ktfileint_size_entry(krb5_context, krb5_keytab_entry *, krb5_int32 *);

static krb5_error_code
krb5_ktfileint_find_slot(krb5_context, krb5_keytab, krb5_int32 *,
                         krb5_int32 *);


/*
 * This is an implementation specific resolver.  It returns a keytab id
 * initialized with file keytab routines.
 */

static krb5_error_code KRB5_CALLCONV
krb5_ktfile_resolve(krb5_context context, const char *name,
                    krb5_keytab *id_out)
{
    krb5_ktfile_data *data = NULL;
    krb5_error_code err = ENOMEM;
    krb5_keytab id;

    *id_out = NULL;

    id = calloc(1, sizeof(*id));
    if (id == NULL)
        return ENOMEM;

    id->ops = &krb5_ktf_ops;
    data = calloc(1, sizeof(krb5_ktfile_data));
    if (data == NULL)
        goto cleanup;

    data->name = strdup(name);
    if (data->name == NULL)
        goto cleanup;

    err = k5_mutex_init(&data->lock);
    if (err)
        goto cleanup;

    data->openf = 0;
    data->version = 0;
    data->iter_count = 0;

    id->data = (krb5_pointer) data;
    id->magic = KV5M_KEYTAB;
    *id_out = id;
    return 0;
cleanup:
    if (data)
        free(data->name);
    free(data);
    free(id);
    return err;
}


/*
 * "Close" a file-based keytab and invalidate the id.  This means
 * free memory hidden in the structures.
 */

static krb5_error_code KRB5_CALLCONV
krb5_ktfile_close(krb5_context context, krb5_keytab id)
/*
 * This routine is responsible for freeing all memory allocated
 * for this keytab.  There are no system resources that need
 * to be freed nor are there any open files.
 *
 * This routine should undo anything done by krb5_ktfile_resolve().
 */
{
    free(KTFILENAME(id));
    zap(KTFILEBUFP(id), BUFSIZ);
    k5_mutex_destroy(&((krb5_ktfile_data *)id->data)->lock);
    free(id->data);
    id->ops = 0;
    free(id);
    return (0);
}

/* Return true if k1 is more recent than k2, applying wraparound heuristics. */
static krb5_boolean
more_recent(const krb5_keytab_entry *k1, const krb5_keytab_entry *k2)
{
    /*
     * If a small kvno was written at the same time or later than a large kvno,
     * the kvno probably wrapped at some boundary, so consider the small kvno
     * more recent.  Wraparound can happen due to pre-1.14 keytab file format
     * limitations (8-bit kvno storage), pre-1.14 kadmin protocol limitations
     * (8-bit kvno marshalling), or KDB limitations (16-bit kvno storage).
     */
    if (!ts_after(k2->timestamp, k1->timestamp) &&
        k1->vno < 128 && k2->vno > 240)
        return TRUE;
    if (!ts_after(k1->timestamp, k2->timestamp) &&
        k1->vno > 240 && k2->vno < 128)
        return FALSE;

    /* Otherwise do a simple version comparison. */
    return k1->vno > k2->vno;
}

/*
 * This is the get_entry routine for the file based keytab implementation.
 * It opens the keytab file, and either retrieves the entry or returns
 * an error.
 */

static krb5_error_code KRB5_CALLCONV
krb5_ktfile_get_entry(krb5_context context, krb5_keytab id,
                      krb5_const_principal principal, krb5_kvno kvno,
                      krb5_enctype enctype, krb5_keytab_entry *entry)
{
    krb5_keytab_entry cur_entry, new_entry;
    krb5_error_code kerror = 0;
    int found_wrong_kvno = 0;
    int was_open;
    char *princname;

    KTLOCK(id);

    if (KTFILEP(id) != NULL) {
        was_open = 1;

        if (fseek(KTFILEP(id), KTSTARTOFF(id), SEEK_SET) == -1) {
            KTUNLOCK(id);
            return errno;
        }
    } else {
        was_open = 0;

        /* Open the keyfile for reading */
        if ((kerror = krb5_ktfileint_openr(context, id))) {
            KTUNLOCK(id);
            return(kerror);
        }
    }

    /*
     * For efficiency and simplicity, we'll use a while true that
     * is exited with a break statement.
     */
    cur_entry.principal = 0;
    cur_entry.vno = 0;
    cur_entry.key.contents = 0;

    while (TRUE) {
        if ((kerror = krb5_ktfileint_read_entry(context, id, &new_entry)))
            break;

        /* by the time this loop exits, it must either free cur_entry,
           and copy new_entry there, or free new_entry.  Otherwise, it
           leaks. */

        /* if the principal isn't the one requested, free new_entry
           and continue to the next. */

        if (!krb5_principal_compare(context, principal, new_entry.principal)) {
            krb5_kt_free_entry(context, &new_entry);
            continue;
        }

        /* If the enctype is not ignored and doesn't match, free new_entry and
           continue to the next. */
        if (enctype != IGNORE_ENCTYPE && enctype != new_entry.key.enctype) {
            krb5_kt_free_entry(context, &new_entry);
            continue;
        }

        if (kvno == IGNORE_VNO || new_entry.vno == IGNORE_VNO) {
            /* If this entry is more recent (or the first match), free the
             * current and keep the new.  Otherwise, free the new. */
            if (cur_entry.principal == NULL ||
                more_recent(&new_entry, &cur_entry)) {
                krb5_kt_free_entry(context, &cur_entry);
                cur_entry = new_entry;
            } else {
                krb5_kt_free_entry(context, &new_entry);
            }
        } else {
            /*
             * If this kvno matches exactly, free the current, keep the new,
             * and break out.  If it matches the low 8 bits of the desired
             * kvno, remember the first match (because the recorded kvno may
             * have been truncated due to pre-1.14 keytab format or kadmin
             * protocol limitations) but keep looking for an exact match.
             * Otherwise, remember that we were here so we can return the right
             * error, and free the new.
             */
            if (new_entry.vno == kvno) {
                krb5_kt_free_entry(context, &cur_entry);
                cur_entry = new_entry;
                if (new_entry.vno == kvno)
                    break;
            } else if (new_entry.vno == (kvno & 0xff) &&
                       cur_entry.principal == NULL) {
                cur_entry = new_entry;
            } else {
                found_wrong_kvno++;
                krb5_kt_free_entry(context, &new_entry);
            }
        }
    }

    if (kerror == KRB5_KT_END) {
        if (cur_entry.principal)
            kerror = 0;
        else if (found_wrong_kvno)
            kerror = KRB5_KT_KVNONOTFOUND;
        else {
            kerror = KRB5_KT_NOTFOUND;
            if (krb5_unparse_name(context, principal, &princname) == 0) {
                k5_setmsg(context, kerror,
                          _("No key table entry found for %s"), princname);
                free(princname);
            }
        }
    }
    if (kerror) {
        if (was_open == 0)
            (void) krb5_ktfileint_close(context, id);
        KTUNLOCK(id);
        krb5_kt_free_entry(context, &cur_entry);
        return kerror;
    }
    if (was_open == 0 && (kerror = krb5_ktfileint_close(context, id)) != 0) {
        KTUNLOCK(id);
        krb5_kt_free_entry(context, &cur_entry);
        return kerror;
    }
    KTUNLOCK(id);
    *entry = cur_entry;
    return 0;
}

/*
 * Get the name of the file containing a file-based keytab.
 */

static krb5_error_code KRB5_CALLCONV
krb5_ktfile_get_name(krb5_context context, krb5_keytab id, char *name, unsigned int len)
/*
 * This routine returns the name of the name of the file associated with
 * this file-based keytab.  name is zeroed and the filename is truncated
 * to fit in name if necessary.  The name is prefixed with PREFIX:, so that
 * trt will happen if the name is passed back to resolve.
 */
{
    int result;

    memset(name, 0, len);
    result = snprintf(name, len, "%s:%s", id->ops->prefix, KTFILENAME(id));
    if (SNPRINTF_OVERFLOW(result, len))
        return(KRB5_KT_NAME_TOOLONG);
    return(0);
}

/*
 * krb5_ktfile_start_seq_get()
 */

static krb5_error_code KRB5_CALLCONV
krb5_ktfile_start_seq_get(krb5_context context, krb5_keytab id, krb5_kt_cursor *cursorp)
{
    krb5_error_code retval;
    long *fileoff;

    KTLOCK(id);

    if (KTITERS(id) == 0) {
        if ((retval = krb5_ktfileint_openr(context, id))) {
            KTUNLOCK(id);
            return retval;
        }
    }

    if (!(fileoff = (long *)malloc(sizeof(*fileoff)))) {
        if (KTITERS(id) == 0)
            krb5_ktfileint_close(context, id);
        KTUNLOCK(id);
        return ENOMEM;
    }
    *fileoff = KTSTARTOFF(id);
    *cursorp = (krb5_kt_cursor)fileoff;
    KTITERS(id)++;
    if (KTITERS(id) == 0) {
        /* Wrapped?!  */
        KTITERS(id)--;
        KTUNLOCK(id);
        k5_setmsg(context, KRB5_KT_IOERR, "Too many keytab iterators active");
        return KRB5_KT_IOERR;   /* XXX */
    }
    KTUNLOCK(id);

    return 0;
}

/*
 * krb5_ktfile_get_next()
 */

static krb5_error_code KRB5_CALLCONV
krb5_ktfile_get_next(krb5_context context, krb5_keytab id, krb5_keytab_entry *entry, krb5_kt_cursor *cursor)
{
    long *fileoff = (long *)*cursor;
    krb5_keytab_entry cur_entry;
    krb5_error_code kerror;

    KTLOCK(id);
    if (KTFILEP(id) == NULL) {
        KTUNLOCK(id);
        return KRB5_KT_IOERR;
    }
    if (fseek(KTFILEP(id), *fileoff, 0) == -1) {
        KTUNLOCK(id);
        return KRB5_KT_END;
    }
    if ((kerror = krb5_ktfileint_read_entry(context, id, &cur_entry))) {
        KTUNLOCK(id);
        return kerror;
    }
    *fileoff = ftell(KTFILEP(id));
    *entry = cur_entry;
    KTUNLOCK(id);
    return 0;
}

/*
 * krb5_ktfile_end_get()
 */

static krb5_error_code KRB5_CALLCONV
krb5_ktfile_end_get(krb5_context context, krb5_keytab id, krb5_kt_cursor *cursor)
{
    krb5_error_code kerror;

    free(*cursor);
    KTLOCK(id);
    KTITERS(id)--;
    if (KTFILEP(id) != NULL && KTITERS(id) == 0)
        kerror = krb5_ktfileint_close(context, id);
    else
        kerror = 0;
    KTUNLOCK(id);
    return kerror;
}

/*
 * krb5_ktfile_add()
 */

static krb5_error_code KRB5_CALLCONV
krb5_ktfile_add(krb5_context context, krb5_keytab id, krb5_keytab_entry *entry)
{
    krb5_error_code retval;

    KTLOCK(id);
    if (KTFILEP(id)) {
        /* Iterator(s) active -- no changes.  */
        KTUNLOCK(id);
        k5_setmsg(context, KRB5_KT_IOERR,
                  _("Cannot change keytab with keytab iterators active"));
        return KRB5_KT_IOERR;   /* XXX */
    }
    if ((retval = krb5_ktfileint_openw(context, id))) {
        KTUNLOCK(id);
        return retval;
    }
    if (fseek(KTFILEP(id), 0, 2) == -1) {
        KTUNLOCK(id);
        return KRB5_KT_END;
    }
    retval = krb5_ktfileint_write_entry(context, id, entry);
    krb5_ktfileint_close(context, id);
    KTUNLOCK(id);
    return retval;
}

/*
 * krb5_ktfile_remove()
 */

static krb5_error_code KRB5_CALLCONV
krb5_ktfile_remove(krb5_context context, krb5_keytab id, krb5_keytab_entry *entry)
{
    krb5_keytab_entry   cur_entry;
    krb5_error_code     kerror;
    krb5_int32          delete_point;

    KTLOCK(id);
    if (KTFILEP(id)) {
        /* Iterator(s) active -- no changes.  */
        KTUNLOCK(id);
        k5_setmsg(context, KRB5_KT_IOERR,
                  _("Cannot change keytab with keytab iterators active"));
        return KRB5_KT_IOERR;   /* XXX */
    }

    if ((kerror = krb5_ktfileint_openw(context, id))) {
        KTUNLOCK(id);
        return kerror;
    }

    /*
     * For efficiency and simplicity, we'll use a while true that
     * is exited with a break statement.
     */
    while (TRUE) {
        if ((kerror = krb5_ktfileint_internal_read_entry(context, id,
                                                         &cur_entry,
                                                         &delete_point)))
            break;

        if ((entry->vno == cur_entry.vno) &&
            (entry->key.enctype == cur_entry.key.enctype) &&
            krb5_principal_compare(context, entry->principal, cur_entry.principal)) {
            /* found a match */
            krb5_kt_free_entry(context, &cur_entry);
            break;
        }
        krb5_kt_free_entry(context, &cur_entry);
    }

    if (kerror == KRB5_KT_END)
        kerror = KRB5_KT_NOTFOUND;

    if (kerror) {
        (void) krb5_ktfileint_close(context, id);
        KTUNLOCK(id);
        return kerror;
    }

    kerror = krb5_ktfileint_delete_entry(context, id, delete_point);

    if (kerror) {
        (void) krb5_ktfileint_close(context, id);
    } else {
        kerror = krb5_ktfileint_close(context, id);
    }
    KTUNLOCK(id);
    return kerror;
}

/*
 * krb5_ktf_ops
 */

const struct _krb5_kt_ops krb5_ktf_ops = {
    0,
    "FILE",     /* Prefix -- this string should not appear anywhere else! */
    krb5_ktfile_resolve,
    krb5_ktfile_get_name,
    krb5_ktfile_close,
    krb5_ktfile_get_entry,
    krb5_ktfile_start_seq_get,
    krb5_ktfile_get_next,
    krb5_ktfile_end_get,
    krb5_ktfile_add,
    krb5_ktfile_remove
};

/*
 * krb5_ktf_writable_ops -- this is the same as krb5_ktf_ops except for the
 * prefix.  WRFILE should no longer be needed, but is effectively aliased to
 * FILE for compatibility.
 */

const struct _krb5_kt_ops krb5_ktf_writable_ops = {
    0,
    "WRFILE",   /* Prefix -- this string should not appear anywhere else! */
    krb5_ktfile_resolve,
    krb5_ktfile_get_name,
    krb5_ktfile_close,
    krb5_ktfile_get_entry,
    krb5_ktfile_start_seq_get,
    krb5_ktfile_get_next,
    krb5_ktfile_end_get,
    krb5_ktfile_add,
    krb5_ktfile_remove
};

/*
 * krb5_kt_dfl_ops
 */

const krb5_kt_ops krb5_kt_dfl_ops = {
    0,
    "FILE",     /* Prefix -- this string should not appear anywhere else! */
    krb5_ktfile_resolve,
    krb5_ktfile_get_name,
    krb5_ktfile_close,
    krb5_ktfile_get_entry,
    krb5_ktfile_start_seq_get,
    krb5_ktfile_get_next,
    krb5_ktfile_end_get,
    0,
    0
};

/* Formerly lib/krb5/keytab/file/ktf_util.c */

/*
 * This function contains utilities for the file based implementation of
 * the keytab.  There are no public functions in this file.
 *
 * This file is the only one that has knowledge of the format of a
 * keytab file.
 *
 * The format is as follows:
 *
 * <file format vno>
 * <record length>
 * principal timestamp vno key
 * <record length>
 * principal timestamp vno key
 * ....
 *
 * A length field (sizeof(krb5_int32)) exists between entries.  When this
 * length is positive it indicates an active entry, when negative a hole.
 * The length indicates the size of the block in the file (this may be
 * larger than the size of the next record, since we are using a first
 * fit algorithm for re-using holes and the first fit may be larger than
 * the entry we are writing).  Another (compatible) implementation could
 * break up holes when allocating them to smaller entries to minimize
 * wasted space.  (Such an implementation should also coalesce adjacent
 * holes to reduce fragmentation).  This implementation does neither.
 *
 * There are no separators between fields of an entry.
 * A principal is a length-encoded array of length-encoded strings.  The
 * length is a krb5_int16 in each case.  The specific format, then, is
 * multiple entries concatenated with no separators.  An entry has this
 * exact format:
 *
 * sizeof(krb5_int16) bytes for number of components in the principal;
 * then, each component listed in ordser.
 * For each component, sizeof(krb5_int16) bytes for the number of bytes
 * in the component, followed by the component.
 * sizeof(krb5_int32) for the principal type (for KEYTAB V2 and higher)
 * sizeof(krb5_int32) bytes for the timestamp
 * sizeof(krb5_octet) bytes for the key version number
 * sizeof(krb5_int16) bytes for the enctype
 * sizeof(krb5_int16) bytes for the key length, followed by the key
 */

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#endif

typedef krb5_int16  krb5_kt_vno;

#define krb5_kt_default_vno ((krb5_kt_vno)KRB5_KT_DEFAULT_VNO)

static krb5_error_code
krb5_ktfileint_open(krb5_context context, krb5_keytab id, int mode)
{
    krb5_error_code kerror;
    krb5_kt_vno kt_vno;
    int writevno = 0;

    KTCHECKLOCK(id);
    errno = 0;
    KTFILEP(id) = fopen(KTFILENAME(id),
                        (mode == KRB5_LOCKMODE_EXCLUSIVE) ? "rb+" : "rb");
    if (!KTFILEP(id)) {
        if ((mode == KRB5_LOCKMODE_EXCLUSIVE) && (errno == ENOENT)) {
            /* try making it first time around */
            k5_create_secure_file(context, KTFILENAME(id));
            errno = 0;
            KTFILEP(id) = fopen(KTFILENAME(id), "rb+");
            if (!KTFILEP(id))
                goto report_errno;
            writevno = 1;
        } else {
        report_errno:
            switch (errno) {
            case 0:
                /* XXX */
                return EMFILE;
            case ENOENT:
                k5_setmsg(context, ENOENT,
                          _("Key table file '%s' not found"), KTFILENAME(id));
                return ENOENT;
            default:
                return errno;
            }
        }
    }
    set_cloexec_file(KTFILEP(id));
    if ((kerror = krb5_lock_file(context, fileno(KTFILEP(id)), mode))) {
        (void) fclose(KTFILEP(id));
        KTFILEP(id) = 0;
        return kerror;
    }
    /* assume ANSI or BSD-style stdio */
    setbuf(KTFILEP(id), KTFILEBUFP(id));

    /* get the vno and verify it */
    if (writevno) {
        kt_vno = htons(krb5_kt_default_vno);
        KTVERSION(id) = krb5_kt_default_vno;
        if (!fwrite(&kt_vno, sizeof(kt_vno), 1, KTFILEP(id))) {
            kerror = errno;
            (void) krb5_unlock_file(context, fileno(KTFILEP(id)));
            (void) fclose(KTFILEP(id));
            KTFILEP(id) = 0;
            return kerror;
        }
    } else {
        /* gotta verify it instead... */
        if (!fread(&kt_vno, sizeof(kt_vno), 1, KTFILEP(id))) {
            if (feof(KTFILEP(id)))
                kerror = KRB5_KEYTAB_BADVNO;
            else
                kerror = errno;
            (void) krb5_unlock_file(context, fileno(KTFILEP(id)));
            (void) fclose(KTFILEP(id));
            KTFILEP(id) = 0;
            return kerror;
        }
        kt_vno = KTVERSION(id) = ntohs(kt_vno);
        if ((kt_vno != KRB5_KT_VNO) &&
            (kt_vno != KRB5_KT_VNO_1)) {
            (void) krb5_unlock_file(context, fileno(KTFILEP(id)));
            (void) fclose(KTFILEP(id));
            KTFILEP(id) = 0;
            return KRB5_KEYTAB_BADVNO;
        }
    }
    KTSTARTOFF(id) = ftell(KTFILEP(id));
    return 0;
}

static krb5_error_code
krb5_ktfileint_openr(krb5_context context, krb5_keytab id)
{
    return krb5_ktfileint_open(context, id, KRB5_LOCKMODE_SHARED);
}

static krb5_error_code
krb5_ktfileint_openw(krb5_context context, krb5_keytab id)
{
    return krb5_ktfileint_open(context, id, KRB5_LOCKMODE_EXCLUSIVE);
}

static krb5_error_code
krb5_ktfileint_close(krb5_context context, krb5_keytab id)
{
    krb5_error_code kerror;

    KTCHECKLOCK(id);
    if (!KTFILEP(id))
        return 0;
    kerror = krb5_unlock_file(context, fileno(KTFILEP(id)));
    (void) fclose(KTFILEP(id));
    KTFILEP(id) = 0;
    return kerror;
}

static krb5_error_code
krb5_ktfileint_delete_entry(krb5_context context, krb5_keytab id, krb5_int32 delete_point)
{
    krb5_int32  size;
    krb5_int32  len;
    char        iobuf[BUFSIZ];

    KTCHECKLOCK(id);
    if (fseek(KTFILEP(id), delete_point, SEEK_SET)) {
        return errno;
    }
    if (!fread(&size, sizeof(size), 1, KTFILEP(id))) {
        return KRB5_KT_END;
    }
    if (KTVERSION(id) != KRB5_KT_VNO_1)
        size = ntohl(size);

    if (size > 0) {
        krb5_int32 minus_size = -size;
        if (KTVERSION(id) != KRB5_KT_VNO_1)
            minus_size = htonl(minus_size);

        if (fseek(KTFILEP(id), delete_point, SEEK_SET)) {
            return errno;
        }

        if (!fwrite(&minus_size, sizeof(minus_size), 1, KTFILEP(id))) {
            return KRB5_KT_IOERR;
        }

        if (size < BUFSIZ) {
            len = size;
        } else {
            len = BUFSIZ;
        }

        memset(iobuf, 0, (size_t) len);
        while (size > 0) {
            if (!fwrite(iobuf, 1, (size_t) len, KTFILEP(id))) {
                return KRB5_KT_IOERR;
            }
            size -= len;
            if (size < len) {
                len = size;
            }
        }

        return k5_sync_disk_file(context, KTFILEP(id));
    }

    return 0;
}

static krb5_error_code
krb5_ktfileint_internal_read_entry(krb5_context context, krb5_keytab id, krb5_keytab_entry *ret_entry, krb5_int32 *delete_point)
{
    krb5_octet vno;
    krb5_int16 count;
    unsigned int u_count, u_princ_size;
    krb5_int16 enctype;
    krb5_int16 princ_size;
    int i;
    krb5_int32 size;
    krb5_int32 start_pos, pos;
    krb5_error_code error;
    char        *tmpdata;
    krb5_data   *princ;
    uint32_t    vno32;

    KTCHECKLOCK(id);
    memset(ret_entry, 0, sizeof(krb5_keytab_entry));
    ret_entry->magic = KV5M_KEYTAB_ENTRY;

    /* fseek to synchronise buffered I/O on the key table. */

    if (fseek(KTFILEP(id), 0L, SEEK_CUR) < 0)
    {
        return errno;
    }

    do {
        *delete_point = ftell(KTFILEP(id));
        if (!fread(&size, sizeof(size), 1, KTFILEP(id))) {
            return KRB5_KT_END;
        }
        if (KTVERSION(id) != KRB5_KT_VNO_1)
            size = ntohl(size);

        if (size < 0) {
            if (size == INT32_MIN)  /* INT32_MIN inverts to itself. */
                return KRB5_KT_FORMAT;
            if (fseek(KTFILEP(id), -size, SEEK_CUR)) {
                return errno;
            }
        }
    } while (size < 0);

    if (size == 0) {
        return KRB5_KT_END;
    }

    start_pos = ftell(KTFILEP(id));

    /* deal with guts of parsing... */

    /* first, int16 with #princ components */
    if (!fread(&count, sizeof(count), 1, KTFILEP(id)))
        return KRB5_KT_END;
    if (KTVERSION(id) == KRB5_KT_VNO_1) {
        count -= 1;         /* V1 includes the realm in the count */
    } else {
        count = ntohs(count);
    }
    if (!count || (count < 0))
        return KRB5_KT_END;
    ret_entry->principal = (krb5_principal)malloc(sizeof(krb5_principal_data));
    if (!ret_entry->principal)
        return ENOMEM;

    u_count = count;
    ret_entry->principal->magic = KV5M_PRINCIPAL;
    ret_entry->principal->length = u_count;
    ret_entry->principal->data = (krb5_data *)
        calloc(u_count, sizeof(krb5_data));
    if (!ret_entry->principal->data) {
        free(ret_entry->principal);
        ret_entry->principal = 0;
        return ENOMEM;
    }

    /* Now, get the realm data */
    if (!fread(&princ_size, sizeof(princ_size), 1, KTFILEP(id))) {
        error = KRB5_KT_END;
        goto fail;
    }
    if (KTVERSION(id) != KRB5_KT_VNO_1)
        princ_size = ntohs(princ_size);
    if (!princ_size || (princ_size < 0)) {
        error = KRB5_KT_END;
        goto fail;
    }
    u_princ_size = princ_size;

    ret_entry->principal->realm.length = u_princ_size;
    tmpdata = malloc(u_princ_size+1);
    if (!tmpdata) {
        error = ENOMEM;
        goto fail;
    }
    if (fread(tmpdata, 1, u_princ_size, KTFILEP(id)) != (size_t) princ_size) {
        free(tmpdata);
        error = KRB5_KT_END;
        goto fail;
    }
    tmpdata[princ_size] = 0;    /* Some things might be expecting null */
                                /* termination...  ``Be conservative in */
                                /* what you send out'' */
    ret_entry->principal->realm.data = tmpdata;

    for (i = 0; i < count; i++) {
        princ = &ret_entry->principal->data[i];
        if (!fread(&princ_size, sizeof(princ_size), 1, KTFILEP(id))) {
            error = KRB5_KT_END;
            goto fail;
        }
        if (KTVERSION(id) != KRB5_KT_VNO_1)
            princ_size = ntohs(princ_size);
        if (!princ_size || (princ_size < 0)) {
            error = KRB5_KT_END;
            goto fail;
        }

        u_princ_size = princ_size;
        princ->length = u_princ_size;
        princ->data = malloc(u_princ_size+1);
        if (!princ->data) {
            error = ENOMEM;
            goto fail;
        }
        if (!fread(princ->data, sizeof(char), u_princ_size, KTFILEP(id))) {
            error = KRB5_KT_END;
            goto fail;
        }
        princ->data[princ_size] = 0; /* Null terminate */
    }

    /* read in the principal type, if we can get it */
    if (KTVERSION(id) != KRB5_KT_VNO_1) {
        if (!fread(&ret_entry->principal->type,
                   sizeof(ret_entry->principal->type), 1, KTFILEP(id))) {
            error = KRB5_KT_END;
            goto fail;
        }
        ret_entry->principal->type = ntohl(ret_entry->principal->type);
    }

    /* read in the timestamp */
    if (!fread(&ret_entry->timestamp, sizeof(ret_entry->timestamp), 1, KTFILEP(id))) {
        error = KRB5_KT_END;
        goto fail;
    }
    if (KTVERSION(id) != KRB5_KT_VNO_1)
        ret_entry->timestamp = ntohl(ret_entry->timestamp);

    /* read in the version number */
    if (!fread(&vno, sizeof(vno), 1, KTFILEP(id))) {
        error = KRB5_KT_END;
        goto fail;
    }
    ret_entry->vno = (krb5_kvno)vno;

    /* key type */
    if (!fread(&enctype, sizeof(enctype), 1, KTFILEP(id))) {
        error = KRB5_KT_END;
        goto fail;
    }
    if (KTVERSION(id) != KRB5_KT_VNO_1)
        enctype = ntohs(enctype);
    ret_entry->key.enctype = (krb5_enctype)enctype;

    /* key contents */
    ret_entry->key.magic = KV5M_KEYBLOCK;

    if (!fread(&count, sizeof(count), 1, KTFILEP(id))) {
        error = KRB5_KT_END;
        goto fail;
    }
    if (KTVERSION(id) != KRB5_KT_VNO_1)
        count = ntohs(count);
    if (!count || (count < 0)) {
        error = KRB5_KT_END;
        goto fail;
    }

    u_count = count;
    ret_entry->key.length = u_count;

    ret_entry->key.contents = (krb5_octet *)malloc(u_count);
    if (!ret_entry->key.contents) {
        error = ENOMEM;
        goto fail;
    }
    if (!fread(ret_entry->key.contents, sizeof(krb5_octet), count,
               KTFILEP(id))) {
        error = KRB5_KT_END;
        goto fail;
    }

    /* Check for a 32-bit kvno extension if four or more bytes remain. */
    pos = ftell(KTFILEP(id));
    if (pos - start_pos + 4 <= size) {
        if (!fread(&vno32, sizeof(vno32), 1, KTFILEP(id))) {
            error = KRB5_KT_END;
            goto fail;
        }
        if (KTVERSION(id) != KRB5_KT_VNO_1)
            vno32 = ntohl(vno32);
        /* If the value is 0, the bytes are just zero-fill. */
        if (vno32)
            ret_entry->vno = vno32;
    }

    /*
     * Reposition file pointer to the next inter-record length field.
     */
    if (fseek(KTFILEP(id), start_pos + size, SEEK_SET) == -1) {
        error = errno;
        goto fail;
    }

    return 0;
fail:

    for (i = 0; i < ret_entry->principal->length; i++)
        free(ret_entry->principal->data[i].data);
    free(ret_entry->principal->data);
    ret_entry->principal->data = 0;
    free(ret_entry->principal);
    ret_entry->principal = 0;
    return error;
}

static krb5_error_code
krb5_ktfileint_read_entry(krb5_context context, krb5_keytab id, krb5_keytab_entry *entryp)
{
    krb5_int32 delete_point;

    return krb5_ktfileint_internal_read_entry(context, id, entryp, &delete_point);
}

static krb5_error_code
krb5_ktfileint_write_entry(krb5_context context, krb5_keytab id, krb5_keytab_entry *entry)
{
    krb5_octet vno;
    krb5_data *princ;
    krb5_int16 count, size, enctype;
    krb5_error_code retval = 0;
    krb5_timestamp timestamp;
    krb5_int32  princ_type;
    krb5_int32  size_needed;
    krb5_int32  commit_point = -1;
    uint32_t    vno32;
    int         i;

    KTCHECKLOCK(id);
    retval = krb5_ktfileint_size_entry(context, entry, &size_needed);
    if (retval)
        return retval;
    retval = krb5_ktfileint_find_slot(context, id, &size_needed, &commit_point);
    if (retval)
        return retval;

    /* fseek to synchronise buffered I/O on the key table. */
    /* XXX Without the weird setbuf crock, can we get rid of this now?  */
    if (fseek(KTFILEP(id), 0L, SEEK_CUR) < 0)
    {
        return errno;
    }

    if (KTVERSION(id) == KRB5_KT_VNO_1) {
        count = (krb5_int16)entry->principal->length + 1;
    } else {
        count = htons((u_short)entry->principal->length);
    }

    if (!fwrite(&count, sizeof(count), 1, KTFILEP(id))) {
    abend:
        return KRB5_KT_IOERR;
    }
    size = entry->principal->realm.length;
    if (KTVERSION(id) != KRB5_KT_VNO_1)
        size = htons(size);
    if (!fwrite(&size, sizeof(size), 1, KTFILEP(id))) {
        goto abend;
    }
    if (!fwrite(entry->principal->realm.data, sizeof(char),
                entry->principal->realm.length, KTFILEP(id))) {
        goto abend;
    }

    count = (krb5_int16)entry->principal->length;
    for (i = 0; i < count; i++) {
        princ = &entry->principal->data[i];
        size = princ->length;
        if (KTVERSION(id) != KRB5_KT_VNO_1)
            size = htons(size);
        if (!fwrite(&size, sizeof(size), 1, KTFILEP(id))) {
            goto abend;
        }
        if (!fwrite(princ->data, sizeof(char), princ->length, KTFILEP(id))) {
            goto abend;
        }
    }

    /*
     * Write out the principal type
     */
    if (KTVERSION(id) != KRB5_KT_VNO_1) {
        princ_type = htonl(entry->principal->type);
        if (!fwrite(&princ_type, sizeof(princ_type), 1, KTFILEP(id))) {
            goto abend;
        }
    }

    /*
     * Fill in the time of day the entry was written to the keytab.
     */
    if (krb5_timeofday(context, &entry->timestamp)) {
        entry->timestamp = 0;
    }
    if (KTVERSION(id) == KRB5_KT_VNO_1)
        timestamp = entry->timestamp;
    else
        timestamp = htonl(entry->timestamp);
    if (!fwrite(&timestamp, sizeof(timestamp), 1, KTFILEP(id))) {
        goto abend;
    }

    /* key version number */
    vno = (krb5_octet)entry->vno;
    if (!fwrite(&vno, sizeof(vno), 1, KTFILEP(id))) {
        goto abend;
    }
    /* key type */
    if (KTVERSION(id) == KRB5_KT_VNO_1)
        enctype = entry->key.enctype;
    else
        enctype = htons(entry->key.enctype);
    if (!fwrite(&enctype, sizeof(enctype), 1, KTFILEP(id))) {
        goto abend;
    }
    /* key length */
    if (KTVERSION(id) == KRB5_KT_VNO_1)
        size = entry->key.length;
    else
        size = htons(entry->key.length);
    if (!fwrite(&size, sizeof(size), 1, KTFILEP(id))) {
        goto abend;
    }
    if (!fwrite(entry->key.contents, sizeof(krb5_octet),
                entry->key.length, KTFILEP(id))) {
        goto abend;
    }

    /* 32-bit key version number */
    vno32 = entry->vno;
    if (KTVERSION(id) != KRB5_KT_VNO_1)
        vno32 = htonl(vno32);
    if (!fwrite(&vno32, sizeof(vno32), 1, KTFILEP(id)))
        goto abend;

    if (fflush(KTFILEP(id)))
        goto abend;

    retval = k5_sync_disk_file(context, KTFILEP(id));

    if (retval) {
        return retval;
    }

    if (fseek(KTFILEP(id), commit_point, SEEK_SET)) {
        return errno;
    }
    if (KTVERSION(id) != KRB5_KT_VNO_1)
        size_needed = htonl(size_needed);
    if (!fwrite(&size_needed, sizeof(size_needed), 1, KTFILEP(id))) {
        goto abend;
    }
    if (fflush(KTFILEP(id)))
        goto abend;
    retval = k5_sync_disk_file(context, KTFILEP(id));

    return retval;
}

/*
 * Determine the size needed for a file entry for the given
 * keytab entry.
 */
static krb5_error_code
krb5_ktfileint_size_entry(krb5_context context, krb5_keytab_entry *entry, krb5_int32 *size_needed)
{
    krb5_int16 count;
    krb5_int32 total_size, i;
    krb5_error_code retval = 0;

    count = (krb5_int16)entry->principal->length;

    total_size = sizeof(count);
    total_size += entry->principal->realm.length + sizeof(krb5_int16);

    for (i = 0; i < count; i++)
        total_size += entry->principal->data[i].length + sizeof(krb5_int16);

    total_size += sizeof(entry->principal->type);
    total_size += sizeof(entry->timestamp);
    total_size += sizeof(krb5_octet);
    total_size += sizeof(krb5_int16);
    total_size += sizeof(krb5_int16) + entry->key.length;
    total_size += sizeof(uint32_t);

    *size_needed = total_size;
    return retval;
}

/*
 * Find and reserve a slot in the file for an entry of the needed size.
 * The commit point will be set to the position in the file where the
 * the length (sizeof(krb5_int32) bytes) of this node should be written
 * when committing the write.  The file position left as a result of this
 * call is the position where the actual data should be written.
 *
 * The size_needed argument may be adjusted if we find a hole that is
 * larger than the size needed.  (Recall that size_needed will be used
 * to commit the write, but that this field must indicate the size of the
 * block in the file rather than the size of the actual entry)
 */
static krb5_error_code
krb5_ktfileint_find_slot(krb5_context context, krb5_keytab id, krb5_int32 *size_needed, krb5_int32 *commit_point_ptr)
{
    FILE *fp;
    krb5_int32 size, zero_point, commit_point;
    krb5_kt_vno kt_vno;

    KTCHECKLOCK(id);
    fp = KTFILEP(id);
    /* Skip over file version number. */
    if (fseek(fp, 0, SEEK_SET))
        return errno;
    if (!fread(&kt_vno, sizeof(kt_vno), 1, fp))
        return errno;

    for (;;) {
        commit_point = ftell(fp);
        if (commit_point == -1)
            return errno;
        if (!fread(&size, sizeof(size), 1, fp)) {
            /* Hit the end of file, reserve this slot. */
            /* Necessary to avoid a later fseek failing on Solaris 10. */
            if (fseek(fp, 0, SEEK_CUR))
                return errno;
            /* htonl(0) is 0, so no need to worry about byte order */
            size = 0;
            if (!fwrite(&size, sizeof(size), 1, fp))
                return errno;
            break;
        }

        if (KTVERSION(id) != KRB5_KT_VNO_1)
            size = ntohl(size);

        if (size > 0) {
            /* Non-empty record; seek past it. */
            if (fseek(fp, size, SEEK_CUR))
                return errno;
        } else if (size < 0) {
            /* Empty record; use if it's big enough, seek past otherwise. */
            if (size == INT32_MIN)  /* INT32_MIN inverts to itself. */
                return KRB5_KT_FORMAT;
            size = -size;
            if (size >= *size_needed) {
                *size_needed = size;
                break;
            } else {
                if (fseek(fp, size, SEEK_CUR))
                    return errno;
            }
        } else {
            /* Empty record at end of file; use it. */
            /* Ensure the new record will be followed by another 0. */
            zero_point = ftell(fp);
            if (zero_point == -1)
                return errno;
            if (fseek(fp, *size_needed, SEEK_CUR))
                return errno;
            /* htonl(0) is 0, so no need to worry about byte order */
            if (!fwrite(&size, sizeof(size), 1, fp))
                return errno;
            if (fseek(fp, zero_point, SEEK_SET))
                return errno;
            break;
        }
    }

    *commit_point_ptr = commit_point;
    return 0;
}
#endif /* LEAN_CLIENT */
