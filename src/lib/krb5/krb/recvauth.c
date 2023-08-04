/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/recvauth.c */
/*
 * Copyright 1991 by the Massachusetts Institute of Technology.
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
 *
 * convenience sendauth/recvauth functions
 */

#include "k5-int.h"
#include "auth_con.h"
#include "com_err.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

static const char sendauth_version[] = "KRB5_SENDAUTH_V1.0";

static krb5_error_code
recvauth_common(krb5_context context,
                krb5_auth_context * auth_context,
                /* IN */
                krb5_pointer fd,
                char *appl_version,
                krb5_principal server,
                krb5_int32 flags,
                krb5_keytab keytab,
                /* OUT */
                krb5_ticket ** ticket,
                krb5_data *version)
{
    krb5_auth_context     new_auth_context;
    krb5_flags            ap_option = 0;
    krb5_error_code       retval, problem;
    krb5_data             inbuf;
    krb5_data             outbuf;
    krb5_rcache           rcache = 0;
    krb5_octet            response;
    krb5_data             d;
    int                   need_error_free = 0;
    int                   local_rcache = 0, local_authcon = 0;

    /*
     * Zero out problem variable.  If problem is set at the end of
     * the initial version negotiation section, it means that we
     * need to send an error code back to the client application
     * and exit.
     */
    problem = 0;
    response = 0;

    if (!(flags & KRB5_RECVAUTH_SKIP_VERSION)) {
        /*
         * First read the sendauth version string and check it.
         */
        if ((retval = krb5_read_message(context, fd, &inbuf)))
            return(retval);
        d = make_data((char *)sendauth_version, strlen(sendauth_version) + 1);
        if (!data_eq(inbuf, d)) {
            problem = KRB5_SENDAUTH_BADAUTHVERS;
            response = 1;
        }
        free(inbuf.data);
    }
    if (flags & KRB5_RECVAUTH_BADAUTHVERS) {
        problem = KRB5_SENDAUTH_BADAUTHVERS;
        response = 1;
    }

    /*
     * Do the same thing for the application version string.
     */
    if ((retval = krb5_read_message(context, fd, &inbuf)))
        return(retval);
    if (appl_version != NULL && !problem) {
        d = make_data(appl_version, strlen(appl_version) + 1);
        if (!data_eq(inbuf, d)) {
            problem = KRB5_SENDAUTH_BADAPPLVERS;
            response = 2;
        }
    }
    if (version && !problem)
        *version = inbuf;
    else
        free(inbuf.data);

    /*
     * Now we actually write the response.  If the response is non-zero,
     * exit with a return value of problem
     */
    if ((krb5_net_write(context, *((int *)fd), (char *)&response, 1)) < 0) {
        return(problem); /* We'll return the top-level problem */
    }
    if (problem)
        return(problem);

    /* We are clear of errors here */

    /*
     * Now, let's read the AP_REQ message and decode it
     */
    if ((retval = krb5_read_message(context, fd, &inbuf)))
        return retval;

    if (*auth_context == NULL) {
        problem = krb5_auth_con_init(context, &new_auth_context);
        *auth_context = new_auth_context;
        local_authcon = 1;
    }
    krb5_auth_con_getrcache(context, *auth_context, &rcache);
    if ((!problem) && rcache == NULL) {
        problem = k5_rc_default(context, &rcache);
        if (!problem)
            problem = krb5_auth_con_setrcache(context, *auth_context, rcache);
        local_rcache = 1;
    }
    if (!problem) {
        problem = krb5_rd_req(context, auth_context, &inbuf, server,
                              keytab, &ap_option, ticket);
        free(inbuf.data);
    }

    /*
     * If there was a problem, send back a krb5_error message,
     * preceded by the length of the krb5_error message.  If
     * everything's ok, send back 0 for the length.
     */
    if (problem) {
        krb5_error      error;
        const   char *message;

        memset(&error, 0, sizeof(error));
        krb5_us_timeofday(context, &error.stime, &error.susec);
        if(server)
            error.server = server;
        else {
            /* If this fails - ie. ENOMEM we are hosed
               we cannot even send the error if we wanted to... */
            (void) krb5_parse_name(context, "????", &error.server);
            need_error_free = 1;
        }

        error.error = problem - ERROR_TABLE_BASE_krb5;
        if (error.error > 127)
            error.error = KRB_ERR_GENERIC;
        message = error_message(problem);
        error.text.length  = strlen(message) + 1;
        error.text.data = strdup(message);
        if (!error.text.data) {
            retval = ENOMEM;
            goto cleanup;
        }
        if ((retval = krb5_mk_error(context, &error, &outbuf))) {
            free(error.text.data);
            goto cleanup;
        }
        free(error.text.data);
        if(need_error_free)
            krb5_free_principal(context, error.server);

    } else {
        outbuf.length = 0;
        outbuf.data = 0;
    }

    retval = krb5_write_message(context, fd, &outbuf);
    if (outbuf.data) {
        free(outbuf.data);
        /* We sent back an error, we need cleanup then return */
        retval = problem;
        goto cleanup;
    }
    if (retval)
        goto cleanup;

    /* Here lies the mutual authentication stuff... */
    if ((ap_option & AP_OPTS_MUTUAL_REQUIRED)) {
        if ((retval = krb5_mk_rep(context, *auth_context, &outbuf))) {
            return(retval);
        }
        retval = krb5_write_message(context, fd, &outbuf);
        free(outbuf.data);
    }

cleanup:;
    if (retval) {
        if (local_authcon) {
            krb5_auth_con_free(context, *auth_context);
        } else if (local_rcache && rcache != NULL) {
            k5_rc_close(context, rcache);
            krb5_auth_con_setrcache(context, *auth_context, NULL);
        }
    }
    return retval;
}

krb5_error_code KRB5_CALLCONV
krb5_recvauth(krb5_context context, krb5_auth_context *auth_context, krb5_pointer fd, char *appl_version, krb5_principal server, krb5_int32 flags, krb5_keytab keytab, krb5_ticket **ticket)
{
    return recvauth_common (context, auth_context, fd, appl_version,
                            server, flags, keytab, ticket, 0);
}

krb5_error_code KRB5_CALLCONV
krb5_recvauth_version(krb5_context context,
                      krb5_auth_context *auth_context,
                      /* IN */
                      krb5_pointer fd,
                      krb5_principal server,
                      krb5_int32 flags,
                      krb5_keytab keytab,
                      /* OUT */
                      krb5_ticket **ticket,
                      krb5_data *version)
{
    return recvauth_common (context, auth_context, fd, 0,
                            server, flags, keytab, ticket, version);
}
