/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/db2/kdb_xdr.c */
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

#include "k5-int.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "kdb_xdr.h"

krb5_error_code
krb5_encode_princ_dbkey(krb5_context context, krb5_data *key,
                        krb5_const_principal principal)
{
    char *princ_name;
    krb5_error_code retval;

    if (!(retval = krb5_unparse_name(context, principal, &princ_name))) {
        /* need to store the NULL for decoding */
        key->length = strlen(princ_name)+1;
        key->data = princ_name;
    }
    return(retval);
}

krb5_error_code
krb5_encode_princ_entry(krb5_context context, krb5_data *content,
                        krb5_db_entry *entry)
{
    int                   i, j;
    unsigned int          unparse_princ_size;
    char                * unparse_princ;
    unsigned char       * nextloc;
    krb5_tl_data        * tl_data;
    krb5_error_code       retval;
    krb5_int16            psize16;

    /*
     * Generate one lump of data from the krb5_db_entry.
     * This data must be independent of byte order of the machine,
     * compact and extensible.
     */

    /*
     * First allocate enough space for all the data.
     * Need  2 bytes for the length of the base structure
     * then 36 [ 8 * 4 + 2 * 2] bytes for the base information
     *         [ attributes, max_life, max_renewable_life, expiration,
     *           pw_expiration, last_success, last_failed, fail_auth_count ]
     *         [ n_key_data, n_tl_data ]
     * then XX bytes [ e_length ] for the extra data [ e_data ]
     * then XX bytes [ 2 for length + length for string ] for the principal,
     * then (4 [type + length] + tl_data_length) bytes per tl_data
     * then (4 + (4 + key_data_length) per key_data_contents) bytes per key_data
     */
    content->length = entry->len + entry->e_length;

    if ((retval = krb5_unparse_name(context, entry->princ, &unparse_princ)))
        return(retval);

    unparse_princ_size = strlen(unparse_princ) + 1;
    content->length += unparse_princ_size;
    content->length += 2;

    i = 0;
    /* tl_data is a linked list */
    for (tl_data = entry->tl_data; tl_data; tl_data = tl_data->tl_data_next) {
        content->length += tl_data->tl_data_length;
        content->length += 4; /* type, length */
        i++;
    }

    if (i != entry->n_tl_data) {
        retval = KRB5_KDB_TRUNCATED_RECORD;
        goto epc_error;
    }

    /* key_data is an array */
    for (i = 0; i < entry->n_key_data; i++) {
        content->length += 4; /* Version, KVNO */
        for (j = 0; j < entry->key_data[i].key_data_ver; j++) {
            content->length += entry->key_data[i].key_data_length[j];
            content->length += 4; /* type + length */
        }
    }

    if ((content->data = malloc(content->length)) == NULL) {
        retval = ENOMEM;
        goto epc_error;
    }

    /*
     * Now we go through entry again, this time copying data
     * These first entries are always saved regardless of version
     */
    nextloc = (unsigned char *)content->data;

    /* Base Length */
    krb5_kdb_encode_int16(entry->len, nextloc);
    nextloc += 2;

    /* Attributes */
    krb5_kdb_encode_int32(entry->attributes, nextloc);
    nextloc += 4;

    /* Max Life */
    krb5_kdb_encode_int32(entry->max_life, nextloc);
    nextloc += 4;

    /* Max Renewable Life */
    krb5_kdb_encode_int32(entry->max_renewable_life, nextloc);
    nextloc += 4;

    /* When the client expires */
    krb5_kdb_encode_int32(entry->expiration, nextloc);
    nextloc += 4;

    /* When its passwd expires */
    krb5_kdb_encode_int32(entry->pw_expiration, nextloc);
    nextloc += 4;

    /* Last successful passwd */
    krb5_kdb_encode_int32(entry->last_success, nextloc);
    nextloc += 4;

    /* Last failed passwd attempt */
    krb5_kdb_encode_int32(entry->last_failed, nextloc);
    nextloc += 4;

    /* # of failed passwd attempt */
    krb5_kdb_encode_int32(entry->fail_auth_count, nextloc);
    nextloc += 4;

    /* # tl_data strutures */
    krb5_kdb_encode_int16(entry->n_tl_data, nextloc);
    nextloc += 2;

    /* # key_data strutures */
    krb5_kdb_encode_int16(entry->n_key_data, nextloc);
    nextloc += 2;

    /* Put extended fields here */
    if (entry->len != KRB5_KDB_V1_BASE_LENGTH)
        abort();

    /* Any extra data that this version doesn't understand. */
    if (entry->e_length) {
        memcpy(nextloc, entry->e_data, entry->e_length);
        nextloc += entry->e_length;
    }

    /*
     * Now we get to the principal.
     * To squeze a few extra bytes out it is always assumed to come
     * after the base type.
     */
    psize16 = (krb5_int16) unparse_princ_size;
    krb5_kdb_encode_int16(psize16, nextloc);
    nextloc += 2;
    (void) memcpy(nextloc, unparse_princ, unparse_princ_size);
    nextloc += unparse_princ_size;

    /* tl_data is a linked list, of type, legth, contents */
    for (tl_data = entry->tl_data; tl_data; tl_data = tl_data->tl_data_next) {
        krb5_kdb_encode_int16(tl_data->tl_data_type, nextloc);
        nextloc += 2;
        krb5_kdb_encode_int16(tl_data->tl_data_length, nextloc);
        nextloc += 2;

        memcpy(nextloc, tl_data->tl_data_contents, tl_data->tl_data_length);
        nextloc += tl_data->tl_data_length;
    }

    /* key_data is an array */
    for (i = 0; i < entry->n_key_data; i++) {
        krb5_kdb_encode_int16(entry->key_data[i].key_data_ver, nextloc);
        nextloc += 2;
        krb5_kdb_encode_int16(entry->key_data[i].key_data_kvno, nextloc);
        nextloc += 2;

        for (j = 0; j < entry->key_data[i].key_data_ver; j++) {
            krb5_int16 type = entry->key_data[i].key_data_type[j];
            krb5_ui_2  length = entry->key_data[i].key_data_length[j];

            krb5_kdb_encode_int16(type, nextloc);
            nextloc += 2;
            krb5_kdb_encode_int16(length, nextloc);
            nextloc += 2;

            if (length) {
                memcpy(nextloc, entry->key_data[i].key_data_contents[j],length);
                nextloc += length;
            }
        }
    }

epc_error:;
    free(unparse_princ);
    return retval;
}

