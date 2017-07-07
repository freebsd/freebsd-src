/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/ldap/libkdb_ldap/ldap_principal2.c */
/*
 * Copyright (C) 2016 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
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
#include "ldap_principal.h"
#include "princ_xdr.h"
#include "ldap_tkt_policy.h"
#include "ldap_pwd_policy.h"
#include "ldap_err.h"
#include <kadm5/admin.h>
#include <time.h>

extern char* principal_attributes[];
extern char* max_pwd_life_attr[];

static char *
getstringtime(krb5_timestamp);

krb5_error_code
berval2tl_data(struct berval *in, krb5_tl_data **out)
{
    *out = (krb5_tl_data *) malloc (sizeof (krb5_tl_data));
    if (*out == NULL)
        return ENOMEM;

    (*out)->tl_data_length = in->bv_len - 2;
    (*out)->tl_data_contents =  (krb5_octet *) malloc
        ((*out)->tl_data_length * sizeof (krb5_octet));
    if ((*out)->tl_data_contents == NULL) {
        free (*out);
        return ENOMEM;
    }

    UNSTORE16_INT (in->bv_val, (*out)->tl_data_type);
    memcpy ((*out)->tl_data_contents, in->bv_val + 2, (*out)->tl_data_length);

    return 0;
}

/*
 * look up a principal in the directory.
 */

krb5_error_code
krb5_ldap_get_principal(krb5_context context, krb5_const_principal searchfor,
                        unsigned int flags, krb5_db_entry **entry_ptr)
{
    char                        *user=NULL, *filter=NULL, *filtuser=NULL;
    unsigned int                tree=0, ntrees=1, princlen=0;
    krb5_error_code             tempst=0, st=0;
    char                        **values=NULL, **subtree=NULL, *cname=NULL;
    LDAP                        *ld=NULL;
    LDAPMessage                 *result=NULL, *ent=NULL;
    krb5_ldap_context           *ldap_context=NULL;
    kdb5_dal_handle             *dal_handle=NULL;
    krb5_ldap_server_handle     *ldap_server_handle=NULL;
    krb5_principal              cprinc=NULL;
    krb5_boolean                found=FALSE;
    krb5_db_entry               *entry = NULL;

    *entry_ptr = NULL;

    /* Clear the global error string */
    krb5_clear_error_message(context);

    if (searchfor == NULL)
        return EINVAL;

    dal_handle = context->dal_handle;
    ldap_context = (krb5_ldap_context *) dal_handle->db_context;

    CHECK_LDAP_HANDLE(ldap_context);

    if (!is_principal_in_realm(ldap_context, searchfor)) {
        st = KRB5_KDB_NOENTRY;
        k5_setmsg(context, st, _("Principal does not belong to realm"));
        goto cleanup;
    }

    if ((st=krb5_unparse_name(context, searchfor, &user)) != 0)
        goto cleanup;

    if ((st=krb5_ldap_unparse_principal_name(user)) != 0)
        goto cleanup;

    filtuser = ldap_filter_correct(user);
    if (filtuser == NULL) {
        st = ENOMEM;
        goto cleanup;
    }

    princlen = strlen(FILTER) + strlen(filtuser) + 2 + 1;  /* 2 for closing brackets */
    if ((filter = malloc(princlen)) == NULL) {
        st = ENOMEM;
        goto cleanup;
    }
    snprintf(filter, princlen, FILTER"%s))", filtuser);

    if ((st = krb5_get_subtree_info(ldap_context, &subtree, &ntrees)) != 0)
        goto cleanup;

    GET_HANDLE();
    for (tree=0; tree < ntrees && !found; ++tree) {

        LDAP_SEARCH(subtree[tree], ldap_context->lrparams->search_scope, filter, principal_attributes);
        for (ent=ldap_first_entry(ld, result); ent != NULL && !found; ent=ldap_next_entry(ld, ent)) {

            /* get the associated directory user information */
            if ((values=ldap_get_values(ld, ent, "krbprincipalname")) != NULL) {
                int i;

                /* a wild-card in a principal name can return a list of kerberos principals.
                 * Make sure that the correct principal is returned.
                 * NOTE: a principalname k* in ldap server will return all the principals starting with a k
                 */
                for (i=0; values[i] != NULL; ++i) {
                    if (strcmp(values[i], user) == 0) {
                        found = TRUE;
                        break;
                    }
                }
                ldap_value_free(values);

                if (!found) /* no matching principal found */
                    continue;
            }

            if ((values=ldap_get_values(ld, ent, "krbcanonicalname")) != NULL) {
                if (values[0] && strcmp(values[0], user) != 0) {
                    /* We matched an alias, not the canonical name. */
                    if (flags & KRB5_KDB_FLAG_ALIAS_OK) {
                        st = krb5_ldap_parse_principal_name(values[0], &cname);
                        if (st != 0)
                            goto cleanup;
                        st = krb5_parse_name(context, cname, &cprinc);
                        if (st != 0)
                            goto cleanup;
                    } else /* No canonicalization, so don't return aliases. */
                        found = FALSE;
                }
                ldap_value_free(values);
                if (!found)
                    continue;
            }

            entry = k5alloc(sizeof(*entry), &st);
            if (entry == NULL)
                goto cleanup;
            if ((st = populate_krb5_db_entry(context, ldap_context, ld, ent,
                                             cprinc ? cprinc : searchfor,
                                             entry)) != 0)
                goto cleanup;
        }
        ldap_msgfree(result);
        result = NULL;
    } /* for (tree=0 ... */

    if (found) {
        *entry_ptr = entry;
        entry = NULL;
    } else
        st = KRB5_KDB_NOENTRY;

cleanup:
    ldap_msgfree(result);
    krb5_db_free_principal(context, entry);

    if (filter)
        free (filter);

    if (subtree) {
        for (; ntrees; --ntrees)
            if (subtree[ntrees-1])
                free (subtree[ntrees-1]);
        free (subtree);
    }

    if (ldap_server_handle)
        krb5_ldap_put_handle_to_pool(ldap_context, ldap_server_handle);

    if (user)
        free(user);

    if (filtuser)
        free(filtuser);

    if (cname)
        free(cname);

    if (cprinc)
        krb5_free_principal(context, cprinc);

    return st;
}

typedef enum{ ADD_PRINCIPAL, MODIFY_PRINCIPAL } OPERATION;
/*
 * ptype is creating confusions. Additionally the logic
 * surronding ptype is redundunt and can be achevied
 * with the help of dn and containerdn members.
 * so dropping the ptype member
 */

typedef struct _xargs_t {
    char           *dn;
    char           *linkdn;
    krb5_boolean   dn_from_kbd;
    char           *containerdn;
    char           *tktpolicydn;
}xargs_t;

static void
free_xargs(xargs_t xargs)
{
    if (xargs.dn)
        free (xargs.dn);
    if (xargs.linkdn)
        free(xargs.linkdn);
    if (xargs.containerdn)
        free (xargs.containerdn);
    if (xargs.tktpolicydn)
        free (xargs.tktpolicydn);
}

