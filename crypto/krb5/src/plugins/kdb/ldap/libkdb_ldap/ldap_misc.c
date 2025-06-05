/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/ldap/libkdb_ldap/ldap_misc.c */
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
#include "kdb_ldap.h"
#include "ldap_misc.h"
#include "ldap_handle.h"
#include "ldap_err.h"
#include "ldap_principal.h"
#include "princ_xdr.h"
#include "ldap_pwd_policy.h"
#include <time.h>
#include <ctype.h>
#include <kadm5/admin.h>

#ifdef NEED_STRPTIME_PROTO
extern char *strptime(const char *, const char *, struct tm *);
#endif

static void remove_overlapping_subtrees(char **listin, int *subtcount,
                                        int sscope);

/* Set an extended error message about being unable to read name. */
static krb5_error_code
attr_read_error(krb5_context ctx, krb5_error_code code, const char *name)
{
    k5_setmsg(ctx, code, _("Error reading '%s' attribute: %s"), name,
              error_message(code));
    return code;
}

/* Get integer or string values from the config section, falling back to the
 * default section, then to hard-coded values. */
static krb5_error_code
prof_get_integer_def(krb5_context ctx, const char *conf_section,
                     const char *name, int dfl, krb5_ui_4 *out)
{
    krb5_error_code ret;
    int val;

    ret = profile_get_integer(ctx->profile, KDB_MODULE_SECTION, conf_section,
                              name, 0, &val);
    if (ret)
        return attr_read_error(ctx, ret, name);
    if (val != 0) {
        *out = val;
        return 0;
    }
    ret = profile_get_integer(ctx->profile, KDB_MODULE_DEF_SECTION, name, NULL,
                              dfl, &val);
    if (ret)
        return attr_read_error(ctx, ret, name);
    *out = val;
    return 0;
}

/* Get integer or string values from the config section, falling back to the
 * default section, then to hard-coded values. */
static krb5_error_code
prof_get_boolean_def(krb5_context ctx, const char *conf_section,
                     const char *name, krb5_boolean dfl, krb5_boolean *out)
{
    krb5_error_code ret;
    int val = 0;

    ret = profile_get_boolean(ctx->profile, KDB_MODULE_SECTION, conf_section,
                              name, -1, &val);
    if (ret)
        return attr_read_error(ctx, ret, name);
    if (val != -1) {
        *out = val;
        return 0;
    }
    ret = profile_get_boolean(ctx->profile, KDB_MODULE_DEF_SECTION, name, NULL,
                              dfl, &val);
    if (ret)
        return attr_read_error(ctx, ret, name);
    *out = val;
    return 0;
}

/* We don't have non-null defaults in any of our calls, so don't bother with
 * the extra argument. */
static krb5_error_code
prof_get_string_def(krb5_context ctx, const char *conf_section,
                    const char *name, char **out)
{
    krb5_error_code ret;

    ret = profile_get_string(ctx->profile, KDB_MODULE_SECTION, conf_section,
                             name, NULL, out);
    if (ret)
        return attr_read_error(ctx, ret, name);
    if (*out != NULL)
        return 0;
    ret = profile_get_string(ctx->profile, KDB_MODULE_DEF_SECTION, name, NULL,
                             NULL, out);
    if (ret)
        return attr_read_error(ctx, ret, name);
    return 0;
}

static krb5_error_code
get_db_opt(const char *input, char **opt_out, char **val_out)
{
    krb5_error_code ret;
    const char *pos;
    char *opt, *val = NULL;
    size_t len;

    *opt_out = *val_out = NULL;
    pos = strchr(input, '=');
    if (pos == NULL) {
        opt = strdup(input);
        if (opt == NULL)
            return ENOMEM;
    } else {
        len = pos - input;
        /* Ignore trailing spaces. */
        while (len > 0 && isspace((unsigned char)input[len - 1]))
            len--;
        opt = k5memdup0(input, len, &ret);
        if (opt == NULL)
            return ret;

        pos++;                  /* Move past '='. */
        while (isspace(*pos))   /* Ignore leading spaces. */
            pos++;
        if (*pos != '\0') {
            val = strdup(pos);
            if (val == NULL) {
                free(opt);
                return ENOMEM;
            }
        }
    }
    *opt_out = opt;
    *val_out = val;
    return 0;
}

static krb5_error_code
add_server_entry(krb5_context context, const char *name)
{
    krb5_ldap_context *ctx = context->dal_handle->db_context;
    krb5_ldap_server_info **sp, **list, *server;
    size_t count = 0;

    /* Allocate list space for the new entry and null terminator. */
    for (sp = ctx->server_info_list; sp != NULL && *sp != NULL; sp++)
        count++;
    list = realloc(ctx->server_info_list, (count + 2) * sizeof(*list));
    if (list == NULL)
        return ENOMEM;
    ctx->server_info_list = list;

    server = calloc(1, sizeof(krb5_ldap_server_info));
    if (server == NULL)
        return ENOMEM;
    server->server_status = NOTSET;
    server->server_name = strdup(name);
    if (server->server_name == NULL) {
        free(server);
        return ENOMEM;
    }
    list[count] = server;
    list[count + 1] = NULL;
    return 0;
}

krb5_error_code
krb5_ldap_parse_db_params(krb5_context context, char **db_args)
{
    char *opt = NULL, *val = NULL;
    krb5_error_code ret = 0;
    krb5_ldap_context *ctx = context->dal_handle->db_context;

    if (db_args == NULL)
        return 0;
    for (; *db_args != NULL; db_args++) {
        ret = get_db_opt(*db_args, &opt, &val);
        if (ret)
            goto cleanup;

        /* Check for options which don't require values. */
        if (!strcmp(opt, "temporary")) {
            /* "temporary" is passed by kdb5_util load without -update,
             * which we don't support. */
            ret = EINVAL;
            k5_setmsg(context, ret, _("KDB module requires -update argument"));
            goto cleanup;
        }

        if (val == NULL) {
            ret = EINVAL;
            k5_setmsg(context, ret, _("'%s' value missing"), opt);
            goto cleanup;
        }

        /* Check for options which do require arguments. */
        if (!strcmp(opt, "binddn")) {
            free(ctx->bind_dn);
            ctx->bind_dn = strdup(val);
            if (ctx->bind_dn == NULL) {
                ret = ENOMEM;
                goto cleanup;
            }
        } else if (!strcmp(opt, "nconns")) {
            ctx->max_server_conns = atoi(val) ? atoi(val) :
                DEFAULT_CONNS_PER_SERVER;
        } else if (!strcmp(opt, "bindpwd")) {
            free(ctx->bind_pwd);
            ctx->bind_pwd = strdup(val);
            if (ctx->bind_pwd == NULL) {
                ret = ENOMEM;
                goto cleanup;
            }
        } else if (!strcmp(opt, "sasl_mech")) {
            free(ctx->sasl_mech);
            ctx->sasl_mech = strdup(val);
            if (ctx->sasl_mech == NULL) {
                ret = ENOMEM;
                goto cleanup;
            }
        } else if (!strcmp(opt, "sasl_authcid")) {
            free(ctx->sasl_authcid);
            ctx->sasl_authcid = strdup(val);
            if (ctx->sasl_authcid == NULL) {
                ret = ENOMEM;
                goto cleanup;
            }
        } else if (!strcmp(opt, "sasl_authzid")) {
            free(ctx->sasl_authzid);
            ctx->sasl_authzid = strdup(val);
            if (ctx->sasl_authzid == NULL) {
                ret = ENOMEM;
                goto cleanup;
            }
        } else if (!strcmp(opt, "sasl_realm")) {
            free(ctx->sasl_realm);
            ctx->sasl_realm = strdup(val);
            if (ctx->sasl_realm == NULL) {
                ret = ENOMEM;
                goto cleanup;
            }
        } else if (!strcmp(opt, "host")) {
            ret = add_server_entry(context, val);
            if (ret)
                goto cleanup;
        } else if (!strcmp(opt, "debug")) {
            ctx->ldap_debug = atoi(val);
        } else {
            ret = EINVAL;
            k5_setmsg(context, ret, _("unknown option '%s'"), opt);
            goto cleanup;
        }

        free(opt);
        free(val);
        opt = val = NULL;
    }

cleanup:
    free(opt);
    free(val);
    return ret;
}

