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

#ifdef K5_BUILTIN_DES

static krb5_error_code
validate_and_schedule(krb5_key key, const krb5_data *ivec,
                      const krb5_crypto_iov *data, size_t num_data,
                      mit_des3_key_schedule *schedule)
{
    if (key->keyblock.length != 24)
        return(KRB5_BAD_KEYSIZE);
    if (iov_total_length(data, num_data, FALSE) % 8 != 0)
        return(KRB5_BAD_MSIZE);
    if (ivec && (ivec->length != 8))
        return(KRB5_BAD_MSIZE);

    switch (mit_des3_key_sched(*(mit_des3_cblock *)key->keyblock.contents,
                               *schedule)) {
    case -1:
        return(KRB5DES_BAD_KEYPAR);
    case -2:
        return(KRB5DES_WEAK_KEY);
    }
    return 0;
}

static krb5_error_code
k5_des3_encrypt(krb5_key key, const krb5_data *ivec, krb5_crypto_iov *data,
                size_t num_data)
{
    mit_des3_key_schedule schedule;
    krb5_error_code err;

    err = validate_and_schedule(key, ivec, data, num_data, &schedule);
    if (err)
        return err;

    /* this has a return value, but the code always returns zero */
    krb5int_des3_cbc_encrypt(data, num_data,
                             schedule[0], schedule[1], schedule[2],
                             ivec != NULL ? (unsigned char *) ivec->data :
                             NULL);

    zap(schedule, sizeof(schedule));

    return(0);
}

static krb5_error_code
k5_des3_decrypt(krb5_key key, const krb5_data *ivec, krb5_crypto_iov *data,
                size_t num_data)
{
    mit_des3_key_schedule schedule;
    krb5_error_code err;

    err = validate_and_schedule(key, ivec, data, num_data, &schedule);
    if (err)
        return err;

    /* this has a return value, but the code always returns zero */
    krb5int_des3_cbc_decrypt(data, num_data,
                             schedule[0], schedule[1], schedule[2],
                             ivec != NULL ? (unsigned char *) ivec->data :
                             NULL);

    zap(schedule, sizeof(schedule));

    return 0;
}

const struct krb5_enc_provider krb5int_enc_des3 = {
    8,
    21, 24,
    k5_des3_encrypt,
    k5_des3_decrypt,
    NULL,
    krb5int_des_init_state,
    krb5int_default_free_state
};

#endif /* K5_BUILTIN_DES */