static krb5_error_code
process_db_args(krb5_context context, char **db_args, xargs_t *xargs,
                OPERATION optype)
{
    int                   i=0;
    krb5_error_code       st=0;
    char                  *arg=NULL, *arg_val=NULL;
    char                  **dptr=NULL;
    unsigned int          arg_val_len=0;

    if (db_args) {
        for (i=0; db_args[i]; ++i) {
            arg = strtok_r(db_args[i], "=", &arg_val);
            arg = (arg != NULL) ? arg : "";
            if (strcmp(arg, TKTPOLICY_ARG) == 0) {
                dptr = &xargs->tktpolicydn;
            } else {
                if (strcmp(arg, USERDN_ARG) == 0) {
                    if (optype == MODIFY_PRINCIPAL ||
                        xargs->dn != NULL || xargs->containerdn != NULL ||
                        xargs->linkdn != NULL) {
                        st = EINVAL;
                        k5_setmsg(context, st, _("%s option not supported"),
                                  arg);
                        goto cleanup;
                    }
                    dptr = &xargs->dn;
                } else if (strcmp(arg, CONTAINERDN_ARG) == 0) {
                    if (optype == MODIFY_PRINCIPAL ||
                        xargs->dn != NULL || xargs->containerdn != NULL) {
                        st = EINVAL;
                        k5_setmsg(context, st, _("%s option not supported"),
                                  arg);
                        goto cleanup;
                    }
                    dptr = &xargs->containerdn;
                } else if (strcmp(arg, LINKDN_ARG) == 0) {
                    if (xargs->dn != NULL || xargs->linkdn != NULL) {
                        st = EINVAL;
                        k5_setmsg(context, st, _("%s option not supported"),
                                  arg);
                        goto cleanup;
                    }
                    dptr = &xargs->linkdn;
                } else {
                    st = EINVAL;
                    k5_setmsg(context, st, _("unknown option: %s"), arg);
                    goto cleanup;
                }

                xargs->dn_from_kbd = TRUE;
                if (arg_val == NULL || strlen(arg_val) == 0) {
                    st = EINVAL;
                    k5_setmsg(context, st, _("%s option value missing"), arg);
                    goto cleanup;
                }
            }

            if (arg_val == NULL) {
                st = EINVAL;
                k5_setmsg(context, st, _("%s option value missing"), arg);
                goto cleanup;
            }
            arg_val_len = strlen(arg_val) + 1;

            if (strcmp(arg, TKTPOLICY_ARG) == 0) {
                if ((st = krb5_ldap_name_to_policydn (context,
                                                      arg_val,
                                                      dptr)) != 0)
                    goto cleanup;
            } else {
                *dptr = k5memdup(arg_val, arg_val_len, &st);
                if (*dptr == NULL)
                    goto cleanup;
            }
        }
    }

cleanup:
    return st;
}

krb5int_access accessor;

static krb5_error_code
asn1_encode_sequence_of_keys(krb5_key_data *key_data, krb5_int16 n_key_data,
                             krb5_int32 mkvno, krb5_data **code)
{
    krb5_error_code err;
    ldap_seqof_key_data val;

    /*
     * This should be pushed back into other library initialization
     * code.
     */
    err = kldap_ensure_initialized ();
    if (err)
        return err;

    val.key_data = key_data;
    val.n_key_data = n_key_data;
    val.mkvno = mkvno;
    val.kvno = key_data[0].key_data_kvno;

    return accessor.asn1_ldap_encode_sequence_of_keys(&val, code);
}

static krb5_error_code
asn1_decode_sequence_of_keys(krb5_data *in, ldap_seqof_key_data *out)
{
    krb5_error_code err;
    ldap_seqof_key_data *p;
    int i;

    memset(out, 0, sizeof(*out));

    /*
     * This should be pushed back into other library initialization
     * code.
     */
    err = kldap_ensure_initialized ();
    if (err)
        return err;

    err = accessor.asn1_ldap_decode_sequence_of_keys(in, &p);
    if (err)
        return err;

    /* Set kvno and key_data_ver in each key_data element. */
    for (i = 0; i < p->n_key_data; i++) {
        p->key_data[i].key_data_kvno = p->kvno;
        /* The decoder sets key_data_ver to 1 if no salt is present, but leaves
         * it at 0 if salt is present. */
        if (p->key_data[i].key_data_ver == 0)
            p->key_data[i].key_data_ver = 2;
    }

    *out = *p;
    free(p);
    return 0;
}

/*
 * Free a NULL-terminated struct berval *array[] and all its contents.
 * Does not set array to NULL after freeing it.
 */
void
free_berdata(struct berval **array)
{
    int i;

    if (array != NULL) {
        for (i = 0; array[i] != NULL; i++) {
            if (array[i]->bv_val != NULL)
                free(array[i]->bv_val);
            free(array[i]);
        }
        free(array);
    }
}

/*
 * Encode krb5_key_data into a berval struct for insertion into LDAP.
 */
static krb5_error_code
encode_keys(krb5_key_data *key_data_in, int n_key_data, krb5_kvno mkvno,
            struct berval **bval_out)
{
    krb5_error_code err = 0;
    int i;
    krb5_key_data *key_data = NULL;
    struct berval *bval = NULL;
    krb5_data *code;

    *bval_out = NULL;
    if (n_key_data <= 0) {
        err = EINVAL;
        goto cleanup;
    }

    /* Make a shallow copy of the key data so we can alter it. */
    key_data = k5calloc(n_key_data, sizeof(*key_data), &err);
    if (key_data == NULL)
        goto cleanup;
    memcpy(key_data, key_data_in, n_key_data * sizeof(*key_data));

    /* Unpatched krb5 1.11 and 1.12 cannot decode KrbKey sequences with no salt
     * field.  For compatibility, always encode a salt field. */
    for (i = 0; i < n_key_data; i++) {
        if (key_data[i].key_data_ver == 1) {
            key_data[i].key_data_ver = 2;
            key_data[i].key_data_type[1] = KRB5_KDB_SALTTYPE_NORMAL;
            key_data[i].key_data_length[1] = 0;
            key_data[i].key_data_contents[1] = NULL;
        }
    }

    bval = k5alloc(sizeof(struct berval), &err);
    if (bval == NULL)
        goto cleanup;

    err = asn1_encode_sequence_of_keys(key_data, n_key_data, mkvno, &code);
    if (err)
        goto cleanup;

    /* Steal the data pointer from code for bval and discard code. */
    bval->bv_len = code->length;
    bval->bv_val = code->data;
    free(code);

    *bval_out = bval;
    bval = NULL;

cleanup:
    free(key_data);
    free(bval);
    return err;
}

/* Decoding ASN.1 encoded key */
struct berval **
krb5_encode_krbsecretkey(krb5_key_data *key_data, int n_key_data,
                         krb5_kvno mkvno)
{
    struct berval **ret = NULL;
    int currkvno;
    int num_versions = 0;
    int i, j, last;
    krb5_error_code err = 0;

    if (n_key_data < 0)
        return NULL;

    /* Find the number of key versions */
    if (n_key_data > 0) {
        for (i = 0, num_versions = 1; i < n_key_data - 1; i++) {
            if (key_data[i].key_data_kvno != key_data[i + 1].key_data_kvno)
                num_versions++;
        }
    }

    ret = calloc(num_versions + 1, sizeof(struct berval *));
    if (ret == NULL) {
        err = ENOMEM;
        goto cleanup;
    }
    ret[num_versions] = NULL;

    /* n_key_data may be 0 if a principal is created without a key. */
    if (n_key_data == 0)
        goto cleanup;

    currkvno = key_data[0].key_data_kvno;
    for (i = 0, last = 0, j = 0; i < n_key_data; i++) {
        if (i == n_key_data - 1 || key_data[i + 1].key_data_kvno != currkvno) {
            err = encode_keys(key_data + last, (krb5_int16)i - last + 1, mkvno,
                              &ret[j]);
            if (err)
                goto cleanup;
            j++;
            last = i + 1;

            if (i < n_key_data - 1)
                currkvno = key_data[i + 1].key_data_kvno;
        }
    }

cleanup:
    if (err != 0) {
        free_berdata(ret);
        ret = NULL;
    }

    return ret;
}

