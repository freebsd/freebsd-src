/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/ldap/libkdb_ldap/ldap_tkt_policy.c */
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

#include "ldap_main.h"
#include "kdb_ldap.h"
#include "ldap_tkt_policy.h"
#include "ldap_err.h"

/* Ticket policy object management */

static void
free_list(char **list)
{
    int i;

    for (i = 0; list != NULL && list[i] != NULL; i++)
        free(list[i]);
    free(list);
}

/*
 * create the Ticket policy object in Directory.
 */
krb5_error_code
krb5_ldap_create_policy(krb5_context context, krb5_ldap_policy_params *policy,
                        int mask)
{
    krb5_error_code             st=0;
    LDAP                        *ld=NULL;
    char                        *strval[3]={NULL}, *policy_dn = NULL;
    LDAPMod                     **mods=NULL;
    kdb5_dal_handle             *dal_handle=NULL;
    krb5_ldap_context           *ldap_context=NULL;
    krb5_ldap_server_handle     *ldap_server_handle=NULL;

    /* validate the input parameters */
    if (policy == NULL || policy->policy == NULL) {
        st = EINVAL;
        k5_setmsg(context, st, _("Ticket Policy Name missing"));
        goto cleanup;
    }

    SETUP_CONTEXT();
    GET_HANDLE();

    if ((st = krb5_ldap_name_to_policydn (context, policy->policy, &policy_dn)) != 0)
        goto cleanup;

    memset(strval, 0, sizeof(strval));
    strval[0] = policy->policy;
    if ((st=krb5_add_str_mem_ldap_mod(&mods, "cn", LDAP_MOD_ADD, strval)) != 0)
        goto cleanup;

    memset(strval, 0, sizeof(strval));
    strval[0] = "krbTicketPolicy";
    strval[1] = "krbTicketPolicyaux";
    if ((st=krb5_add_str_mem_ldap_mod(&mods, "objectclass", LDAP_MOD_ADD, strval)) != 0)
        goto cleanup;

    if (mask & LDAP_POLICY_MAXTKTLIFE) {
        if ((st=krb5_add_int_mem_ldap_mod(&mods, "krbmaxticketlife", LDAP_MOD_ADD,
                                          policy->maxtktlife)) != 0)
            goto cleanup;
    }

    if (mask & LDAP_POLICY_MAXRENEWLIFE) {
        if ((st=krb5_add_int_mem_ldap_mod(&mods, "krbmaxrenewableage", LDAP_MOD_ADD,
                                          policy->maxrenewlife)) != 0)
            goto cleanup;
    }

    if (mask & LDAP_POLICY_TKTFLAGS) {
        if ((st=krb5_add_int_mem_ldap_mod(&mods, "krbticketflags", LDAP_MOD_ADD,
                                          policy->tktflags)) != 0)
            goto cleanup;
    }

    /* ldap add operation */
    if ((st=ldap_add_ext_s(ld, policy_dn, mods, NULL, NULL)) != LDAP_SUCCESS) {
        st = set_ldap_error (context, st, OP_ADD);
        goto cleanup;
    }

cleanup:
    if (policy_dn != NULL)
        free(policy_dn);

    ldap_mods_free(mods, 1);
    krb5_ldap_put_handle_to_pool(ldap_context, ldap_server_handle);
    return st;
}


/*
 * modify the Ticket policy object in Directory.
 */