/* Pick kdc_var or kadmind_var depending on the server type. */
static inline const char *
choose_var(int srv_type, const char *kdc_var, const char *kadmind_var)
{
    return (srv_type == KRB5_KDB_SRV_TYPE_KDC) ? kdc_var : kadmind_var;
}

/*
 * This function reads the parameters from the krb5.conf file. The
 * parameters read here are DAL-LDAP specific attributes. Some of
 * these are ldap_server ....
 */
krb5_error_code
krb5_ldap_read_server_params(krb5_context context, char *conf_section,
                             int srv_type)
{
    char *servers, *save_ptr, *item;
    const char *delims = "\t\n\f\v\r ,", *name;
    krb5_error_code ret = 0;
    kdb5_dal_handle *dal_handle = context->dal_handle;
    krb5_ldap_context *ldap_context = dal_handle->db_context;

    /* copy the conf_section into ldap_context for later use */
    if (conf_section != NULL) {
        ldap_context->conf_section = strdup(conf_section);
        if (ldap_context->conf_section == NULL)
            return ENOMEM;
    }

    /* This mutex is used in the LDAP connection pool. */
    if (k5_mutex_init(&(ldap_context->hndl_lock)) != 0)
        return KRB5_KDB_SERVER_INTERNAL_ERR;

    /* Read the maximum number of LDAP connections per server. */
    if (ldap_context->max_server_conns == 0) {
        ret = prof_get_integer_def(context, conf_section,
                                   KRB5_CONF_LDAP_CONNS_PER_SERVER,
                                   DEFAULT_CONNS_PER_SERVER,
                                   &ldap_context->max_server_conns);
        if (ret)
            return ret;
    }

    if (ldap_context->max_server_conns < 2) {
        k5_setmsg(context, EINVAL,
                  _("Minimum connections required per server is 2"));
        return EINVAL;
    }

    /* Read the DN used to connect to the LDAP server. */
    if (ldap_context->bind_dn == NULL) {
        name = choose_var(srv_type, KRB5_CONF_LDAP_KDC_DN,
                          KRB5_CONF_LDAP_KADMIND_DN);
        ret = prof_get_string_def(context, conf_section, name,
                                  &ldap_context->bind_dn);
        if (ret)
            return ret;
    }

    /* Read the filename containing stashed DN passwords. */
    if (ldap_context->service_password_file == NULL) {
        ret = prof_get_string_def(context, conf_section,
                                  KRB5_CONF_LDAP_SERVICE_PASSWORD_FILE,
                                  &ldap_context->service_password_file);
        if (ret)
            return ret;
    }

    if (ldap_context->sasl_mech == NULL) {
        name = choose_var(srv_type, KRB5_CONF_LDAP_KDC_SASL_MECH,
                          KRB5_CONF_LDAP_KADMIND_SASL_MECH);
        ret = prof_get_string_def(context, conf_section, name,
                                  &ldap_context->sasl_mech);
        if (ret)
            return ret;
    }

    if (ldap_context->sasl_authcid == NULL) {
        name = choose_var(srv_type, KRB5_CONF_LDAP_KDC_SASL_AUTHCID,
                          KRB5_CONF_LDAP_KADMIND_SASL_AUTHCID);
        ret = prof_get_string_def(context, conf_section, name,
                                  &ldap_context->sasl_authcid);
        if (ret)
            return ret;
    }

    if (ldap_context->sasl_authzid == NULL) {
        name = choose_var(srv_type, KRB5_CONF_LDAP_KDC_SASL_AUTHZID,
                          KRB5_CONF_LDAP_KADMIND_SASL_AUTHZID);
        ret = prof_get_string_def(context, conf_section, name,
                                  &ldap_context->sasl_authzid);
        if (ret)
            return ret;
    }

    if (ldap_context->sasl_realm == NULL) {
        name = choose_var(srv_type, KRB5_CONF_LDAP_KDC_SASL_REALM,
                          KRB5_CONF_LDAP_KADMIND_SASL_REALM);
        ret = prof_get_string_def(context, conf_section, name,
                                  &ldap_context->sasl_realm);
        if (ret)
            return ret;
    }

    /* Read the LDAP server URL list. */
    if (ldap_context->server_info_list == NULL) {
        ret = profile_get_string(context->profile, KDB_MODULE_SECTION,
                                 conf_section, KRB5_CONF_LDAP_SERVERS, NULL,
                                 &servers);
        if (ret)
            return attr_read_error(context, ret, KRB5_CONF_LDAP_SERVERS);

        if (servers == NULL) {
            ret = add_server_entry(context, "ldapi://");
            if (ret)
                return ret;
        } else {
            item = strtok_r(servers, delims, &save_ptr);
            while (item != NULL) {
                ret = add_server_entry(context, item);
                if (ret) {
                    profile_release_string(servers);
                    return ret;
                }
                item = strtok_r(NULL, delims, &save_ptr);
            }
            profile_release_string(servers);
        }
    }

    ret = prof_get_boolean_def(context, conf_section,
                               KRB5_CONF_DISABLE_LAST_SUCCESS, FALSE,
                               &ldap_context->disable_last_success);
    if (ret)
        return ret;

    return prof_get_boolean_def(context, conf_section,
                                KRB5_CONF_DISABLE_LOCKOUT, FALSE,
                                &ldap_context->disable_lockout);
}