/*
 * Encode a principal's key history for insertion into ldap.
 */
static struct berval **
krb5_encode_histkey(osa_princ_ent_rec *princ_ent)
{
    unsigned int i;
    krb5_error_code err = 0;
    struct berval **ret = NULL;

    if (princ_ent->old_key_len <= 0)
        return NULL;

    ret = k5calloc(princ_ent->old_key_len + 1, sizeof(struct berval *), &err);
    if (ret == NULL)
        goto cleanup;

    for (i = 0; i < princ_ent->old_key_len; i++) {
        if (princ_ent->old_keys[i].n_key_data <= 0) {
            err = EINVAL;
            goto cleanup;
        }
        err = encode_keys(princ_ent->old_keys[i].key_data,
                          princ_ent->old_keys[i].n_key_data,
                          princ_ent->admin_history_kvno, &ret[i]);
        if (err)
            goto cleanup;
    }

    ret[princ_ent->old_key_len] = NULL;

cleanup:
    if (err != 0) {
        free_berdata(ret);
        ret = NULL;
    }

    return ret;
}

static krb5_error_code
tl_data2berval (krb5_tl_data *in, struct berval **out)
{
    *out = (struct berval *) malloc (sizeof (struct berval));
    if (*out == NULL)
        return ENOMEM;

    (*out)->bv_len = in->tl_data_length + 2;
    (*out)->bv_val =  (char *) malloc ((*out)->bv_len);
    if ((*out)->bv_val == NULL) {
        free (*out);
        return ENOMEM;
    }

    STORE16_INT((*out)->bv_val, in->tl_data_type);
    memcpy ((*out)->bv_val + 2, in->tl_data_contents, in->tl_data_length);

    return 0;
}

/* Parse the "require_auth" string for auth indicators, adding them to the
 * krbPrincipalAuthInd attribute. */
static krb5_error_code
update_ldap_mod_auth_ind(krb5_context context, krb5_db_entry *entry,
                         LDAPMod ***mods)
{
    int i = 0;
    krb5_error_code ret;
    char *auth_ind = NULL;
    char *strval[10] = {};
    char *ai, *ai_save = NULL;
    int sv_num = sizeof(strval) / sizeof(*strval);

    ret = krb5_dbe_get_string(context, entry, KRB5_KDB_SK_REQUIRE_AUTH,
                              &auth_ind);
    if (ret || auth_ind == NULL)
        goto cleanup;

    ai = strtok_r(auth_ind, " ", &ai_save);
    while (ai != NULL && i < sv_num) {
        strval[i++] = ai;
        ai = strtok_r(NULL, " ", &ai_save);
    }

    ret = krb5_add_str_mem_ldap_mod(mods, "krbPrincipalAuthInd",
                                    LDAP_MOD_REPLACE, strval);

cleanup:
    krb5_dbe_free_string(context, auth_ind);
    return ret;
}

