/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/keytab/kt_srvtab.c */
/*
 * Copyright 1990,1991,2002,2007,2008 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include <stdio.h>

#ifndef LEAN_CLIENT

/*
 * Constants
 */

#define KRB5_KT_VNO_1   0x0501  /* krb v5, keytab version 1 (DCE compat) */
#define KRB5_KT_VNO     0x0502  /* krb v5, keytab version 2 (standard)  */

#define KRB5_KT_DEFAULT_VNO KRB5_KT_VNO

/*
 * Types
 */
typedef struct _krb5_ktsrvtab_data {
    char *name;                 /* Name of the file */
    FILE *openf;                /* open file, if any. */
} krb5_ktsrvtab_data;

/*
 * Macros
 */
#define KTPRIVATE(id) ((krb5_ktsrvtab_data *)(id)->data)
#define KTFILENAME(id) (((krb5_ktsrvtab_data *)(id)->data)->name)
#define KTFILEP(id) (((krb5_ktsrvtab_data *)(id)->data)->openf)

extern const struct _krb5_kt_ops krb5_kts_ops;

static krb5_error_code KRB5_CALLCONV
krb5_ktsrvtab_resolve(krb5_context, const char *, krb5_keytab *);

static krb5_error_code KRB5_CALLCONV
krb5_ktsrvtab_get_name(krb5_context, krb5_keytab, char *, unsigned int);

static krb5_error_code KRB5_CALLCONV
krb5_ktsrvtab_close(krb5_context, krb5_keytab);

static krb5_error_code KRB5_CALLCONV
krb5_ktsrvtab_get_entry(krb5_context, krb5_keytab, krb5_const_principal,
                        krb5_kvno, krb5_enctype, krb5_keytab_entry *);

static krb5_error_code KRB5_CALLCONV
krb5_ktsrvtab_start_seq_get(krb5_context, krb5_keytab, krb5_kt_cursor *);

static krb5_error_code KRB5_CALLCONV
krb5_ktsrvtab_get_next(krb5_context, krb5_keytab, krb5_keytab_entry *,
                       krb5_kt_cursor *);

static krb5_error_code KRB5_CALLCONV
krb5_ktsrvtab_end_get(krb5_context, krb5_keytab, krb5_kt_cursor *);

static krb5_error_code
krb5_ktsrvint_open(krb5_context, krb5_keytab);

static krb5_error_code
krb5_ktsrvint_close(krb5_context, krb5_keytab);

static krb5_error_code
krb5_ktsrvint_read_entry(krb5_context, krb5_keytab, krb5_keytab_entry *);

/*
 * This is an implementation specific resolver.  It returns a keytab id
 * initialized with srvtab keytab routines.
 */

static krb5_error_code KRB5_CALLCONV
krb5_ktsrvtab_resolve(krb5_context context, const char *name, krb5_keytab *id)
{
    krb5_ktsrvtab_data *data;

    if ((*id = (krb5_keytab) malloc(sizeof(**id))) == NULL)
        return(ENOMEM);

    (*id)->ops = &krb5_kts_ops;
    data = (krb5_ktsrvtab_data *)malloc(sizeof(krb5_ktsrvtab_data));
    if (data == NULL) {
        free(*id);
        return(ENOMEM);
    }

    data->name = strdup(name);
    if (data->name == NULL) {
        free(data);
        free(*id);
        return(ENOMEM);
    }

    data->openf = 0;

    (*id)->data = (krb5_pointer)data;
    (*id)->magic = KV5M_KEYTAB;
    return(0);
}

/*
 * "Close" a file-based keytab and invalidate the id.  This means
 * free memory hidden in the structures.
 */

krb5_error_code KRB5_CALLCONV
krb5_ktsrvtab_close(krb5_context context, krb5_keytab id)
/*
 * This routine is responsible for freeing all memory allocated
 * for this keytab.  There are no system resources that need
 * to be freed nor are there any open files.
 *
 * This routine should undo anything done by krb5_ktsrvtab_resolve().
 */
{
    free(KTFILENAME(id));
    free(id->data);
    id->ops = 0;
    free(id);
    return (0);
}

