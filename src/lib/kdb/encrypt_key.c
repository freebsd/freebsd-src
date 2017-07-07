/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/kdb/encrypt_key.c */
/*
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

/*
 * Encrypt a key for storage in the database.  "eblock" is used
 * to encrypt the key in "in" into "out"; the storage pointed to by "out"
 * is allocated before use.
 */

krb5_error_code
krb5_dbe_def_encrypt_key_data( krb5_context             context,
                               const krb5_keyblock    * mkey,
                               const krb5_keyblock    * dbkey,
                               const krb5_keysalt     * keysalt,
                               int                      keyver,
                               krb5_key_data          * key_data)
{
    krb5_error_code               retval;
    krb5_octet                  * ptr;
    size_t                        len;
    int                           i;
    krb5_data                     plain;
    krb5_enc_data                 cipher;

    for (i = 0; i < key_data->key_data_ver; i++) {
        free(key_data->key_data_contents[i]);
        key_data->key_data_contents[i] = NULL;
    }

    key_data->key_data_ver = 1;
    key_data->key_data_kvno = keyver;

    /*
     * The First element of the type/length/contents
     * fields is the key type/length/contents
     */
    if ((retval = krb5_c_encrypt_length(context, mkey->enctype, dbkey->length,
                                        &len)))
        return(retval);

    ptr = malloc(2 + len);
    if (ptr == NULL)
        return(ENOMEM);

    key_data->key_data_type[0] = dbkey->enctype;
    key_data->key_data_length[0] = 2 + len;
    key_data->key_data_contents[0] = ptr;

    krb5_kdb_encode_int16(dbkey->length, ptr);
    ptr += 2;

    plain.length = dbkey->length;
    plain.data = (char *) dbkey->contents;

    cipher.ciphertext.length = len;
    cipher.ciphertext.data = (char *) ptr;

    if ((retval = krb5_c_encrypt(context, mkey, /* XXX */ 0, 0,
                                 &plain, &cipher))) {
        free(key_data->key_data_contents[0]);
        return retval;
    }

    /* After key comes the salt in necessary */
    if (keysalt) {
        if (keysalt->type > 0) {
            key_data->key_data_ver++;
            key_data->key_data_type[1] = keysalt->type;
            if ((key_data->key_data_length[1] = keysalt->data.length) != 0) {
                key_data->key_data_contents[1] = malloc(keysalt->data.length);
                if (key_data->key_data_contents[1] == NULL) {
                    free(key_data->key_data_contents[0]);
                    return ENOMEM;
                }
                memcpy(key_data->key_data_contents[1], keysalt->data.data,
                       (size_t) keysalt->data.length);
            }
        }
    }

    return retval;
}
