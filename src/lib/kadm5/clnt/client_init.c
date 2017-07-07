/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 */

/*
 * Copyright (C) 1998 by the FundsXpress, INC.
 *
 * All rights reserved.
 *
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of FundsXpress. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  FundsXpress makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <k5-int.h>
#include <netdb.h>
#include <com_err.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fake-addrinfo.h>
#include <krb5.h>

#include <kadm5/admin.h>
#include <kadm5/kadm_rpc.h>
#include "client_internal.h"
#include <iprop_hdr.h>
#include "iprop.h"

#include <gssrpc/rpc.h>
#include <gssapi/gssapi.h>
#include <gssapi/gssapi_krb5.h>
#include <gssrpc/auth_gssapi.h>

#define ADM_CCACHE  "/tmp/ovsec_adm.XXXXXX"

enum init_type { INIT_PASS, INIT_SKEY, INIT_CREDS, INIT_ANONYMOUS };

static kadm5_ret_t
init_any(krb5_context context, char *client_name, enum init_type init_type,
         char *pass, krb5_ccache ccache_in, char *service_name,
         kadm5_config_params *params, krb5_ui_4 struct_version,
         krb5_ui_4 api_version, char **db_args, void **server_handle);

static kadm5_ret_t
get_init_creds(kadm5_server_handle_t handle, krb5_principal client,
               enum init_type init_type, char *pass, krb5_ccache ccache_in,
               char *svcname_in, char *realm, krb5_principal *server_out);

static kadm5_ret_t
gic_iter(kadm5_server_handle_t handle, enum init_type init_type,
         krb5_ccache ccache, krb5_principal client, char *pass,
         char *svcname, char *realm, krb5_principal *server_out);

static kadm5_ret_t
connect_to_server(const char *hostname, int port, int *fd);

static kadm5_ret_t
setup_gss(kadm5_server_handle_t handle, kadm5_config_params *params_in,
          krb5_principal client, krb5_principal server);

static void
rpc_auth(kadm5_server_handle_t handle, kadm5_config_params *params_in,
         gss_cred_id_t gss_client_creds, gss_name_t gss_target);

kadm5_ret_t
kadm5_init_with_creds(krb5_context context, char *client_name,
                      krb5_ccache ccache, char *service_name,
                      kadm5_config_params *params, krb5_ui_4 struct_version,
                      krb5_ui_4 api_version, char **db_args,
                      void **server_handle)
{
    return init_any(context, client_name, INIT_CREDS, NULL, ccache,
                    service_name, params, struct_version, api_version, db_args,
                    server_handle);
}

kadm5_ret_t
kadm5_init_with_password(krb5_context context, char *client_name,
                         char *pass, char *service_name,
                         kadm5_config_params *params, krb5_ui_4 struct_version,
                         krb5_ui_4 api_version, char **db_args,
                         void **server_handle)
{
    return init_any(context, client_name, INIT_PASS, pass, NULL, service_name,
                    params, struct_version, api_version, db_args,
                    server_handle);
}

kadm5_ret_t
kadm5_init_anonymous(krb5_context context, char *client_name,
                     char *service_name, kadm5_config_params *params,
                     krb5_ui_4 struct_version, krb5_ui_4 api_version,
                     char **db_args, void **server_handle)
{
    return init_any(context, client_name, INIT_ANONYMOUS, NULL, NULL,
                    service_name, params, struct_version, api_version,
                    db_args, server_handle);
}

kadm5_ret_t
kadm5_init(krb5_context context, char *client_name, char *pass,
           char *service_name, kadm5_config_params *params,
           krb5_ui_4 struct_version, krb5_ui_4 api_version, char **db_args,
           void **server_handle)
{
    return init_any(context, client_name, INIT_PASS, pass, NULL, service_name,
                    params, struct_version, api_version, db_args,
                    server_handle);
}

kadm5_ret_t
kadm5_init_with_skey(krb5_context context, char *client_name,
                     char *keytab, char *service_name,
                     kadm5_config_params *params, krb5_ui_4 struct_version,
                     krb5_ui_4 api_version, char **db_args,
                     void **server_handle)
{
    return init_any(context, client_name, INIT_SKEY, keytab, NULL,
                    service_name, params, struct_version, api_version, db_args,
                    server_handle);
}

static kadm5_ret_t
init_any(krb5_context context, char *client_name, enum init_type init_type,
         char *pass, krb5_ccache ccache_in, char *service_name,
         kadm5_config_params *params_in, krb5_ui_4 struct_version,
         krb5_ui_4 api_version, char **db_args, void **server_handle)
{
    int fd = -1;
    OM_uint32 minor_stat;
    krb5_boolean iprop_enable;
    int port;
    rpcprog_t rpc_prog;
    rpcvers_t rpc_vers;
    krb5_ccache ccache;
    krb5_principal client = NULL, server = NULL;
    struct timeval timeout;

    kadm5_server_handle_t handle;
    kadm5_config_params params_local;

    int code = 0;
    generic_ret r = { 0, 0 };

    initialize_ovk_error_table();
/*      initialize_adb_error_table(); */
    initialize_ovku_error_table();

    if (! server_handle) {
        return EINVAL;
    }

    if (! (handle = malloc(sizeof(*handle)))) {
        return ENOMEM;
    }
    memset(handle, 0, sizeof(*handle));
    if (! (handle->lhandle = malloc(sizeof(*handle)))) {
        free(handle);
        return ENOMEM;
    }

    handle->magic_number = KADM5_SERVER_HANDLE_MAGIC;
    handle->struct_version = struct_version;
    handle->api_version = api_version;
    handle->clnt = 0;
    handle->client_socket = -1;
    handle->cache_name = 0;
    handle->destroy_cache = 0;
    handle->context = 0;
    handle->cred = GSS_C_NO_CREDENTIAL;
    *handle->lhandle = *handle;
    handle->lhandle->api_version = KADM5_API_VERSION_4;
    handle->lhandle->struct_version = KADM5_STRUCT_VERSION;
    handle->lhandle->lhandle = handle->lhandle;

    handle->context = context;

    if(client_name == NULL) {
        free(handle);
        return EINVAL;
    }

    /*
     * Verify the version numbers before proceeding; we can't use
     * CHECK_HANDLE because not all fields are set yet.
     */
    GENERIC_CHECK_HANDLE(handle, KADM5_OLD_LIB_API_VERSION,
                         KADM5_NEW_LIB_API_VERSION);

    memset(&params_local, 0, sizeof(params_local));

    if ((code = kadm5_get_config_params(handle->context, 0,
                                        params_in, &handle->params))) {
        free(handle);
        return(code);
    }

