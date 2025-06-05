/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/kdb/decrypt_key.c */
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

/* Decrypt key_data, putting the result into dbkey_out and (if not null)
 * keysalt_out. */
krb5_error_code
krb5_dbe_def_decrypt_key_data(krb5_context context, const krb5_keyblock *mkey,
                              const krb5_key_data *kd,
                              krb5_keyblock *dbkey_out,
                              krb5_keysalt *keysalt_out)
{
    krb5_error_code ret;
    int16_t keylen;
    krb5_enc_data cipher;
    krb5_data plain = empty_data();
    krb5_keyblock kb = { 0 };
    krb5_keysalt salt = { 0 };

    memset(dbkey_out, 0, sizeof(*dbkey_out));
    if (keysalt_out != NULL)
        memset(keysalt_out, 0, sizeof(*keysalt_out));

    if (mkey == NULL)
        return KRB5_KDB_BADSTORED_MKEY;

    if (kd->key_data_contents[0] != NULL && kd->key_data_length[0] >= 2) {
        keylen = load_16_le(kd->key_data_contents[0]);
        if (keylen < 0)
            return EINVAL;
        cipher.enctype = ENCTYPE_UNKNOWN;
        cipher.ciphertext = make_data(kd->key_data_contents[0] + 2,
                                      kd->key_data_length[0] - 2);
        ret = alloc_data(&plain, kd->key_data_length[0] - 2);
        if (ret)
            goto cleanup;

        ret = krb5_c_decrypt(context, mkey, 0, 0, &cipher, &plain);
        if (ret)
            goto cleanup;

        /* Make sure the plaintext has at least as many bytes as the true ke
         * length (it may have more due to padding). */
        if ((unsigned int)keylen > plain.length) {
            ret = KRB5_CRYPTO_INTERNAL;
            if (ret)
                goto cleanup;
        }

        kb.magic = KV5M_KEYBLOCK;
        kb.enctype = kd->key_data_type[0];
        kb.length = keylen;
        kb.contents = (uint8_t *)plain.data;
        plain = empty_data();
    }

    /* Decode salt data. */
    if (keysalt_out != NULL) {
        if (kd->key_data_ver == 2) {
            salt.type = kd->key_data_type[1];
            salt.data.length = kd->key_data_length[1];
            if (kd->key_data_length[1] > 0) {
                ret = alloc_data(&salt.data, kd->key_data_length[1]);
                if (ret)
                    goto cleanup;
                memcpy(salt.data.data, kd->key_data_contents[1],
                       salt.data.length);
            }
        } else {
            salt.type = KRB5_KDB_SALTTYPE_NORMAL;
        }
    }

    *dbkey_out = kb;
    if (keysalt_out != NULL)
        *keysalt_out = salt;
    memset(&kb, 0, sizeof(kb));
    memset(&salt, 0, sizeof(salt));

cleanup:
    zapfree(plain.data, plain.length);
    krb5_free_keyblock_contents(context, &kb);
    free(salt.data.data);
    return ret;
}
