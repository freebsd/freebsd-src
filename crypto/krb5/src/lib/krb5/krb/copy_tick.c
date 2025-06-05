/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/copy_tick.c */
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

#include "k5-int.h"

static krb5_error_code
copy_enc_tkt_part(krb5_context context, const krb5_enc_tkt_part *partfrom,
                  krb5_enc_tkt_part **partto)
{
    krb5_error_code retval;
    krb5_enc_tkt_part *tempto;

    if (!(tempto = (krb5_enc_tkt_part *)malloc(sizeof(*tempto))))
        return ENOMEM;
    *tempto = *partfrom;
    retval = krb5_copy_keyblock(context, partfrom->session,
                                &tempto->session);
    if (retval) {
        free(tempto);
        return retval;
    }
    retval = krb5_copy_principal(context, partfrom->client, &tempto->client);
    if (retval) {
        krb5_free_keyblock(context, tempto->session);
        free(tempto);
        return retval;
    }
    tempto->transited = partfrom->transited;
    if (tempto->transited.tr_contents.length == 0) {
        tempto->transited.tr_contents.data = 0;
    } else {
        tempto->transited.tr_contents.data =
            k5memdup(partfrom->transited.tr_contents.data,
                     partfrom->transited.tr_contents.length, &retval);
        if (!tempto->transited.tr_contents.data) {
            krb5_free_principal(context, tempto->client);
            krb5_free_keyblock(context, tempto->session);
            free(tempto);
            return ENOMEM;
        }
    }

    retval = krb5_copy_addresses(context, partfrom->caddrs, &tempto->caddrs);
    if (retval) {
        free(tempto->transited.tr_contents.data);
        krb5_free_principal(context, tempto->client);
        krb5_free_keyblock(context, tempto->session);
        free(tempto);
        return retval;
    }
    if (partfrom->authorization_data) {
        retval = krb5_copy_authdata(context, partfrom->authorization_data,
                                    &tempto->authorization_data);
        if (retval) {
            krb5_free_addresses(context, tempto->caddrs);
            free(tempto->transited.tr_contents.data);
            krb5_free_principal(context, tempto->client);
            krb5_free_keyblock(context, tempto->session);
            free(tempto);
            return retval;
        }
    }
    *partto = tempto;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_copy_ticket(krb5_context context, const krb5_ticket *from, krb5_ticket **pto)
{
    krb5_error_code retval;
    krb5_ticket *tempto;
    krb5_data *scratch;

    if (!(tempto = (krb5_ticket *)malloc(sizeof(*tempto))))
        return ENOMEM;
    *tempto = *from;
    retval = krb5_copy_principal(context, from->server, &tempto->server);
    if (retval) {
        free(tempto);
        return retval;
    }
    retval = krb5_copy_data(context, &from->enc_part.ciphertext, &scratch);
    if (retval) {
        krb5_free_principal(context, tempto->server);
        free(tempto);
        return retval;
    }
    tempto->enc_part.ciphertext = *scratch;
    free(scratch);
    retval = copy_enc_tkt_part(context, from->enc_part2, &tempto->enc_part2);
    if (retval) {
        free(tempto->enc_part.ciphertext.data);
        krb5_free_principal(context, tempto->server);
        free(tempto);
        return retval;
    }
    *pto = tempto;
    return 0;
}