#define REQUIRED_PARAMS (KADM5_CONFIG_REALM |           \
                         KADM5_CONFIG_ADMIN_SERVER |    \
                         KADM5_CONFIG_KADMIND_PORT)

    if ((handle->params.mask & REQUIRED_PARAMS) != REQUIRED_PARAMS) {
        free(handle);
        return KADM5_MISSING_KRB5_CONF_PARAMS;
    }

    code = krb5_parse_name(handle->context, client_name, &client);
    if (code)
        goto error;

    /*
     * Get credentials.  Also does some fallbacks in case kadmin/fqdn
     * principal doesn't exist.
     */
    code = get_init_creds(handle, client, init_type, pass, ccache_in,
                          service_name, handle->params.realm, &server);
    if (code)
        goto error;

    /* If the service_name and client_name are iprop-centric, use the iprop
     * port and RPC identifiers. */
    iprop_enable = (service_name != NULL &&
                    strstr(service_name, KIPROP_SVC_NAME) != NULL &&
                    strstr(client_name, KIPROP_SVC_NAME) != NULL);
    if (iprop_enable) {
        port = handle->params.iprop_port;
        rpc_prog = KRB5_IPROP_PROG;
        rpc_vers = KRB5_IPROP_VERS;
    } else {
        port = handle->params.kadmind_port;
        rpc_prog = KADM;
        rpc_vers = KADMVERS;
    }

    code = connect_to_server(handle->params.admin_server, port, &fd);
    if (code)
        goto error;

    handle->clnt = clnttcp_create(NULL, rpc_prog, rpc_vers, &fd, 0, 0);
    if (handle->clnt == NULL) {
        code = KADM5_RPC_ERROR;
#ifdef DEBUG
        clnt_pcreateerror("clnttcp_create");
#endif
        goto error;
    }

    /* Set a one-hour timeout. */
    timeout.tv_sec = 3600;
    timeout.tv_usec = 0;
    (void)clnt_control(handle->clnt, CLSET_TIMEOUT, &timeout);

    handle->client_socket = fd;
    handle->lhandle->clnt = handle->clnt;
    handle->lhandle->client_socket = fd;

    /* now that handle->clnt is set, we can check the handle */
    if ((code = _kadm5_check_handle((void *) handle)))
        goto error;

    /*
     * The RPC connection is open; establish the GSS-API
     * authentication context.
     */
    code = setup_gss(handle, params_in,
                     (init_type == INIT_CREDS) ? client : NULL, server);
    if (code)
        goto error;

    /*
     * Bypass the remainder of the code and return straightaway
     * if the gss service requested is kiprop
     */
    if (iprop_enable) {
        code = 0;
        *server_handle = (void *) handle;
        goto cleanup;
    }

    if (init_2(&handle->api_version, &r, handle->clnt)) {
        code = KADM5_RPC_ERROR;
#ifdef DEBUG
        clnt_perror(handle->clnt, "init_2 null resp");
#endif
        goto error;
    }
    /* Drop down to v3 wire protocol if server does not support v4 */
    if (r.code == KADM5_NEW_SERVER_API_VERSION &&
        handle->api_version == KADM5_API_VERSION_4) {
        handle->api_version = KADM5_API_VERSION_3;
        memset(&r, 0, sizeof(generic_ret));
        if (init_2(&handle->api_version, &r, handle->clnt)) {
            code = KADM5_RPC_ERROR;
            goto error;
        }
    }
    /* Drop down to v2 wire protocol if server does not support v3 */
    if (r.code == KADM5_NEW_SERVER_API_VERSION &&
        handle->api_version == KADM5_API_VERSION_3) {
        handle->api_version = KADM5_API_VERSION_2;
        memset(&r, 0, sizeof(generic_ret));
        if (init_2(&handle->api_version, &r, handle->clnt)) {
            code = KADM5_RPC_ERROR;
            goto error;
        }
    }
    if (r.code) {
        code = r.code;
        goto error;
    }

    *server_handle = (void *) handle;

    goto cleanup;

