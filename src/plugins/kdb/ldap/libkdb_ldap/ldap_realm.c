/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/ldap/libkdb_ldap/ldap_realm.c */
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
#include "ldap_realm.h"
#include "ldap_principal.h"
#include "ldap_pwd_policy.h"
#include "ldap_err.h"

#define END_OF_LIST -1
char  *realm_attributes[] = {"krbSearchScope","krbSubTrees", "krbPrincContainerRef",
                             "krbMaxTicketLife", "krbMaxRenewableAge",
                             "krbTicketFlags", "krbUpEnabled",
                             "krbLdapServers",
                             "krbKdcServers",  "krbAdmServers",
                             "krbPwdServers", NULL};


char  *policy_attributes[] = { "krbMaxTicketLife",
                               "krbMaxRenewableAge",
                               "krbTicketFlags",
                               NULL };



char  *policyclass[] =     { "krbTicketPolicy", NULL };
char  *kdcclass[] =        { "krbKdcService", NULL };
char  *adminclass[] =      { "krbAdmService", NULL };
char  *pwdclass[] =        { "krbPwdService", NULL };
char  *subtreeclass[] =    { "Organization", "OrganizationalUnit", "Domain", "krbContainer",
                             "krbRealmContainer", "Country", "Locality", NULL };


char  *krbContainerRefclass[] = { "krbContainerRefAux", NULL};

/*
 * list realms from eDirectory
 */

/* Return a copy of in, quoting all characters which are special in an LDAP
 * filter (RFC 4515) or DN string (RFC 4514).  Return NULL on failure. */
char *
ldap_filter_correct (char *in)
{
    size_t count;
    const char special[] = "*()\\ #\"+,;<>";
    struct k5buf buf;

    k5_buf_init_dynamic(&buf);
    while (TRUE) {
        count = strcspn(in, special);
        k5_buf_add_len(&buf, in, count);
        in += count;
        if (*in == '\0')
            break;
        k5_buf_add_fmt(&buf, "\\%2x", (unsigned char)*in++);
    }
    return buf.data;
}

static int
principal_in_realm_2(krb5_principal principal, char *realm) {
    /* Cross realm trust ... */
    if (principal->length == 2 &&
        principal->data[0].length == sizeof ("krbtgt") &&
        strncasecmp (principal->data[0].data, "krbtgt", sizeof ("krbtgt")) &&
        principal->data[1].length == strlen (realm) &&
        strncasecmp (principal->data[1].data, realm, strlen (realm)))
        return 0;

    if (strlen(realm) != principal->realm.length)
        return 1;

    if (strncasecmp(realm, principal->realm.data, principal->realm.length) != 0)
        return 1;

    return 0;
}

/*
 * Lists the realms in the Directory.
 */

krb5_error_code
krb5_ldap_list_realm(krb5_context context, char ***realms)
{
    char                        **values = NULL;
    unsigned int                i = 0;
    int                         count = 0;
    krb5_error_code             st = 0, tempst = 0;
    LDAP                        *ld = NULL;
    LDAPMessage                 *result = NULL, *ent = NULL;
    kdb5_dal_handle             *dal_handle = NULL;
    krb5_ldap_context           *ldap_context = NULL;
    krb5_ldap_server_handle     *ldap_server_handle = NULL;

    SETUP_CONTEXT ();

    /* get the kerberos container DN information */
    if (ldap_context->container_dn == NULL) {
        if ((st = krb5_ldap_read_krbcontainer_dn(context,
                                                 &(ldap_context->container_dn))) != 0)
            goto cleanup;
    }

    /* get ldap handle */
    GET_HANDLE ();

    {
        char *cn[] = {"cn", NULL};
        LDAP_SEARCH(ldap_context->container_dn,
                    LDAP_SCOPE_ONELEVEL,
                    "(objectclass=krbRealmContainer)",
                    cn);
    }

    *realms = NULL;

    count = ldap_count_entries (ld, result);
    if (count == -1) {
        ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &st);
        st = set_ldap_error (context, st, OP_SEARCH);
        goto cleanup;
    }

    *realms = calloc((unsigned int) count+1, sizeof (char *));
    CHECK_NULL(*realms);

    for (ent = ldap_first_entry(ld, result), count = 0; ent != NULL;
         ent = ldap_next_entry(ld, ent)) {

        if ((values = ldap_get_values (ld, ent, "cn")) != NULL) {

            (*realms)[count] = strdup(values[0]);
            CHECK_NULL((*realms)[count]);
            count += 1;

            ldap_value_free(values);
        }
    } /* for (ent= ... */