krb5_error_code
krb5_ldap_modify_policy(krb5_context context, krb5_ldap_policy_params *policy,
                        int mask)
{
    int                         objectmask=0;
    krb5_error_code             st=0;
    LDAP                        *ld=NULL;
    char                        *attrvalues[]={"krbTicketPolicy", "krbTicketPolicyAux", NULL}, *strval[2]={NULL};
    char                        *policy_dn = NULL;
    LDAPMod                     **mods=NULL;
    kdb5_dal_handle             *dal_handle=NULL;
    krb5_ldap_context           *ldap_context=NULL;
    krb5_ldap_server_handle     *ldap_server_handle=NULL;

    /* validate the input parameters */
    if (policy == NULL || policy->policy==NULL) {
        st = EINVAL;
        k5_setmsg(context, st, _("Ticket Policy Name missing"));
        goto cleanup;
    }

    SETUP_CONTEXT();
    GET_HANDLE();

    if ((st = krb5_ldap_name_to_policydn (context, policy->policy, &policy_dn)) != 0)
        goto cleanup;

    /* the policydn object should be of the krbTicketPolicy object class */
    st = checkattributevalue(ld, policy_dn, "objectClass", attrvalues, &objectmask);
    CHECK_CLASS_VALIDITY(st, objectmask, _("ticket policy object: "));

    if ((objectmask & 0x02) == 0) { /* add krbticketpolicyaux to the object class list */
        memset(strval, 0, sizeof(strval));
        strval[0] = "krbTicketPolicyAux";
        if ((st=krb5_add_str_mem_ldap_mod(&mods, "objectclass", LDAP_MOD_ADD, strval)) != 0)
            goto cleanup;
    }

    if (mask & LDAP_POLICY_MAXTKTLIFE) {
        if ((st=krb5_add_int_mem_ldap_mod(&mods, "krbmaxticketlife", LDAP_MOD_REPLACE,
                                          policy->maxtktlife)) != 0)
            goto cleanup;
    }

    if (mask & LDAP_POLICY_MAXRENEWLIFE) {
        if ((st=krb5_add_int_mem_ldap_mod(&mods, "krbmaxrenewableage", LDAP_MOD_REPLACE,
                                          policy->maxrenewlife)) != 0)
            goto cleanup;
    }

    if (mask & LDAP_POLICY_TKTFLAGS) {
        if ((st=krb5_add_int_mem_ldap_mod(&mods, "krbticketflags", LDAP_MOD_REPLACE,
                                          policy->tktflags)) != 0)
            goto cleanup;
    }

    if ((st=ldap_modify_ext_s(ld, policy_dn, mods, NULL, NULL)) != LDAP_SUCCESS) {
        st = set_ldap_error (context, st, OP_MOD);
        goto cleanup;
    }

cleanup:
    if (policy_dn != NULL)
        free(policy_dn);

    ldap_mods_free(mods, 1);
    krb5_ldap_put_handle_to_pool(ldap_context, ldap_server_handle);
    return st;
}


/*
 * Read the policy object from the Directory and populate the krb5_ldap_policy_params
 * structure.
 */

krb5_error_code
krb5_ldap_read_policy(krb5_context context, char *policyname,
                      krb5_ldap_policy_params **policy, int *omask)
{
    krb5_error_code             st=0, tempst=0;
    int                         objectmask=0, val=0;
    LDAP                        *ld=NULL;
    LDAPMessage                 *result=NULL,*ent=NULL;
    char                        *attributes[] = { "krbMaxTicketLife", "krbMaxRenewableAge", "krbTicketFlags", NULL};
    char                        *attrvalues[] = { "krbTicketPolicy", NULL}, *policy_dn = NULL;
    krb5_ldap_policy_params     *lpolicy=NULL;
    kdb5_dal_handle             *dal_handle=NULL;
    krb5_ldap_context           *ldap_context=NULL;
    krb5_ldap_server_handle     *ldap_server_handle=NULL;

    /* validate the input parameters */
    if (policyname == NULL  || policy == NULL) {
        st = EINVAL;
        k5_setmsg(context, st, _("Ticket Policy Object information missing"));
        goto cleanup;
    }

    SETUP_CONTEXT();
    GET_HANDLE();

    if ((st = krb5_ldap_name_to_policydn (context, policyname, &policy_dn)) != 0)
        goto cleanup;

    /* the policydn object should be of the krbTicketPolicy object class */
    st = checkattributevalue(ld, policy_dn, "objectClass", attrvalues, &objectmask);
    CHECK_CLASS_VALIDITY(st, objectmask, _("ticket policy object: "));

    /* Initialize ticket policy structure */
    lpolicy =(krb5_ldap_policy_params *) malloc(sizeof(krb5_ldap_policy_params));
    CHECK_NULL(lpolicy);
    memset(lpolicy, 0, sizeof(krb5_ldap_policy_params));

    if ((lpolicy->policy = strdup (policyname)) == NULL) {
        st = ENOMEM;
        goto cleanup;
    }

    lpolicy->tl_data = calloc (1, sizeof(*lpolicy->tl_data));
    CHECK_NULL(lpolicy->tl_data);
    lpolicy->tl_data->tl_data_type = KDB_TL_USER_INFO;

    LDAP_SEARCH(policy_dn, LDAP_SCOPE_BASE, "(objectclass=krbTicketPolicy)", attributes);

    *omask = 0;

    ent=ldap_first_entry(ld, result);
    if (ent != NULL) {
        if (krb5_ldap_get_value(ld, ent, "krbmaxticketlife", &val) == 0) {
            lpolicy->maxtktlife = val;
            *omask |= LDAP_POLICY_MAXTKTLIFE;
        }
        if (krb5_ldap_get_value(ld, ent, "krbmaxrenewableage", &val) == 0) {
            lpolicy->maxrenewlife = val;
            *omask |= LDAP_POLICY_MAXRENEWLIFE;
        }
        if (krb5_ldap_get_value(ld, ent, "krbticketflags", &val) == 0) {
            lpolicy->tktflags = val;
            *omask |= LDAP_POLICY_TKTFLAGS;
        }
    }

    lpolicy->mask = *omask;
    store_tl_data(lpolicy->tl_data, KDB_TL_MASK, omask);
    *policy = lpolicy;

cleanup:
    if (st != 0) {
        krb5_ldap_free_policy(context, lpolicy);
        *policy = NULL;
    }
    free(policy_dn);
    ldap_msgfree(result);
    krb5_ldap_put_handle_to_pool(ldap_context, ldap_server_handle);
    return st;
}