void
krb5_ldap_free_server_context_params(krb5_ldap_context *ctx)
{
    int i;
    krb5_ldap_server_info **list;
    krb5_ldap_server_handle *h, *next;

    if (ctx == NULL)
        return;

    list = ctx->server_info_list;
    for (i = 0; list != NULL && list[i] != NULL; i++) {
        free(list[i]->server_name);
        for (h = list[i]->ldap_server_handles; h != NULL; h = next) {
            next = h->next;
            ldap_unbind_ext_s(h->ldap_handle, NULL, NULL);
            free(h);
        }
        free(list[i]);
    }
    free(list);
    ctx->server_info_list = NULL;

    free(ctx->sasl_mech);
    free(ctx->sasl_authcid);
    free(ctx->sasl_authzid);
    free(ctx->sasl_realm);
    free(ctx->conf_section);
    free(ctx->bind_dn);
    zapfreestr(ctx->bind_pwd);
    free(ctx->service_password_file);
    ctx->conf_section = ctx->bind_dn = ctx->bind_pwd = NULL;
    ctx->service_password_file = NULL;
}

void
krb5_ldap_free_server_params(krb5_ldap_context *ctx)
{
    if (ctx == NULL)
        return;
    krb5_ldap_free_server_context_params(ctx);
    k5_mutex_destroy(&ctx->hndl_lock);
    free(ctx);
}

/* Return true if princ is in the default realm of ldap_context or is a
 * cross-realm TGS principal for that realm. */
krb5_boolean
is_principal_in_realm(krb5_ldap_context *ldap_context,
                      krb5_const_principal princ)
{
    const char *realm = ldap_context->lrparams->realm_name;

    if (princ->length == 2 &&
        data_eq_string(princ->data[0], "krbtgt") &&
        data_eq_string(princ->data[1], realm))
        return TRUE;
    return data_eq_string(princ->realm, realm);
}

/*
 * Deduce the subtree information from the context. A realm can have
 * multiple subtrees.
 * 1. the Realm container
 * 2. the actual subtrees associated with the Realm
 *
 * However, there are some conditions to be considered to deduce the
 * actual subtree/s associated with the realm.  The conditions are as
 * follows:
 * 1. If the subtree information of the Realm is [Root] or NULL (that
 *    is internal a [Root]) then the realm has only one subtree
 *    i.e [Root], i.e. whole of the tree.
 * 2. If the subtree information of the Realm is missing/absent, then the
 *    realm has only one, i.e., the Realm container.  NOTE: In all cases
 *    Realm container SHOULD be the one among the subtrees or the only
 *    one subtree.
 * 3. The subtree information of the realm is overlapping the realm
 *    container of the realm, then the realm has only one subtree and
 *    it is the subtree information associated with the realm.
 */
krb5_error_code
krb5_get_subtree_info(krb5_ldap_context *ldap_context, char ***subtreearr,
                      unsigned int *ntree)
{
    krb5_error_code ret;
    int subtreecount, count = 0, search_scope;
    char **subtree, *realm_cont_dn, *containerref;
    char **subtarr = NULL;

    containerref = ldap_context->lrparams->containerref;
    subtree = ldap_context->lrparams->subtree;
    realm_cont_dn = ldap_context->lrparams->realmdn;
    subtreecount = ldap_context->lrparams->subtreecount;
    search_scope = ldap_context->lrparams->search_scope;

    /* Leave space for realm DN, containerref, and null terminator. */
    subtarr = k5calloc(subtreecount + 3, sizeof(char *), &ret);
    if (subtarr == NULL)
        goto cleanup;

    /* Get the complete subtree list. */
    while (count < subtreecount && subtree[count] != NULL) {
        subtarr[count] = strdup(subtree[count]);
        if (subtarr[count++] == NULL) {
            ret = ENOMEM;
            goto cleanup;
        }
    }

    subtarr[count] = strdup(realm_cont_dn);
    if (subtarr[count++] == NULL) {
        ret = ENOMEM;
        goto cleanup;
    }

    if (containerref != NULL) {
        subtarr[count] = strdup(containerref);
        if (subtarr[count++] == NULL) {
            ret = ENOMEM;
            goto cleanup;
        }
    }

    remove_overlapping_subtrees(subtarr, &count, search_scope);
    *ntree = count;
    *subtreearr = subtarr;
    subtarr = NULL;
    count = 0;

cleanup:
    while (count > 0)
        free(subtarr[--count]);
    free(subtarr);
    return ret;
}

/* Reallocate tl and return a pointer to the new space, or NULL on failure. */
static unsigned char *
expand_tl_data(krb5_tl_data *tl, uint16_t len)
{
    unsigned char *newptr;

    if (len > UINT16_MAX - tl->tl_data_length)
        return NULL;
    newptr = realloc(tl->tl_data_contents, tl->tl_data_length + len);
    if (newptr == NULL)
        return NULL;
    tl->tl_data_contents = newptr;
    tl->tl_data_length += len;
    return tl->tl_data_contents + tl->tl_data_length - len;
}

/* Append a one-byte type, a two-byte length, and a value to a KDB_TL_USER_INFO
 * tl_data item.  The length is inferred from type and value. */
krb5_error_code
store_tl_data(krb5_tl_data *tl, int type, void *value)
{
    unsigned char *ptr;
    int ival;
    char *str;
    size_t len;

    tl->tl_data_type = KDB_TL_USER_INFO;
    switch (type) {
    case KDB_TL_PRINCCOUNT:
    case KDB_TL_PRINCTYPE:
    case KDB_TL_MASK:
        ival = *(int *)value;
        if (ival > UINT16_MAX)
            return EINVAL;
        ptr = expand_tl_data(tl, 5);
        if (ptr == NULL)
            return ENOMEM;
        *ptr = type;
        store_16_be(2, ptr + 1);
        store_16_be(ival, ptr + 3);
        break;

    case KDB_TL_USERDN:
    case KDB_TL_LINKDN:
        str = value;
        len = strlen(str);
        if (len > UINT16_MAX - 3)
            return ENOMEM;
        ptr = expand_tl_data(tl, 3 + len);
        if (ptr == NULL)
            return ENOMEM;
        *ptr = type;
        store_16_be(len, ptr + 1);
        memcpy(ptr + 3, str, len);
        break;

    default:
        return EINVAL;
    }
    return 0;
}

/* Scan tl for a value of the given type and return it in allocated memory.
 * For KDB_TL_LINKDN, return a list of all values found. */
