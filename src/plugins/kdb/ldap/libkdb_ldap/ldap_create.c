/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/ldap/libkdb_ldap/ldap_create.c */
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
#include "ldap_krbcontainer.h"
#include "ldap_err.h"

/*
 * ******************************************************************************
 * DAL functions
 * ******************************************************************************
 */

/*
 * This function will create a krbcontainer and realm on the LDAP Server, with
 * the specified attributes.
 */
krb5_error_code
krb5_ldap_create(krb5_context context, char *conf_section, char **db_args)
{
    krb5_error_code status = 0;
    krb5_ldap_realm_params *rparams = NULL;
    krb5_ldap_context *ldap_context=NULL;
    krb5_boolean realm_obj_created = FALSE;
    krb5_boolean krbcontainer_obj_created = FALSE;
    int mask = 0;

    /* Clear the global error string */
    krb5_clear_error_message(context);

    ldap_context = k5alloc(sizeof(krb5_ldap_context), &status);
    if (ldap_context == NULL)
        goto cleanup;
    context->dal_handle->db_context = ldap_context;
    ldap_context->kcontext = context;

    status = krb5_ldap_parse_db_params(context, db_args);
    if (status) {
        k5_prependmsg(context, status, _("Error processing LDAP DB params"));
        goto cleanup;
    }

    status = krb5_ldap_read_server_params(context, conf_section, KRB5_KDB_SRV_TYPE_ADMIN);
    if (status) {
        k5_prependmsg(context, status, _("Error reading LDAP server params"));
        goto cleanup;
    }
    status = krb5_ldap_db_init(context, ldap_context);
    if (status) {
        goto cleanup;
    }

    /* read the kerberos container */
    status = krb5_ldap_read_krbcontainer_dn(context,
                                            &ldap_context->container_dn);
    if (status)
        goto cleanup;

    status = krb5_ldap_create_krbcontainer(context,
                                           ldap_context->container_dn);
    if (status)
        goto cleanup;

    rparams = (krb5_ldap_realm_params *) malloc(sizeof(krb5_ldap_realm_params));
    if (rparams == NULL) {
        status = ENOMEM;
        goto cleanup;
    }
    memset(rparams, 0, sizeof(*rparams));
    rparams->realm_name = strdup(context->default_realm);
    if (rparams->realm_name == NULL) {
        status = ENOMEM;
        goto cleanup;
    }

    if ((status = krb5_ldap_create_realm(context, rparams, mask)))
        goto cleanup;

    /* We just created the Realm container. Here starts our transaction tracking */
    realm_obj_created = TRUE;

    /* verify realm object */
    if ((status = krb5_ldap_read_realm_params(context,
                                              rparams->realm_name,
                                              &(ldap_context->lrparams),
                                              &mask)))
        goto cleanup;

cleanup:
    /* If the krbcontainer/realm creation is not complete, do the roll-back here */
    if ((krbcontainer_obj_created) && (!realm_obj_created)) {
        int rc;
        rc = krb5_ldap_delete_krbcontainer(context,
                                           ldap_context->container_dn);
        k5_setmsg(context, rc, _("could not complete roll-back, error "
                                 "deleting Kerberos Container"));
    }

    if (rparams)
        krb5_ldap_free_realm_params(rparams);

    if (status)
        krb5_ldap_close(context);
    return(status);
}
