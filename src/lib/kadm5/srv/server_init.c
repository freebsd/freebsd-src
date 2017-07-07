/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved.
 *
 * $Id$
 * $Source$
 */
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include "k5-int.h"
#include <com_err.h>
#include <kadm5/admin.h>
#include <krb5.h>
#include <kdb_log.h>
#include "server_internal.h"
#include "osconf.h"
#include "iprop_hdr.h"

/*
 * Function check_handle
 *
 * Purpose: Check a server handle and return a com_err code if it is
 * invalid or 0 if it is valid.
 *
 * Arguments:
 *
 *      handle          The server handle.
 */

static int check_handle(void *handle)
{
    CHECK_HANDLE(handle);
    return 0;
}

static int dup_db_args(kadm5_server_handle_t handle, char **db_args)
{
    int count  = 0;
    int ret = 0;

    for (count=0; db_args && db_args[count]; count++);
    if (count == 0) {
        handle->db_args = NULL;
        goto clean_n_exit;
    }

    handle->db_args = calloc(sizeof(char*), count+1);
    if (handle->db_args == NULL) {
        ret=ENOMEM;
        goto clean_n_exit;
    }

    for (count=0; db_args[count]; count++) {
        handle->db_args[count] = strdup(db_args[count]);
        if (handle->db_args[count] == NULL) {
            ret = ENOMEM;
            goto clean_n_exit;
        }
    }

clean_n_exit:
    if (ret && handle->db_args) {
        for (count=0; handle->db_args[count]; count++)
            free(handle->db_args[count]);

        free(handle->db_args), handle->db_args = NULL;
    }

    return ret;
}

static void free_db_args(kadm5_server_handle_t handle)
{
    int count;

    if (handle->db_args) {
        for (count=0; handle->db_args[count]; count++)
            free(handle->db_args[count]);

        free(handle->db_args), handle->db_args = NULL;
    }
}

kadm5_ret_t kadm5_init_with_password(krb5_context context, char *client_name,
                                     char *pass, char *service_name,
                                     kadm5_config_params *params,
                                     krb5_ui_4 struct_version,
                                     krb5_ui_4 api_version,
                                     char **db_args,
                                     void **server_handle)
{
    return kadm5_init(context, client_name, pass, service_name, params,
                      struct_version, api_version, db_args,
                      server_handle);
}

kadm5_ret_t kadm5_init_anonymous(krb5_context context, char *client_name,
                                 char *service_name,
                                 kadm5_config_params *params,
                                 krb5_ui_4 struct_version,
                                 krb5_ui_4 api_version,
                                 char **db_args,
                                 void **server_handle)
{
    return kadm5_init(context, client_name, NULL, service_name, params,
                      struct_version, api_version, db_args,
                      server_handle);
}

kadm5_ret_t kadm5_init_with_creds(krb5_context context,
                                  char *client_name,
                                  krb5_ccache ccache,
                                  char *service_name,
                                  kadm5_config_params *params,
                                  krb5_ui_4 struct_version,
                                  krb5_ui_4 api_version,
                                  char **db_args,
                                  void **server_handle)
{
    /*
     * A program calling init_with_creds *never* expects to prompt
     * the user.  If this is KADM5_API_VERSION_2 and MKEY_FROM_KBD is
     * non-zero, return an error.
     */
    if (params && (params->mask & KADM5_CONFIG_MKEY_FROM_KBD) &&
        params->mkey_from_kbd)
        return KADM5_BAD_SERVER_PARAMS;
    return kadm5_init(context, client_name, NULL, service_name, params,
                      struct_version, api_version, db_args,
                      server_handle);
}


kadm5_ret_t kadm5_init_with_skey(krb5_context context, char *client_name,
                                 char *keytab, char *service_name,
                                 kadm5_config_params *params,
                                 krb5_ui_4 struct_version,
                                 krb5_ui_4 api_version,
                                 char **db_args,
                                 void **server_handle)
{
    /*
     * A program calling init_with_skey *never* expects to prompt the
     * user.  If this is KADM5_API_VERSION_2 and MKEY_FROM_KBD is
     * non-zero, return an error.
     */
    if (params && (params->mask & KADM5_CONFIG_MKEY_FROM_KBD) &&
        params->mkey_from_kbd)
        return KADM5_BAD_SERVER_PARAMS;
    return kadm5_init(context, client_name, NULL, service_name, params,
                      struct_version, api_version, db_args,
                      server_handle);
}