static krb5_error_code
decode_tl_data(krb5_tl_data *tl, int type, void **data_out)
{
    krb5_error_code ret;
    const unsigned char *ptr, *end;
    uint16_t len;
    size_t linkcount = 0, i;
    char **dnlist = NULL, **newlist;
    int *intptr;

    *data_out = NULL;

    /* Find the first matching subfield or return ENOENT.  For KDB_TL_LINKDN,
     * keep iterating after finding a match as it may be repeated. */
    ptr = tl->tl_data_contents;
    end = ptr + tl->tl_data_length;
    for (;;) {
        if (end - ptr < 3)
            break;
        len = load_16_be(ptr + 1);
        if (len > (end - ptr) - 3)
            break;
        if (*ptr != type) {
            ptr += 3 + len;
            continue;
        }
        ptr += 3;

        switch (type) {
        case KDB_TL_PRINCCOUNT:
        case KDB_TL_PRINCTYPE:
        case KDB_TL_MASK:
            if (len != 2)
                return EINVAL;
            intptr = malloc(sizeof(int));
            if (intptr == NULL)
                return ENOMEM;
            *intptr = load_16_be(ptr);
            *data_out = intptr;
            return 0;

        case KDB_TL_USERDN:
            *data_out = k5memdup0(ptr, len, &ret);
            return ret;

        case KDB_TL_LINKDN:
            newlist = realloc(dnlist, (linkcount + 2) * sizeof(char *));
            if (newlist == NULL)
                goto oom;
            dnlist = newlist;
            dnlist[linkcount] = k5memdup0(ptr, len, &ret);
            if (dnlist[linkcount] == NULL)
                goto oom;
            dnlist[linkcount + 1] = NULL;
            linkcount++;
            break;
        }

        ptr += len;
    }

    if (type != KDB_TL_LINKDN || dnlist == NULL)
        return ENOENT;
    *data_out = dnlist;
    return 0;

oom:
    for (i = 0; i < linkcount; i++)
        free(dnlist[i]);
    free(dnlist);
    return ENOMEM;
}

/*
 * wrapper routines for decode_tl_data
 */
static krb5_error_code
get_int_from_tl_data(krb5_context context, krb5_db_entry *entry, int type,
                     int *intval)
{
    krb5_error_code ret;
    krb5_tl_data tl_data;
    void *ptr;
    int *intptr;

    *intval = 0;

    tl_data.tl_data_type = KDB_TL_USER_INFO;
    ret = krb5_dbe_lookup_tl_data(context, entry, &tl_data);
    if (ret || tl_data.tl_data_length == 0)
        return ret;

    if (decode_tl_data(&tl_data, type, &ptr) == 0) {
        intptr = ptr;
        *intval = *intptr;
        free(intptr);
    }

    return 0;
}

/*
 * Get the mask representing the attributes set on the directory
 * object (user, policy ...).
 */
krb5_error_code
krb5_get_attributes_mask(krb5_context context, krb5_db_entry *entry,
                         int *mask)
{
    return get_int_from_tl_data(context, entry, KDB_TL_MASK, mask);
}

krb5_error_code
krb5_get_princ_type(krb5_context context, krb5_db_entry *entry, int *ptype)
{
    return get_int_from_tl_data(context, entry, KDB_TL_PRINCTYPE, ptype);
}

krb5_error_code
krb5_get_princ_count(krb5_context context, krb5_db_entry *entry, int *pcount)
{
    return get_int_from_tl_data(context, entry, KDB_TL_PRINCCOUNT, pcount);
}

krb5_error_code
krb5_get_linkdn(krb5_context context, krb5_db_entry *entry, char ***link_dn)
{
    krb5_error_code ret;
    krb5_tl_data tl_data;
    void *ptr;

    *link_dn = NULL;
    tl_data.tl_data_type = KDB_TL_USER_INFO;
    ret = krb5_dbe_lookup_tl_data(context, entry, &tl_data);
    if (ret || tl_data.tl_data_length == 0)
        return ret;

    if (decode_tl_data(&tl_data, KDB_TL_LINKDN, &ptr) == 0)
        *link_dn = ptr;

    return 0;
}

static krb5_error_code
get_str_from_tl_data(krb5_context context, krb5_db_entry *entry, int type,
                     char **strval)
{
    krb5_error_code ret;
    krb5_tl_data tl_data;
    void *ptr;

    if (type != KDB_TL_USERDN)
        return EINVAL;

    tl_data.tl_data_type = KDB_TL_USER_INFO;
    ret = krb5_dbe_lookup_tl_data(context, entry, &tl_data);
    if (ret || tl_data.tl_data_length == 0)
        return ret;

    if (decode_tl_data(&tl_data, type, &ptr) == 0)
        *strval = ptr;

    return 0;
}

/*
 * Replace the relative DN component of dn with newrdn.
 */
krb5_error_code
replace_rdn(krb5_context context, const char *dn, const char *newrdn,
            char **newdn_out)
{
    krb5_error_code ret;
    LDAPDN ldn = NULL;
    LDAPRDN lrdn = NULL;
    char *next;

    *newdn_out = NULL;

    ret = ldap_str2dn(dn, &ldn, LDAP_DN_FORMAT_LDAPV3);
    if (ret != LDAP_SUCCESS || ldn[0] == NULL) {
        ret = EINVAL;
        goto cleanup;
    }

    ret = ldap_str2rdn(newrdn, &lrdn, &next, LDAP_DN_FORMAT_LDAPV3);
    if (ret != LDAP_SUCCESS) {
        ret = EINVAL;
        goto cleanup;
    }

    ldap_rdnfree(ldn[0]);
    ldn[0] = lrdn;
    lrdn = NULL;

    ret = ldap_dn2str(ldn, newdn_out, LDAP_DN_FORMAT_LDAPV3);
    if (ret != LDAP_SUCCESS)
        ret = KRB5_KDB_SERVER_INTERNAL_ERR;

cleanup:
    if (ldn != NULL)
        ldap_dnfree(ldn);
    if (lrdn != NULL)
        ldap_rdnfree(lrdn);
    return ret;
}

krb5_error_code
krb5_get_userdn(krb5_context context, krb5_db_entry *entry, char **userdn)
{
    *userdn = NULL;
    return get_str_from_tl_data(context, entry, KDB_TL_USERDN, userdn);
}

/*
 * If attribute or attrvalues is NULL, just check for the existence of dn.
 * Otherwise, read values for attribute from dn; then set the bit 1<<n in mask
 * for each attrvalues[n] which is present in the values read.
 */
krb5_error_code
checkattributevalue(LDAP *ld, char *dn, char *attribute, char **attrvalues,
                    int *mask)
{
    krb5_error_code ret;
    int one = 1, i, j;
    char **values = NULL, *attributes[2] = { NULL };
    LDAPMessage *result = NULL, *entry;

    if (strlen(dn) == 0)
        return set_ldap_error(0, LDAP_NO_SUCH_OBJECT, OP_SEARCH);

    attributes[0] = attribute;

    /* Read values for attribute from the dn, or check for its existence. */
    ret = ldap_search_ext_s(ld, dn, LDAP_SCOPE_BASE, 0, attributes, 0, NULL,
                            NULL, &timelimit, LDAP_NO_LIMIT, &result);
    if (ret != LDAP_SUCCESS) {
        ldap_msgfree(result);
        return set_ldap_error(0, ret, OP_SEARCH);
    }

    /* Don't touch *mask if we are only checking for existence. */
    if (attribute == NULL || attrvalues == NULL)
        goto done;

    *mask = 0;

    entry = ldap_first_entry(ld, result);
    if (entry == NULL)
        goto done;
    values = ldap_get_values(ld, entry, attribute);
    if (values == NULL)
        goto done;

    /* Set bits in mask for each matching value we read. */
    for (i = 0; attrvalues[i]; i++) {
        for (j = 0; values[j]; j++) {
            if (strcasecmp(attrvalues[i], values[j]) == 0) {
                *mask |= (one << i);
                break;
            }
        }
    }

done:
    ldap_msgfree(result);
    ldap_value_free(values);
    return 0;
}

