/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/rd_req.c */
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
#include "auth_con.h"

/*
 *  Parses a KRB_AP_REQ message, returning its contents.
 *
 *  server specifies the expected server's name for the ticket.
 *
 *  keyproc specifies a procedure to generate a decryption key for the
 *  ticket.  If keyproc is non-NULL, keyprocarg is passed to it, and the result
 *  used as a decryption key. If keyproc is NULL, then fetchfrom is checked;
 *  if it is non-NULL, it specifies a parameter name from which to retrieve the
 *  decryption key.  If fetchfrom is NULL, then the default key store is
 *  consulted.
 *
 *  returns system errors, encryption errors, replay errors
 */

krb5_error_code KRB5_CALLCONV
krb5_rd_req(krb5_context context, krb5_auth_context *auth_context,
            const krb5_data *inbuf, krb5_const_principal server,
            krb5_keytab keytab, krb5_flags *ap_req_options,
            krb5_ticket **ticket)
{
    krb5_error_code       retval;
    krb5_ap_req         * request;
    krb5_auth_context     new_auth_context;
    krb5_keytab           new_keytab = NULL;

    if (!krb5_is_ap_req(inbuf))
        return KRB5KRB_AP_ERR_MSG_TYPE;
#ifndef LEAN_CLIENT
    if ((retval = decode_krb5_ap_req(inbuf, &request))) {
        switch (retval) {
        case KRB5_BADMSGTYPE:
            return KRB5KRB_AP_ERR_BADVERSION;
        default:
            return(retval);
        }
    }
#endif /* LEAN_CLIENT */

    /* Get an auth context if necessary. */
    new_auth_context = NULL;
    if (*auth_context == NULL) {
        if ((retval = krb5_auth_con_init(context, &new_auth_context)))
            goto cleanup_request;
        *auth_context = new_auth_context;
    }


#ifndef LEAN_CLIENT
    /* Get a keytab if necessary. */
    if (keytab == NULL) {
        if ((retval = krb5_kt_default(context, &new_keytab)))
            goto cleanup_auth_context;
        keytab = new_keytab;
    }
#endif /* LEAN_CLIENT */

    retval = krb5_rd_req_decoded(context, auth_context, request, server,
                                 keytab, ap_req_options, NULL);
    if (!retval && ticket != NULL) {
        /* Steal the ticket pointer for the caller. */
        *ticket = request->ticket;
        request->ticket = NULL;
    }

#ifndef LEAN_CLIENT
    if (new_keytab != NULL)
        (void) krb5_kt_close(context, new_keytab);
#endif /* LEAN_CLIENT */

cleanup_auth_context:
    if (new_auth_context && retval) {
        krb5_auth_con_free(context, new_auth_context);
        *auth_context = NULL;
    }

cleanup_request:
    krb5_free_ap_req(context, request);
    return retval;
}