kadm5_ret_t kadm5_init(krb5_context context, char *client_name, char *pass,
                       char *service_name,
                       kadm5_config_params *params_in,
                       krb5_ui_4 struct_version,
                       krb5_ui_4 api_version,
                       char **db_args,
                       void **server_handle)
{
    int ret;
    kadm5_server_handle_t handle;
    kadm5_config_params params_local; /* for v1 compat */

    if (! server_handle)
        return EINVAL;

    if (! client_name)
        return EINVAL;

    if (! (handle = (kadm5_server_handle_t) malloc(sizeof *handle)))
        return ENOMEM;
    memset(handle, 0, sizeof(*handle));

    ret = dup_db_args(handle, db_args);
    if (ret) {
        free(handle);
        return ret;
    }

    handle->context = context;

    initialize_ovk_error_table();
/*     initialize_adb_error_table(); */
    initialize_ovku_error_table();

    handle->magic_number = KADM5_SERVER_HANDLE_MAGIC;
    handle->struct_version = struct_version;
    handle->api_version = api_version;

    /*
     * Verify the version numbers before proceeding; we can't use
     * CHECK_HANDLE because not all fields are set yet.
     */
    GENERIC_CHECK_HANDLE(handle, KADM5_OLD_SERVER_API_VERSION,
                         KADM5_NEW_SERVER_API_VERSION);

    /*
     * Acquire relevant profile entries.  Merge values
     * in params_in with values from profile, based on
     * params_in->mask.
     */
    memset(&params_local, 0, sizeof(params_local));

#if 0 /* Now that we look at krb5.conf as well as kdc.conf, we can
         expect to see admin_server being set sometimes.  */
#define ILLEGAL_PARAMS (KADM5_CONFIG_ADMIN_SERVER)
    if (params_in && (params_in->mask & ILLEGAL_PARAMS)) {
        free_db_args(handle);
        free(handle);
        return KADM5_BAD_SERVER_PARAMS;
    }
#endif

    ret = kadm5_get_config_params(handle->context, 1, params_in,
                                  &handle->params);
    if (ret) {
        free_db_args(handle);
        free(handle);
        return(ret);
    }

#define REQUIRED_PARAMS (KADM5_CONFIG_REALM | KADM5_CONFIG_DBNAME |     \
                         KADM5_CONFIG_ENCTYPE |                         \
                         KADM5_CONFIG_FLAGS |                           \
                         KADM5_CONFIG_MAX_LIFE | KADM5_CONFIG_MAX_RLIFE | \
                         KADM5_CONFIG_EXPIRATION | KADM5_CONFIG_ENCTYPES)

#define IPROP_REQUIRED_PARAMS                   \
    (KADM5_CONFIG_IPROP_ENABLED |               \
     KADM5_CONFIG_IPROP_LOGFILE |               \
     KADM5_CONFIG_IPROP_PORT)

    if ((handle->params.mask & REQUIRED_PARAMS) != REQUIRED_PARAMS) {
        kadm5_free_config_params(handle->context, &handle->params);
        free_db_args(handle);
        free(handle);
        return KADM5_MISSING_CONF_PARAMS;
    }
    if ((handle->params.mask & KADM5_CONFIG_IPROP_ENABLED) == KADM5_CONFIG_IPROP_ENABLED
        && handle->params.iprop_enabled) {
        if ((handle->params.mask & IPROP_REQUIRED_PARAMS) != IPROP_REQUIRED_PARAMS) {
            kadm5_free_config_params(handle->context, &handle->params);
            free_db_args(handle);
            free(handle);
            return KADM5_MISSING_CONF_PARAMS;
        }
    }

    ret = krb5_set_default_realm(handle->context, handle->params.realm);
    if (ret) {
        kadm5_free_config_params(handle->context, &handle->params);
        free_db_args(handle);
        free(handle);
        return ret;
    }

    ret = krb5_db_open(handle->context, db_args,
                       KRB5_KDB_OPEN_RW | KRB5_KDB_SRV_TYPE_ADMIN);
    if (ret) {
        kadm5_free_config_params(handle->context, &handle->params);
        free_db_args(handle);
        free(handle);
        return(ret);
    }

    if ((ret = krb5_parse_name(handle->context, client_name,
                               &handle->current_caller))) {
        kadm5_free_config_params(handle->context, &handle->params);
        krb5_db_fini(handle->context);
        free_db_args(handle);
        free(handle);
        return ret;
    }

    if (! (handle->lhandle = malloc(sizeof(*handle)))) {
        kadm5_free_config_params(handle->context, &handle->params);
        krb5_db_fini(handle->context);
        free_db_args(handle);
        free(handle);
        return ENOMEM;
    }
    *handle->lhandle = *handle;
    handle->lhandle->api_version = KADM5_API_VERSION_4;
    handle->lhandle->struct_version = KADM5_STRUCT_VERSION;
    handle->lhandle->lhandle = handle->lhandle;

    /* can't check the handle until current_caller is set */
    ret = check_handle((void *) handle);
    if (ret) {
        kadm5_free_config_params(handle->context, &handle->params);
        free_db_args(handle);
        free(handle);
        return ret;
    }

    ret = kdb_init_master(handle, handle->params.realm,
                          (handle->params.mask & KADM5_CONFIG_MKEY_FROM_KBD)
                          && handle->params.mkey_from_kbd);
    if (ret) {
        kadm5_free_config_params(handle->context, &handle->params);
        krb5_db_fini(handle->context);
        free_db_args(handle);
        free(handle);
        return ret;
    }

    ret = kdb_init_hist(handle, handle->params.realm);
    if (ret) {
        kadm5_free_config_params(handle->context, &handle->params);
        krb5_db_fini(handle->context);
        free_db_args(handle);
        free(handle);
        return ret;
    }

    ret = k5_kadm5_hook_load(context,&handle->hook_handles);
    if (ret) {
        kadm5_free_config_params(handle->context, &handle->params);
        krb5_db_fini(handle->context);
        krb5_free_principal(handle->context, handle->current_caller);
        free_db_args(handle);
        free(handle);
        return ret;
    }

    ret = init_pwqual(handle);
    if (ret) {
        kadm5_free_config_params(handle->context, &handle->params);
        k5_kadm5_hook_free_handles(context, handle->hook_handles);
        krb5_db_fini(handle->context);
        krb5_free_principal(handle->context, handle->current_caller);
        free_db_args(handle);
        free(handle);
        return ret;
    }

    *server_handle = (void *) handle;

    return KADM5_OK;
}