static krb5_error_code
getepochtime(char *strtime, krb5_timestamp *epochtime)
{
    struct tm tme;

    memset(&tme, 0, sizeof(tme));
    if (strptime(strtime,"%Y%m%d%H%M%SZ", &tme) == NULL) {
        *epochtime = 0;
        return EINVAL;
    }
    *epochtime = krb5int_gmt_mktime(&tme);
    return 0;
}

/* Get the integer value of attribute from int.  If it is not found, return
 * ENOENT and set *val_out to 0. */
krb5_error_code
krb5_ldap_get_value(LDAP *ld, LDAPMessage *ent, char *attribute, int *val_out)
{
    char **values;

    *val_out = 0;
    values = ldap_get_values(ld, ent, attribute);
    if (values == NULL)
        return ENOENT;
    if (values[0] != NULL)
        *val_out = atoi(values[0]);
    ldap_value_free(values);
    return 0;
}

/* Return the first string value of attribute in ent. */
krb5_error_code
krb5_ldap_get_string(LDAP *ld, LDAPMessage *ent, char *attribute,
                     char **str_out, krb5_boolean *attr_present)
{
    char **values;
    krb5_error_code ret = 0;

    *str_out = NULL;
    if (attr_present != NULL)
        *attr_present = FALSE;

    values = ldap_get_values(ld, ent, attribute);
    if (values == NULL)
        return 0;
    if (values[0] != NULL) {
        if (attr_present != NULL)
            *attr_present = TRUE;
        *str_out = strdup(values[0]);
        if (*str_out == NULL)
            ret = ENOMEM;
    }
    ldap_value_free(values);
    return ret;
}

static krb5_error_code
get_time(LDAP *ld, LDAPMessage *ent, char *attribute, krb5_timestamp *time_out,
         krb5_boolean *attr_present)
{
    char **values = NULL;
    krb5_error_code ret = 0;

    *time_out = 0;
    *attr_present = FALSE;

    values = ldap_get_values(ld, ent, attribute);
    if (values == NULL)
        return 0;
    if (values[0] != NULL) {
        *attr_present = TRUE;
        ret = getepochtime(values[0], time_out);
    }
    ldap_value_free(values);
    return ret;
}

/* Add an entry to *list_inout and return it in *mod_out. */
static krb5_error_code
alloc_mod(LDAPMod ***list_inout, LDAPMod **mod_out)
{
    size_t count;
    LDAPMod **mods = *list_inout;

    *mod_out = NULL;

    for (count = 0; mods != NULL && mods[count] != NULL; count++);
    mods = realloc(mods, (count + 2) * sizeof(*mods));
    if (mods == NULL)
        return ENOMEM;
    *list_inout = mods;

    mods[count] = calloc(1, sizeof(LDAPMod));
    if (mods[count] == NULL)
        return ENOMEM;
    mods[count + 1] = NULL;
    *mod_out = mods[count];
    return 0;
}

krb5_error_code
krb5_add_str_mem_ldap_mod(LDAPMod ***list, char *attribute, int op,
                          char **values)
{
    krb5_error_code ret;
    LDAPMod *mod;
    size_t count, i;

    ret = alloc_mod(list, &mod);
    if (ret)
        return ret;

    mod->mod_type = strdup(attribute);
    if (mod->mod_type == NULL)
        return ENOMEM;
    mod->mod_op = op;

    mod->mod_values = NULL;
    if (values == NULL)
        return 0;

    for (count = 0; values[count] != NULL; count++);
    mod->mod_values = calloc(count + 1, sizeof(char *));
    if (mod->mod_values == NULL)
        return ENOMEM;

    for (i = 0; i < count; i++) {
        mod->mod_values[i] = strdup(values[i]);
        if (mod->mod_values[i] == NULL)
            return ENOMEM;
    }
    mod->mod_values[i] = NULL;
    return 0;
}

krb5_error_code
krb5_add_ber_mem_ldap_mod(LDAPMod ***list, char *attribute, int op,
                          struct berval **ber_values)
{
    krb5_error_code ret;
    LDAPMod *mod;
    size_t count, i;

    ret = alloc_mod(list, &mod);
    if (ret)
        return ret;

    mod->mod_type = strdup(attribute);
    if (mod->mod_type == NULL)
        return ENOMEM;
    mod->mod_op = op;

    for (count = 0; ber_values[count] != NULL; count++);
    mod->mod_bvalues = calloc(count + 1, sizeof(struct berval *));
    if (mod->mod_bvalues == NULL)
        return ENOMEM;

    for (i = 0; i < count; i++) {
        mod->mod_bvalues[i] = calloc(1, sizeof(struct berval));
        if (mod->mod_bvalues[i] == NULL)
            return ENOMEM;

        mod->mod_bvalues[i]->bv_len = ber_values[i]->bv_len;
        mod->mod_bvalues[i]->bv_val = k5memdup(ber_values[i]->bv_val,
                                               ber_values[i]->bv_len, &ret);
        if (mod->mod_bvalues[i]->bv_val == NULL)
            return ret;
    }
    mod->mod_bvalues[i] = NULL;
    return 0;
}

static inline char *
format_d(int val)
{
    char tmpbuf[3 * sizeof(val) + 2];

    snprintf(tmpbuf, sizeof(tmpbuf), "%d", val);
    return strdup(tmpbuf);
}

krb5_error_code
krb5_add_int_mem_ldap_mod(LDAPMod ***list, char *attribute, int op, int value)
{
    krb5_error_code ret;
    LDAPMod *mod;

    ret = alloc_mod(list, &mod);
    if (ret)
        return ret;

    mod->mod_type = strdup(attribute);
    if (mod->mod_type == NULL)
        return ENOMEM;

    mod->mod_op = op;
    mod->mod_values = calloc(2, sizeof(char *));
    if (mod->mod_values == NULL)
        return ENOMEM;
    mod->mod_values[0] = format_d(value);
    if (mod->mod_values[0] == NULL)
        return ENOMEM;
    return 0;
}

krb5_error_code
krb5_ldap_modify_ext(krb5_context context, LDAP *ld, const char *dn,
                     LDAPMod **mods, int op)
{
    int ret;

    ret = ldap_modify_ext_s(ld, dn, mods, NULL, NULL);
    return (ret == LDAP_SUCCESS) ? 0 : set_ldap_error(context, ret, op);
}