cleanup:

    /* some error, free up all the memory */
    if (st != 0) {
        if (*realms) {
            for (i=0; (*realms)[i] != NULL; ++i) {
                free ((*realms)[i]);
            }
            free (*realms);
            *realms = NULL;
        }
    }

    /* If there are no elements, still return a NULL terminated array */

    ldap_msgfree(result);
    krb5_ldap_put_handle_to_pool(ldap_context, ldap_server_handle);
    return st;
}

/*
 * Delete the realm along with the principals belonging to the realm in the Directory.
 */

static void
delete_password_policy (krb5_pointer ptr, osa_policy_ent_t pol)
{
    krb5_ldap_delete_password_policy ((krb5_context)ptr, pol->name);
}

krb5_error_code
krb5_ldap_delete_realm (krb5_context context, char *lrealm)
{
    LDAP                        *ld = NULL;
    krb5_error_code             st = 0, tempst=0;
    char                        **values=NULL, **subtrees=NULL, **policy=NULL;
    LDAPMessage                 **result_arr=NULL, *result = NULL, *ent = NULL;
    krb5_principal              principal;
    unsigned int                l=0, ntree=0;
    int                         i=0, j=0, mask=0;
    kdb5_dal_handle             *dal_handle = NULL;
    krb5_ldap_context           *ldap_context = NULL;
    krb5_ldap_server_handle     *ldap_server_handle = NULL;
    krb5_ldap_realm_params      *rparam=NULL;

    SETUP_CONTEXT ();

    if (lrealm == NULL) {
        st = EINVAL;
        k5_setmsg(context, st, _("Realm information not available"));
        goto cleanup;
    }

    if ((st=krb5_ldap_read_realm_params(context, lrealm, &rparam, &mask)) != 0)
        goto cleanup;

    /* get ldap handle */
    GET_HANDLE ();

    /* delete all the principals belonging to the realm in the tree */
    {
        char *attr[] = {"krbprincipalname", NULL}, *realm=NULL, filter[256];
        krb5_ldap_context lcontext;

        realm = ldap_filter_correct (lrealm);
        assert (sizeof (filter) >= sizeof ("(krbprincipalname=)") +
                strlen (realm) + 2 /* "*@" */ + 1);

        snprintf (filter, sizeof(filter), "(krbprincipalname=*@%s)", realm);
        free (realm);

        /* LDAP_SEARCH(NULL, LDAP_SCOPE_SUBTREE, filter, attr); */
        memset(&lcontext, 0, sizeof(krb5_ldap_context));
        lcontext.lrparams = rparam;
        if ((st=krb5_get_subtree_info(&lcontext, &subtrees, &ntree)) != 0)
            goto cleanup;

        result_arr = (LDAPMessage **)  calloc((unsigned int)ntree+1,
                                              sizeof(LDAPMessage *));
        if (result_arr == NULL) {
            st = ENOMEM;
            goto cleanup;
        }

        for (l=0; l < ntree; ++l) {
            LDAP_SEARCH(subtrees[l], rparam->search_scope, filter, attr);
            result_arr[l] = result;
        }
    }

    /* NOTE: Here all the principals should be cached and the ldap handle should be freed,
     * as a DAL-LDAP interface is called right down here. Caching might be constrained by
     * availability of the memory. The caching is not done, however there would be limit
     * on the minimum number of handles for a server and it is 2. As the DAL-LDAP is not
     * thread-safe this should suffice.
     */
    for (j=0; (result=result_arr[j]) != NULL; ++j) {
        for (ent = ldap_first_entry (ld, result); ent != NULL;
             ent = ldap_next_entry (ld, ent)) {
            if ((values = ldap_get_values(ld, ent, "krbPrincipalName")) != NULL) {
                for (i = 0; values[i] != NULL; ++i) {
                    krb5_parse_name(context, values[i], &principal);
                    if (principal_in_realm_2(principal, lrealm) == 0) {
                        st=krb5_ldap_delete_principal(context, principal);
                        if (st && st != KRB5_KDB_NOENTRY)
                            goto cleanup;
                    }
                    krb5_free_principal(context, principal);
                }
                ldap_value_free(values);
            }
        }
    }

    /* Delete all password policies */
    krb5_ldap_iterate_password_policy (context, "*", delete_password_policy, context);

    /* Delete all ticket policies */
    {
        if ((st = krb5_ldap_list_policy (context, ldap_context->lrparams->realmdn, &policy)) != 0) {
            k5_prependmsg(context, st, _("Error reading ticket policy"));
            goto cleanup;
        }

        for (i = 0; policy [i] != NULL; i++)
            krb5_ldap_delete_policy(context, policy[i]);
    }

    /* Delete the realm object */
    if ((st=ldap_delete_ext_s(ld, ldap_context->lrparams->realmdn, NULL, NULL)) != LDAP_SUCCESS) {
        int ost = st;
        st = translate_ldap_error (st, OP_DEL);
        k5_setmsg(context, st, _("Realm Delete FAILED: %s"),
                  ldap_err2string(ost));
    }

cleanup:
    if (subtrees) {
        for (l=0; l < ntree; ++l) {
            if (subtrees[l])
                free (subtrees[l]);
        }
        free (subtrees);
    }

    if (result_arr != NULL) {
        for (l = 0; l < ntree; l++)
            ldap_msgfree(result_arr[l]);
        free(result_arr);
    }

    if (policy != NULL) {
        for (i = 0; policy[i] != NULL; i++)
            free (policy[i]);
        free (policy);
    }

    krb5_ldap_free_realm_params(rparam);
    krb5_ldap_put_handle_to_pool(ldap_context, ldap_server_handle);
    return st;
}