krb5_error_code
krb5_ldap_put_principal(krb5_context context, krb5_db_entry *entry,
                        char **db_args)
{
    int                         l=0, kerberos_principal_object_type=0;
    unsigned int                ntrees=0, tre=0;
    krb5_error_code             st=0, tempst=0;
    LDAP                        *ld=NULL;
    LDAPMessage                 *result=NULL, *ent=NULL;
    char                        **subtreelist = NULL;
    char                        *user=NULL, *subtree=NULL, *principal_dn=NULL;
    char                        **values=NULL, *strval[10]={NULL}, errbuf[1024];
    char                        *filtuser=NULL;
    struct berval               **bersecretkey=NULL;
    LDAPMod                     **mods=NULL;
    krb5_boolean                create_standalone=FALSE;
    krb5_boolean                krb_identity_exists=FALSE, establish_links=FALSE;
    char                        *standalone_principal_dn=NULL;
    krb5_tl_data                *tl_data=NULL;
    krb5_key_data               **keys=NULL;
    kdb5_dal_handle             *dal_handle=NULL;
    krb5_ldap_context           *ldap_context=NULL;
    krb5_ldap_server_handle     *ldap_server_handle=NULL;
    osa_princ_ent_rec           princ_ent = {0};
    xargs_t                     xargs = {0};
    char                        *polname = NULL;
    OPERATION optype;
    krb5_boolean                found_entry = FALSE;

    /* Clear the global error string */
    krb5_clear_error_message(context);

    SETUP_CONTEXT();
    if (ldap_context->lrparams == NULL || ldap_context->container_dn == NULL)
        return EINVAL;

    /* get ldap handle */
    GET_HANDLE();

    if (!is_principal_in_realm(ldap_context, entry->princ)) {
        st = EINVAL;
        k5_setmsg(context, st,
                  _("Principal does not belong to the default realm"));
        goto cleanup;
    }

    /* get the principal information to act on */
    if (((st=krb5_unparse_name(context, entry->princ, &user)) != 0) ||
        ((st=krb5_ldap_unparse_principal_name(user)) != 0))
        goto cleanup;
    filtuser = ldap_filter_correct(user);
    if (filtuser == NULL) {
        st = ENOMEM;
        goto cleanup;
    }

    /* Identity the type of operation, it can be
     * add principal or modify principal.
     * hack if the entry->mask has KRB_PRINCIPAL flag set
     * then it is a add operation
     */
    if (entry->mask & KADM5_PRINCIPAL)
        optype = ADD_PRINCIPAL;
    else
        optype = MODIFY_PRINCIPAL;

    if (((st=krb5_get_princ_type(context, entry, &kerberos_principal_object_type)) != 0) ||
        ((st=krb5_get_userdn(context, entry, &principal_dn)) != 0))
        goto cleanup;

    if ((st=process_db_args(context, db_args, &xargs, optype)) != 0)
        goto cleanup;

    if (entry->mask & KADM5_LOAD) {
        unsigned int     tree = 0;
        int              numlentries = 0;
        char             *filter = NULL;

        /*  A load operation is special, will do a mix-in (add krbprinc
         *  attrs to a non-krb object entry) if an object exists with a
         *  matching krbprincipalname attribute so try to find existing
         *  object and set principal_dn.  This assumes that the
         *  krbprincipalname attribute is unique (only one object entry has
         *  a particular krbprincipalname attribute).
         */
        if (asprintf(&filter, FILTER"%s))", filtuser) < 0) {
            filter = NULL;
            st = ENOMEM;
            goto cleanup;
        }

        /* get the current subtree list */
        if ((st = krb5_get_subtree_info(ldap_context, &subtreelist, &ntrees)) != 0)
            goto cleanup;

        found_entry = FALSE;
        /* search for entry with matching krbprincipalname attribute */
        for (tree = 0; found_entry == FALSE && tree < ntrees; ++tree) {
            if (principal_dn == NULL) {
                LDAP_SEARCH_1(subtreelist[tree], ldap_context->lrparams->search_scope, filter, principal_attributes, IGNORE_STATUS);
            } else {
                /* just look for entry with principal_dn */
                LDAP_SEARCH_1(principal_dn, LDAP_SCOPE_BASE, filter, principal_attributes, IGNORE_STATUS);
            }
            if (st == LDAP_SUCCESS) {
                numlentries = ldap_count_entries(ld, result);
                if (numlentries > 1) {
                    free(filter);
                    st = EINVAL;
                    k5_setmsg(context, st,
                              _("operation can not continue, more than one "
                                "entry with principal name \"%s\" found"),
                              user);
                    goto cleanup;
                } else if (numlentries == 1) {
                    found_entry = TRUE;
                    if (principal_dn == NULL) {
                        ent = ldap_first_entry(ld, result);
                        if (ent != NULL) {
                            /* setting principal_dn will cause that entry to be modified further down */
                            if ((principal_dn = ldap_get_dn(ld, ent)) == NULL) {
                                ldap_get_option (ld, LDAP_OPT_RESULT_CODE, &st);
                                st = set_ldap_error (context, st, 0);
                                free(filter);
                                goto cleanup;
                            }
                        }
                    }
                }
            } else if (st != LDAP_NO_SUCH_OBJECT) {
                /* could not perform search, return with failure */
                st = set_ldap_error (context, st, 0);
                free(filter);
                goto cleanup;
            }
            ldap_msgfree(result);
            result = NULL;
            /*
             * If it isn't found then assume a standalone princ entry is to
             * be created.
             */
        } /* end for (tree = 0; principal_dn == ... */

        free(filter);

        if (found_entry == FALSE && principal_dn != NULL) {
            /*
             * if principal_dn is null then there is code further down to
             * deal with setting standalone_principal_dn.  Also note that
             * this will set create_standalone true for
             * non-mix-in entries which is okay if loading from a dump.
             */
            create_standalone = TRUE;
            standalone_principal_dn = strdup(principal_dn);
            CHECK_NULL(standalone_principal_dn);
        }
    } /* end if (entry->mask & KADM5_LOAD */

    /* time to generate the DN information with the help of
     * containerdn, principalcontainerreference or
     * realmcontainerdn information
     */
    if (principal_dn == NULL && xargs.dn == NULL) { /* creation of standalone principal */
        /* get the subtree information */
        if (entry->princ->length == 2 && entry->princ->data[0].length == strlen("krbtgt") &&
            strncmp(entry->princ->data[0].data, "krbtgt", entry->princ->data[0].length) == 0) {
            /* if the principal is a inter-realm principal, always created in the realm container */
            subtree = strdup(ldap_context->lrparams->realmdn);
        } else if (xargs.containerdn) {
            if ((st=checkattributevalue(ld, xargs.containerdn, NULL, NULL, NULL)) != 0) {
                if (st == KRB5_KDB_NOENTRY || st == KRB5_KDB_CONSTRAINT_VIOLATION) {
                    int ost = st;
                    st = EINVAL;
                    k5_wrapmsg(context, ost, st, _("'%s' not found"),
                               xargs.containerdn);
                }
                goto cleanup;
            }
            subtree = strdup(xargs.containerdn);
        } else if (ldap_context->lrparams->containerref && strlen(ldap_context->lrparams->containerref) != 0) {
            /*
             * Here the subtree should be changed with
             * principalcontainerreference attribute value
             */
            subtree = strdup(ldap_context->lrparams->containerref);
        } else {
            subtree = strdup(ldap_context->lrparams->realmdn);
        }
        CHECK_NULL(subtree);

        if (asprintf(&standalone_principal_dn, "krbprincipalname=%s,%s",
                     filtuser, subtree) < 0)
            standalone_principal_dn = NULL;
        CHECK_NULL(standalone_principal_dn);
        /*
         * free subtree when you are done using the subtree
         * set the boolean create_standalone to TRUE
         */
        create_standalone = TRUE;
        free(subtree);
        subtree = NULL;
    }

    /*
     * If the DN information is presented by the user, time to
     * validate the input to ensure that the DN falls under
     * any of the subtrees
     */
    if (xargs.dn_from_kbd == TRUE) {
        /* make sure the DN falls in the subtree */
        int              dnlen=0, subtreelen=0;
        char             *dn=NULL;
        krb5_boolean     outofsubtree=TRUE;

        if (xargs.dn != NULL) {
            dn = xargs.dn;
        } else if (xargs.linkdn != NULL) {
            dn = xargs.linkdn;
        } else if (standalone_principal_dn != NULL) {
            /*
             * Even though the standalone_principal_dn is constructed
             * within this function, there is the containerdn input
             * from the user that can become part of the it.
             */
            dn = standalone_principal_dn;
        }

        /* Get the current subtree list if we haven't already done so. */
        if (subtreelist == NULL) {
            st = krb5_get_subtree_info(ldap_context, &subtreelist, &ntrees);
            if (st)
                goto cleanup;
        }

        for (tre=0; tre<ntrees; ++tre) {
            if (subtreelist[tre] == NULL || strlen(subtreelist[tre]) == 0) {
                outofsubtree = FALSE;
                break;
            } else {
                dnlen = strlen (dn);
                subtreelen = strlen(subtreelist[tre]);
                if ((dnlen >= subtreelen) && (strcasecmp((dn + dnlen - subtreelen), subtreelist[tre]) == 0)) {
                    outofsubtree = FALSE;
                    break;
                }
            }
        }

        if (outofsubtree == TRUE) {
            st = EINVAL;
            k5_setmsg(context, st, _("DN is out of the realm subtree"));
            goto cleanup;
        }

        /*
         * dn value will be set either by dn, linkdn or the standalone_principal_dn
         * In the first 2 cases, the dn should be existing and in the last case we
         * are supposed to create the ldap object. so the below should not be
         * executed for the last case.
         */

        if (standalone_principal_dn == NULL) {
            /*
             * If the ldap object is missing, this results in an error.
             */

            /*
             * Search for krbprincipalname attribute here.
             * This is to find if a kerberos identity is already present
             * on the ldap object, in which case adding a kerberos identity
             * on the ldap object should result in an error.
             */
            char  *attributes[]={"krbticketpolicyreference", "krbprincipalname", NULL};

            ldap_msgfree(result);
            result = NULL;
            LDAP_SEARCH_1(dn, LDAP_SCOPE_BASE, 0, attributes, IGNORE_STATUS);
            if (st == LDAP_SUCCESS) {
                ent = ldap_first_entry(ld, result);
                if (ent != NULL) {
                    if ((values=ldap_get_values(ld, ent, "krbticketpolicyreference")) != NULL) {
                        ldap_value_free(values);
                    }

                    if ((values=ldap_get_values(ld, ent, "krbprincipalname")) != NULL) {
                        krb_identity_exists = TRUE;
                        ldap_value_free(values);
                    }
                }
            } else {
                st = set_ldap_error(context, st, OP_SEARCH);
                goto cleanup;
            }
        }
    }

    /*
     * If xargs.dn is set then the request is to add a
     * kerberos principal on a ldap object, but if
     * there is one already on the ldap object this
     * should result in an error.
     */

    if (xargs.dn != NULL && krb_identity_exists == TRUE) {
        st = EINVAL;
        snprintf(errbuf, sizeof(errbuf),
                 _("ldap object is already kerberized"));
        k5_setmsg(context, st, "%s", errbuf);
        goto cleanup;
    }

    if (xargs.linkdn != NULL) {
        /*
         * link information can be changed using modprinc.
         * However, link information can be changed only on the
         * standalone kerberos principal objects. A standalone
         * kerberos principal object is of type krbprincipal
         * structural objectclass.
         *
         * NOTE: kerberos principals on an ldap object can't be
         * linked to other ldap objects.
         */
        if (optype == MODIFY_PRINCIPAL &&
            kerberos_principal_object_type != KDB_STANDALONE_PRINCIPAL_OBJECT) {
            st = EINVAL;
            snprintf(errbuf, sizeof(errbuf),
                     _("link information can not be set/updated as the "
                       "kerberos principal belongs to an ldap object"));
            k5_setmsg(context, st, "%s", errbuf);
            goto cleanup;
        }
        /*
         * Check the link information. If there is already a link
         * existing then this operation is not allowed.
         */
        {
            char **linkdns=NULL;
            int  j=0;

            if ((st=krb5_get_linkdn(context, entry, &linkdns)) != 0) {
                snprintf(errbuf, sizeof(errbuf),
                         _("Failed getting object references"));
                k5_setmsg(context, st, "%s", errbuf);
                goto cleanup;
            }
            if (linkdns != NULL) {
                st = EINVAL;
                snprintf(errbuf, sizeof(errbuf),
                         _("kerberos principal is already linked to a ldap "
                           "object"));
                k5_setmsg(context, st, "%s", errbuf);
                for (j=0; linkdns[j] != NULL; ++j)
                    free (linkdns[j]);
                free (linkdns);
                goto cleanup;
            }
        }

        establish_links = TRUE;
    }

    if (entry->mask & KADM5_LAST_SUCCESS) {
        memset(strval, 0, sizeof(strval));
        if ((strval[0]=getstringtime(entry->last_success)) == NULL)
            goto cleanup;
        if ((st=krb5_add_str_mem_ldap_mod(&mods, "krbLastSuccessfulAuth", LDAP_MOD_REPLACE, strval)) != 0) {
            free (strval[0]);
            goto cleanup;
        }
        free (strval[0]);
    }

    if (entry->mask & KADM5_LAST_FAILED) {
        memset(strval, 0, sizeof(strval));
        if ((strval[0]=getstringtime(entry->last_failed)) == NULL)
            goto cleanup;
        if ((st=krb5_add_str_mem_ldap_mod(&mods, "krbLastFailedAuth", LDAP_MOD_REPLACE, strval)) != 0) {
            free (strval[0]);
            goto cleanup;
        }
        free(strval[0]);
    }

    if (entry->mask & KADM5_FAIL_AUTH_COUNT) {
        krb5_kvno fail_auth_count;

        fail_auth_count = entry->fail_auth_count;
        if (entry->mask & KADM5_FAIL_AUTH_COUNT_INCREMENT)
            fail_auth_count++;

        st = krb5_add_int_mem_ldap_mod(&mods, "krbLoginFailedCount",
                                       LDAP_MOD_REPLACE,
                                       fail_auth_count);
        if (st != 0)
            goto cleanup;
    } else if (entry->mask & KADM5_FAIL_AUTH_COUNT_INCREMENT) {
        int attr_mask = 0;
        krb5_boolean has_fail_count;

        /* Check if the krbLoginFailedCount attribute exists.  (Through
         * krb5 1.8.1, it wasn't set in new entries.) */
        st = krb5_get_attributes_mask(context, entry, &attr_mask);
        if (st != 0)
            goto cleanup;
        has_fail_count = ((attr_mask & KDB_FAIL_AUTH_COUNT_ATTR) != 0);

        /*
         * If the client library and server supports RFC 4525,
         * then use it to increment by one the value of the
         * krbLoginFailedCount attribute. Otherwise, assert the
         * (provided) old value by deleting it before adding.
         */
#ifdef LDAP_MOD_INCREMENT
        if (ldap_server_handle->server_info->modify_increment &&
            has_fail_count) {
            st = krb5_add_int_mem_ldap_mod(&mods, "krbLoginFailedCount",
                                           LDAP_MOD_INCREMENT, 1);
            if (st != 0)
                goto cleanup;
        } else {
#endif /* LDAP_MOD_INCREMENT */
            if (has_fail_count) {
                st = krb5_add_int_mem_ldap_mod(&mods,
                                               "krbLoginFailedCount",
                                               LDAP_MOD_DELETE,
                                               entry->fail_auth_count);
                if (st != 0)
                    goto cleanup;
            }
            st = krb5_add_int_mem_ldap_mod(&mods, "krbLoginFailedCount",
                                           LDAP_MOD_ADD,
                                           entry->fail_auth_count + 1);
            if (st != 0)
                goto cleanup;
#ifdef LDAP_MOD_INCREMENT
        }
#endif
    } else if (optype == ADD_PRINCIPAL) {
        /* Initialize krbLoginFailedCount in new entries to help avoid a
         * race during the first failed login. */
        st = krb5_add_int_mem_ldap_mod(&mods, "krbLoginFailedCount",
                                       LDAP_MOD_ADD, 0);
    }

    if (entry->mask & KADM5_MAX_LIFE) {
        if ((st=krb5_add_int_mem_ldap_mod(&mods, "krbmaxticketlife", LDAP_MOD_REPLACE, entry->max_life)) != 0)
            goto cleanup;
    }

    if (entry->mask & KADM5_MAX_RLIFE) {
        if ((st=krb5_add_int_mem_ldap_mod(&mods, "krbmaxrenewableage", LDAP_MOD_REPLACE,
                                          entry->max_renewable_life)) != 0)
            goto cleanup;
    }

    if (entry->mask & KADM5_ATTRIBUTES) {
        if ((st=krb5_add_int_mem_ldap_mod(&mods, "krbticketflags", LDAP_MOD_REPLACE,
                                          entry->attributes)) != 0)
            goto cleanup;
    }

    if (entry->mask & KADM5_PRINCIPAL) {
        memset(strval, 0, sizeof(strval));
        strval[0] = user;
        if ((st=krb5_add_str_mem_ldap_mod(&mods, "krbprincipalname", LDAP_MOD_REPLACE, strval)) != 0)
            goto cleanup;
    }

    if (entry->mask & KADM5_PRINC_EXPIRE_TIME) {
        memset(strval, 0, sizeof(strval));
        if ((strval[0]=getstringtime(entry->expiration)) == NULL)
            goto cleanup;
        if ((st=krb5_add_str_mem_ldap_mod(&mods, "krbprincipalexpiration", LDAP_MOD_REPLACE, strval)) != 0) {
            free (strval[0]);
            goto cleanup;
        }
        free (strval[0]);
    }

    if (entry->mask & KADM5_PW_EXPIRATION) {
        memset(strval, 0, sizeof(strval));
        if ((strval[0]=getstringtime(entry->pw_expiration)) == NULL)
            goto cleanup;
        if ((st=krb5_add_str_mem_ldap_mod(&mods, "krbpasswordexpiration",
                                          LDAP_MOD_REPLACE,
                                          strval)) != 0) {
            free (strval[0]);
            goto cleanup;
        }
        free (strval[0]);
    }

    if (entry->mask & KADM5_POLICY || entry->mask & KADM5_KEY_HIST) {
        memset(&princ_ent, 0, sizeof(princ_ent));
        for (tl_data=entry->tl_data; tl_data; tl_data=tl_data->tl_data_next) {
            if (tl_data->tl_data_type == KRB5_TL_KADM_DATA) {
                if ((st = krb5_lookup_tl_kadm_data(tl_data, &princ_ent)) != 0) {
                    goto cleanup;
                }
                break;
            }
        }
    }

    if (entry->mask & KADM5_POLICY) {
        if (princ_ent.aux_attributes & KADM5_POLICY) {
            memset(strval, 0, sizeof(strval));
            if ((st = krb5_ldap_name_to_policydn (context, princ_ent.policy, &polname)) != 0)
                goto cleanup;
            strval[0] = polname;
            if ((st=krb5_add_str_mem_ldap_mod(&mods, "krbpwdpolicyreference", LDAP_MOD_REPLACE, strval)) != 0)
                goto cleanup;
        } else {
            st = EINVAL;
            k5_setmsg(context, st, "Password policy value null");
            goto cleanup;
        }
    } else if (entry->mask & KADM5_LOAD && found_entry == TRUE) {
        /*
         * a load is special in that existing entries must have attrs that
         * removed.
         */

        if ((st=krb5_add_str_mem_ldap_mod(&mods, "krbpwdpolicyreference", LDAP_MOD_REPLACE, NULL)) != 0)
            goto cleanup;
    }

    if (entry->mask & KADM5_POLICY_CLR) {
        if ((st=krb5_add_str_mem_ldap_mod(&mods, "krbpwdpolicyreference", LDAP_MOD_DELETE, NULL)) != 0)
            goto cleanup;
    }

    if (entry->mask & KADM5_KEY_HIST) {
        bersecretkey = krb5_encode_histkey(&princ_ent);
        if (bersecretkey == NULL) {
            st = ENOMEM;
            goto cleanup;
        }

        st = krb5_add_ber_mem_ldap_mod(&mods, "krbpwdhistory",
                                       LDAP_MOD_REPLACE | LDAP_MOD_BVALUES,
                                       bersecretkey);
        if (st != 0)
            goto cleanup;
        free_berdata(bersecretkey);
        bersecretkey = NULL;
    }

    if (entry->mask & KADM5_KEY_DATA || entry->mask & KADM5_KVNO) {
        krb5_kvno mkvno;

        if ((st=krb5_dbe_lookup_mkvno(context, entry, &mkvno)) != 0)
            goto cleanup;
        bersecretkey = krb5_encode_krbsecretkey (entry->key_data,
                                                 entry->n_key_data, mkvno);

        if (bersecretkey == NULL) {
            st = ENOMEM;
            goto cleanup;
        }
        /* An empty list of bervals is only accepted for modify operations,
         * not add operations. */
        if (bersecretkey[0] != NULL || !create_standalone) {
            st = krb5_add_ber_mem_ldap_mod(&mods, "krbprincipalkey",
                                           LDAP_MOD_REPLACE | LDAP_MOD_BVALUES,
                                           bersecretkey);
            if (st != 0)
                goto cleanup;
        }

        if (!(entry->mask & KADM5_PRINCIPAL)) {
            memset(strval, 0, sizeof(strval));
            if ((strval[0]=getstringtime(entry->pw_expiration)) == NULL)
                goto cleanup;
            if ((st=krb5_add_str_mem_ldap_mod(&mods,
                                              "krbpasswordexpiration",
                                              LDAP_MOD_REPLACE, strval)) != 0) {
                free (strval[0]);
                goto cleanup;
            }
            free (strval[0]);
        }

        /* Update last password change whenever a new key is set */
        {
            krb5_timestamp last_pw_changed;
            if ((st=krb5_dbe_lookup_last_pwd_change(context, entry,
                                                    &last_pw_changed)) != 0)
                goto cleanup;

            memset(strval, 0, sizeof(strval));
            if ((strval[0] = getstringtime(last_pw_changed)) == NULL)
                goto cleanup;

            if ((st=krb5_add_str_mem_ldap_mod(&mods, "krbLastPwdChange",
                                              LDAP_MOD_REPLACE, strval)) != 0) {
                free (strval[0]);
                goto cleanup;
            }
            free (strval[0]);
        }

    } /* Modify Key data ends here */

    /* Auth indicators will also be stored in krbExtraData when processing
     * tl_data. */
    st = update_ldap_mod_auth_ind(context, entry, &mods);
    if (st != 0)
        goto cleanup;

    /* Set tl_data */
    if (entry->tl_data != NULL) {
        int count = 0;
        struct berval **ber_tl_data = NULL;
        krb5_tl_data *ptr;
        krb5_timestamp unlock_time;
        for (ptr = entry->tl_data; ptr != NULL; ptr = ptr->tl_data_next) {
            if (ptr->tl_data_type == KRB5_TL_LAST_PWD_CHANGE
#ifdef SECURID
                || ptr->tl_data_type == KRB5_TL_DB_ARGS
#endif
                || ptr->tl_data_type == KRB5_TL_KADM_DATA
                || ptr->tl_data_type == KDB_TL_USER_INFO
                || ptr->tl_data_type == KRB5_TL_CONSTRAINED_DELEGATION_ACL
                || ptr->tl_data_type == KRB5_TL_LAST_ADMIN_UNLOCK)
                continue;
            count++;
        }
        if (count != 0) {
            int j;
            ber_tl_data = (struct berval **) calloc (count + 1,
                                                     sizeof (struct berval*));
            if (ber_tl_data == NULL) {
                st = ENOMEM;
                goto cleanup;
            }
            for (j = 0, ptr = entry->tl_data; ptr != NULL; ptr = ptr->tl_data_next) {
                /* Ignore tl_data that are stored in separate directory
                 * attributes */
                if (ptr->tl_data_type == KRB5_TL_LAST_PWD_CHANGE
#ifdef SECURID
                    || ptr->tl_data_type == KRB5_TL_DB_ARGS
#endif
                    || ptr->tl_data_type == KRB5_TL_KADM_DATA
                    || ptr->tl_data_type == KDB_TL_USER_INFO
                    || ptr->tl_data_type == KRB5_TL_CONSTRAINED_DELEGATION_ACL
                    || ptr->tl_data_type == KRB5_TL_LAST_ADMIN_UNLOCK)
                    continue;
                if ((st = tl_data2berval (ptr, &ber_tl_data[j])) != 0)
                    break;
                j++;
            }
            if (st == 0) {
                ber_tl_data[count] = NULL;
                st=krb5_add_ber_mem_ldap_mod(&mods, "krbExtraData",
                                             LDAP_MOD_REPLACE |
                                             LDAP_MOD_BVALUES, ber_tl_data);
            }
            free_berdata(ber_tl_data);
            if (st != 0)
                goto cleanup;
        }
        if ((st=krb5_dbe_lookup_last_admin_unlock(context, entry,
                                                  &unlock_time)) != 0)
            goto cleanup;
        if (unlock_time != 0) {
            /* Update last admin unlock */
            memset(strval, 0, sizeof(strval));
            if ((strval[0] = getstringtime(unlock_time)) == NULL)
                goto cleanup;

            if ((st=krb5_add_str_mem_ldap_mod(&mods, "krbLastAdminUnlock",
                                              LDAP_MOD_REPLACE, strval)) != 0) {
                free (strval[0]);
                goto cleanup;
            }
            free (strval[0]);
        }
    }

    /* Directory specific attribute */
    if (xargs.tktpolicydn != NULL) {
        int tmask=0;

        if (strlen(xargs.tktpolicydn) != 0) {
            st = checkattributevalue(ld, xargs.tktpolicydn, "objectclass", policyclass, &tmask);
            CHECK_CLASS_VALIDITY(st, tmask, _("ticket policy object value: "));

            strval[0] = xargs.tktpolicydn;
            strval[1] = NULL;
            if ((st=krb5_add_str_mem_ldap_mod(&mods, "krbticketpolicyreference", LDAP_MOD_REPLACE, strval)) != 0)
                goto cleanup;

        } else {
            /* if xargs.tktpolicydn is a empty string, then delete
             * already existing krbticketpolicyreference attr */
            if ((st=krb5_add_str_mem_ldap_mod(&mods, "krbticketpolicyreference", LDAP_MOD_DELETE, NULL)) != 0)
                goto cleanup;
        }

    }

    if (establish_links == TRUE) {
        memset(strval, 0, sizeof(strval));
        strval[0] = xargs.linkdn;
        if ((st=krb5_add_str_mem_ldap_mod(&mods, "krbObjectReferences", LDAP_MOD_REPLACE, strval)) != 0)
            goto cleanup;
    }

    /*
     * in case mods is NULL then return
     * not sure but can happen in a modprinc
     * so no need to return an error
     * addprinc will at least have the principal name
     * and the keys passed in
     */
    if (mods == NULL)
        goto cleanup;

    if (create_standalone == TRUE) {
        memset(strval, 0, sizeof(strval));
        strval[0] = "krbprincipal";
        strval[1] = "krbprincipalaux";
        strval[2] = "krbTicketPolicyAux";

        if ((st=krb5_add_str_mem_ldap_mod(&mods, "objectclass", LDAP_MOD_ADD, strval)) != 0)
            goto cleanup;

        st = ldap_add_ext_s(ld, standalone_principal_dn, mods, NULL, NULL);
        if (st == LDAP_ALREADY_EXISTS && entry->mask & KADM5_LOAD) {
            /* a load operation must replace an existing entry */
            st = ldap_delete_ext_s(ld, standalone_principal_dn, NULL, NULL);
            if (st != LDAP_SUCCESS) {
                snprintf(errbuf, sizeof(errbuf),
                         _("Principal delete failed (trying to replace "
                           "entry): %s"), ldap_err2string(st));
                st = translate_ldap_error (st, OP_ADD);
                k5_setmsg(context, st, "%s", errbuf);
                goto cleanup;
            } else {
                st = ldap_add_ext_s(ld, standalone_principal_dn, mods, NULL, NULL);
            }
        }
        if (st != LDAP_SUCCESS) {
            snprintf(errbuf, sizeof(errbuf), _("Principal add failed: %s"),
                     ldap_err2string(st));
            st = translate_ldap_error (st, OP_ADD);
            k5_setmsg(context, st, "%s", errbuf);
            goto cleanup;
        }
    } else {
        /*
         * Here existing ldap object is modified and can be related
         * to any attribute, so always ensure that the ldap
         * object is extended with all the kerberos related
         * objectclasses so that there are no constraint
         * violations.
         */
        {
            char *attrvalues[] = {"krbprincipalaux", "krbTicketPolicyAux", NULL};
            int p, q, r=0, amask=0;

            if ((st=checkattributevalue(ld, (xargs.dn) ? xargs.dn : principal_dn,
                                        "objectclass", attrvalues, &amask)) != 0)
                goto cleanup;

            memset(strval, 0, sizeof(strval));
            for (p=1, q=0; p<=2; p<<=1, ++q) {
                if ((p & amask) == 0)
                    strval[r++] = attrvalues[q];
            }
            if (r != 0) {
                if ((st=krb5_add_str_mem_ldap_mod(&mods, "objectclass", LDAP_MOD_ADD, strval)) != 0)
                    goto cleanup;
            }
        }
        if (xargs.dn != NULL)
            st=ldap_modify_ext_s(ld, xargs.dn, mods, NULL, NULL);
        else
            st = ldap_modify_ext_s(ld, principal_dn, mods, NULL, NULL);

        if (st != LDAP_SUCCESS) {
            snprintf(errbuf, sizeof(errbuf), _("User modification failed: %s"),
                     ldap_err2string(st));
            st = translate_ldap_error (st, OP_MOD);
            k5_setmsg(context, st, "%s", errbuf);
            goto cleanup;
        }

        if (entry->mask & KADM5_FAIL_AUTH_COUNT_INCREMENT)
            entry->fail_auth_count++;
    }

cleanup:
    if (user)
        free(user);

    if (filtuser)
        free(filtuser);

    free_xargs(xargs);

    if (standalone_principal_dn)
        free(standalone_principal_dn);

    if (principal_dn)
        free (principal_dn);

    if (polname != NULL)
        free(polname);

    for (tre = 0; tre < ntrees; tre++)
        free(subtreelist[tre]);
    free(subtreelist);

    if (subtree)
        free (subtree);

    if (bersecretkey) {
        for (l=0; bersecretkey[l]; ++l) {
            if (bersecretkey[l]->bv_val)
                free (bersecretkey[l]->bv_val);
            free (bersecretkey[l]);
        }
        free (bersecretkey);
    }

    if (keys)
        free (keys);

    ldap_mods_free(mods, 1);
    ldap_osa_free_princ_ent(&princ_ent);
    ldap_msgfree(result);
    krb5_ldap_put_handle_to_pool(ldap_context, ldap_server_handle);
    return(st);
}

