/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/srv_dec_tkt.c */
/*
 * Copyright 2006, 2008 by the Massachusetts Institute of Technology.
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
 * Server decrypt ticket via keytab or keyblock.
 *
 * Different from krb5_rd_req_decoded. (krb5/src/lib/krb5/krb/rd_req_dec.c)
 *   - No krb5_principal_compare or KRB5KRB_AP_ERR_BADMATCH error.
 *   - No replay cache processing.
 *   - No skew checking or KRB5KRB_AP_ERR_SKEW error.
 *   - No address checking or KRB5KRB_AP_ERR_BADADDR error.
 *   - No time validation.
 *   - No permitted enctype validation or KRB5_NOPERM_ETYPE error.
 *   - Does not free ticket->enc_part2 on error.
 */

#include <k5-int.h>

#ifndef LEAN_CLIENT
static krb5_error_code
decrypt_ticket_keyblock(krb5_context context, const krb5_keyblock *key,
                        krb5_ticket *ticket)
{
    krb5_error_code retval;
    krb5_data *realm;
    krb5_transited *trans;

    retval = krb5_decrypt_tkt_part(context, key, ticket);
    if (retval)
        goto done;

    trans = &ticket->enc_part2->transited;
    realm = &ticket->enc_part2->client->realm;
    if (trans->tr_contents.data && *trans->tr_contents.data) {
        retval = krb5_check_transited_list(context, &trans->tr_contents,
                                           realm, &ticket->server->realm);
        goto done;
    }

    if (ticket->enc_part2->flags & TKT_FLG_INVALID) {   /* ie, KDC_OPT_POSTDATED */
        retval = KRB5KRB_AP_ERR_TKT_INVALID;
        goto done;
    }

done:
    return retval;
}


krb5_error_code KRB5_CALLCONV
krb5_server_decrypt_ticket_keytab(krb5_context context,
                                  const krb5_keytab keytab,
                                  krb5_ticket *ticket)
{
    krb5_error_code       retval;
    krb5_keytab_entry     ktent;

    if (keytab->ops->start_seq_get == NULL) {
        retval = krb5_kt_get_entry(context, keytab,
                                   ticket->server,
                                   ticket->enc_part.kvno,
                                   ticket->enc_part.enctype, &ktent);
        if (retval == 0) {
            retval = decrypt_ticket_keyblock(context, &ktent.key, ticket);

            (void) krb5_free_keytab_entry_contents(context, &ktent);
        }
    } else {
        krb5_error_code code;
        krb5_kt_cursor cursor;

        retval = krb5_kt_start_seq_get(context, keytab, &cursor);
        if (retval != 0)
            goto map_error;

        retval = KRB5_KT_NOTFOUND;
        while ((code = krb5_kt_next_entry(context, keytab,
                                          &ktent, &cursor)) == 0) {
            if (ktent.key.enctype != ticket->enc_part.enctype) {
                (void) krb5_free_keytab_entry_contents(context, &ktent);
                continue;
            }

            retval = decrypt_ticket_keyblock(context, &ktent.key, ticket);
            if (retval == 0) {
                krb5_principal tmp;

                retval = krb5_copy_principal(context, ktent.principal, &tmp);
                if (retval == 0) {
                    krb5_free_principal(context, ticket->server);
                    ticket->server = tmp;
                }
                (void) krb5_free_keytab_entry_contents(context, &ktent);
                break;
            }
            (void) krb5_free_keytab_entry_contents(context, &ktent);
        }

        code = krb5_kt_end_seq_get(context, keytab, &cursor);
        if (code != 0)
            retval = code;
    }

map_error:
    switch (retval) {
    case KRB5_KT_KVNONOTFOUND:
    case KRB5_KT_NOTFOUND:
    case KRB5KRB_AP_ERR_BAD_INTEGRITY:
        retval = KRB5KRB_AP_WRONG_PRINC;
        break;
    default:
        break;
    }

    return retval;
}
#endif /* LEAN_CLIENT */
