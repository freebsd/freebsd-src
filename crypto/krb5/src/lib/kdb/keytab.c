/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/kdb/keytab.c */
/*
 * Copyright 1995 by the Massachusetts Institute of Technology.
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

#include <string.h>

#include "k5-int.h"
#include "kdb_kt.h"

static int
is_xrealm_tgt(krb5_context, krb5_const_principal);

krb5_error_code krb5_ktkdb_close (krb5_context, krb5_keytab);

krb5_error_code krb5_ktkdb_get_entry (krb5_context, krb5_keytab, krb5_const_principal,
                                      krb5_kvno, krb5_enctype, krb5_keytab_entry *);

static krb5_error_code
krb5_ktkdb_get_name(krb5_context context, krb5_keytab keytab,
                    char *name, unsigned int namelen)
{
    if (strlcpy(name, "KDB:", namelen) >= namelen)
        return KRB5_KT_NAME_TOOLONG;
    return 0;
}

krb5_kt_ops krb5_kt_kdb_ops = {
    0,
    "KDB",      /* Prefix -- this string should not appear anywhere else! */
    krb5_ktkdb_resolve,         /* resolve */
    krb5_ktkdb_get_name,        /* get_name */
    krb5_ktkdb_close,           /* close */
    krb5_ktkdb_get_entry,       /* get */
    NULL,                       /* start_seq_get */
    NULL,                       /* get_next */
    NULL,                       /* end_get */
    NULL,                       /* add (extended) */
    NULL,                       /* remove (extended) */
};

typedef struct krb5_ktkdb_data {
    char * name;
} krb5_ktkdb_data;

krb5_error_code
krb5_db_register_keytab(krb5_context context)
{
    return krb5_kt_register(context, &krb5_kt_kdb_ops);
}

krb5_error_code
krb5_ktkdb_resolve(context, name, id)
    krb5_context          context;
    const char          * name;
    krb5_keytab         * id;
{
    if ((*id = (krb5_keytab) malloc(sizeof(**id))) == NULL)
        return(ENOMEM);
    (*id)->ops = &krb5_kt_kdb_ops;
    (*id)->magic = KV5M_KEYTAB;
    return(0);
}

krb5_error_code
krb5_ktkdb_close(context, kt)
    krb5_context context;
    krb5_keytab kt;
{
    /*
     * This routine is responsible for freeing all memory allocated
     * for this keytab.  There are no system resources that need
     * to be freed nor are there any open files.
     *
     * This routine should undo anything done by krb5_ktkdb_resolve().
     */

    kt->ops = NULL;
    free(kt);

    return 0;
}

static krb5_context ktkdb_ctx = NULL;

/*
 * Set a different context for use with ktkdb_get_entry().  This is
 * primarily useful for kadmind, where the gssapi library context,
 * which will be used for the keytab, will necessarily have a
 * different context than that used by the kadm5 library to access the
 * database for its own purposes.
 */
krb5_error_code
krb5_ktkdb_set_context(krb5_context ctx)
{
    ktkdb_ctx = ctx;
    return 0;
}

krb5_error_code
krb5_ktkdb_get_entry(in_context, id, principal, kvno, enctype, entry)
    krb5_context          in_context;
    krb5_keytab           id;
    krb5_const_principal  principal;
    krb5_kvno             kvno;
    krb5_enctype          enctype;
    krb5_keytab_entry   * entry;
{
    krb5_context          context;
    krb5_error_code       kerror = 0;
    krb5_key_data       * key_data;
    krb5_db_entry       * db_entry;
    int xrealm_tgt;
    krb5_boolean similar;

    if (ktkdb_ctx)
        context = ktkdb_ctx;
    else
        context = in_context;

    xrealm_tgt = is_xrealm_tgt(context, principal);

    /* Check whether database is inited. Open is commented */
    if ((kerror = krb5_db_inited(context)))
        return(kerror);

    /* get_principal */
    kerror = krb5_db_get_principal(context, principal, 0, &db_entry);
    if (kerror == KRB5_KDB_NOENTRY)
        return(KRB5_KT_NOTFOUND);
    if (kerror)
        return(kerror);

    if (db_entry->attributes & KRB5_KDB_DISALLOW_SVR
        || db_entry->attributes & KRB5_KDB_DISALLOW_ALL_TIX) {
        kerror = KRB5_KT_NOTFOUND;
        goto error;
    }

    /* match key */
    /* For cross realm tgts, we match whatever enctype is provided;
     * for other principals, we only match the first enctype that is
     * found.  Since the TGS and AS code do the same thing, then we
     * will only successfully decrypt  tickets we have issued.*/
    kerror = krb5_dbe_find_enctype(context, db_entry,
                                   xrealm_tgt?enctype:-1,
                                   -1, kvno, &key_data);
    if (kerror == KRB5_KDB_NO_MATCHING_KEY)
        kerror = KRB5_KT_KVNONOTFOUND;
    if (kerror)
        goto error;


    kerror = krb5_dbe_decrypt_key_data(context, NULL, key_data,
                                       &entry->key, NULL);
    if (kerror)
        goto error;

    if (enctype > 0) {
        kerror = krb5_c_enctype_compare(context, enctype,
                                        entry->key.enctype, &similar);
        if (kerror)
            goto error;

        if (!similar) {
            kerror = KRB5_KDB_NO_PERMITTED_KEY;
            goto error;
        }
    }
    /*
     * Coerce the enctype of the output keyblock in case we got an
     * inexact match on the enctype.
     */
    entry->key.enctype = enctype;

    kerror = krb5_copy_principal(context, principal, &entry->principal);
    if (kerror)
        goto error;

    /* Close database */
error:
    krb5_db_free_principal(context, db_entry);
    /*    krb5_db_close_database(context); */
    return(kerror);
}

/*
 * is_xrealm_tgt: Returns true if the principal is a cross-realm  TGT
 * principal-- a principal with first component  krbtgt and second
 * component not equal to realm.
 */
static int
is_xrealm_tgt(krb5_context context, krb5_const_principal princ)
{
    krb5_data *dat;
    if (krb5_princ_size(context, princ) != 2)
        return 0;
    dat = krb5_princ_component(context, princ, 0);
    if (strncmp("krbtgt", dat->data, dat->length) != 0)
        return 0;
    dat = krb5_princ_component(context, princ, 1);
    if (dat->length != princ->realm.length)
        return 1;
    if (strncmp(dat->data, princ->realm.data, dat->length) == 0)
        return 0;
    return 1;

}
