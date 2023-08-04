/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/in_tkt_sky.c */
/*
 * Copyright 1990,1991, 2008 by the Massachusetts Institute of Technology.
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

/* Copy the caller-provided keyblock into the AS keyblock. */
static krb5_error_code
get_as_key_skey(krb5_context context, krb5_principal client,
                krb5_enctype etype, krb5_prompter_fct prompter,
                void *prompter_data, krb5_data *salt, krb5_data *params,
                krb5_keyblock *as_key, void *gak_data,
                k5_response_items *ritems)
{
    const krb5_keyblock *key = gak_data;

    if (!krb5_c_valid_enctype(etype))
        return(KRB5_PROG_ETYPE_NOSUPP);
    if (as_key->length)
        krb5_free_keyblock_contents(context, as_key);
    return krb5int_c_copy_keyblock_contents(context, key, as_key);
}

/*
  Similar to krb5_get_in_tkt_with_password.

  Attempts to get an initial ticket for creds->client to use server
  creds->server, (realm is taken from creds->client), with options
  options, and using creds->times.starttime, creds->times.endtime,
  creds->times.renew_till as from, till, and rtime.
  creds->times.renew_till is ignored unless the RENEWABLE option is requested.

  If addrs is non-NULL, it is used for the addresses requested.  If it is
  null, the system standard addresses are used.

  If keyblock is NULL, an appropriate key for creds->client is retrieved from
  the system key store (e.g. /etc/krb5.keytab).  If keyblock is non-NULL, it
  is used as the decryption key.

  A successful call will place the ticket in the credentials cache ccache.

  returns system errors, encryption errors

*/
krb5_error_code KRB5_CALLCONV
krb5_get_in_tkt_with_skey(krb5_context context, krb5_flags options,
                          krb5_address *const *addrs, krb5_enctype *ktypes,
                          krb5_preauthtype *pre_auth_types,
                          const krb5_keyblock *key, krb5_ccache ccache,
                          krb5_creds *creds, krb5_kdc_rep **ret_as_reply)
{
    krb5_error_code retval;
    char *server;
    krb5_principal server_princ, client_princ;
    int use_primary = 0;
    krb5_get_init_creds_opt *opts = NULL;

    retval = k5_populate_gic_opt(context, &opts, options, addrs, ktypes,
                                 pre_auth_types, creds);
    if (retval)
        return retval;

    retval = krb5_get_init_creds_opt_set_out_ccache(context, opts, ccache);
    if (retval)
        goto cleanup;

#ifndef LEAN_CLIENT
    if (key == NULL) {
        retval = krb5_get_init_creds_keytab(context, creds, creds->client,
                                            NULL /* keytab */,
                                            creds->times.starttime,
                                            NULL /* in_tkt_service */,
                                            opts);
        goto cleanup;
    }
#endif /* LEAN_CLIENT */

    retval = krb5_unparse_name(context, creds->server, &server);
    if (retval)
        goto cleanup;
    server_princ = creds->server;
    client_princ = creds->client;
    retval = k5_get_init_creds(context, creds, creds->client,
                               krb5_prompter_posix, NULL, 0, server, opts,
                               get_as_key_skey, (void *)key, &use_primary,
                               ret_as_reply);
    krb5_free_unparsed_name(context, server);
    if (retval)
        goto cleanup;
    krb5_free_principal( context, creds->server);
    krb5_free_principal( context, creds->client);
    creds->client = client_princ;
    creds->server = server_princ;
cleanup:
    krb5_get_init_creds_opt_free(context, opts);
    return retval;
}