/*
 * Function to delete ticket policy object from the directory.  Before
 * calling this function krb5_ldap_read_policy should be called to
 * check the existence of the object.  This serves one major purpose,
 * i.e., if the object to be is anything other than the ticket policy
 * object then the krb5_ldap_read_policy returns an error and thus is
 * not accidently deleted in this function.
 *
 * NOTE: Other kerberos objects (user/realm object) might be having
 * references to the policy object to be deleted. This situation is
 * not handled here, instead is taken care of at all the places where
 * the deleted policy object is read, to ignore a return status of
 * LDAP_NO_SUCH_OBJECT and continue.
 */

krb5_error_code
krb5_ldap_delete_policy(krb5_context context, char *policyname)
{
    int                         refcount = 0;
    char                        *policy_dn = NULL;
    krb5_error_code             st = 0;
    LDAP                        *ld = NULL;
    kdb5_dal_handle             *dal_handle=NULL;
    krb5_ldap_context           *ldap_context=NULL;
    krb5_ldap_server_handle     *ldap_server_handle=NULL;

    if (policyname == NULL) {
        st = EINVAL;
        k5_prependmsg(context, st, _("Ticket Policy Object DN missing"));
        goto cleanup;
    }


    SETUP_CONTEXT();
    GET_HANDLE();

    if ((st = krb5_ldap_name_to_policydn (context, policyname, &policy_dn)) != 0)
        goto cleanup;

    /* Checking for policy count for 0 and will not permit delete if
     * it is greater than 0.  */

    if ((st = krb5_ldap_get_reference_count (context, policy_dn,
                                             "krbTicketPolicyReference", &refcount, ld)) != 0)
        goto cleanup;

    if (refcount == 0) {
        if ((st=ldap_delete_ext_s(ld, policy_dn, NULL, NULL)) != 0) {
            k5_prependmsg(context, st, "%s", ldap_err2string(st));

            goto cleanup;
        }
    } else {
        st = EINVAL;
        k5_prependmsg(context, st,
                      _("Delete Failed: One or more Principals associated "
                        "with the Ticket Policy"));
        goto cleanup;
    }

cleanup:
    if (policy_dn != NULL)
        free (policy_dn);
    krb5_ldap_put_handle_to_pool(ldap_context, ldap_server_handle);
    return st;
}


/*
 * list policy objects from Directory
 */