error:
    /*
     * Note that it is illegal for this code to execute if "handle"
     * has not been allocated and initialized.  I.e., don't use "goto
     * error" before the block of code at the top of the function
     * that allocates and initializes "handle".
     */
    if (handle->destroy_cache && handle->cache_name) {
        if (krb5_cc_resolve(handle->context,
                            handle->cache_name, &ccache) == 0)
            (void) krb5_cc_destroy (handle->context, ccache);
    }
    if (handle->cache_name)
        free(handle->cache_name);
    (void)gss_release_cred(&minor_stat, &handle->cred);
    if(handle->clnt && handle->clnt->cl_auth)
        AUTH_DESTROY(handle->clnt->cl_auth);
    if(handle->clnt)
        clnt_destroy(handle->clnt);
    if (fd != -1)
        close(fd);
    free(handle->lhandle);
    kadm5_free_config_params(handle->context, &handle->params);

cleanup:
    krb5_free_principal(handle->context, client);
    krb5_free_principal(handle->context, server);
    if (code)
        free(handle);

    return code;
}

/* Get initial credentials for authenticating to server.  Perform fallback from
 * kadmin/fqdn to kadmin/admin if svcname_in is NULL. */
static kadm5_ret_t
get_init_creds(kadm5_server_handle_t handle, krb5_principal client,
               enum init_type init_type, char *pass, krb5_ccache ccache_in,
               char *svcname_in, char *realm, krb5_principal *server_out)
{
    kadm5_ret_t code;
    krb5_ccache ccache = NULL;
    char svcname[BUFSIZ];

    *server_out = NULL;

    /* NULL svcname means use host-based. */
    if (svcname_in == NULL) {
        code = kadm5_get_admin_service_name(handle->context,
                                            handle->params.realm,
                                            svcname, sizeof(svcname));
        if (code)
            goto error;
    } else {
        strncpy(svcname, svcname_in, sizeof(svcname));
        svcname[sizeof(svcname)-1] = '\0';
    }

    /*
     * Acquire a service ticket for svcname@realm for client, using password
     * pass (which could be NULL), and create a ccache to store them in.  If
     * INIT_CREDS, use the ccache we were provided instead.
     */
    if (init_type == INIT_CREDS) {
        ccache = ccache_in;
        if (asprintf(&handle->cache_name, "%s:%s",
                     krb5_cc_get_type(handle->context, ccache),
                     krb5_cc_get_name(handle->context, ccache)) < 0) {
            handle->cache_name = NULL;
            code = ENOMEM;
            goto error;
        }
    } else {
        static int counter = 0;

        if (asprintf(&handle->cache_name, "MEMORY:kadm5_%u", counter++) < 0) {
            handle->cache_name = NULL;
            code = ENOMEM;
            goto error;
        }
        code = krb5_cc_resolve(handle->context, handle->cache_name,
                               &ccache);
        if (code)
            goto error;

        code = krb5_cc_initialize (handle->context, ccache, client);
        if (code)
            goto error;

        handle->destroy_cache = 1;
    }
    handle->lhandle->cache_name = handle->cache_name;

    code = gic_iter(handle, init_type, ccache, client, pass, svcname, realm,
                    server_out);
    if ((code == KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN
         || code == KRB5_CC_NOTFOUND) && svcname_in == NULL) {
        /* Retry with old host-independent service principal. */
        code = gic_iter(handle, init_type, ccache, client, pass,
                        KADM5_ADMIN_SERVICE, realm, server_out);
    }
    /* Improved error messages */
    if (code == KRB5KRB_AP_ERR_BAD_INTEGRITY) code = KADM5_BAD_PASSWORD;
    if (code == KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN)
        code = KADM5_SECURE_PRINC_MISSING;

error:
    if (ccache != NULL && init_type != INIT_CREDS)
        krb5_cc_close(handle->context, ccache);
    return code;
}

