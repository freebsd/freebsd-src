/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/changepw.c */
/*
 * Copyright 1990,1999,2001,2008 by the Massachusetts Institute of Technology.
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
 * krb5_set_password - Implements set password per RFC 3244
 * Added by Paul W. Nelson, Thursby Software Systems, Inc.
 * Modified by Todd Stecher, Isilon Systems, to use krb1.4 socket
 *  infrastructure
 */

#include "k5-int.h"
#include "fake-addrinfo.h"
#include "os-proto.h"
#include "../krb/auth_con.h"
#include "../krb/int-proto.h"

#include <stdio.h>
#include <errno.h>

#ifndef GETSOCKNAME_ARG3_TYPE
#define GETSOCKNAME_ARG3_TYPE int
#endif

struct sendto_callback_context {
    krb5_context        context;
    krb5_auth_context   auth_context;
    krb5_principal      set_password_for;
    const char          *newpw;
    krb5_data           ap_req;
    krb5_ui_4           remote_seq_num, local_seq_num;
};

/*
 * Wrapper function for the two backends
 */

static krb5_error_code
locate_kpasswd(krb5_context context, const krb5_data *realm,
               struct serverlist *serverlist)
{
    krb5_error_code code;

    code = k5_locate_server(context, realm, serverlist, locate_service_kpasswd,
                            FALSE);
    if (code == KRB5_REALM_CANT_RESOLVE || code == KRB5_REALM_UNKNOWN) {
        code = k5_locate_server(context, realm, serverlist,
                                locate_service_kadmin, TRUE);
        if (!code) {
            /* Success with admin_server but now we need to change the port
             * number to use DEFAULT_KPASSWD_PORT and the transport. */
            size_t i;
            for (i = 0; i < serverlist->nservers; i++) {
                struct server_entry *s = &serverlist->servers[i];

                if (s->transport == TCP)
                    s->transport = TCP_OR_UDP;
                if (s->hostname != NULL)
                    s->port = DEFAULT_KPASSWD_PORT;
                else if (s->family == AF_INET)
                    ss2sin(&s->addr)->sin_port = htons(DEFAULT_KPASSWD_PORT);
                else if (s->family == AF_INET6)
                    ss2sin6(&s->addr)->sin6_port = htons(DEFAULT_KPASSWD_PORT);
            }
        }
    }
    return (code);
}


/**
 * This routine is used for a callback in sendto_kdc.c code. Simply
 * put, we need the client addr to build the krb_priv portion of the
 * password request.
 */


static void
kpasswd_sendto_msg_cleanup(void *data, krb5_data *message)
{
    struct sendto_callback_context *ctx = data;

    krb5_free_data_contents(ctx->context, message);
}