/*
 * This is the get_entry routine for the file based keytab implementation.
 * It opens the keytab file, and either retrieves the entry or returns
 * an error.
 */

krb5_error_code KRB5_CALLCONV
krb5_ktsrvtab_get_entry(krb5_context context, krb5_keytab id, krb5_const_principal principal, krb5_kvno kvno, krb5_enctype enctype, krb5_keytab_entry *entry)
{
    krb5_keytab_entry best_entry, ent;
    krb5_error_code kerror = 0;
    int found_wrong_kvno = 0;

    /* Open the srvtab. */
    if ((kerror = krb5_ktsrvint_open(context, id)))
        return(kerror);

    /* srvtab files only have DES_CBC_CRC keys. */
    switch (enctype) {
    case ENCTYPE_DES_CBC_CRC:
    case ENCTYPE_DES_CBC_MD5:
    case ENCTYPE_DES_CBC_MD4:
    case ENCTYPE_DES_CBC_RAW:
    case IGNORE_ENCTYPE:
        break;
    default:
        return KRB5_KT_NOTFOUND;
    }

    best_entry.principal = 0;
    best_entry.vno = 0;
    best_entry.key.contents = 0;
    while ((kerror = krb5_ktsrvint_read_entry(context, id, &ent)) == 0) {
        ent.key.enctype = enctype;
        if (krb5_principal_compare(context, principal, ent.principal)) {
            if (kvno == IGNORE_VNO) {
                if (!best_entry.principal || (best_entry.vno < ent.vno)) {
                    krb5_kt_free_entry(context, &best_entry);
                    best_entry = ent;
                }
            } else {
                if (ent.vno == kvno) {
                    best_entry = ent;
                    break;
                } else {
                    found_wrong_kvno = 1;
                }
            }
        } else {
            krb5_kt_free_entry(context, &ent);
        }
    }
    if (kerror == KRB5_KT_END) {
        if (best_entry.principal)
            kerror = 0;
        else if (found_wrong_kvno)
            kerror = KRB5_KT_KVNONOTFOUND;
        else
            kerror = KRB5_KT_NOTFOUND;
    }
    if (kerror) {
        (void) krb5_ktsrvint_close(context, id);
        krb5_kt_free_entry(context, &best_entry);
        return kerror;
    }
    if ((kerror = krb5_ktsrvint_close(context, id)) != 0) {
        krb5_kt_free_entry(context, &best_entry);
        return kerror;
    }
    *entry = best_entry;
    return 0;
}

/*
 * Get the name of the file containing a srvtab-based keytab.
 */

krb5_error_code KRB5_CALLCONV
krb5_ktsrvtab_get_name(krb5_context context, krb5_keytab id, char *name, unsigned int len)
/*
 * This routine returns the name of the name of the file associated with
 * this srvtab-based keytab.  The name is prefixed with PREFIX:, so that
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
 * krb5_ktsrvtab_start_seq_get()
 */

krb5_error_code KRB5_CALLCONV
krb5_ktsrvtab_start_seq_get(krb5_context context, krb5_keytab id, krb5_kt_cursor *cursorp)
{
    krb5_error_code retval;
    long *fileoff;

    if ((retval = krb5_ktsrvint_open(context, id)))
        return retval;

    if (!(fileoff = (long *)malloc(sizeof(*fileoff)))) {
        krb5_ktsrvint_close(context, id);
        return ENOMEM;
    }
    *fileoff = ftell(KTFILEP(id));
    *cursorp = (krb5_kt_cursor)fileoff;

    return 0;
}

/*
 * krb5_ktsrvtab_get_next()
 */

krb5_error_code KRB5_CALLCONV
krb5_ktsrvtab_get_next(krb5_context context, krb5_keytab id, krb5_keytab_entry *entry, krb5_kt_cursor *cursor)
{
    long *fileoff = (long *)*cursor;
    krb5_keytab_entry cur_entry;
    krb5_error_code kerror;

    if (fseek(KTFILEP(id), *fileoff, 0) == -1)
        return KRB5_KT_END;
    if ((kerror = krb5_ktsrvint_read_entry(context, id, &cur_entry)))
        return kerror;
    *fileoff = ftell(KTFILEP(id));
    *entry = cur_entry;
    return 0;
}