/*
 * Modify the realm attributes in the Directory.
 */

krb5_error_code
krb5_ldap_modify_realm(krb5_context context, krb5_ldap_realm_params *rparams,
                       int mask)
{
    LDAP                  *ld=NULL;
    krb5_error_code       st=0;
    char                  **strval=NULL, *strvalprc[5]={NULL};
    LDAPMod               **mods = NULL;
    int                   objectmask=0,k=0;
    kdb5_dal_handle       *dal_handle=NULL;
    krb5_ldap_context     *ldap_context=NULL;
    krb5_ldap_server_handle *ldap_server_handle=NULL;

    if (mask == 0)
        return 0;

    if (rparams == NULL) {
        st = EINVAL;
        return st;
    }

    SETUP_CONTEXT ();

    /* Check validity of arguments */
    if (ldap_context->container_dn == NULL ||
        rparams->tl_data == NULL ||
        rparams->tl_data->tl_data_contents == NULL ||
        ((mask & LDAP_REALM_SUBTREE) && rparams->subtree == NULL) ||
        ((mask & LDAP_REALM_CONTREF) && rparams->containerref == NULL) ||
        0) {
        st = EINVAL;
        goto cleanup;
    }

    /* get ldap handle */
    GET_HANDLE ();

    /* SUBTREE ATTRIBUTE */
    if (mask & LDAP_REALM_SUBTREE) {
        if ( rparams->subtree!=NULL)  {
            /*replace the subtrees with the present if the subtrees are present*/
            for(k=0;k<rparams->subtreecount && rparams->subtree[k]!=NULL;k++) {
                if (strlen(rparams->subtree[k]) != 0) {
                    st = checkattributevalue(ld, rparams->subtree[k], "Objectclass", subtreeclass,
                                             &objectmask);
                    CHECK_CLASS_VALIDITY(st, objectmask, _("subtree value: "));
                }
            }
            strval = rparams->subtree;
            if ((st=krb5_add_str_mem_ldap_mod(&mods, "krbsubtrees", LDAP_MOD_REPLACE,
                                              strval)) != 0) {
                goto cleanup;
            }
        }
    }

    /* CONTAINERREF ATTRIBUTE */
    if (mask & LDAP_REALM_CONTREF) {
        if (strlen(rparams->containerref) != 0 ) {
            st = checkattributevalue(ld, rparams->containerref, "Objectclass", subtreeclass,
                                     &objectmask);
            CHECK_CLASS_VALIDITY(st, objectmask,
                                 _("container reference value: "));
            strvalprc[0] = rparams->containerref;
            strvalprc[1] = NULL;
            if ((st=krb5_add_str_mem_ldap_mod(&mods, "krbPrincContainerRef", LDAP_MOD_REPLACE,
                                              strvalprc)) != 0)
                goto cleanup;
        }
    }

    /* SEARCHSCOPE ATTRIBUTE */
    if (mask & LDAP_REALM_SEARCHSCOPE) {
        if ((st=krb5_add_int_mem_ldap_mod(&mods, "krbsearchscope", LDAP_MOD_REPLACE,
                                          (rparams->search_scope == LDAP_SCOPE_ONELEVEL
                                           || rparams->search_scope == LDAP_SCOPE_SUBTREE) ?
                                          rparams->search_scope : LDAP_SCOPE_SUBTREE)) != 0)
            goto cleanup;
    }

    if (mask & LDAP_REALM_MAXRENEWLIFE) {

        if ((st=krb5_add_int_mem_ldap_mod(&mods, "krbMaxRenewableAge", LDAP_MOD_REPLACE,
                                          rparams->max_renewable_life)) != 0)
            goto cleanup;
    }

    /* krbMaxTicketLife ATTRIBUTE */

    if (mask & LDAP_REALM_MAXTICKETLIFE) {

        if ((st=krb5_add_int_mem_ldap_mod(&mods, "krbMaxTicketLife", LDAP_MOD_REPLACE,
                                          rparams->max_life)) != 0)
            goto cleanup;
    }

    /* krbTicketFlags ATTRIBUTE */

    if (mask & LDAP_REALM_KRBTICKETFLAGS) {

        if ((st=krb5_add_int_mem_ldap_mod(&mods, "krbTicketFlags", LDAP_MOD_REPLACE,
                                          rparams->tktflags)) != 0)
            goto cleanup;
    }


    /* Realm modify opearation */
    if (mods != NULL) {
        if ((st=ldap_modify_ext_s(ld, rparams->realmdn, mods, NULL, NULL)) != LDAP_SUCCESS) {
            st = set_ldap_error (context, st, OP_MOD);
            goto cleanup;
        }
    }

cleanup:

    ldap_mods_free(mods, 1);
    krb5_ldap_put_handle_to_pool(ldap_context, ldap_server_handle);
    return st;
}



