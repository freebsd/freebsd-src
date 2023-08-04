/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/ldap/libkdb_ldap/kdb_ldap.c */
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

#include <ctype.h>
#include "kdb_ldap.h"
#include "ldap_misc.h"
#include <kdb5.h>
#include <kadm5/admin.h>

/*
 * ldap get age
 */
krb5_error_code
krb5_ldap_get_age(context, db_name, age)
    krb5_context context;
    char *db_name;
    time_t *age;
{
    time (age);
    return 0;
}

/*
 * read startup information - kerberos and realm container
 */
krb5_error_code
krb5_ldap_read_startup_information(krb5_context context)
{
    krb5_error_code      retval = 0;
    kdb5_dal_handle      *dal_handle=NULL;
    krb5_ldap_context    *ldap_context=NULL;
    int                  mask = 0;

    SETUP_CONTEXT();
    if ((retval=krb5_ldap_read_krbcontainer_dn(context, &(ldap_context->container_dn)))) {
        k5_prependmsg(context, retval, _("Unable to read Kerberos container"));
        goto cleanup;
    }

    if ((retval=krb5_ldap_read_realm_params(context, context->default_realm, &(ldap_context->lrparams), &mask))) {
        k5_prependmsg(context, retval, _("Unable to read Realm"));
        goto cleanup;
    }

    if (((mask & LDAP_REALM_MAXTICKETLIFE) == 0) || ((mask & LDAP_REALM_MAXRENEWLIFE) == 0)
        || ((mask & LDAP_REALM_KRBTICKETFLAGS) == 0)) {
        kadm5_config_params  params_in, params_out;

        memset(&params_in, 0, sizeof(params_in));
        memset(&params_out, 0, sizeof(params_out));

        retval = kadm5_get_config_params(context, 1, &params_in, &params_out);
        if (retval) {
            if ((mask & LDAP_REALM_MAXTICKETLIFE) == 0) {
                ldap_context->lrparams->max_life = 24 * 60 * 60; /* 1 day */
            }
            if ((mask & LDAP_REALM_MAXRENEWLIFE) == 0) {
                ldap_context->lrparams->max_renewable_life = 0;
            }
            if ((mask & LDAP_REALM_KRBTICKETFLAGS) == 0) {
                ldap_context->lrparams->tktflags = KRB5_KDB_DEF_FLAGS;
            }
            retval = 0;
            goto cleanup;
        }

        if ((mask & LDAP_REALM_MAXTICKETLIFE) == 0) {
            if (params_out.mask & KADM5_CONFIG_MAX_LIFE)
                ldap_context->lrparams->max_life = params_out.max_life;
        }

        if ((mask & LDAP_REALM_MAXRENEWLIFE) == 0) {
            if (params_out.mask & KADM5_CONFIG_MAX_RLIFE)
                ldap_context->lrparams->max_renewable_life = params_out.max_rlife;
        }

        if ((mask & LDAP_REALM_KRBTICKETFLAGS) == 0) {
            if (params_out.mask & KADM5_CONFIG_FLAGS)
                ldap_context->lrparams->tktflags = params_out.flags;
        }

        kadm5_free_config_params(context, &params_out);
    }

cleanup:
    return retval;
}


/* Interrogate the root DSE (zero length DN) for an attribute value assertion.
 * Return true if it is present, false if it is absent or we can't tell. */
static krb5_boolean
has_rootdse_ava(krb5_context context, const char *server_name,
                const char *attribute, const char *value)
{
    krb5_boolean result = FALSE;
    char *attrs[2], **values = NULL;
    int i, st;
    LDAP *ld = NULL;
    LDAPMessage *msg, *res = NULL;
    struct berval cred;

    attrs[0] = (char *)attribute;
    attrs[1] = NULL;

    st = ldap_initialize(&ld, server_name);
    if (st != LDAP_SUCCESS)
        goto cleanup;

    /* Bind anonymously. */
    cred.bv_val = "";
    cred.bv_len = 0;
    st = ldap_sasl_bind_s(ld, "", NULL, &cred, NULL, NULL, NULL);
    if (st != LDAP_SUCCESS)
        goto cleanup;

    st = ldap_search_ext_s(ld, "", LDAP_SCOPE_BASE, NULL, attrs, 0, NULL,
                           NULL, NULL, 0, &res);
    if (st != LDAP_SUCCESS)
        goto cleanup;

    msg = ldap_first_message(ld, res);
    if (msg == NULL)
        goto cleanup;

    values = ldap_get_values(ld, msg, attribute);
    if (values == NULL)
        goto cleanup;

    for (i = 0; values[i] != NULL; i++) {
        if (strcmp(values[i], value) == 0) {
            result = TRUE;
            goto cleanup;
        }
    }

cleanup:
    ldap_value_free(values);
    ldap_msgfree(res);
    ldap_unbind_ext_s(ld, NULL, NULL);

    return result;
}

