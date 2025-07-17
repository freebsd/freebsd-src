/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/ldap/libkdb_ldap/ldap_pwd_policy.c */
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
/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include "ldap_main.h"
#include "kdb_ldap.h"
#include "ldap_pwd_policy.h"
#include "ldap_err.h"

static char *password_policy_attributes[] = { "cn", "krbmaxpwdlife", "krbminpwdlife",
                                              "krbpwdmindiffchars", "krbpwdminlength",
                                              "krbpwdhistorylength", "krbpwdmaxfailure",
                                              "krbpwdfailurecountinterval",
                                              "krbpwdlockoutduration",
                                              "krbpwdattributes",
                                              "krbpwdmaxlife",
                                              "krbpwdmaxrenewablelife",
                                              "krbpwdallowedkeysalts", NULL };

/* Fill in mods with LDAP operations for the fields of policy, using the
 * modification type op.  mods must be freed by the caller on error. */
static krb5_error_code
add_policy_mods(krb5_context context, LDAPMod ***mods, osa_policy_ent_t policy,
                int op)
{
    krb5_error_code st;
    char *strval[2] = { NULL };

    st = krb5_add_int_mem_ldap_mod(mods, "krbmaxpwdlife", op,
                                   (int)policy->pw_max_life);
    if (st)
        return st;

    st = krb5_add_int_mem_ldap_mod(mods, "krbminpwdlife", op,
                                   (int)policy->pw_min_life);
    if (st)
        return st;

    st = krb5_add_int_mem_ldap_mod(mods, "krbpwdmindiffchars", op,
                                   (int)policy->pw_min_classes);
    if (st)
        return st;

    st = krb5_add_int_mem_ldap_mod(mods, "krbpwdminlength", op,
                                   (int)policy->pw_min_length);
    if (st)
        return st;

    st = krb5_add_int_mem_ldap_mod(mods, "krbpwdhistorylength", op,
                                   (int)policy->pw_history_num);
    if (st)
        return st;

    st = krb5_add_int_mem_ldap_mod(mods, "krbpwdmaxfailure", op,
                                   (int)policy->pw_max_fail);
    if (st)
        return st;

    st = krb5_add_int_mem_ldap_mod(mods, "krbpwdfailurecountinterval", op,
                                   (int)policy->pw_failcnt_interval);
    if (st)
        return st;

    st = krb5_add_int_mem_ldap_mod(mods, "krbpwdlockoutduration", op,
                                   (int)policy->pw_lockout_duration);
    if (st)
        return st;

    st = krb5_add_int_mem_ldap_mod(mods, "krbpwdattributes", op,
                                   (int)policy->attributes);
    if (st)
        return st;

    st = krb5_add_int_mem_ldap_mod(mods, "krbpwdmaxlife", op,
                                   (int)policy->max_life);
    if (st)
        return st;

    st = krb5_add_int_mem_ldap_mod(mods, "krbpwdmaxrenewablelife", op,
                                   (int)policy->max_renewable_life);
    if (st)
        return st;

    if (policy->allowed_keysalts != NULL) {
        strval[0] = policy->allowed_keysalts;
        st = krb5_add_str_mem_ldap_mod(mods, "krbpwdallowedkeysalts",
                                       op, strval);
        if (st)
            return st;
    }

    /*
     * Each policy tl-data type we add should be explicitly marshalled here.
     * Unlike principals, we do not marshal unrecognized policy tl-data.
     */

    return 0;
}

/*
 * Function to create password policy object.
 */

krb5_error_code
krb5_ldap_create_password_policy(krb5_context context, osa_policy_ent_t policy)
{
    krb5_error_code             st=0;
    LDAP                        *ld=NULL;
    LDAPMod                     **mods={NULL};
    kdb5_dal_handle             *dal_handle=NULL;
    krb5_ldap_context           *ldap_context=NULL;
    krb5_ldap_server_handle     *ldap_server_handle=NULL;
    char                        *strval[2]={NULL}, *policy_dn=NULL;

    /* Clear the global error string */
    krb5_clear_error_message(context);

    /* validate the input parameters */
    if (policy == NULL || policy->name == NULL)
        return EINVAL;

    SETUP_CONTEXT();
    GET_HANDLE();

    st = krb5_ldap_name_to_policydn (context, policy->name, &policy_dn);
    if (st != 0)
        goto cleanup;

    strval[0] = policy->name;
    if ((st=krb5_add_str_mem_ldap_mod(&mods, "cn", LDAP_MOD_ADD, strval)) != 0)
        goto cleanup;

    strval[0] = "krbPwdPolicy";
    if ((st=krb5_add_str_mem_ldap_mod(&mods, "objectclass", LDAP_MOD_ADD, strval)) != 0)
        goto cleanup;

    st = add_policy_mods(context, &mods, policy, LDAP_MOD_ADD);
    if (st)
        goto cleanup;

    /* password policy object creation */
    if ((st=ldap_add_ext_s(ld, policy_dn, mods, NULL, NULL)) != LDAP_SUCCESS) {
        st = set_ldap_error (context, st, OP_ADD);
        goto cleanup;
    }

cleanup:
    free(policy_dn);
    ldap_mods_free(mods, 1);
    krb5_ldap_put_handle_to_pool(ldap_context, ldap_server_handle);
    return(st);
}