static int
kpasswd_sendto_msg_callback(SOCKET fd, void *data, krb5_data *message)
{
    krb5_error_code                     code = 0;
    struct sockaddr_storage             local_addr;
    krb5_address                        local_kaddr;
    struct sendto_callback_context      *ctx = data;
    GETSOCKNAME_ARG3_TYPE               addrlen;
    krb5_data                           output;

    memset (message, 0, sizeof(krb5_data));

    /*
     * We need the local addr from the connection socket
     */
    addrlen = sizeof(local_addr);

    if (getsockname(fd, ss2sa(&local_addr), &addrlen) < 0) {
        code = SOCKET_ERRNO;
        goto cleanup;
    }

    /* some brain-dead OS's don't return useful information from
     * the getsockname call.  Namely, windows and solaris.  */

    if (local_addr.ss_family == AF_INET &&
        ss2sin(&local_addr)->sin_addr.s_addr != 0) {
        local_kaddr.addrtype = ADDRTYPE_INET;
        local_kaddr.length = sizeof(ss2sin(&local_addr)->sin_addr);
        local_kaddr.contents = (krb5_octet *) &ss2sin(&local_addr)->sin_addr;
    } else if (local_addr.ss_family == AF_INET6 &&
               memcmp(ss2sin6(&local_addr)->sin6_addr.s6_addr,
                      in6addr_any.s6_addr, sizeof(in6addr_any.s6_addr)) != 0) {
        local_kaddr.addrtype = ADDRTYPE_INET6;
        local_kaddr.length = sizeof(ss2sin6(&local_addr)->sin6_addr);
        local_kaddr.contents = (krb5_octet *) &ss2sin6(&local_addr)->sin6_addr;
    } else {
        krb5_address **addrs;

        code = krb5_os_localaddr(ctx->context, &addrs);
        if (code)
            goto cleanup;

        local_kaddr.magic = addrs[0]->magic;
        local_kaddr.addrtype = addrs[0]->addrtype;
        local_kaddr.length = addrs[0]->length;
        local_kaddr.contents = k5memdup(addrs[0]->contents, addrs[0]->length,
                                        &code);
        krb5_free_addresses(ctx->context, addrs);
        if (local_kaddr.contents == NULL)
            goto cleanup;
    }


    /*
     * TBD:  Does this tamper w/ the auth context in such a way
     * to break us?  Yes - provide 1 per conn-state / host...
     */


    if ((code = krb5_auth_con_setaddrs(ctx->context, ctx->auth_context,
                                       &local_kaddr, NULL)))
        goto cleanup;

    ctx->auth_context->remote_seq_number = ctx->remote_seq_num;
    ctx->auth_context->local_seq_number = ctx->local_seq_num;

    if (ctx->set_password_for)
        code = krb5int_mk_setpw_req(ctx->context,
                                    ctx->auth_context,
                                    &ctx->ap_req,
                                    ctx->set_password_for,
                                    ctx->newpw,
                                    &output);
    else
        code = krb5int_mk_chpw_req(ctx->context,
                                   ctx->auth_context,
                                   &ctx->ap_req,
                                   ctx->newpw,
                                   &output);
    if (code)
        goto cleanup;

    message->length = output.length;
    message->data = output.data;

cleanup:
    return code;
}


/*
** The logic for setting and changing a password is mostly the same
** change_set_password handles both cases
**      if set_password_for is NULL, then a password change is performed,
**  otherwise, the password is set for the principal indicated in set_password_for
*/
static krb5_error_code
change_set_password(krb5_context context,
                    krb5_creds *creds,
                    const char *newpw,
                    krb5_principal set_password_for,
                    int *result_code,
                    krb5_data *result_code_string,
                    krb5_data *result_string)
{
    krb5_data                   chpw_rep;
    GETSOCKNAME_ARG3_TYPE       addrlen;
    krb5_error_code             code = 0;
    char                        *code_string;
    int                         local_result_code;

    struct sendto_callback_context  callback_ctx;
    struct sendto_callback_info callback_info;
    struct sockaddr_storage     remote_addr;
    struct serverlist           sl = SERVERLIST_INIT;

    memset(&chpw_rep, 0, sizeof(krb5_data));
    memset( &callback_ctx, 0, sizeof(struct sendto_callback_context));
    callback_ctx.context = context;
    callback_ctx.newpw = newpw;
    callback_ctx.set_password_for = set_password_for;

    if ((code = krb5_auth_con_init(callback_ctx.context,
                                   &callback_ctx.auth_context)))
        goto cleanup;

    if ((code = krb5_mk_req_extended(callback_ctx.context,
                                     &callback_ctx.auth_context,
                                     AP_OPTS_USE_SUBKEY,
                                     NULL,
                                     creds,
                                     &callback_ctx.ap_req)))
        goto cleanup;

    callback_ctx.remote_seq_num = callback_ctx.auth_context->remote_seq_number;
    callback_ctx.local_seq_num = callback_ctx.auth_context->local_seq_number;

    code = locate_kpasswd(callback_ctx.context, &creds->server->realm, &sl);
    if (code)
        goto cleanup;

    addrlen = sizeof(remote_addr);

    callback_info.data = &callback_ctx;
    callback_info.pfn_callback = kpasswd_sendto_msg_callback;
    callback_info.pfn_cleanup = kpasswd_sendto_msg_cleanup;
    krb5_free_data_contents(callback_ctx.context, &chpw_rep);

    /* UDP retransmits may be seen as replays.  Only try UDP after other
     * transports fail completely. */
    code = k5_sendto(callback_ctx.context, NULL, &creds->server->realm,
                     &sl, NO_UDP, &callback_info, &chpw_rep,
                     ss2sa(&remote_addr), &addrlen, NULL, NULL, NULL);
    if (code == KRB5_KDC_UNREACH) {
        code = k5_sendto(callback_ctx.context, NULL, &creds->server->realm,
                         &sl, ONLY_UDP, &callback_info, &chpw_rep,
                         ss2sa(&remote_addr), &addrlen, NULL, NULL, NULL);
    }
    if (code)
        goto cleanup;

    code = krb5int_rd_chpw_rep(callback_ctx.context,
                               callback_ctx.auth_context,
                               &chpw_rep, &local_result_code,
                               result_string);

    if (code)
        goto cleanup;

    if (result_code)
        *result_code = local_result_code;

    if (result_code_string) {
        code = krb5_chpw_result_code_string(callback_ctx.context,
                                            local_result_code,
                                            &code_string);
        if (code)
            goto cleanup;

        result_code_string->length = strlen(code_string);
        result_code_string->data = malloc(result_code_string->length);
        if (result_code_string->data == NULL) {
            code = ENOMEM;
            goto cleanup;
        }
        strncpy(result_code_string->data, code_string, result_code_string->length);
    }

cleanup:
    if (callback_ctx.auth_context != NULL)
        krb5_auth_con_free(callback_ctx.context, callback_ctx.auth_context);

    k5_free_serverlist(&sl);
    krb5_free_data_contents(callback_ctx.context, &callback_ctx.ap_req);
    krb5_free_data_contents(callback_ctx.context, &chpw_rep);

    return(code);
}