krb5_error_code
krb5_read_tkt_policy(krb5_context context, krb5_ldap_context *ldap_context,
                     krb5_db_entry *entries, char *policy)
{
    krb5_error_code             st=0;
    int                         mask=0, omask=0;
    int                         tkt_mask=(KDB_MAX_LIFE_ATTR | KDB_MAX_RLIFE_ATTR | KDB_TKT_FLAGS_ATTR);
    krb5_ldap_policy_params     *tktpoldnparam=NULL;

    if ((st=krb5_get_attributes_mask(context, entries, &mask)) != 0)
        goto cleanup;

    if ((mask & tkt_mask) == tkt_mask)
        goto cleanup;

    if (policy != NULL) {
        st = krb5_ldap_read_policy(context, policy, &tktpoldnparam, &omask);
        if (st && st != KRB5_KDB_NOENTRY) {
            k5_prependmsg(context, st, _("Error reading ticket policy"));
            goto cleanup;
        }

        st = 0; /* reset the return status */
    }

    if ((mask & KDB_MAX_LIFE_ATTR) == 0) {
        if ((omask & KDB_MAX_LIFE_ATTR) ==  KDB_MAX_LIFE_ATTR)
            entries->max_life = tktpoldnparam->maxtktlife;
        else if (ldap_context->lrparams->max_life)
            entries->max_life = ldap_context->lrparams->max_life;
    }

    if ((mask & KDB_MAX_RLIFE_ATTR) == 0) {
        if ((omask & KDB_MAX_RLIFE_ATTR) == KDB_MAX_RLIFE_ATTR)
            entries->max_renewable_life = tktpoldnparam->maxrenewlife;
        else if (ldap_context->lrparams->max_renewable_life)
            entries->max_renewable_life = ldap_context->lrparams->max_renewable_life;
    }

    if ((mask & KDB_TKT_FLAGS_ATTR) == 0) {
        if ((omask & KDB_TKT_FLAGS_ATTR) == KDB_TKT_FLAGS_ATTR)
            entries->attributes = tktpoldnparam->tktflags;
        else if (ldap_context->lrparams->tktflags)
            entries->attributes |= ldap_context->lrparams->tktflags;
    }
    krb5_ldap_free_policy(context, tktpoldnparam);

cleanup:
    return st;
}