/*
 * Function to modify password policy object.
 */

krb5_error_code
krb5_ldap_put_password_policy(krb5_context context, osa_policy_ent_t policy)
{
    char                        *policy_dn=NULL;
    krb5_error_code             st=0;
    LDAP                        *ld=NULL;
    LDAPMod                     **mods=NULL;
    kdb5_dal_handle             *dal_handle=NULL;
    krb5_ldap_context           *ldap_context=NULL;
    krb5_ldap_server_handle     *ldap_server_handle=NULL;

    /* Clear the global error string */
    krb5_clear_error_message(context);

    /* validate the input parameters */
    if (policy == NULL || policy->name == NULL)
        return EINVAL;

    SETUP_CONTEXT();
    GET_HANDLE();

    st = krb5_ldap_name_to_policydn (context, policy->name, &policy_dn);
    if (st != 0)
        goto cleanup;

    st = add_policy_mods(context, &mods, policy, LDAP_MOD_REPLACE);
    if (st)
        goto cleanup;

    /* modify the password policy object. */
    /*
     * This will fail if the 'policy_dn' is anywhere other than under the realm
     * container. This is correct behaviour. 'kdb5_ldap_util' will support
     * management of only such policy objects.
     */
    if ((st=ldap_modify_ext_s(ld, policy_dn, mods, NULL, NULL)) != LDAP_SUCCESS) {
        st = set_ldap_error (context, st, OP_MOD);
        goto cleanup;
    }

cleanup:
    free(policy_dn);
    ldap_mods_free(mods, 1);
    krb5_ldap_put_handle_to_pool(ldap_context, ldap_server_handle);
    return(st);
}

static void
get_ui4(LDAP *ld, LDAPMessage *ent, char *name, krb5_ui_4 *out)
{
    int val;

    krb5_ldap_get_value(ld, ent, name, &val);
    *out = val;
}

static krb5_error_code
populate_policy(krb5_context context,
                LDAP *ld,
                LDAPMessage *ent,
                char *pol_name,
                osa_policy_ent_t pol_entry)
{
    int st = 0;

    pol_entry->name = strdup(pol_name);
    CHECK_NULL(pol_entry->name);
    pol_entry->version = 1;

    get_ui4(ld, ent, "krbmaxpwdlife", &pol_entry->pw_max_life);
    get_ui4(ld, ent, "krbminpwdlife", &pol_entry->pw_min_life);
    get_ui4(ld, ent, "krbpwdmindiffchars", &pol_entry->pw_min_classes);
    get_ui4(ld, ent, "krbpwdminlength", &pol_entry->pw_min_length);
    get_ui4(ld, ent, "krbpwdhistorylength", &pol_entry->pw_history_num);
    get_ui4(ld, ent, "krbpwdmaxfailure", &pol_entry->pw_max_fail);
    get_ui4(ld, ent, "krbpwdfailurecountinterval",
            &pol_entry->pw_failcnt_interval);
    get_ui4(ld, ent, "krbpwdlockoutduration", &pol_entry->pw_lockout_duration);
    get_ui4(ld, ent, "krbpwdattributes", &pol_entry->attributes);
    get_ui4(ld, ent, "krbpwdmaxlife", &pol_entry->max_life);
    get_ui4(ld, ent, "krbpwdmaxrenewablelife", &pol_entry->max_renewable_life);

    st = krb5_ldap_get_string(ld, ent, "krbpwdallowedkeysalts",
                              &(pol_entry->allowed_keysalts), NULL);
    if (st)
        goto cleanup;
    /*
     * We don't store the policy refcnt, because principals might be maintained
     * outside of kadmin.  Instead, we will check for principal references when
     * policies are deleted.
     */
    pol_entry->policy_refcnt = 0;

cleanup:
    return st;
}

static krb5_error_code
krb5_ldap_get_password_policy_from_dn(krb5_context context, char *pol_name,
                                      char *pol_dn, osa_policy_ent_t *policy)
{
    krb5_error_code             st=0, tempst=0;
    LDAP                        *ld=NULL;
    LDAPMessage                 *result=NULL,*ent=NULL;
    kdb5_dal_handle             *dal_handle=NULL;
    krb5_ldap_context           *ldap_context=NULL;
    krb5_ldap_server_handle     *ldap_server_handle=NULL;

    /* Clear the global error string */
    krb5_clear_error_message(context);

    /* validate the input parameters */
    if (pol_dn == NULL)
        return EINVAL;

    *policy = NULL;
    SETUP_CONTEXT();
    GET_HANDLE();

    *(policy) = (osa_policy_ent_t) malloc(sizeof(osa_policy_ent_rec));
    if (*policy == NULL) {
        st = ENOMEM;
        goto cleanup;
    }
    memset(*policy, 0, sizeof(osa_policy_ent_rec));

    LDAP_SEARCH(pol_dn, LDAP_SCOPE_BASE, "(objectclass=krbPwdPolicy)", password_policy_attributes);

    ent=ldap_first_entry(ld, result);
    if (ent == NULL) {
        st = KRB5_KDB_NOENTRY;
        goto cleanup;
    }
    st = populate_policy(context, ld, ent, pol_name, *policy);

cleanup:
    ldap_msgfree(result);
    if (st != 0) {
        if (*policy != NULL) {
            krb5_db_free_policy(context, *policy);
            *policy = NULL;
        }
    }

    krb5_ldap_put_handle_to_pool(ldap_context, ldap_server_handle);
    return st;
}

