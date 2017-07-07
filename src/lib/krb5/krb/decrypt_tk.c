/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/decrypt_tk.c */
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
  Decrypts dec_ticket->enc_part
  using *srv_key, and places result in dec_ticket->enc_part2.
  The storage of dec_ticket->enc_part2 will be allocated before return.

  returns errors from encryption routines, system errors

*/

krb5_error_code KRB5_CALLCONV
krb5_decrypt_tkt_part(krb5_context context, const krb5_keyblock *srv_key, register krb5_ticket *ticket)
{
    krb5_enc_tkt_part *dec_tkt_part;
    krb5_data scratch;
    krb5_error_code retval;

    if (!krb5_c_valid_enctype(ticket->enc_part.enctype))
        return KRB5_PROG_ETYPE_NOSUPP;

    if (!krb5_is_permitted_enctype(context, ticket->enc_part.enctype))
        return KRB5_NOPERM_ETYPE;

    scratch.length = ticket->enc_part.ciphertext.length;
    if (!(scratch.data = malloc(ticket->enc_part.ciphertext.length)))
        return(ENOMEM);

    /* call the encryption routine */
    if ((retval = krb5_c_decrypt(context, srv_key,
                                 KRB5_KEYUSAGE_KDC_REP_TICKET, 0,
                                 &ticket->enc_part, &scratch))) {
        free(scratch.data);
        return retval;
    }

    /*  now decode the decrypted stuff */
    retval = decode_krb5_enc_tkt_part(&scratch, &dec_tkt_part);
    if (!retval) {
        ticket->enc_part2 = dec_tkt_part;
    }
    zapfree(scratch.data, scratch.length);
    return retval;
}