static void
free_ldap_seqof_key_data(ldap_seqof_key_data *keysets, krb5_int16 n_keysets)
{
    int i;

    if (keysets == NULL)
        return;

    for (i = 0; i < n_keysets; i++)
        k5_free_key_data(keysets[i].n_key_data, keysets[i].key_data);
    free(keysets);
}

/*
 * Decode keys from ldap search results.
 *
 * Arguments:
 *  - bvalues
 *      The ldap search results containing the key data.
 *  - mkvno
 *      The master kvno that the keys were encrypted with.
 *  - keysets_out
 *      The decoded keys in a ldap_seqof_key_data struct.  Must be freed using
 *      free_ldap_seqof_key_data.
 *  - n_keysets_out
 *      The number of entries in keys_out.
 *  - total_keys_out
 *      An optional argument that if given will be set to the total number of
 *      keys found throughout all the entries: sum(keys_out.n_key_data)
 *      May be NULL.
 */
static krb5_error_code
decode_keys(struct berval **bvalues, ldap_seqof_key_data **keysets_out,
            krb5_int16 *n_keysets_out, krb5_int16 *total_keys_out)
{
    krb5_error_code err = 0;
    krb5_int16 n_keys, i, ki, total_keys;
    ldap_seqof_key_data *keysets = NULL;

    *keysets_out = NULL;
    *n_keysets_out = 0;
    if (total_keys_out)
        *total_keys_out = 0;

    /* Precount the number of keys. */
    for (n_keys = 0, i = 0; bvalues[i] != NULL; i++) {
        if (bvalues[i]->bv_len > 0)
            n_keys++;
    }

    keysets = k5calloc(n_keys, sizeof(ldap_seqof_key_data), &err);
    if (keysets == NULL)
        goto cleanup;
    memset(keysets, 0, n_keys * sizeof(ldap_seqof_key_data));

    for (i = 0, ki = 0, total_keys = 0; bvalues[i] != NULL; i++) {
        krb5_data in;

        if (bvalues[i]->bv_len == 0)
            continue;
        in.length = bvalues[i]->bv_len;
        in.data = bvalues[i]->bv_val;

        err = asn1_decode_sequence_of_keys(&in, &keysets[ki]);
        if (err)
            goto cleanup;

        if (total_keys_out)
            total_keys += keysets[ki].n_key_data;
        ki++;
    }

    if (total_keys_out)
        *total_keys_out = total_keys;

    *n_keysets_out = n_keys;
    *keysets_out = keysets;
    keysets = NULL;
    n_keys = 0;

cleanup:
    free_ldap_seqof_key_data(keysets, n_keys);
    return err;
}