/* Perform one iteration of attempting to get credentials.  This includes
 * searching existing ccache for requested service if INIT_CREDS. */
static kadm5_ret_t
gic_iter(kadm5_server_handle_t handle, enum init_type init_type,
         krb5_ccache ccache, krb5_principal client, char *pass, char *svcname,
         char *realm, krb5_principal *server_out)
{
    kadm5_ret_t code;
    krb5_context ctx;
    krb5_keytab kt;
    krb5_get_init_creds_opt *opt = NULL;
    krb5_creds mcreds, outcreds;

    *server_out = NULL;
    ctx = handle->context;
    kt = NULL;
    memset(&opt, 0, sizeof(opt));
    memset(&mcreds, 0, sizeof(mcreds));
    memset(&outcreds, 0, sizeof(outcreds));

    /* Credentials for kadmin don't need to be forwardable or proxiable. */
    if (init_type != INIT_CREDS) {
        code = krb5_get_init_creds_opt_alloc(ctx, &opt);
        krb5_get_init_creds_opt_set_forwardable(opt, 0);
        krb5_get_init_creds_opt_set_proxiable(opt, 0);
        krb5_get_init_creds_opt_set_out_ccache(ctx, opt, ccache);
        if (init_type == INIT_ANONYMOUS)
            krb5_get_init_creds_opt_set_anonymous(opt, 1);
    }

    if (init_type == INIT_PASS || init_type == INIT_ANONYMOUS) {
        code = krb5_get_init_creds_password(ctx, &outcreds, client, pass,
                                            krb5_prompter_posix,
                                            NULL, 0, svcname, opt);
        if (code)
            goto error;
    } else if (init_type == INIT_SKEY) {
        if (pass) {
            code = krb5_kt_resolve(ctx, pass, &kt);
            if (code)
                goto error;
        }
        code = krb5_get_init_creds_keytab(ctx, &outcreds, client, kt,
                                          0, svcname, opt);
        if (pass)
            krb5_kt_close(ctx, kt);
        if (code)
            goto error;
    } else if (init_type == INIT_CREDS) {
        mcreds.client = client;
        code = krb5_parse_name_flags(ctx, svcname,
                                     KRB5_PRINCIPAL_PARSE_IGNORE_REALM,
                                     &mcreds.server);
        if (code)
            goto error;
        code = krb5_set_principal_realm(ctx, mcreds.server, realm);
        if (code)
            goto error;
        code = krb5_cc_retrieve_cred(ctx, ccache, 0,
                                     &mcreds, &outcreds);
        krb5_free_principal(ctx, mcreds.server);
        if (code)
            goto error;
    } else {
        code = EINVAL;
        goto error;
    }

    /* Steal the server principal of the creds we acquired and return it to the
     * caller, which needs to knows what service to authenticate to. */
    *server_out = outcreds.server;
    outcreds.server = NULL;

error:
    krb5_free_cred_contents(ctx, &outcreds);
    if (opt)
        krb5_get_init_creds_opt_free(ctx, opt);
    return code;
}