krb5_error_code
krb5_ldap_lock(krb5_context kcontext, int mode)
{
    krb5_error_code status = KRB5_PLUGIN_OP_NOTSUPP;

    k5_setmsg(kcontext, status, "LDAP %s", error_message(status));
    return status;
}

krb5_error_code
krb5_ldap_unlock(krb5_context kcontext)
{
    krb5_error_code status = KRB5_PLUGIN_OP_NOTSUPP;

    k5_setmsg(kcontext, status, "LDAP %s", error_message(status));
    return status;
}


/*
 * Get the number of times an object has been referred to in a realm.  This is
 * needed to find out if deleting the attribute will cause dangling links.
 *
 * An LDAP handle may be optionally specified to prevent race condition - there
 * are a limited number of LDAP handles.
 */
krb5_error_code
krb5_ldap_get_reference_count(krb5_context context, char *dn, char *refattr,
                              int *count, LDAP *ld)
{
    int n, st, tempst, gothandle = 0;
    unsigned int i, ntrees = 0;
    char *refcntattr[2];
    char *filter = NULL, *corrected = NULL, **subtree = NULL;
    kdb5_dal_handle *dal_handle = NULL;
    krb5_ldap_context *ldap_context = NULL;
    krb5_ldap_server_handle *ldap_server_handle = NULL;
    LDAPMessage *result = NULL;

    if (dn == NULL || refattr == NULL) {
        st = EINVAL;
        goto cleanup;
    }

    SETUP_CONTEXT();
    if (ld == NULL) {
        GET_HANDLE();
        gothandle = 1;
    }

    refcntattr[0] = refattr;
    refcntattr[1] = NULL;

    corrected = ldap_filter_correct(dn);
    if (corrected == NULL) {
        st = ENOMEM;
        goto cleanup;
    }

    if (asprintf(&filter, "%s=%s", refattr, corrected) < 0) {
        filter = NULL;
        st = ENOMEM;
        goto cleanup;
    }

    st = krb5_get_subtree_info(ldap_context, &subtree, &ntrees);
    if (st)
        goto cleanup;

    for (i = 0, *count = 0; i < ntrees; i++) {
        LDAP_SEARCH(subtree[i], LDAP_SCOPE_SUBTREE, filter, refcntattr);
        n = ldap_count_entries(ld, result);
        if (n == -1) {
            int ret, errcode = 0;
            ret = ldap_parse_result(ld, result, &errcode, NULL, NULL, NULL,
                                    NULL, 0);
            if (ret != LDAP_SUCCESS)
                errcode = ret;
            st = translate_ldap_error(errcode, OP_SEARCH);
            goto cleanup;
        }

        ldap_msgfree(result);
        result = NULL;

        *count += n;
    }

cleanup:
    free(filter);
    ldap_msgfree(result);
    for (i = 0; i < ntrees; i++)
        free(subtree[i]);
    free(subtree);
    free(corrected);
    if (gothandle)
        krb5_ldap_put_handle_to_pool(ldap_context, ldap_server_handle);
    return st;
}

/* Extract a name from policy_dn, which must be directly under the realm
 * container. */
krb5_error_code
krb5_ldap_policydn_to_name(krb5_context context, const char *policy_dn,
                           char **name_out)
{
    size_t len1, len2, plen;
    krb5_error_code ret;
    kdb5_dal_handle *dal_handle;
    krb5_ldap_context *ldap_context;
    const char *realmdn;
    char *rdn;
    LDAPDN dn;

    *name_out = NULL;
    SETUP_CONTEXT();

    realmdn = ldap_context->lrparams->realmdn;
    if (realmdn == NULL)
        return EINVAL;

    /* policyn_dn should be "cn=<policyname>,<realmdn>". */
    len1 = strlen(realmdn);
    len2 = strlen(policy_dn);
    if (len1 == 0 || len2 == 0 || len1 + 1 >= len2)
        return EINVAL;
    plen = len2 - len1 - 1;
    if (policy_dn[plen] != ',' || strcmp(realmdn, policy_dn + plen + 1) != 0)
        return EINVAL;

    rdn = k5memdup0(policy_dn, plen, &ret);
    if (rdn == NULL)
        return ret;
    ret = ldap_str2dn(rdn, &dn, LDAP_DN_FORMAT_LDAPV3 | LDAP_DN_PEDANTIC);
    free(rdn);
    if (ret)
        return EINVAL;
    if (dn[0] == NULL || dn[1] != NULL || dn[0][0]->la_attr.bv_len != 2 ||
        strncasecmp(dn[0][0]->la_attr.bv_val, "cn", 2) != 0) {
        ret = EINVAL;
    } else {
        *name_out = k5memdup0(dn[0][0]->la_value.bv_val,
                              dn[0][0]->la_value.bv_len, &ret);
    }
    ldap_dnfree(dn);
    return ret;
}

/* Compute the policy DN for the given policy name. */
krb5_error_code
krb5_ldap_name_to_policydn(krb5_context context, char *name, char **policy_dn)
{
    int st;
    char *corrected;
    kdb5_dal_handle *dal_handle;
    krb5_ldap_context *ldap_context;

    *policy_dn = NULL;

    /* Used for removing policy reference from an object */
    if (name[0] == '\0') {
        *policy_dn = strdup("");
        return (*policy_dn == NULL) ? ENOMEM : 0;
    }

    SETUP_CONTEXT();

    if (ldap_context->lrparams->realmdn == NULL)
        return EINVAL;

    corrected = ldap_filter_correct(name);
    if (corrected == NULL)
        return ENOMEM;

    st = asprintf(policy_dn, "cn=%s,%s", corrected,
                  ldap_context->lrparams->realmdn);
    free(corrected);
    if (st == -1) {
        *policy_dn = NULL;
        return ENOMEM;
    }
    return 0;
}

/* Return true if dn1 is a subtree of dn2. */
static inline krb5_boolean
is_subtree(const char *dn1, size_t len1, const char *dn2, size_t len2)
{
    return len1 > len2 && dn1[len1 - len2 - 1] == ',' &&
        strcasecmp(dn1 + (len1 - len2), dn2) == 0;
}

/* Remove overlapping and repeated subtree entries from the list of subtrees.
 * If sscope is not 2 (sub), only remove repeated entries. */