krb5_error_code
krb5_decode_krbsecretkey(krb5_context context, krb5_db_entry *entries,
                         struct berval **bvalues, krb5_kvno *mkvno)
{
    krb5_key_data *key_data = NULL, *tmp;
    krb5_error_code err = 0;
    ldap_seqof_key_data *keysets = NULL;
    krb5_int16 i, n_keysets = 0, total_keys = 0;

    err = decode_keys(bvalues, &keysets, &n_keysets, &total_keys);
    if (err != 0) {
        k5_prependmsg(context, err,
                      _("unable to decode stored principal key data"));
        goto cleanup;
    }

    key_data = k5calloc(total_keys, sizeof(krb5_key_data), &err);
    if (key_data == NULL)
        goto cleanup;
    memset(key_data, 0, total_keys * sizeof(krb5_key_data));

    if (n_keysets > 0)
        *mkvno = keysets[0].mkvno;

    /* Transfer key data values from keysets to a flat list in entries. */
    tmp = key_data;
    for (i = 0; i < n_keysets; i++) {
        memcpy(tmp, keysets[i].key_data,
               sizeof(krb5_key_data) * keysets[i].n_key_data);
        tmp += keysets[i].n_key_data;
        keysets[i].n_key_data = 0;
    }
    entries->n_key_data = total_keys;
    entries->key_data = key_data;
    key_data = NULL;

cleanup:
    free_ldap_seqof_key_data(keysets, n_keysets);
    k5_free_key_data(total_keys, key_data);
    return err;
}