/* Set *fd to a socket connected to hostname and port. */
static kadm5_ret_t
connect_to_server(const char *hostname, int port, int *fd)
{
    struct addrinfo hint, *addrs, *a;
    char portbuf[32];
    int err, s;
    kadm5_ret_t code;

    /* Look up the server's addresses. */
    (void) snprintf(portbuf, sizeof(portbuf), "%d", port);
    memset(&hint, 0, sizeof(hint));
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_ADDRCONFIG;
#ifdef AI_NUMERICSERV
    hint.ai_flags |= AI_NUMERICSERV;
#endif
    err = getaddrinfo(hostname, portbuf, &hint, &addrs);
    if (err != 0)
        return KADM5_CANT_RESOLVE;

    /* Try to connect to each address until we succeed. */
    for (a = addrs; a != NULL; a = a->ai_next) {
        s = socket(a->ai_family, a->ai_socktype, 0);
        if (s == -1) {
            code = KADM5_FAILURE;
            goto cleanup;
        }
        err = connect(s, a->ai_addr, a->ai_addrlen);
        if (err == 0) {
            *fd = s;
            code = 0;
            goto cleanup;
        }
        close(s);
    }

    /* We didn't succeed on any address. */
    code = KADM5_RPC_ERROR;
cleanup:
    freeaddrinfo(addrs);
    return code;
}

/* Acquire GSSAPI credentials and set up RPC auth flavor. */
static kadm5_ret_t
setup_gss(kadm5_server_handle_t handle, kadm5_config_params *params_in,
          krb5_principal client, krb5_principal server)
{
    OM_uint32 gssstat, minor_stat;
    gss_buffer_desc buf;
    gss_name_t gss_client;
    gss_name_t gss_target;
    const char *c_ccname_orig;
    char *ccname_orig;

    ccname_orig = NULL;
    gss_client = gss_target = GSS_C_NO_NAME;

    /* Temporarily use the kadm5 cache. */
    gssstat = gss_krb5_ccache_name(&minor_stat, handle->cache_name,
                                   &c_ccname_orig);
    if (gssstat != GSS_S_COMPLETE)
        goto error;
    if (c_ccname_orig)
        ccname_orig = strdup(c_ccname_orig);
    else
        ccname_orig = 0;

    buf.value = &server;
    buf.length = sizeof(server);
    gssstat = gss_import_name(&minor_stat, &buf,
                              (gss_OID)gss_nt_krb5_principal, &gss_target);
    if (gssstat != GSS_S_COMPLETE)
        goto error;

    if (client != NULL) {
        buf.value = &client;
        buf.length = sizeof(client);
        gssstat = gss_import_name(&minor_stat, &buf,
                                  (gss_OID)gss_nt_krb5_principal, &gss_client);
    } else gss_client = GSS_C_NO_NAME;

    if (gssstat != GSS_S_COMPLETE)
        goto error;

    gssstat = gss_acquire_cred(&minor_stat, gss_client, 0,
                               GSS_C_NULL_OID_SET, GSS_C_INITIATE,
                               &handle->cred, NULL, NULL);
    if (gssstat != GSS_S_COMPLETE) {
#if 0 /* for debugging only */
        {
            OM_uint32 maj_status, min_status, message_context = 0;
            gss_buffer_desc status_string;
            do {
                maj_status = gss_display_status(&min_status,
                                                gssstat,
                                                GSS_C_GSS_CODE,
                                                GSS_C_NO_OID,
                                                &message_context,
                                                &status_string);
                if (maj_status == GSS_S_COMPLETE) {
                    fprintf(stderr, "MAJ: %.*s\n",
                            (int) status_string.length,
                            (char *)status_string.value);
                    gss_release_buffer(&min_status, &status_string);
                } else {
                    fprintf(stderr,
                            "MAJ? gss_display_status returns 0x%lx?!\n",
                            (unsigned long) maj_status);
                    message_context = 0;
                }
            } while (message_context != 0);
            do {
                maj_status = gss_display_status(&min_status,
                                                minor_stat,
                                                GSS_C_MECH_CODE,
                                                GSS_C_NO_OID,
                                                &message_context,
                                                &status_string);
                if (maj_status == GSS_S_COMPLETE) {
                    fprintf(stderr, "MIN: %.*s\n",
                            (int) status_string.length,
                            (char *)status_string.value);
                    gss_release_buffer(&min_status, &status_string);
                } else {
                    fprintf(stderr,
                            "MIN? gss_display_status returns 0x%lx?!\n",
                            (unsigned long) maj_status);
                    message_context = 0;
                }
            } while (message_context != 0);
        }
#endif
        goto error;
    }

    /*
     * Do actual creation of RPC auth handle.  Implements auth flavor
     * fallback.
     */
    rpc_auth(handle, params_in, handle->cred, gss_target);

error:
    if (gss_client)
        gss_release_name(&minor_stat, &gss_client);
    if (gss_target)
        gss_release_name(&minor_stat, &gss_target);

    /* Revert to prior gss_krb5 ccache. */
    if (ccname_orig) {
        gssstat = gss_krb5_ccache_name(&minor_stat, ccname_orig, NULL);
        if (gssstat) {
            return KADM5_GSS_ERROR;
        }
        free(ccname_orig);
    } else {
        gssstat = gss_krb5_ccache_name(&minor_stat, NULL, NULL);
        if (gssstat) {
            return KADM5_GSS_ERROR;
        }
    }

    if (handle->clnt->cl_auth == NULL) {
        return KADM5_GSS_ERROR;
    }
    return 0;
}

