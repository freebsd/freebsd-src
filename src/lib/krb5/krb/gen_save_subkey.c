/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/gen_save_subkey.c */
/*
 * Copyright 2009 by the Massachusetts Institute of Technology.
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
#include "int-proto.h"
#include "auth_con.h"

krb5_error_code
k5_generate_and_save_subkey(krb5_context context,
                            krb5_auth_context auth_context,
                            krb5_keyblock *keyblock, krb5_enctype enctype)
{
    /* Provide some more fodder for random number code.
       This isn't strong cryptographically; the point here is not
       to guarantee randomness, but to make it less likely that multiple
       sessions could pick the same subkey.  */
    struct {
        krb5_timestamp sec;
        krb5_int32 usec;
    } rnd_data;
    krb5_data d;
    krb5_error_code retval;
    krb5_keyblock *kb = NULL;

    if (krb5_crypto_us_timeofday(&rnd_data.sec, &rnd_data.usec) == 0) {
        d.length = sizeof(rnd_data);
        d.data = (char *) &rnd_data;
        krb5_c_random_add_entropy(context, KRB5_C_RANDSOURCE_TIMING, &d);
    }

    retval = krb5_generate_subkey_extended(context, keyblock, enctype, &kb);
    if (retval)
        return retval;
    retval = krb5_auth_con_setsendsubkey(context, auth_context, kb);
    if (retval)
        goto cleanup;
    retval = krb5_auth_con_setrecvsubkey(context, auth_context, kb);
    if (retval)
        goto cleanup;

cleanup:
    if (retval) {
        (void) krb5_auth_con_setsendsubkey(context, auth_context, NULL);
        (void) krb5_auth_con_setrecvsubkey(context, auth_context, NULL);
    }
    krb5_free_keyblock(context, kb);
    return retval;
}