static int
compare_osa_pw_hist_ent(const void *left_in, const void *right_in)
{
    int kvno_left, kvno_right;
    osa_pw_hist_ent *left = (osa_pw_hist_ent *)left_in;
    osa_pw_hist_ent *right = (osa_pw_hist_ent *)right_in;

    kvno_left = left->n_key_data ? left->key_data[0].key_data_kvno : 0;
    kvno_right = right->n_key_data ? right->key_data[0].key_data_kvno : 0;
    return kvno_left - kvno_right;
}

/*
 * Decode the key history entries from an LDAP search.
 *
 * NOTE: the caller must free princ_ent->old_keys even on error.
 */
krb5_error_code
krb5_decode_histkey(krb5_context context, struct berval **bvalues,
                    osa_princ_ent_rec *princ_ent)
{
    krb5_error_code err = 0;
    krb5_int16 i, n_keysets = 0;
    ldap_seqof_key_data *keysets = NULL;

    err = decode_keys(bvalues, &keysets, &n_keysets, NULL);
    if (err != 0) {
        k5_prependmsg(context, err,
                      _("unable to decode stored principal pw history"));
        goto cleanup;
    }

    princ_ent->old_keys = k5calloc(n_keysets, sizeof(osa_pw_hist_ent), &err);
    if (princ_ent->old_keys == NULL)
        goto cleanup;
    princ_ent->old_key_len = n_keysets;

    if (n_keysets > 0)
        princ_ent->admin_history_kvno = keysets[0].mkvno;

    /* Transfer key data pointers from keysets to princ_ent. */
    for (i = 0; i < n_keysets; i++) {
        princ_ent->old_keys[i].n_key_data = keysets[i].n_key_data;
        princ_ent->old_keys[i].key_data = keysets[i].key_data;
        keysets[i].n_key_data = 0;
        keysets[i].key_data = NULL;
    }

    /* Sort the principal entries by kvno in ascending order. */
    qsort(princ_ent->old_keys, princ_ent->old_key_len, sizeof(osa_pw_hist_ent),
          &compare_osa_pw_hist_ent);

    princ_ent->aux_attributes |= KADM5_KEY_HIST;

    /* Set the next key to the end of the list.  The queue will be lengthened
     * if it isn't full yet; the first entry will be replaced if it is full. */
    princ_ent->old_key_next = princ_ent->old_key_len;

cleanup:
    free_ldap_seqof_key_data(keysets, n_keysets);
    return err;
}

static char *
getstringtime(krb5_timestamp epochtime)
{
    struct tm           tme;
    char                *strtime=NULL;
    time_t              posixtime = epochtime;

    strtime = calloc (50, 1);
    if (strtime == NULL)
        return NULL;

    if (gmtime_r(&posixtime, &tme) == NULL)
        return NULL;

    strftime(strtime, 50, "%Y%m%d%H%M%SZ", &tme);
    return strtime;
}