static void
remove_overlapping_subtrees(char **list, int *subtcount, int sscope)
{
    size_t ilen, jlen;
    int i, j;
    int count = *subtcount;

    for (i = 0; i < count && list[i] != NULL; i++) {
        ilen = strlen(list[i]);
        for (j = i + 1; j < count && list[j] != NULL; j++) {
            jlen = strlen(list[j]);
            /* Remove list[j] if it is identical to list[i] or a subtree of it.
             * Remove list[i] if it is a subtree of list[j]. */
            if ((ilen == jlen && strcasecmp(list[j], list[i]) == 0) ||
                (sscope == 2 && is_subtree(list[j], jlen, list[i], ilen))) {
                free(list[j]);
                list[j--] = list[count - 1];
                list[--count] = NULL;
            } else if (sscope == 2 &&
                       is_subtree(list[i], ilen, list[j], jlen)) {
                free(list[i]);
                list[i--] = list[count - 1];
                list[--count] = NULL;
                break;
            }
        }
    }
    *subtcount = count;
}

static void
free_princ_ent_contents(osa_princ_ent_t princ_ent)
{
    unsigned int i;

    for (i = 0; i < princ_ent->old_key_len; i++) {
        k5_free_key_data(princ_ent->old_keys[i].n_key_data,
                         princ_ent->old_keys[i].key_data);
        princ_ent->old_keys[i].n_key_data = 0;
        princ_ent->old_keys[i].key_data = NULL;
    }
    free(princ_ent->old_keys);
    princ_ent->old_keys = NULL;
    princ_ent->old_key_len = 0;
}

/* Get any auth indicator values from LDAP and update the "require_auth"
 * string. */
static krb5_error_code
get_ldap_auth_ind(krb5_context context, LDAP *ld, LDAPMessage *ldap_ent,
                  krb5_db_entry *entry, unsigned int *mask)
{
    krb5_error_code ret;
    int i;
    char **auth_inds = NULL, *indstr;
    struct k5buf buf = EMPTY_K5BUF;

    auth_inds = ldap_get_values(ld, ldap_ent, "krbPrincipalAuthInd");
    if (auth_inds == NULL)
        return 0;

    k5_buf_init_dynamic(&buf);

    /* Make a space-separated list of indicators. */
    for (i = 0; auth_inds[i] != NULL; i++) {
        k5_buf_add(&buf, auth_inds[i]);
        if (auth_inds[i + 1] != NULL)
            k5_buf_add(&buf, " ");
    }

    indstr = k5_buf_cstring(&buf);
    if (indstr == NULL) {
        ret = ENOMEM;
        goto cleanup;
    }

    ret = krb5_dbe_set_string(context, entry, KRB5_KDB_SK_REQUIRE_AUTH,
                              indstr);
    if (!ret)
        *mask |= KDB_AUTH_IND_ATTR;

cleanup:
    k5_buf_free(&buf);
    ldap_value_free(auth_inds);
    return ret;
}

/*
 * Fill out a krb5_db_entry princ entry struct given a LDAP message containing
 * the results of a principal search of the directory.
 */
