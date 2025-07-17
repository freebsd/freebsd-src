/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/val_renew.c */
/*
 * Copyright (C) 2010 by the Massachusetts Institute of Technology.
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
 */

/*
 * Implements the following APIs:
 *
 *   krb5_get_credentials_validate
 *   krb5_get_credentials_renew
 *   krb5_get_validated_creds
 *   krb5_get_renewed_creds
 *
 * The first two are old but not formally deprecated; the latter two are newer.
 */

#include "k5-int.h"
#include "int-proto.h"

/*
 * Get a validated or renewed credential matching in_creds, by retrieving a
 * matching credential from ccache and renewing or validating it with the
 * credential's realm's KDC.  kdcopt specifies whether to validate or renew.
 * The result is placed in *out_creds.
 */
static krb5_error_code
get_new_creds(krb5_context context, krb5_ccache ccache, krb5_creds *in_creds,
              krb5_flags kdcopt, krb5_creds **out_creds)
{
    krb5_error_code code;
    krb5_creds old_creds, *new_creds = NULL;

    *out_creds = NULL;

    /* Retrieve an existing cached credential matching in_creds. */
    code = krb5_cc_retrieve_cred(context, ccache, KRB5_TC_SUPPORTED_KTYPES,
                                 in_creds, &old_creds);
    if (code != 0)
        return code;

    /* Use KDC options from old credential as well as requested options. */
    kdcopt |= (old_creds.ticket_flags & KDC_TKT_COMMON_MASK);

    /* Use the old credential to get a new credential from the KDC. */
    code = krb5_get_cred_via_tkt(context, &old_creds, kdcopt,
                                 old_creds.addresses, in_creds, &new_creds);
    krb5_free_cred_contents(context, &old_creds);
    if (code != 0)
        return code;

    *out_creds = new_creds;
    return code;
}

/*
 * Core of the older pair of APIs: get a validated or renewed credential
 * matching in_creds and reinitialize ccache so that it contains only the new
 * credential.
 */
static krb5_error_code
gc_valrenew(krb5_context context, krb5_ccache ccache, krb5_creds *in_creds,
            krb5_flags kdcopt, krb5_creds **out_creds)
{
    krb5_error_code code;
    krb5_creds *new_creds = NULL;
    krb5_principal default_princ = NULL;

    /* Get the validated or renewed credential. */
    code = get_new_creds(context, ccache, in_creds, kdcopt, &new_creds);
    if (code != 0)
        goto cleanup;

    /* Reinitialize the cache without changing its default principal. */
    code = krb5_cc_get_principal(context, ccache, &default_princ);
    if (code != 0)
        goto cleanup;
    code = krb5_cc_initialize(context, ccache, default_princ);
    if (code != 0)
        goto cleanup;

    /* Store the validated or renewed cred in the now-empty cache. */
    code = krb5_cc_store_cred(context, ccache, new_creds);
    if (code != 0)
        goto cleanup;

    *out_creds = new_creds;
    new_creds = NULL;

cleanup:
    krb5_free_principal(context, default_princ);
    krb5_free_creds(context, new_creds);
    return code;
}

krb5_error_code KRB5_CALLCONV
krb5_get_credentials_validate(krb5_context context, krb5_flags options,
                              krb5_ccache ccache, krb5_creds *in_creds,
                              krb5_creds **out_creds)
{
    return gc_valrenew(context, ccache, in_creds, KDC_OPT_VALIDATE, out_creds);
}

krb5_error_code KRB5_CALLCONV
krb5_get_credentials_renew(krb5_context context, krb5_flags options,
                           krb5_ccache ccache, krb5_creds *in_creds,
                           krb5_creds **out_creds)
{
    return gc_valrenew(context, ccache, in_creds, KDC_OPT_RENEW, out_creds);
}

/*
 * Core of the newer pair of APIs: get new credentials for in_tkt_service
 * (defaults to the TGT of the client's realm) and store them into *out_creds.
 */
static krb5_error_code
get_valrenewed_creds(krb5_context context, krb5_creds *out_creds,
                     krb5_principal client, krb5_ccache ccache,
                     const char *in_tkt_service, int kdcopt)
{
    krb5_error_code code;
    krb5_creds in_creds, *new_creds;
    krb5_principal server = NULL;

    if (in_tkt_service != NULL) {
        /* Parse in_tkt_service, but use the client's realm. */
        code = krb5_parse_name(context, in_tkt_service, &server);
        if (code != 0)
            goto cleanup;
        krb5_free_data_contents(context, &server->realm);
        code = krb5int_copy_data_contents(context, &client->realm,
                                          &server->realm);
        if (code != 0)
            goto cleanup;
    } else {
        /* Use the TGT name for the client's realm. */
        code = krb5int_tgtname(context, &client->realm, &client->realm,
                               &server);
        if (code != 0)
            goto cleanup;
    }

    memset(&in_creds, 0, sizeof(krb5_creds));
    in_creds.client = client;
    in_creds.server = server;

    /* Get the validated or renewed credential from the KDC. */
    code = get_new_creds(context, ccache, &in_creds, kdcopt, &new_creds);
    if (code != 0)
        goto cleanup;

    /* Fill in *out_creds and free the unwanted new_creds container. */
    *out_creds = *new_creds;
    free(new_creds);

cleanup:
    krb5_free_principal(context, server);
    return code;
}

krb5_error_code KRB5_CALLCONV
krb5_get_validated_creds(krb5_context context, krb5_creds *creds,
                         krb5_principal client, krb5_ccache ccache,
                         const char *in_tkt_service)
{
    return get_valrenewed_creds(context, creds, client, ccache,
                                in_tkt_service, KDC_OPT_VALIDATE);
}

krb5_error_code KRB5_CALLCONV
krb5_get_renewed_creds(krb5_context context, krb5_creds *creds,
                       krb5_principal client, krb5_ccache ccache,
                       const char *in_tkt_service)
{
    return get_valrenewed_creds(context, creds, client, ccache,
                                in_tkt_service, KDC_OPT_RENEW);
}