/*
 * krb5_ktsrvtab_end_get()
 */

krb5_error_code KRB5_CALLCONV
krb5_ktsrvtab_end_get(krb5_context context, krb5_keytab id, krb5_kt_cursor *cursor)
{
    free(*cursor);
    return krb5_ktsrvint_close(context, id);
}

/*
 * krb5_kts_ops
 */

const struct _krb5_kt_ops krb5_kts_ops = {
    0,
    "SRVTAB",   /* Prefix -- this string should not appear anywhere else! */
    krb5_ktsrvtab_resolve,
    krb5_ktsrvtab_get_name,
    krb5_ktsrvtab_close,
    krb5_ktsrvtab_get_entry,
    krb5_ktsrvtab_start_seq_get,
    krb5_ktsrvtab_get_next,
    krb5_ktsrvtab_end_get,
    0,
    0,
    0
};

/* formerly: lib/krb5/keytab/srvtab/kts_util.c */

#include <stdio.h>

/* The maximum sizes for V4 aname, realm, sname, and instance +1 */
/* Taken from krb.h */
#define         ANAME_SZ        40
#define         REALM_SZ        40
#define         SNAME_SZ        40
#define         INST_SZ         40

static krb5_error_code
read_field(FILE *fp, char *s, int len)
{
    int c;

    while ((c = getc(fp)) != 0) {
        if (c == EOF || len <= 1)
            return KRB5_KT_END;
        *s = c;
        s++;
        len--;
    }
    *s = 0;
    return 0;
}

krb5_error_code
krb5_ktsrvint_open(krb5_context context, krb5_keytab id)
{
    KTFILEP(id) = fopen(KTFILENAME(id), "rb");
    if (!KTFILEP(id))
        return errno;
    set_cloexec_file(KTFILEP(id));
    return 0;
}

krb5_error_code
krb5_ktsrvint_close(krb5_context context, krb5_keytab id)
{
    if (!KTFILEP(id))
        return 0;
    (void) fclose(KTFILEP(id));
    KTFILEP(id) = 0;
    return 0;
}

krb5_error_code
krb5_ktsrvint_read_entry(krb5_context context, krb5_keytab id, krb5_keytab_entry *ret_entry)
{
    FILE *fp;
    char name[SNAME_SZ], instance[INST_SZ], realm[REALM_SZ];
    unsigned char key[8];
    int vno;
    krb5_error_code kerror;

    /* Read in an entry from the srvtab file. */
    fp = KTFILEP(id);
    kerror = read_field(fp, name, sizeof(name));
    if (kerror != 0)
        return kerror;
    kerror = read_field(fp, instance, sizeof(instance));
    if (kerror != 0)
        return kerror;
    kerror = read_field(fp, realm, sizeof(realm));
    if (kerror != 0)
        return kerror;
    vno = getc(fp);
    if (vno == EOF)
        return KRB5_KT_END;
    if (fread(key, 1, sizeof(key), fp) != sizeof(key))
        return KRB5_KT_END;

    /* Fill in ret_entry with the data we read.  Everything maps well
     * except for the timestamp, which we don't have a value for.  For
     * now we just set it to 0. */
    memset(ret_entry, 0, sizeof(*ret_entry));
    ret_entry->magic = KV5M_KEYTAB_ENTRY;
    kerror = krb5_425_conv_principal(context, name, instance, realm,
                                     &ret_entry->principal);
    if (kerror != 0)
        return kerror;
    ret_entry->vno = vno;
    ret_entry->timestamp = 0;
    ret_entry->key.enctype = ENCTYPE_DES_CBC_CRC;
    ret_entry->key.magic = KV5M_KEYBLOCK;
    ret_entry->key.length = sizeof(key);
    ret_entry->key.contents = k5memdup(key, sizeof(key), &kerror);
    if (ret_entry->key.contents == NULL) {
        krb5_free_principal(context, ret_entry->principal);
        return kerror;
    }

    return 0;
}
#endif /* LEAN_CLIENT */
