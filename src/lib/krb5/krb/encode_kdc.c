/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/encode_kdc.c */
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
  Takes KDC rep parts in *rep and *encpart, and formats it into *enc_rep,
  using message type type and encryption key client_key and encryption type
  etype.

  The string *enc_rep will be allocated before formatting; the caller should
  free when finished.

  returns system errors

  dec_rep->enc_part.ciphertext is allocated and filled in.
*/
/* due to argument promotion rules, we need to use the DECLARG/OLDDECLARG
   stuff... */
krb5_error_code
krb5_encode_kdc_rep(krb5_context context, krb5_msgtype type,
                    const krb5_enc_kdc_rep_part *encpart,
                    int using_subkey, const krb5_keyblock *client_key,
                    krb5_kdc_rep *dec_rep, krb5_data **enc_rep)
{
    krb5_data *scratch;
    krb5_error_code retval;
    krb5_enc_kdc_rep_part tmp_encpart;
    krb5_keyusage usage;

    if (!krb5_c_valid_enctype(dec_rep->enc_part.enctype))
        return KRB5_PROG_ETYPE_NOSUPP;

    switch (type) {
    case KRB5_AS_REP:
        usage = KRB5_KEYUSAGE_AS_REP_ENCPART;
        break;
    case KRB5_TGS_REP:
        if (using_subkey)
            usage = KRB5_KEYUSAGE_TGS_REP_ENCPART_SUBKEY;
        else
            usage = KRB5_KEYUSAGE_TGS_REP_ENCPART_SESSKEY;
        break;
    default:
        return KRB5_BADMSGTYPE;
    }

    /*
     * We don't want to modify encpart, but we need to be able to pass
     * in the message type to the encoder, so it can set the ASN.1
     * type correct.
     *
     * Although note that it may be doing nothing with the message
     * type, to be compatible with old versions of Kerberos that always
     * encode this as a TGS_REP regardly of what it really should be;
     * also note that the reason why we are passing it in a structure
     * instead of as an argument to encode_krb5_enc_kdc_rep_part (the
     * way we should) is for compatibility with the ISODE version of
     * this fuction.  Ah, compatibility....
     */
    tmp_encpart = *encpart;
    tmp_encpart.msg_type = type;
    retval = encode_krb5_enc_kdc_rep_part(&tmp_encpart, &scratch);
    if (retval) {
        return retval;
    }
    memset(&tmp_encpart, 0, sizeof(tmp_encpart));

#define cleanup_scratch() { (void) memset(scratch->data, 0, scratch->length); \
        krb5_free_data(context, scratch); }

    retval = krb5_encrypt_helper(context, client_key, usage, scratch,
                                 &dec_rep->enc_part);

#define cleanup_encpart() {                                     \
        (void) memset(dec_rep->enc_part.ciphertext.data, 0,     \
                      dec_rep->enc_part.ciphertext.length);     \
        free(dec_rep->enc_part.ciphertext.data);                \
        dec_rep->enc_part.ciphertext.length = 0;                \
        dec_rep->enc_part.ciphertext.data = 0;}

    cleanup_scratch();

    if (retval)
        return(retval);

    /* now it's ready to be encoded for the wire! */

    switch (type) {
    case KRB5_AS_REP:
        retval = encode_krb5_as_rep(dec_rep, enc_rep);
        break;
    case KRB5_TGS_REP:
        retval = encode_krb5_tgs_rep(dec_rep, enc_rep);
        break;
    }

    if (retval)
        cleanup_encpart();

    return retval;
}
