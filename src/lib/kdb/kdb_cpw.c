/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/kdb/kdb_cpw.c */
/*
 * Copyright 1995, 2009, 2014 by the Massachusetts Institute of Technology.
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
 * Copyright (C) 1998 by the FundsXpress, INC.
 *
 * All rights reserved.
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
 * the name of FundsXpress. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  FundsXpress makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "k5-int.h"
#include "kdb.h"
#include <stdio.h>
#include <errno.h>

enum save { DISCARD_ALL, KEEP_LAST_KVNO, KEEP_ALL };

int
krb5_db_get_key_data_kvno(context, count, data)
    krb5_context          context;
    int                   count;
    krb5_key_data       * data;
{
    int i, kvno;
    /* Find last key version number */
    for (kvno = i = 0; i < count; i++) {
        if (kvno < data[i].key_data_kvno) {
            kvno = data[i].key_data_kvno;
        }
    }
    return(kvno);
}

static void
cleanup_key_data(context, count, data)
    krb5_context          context;
    int                   count;
    krb5_key_data       * data;
{
    int i;

    /* If data is NULL, count is always 0 */
    if (data == NULL) return;

    for (i = 0; i < count; i++)
        krb5_dbe_free_key_data_contents(context, &data[i]);
    free(data);
}

/* Transfer key data from old_kd to new_kd, making sure that new_kd is
 * encrypted with mkey.  May steal from old_kd and zero it out. */
static krb5_error_code
preserve_one_old_key(krb5_context context, krb5_keyblock *mkey,
                     krb5_db_entry *dbent, krb5_key_data *old_kd,
                     krb5_key_data *new_kd)
{
    krb5_error_code ret;
    krb5_keyblock kb;
    krb5_keysalt salt;

    memset(new_kd, 0, sizeof(*new_kd));

    ret = krb5_dbe_decrypt_key_data(context, mkey, old_kd, &kb, NULL);
    if (ret == 0) {
        /* old_kd is already encrypted in mkey, so just move it. */
        *new_kd = *old_kd;
        memset(old_kd, 0, sizeof(*old_kd));
        krb5_free_keyblock_contents(context, &kb);
        return 0;
    }

    /* Decrypt and re-encrypt old_kd using mkey. */
    ret = krb5_dbe_decrypt_key_data(context, NULL, old_kd, &kb, &salt);
    if (ret)
        return ret;
    ret = krb5_dbe_encrypt_key_data(context, mkey, &kb, &salt,
                                    old_kd->key_data_kvno, new_kd);
    krb5_free_keyblock_contents(context, &kb);
    krb5_free_data_contents(context, &salt.data);
    return ret;
}

/*
 * Add key_data to dbent, making sure that each entry is encrypted in mkey.  If
 * kvno is non-zero, preserve only keys of that kvno.  May steal some elements
 * from key_data and zero them out.
 */
static krb5_error_code
preserve_old_keys(krb5_context context, krb5_keyblock *mkey,
                  krb5_db_entry *dbent, int kvno, int n_key_data,
                  krb5_key_data *key_data)
{
    krb5_error_code ret;
    int i;

    for (i = 0; i < n_key_data; i++) {
        if (kvno != 0 && key_data[i].key_data_kvno != kvno)
            continue;
        ret = krb5_dbe_create_key_data(context, dbent);
        if (ret)
            return ret;
        ret = preserve_one_old_key(context, mkey, dbent, &key_data[i],
                                   &dbent->key_data[dbent->n_key_data - 1]);
        if (ret)
            return ret;
    }
    return 0;
}

static krb5_error_code
add_key_rnd(context, master_key, ks_tuple, ks_tuple_count, db_entry, kvno)
    krb5_context          context;
    krb5_keyblock       * master_key;
    krb5_key_salt_tuple * ks_tuple;
    int                   ks_tuple_count;
    krb5_db_entry       * db_entry;
    int                   kvno;
{
    krb5_keyblock         key;
    int                   i, j;
    krb5_error_code       retval;
    krb5_key_data        *kd_slot;

    for (i = 0; i < ks_tuple_count; i++) {
        krb5_boolean similar;

        similar = 0;

        /*
         * We could use krb5_keysalt_iterate to replace this loop, or use
         * krb5_keysalt_is_present for the loop below, but we want to avoid
         * circular library dependencies.
         */
        for (j = 0; j < i; j++) {
            if ((retval = krb5_c_enctype_compare(context,
                                                 ks_tuple[i].ks_enctype,
                                                 ks_tuple[j].ks_enctype,
                                                 &similar)))
                return(retval);

            if (similar)
                break;
        }

        if (similar)
            continue;

        if ((retval = krb5_dbe_create_key_data(context, db_entry)))
            return retval;
        kd_slot = &db_entry->key_data[db_entry->n_key_data - 1];

        /* there used to be code here to extract the old key, and derive
           a new key from it.  Now that there's a unified prng, that isn't
           necessary. */

        /* make new key */
        if ((retval = krb5_c_make_random_key(context, ks_tuple[i].ks_enctype,
                                             &key)))
            return retval;

        retval = krb5_dbe_encrypt_key_data(context, master_key, &key, NULL,
                                           kvno, kd_slot);

        krb5_free_keyblock_contents(context, &key);
        if( retval )
            return retval;
    }

    return 0;
}

