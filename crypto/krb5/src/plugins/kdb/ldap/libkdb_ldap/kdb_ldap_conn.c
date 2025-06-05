/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/ldap/libkdb_ldap/kdb_ldap_conn.c */
/*
 * Copyright (c) 2004-2005, Novell, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *   * The copyright holder's name is not used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "autoconf.h"
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "ldap_main.h"
#include "ldap_service_stash.h"
#include <kdb5.h>
#ifdef HAVE_SASL_SASL_H
#include <sasl/sasl.h>
#endif

/* Ensure that we have the parameters we need to authenticate to the LDAP
 * server.  Read the password if necessary. */
static krb5_error_code
validate_context(krb5_context context, krb5_ldap_context *ctx)
{
    krb5_error_code ret;

    if (ctx->sasl_mech != NULL) {
        /* Read the password for use as the SASL secret if we can, but do not
         * require one as not all mechanisms need it. */
        if (ctx->bind_pwd == NULL && ctx->sasl_authcid != NULL &&
            ctx->service_password_file != NULL) {
            (void)krb5_ldap_readpassword(context, ctx->service_password_file,
                                         ctx->sasl_authcid, &ctx->bind_pwd);
        }
        return 0;
    }

    /* For a simple bind, a DN and password are required. */

    if (ctx->bind_dn == NULL) {
        k5_setmsg(context, EINVAL, _("LDAP bind dn value missing"));
        return EINVAL;
    }

    if (ctx->bind_pwd == NULL && ctx->service_password_file == NULL) {
        k5_setmsg(context, EINVAL, _("LDAP bind password value missing"));
        return EINVAL;
    }

    if (ctx->bind_pwd == NULL && ctx->service_password_file != NULL) {
        ret = krb5_ldap_readpassword(context, ctx->service_password_file,
                                     ctx->bind_dn, &ctx->bind_pwd);
        if (ret) {
            k5_prependmsg(context, ret,
                          _("Error reading password from stash"));
            return ret;
        }
    }

    /* An empty password is not allowed. */
    if (*ctx->bind_pwd == '\0') {
        k5_setmsg(context, EINVAL, _("Service password length is zero"));
        return EINVAL;
    }

    return 0;
}

/*
 * Internal Functions called by init functions.
 */

#ifdef HAVE_SASL_SASL_H

static int
interact(LDAP *ld, unsigned flags, void *defaults, void *sin)
{
    sasl_interact_t *in = NULL;
    krb5_ldap_context *ctx = defaults;

    for (in = sin; in != NULL && in->id != SASL_CB_LIST_END; in++) {
        if (in->id == SASL_CB_AUTHNAME)
            in->result = ctx->sasl_authcid;
        else if (in->id == SASL_CB_USER)
            in->result = ctx->sasl_authzid;
        else if (in->id == SASL_CB_GETREALM)
            in->result = ctx->sasl_realm;
        else if (in->id == SASL_CB_PASS)
            in->result = ctx->bind_pwd;
        else
            return LDAP_OTHER;
        in->len = (in->result != NULL) ? strlen(in->result) : 0;
    }

    return LDAP_SUCCESS;
}

#else /* HAVE_SASL_SASL_H */

/* We can't define an interaction function, so only non-interactive mechs like
 * EXTERNAL can work. */
static int
interact(LDAP *ld, unsigned flags, void *defaults, void *sin)
{
    return LDAP_OTHER;
}

#endif

static krb5_error_code
authenticate(krb5_ldap_context *ctx, krb5_ldap_server_handle *server)
{
    int st;
    struct berval bv;

    if (ctx->sasl_mech != NULL) {
        st = ldap_sasl_interactive_bind_s(server->ldap_handle, NULL,
                                          ctx->sasl_mech, NULL, NULL,
                                          LDAP_SASL_QUIET, interact, ctx);
        if (st != LDAP_SUCCESS) {
            k5_setmsg(ctx->kcontext, KRB5_KDB_ACCESS_ERROR,
                      _("Cannot bind to LDAP server '%s' with SASL mechanism "
                        "'%s': %s"), server->server_info->server_name,
                      ctx->sasl_mech, ldap_err2string(st));
            return KRB5_KDB_ACCESS_ERROR;
        }
    } else {
        /* Do a simple bind with DN and password. */
        bv.bv_val = ctx->bind_pwd;
        bv.bv_len = strlen(ctx->bind_pwd);
        st = ldap_sasl_bind_s(server->ldap_handle, ctx->bind_dn, NULL, &bv,
                              NULL, NULL, NULL);
        if (st != LDAP_SUCCESS) {
            k5_setmsg(ctx->kcontext, KRB5_KDB_ACCESS_ERROR,
                      _("Cannot bind to LDAP server '%s' as '%s': %s"),
                      server->server_info->server_name, ctx->bind_dn,
                      ldap_err2string(st));
            return KRB5_KDB_ACCESS_ERROR;
        }
    }
    return 0;
}

