/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 2001, 2014 by the Massachusetts Institute of Technology.
 * All rights reserved.
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
 *
 * Section 6 (Encryption) of the Kerberos revisions document defines
 * cipher states to be used to chain encryptions and decryptions
 * together.  Examples of cipher states include initialization vectors
 * for CBC encription.  Most Kerberos encryption systems can share
 * code for initializing and freeing cipher states.  This file
 * contains that default code.
 */

#include "crypto_int.h"

krb5_error_code
krb5int_des_init_state(const krb5_keyblock *key, krb5_keyusage usage,
                       krb5_data *state_out)
{
    if (alloc_data(state_out, 8))
        return ENOMEM;

    /* des-cbc-crc uses the key as the initial ivec. */
    if (key->enctype == ENCTYPE_DES_CBC_CRC)
        memcpy(state_out->data, key->contents, state_out->length);

    return 0;
}

void
krb5int_default_free_state(krb5_data *state)
{
    free(state->data);
    *state = empty_data();
}