/* Construct a random explicit salt. */
static krb5_error_code
make_random_salt(krb5_context context, krb5_keysalt *salt_out)
{
    krb5_error_code retval;
    unsigned char rndbuf[8];
    krb5_data salt, rnd = make_data(rndbuf, sizeof(rndbuf));
    unsigned int i;

    /*
     * Salts are limited by RFC 4120 to 7-bit ASCII.  For ease of examination
     * and to avoid certain folding issues for older enctypes, we use printable
     * characters with four fixed bits and four random bits, encoding 64
     * psuedo-random bits into 16 bytes.
     */
    retval = krb5_c_random_make_octets(context, &rnd);
    if (retval)
        return retval;
    retval = alloc_data(&salt, sizeof(rndbuf) * 2);
    if (retval)
        return retval;
    for (i = 0; i < sizeof(rndbuf); i++) {
        salt.data[i * 2] = 0x40 | (rndbuf[i] >> 4);
        salt.data[i * 2 + 1] = 0x40 | (rndbuf[i] & 0xf);
    }

    salt_out->type = KRB5_KDB_SALTTYPE_SPECIAL;
    salt_out->data = salt;
    return 0;
}

/*
 * Add key_data for a krb5_db_entry
 * If passwd is NULL the assumes that the caller wants a random password.
 */
static krb5_error_code
add_key_pwd(context, master_key, ks_tuple, ks_tuple_count, passwd,
            db_entry, kvno)
    krb5_context          context;
    krb5_keyblock       * master_key;
    krb5_key_salt_tuple * ks_tuple;
    int                   ks_tuple_count;
    const char          * passwd;
    krb5_db_entry       * db_entry;
    int                   kvno;
{
    krb5_error_code       retval;
    krb5_keysalt          key_salt;
    krb5_keyblock         key;
    krb5_data             pwd;
    krb5_data             afs_params = string2data("\1"), *s2k_params;
    int                   i, j;
    krb5_key_data        *kd_slot;

    for (i = 0; i < ks_tuple_count; i++) {
        krb5_boolean similar;

        similar = 0;
        s2k_params = NULL;

        /*
         * We could use krb5_keysalt_iterate to replace this loop, or use
         * krb5_keysalt_is_present for the loop below, but we want to avoid
         * circular library dependencies.
         */
        for (j = 0; j < i; j++) {
            if ((retval = krb5_c_enctype_compare(context,
                                                 ks_tuple[i].ks_enctype,
                                                 ks_tuple[j].ks_enctype,
                                                 &similar)))
                return(retval);

            if (similar &&
                (ks_tuple[j].ks_salttype == ks_tuple[i].ks_salttype))
                break;
        }

        if (j < i)
            continue;

        if ((retval = krb5_dbe_create_key_data(context, db_entry)))
            return(retval);
        kd_slot = &db_entry->key_data[db_entry->n_key_data - 1];

        /* Convert password string to key using appropriate salt */
        switch (key_salt.type = ks_tuple[i].ks_salttype) {
        case KRB5_KDB_SALTTYPE_ONLYREALM: {
            krb5_data * saltdata;
            if ((retval = krb5_copy_data(context, krb5_princ_realm(context,
                                                                   db_entry->princ), &saltdata)))
                return(retval);

            key_salt.data = *saltdata;
            free(saltdata);
        }
            break;
        case KRB5_KDB_SALTTYPE_NOREALM:
            if ((retval=krb5_principal2salt_norealm(context, db_entry->princ,
                                                    &key_salt.data)))
                return(retval);
            break;
        case KRB5_KDB_SALTTYPE_NORMAL:
            if ((retval = krb5_principal2salt(context, db_entry->princ,
                                              &key_salt.data)))
                return(retval);
            break;
        case KRB5_KDB_SALTTYPE_V4:
            key_salt.data.length = 0;
            key_salt.data.data = 0;
            break;
        case KRB5_KDB_SALTTYPE_AFS3:
            retval = krb5int_copy_data_contents(context,
                                                &db_entry->princ->realm,
                                                &key_salt.data);
            if (retval)
                return retval;
            s2k_params = &afs_params;
            break;
        case KRB5_KDB_SALTTYPE_SPECIAL:
            retval = make_random_salt(context, &key_salt);
            if (retval)
                return retval;
            break;
        default:
            return(KRB5_KDB_BAD_SALTTYPE);
        }

        pwd = string2data((char *)passwd);

        retval = krb5_c_string_to_key_with_params(context,
                                                  ks_tuple[i].ks_enctype,
                                                  &pwd, &key_salt.data,
                                                  s2k_params, &key);
        if (retval) {
            free(key_salt.data.data);
            return retval;
        }

        retval = krb5_dbe_encrypt_key_data(context, master_key, &key,
                                           (const krb5_keysalt *)&key_salt,
                                           kvno, kd_slot);
        if (key_salt.data.data)
            free(key_salt.data.data);
        free(key.contents);

        if( retval )
            return retval;
    }

    return 0;
}