/*
 * Create the Kerberos container in the Directory if it does not exist
 */

krb5_error_code
krb5_ldap_create_krbcontainer(krb5_context context, const char *dn)
{
    LDAP                        *ld=NULL;
    char                        *strval[2]={NULL}, **rdns=NULL;
    LDAPMod                     **mods = NULL;
    krb5_error_code             st=0;
    kdb5_dal_handle             *dal_handle=NULL;
    krb5_ldap_context           *ldap_context=NULL;
    krb5_ldap_server_handle     *ldap_server_handle=NULL;

    SETUP_CONTEXT ();

    /* get ldap handle */
    GET_HANDLE ();

    if (dn == NULL) {
        st = EINVAL;
        k5_setmsg(context, st, _("Kerberos Container information is missing"));
        goto cleanup;
    }

    strval[0] = "krbContainer";
    strval[1] = NULL;
    if ((st=krb5_add_str_mem_ldap_mod(&mods, "objectclass", LDAP_MOD_ADD, strval)) != 0)
        goto cleanup;

    rdns = ldap_explode_dn(dn, 1);
    if (rdns == NULL) {
        st = EINVAL;
        k5_setmsg(context, st, _("Invalid Kerberos container DN"));
        goto cleanup;
    }

    strval[0] = rdns[0];
    strval[1] = NULL;
    if ((st=krb5_add_str_mem_ldap_mod(&mods, "cn", LDAP_MOD_ADD, strval)) != 0)
        goto cleanup;

    /* create the kerberos container */
    st = ldap_add_ext_s(ld, dn, mods, NULL, NULL);
    if (st == LDAP_ALREADY_EXISTS)
        st = LDAP_SUCCESS;
    if (st != LDAP_SUCCESS) {
        int ost = st;
        st = translate_ldap_error (st, OP_ADD);
        k5_setmsg(context, st, _("Kerberos Container create FAILED: %s"),
                  ldap_err2string(ost));
        goto cleanup;
    }

cleanup:

    if (rdns)
        ldap_value_free (rdns);

    ldap_mods_free(mods, 1);
    krb5_ldap_put_handle_to_pool(ldap_context, ldap_server_handle);
    return(st);
}