krb5_error_code
populate_krb5_db_entry(krb5_context context, krb5_ldap_context *ldap_context,
                       LDAP *ld, LDAPMessage *ent, krb5_const_principal princ,
                       krb5_db_entry *entry)
{
    krb5_error_code ret;
    unsigned int mask = 0;
    int val, i, pcount, objtype;
    krb5_boolean attr_present;
    krb5_kvno mkvno = 0;
    krb5_timestamp lastpwdchange, unlock_time;
    char *policydn = NULL, *pwdpolicydn = NULL, *polname = NULL, *user = NULL;
    char *tktpolname = NULL, *dn = NULL, **link_references = NULL;
    char **pnvalues = NULL, **ocvalues = NULL, **a2d2 = NULL;
    struct berval **ber_key_data = NULL, **ber_tl_data = NULL;
    krb5_tl_data userinfo_tl_data = { NULL }, **endp, *tl;
    osa_princ_ent_rec princ_ent;
    char *is_login_disabled = NULL;

    memset(&princ_ent, 0, sizeof(princ_ent));

    ret = krb5_copy_principal(context, princ, &entry->princ);
    if (ret)
        goto cleanup;

    /* get the associated directory user information */
    pnvalues = ldap_get_values(ld, ent, "krbprincipalname");
    if (pnvalues != NULL) {
        ret = krb5_unparse_name(context, princ, &user);
        if (ret)
            goto cleanup;

        pcount = 0;
        for (i = 0; pnvalues[i] != NULL; i++) {
            if (strcasecmp(pnvalues[i], user) == 0) {
                pcount = ldap_count_values(pnvalues);
                break;
            }
        }

        dn = ldap_get_dn(ld, ent);
        if (dn == NULL) {
            ldap_get_option(ld, LDAP_OPT_RESULT_CODE, &ret);
            ret = set_ldap_error(context, ret, 0);
            goto cleanup;
        }

        ocvalues = ldap_get_values(ld, ent, "objectclass");
        if (ocvalues != NULL) {
            for (i = 0; ocvalues[i] != NULL; i++) {
                if (strcasecmp(ocvalues[i], "krbprincipal") == 0) {
                    objtype = KDB_STANDALONE_PRINCIPAL_OBJECT;
                    ret = store_tl_data(&userinfo_tl_data, KDB_TL_PRINCTYPE,
                                        &objtype);
                    if (ret)
                        goto cleanup;
                    break;
                }
            }
        }

        /* Add principalcount, DN and principaltype user information to
         * tl_data */
        ret = store_tl_data(&userinfo_tl_data, KDB_TL_PRINCCOUNT, &pcount);
        if (ret)
            goto cleanup;
        ret = store_tl_data(&userinfo_tl_data, KDB_TL_USERDN, dn);
        if (ret)
            goto cleanup;
    }

    ret = get_time(ld, ent, "krbLastSuccessfulAuth", &entry->last_success,
                   &attr_present);
    if (ret)
        goto cleanup;
    if (attr_present)
        mask |= KDB_LAST_SUCCESS_ATTR;

    ret = get_time(ld, ent, "krbLastFailedAuth", &entry->last_failed,
                   &attr_present);
    if (ret)
        goto cleanup;
    if (attr_present)
        mask |= KDB_LAST_FAILED_ATTR;

    if (krb5_ldap_get_value(ld, ent, "krbLoginFailedCount", &val) == 0) {
        entry->fail_auth_count = val;
        mask |= KDB_FAIL_AUTH_COUNT_ATTR;
    }
    if (krb5_ldap_get_value(ld, ent, "krbmaxticketlife", &val) == 0) {
        entry->max_life = val;
        mask |= KDB_MAX_LIFE_ATTR;
    }
    if (krb5_ldap_get_value(ld, ent, "krbmaxrenewableage", &val) == 0) {
        entry->max_renewable_life = val;
        mask |= KDB_MAX_RLIFE_ATTR;
    }
    if (krb5_ldap_get_value(ld, ent, "krbticketflags", &val) == 0) {
        entry->attributes = val;
        mask |= KDB_TKT_FLAGS_ATTR;
    }
    ret = get_time(ld, ent, "krbprincipalexpiration", &entry->expiration,
                   &attr_present);
    if (ret)
        goto cleanup;
    if (attr_present)
        mask |= KDB_PRINC_EXPIRE_TIME_ATTR;

    ret = get_time(ld, ent, "krbpasswordexpiration", &entry->pw_expiration,
                   &attr_present);
    if (ret)
        goto cleanup;
    if (attr_present)
        mask |= KDB_PWD_EXPIRE_TIME_ATTR;

    ret = krb5_ldap_get_string(ld, ent, "krbticketpolicyreference", &policydn,
                               &attr_present);
    if (ret)
        goto cleanup;
    if (attr_present) {
        mask |= KDB_POL_REF_ATTR;
        /* Ensure that the policy is inside the realm container. */
        ret = krb5_ldap_policydn_to_name(context, policydn, &tktpolname);
        if (ret)
            goto cleanup;
    }

    ret = krb5_ldap_get_string(ld, ent, "krbpwdpolicyreference", &pwdpolicydn,
                               &attr_present);
    if (ret)
        goto cleanup;
    if (attr_present) {
        mask |= KDB_PWD_POL_REF_ATTR;

        /* Ensure that the policy is inside the realm container. */
        ret = krb5_ldap_policydn_to_name(context, pwdpolicydn, &polname);
        if (ret)
            goto cleanup;
        princ_ent.policy = polname;
        princ_ent.aux_attributes |= KADM5_POLICY;
    }

    ber_key_data = ldap_get_values_len(ld, ent, "krbpwdhistory");
    if (ber_key_data != NULL) {
        mask |= KDB_PWD_HISTORY_ATTR;
        ret = krb5_decode_histkey(context, ber_key_data, &princ_ent);
        if (ret)
            goto cleanup;
        ldap_value_free_len(ber_key_data);
    }

    if (princ_ent.aux_attributes) {
        ret = krb5_update_tl_kadm_data(context, entry, &princ_ent);
        if (ret)
            goto cleanup;
    }

    ber_key_data = ldap_get_values_len(ld, ent, "krbprincipalkey");
    if (ber_key_data != NULL) {
        mask |= KDB_SECRET_KEY_ATTR;
        ret = krb5_decode_krbsecretkey(context, entry, ber_key_data, &mkvno);
        if (ret)
            goto cleanup;
        if (mkvno != 0) {
            ret = krb5_dbe_update_mkvno(context, entry, mkvno);
            if (ret)
                goto cleanup;
        }
    }

    ret = get_time(ld, ent, "krbLastPwdChange", &lastpwdchange, &attr_present);
    if (ret)
        goto cleanup;
    if (attr_present) {
        ret = krb5_dbe_update_last_pwd_change(context, entry, lastpwdchange);
        if (ret)
            goto cleanup;
        mask |= KDB_LAST_PWD_CHANGE_ATTR;
    }

    ret = get_time(ld, ent, "krbLastAdminUnlock", &unlock_time, &attr_present);
    if (ret)
        goto cleanup;
    if (attr_present) {
        ret = krb5_dbe_update_last_admin_unlock(context, entry, unlock_time);
        if (ret)
            goto cleanup;
        mask |= KDB_LAST_ADMIN_UNLOCK_ATTR;
    }

    a2d2 = ldap_get_values(ld, ent, "krbAllowedToDelegateTo");
    if (a2d2 != NULL) {
        for (endp = &entry->tl_data; *endp; endp = &(*endp)->tl_data_next);
        for (i = 0; a2d2[i] != NULL; i++) {
            tl = k5alloc(sizeof(*tl), &ret);
            if (tl == NULL)
                goto cleanup;
            tl->tl_data_type = KRB5_TL_CONSTRAINED_DELEGATION_ACL;
            tl->tl_data_length = strlen(a2d2[i]);
            tl->tl_data_contents = (unsigned char *)strdup(a2d2[i]);
            if (tl->tl_data_contents == NULL) {
                ret = ENOMEM;
                free(tl);
                goto cleanup;
            }
            tl->tl_data_next = NULL;
            *endp = tl;
            endp = &tl->tl_data_next;
        }
    }

    link_references = ldap_get_values(ld, ent, "krbobjectreferences");
    if (link_references != NULL) {
        for (i = 0; link_references[i] != NULL; i++) {
            ret = store_tl_data(&userinfo_tl_data, KDB_TL_LINKDN,
                                link_references[i]);
            if (ret)
                goto cleanup;
        }
    }

    ber_tl_data = ldap_get_values_len(ld, ent, "krbExtraData");
    if (ber_tl_data != NULL) {
        for (i = 0; ber_tl_data[i] != NULL; i++) {
            ret = berval2tl_data(ber_tl_data[i], &tl);
            if (ret)
                goto cleanup;
            ret = krb5_dbe_update_tl_data(context, entry, tl);
            free(tl->tl_data_contents);
            free(tl);
            if (ret)
                goto cleanup;
        }
        mask |= KDB_EXTRA_DATA_ATTR;
    }

    /* Auth indicators from krbPrincipalAuthInd will replace those from
     * krbExtraData. */
    ret = get_ldap_auth_ind(context, ld, ent, entry, &mask);
    if (ret)
        goto cleanup;

    /* Update the mask of attributes present on the directory object to the
     * tl_data. */
    ret = store_tl_data(&userinfo_tl_data, KDB_TL_MASK, &mask);
    if (ret)
        goto cleanup;
    ret = krb5_dbe_update_tl_data(context, entry, &userinfo_tl_data);
    if (ret)
        goto cleanup;

    /*
     * 389ds and other Netscape directory server derivatives support an
     * attribute "nsAccountLock" which functions similarly to eDirectory's
     * "loginDisabled".  When the user's account object is also a
     * krbPrincipalAux object, the kdb entry should be treated as if
     * DISALLOW_ALL_TIX has been set.
     */
    ret = krb5_ldap_get_string(ld, ent, "nsAccountLock", &is_login_disabled,
                               &attr_present);
    if (ret)
        goto cleanup;
    if (attr_present == TRUE) {
        if (strcasecmp(is_login_disabled, "TRUE") == 0)
            entry->attributes |= KRB5_KDB_DISALLOW_ALL_TIX;
        free(is_login_disabled);
    }

    ret = krb5_read_tkt_policy(context, ldap_context, entry, tktpolname);
    if (ret)
        goto cleanup;

    /* For compatibility with DB2 principals. */
    entry->len = KRB5_KDB_V1_BASE_LENGTH;

cleanup:
    ldap_memfree(dn);
    ldap_value_free_len(ber_key_data);
    ldap_value_free_len(ber_tl_data);
    ldap_value_free(pnvalues);
    ldap_value_free(ocvalues);
    ldap_value_free(link_references);
    ldap_value_free(a2d2);
    free(userinfo_tl_data.tl_data_contents);
    free(pwdpolicydn);
    free(polname);
    free(tktpolname);
    free(policydn);
    krb5_free_unparsed_name(context, user);
    free_princ_ent_contents(&princ_ent);
    return ret;
}