krb5_error_code
krb5_ldap_list_policy(krb5_context context, char *containerdn, char ***policy)
{
    int                         i, j, count;
    char                        **list = NULL;
    char                        *policycontainerdn = containerdn;
    kdb5_dal_handle             *dal_handle=NULL;
    krb5_ldap_context           *ldap_context=NULL;
    krb5_error_code             st=0;

    SETUP_CONTEXT();
    if (policycontainerdn == NULL) {
        policycontainerdn = ldap_context->lrparams->realmdn;
    }

    if ((st = krb5_ldap_list(context, &list, "krbTicketPolicy", policycontainerdn)) != 0)
        goto cleanup;

    for (i = 0; list[i] != NULL; i++);

    count = i;

    *policy = (char **) calloc ((unsigned) count + 1, sizeof(char *));
    if (*policy == NULL) {
        st = ENOMEM;
        goto cleanup;
    }

    for (i = 0, j = 0; list[i] != NULL; i++, j++) {
        int ret;
        ret = krb5_ldap_policydn_to_name (context, list[i], &(*policy)[i]);
        if (ret != 0)
            j--;
    }

cleanup:
    free_list(list);
    return st;
}

/*
 * Function to free the ticket policy object structure.
 * Note: this function assumes that memory of the policy structure is dynamically allocated and hence the whole
 * structure is freed up. Care should be taken not to call this function on a static structure
 */

krb5_error_code
krb5_ldap_free_policy(krb5_context context, krb5_ldap_policy_params *policy)
{

    krb5_error_code st=0;

    if (policy == NULL)
        return st;

    if (policy->policy)
        free (policy->policy);

    if (policy->tl_data) {
        if (policy->tl_data->tl_data_contents)
            free (policy->tl_data->tl_data_contents);
        free (policy->tl_data);
    }
    free (policy);

    return st;
}

/*
 * This function is general object listing routine.  It is currently
 * used for ticket policy object listing.
 */

krb5_error_code
krb5_ldap_list(krb5_context context, char ***list, char *objectclass,
               char *containerdn)
{
    char                        *filter=NULL, *dn=NULL;
    krb5_error_code             st=0, tempst=0;
    int                         count=0, filterlen=0;
    LDAP                        *ld=NULL;
    LDAPMessage                 *result=NULL,*ent=NULL;
    kdb5_dal_handle             *dal_handle=NULL;
    krb5_ldap_context           *ldap_context=NULL;
    krb5_ldap_server_handle     *ldap_server_handle=NULL;

    SETUP_CONTEXT();
    GET_HANDLE();

    /* check if the containerdn exists */
    if (containerdn) {
        if ((st=checkattributevalue(ld, containerdn, NULL, NULL, NULL)) != 0) {
            k5_prependmsg(context, st, _("Error reading container object"));
            goto cleanup;
        }
    }

    /* set the filter for the search operation */
    filterlen = strlen("(objectclass=") + strlen(objectclass) + 1 + 1;
    filter = malloc ((unsigned) filterlen);
    if (filter == NULL) {
        st = ENOMEM;
        goto cleanup;
    }
    snprintf(filter, (unsigned) filterlen,"(objectclass=%s)",objectclass);

    LDAP_SEARCH(containerdn, LDAP_SCOPE_SUBTREE, filter, NULL);

    count = ldap_count_entries(ld, result);
    if (count == -1) {
        ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &st);
        st = set_ldap_error(context, st, OP_SEARCH);
        goto cleanup;
    }
    *list = (char **) calloc ((unsigned) count+1, sizeof(char *));
    if (*list == NULL) {
        st = ENOMEM;
        goto cleanup;
    }

    for (ent=ldap_first_entry(ld, result), count=0; ent != NULL; ent=ldap_next_entry(ld, ent), ++count) {
        if ((dn=ldap_get_dn(ld, ent)) == NULL)
            continue;
        if (((*list)[count] = strdup(dn)) == NULL) {
            ldap_memfree (dn);
            st = ENOMEM;
            goto cleanup;
        }
        ldap_memfree(dn);
    }

cleanup:
    if (filter)
        free (filter);

    /* some error, free up all the memory */
    if (st != 0) {
        free_list(*list);
        *list = NULL;
    }
    ldap_msgfree(result);
    krb5_ldap_put_handle_to_pool(ldap_context, ldap_server_handle);
    return st;
}
