/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/encrypt_tk.c */
/*
 * Copyright 1990 by the Massachusetts Institute of Technology.
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

/*
  Takes unencrypted dec_ticket & dec_tkt_part, encrypts with
  dec_ticket->enc_part.etype
  using *srv_key, and places result in dec_ticket->enc_part.
  The string dec_ticket->enc_part.ciphertext will be allocated before
  formatting.

  returns errors from encryption routines, system errors

  enc_part->ciphertext.data allocated & filled in with encrypted stuff
*/

krb5_error_code
krb5_encrypt_tkt_part(krb5_context context, const krb5_keyblock *srv_key,
                      krb5_ticket *dec_ticket)
{
    krb5_data *scratch;
    krb5_error_code retval;
    krb5_enc_tkt_part *dec_tkt_part = dec_ticket->enc_part2;

    /*  start by encoding the to-be-encrypted part. */
    if ((retval = encode_krb5_enc_tkt_part(dec_tkt_part, &scratch))) {
        return retval;
    }

#define cleanup_scratch() { (void) memset(scratch->data, 0, scratch->length); \
        krb5_free_data(context, scratch); }

    /* call the encryption routine */
    retval = krb5_encrypt_helper(context, srv_key,
                                 KRB5_KEYUSAGE_KDC_REP_TICKET, scratch,
                                 &dec_ticket->enc_part);

    cleanup_scratch();

    return(retval);
}