krb5_error_code KRB5_CALLCONV
krb5_change_password(krb5_context context,
                     krb5_creds *creds,
                     const char *newpw,
                     int *result_code,
                     krb5_data *result_code_string,
                     krb5_data *result_string)
{
    return change_set_password(context, creds, newpw, NULL,
                               result_code, result_code_string, result_string );
}

/*
 * krb5_set_password - Implements set password per RFC 3244
 *
 */

krb5_error_code KRB5_CALLCONV
krb5_set_password(krb5_context context,
                  krb5_creds *creds,
                  const char *newpw,
                  krb5_principal change_password_for,
                  int *result_code,
                  krb5_data *result_code_string,
                  krb5_data *result_string
)
{
    return change_set_password(context, creds, newpw, change_password_for,
                               result_code, result_code_string, result_string );
}

krb5_error_code KRB5_CALLCONV
krb5_set_password_using_ccache(krb5_context context,
                               krb5_ccache ccache,
                               const char *newpw,
                               krb5_principal change_password_for,
                               int *result_code,
                               krb5_data *result_code_string,
                               krb5_data *result_string
)
{
    krb5_creds          creds;
    krb5_creds          *credsp;
    krb5_error_code     code;

    /*
    ** get the proper creds for use with krb5_set_password -
    */
    memset (&creds, 0, sizeof(creds));
    /*
    ** first get the principal for the password service -
    */
    code = krb5_cc_get_principal (context, ccache, &creds.client);
    if (!code) {
        code = krb5_build_principal(context, &creds.server,
                                    change_password_for->realm.length,
                                    change_password_for->realm.data,
                                    "kadmin", "changepw", NULL);
        if (!code) {
            code = krb5_get_credentials(context, 0, ccache, &creds, &credsp);
            if (!code) {
                code = krb5_set_password(context, credsp, newpw, change_password_for,
                                         result_code, result_code_string,
                                         result_string);
                krb5_free_creds(context, credsp);
            }
        }
        krb5_free_cred_contents(context, &creds);
    }
    return code;
}
