/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/mk_req.c */
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
#include "auth_con.h"

/*
  Formats a KRB_AP_REQ message into outbuf.

  server specifies the principal of the server to receive the message; if
  credentials are not present in the credentials cache for this server, the
  TGS request with default parameters is used in an attempt to obtain
  such credentials, and they are stored in ccache.

  kdc_options specifies the options requested for the
  ap_req_options specifies the KRB_AP_REQ options desired.

  checksum specifies the checksum to be used in the authenticator.

  The outbuf buffer storage is allocated, and should be freed by the
  caller when finished.

  returns system errors
*/

krb5_error_code KRB5_CALLCONV
krb5_mk_req(krb5_context context, krb5_auth_context *auth_context,
            krb5_flags ap_req_options, const char *service,
            const char *hostname, krb5_data *in_data, krb5_ccache ccache,
            krb5_data *outbuf)
{
    krb5_error_code       retval;
    krb5_principal        server;
    krb5_creds          * credsp;
    krb5_creds            creds;

    retval = krb5_sname_to_principal(context, hostname, service,
                                     KRB5_NT_SRV_HST, &server);
    if (retval)
        return retval;

    /* obtain ticket & session key */
    memset(&creds, 0, sizeof(creds));
    if ((retval = krb5_copy_principal(context, server, &creds.server)))
        goto cleanup_princ;

    if ((retval = krb5_cc_get_principal(context, ccache, &creds.client)))
        goto cleanup_creds;

    if ((retval = krb5_get_credentials(context, 0,
                                       ccache, &creds, &credsp)))
        goto cleanup_creds;

    retval = krb5_mk_req_extended(context, auth_context, ap_req_options,
                                  in_data, credsp, outbuf);

    krb5_free_creds(context, credsp);

cleanup_creds:
    krb5_free_cred_contents(context, &creds);

cleanup_princ:
    krb5_free_principal(context, server);

    return retval;
}
