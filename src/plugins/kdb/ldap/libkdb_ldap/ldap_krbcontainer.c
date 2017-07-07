/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/ldap/libkdb_ldap/ldap_krbcontainer.c */
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
#include "ldap_err.h"

/*
 * Read the kerberos container location from krb5.conf.
 */

krb5_error_code
krb5_ldap_read_krbcontainer_dn(krb5_context context, char **container_dn)
{
    krb5_error_code                 st=0;
    char                            *dn=NULL;
    kdb5_dal_handle                 *dal_handle=NULL;
    krb5_ldap_context               *ldap_context=NULL;
    krb5_ldap_server_handle         *ldap_server_handle=NULL;

    *container_dn = NULL;
    SETUP_CONTEXT();

    /* read kerberos containter location from [dbmodules] section of krb5.conf file */
    if (ldap_context->conf_section) {
        if ((st=profile_get_string(context->profile, KDB_MODULE_SECTION, ldap_context->conf_section,
                                   KRB5_CONF_LDAP_KERBEROS_CONTAINER_DN, NULL,
                                   &dn)) != 0) {
            k5_setmsg(context, st, _("Error reading kerberos container "
                                     "location from krb5.conf"));
            goto cleanup;
        }
    }

    /* read kerberos containter location from [dbdefaults] section of krb5.conf file */
    if (dn == NULL) {
        if ((st=profile_get_string(context->profile, KDB_MODULE_DEF_SECTION,
                                   KRB5_CONF_LDAP_KERBEROS_CONTAINER_DN, NULL,
                                   NULL, &dn)) != 0) {
            k5_setmsg(context, st, _("Error reading kerberos container "
                                     "location from krb5.conf"));
            goto cleanup;
        }
    }

    if (dn == NULL) {
        st = KRB5_KDB_SERVER_INTERNAL_ERR;
        k5_setmsg(context, st, _("Kerberos container location not specified"));
        goto cleanup;
    }

    *container_dn = dn;

cleanup:
    krb5_ldap_put_handle_to_pool(ldap_context, ldap_server_handle);
    return st;
}