kadm5_ret_t kadm5_destroy(void *server_handle)
{
    kadm5_server_handle_t handle = server_handle;

    CHECK_HANDLE(server_handle);

    destroy_pwqual(handle);

    k5_kadm5_hook_free_handles(handle->context, handle->hook_handles);
    ulog_fini(handle->context);
    krb5_db_fini(handle->context);
    krb5_free_principal(handle->context, handle->current_caller);
    kadm5_free_config_params(handle->context, &handle->params);
    handle->magic_number = 0;
    free(handle->lhandle);
    free_db_args(handle);
    free(handle);

    return KADM5_OK;
}

kadm5_ret_t kadm5_lock(void *server_handle)
{
    kadm5_server_handle_t handle = server_handle;
    kadm5_ret_t ret;

    CHECK_HANDLE(server_handle);
    ret = krb5_db_lock(handle->context, KRB5_DB_LOCKMODE_EXCLUSIVE);
    if (ret)
        return ret;

    return KADM5_OK;
}

kadm5_ret_t kadm5_unlock(void *server_handle)
{
    kadm5_server_handle_t handle = server_handle;
    kadm5_ret_t ret;

    CHECK_HANDLE(server_handle);
    ret = krb5_db_unlock(handle->context);
    if (ret)
        return ret;

    return KADM5_OK;
}

kadm5_ret_t kadm5_flush(void *server_handle)
{
    kadm5_server_handle_t handle = server_handle;
    kadm5_ret_t ret;

    CHECK_HANDLE(server_handle);

    if ((ret = krb5_db_fini(handle->context)) ||
        (ret = krb5_db_open(handle->context, handle->db_args,
                            KRB5_KDB_OPEN_RW | KRB5_KDB_SRV_TYPE_ADMIN))) {
        (void) kadm5_destroy(server_handle);
        return ret;
    }
    return KADM5_OK;
}

int _kadm5_check_handle(void *handle)
{
    CHECK_HANDLE(handle);
    return 0;
}

#include "gssapiP_krb5.h"
krb5_error_code kadm5_init_krb5_context (krb5_context *ctx)
{
    static int first_time = 1;
    if (first_time) {
        krb5_error_code err;
        err = krb5_gss_use_kdc_context();
        if (err)
            return err;
        first_time = 0;
    }
    return krb5int_init_context_kdc(ctx);
}

krb5_error_code
kadm5_init_iprop(void *handle, char **db_args)
{
    kadm5_server_handle_t iprop_h;
    krb5_error_code retval;

    iprop_h = handle;
    if (iprop_h->params.iprop_enabled) {
        ulog_set_role(iprop_h->context, IPROP_MASTER);
        retval = ulog_map(iprop_h->context, iprop_h->params.iprop_logfile,
                          iprop_h->params.iprop_ulogsize);
        if (retval)
            return (retval);
    }
    return (0);
}