krb5_boolean
has_modify_increment(krb5_context context, const char *server_name)
{
    return has_rootdse_ava(context, server_name, "supportedFeatures",
                           "1.3.6.1.1.14");
}

void *
krb5_ldap_alloc(krb5_context context, void *ptr, size_t size)
{
    return realloc(ptr, size);
}

void
krb5_ldap_free(krb5_context context, void *ptr)
{
    free(ptr);
}

krb5_error_code
krb5_ldap_open(krb5_context context, char *conf_section, char **db_args,
               int mode)
{
    krb5_error_code status  = 0;
    krb5_ldap_context *ldap_context=NULL;

    /* Clear the global error string */
    krb5_clear_error_message(context);

    ldap_context = k5alloc(sizeof(krb5_ldap_context), &status);
    if (ldap_context == NULL)
        goto clean_n_exit;
    context->dal_handle->db_context = ldap_context;
    ldap_context->kcontext = context;

    status = krb5_ldap_parse_db_params(context, db_args);
    if (status) {
        k5_prependmsg(context, status, _("Error processing LDAP DB params"));
        goto clean_n_exit;
    }

    status = krb5_ldap_read_server_params(context, conf_section, mode & 0x0300);
    if (status) {
        k5_prependmsg(context, status, _("Error reading LDAP server params"));
        goto clean_n_exit;
    }
    if ((status=krb5_ldap_db_init(context, ldap_context)) != 0) {
        goto clean_n_exit;
    }

    if ((status=krb5_ldap_read_startup_information(context)) != 0) {
        goto clean_n_exit;
    }

clean_n_exit:
    /* may be clearing up is not required  db_fini might do it for us, check out */
    if (status) {
        krb5_ldap_close(context);
    }
    return status;
}

#include "ldap_err.h"
int
set_ldap_error(krb5_context ctx, int st, int op)
{
    int translated_st = translate_ldap_error(st, op);
    k5_setmsg(ctx, translated_st, "%s", ldap_err2string(st));
    return translated_st;
}

extern krb5int_access accessor;
MAKE_INIT_FUNCTION(kldap_init_fn);

int
kldap_init_fn(void)
{
    /* Global (per-module) initialization.  */
    return krb5int_accessor (&accessor, KRB5INT_ACCESS_VERSION);
}

int
kldap_ensure_initialized(void)
{
    return CALL_INIT_FUNCTION (kldap_init_fn);
}

krb5_error_code
krb5_ldap_check_policy_as(krb5_context kcontext, krb5_kdc_req *request,
                          krb5_db_entry *client, krb5_db_entry *server,
                          krb5_timestamp kdc_time, const char **status,
                          krb5_pa_data ***e_data)
{
    krb5_error_code retval;

    retval = krb5_ldap_lockout_check_policy(kcontext, client, kdc_time);
    if (retval == KRB5KDC_ERR_CLIENT_REVOKED)
        *status = "LOCKED_OUT";
    return retval;
}

void
krb5_ldap_audit_as_req(krb5_context kcontext, krb5_kdc_req *request,
                       const krb5_address *local_addr,
                       const krb5_address *remote_addr, krb5_db_entry *client,
                       krb5_db_entry *server, krb5_timestamp authtime,
                       krb5_error_code error_code)
{
    (void) krb5_ldap_lockout_audit(kcontext, client, authtime, error_code);
}

krb5_error_code
krb5_ldap_check_allowed_to_delegate(krb5_context context,
                                    krb5_const_principal client,
                                    const krb5_db_entry *server,
                                    krb5_const_principal proxy)
{
    krb5_error_code code;
    krb5_tl_data *tlp;

    code = KRB5KDC_ERR_BADOPTION;

    for (tlp = server->tl_data; tlp != NULL; tlp = tlp->tl_data_next) {
        krb5_principal acl;

        if (tlp->tl_data_type != KRB5_TL_CONSTRAINED_DELEGATION_ACL)
            continue;

        if (krb5_parse_name(context, (char *)tlp->tl_data_contents, &acl) != 0)
            continue;

        if (proxy == NULL || krb5_principal_compare(context, proxy, acl)) {
            code = 0;
            krb5_free_principal(context, acl);
            break;
        }
        krb5_free_principal(context, acl);
    }

    return code;
}