static krb5_error_code
initialize_server(krb5_ldap_context *ldap_context, krb5_ldap_server_info *info)
{
    krb5_ldap_server_handle *server;
    krb5_error_code ret;
    int st;

    server = calloc(1, sizeof(krb5_ldap_server_handle));
    if (server == NULL)
        return ENOMEM;
    server->server_info = info;

    st = ldap_initialize(&server->ldap_handle, info->server_name);
    if (st) {
        free(server);
        k5_setmsg(ldap_context->kcontext, KRB5_KDB_ACCESS_ERROR,
                  _("Cannot create LDAP handle for '%s': %s"),
                  info->server_name, ldap_err2string(st));
        return KRB5_KDB_ACCESS_ERROR;
    }

    ret = authenticate(ldap_context, server);
    if (ret) {
        info->server_status = OFF;
        time(&info->downtime);
        free(server);
        return ret;
    }

    server->next = info->ldap_server_handles;
    info->ldap_server_handles = server;
    info->num_conns++;
    info->server_status = ON;
    return 0;
}

/*
 * initialization for data base routines.
 */

krb5_error_code
krb5_ldap_db_init(krb5_context context, krb5_ldap_context *ctx)
{
    krb5_error_code ret;
    int i, version = LDAP_VERSION3;
    unsigned int conns;
    krb5_ldap_server_info *info;
    struct timeval local_timelimit = { 10, 0 };

    ret = validate_context(context, ctx);
    if (ret)
        return ret;

#ifdef LDAP_OPT_DEBUG_LEVEL
    ldap_set_option(NULL, LDAP_OPT_DEBUG_LEVEL, &ctx->ldap_debug);
#endif
    ldap_set_option(NULL, LDAP_OPT_PROTOCOL_VERSION, &version);
#ifdef LDAP_OPT_NETWORK_TIMEOUT
    ldap_set_option(NULL, LDAP_OPT_NETWORK_TIMEOUT, &local_timelimit);
#elif defined LDAP_X_OPT_CONNECT_TIMEOUT
    ldap_set_option(NULL, LDAP_X_OPT_CONNECT_TIMEOUT, &local_timelimit);
#endif

    HNDL_LOCK(ctx);
    for (i = 0; ctx->server_info_list[i] != NULL; i++) {
        info = ctx->server_info_list[i];
        if (info->server_status == NOTSET) {
            krb5_clear_error_message(context);

#ifdef LDAP_MOD_INCREMENT
            info->modify_increment = has_modify_increment(context,
                                                          info->server_name);
#else
            info->modify_increment = 0;
#endif

            for (conns = 0; conns < ctx->max_server_conns; conns++) {
                ret = initialize_server(ctx, info);
                if (ret)
                    break;
            }

            /* If we opened a connection, don't try any more servers. */
            if (info->server_status == ON)
                break;
        }
    }
    HNDL_UNLOCK(ctx);

    return ret;
}


/*
 * get a single handle. Do not lock the mutex
 */

krb5_error_code
krb5_ldap_db_single_init(krb5_ldap_context *ldap_context)
{
    krb5_error_code             st=0;
    int                         cnt=0;
    krb5_ldap_server_info       *server_info=NULL;

    while (ldap_context->server_info_list[cnt] != NULL) {
        server_info = ldap_context->server_info_list[cnt];
        if ((server_info->server_status == NOTSET || server_info->server_status == ON)) {
            if (server_info->num_conns < ldap_context->max_server_conns-1) {
                st = initialize_server(ldap_context, server_info);
                if (st == LDAP_SUCCESS)
                    goto cleanup;
            }
        }
        ++cnt;
    }

    /* If we are here, try to connect to all the servers */

    cnt = 0;
    while (ldap_context->server_info_list[cnt] != NULL) {
        server_info = ldap_context->server_info_list[cnt];
        st = initialize_server(ldap_context, server_info);
        if (st == LDAP_SUCCESS)
            goto cleanup;
        ++cnt;
    }
cleanup:
    return (st);
}

krb5_error_code
krb5_ldap_rebind(krb5_ldap_context *ldap_context,
                 krb5_ldap_server_handle **ldap_server_handle)
{
    krb5_ldap_server_handle *handle = *ldap_server_handle;

    ldap_unbind_ext_s(handle->ldap_handle, NULL, NULL);
    if (ldap_initialize(&handle->ldap_handle,
                        handle->server_info->server_name) != LDAP_SUCCESS ||
        authenticate(ldap_context, handle) != 0) {
        return krb5_ldap_request_next_handle_from_pool(ldap_context,
                                                       ldap_server_handle);
    }
    return LDAP_SUCCESS;
}

/*
 *     DAL API functions
 */
krb5_error_code
krb5_ldap_lib_init()
{
    return 0;
}

krb5_error_code
krb5_ldap_lib_cleanup()
{
    /* right now, no cleanup required */
    return 0;
}

krb5_error_code
krb5_ldap_free_ldap_context(krb5_ldap_context *ldap_context)
{
    if (ldap_context == NULL)
        return 0;

    free(ldap_context->container_dn);
    ldap_context->container_dn = NULL;

    krb5_ldap_free_realm_params(ldap_context->lrparams);
    ldap_context->lrparams = NULL;

    krb5_ldap_free_server_params(ldap_context);

    return 0;
}

krb5_error_code
krb5_ldap_close(krb5_context context)
{
    kdb5_dal_handle  *dal_handle=NULL;
    krb5_ldap_context *ldap_context=NULL;

    if (context == NULL ||
        context->dal_handle == NULL ||
        context->dal_handle->db_context == NULL)
        return 0;

    dal_handle = context->dal_handle;
    ldap_context = (krb5_ldap_context *) dal_handle->db_context;
    dal_handle->db_context = NULL;

    krb5_ldap_free_ldap_context(ldap_context);

    return 0;
}