/*
 * Delete the Kerberos container in the Directory
 */

krb5_error_code
krb5_ldap_delete_krbcontainer(krb5_context context, const char *dn)
{
    LDAP                        *ld=NULL;
    krb5_error_code             st=0;
    kdb5_dal_handle             *dal_handle=NULL;
    krb5_ldap_context           *ldap_context=NULL;
    krb5_ldap_server_handle     *ldap_server_handle=NULL;

    SETUP_CONTEXT ();

    /* get ldap handle */
    GET_HANDLE ();

    if (dn == NULL) {
        st = EINVAL;
        k5_setmsg(context, st, _("Kerberos Container information is missing"));
        goto cleanup;
    }

    /* delete the kerberos container */
    if ((st = ldap_delete_ext_s(ld, dn, NULL, NULL)) != LDAP_SUCCESS) {
        int ost = st;
        st = translate_ldap_error (st, OP_ADD);
        k5_setmsg(context, st, _("Kerberos Container delete FAILED: %s"),
                  ldap_err2string(ost));
        goto cleanup;
    }

cleanup:

    krb5_ldap_put_handle_to_pool(ldap_context, ldap_server_handle);
    return(st);
}


/*
 * Create Realm in eDirectory. This is used by kdb5_util
 */

krb5_error_code
krb5_ldap_create_realm(krb5_context context, krb5_ldap_realm_params *rparams,
                       int mask)
{
    LDAP                        *ld=NULL;
    krb5_error_code             st=0;
    char                        *dn=NULL;
    char                        *strval[4]={NULL};
    char                        *contref[2]={NULL};
    LDAPMod                     **mods = NULL;
    int                         i=0, objectmask=0, subtreecount=0;
    kdb5_dal_handle             *dal_handle=NULL;
    krb5_ldap_context           *ldap_context=NULL;
    krb5_ldap_server_handle     *ldap_server_handle=NULL;
    char                        *realm_name;

    SETUP_CONTEXT ();

    /* Check input validity ... */
    if (ldap_context->container_dn == NULL ||
        rparams == NULL ||
        rparams->realm_name == NULL ||
        ((mask & LDAP_REALM_SUBTREE) && rparams->subtree  == NULL) ||
        ((mask & LDAP_REALM_CONTREF) && rparams->containerref == NULL) ||
        0) {
        st = EINVAL;
        return st;
    }

    /* get ldap handle */
    GET_HANDLE ();

    realm_name = rparams->realm_name;

    if (asprintf(&dn, "cn=%s,%s", realm_name, ldap_context->container_dn) < 0)
        dn = NULL;
    CHECK_NULL(dn);

    strval[0] = realm_name;
    strval[1] = NULL;
    if ((st=krb5_add_str_mem_ldap_mod(&mods, "cn", LDAP_MOD_ADD, strval)) != 0)
        goto cleanup;

    strval[0] = "top";
    strval[1] = "krbrealmcontainer";
    strval[2] = "krbticketpolicyaux";
    strval[3] = NULL;

    if ((st=krb5_add_str_mem_ldap_mod(&mods, "objectclass", LDAP_MOD_ADD, strval)) != 0)
        goto cleanup;

    /* SUBTREE ATTRIBUTE */
    if (mask & LDAP_REALM_SUBTREE) {
        if ( rparams->subtree!=NULL)  {
            subtreecount = rparams->subtreecount;
            for (i=0; rparams->subtree[i]!=NULL && i<subtreecount; i++) {
                if (strlen(rparams->subtree[i]) != 0) {
                    st = checkattributevalue(ld, rparams->subtree[i], "Objectclass", subtreeclass,
                                             &objectmask);
                    CHECK_CLASS_VALIDITY(st, objectmask,
                                         _("realm object value: "));
                }
            }
            if ((st=krb5_add_str_mem_ldap_mod(&mods, "krbsubtrees", LDAP_MOD_ADD,
                                              rparams->subtree)) != 0) {
                goto cleanup;
            }
        }
    }

    /* CONTAINER REFERENCE ATTRIBUTE */
    if (mask & LDAP_REALM_CONTREF) {
        if (strlen(rparams->containerref) != 0 ) {
            st = checkattributevalue(ld, rparams->containerref, "Objectclass", subtreeclass,
                                     &objectmask);
            CHECK_CLASS_VALIDITY(st, objectmask, "realm object value: ");
            contref[0] = rparams->containerref;
            contref[1] = NULL;
            if ((st=krb5_add_str_mem_ldap_mod(&mods, "krbPrincContainerRef", LDAP_MOD_ADD,
                                              contref)) != 0)
                goto cleanup;
        }
    }

    /* SEARCHSCOPE ATTRIBUTE */
    if (mask & LDAP_REALM_SEARCHSCOPE) {
        if ((st=krb5_add_int_mem_ldap_mod(&mods, "krbsearchscope", LDAP_MOD_ADD,
                                          (rparams->search_scope == LDAP_SCOPE_ONELEVEL
                                           || rparams->search_scope == LDAP_SCOPE_SUBTREE) ?
                                          rparams->search_scope : LDAP_SCOPE_SUBTREE)) != 0)
            goto cleanup;
    }
    if (mask & LDAP_REALM_MAXRENEWLIFE) {

        if ((st=krb5_add_int_mem_ldap_mod(&mods, "krbMaxRenewableAge", LDAP_MOD_ADD,
                                          rparams->max_renewable_life)) != 0)
            goto cleanup;
    }

    /* krbMaxTicketLife ATTRIBUTE */

    if (mask & LDAP_REALM_MAXTICKETLIFE) {

        if ((st=krb5_add_int_mem_ldap_mod(&mods, "krbMaxTicketLife", LDAP_MOD_ADD,
                                          rparams->max_life)) != 0)
            goto cleanup;
    }

    /* krbTicketFlags ATTRIBUTE */

    if (mask & LDAP_REALM_KRBTICKETFLAGS) {

        if ((st=krb5_add_int_mem_ldap_mod(&mods, "krbTicketFlags", LDAP_MOD_ADD,
                                          rparams->tktflags)) != 0)
            goto cleanup;
    }


    /* realm creation operation */
    if ((st=ldap_add_ext_s(ld, dn, mods, NULL, NULL)) != LDAP_SUCCESS) {
        st = set_ldap_error (context, st, OP_ADD);
        goto cleanup;
    }

cleanup:

    if (dn)
        free(dn);

    ldap_mods_free(mods, 1);
    krb5_ldap_put_handle_to_pool(ldap_context, ldap_server_handle);
    return st;
}

