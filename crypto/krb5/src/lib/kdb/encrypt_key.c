/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/kdb/encrypt_key.c */
/*
 * Copyright 1990,1991,2023 by the Massachusetts Institute of Technology.
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
 * Encrypt dbkey for storage in the database, putting the result into
 * key_data_out.
 */
krb5_error_code
krb5_dbe_def_encrypt_key_data(krb5_context context, const krb5_keyblock *mkey,
                              const krb5_keyblock *dbkey,
                              const krb5_keysalt *keysalt, int keyver,
                              krb5_key_data *key_data_out)
{
    krb5_error_code ret;
    size_t clen;
    krb5_data plain;
    krb5_enc_data cipher;
    krb5_key_data kd = { 0 };

    memset(key_data_out, 0, sizeof(*key_data_out));

    kd.key_data_ver = 1;
    kd.key_data_kvno = keyver;

    ret = krb5_c_encrypt_length(context, mkey->enctype, dbkey->length, &clen);
    if (ret)
        goto cleanup;

    /* The first element of the type/length/contents fields is the key
     * type/length/contents. */
    kd.key_data_type[0] = dbkey->enctype;
    kd.key_data_length[0] = 2 + clen;
    kd.key_data_contents[0] = k5alloc(kd.key_data_length[0], &ret);
    if (kd.key_data_contents[0] == NULL)
        goto cleanup;
    store_16_le(dbkey->length, kd.key_data_contents[0]);

    plain = make_data(dbkey->contents, dbkey->length);
    cipher.ciphertext = make_data(kd.key_data_contents[0] + 2, clen);
    ret = krb5_c_encrypt(context, mkey, 0, 0, &plain, &cipher);
    if (ret)
        goto cleanup;

    /* The second element of each array is the salt, if necessary. */
    if (keysalt != NULL && keysalt->type > 0) {
        kd.key_data_ver++;
        kd.key_data_type[1] = keysalt->type;
        kd.key_data_length[1] = keysalt->data.length;
        if (keysalt->data.length > 0) {
            kd.key_data_contents[1] = k5memdup(keysalt->data.data,
                                               keysalt->data.length, &ret);
            if (kd.key_data_contents[1] == NULL)
                goto cleanup;
        }
    }

    *key_data_out = kd;
    memset(&kd, 0, sizeof(kd));

cleanup:
    krb5_dbe_free_key_data_contents(context, &kd);
    return ret;
}
