/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
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

#include "crypto_int.h"
#include "des_int.h"

static krb5_error_code
validate_and_schedule(krb5_key key, const krb5_data *ivec,
                      const krb5_crypto_iov *data, size_t num_data,
                      mit_des_key_schedule schedule)
{
    if (key->keyblock.length != 8)
        return KRB5_BAD_KEYSIZE;
    if (iov_total_length(data, num_data, FALSE) % 8 != 0)
        return KRB5_BAD_MSIZE;
    if (ivec != NULL && ivec->length != 8)
        return KRB5_BAD_MSIZE;

    switch (mit_des_key_sched(key->keyblock.contents, schedule)) {
    case -1:
        return(KRB5DES_BAD_KEYPAR);
    case -2:
        return(KRB5DES_WEAK_KEY);
    }
    return 0;
}

static krb5_error_code
des_encrypt(krb5_key key, const krb5_data *ivec, krb5_crypto_iov *data,
            size_t num_data)
{
    mit_des_key_schedule schedule;
    krb5_error_code err;

    err = validate_and_schedule(key, ivec, data, num_data, schedule);
    if (err)
        return err;

    krb5int_des_cbc_encrypt(data, num_data, schedule,
                            ivec != NULL ? (unsigned char *) ivec->data :
                            NULL);

    zap(schedule, sizeof(schedule));
    return 0;
}

static krb5_error_code
des_decrypt(krb5_key key, const krb5_data *ivec, krb5_crypto_iov *data,
            size_t num_data)
{
    mit_des_key_schedule schedule;
    krb5_error_code err;

    err = validate_and_schedule(key, ivec, data, num_data, schedule);
    if (err)
        return err;

    krb5int_des_cbc_decrypt(data, num_data, schedule,
                            ivec != NULL ? (unsigned char *) ivec->data :
                            NULL);

    zap(schedule, sizeof(schedule));
    return 0;
}

static krb5_error_code
des_cbc_mac(krb5_key key, const krb5_crypto_iov *data, size_t num_data,
            const krb5_data *ivec, krb5_data *output)
{
    mit_des_key_schedule schedule;
    krb5_error_code err;

    err = validate_and_schedule(key, ivec, data, num_data, schedule);
    if (err)
        return err;

    if (output->length != 8)
        return KRB5_CRYPTO_INTERNAL;

    krb5int_des_cbc_mac(data, num_data, schedule,
                        ivec != NULL ? (unsigned char *) ivec->data : NULL,
                        (unsigned char *) output->data);

    zap(schedule, sizeof(schedule));
    return 0;
}

const struct krb5_enc_provider krb5int_enc_des = {
    8,
    7, 8,
    des_encrypt,
    des_decrypt,
    des_cbc_mac,
    krb5int_des_init_state,
    krb5int_default_free_state
};