/*
 * Read the realm container configuration from eDirectory for the specified realm.
 */

krb5_error_code
krb5_ldap_read_realm_params(krb5_context context, char *lrealm,
                            krb5_ldap_realm_params **rlparamp, int *mask)
{
    char                   **values=NULL;
    krb5_error_code        st=0, tempst=0;
    LDAP                   *ld=NULL;
    LDAPMessage            *result=NULL,*ent=NULL;
    krb5_ldap_realm_params *rlparams=NULL;
    kdb5_dal_handle        *dal_handle=NULL;
    krb5_ldap_context      *ldap_context=NULL;
    krb5_ldap_server_handle *ldap_server_handle=NULL;
    int x=0;

    SETUP_CONTEXT ();

    /* validate the input parameter */
    if (lrealm == NULL || ldap_context->container_dn == NULL) {
        st = EINVAL;
        goto cleanup;
    }

    /* get ldap handle */
    GET_HANDLE ();

    /* Initialize realm container structure */
    rlparams =(krb5_ldap_realm_params *) malloc(sizeof(krb5_ldap_realm_params));
    CHECK_NULL(rlparams);
    memset(rlparams, 0, sizeof(krb5_ldap_realm_params));

    /* allocate tl_data structure to store MASK information */
    rlparams->tl_data = malloc (sizeof(krb5_tl_data));
    if (rlparams->tl_data == NULL) {
        st = ENOMEM;
        goto cleanup;
    }
    memset(rlparams->tl_data, 0, sizeof(krb5_tl_data));
    rlparams->tl_data->tl_data_type = KDB_TL_USER_INFO;

    /* set the mask parameter to 0 */
    *mask = 0;

    /* set default values */
    rlparams->search_scope = LDAP_SCOPE_SUBTREE;

    if (asprintf(&rlparams->realmdn, "cn=%s,%s", lrealm,
                 ldap_context->container_dn) < 0) {
        rlparams->realmdn = NULL;
        st = ENOMEM;
        goto cleanup;
    }

    /* populate the realm name in the structure */
    rlparams->realm_name = strdup(lrealm);
    CHECK_NULL(rlparams->realm_name);

    LDAP_SEARCH(rlparams->realmdn, LDAP_SCOPE_BASE, "(objectclass=krbRealmContainer)", realm_attributes);

    if ((st = ldap_count_entries(ld, result)) <= 0) {
        /* This could happen when the DN used to bind and read the realm object
         * does not have sufficient rights to read its attributes
         */
        st = KRB5_KDB_ACCESS_ERROR; /* return some other error ? */
        goto cleanup;
    }

    ent = ldap_first_entry (ld, result);
    if (ent == NULL) {
        ldap_get_option (ld, LDAP_OPT_ERROR_NUMBER, (void *) &st);
#if 0
        st = translate_ldap_error(st, OP_SEARCH);
#endif
        goto cleanup;
    }

    /* Read the attributes */
    {
        if ((values=ldap_get_values(ld, ent, "krbSubTrees")) != NULL) {
            rlparams->subtreecount = ldap_count_values(values);
            rlparams->subtree = (char **) malloc(sizeof(char *) * (rlparams->subtreecount + 1));
            if (rlparams->subtree == NULL) {
                st = ENOMEM;
                goto cleanup;
            }
            for (x=0; x<rlparams->subtreecount; x++) {
                rlparams->subtree[x] = strdup(values[x]);
                if (rlparams->subtree[x] == NULL) {
                    st = ENOMEM;
                    goto cleanup;
                }
            }
            rlparams->subtree[rlparams->subtreecount] = NULL;
            *mask |= LDAP_REALM_SUBTREE;
            ldap_value_free(values);
        }

        if((values=ldap_get_values(ld, ent, "krbPrincContainerRef")) != NULL) {
            rlparams->containerref = strdup(values[0]);
            if(rlparams->containerref == NULL) {
                st = ENOMEM;
                goto cleanup;
            }
            *mask |= LDAP_REALM_CONTREF;
            ldap_value_free(values);
        }

        if ((values=ldap_get_values(ld, ent, "krbSearchScope")) != NULL) {
            rlparams->search_scope=atoi(values[0]);
            /* searchscope can be ONE-LEVEL or SUBTREE, else default to SUBTREE */
            if (!(rlparams->search_scope==1 || rlparams->search_scope==2))
                rlparams->search_scope = LDAP_SCOPE_SUBTREE;
            *mask |= LDAP_REALM_SEARCHSCOPE;
            ldap_value_free(values);
        }

        if ((values=ldap_get_values(ld, ent, "krbMaxTicketLife")) != NULL) {
            rlparams->max_life = atoi(values[0]);
            *mask |= LDAP_REALM_MAXTICKETLIFE;
            ldap_value_free(values);
        }

        if ((values=ldap_get_values(ld, ent, "krbMaxRenewableAge")) != NULL) {
            rlparams->max_renewable_life = atoi(values[0]);
            *mask |= LDAP_REALM_MAXRENEWLIFE;
            ldap_value_free(values);
        }

        if ((values=ldap_get_values(ld, ent, "krbTicketFlags")) != NULL) {
            rlparams->tktflags = atoi(values[0]);
            *mask |= LDAP_REALM_KRBTICKETFLAGS;
            ldap_value_free(values);
        }

    }

    rlparams->mask = *mask;
    *rlparamp = rlparams;
    st = store_tl_data(rlparams->tl_data, KDB_TL_MASK, mask);

cleanup:

    /* if there is an error, free allocated structures */
    if (st != 0) {
        krb5_ldap_free_realm_params(rlparams);
        *rlparamp=NULL;
    }
    ldap_msgfree(result);
    krb5_ldap_put_handle_to_pool(ldap_context, ldap_server_handle);
    return st;
}