krb5_error_code
krb5_decode_princ_entry(krb5_context context, krb5_data *content,
                        krb5_db_entry **entry_ptr)
{
    int                   sizeleft, i;
    unsigned char       * nextloc;
    krb5_tl_data       ** tl_data;
    krb5_int16            i16;
    krb5_db_entry       * entry;
    krb5_error_code retval;

    *entry_ptr = NULL;

    entry = k5alloc(sizeof(*entry), &retval);
    if (entry == NULL)
        return retval;

    /*
     * Reverse the encoding of encode_princ_entry.
     *
     * The first part is decoding the base type. If the base type is
     * bigger than the original base type then the additional fields
     * need to be filled in. If the base type is larger than any
     * known base type the additional data goes in e_data.
     */

    /* First do the easy stuff */
    nextloc = (unsigned char *)content->data;
    sizeleft = content->length;
    if (sizeleft < KRB5_KDB_V1_BASE_LENGTH) {
        retval = KRB5_KDB_TRUNCATED_RECORD;
        goto error_out;
    }
    sizeleft -= KRB5_KDB_V1_BASE_LENGTH;

    /* Base Length */
    krb5_kdb_decode_int16(nextloc, entry->len);
    nextloc += 2;

    /* Attributes */
    krb5_kdb_decode_int32(nextloc, entry->attributes);
    nextloc += 4;

    /* Max Life */
    krb5_kdb_decode_int32(nextloc, entry->max_life);
    nextloc += 4;

    /* Max Renewable Life */
    krb5_kdb_decode_int32(nextloc, entry->max_renewable_life);
    nextloc += 4;

    /* When the client expires */
    krb5_kdb_decode_int32(nextloc, entry->expiration);
    nextloc += 4;

    /* When its passwd expires */
    krb5_kdb_decode_int32(nextloc, entry->pw_expiration);
    nextloc += 4;

    /* Last successful passwd */
    krb5_kdb_decode_int32(nextloc, entry->last_success);
    nextloc += 4;

    /* Last failed passwd attempt */
    krb5_kdb_decode_int32(nextloc, entry->last_failed);
    nextloc += 4;

    /* # of failed passwd attempt */
    krb5_kdb_decode_int32(nextloc, entry->fail_auth_count);
    nextloc += 4;

    /* # tl_data strutures */
    krb5_kdb_decode_int16(nextloc, entry->n_tl_data);
    nextloc += 2;

    if (entry->n_tl_data < 0) {
        retval = KRB5_KDB_TRUNCATED_RECORD;
        goto error_out;
    }

    /* # key_data strutures */
    krb5_kdb_decode_int16(nextloc, entry->n_key_data);
    nextloc += 2;

    if (entry->n_key_data < 0) {
        retval = KRB5_KDB_TRUNCATED_RECORD;
        goto error_out;
    }

    /* Check for extra data */
    if (entry->len > KRB5_KDB_V1_BASE_LENGTH) {
        entry->e_length = entry->len - KRB5_KDB_V1_BASE_LENGTH;
        entry->e_data = k5memdup(nextloc, entry->e_length, &retval);
        if (entry->e_data == NULL)
            goto error_out;
        nextloc += entry->e_length;
    }

    /*
     * Get the principal name for the entry
     * (stored as a string which gets unparsed.)
     */
    if (sizeleft < 2) {
        retval = KRB5_KDB_TRUNCATED_RECORD;
        goto error_out;
    }
    sizeleft -= 2;

    i = 0;
    krb5_kdb_decode_int16(nextloc, i16);
    i = (int) i16;
    nextloc += 2;
    if (i <= 0 || i > sizeleft || nextloc[i - 1] != '\0' ||
        memchr((char *)nextloc, '\0', i - 1) != NULL) {
        retval = KRB5_KDB_TRUNCATED_RECORD;
        goto error_out;
    }

    if ((retval = krb5_parse_name(context, (char *)nextloc, &(entry->princ))))
        goto error_out;
    sizeleft -= i;
    nextloc += i;

    /* tl_data is a linked list */
    tl_data = &entry->tl_data;
    for (i = 0; i < entry->n_tl_data; i++) {
        if (sizeleft < 4) {
            retval = KRB5_KDB_TRUNCATED_RECORD;
            goto error_out;
        }
        sizeleft -= 4;
        if ((*tl_data = (krb5_tl_data *)
             malloc(sizeof(krb5_tl_data))) == NULL) {
            retval = ENOMEM;
            goto error_out;
        }
        (*tl_data)->tl_data_next = NULL;
        (*tl_data)->tl_data_contents = NULL;
        krb5_kdb_decode_int16(nextloc, (*tl_data)->tl_data_type);
        nextloc += 2;
        krb5_kdb_decode_int16(nextloc, (*tl_data)->tl_data_length);
        nextloc += 2;

        if ((*tl_data)->tl_data_length > sizeleft) {
            retval = KRB5_KDB_TRUNCATED_RECORD;
            goto error_out;
        }
        sizeleft -= (*tl_data)->tl_data_length;
        (*tl_data)->tl_data_contents =
            k5memdup(nextloc, (*tl_data)->tl_data_length, &retval);
        if ((*tl_data)->tl_data_contents == NULL)
            goto error_out;
        nextloc += (*tl_data)->tl_data_length;
        tl_data = &((*tl_data)->tl_data_next);
    }

    /* key_data is an array */
    if (entry->n_key_data && ((entry->key_data = (krb5_key_data *)
                               malloc(sizeof(krb5_key_data) * entry->n_key_data)) == NULL)) {
        retval = ENOMEM;
        goto error_out;
    }
    for (i = 0; i < entry->n_key_data; i++) {
        krb5_key_data * key_data;
        int j;

        if (sizeleft < 4) {
            retval = KRB5_KDB_TRUNCATED_RECORD;
            goto error_out;
        }
        sizeleft -= 4;
        key_data = entry->key_data + i;
        memset(key_data, 0, sizeof(krb5_key_data));
        krb5_kdb_decode_int16(nextloc, key_data->key_data_ver);
        nextloc += 2;
        krb5_kdb_decode_int16(nextloc, key_data->key_data_kvno);
        nextloc += 2;

        /* key_data_ver determins number of elements and how to unparse them. */
        if (key_data->key_data_ver >= 0 &&
            key_data->key_data_ver <= KRB5_KDB_V1_KEY_DATA_ARRAY) {
            for (j = 0; j < key_data->key_data_ver; j++) {
                if (sizeleft < 4) {
                    retval = KRB5_KDB_TRUNCATED_RECORD;
                    goto error_out;
                }
                sizeleft -= 4;
                krb5_kdb_decode_int16(nextloc, key_data->key_data_type[j]);
                nextloc += 2;
                krb5_kdb_decode_int16(nextloc, key_data->key_data_length[j]);
                nextloc += 2;

                if (key_data->key_data_length[j] > sizeleft) {
                    retval = KRB5_KDB_TRUNCATED_RECORD;
                    goto error_out;
                }
                sizeleft -= key_data->key_data_length[j];
                if (key_data->key_data_length[j]) {
                    key_data->key_data_contents[j] =
                        k5memdup(nextloc, key_data->key_data_length[j],
                                 &retval);
                    if (key_data->key_data_contents[j] == NULL)
                        goto error_out;
                    nextloc += key_data->key_data_length[j];
                }
            }
        } else {
            retval = KRB5_KDB_BAD_VERSION;
            goto error_out;
        }
    }
    *entry_ptr = entry;
    return 0;

error_out:
    krb5_db_free_principal(context, entry);
    return retval;
}