/* Create RPC auth handle.  Do auth flavor fallback if needed. */
static void
rpc_auth(kadm5_server_handle_t handle, kadm5_config_params *params_in,
         gss_cred_id_t gss_client_creds, gss_name_t gss_target)
{
    OM_uint32 gssstat, minor_stat;
    struct rpc_gss_sec sec;

    /* Allow unauthenticated option for testing. */
    if (params_in != NULL && (params_in->mask & KADM5_CONFIG_NO_AUTH))
        return;

    /* Use RPCSEC_GSS by default. */
    if (params_in == NULL ||
        !(params_in->mask & KADM5_CONFIG_OLD_AUTH_GSSAPI)) {
        sec.mech = (gss_OID)gss_mech_krb5;
        sec.qop = GSS_C_QOP_DEFAULT;
        sec.svc = RPCSEC_GSS_SVC_PRIVACY;
        sec.cred = gss_client_creds;
        sec.req_flags = GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG;

        handle->clnt->cl_auth = authgss_create(handle->clnt,
                                               gss_target, &sec);
        if (handle->clnt->cl_auth != NULL)
            return;
    }

    if (params_in != NULL && (params_in->mask & KADM5_CONFIG_AUTH_NOFALLBACK))
        return;

    /* Fall back to old AUTH_GSSAPI. */
    handle->clnt->cl_auth = auth_gssapi_create(handle->clnt,
                                               &gssstat,
                                               &minor_stat,
                                               gss_client_creds,
                                               gss_target,
                                               (gss_OID) gss_mech_krb5,
                                               GSS_C_MUTUAL_FLAG
                                               | GSS_C_REPLAY_FLAG,
                                               0, NULL, NULL, NULL);
}

kadm5_ret_t
kadm5_destroy(void *server_handle)
{
    OM_uint32 minor_stat;
    krb5_ccache            ccache = NULL;
    int                    code = KADM5_OK;
    kadm5_server_handle_t      handle =
        (kadm5_server_handle_t) server_handle;

    CHECK_HANDLE(server_handle);

    if (handle->destroy_cache && handle->cache_name) {
        if ((code = krb5_cc_resolve(handle->context,
                                    handle->cache_name, &ccache)) == 0)
            code = krb5_cc_destroy (handle->context, ccache);
    }
    if (handle->cache_name)
        free(handle->cache_name);
    if (handle->cred)
        (void)gss_release_cred(&minor_stat, &handle->cred);
    if (handle->clnt && handle->clnt->cl_auth)
        AUTH_DESTROY(handle->clnt->cl_auth);
    if (handle->clnt)
        clnt_destroy(handle->clnt);
    if (handle->client_socket != -1)
        close(handle->client_socket);
    if (handle->lhandle)
        free (handle->lhandle);

    kadm5_free_config_params(handle->context, &handle->params);

    handle->magic_number = 0;
    free(handle);

    return code;
}
/* not supported on client */
kadm5_ret_t kadm5_lock(void *server_handle)
{
    return EINVAL;
}

/* not supported on client */
kadm5_ret_t kadm5_unlock(void *server_handle)
{
    return EINVAL;
}

kadm5_ret_t kadm5_flush(void *server_handle)
{
    return KADM5_OK;
}

int _kadm5_check_handle(void *handle)
{
    CHECK_HANDLE(handle);
    return 0;
}

krb5_error_code kadm5_init_krb5_context (krb5_context *ctx)
{
    return krb5_init_context(ctx);
}

/*
 * Stub function for kadmin.  It was created to eliminate the dependency on
 * libkdb's ulog functions.  The srv equivalent makes the actual calls.
 */
krb5_error_code
kadm5_init_iprop(void *handle, char **db_args)
{
    return (0);
}