/*
  Free the krb5_ldap_realm_params.
*/
void
krb5_ldap_free_realm_params(krb5_ldap_realm_params *rparams)
{
    int i=0;

    if (rparams) {
        if (rparams->realmdn)
            free(rparams->realmdn);

        if (rparams->realm_name)
            free(rparams->realm_name);

        if (rparams->subtree) {
            for (i=0; i<rparams->subtreecount && rparams->subtree[i] ; i++)
                free(rparams->subtree[i]);
            free(rparams->subtree);
        }

        if (rparams->containerref)
            free(rparams->containerref);

        if (rparams->kdcservers) {
            for (i=0; rparams->kdcservers[i]; ++i)
                free(rparams->kdcservers[i]);
            free(rparams->kdcservers);
        }

        if (rparams->adminservers) {
            for (i=0; rparams->adminservers[i]; ++i)
                free(rparams->adminservers[i]);
            free(rparams->adminservers);
        }

        if (rparams->passwdservers) {
            for (i=0; rparams->passwdservers[i]; ++i)
                free(rparams->passwdservers[i]);
            free(rparams->passwdservers);
        }

        if (rparams->tl_data) {
            if (rparams->tl_data->tl_data_contents)
                free(rparams->tl_data->tl_data_contents);
            free(rparams->tl_data);
        }

        free(rparams);
    }
    return;
}

/*
 * ******************************************************************************
 * DAL functions
 * ******************************************************************************
 */

krb5_error_code
krb5_ldap_delete_realm_1(krb5_context kcontext, char *conf_section,
                         char **db_args)
{
    krb5_error_code status = KRB5_PLUGIN_OP_NOTSUPP;
    k5_setmsg(kcontext, status, "LDAP %s", error_message(status));
    return status;
}