/*
 * Convert 'name' into a directory DN and call
 * 'krb5_ldap_get_password_policy_from_dn'
 */
krb5_error_code
krb5_ldap_get_password_policy(krb5_context context, char *name,
                              osa_policy_ent_t *policy)
{
    krb5_error_code             st = 0;
    char                        *policy_dn = NULL;

    /* Clear the global error string */
    krb5_clear_error_message(context);

    /* validate the input parameters */
    if (name == NULL) {
        st = EINVAL;
        goto cleanup;
    }

    st = krb5_ldap_name_to_policydn(context, name, &policy_dn);
    if (st != 0)
        goto cleanup;

    st = krb5_ldap_get_password_policy_from_dn(context, name, policy_dn,
                                               policy);

cleanup:
    free(policy_dn);
    return st;
}

krb5_error_code
krb5_ldap_delete_password_policy(krb5_context context, char *policy)
{
    int                         mask = 0;
    char                        *policy_dn = NULL, *class[] = {"krbpwdpolicy", NULL};
    krb5_error_code             st=0;
    LDAP                        *ld=NULL;
    kdb5_dal_handle             *dal_handle=NULL;
    krb5_ldap_context           *ldap_context=NULL;
    krb5_ldap_server_handle     *ldap_server_handle=NULL;

    /* Clear the global error string */
    krb5_clear_error_message(context);

    /* validate the input parameters */
    if (policy == NULL)
        return EINVAL;

    SETUP_CONTEXT();
    GET_HANDLE();

    st = krb5_ldap_name_to_policydn (context, policy, &policy_dn);
    if (st != 0)
        goto cleanup;

    /* Ensure that the object is a password policy */
    if ((st=checkattributevalue(ld, policy_dn, "objectclass", class, &mask)) != 0)
        goto cleanup;

    if (mask == 0) {
        st = KRB5_KDB_NOENTRY;
        goto cleanup;
    }

    if ((st=ldap_delete_ext_s(ld, policy_dn, NULL, NULL)) != LDAP_SUCCESS) {
        st = set_ldap_error (context, st, OP_DEL);
        goto cleanup;
    }

cleanup:
    krb5_ldap_put_handle_to_pool(ldap_context, ldap_server_handle);
    free(policy_dn);

    return st;
}

krb5_error_code
krb5_ldap_iterate_password_policy(krb5_context context, char *match_expr,
                                  void (*func)(krb5_pointer, osa_policy_ent_t),
                                  krb5_pointer func_arg)
{
    osa_policy_ent_rec          *entry=NULL;
    char                        *policy=NULL;
    krb5_error_code             st=0, tempst=0;
    LDAP                        *ld=NULL;
    LDAPMessage                 *result=NULL, *ent=NULL;
    kdb5_dal_handle             *dal_handle=NULL;
    krb5_ldap_context           *ldap_context=NULL;
    krb5_ldap_server_handle     *ldap_server_handle=NULL;

    /* Clear the global error string */
    krb5_clear_error_message(context);

    SETUP_CONTEXT();
    GET_HANDLE();

    if (ldap_context->lrparams->realmdn == NULL) {
        st = EINVAL;
        goto cleanup;
    }

    LDAP_SEARCH(ldap_context->lrparams->realmdn, LDAP_SCOPE_ONELEVEL, "(objectclass=krbpwdpolicy)", password_policy_attributes);
    for (ent=ldap_first_entry(ld, result); ent != NULL; ent=ldap_next_entry(ld, ent)) {
        krb5_boolean attr_present;

        st = krb5_ldap_get_string(ld, ent, "cn", &policy, &attr_present);
        if (st != 0)
            goto cleanup;
        if (attr_present == FALSE)
            continue;

        entry = (osa_policy_ent_t) malloc(sizeof(osa_policy_ent_rec));
        CHECK_NULL(entry);
        memset(entry, 0, sizeof(osa_policy_ent_rec));
        if ((st = populate_policy(context, ld, ent, policy, entry)) != 0)
            goto cleanup;

        (*func)(func_arg, entry);
        krb5_db_free_policy(context, entry);
        entry = NULL;

        free(policy);
        policy = NULL;
    }

cleanup:
    free(entry);
    free(policy);
    ldap_msgfree(result);
    krb5_ldap_put_handle_to_pool(ldap_context, ldap_server_handle);
    return st;
}