static krb5_error_code
rekey(krb5_context context, krb5_keyblock *mkey, krb5_key_salt_tuple *ks_tuple,
      int ks_tuple_count, const char *password, int new_kvno,
      enum save savekeys, krb5_db_entry *db_entry)
{
    krb5_error_code ret;
    krb5_key_data *key_data;
    int n_key_data, old_kvno, save_kvno;

    /* Save aside the old key data. */
    n_key_data = db_entry->n_key_data;
    key_data = db_entry->key_data;
    db_entry->n_key_data = 0;
    db_entry->key_data = NULL;

    /* Make sure the new kvno is greater than the old largest kvno. */
    old_kvno = krb5_db_get_key_data_kvno(context, n_key_data, key_data);
    if (new_kvno < old_kvno + 1)
        new_kvno = old_kvno + 1;
    /* Wrap from 65535 to 1; we can only store 16-bit kvno values in key_data,
     * and we assign special meaning to kvno 0. */
    if (new_kvno == (1 << 16))
        new_kvno = 1;

    /* Add new keys to the front of the list. */
    if (password != NULL) {
        ret = add_key_pwd(context, mkey, ks_tuple, ks_tuple_count, password,
                          db_entry, new_kvno);
    } else {
        ret = add_key_rnd(context, mkey, ks_tuple, ks_tuple_count, db_entry,
                          new_kvno);
    }
    if (ret) {
        cleanup_key_data(context, db_entry->n_key_data, db_entry->key_data);
        db_entry->n_key_data = n_key_data;
        db_entry->key_data = key_data;
        return ret;
    }

    /* Possibly add some or all of the old keys to the back of the list.  May
     * steal from and zero out some of the old key data entries. */
    if (savekeys != DISCARD_ALL) {
        save_kvno = (savekeys == KEEP_LAST_KVNO) ? old_kvno : 0;
        ret = preserve_old_keys(context, mkey, db_entry, save_kvno, n_key_data,
                                key_data);
    }

    /* Free any old key data entries not stolen and zeroed out above. */
    cleanup_key_data(context, n_key_data, key_data);
    return ret;
}

/*
 * Change random key for a krb5_db_entry
 * Assumes the max kvno
 *
 * As a side effect all old keys are nuked if keepold is false.
 */
krb5_error_code
krb5_dbe_crk(krb5_context context, krb5_keyblock *mkey,
             krb5_key_salt_tuple *ks_tuple, int ks_tuple_count,
             krb5_boolean keepold, krb5_db_entry *dbent)
{
    return rekey(context, mkey, ks_tuple, ks_tuple_count, NULL, 0,
                 keepold ? KEEP_ALL : DISCARD_ALL, dbent);
}

/*
 * Add random key for a krb5_db_entry
 * Assumes the max kvno
 *
 * As a side effect all old keys older than the max kvno are nuked.
 */
krb5_error_code
krb5_dbe_ark(krb5_context context, krb5_keyblock *mkey,
             krb5_key_salt_tuple *ks_tuple, int ks_tuple_count,
             krb5_db_entry *dbent)
{
    return rekey(context, mkey, ks_tuple, ks_tuple_count, NULL, 0,
                 KEEP_LAST_KVNO, dbent);
}

/*
 * Change password for a krb5_db_entry
 * Assumes the max kvno
 *
 * As a side effect all old keys are nuked if keepold is false.
 */
krb5_error_code
krb5_dbe_def_cpw(krb5_context context, krb5_keyblock *mkey,
                 krb5_key_salt_tuple *ks_tuple, int ks_tuple_count,
                 char *password, int new_kvno, krb5_boolean keepold,
                 krb5_db_entry *dbent)
{
    return rekey(context, mkey, ks_tuple, ks_tuple_count, password, new_kvno,
                 keepold ? KEEP_ALL : DISCARD_ALL, dbent);
}

/*
 * Add password for a krb5_db_entry
 * Assumes the max kvno
 *
 * As a side effect all old keys older than the max kvno are nuked.
 */
krb5_error_code
krb5_dbe_apw(krb5_context context, krb5_keyblock *mkey,
             krb5_key_salt_tuple *ks_tuple, int ks_tuple_count, char *password,
             krb5_db_entry *dbent)
{
    return rekey(context, mkey, ks_tuple, ks_tuple_count, password, 0,
                 KEEP_LAST_KVNO, dbent);
}
