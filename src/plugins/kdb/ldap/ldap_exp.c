/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/ldap/ldap_exp.c */
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

#include "k5-int.h"
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <errno.h>
#include <utime.h>
#include <kdb5.h>
#include "kdb_ldap.h"
#include "ldap_principal.h"
#include "ldap_pwd_policy.h"


/*
 *      Exposed API
 */

kdb_vftabl PLUGIN_SYMBOL_NAME(krb5_ldap, kdb_function_table) = {
    KRB5_KDB_DAL_MAJOR_VERSION,             /* major version number */
    0,                                      /* minor version number 0 */
    /* init_library */                      krb5_ldap_lib_init,
    /* fini_library */                      krb5_ldap_lib_cleanup,
    /* init_module */                       krb5_ldap_open,
    /* fini_module */                       krb5_ldap_close,
    /* create */                            krb5_ldap_create,
    /* destroy */                           krb5_ldap_delete_realm_1,
    /* get_age */                           krb5_ldap_get_age,
    /* lock */                              krb5_ldap_lock,
    /* unlock */                            krb5_ldap_unlock,
    /* get_principal */                     krb5_ldap_get_principal,
    /* put_principal */                     krb5_ldap_put_principal,
    /* delete_principal */                  krb5_ldap_delete_principal,
    /* rename_principal */                  krb5_ldap_rename_principal,
    /* iterate */                           krb5_ldap_iterate,
    /* create_policy */                     krb5_ldap_create_password_policy,
    /* get_policy */                        krb5_ldap_get_password_policy,
    /* put_policy */                        krb5_ldap_put_password_policy,
    /* iter_policy */                       krb5_ldap_iterate_password_policy,
    /* delete_policy */                     krb5_ldap_delete_password_policy,
    /* optional functions */
    /* fetch_master_key */                  NULL /* krb5_ldap_fetch_mkey */,
    /* fetch_master_key_list */             NULL,
    /* store_master_key_list */             NULL,
    /* Search enc type */                   NULL,
    /* Change pwd   */                      NULL,
    /* promote_db */                        NULL,
    /* decrypt_key_data */                  NULL,
    /* encrypt_key_data */                  NULL,
    /* sign_authdata */                     NULL,
    /* check_transited_realms */            NULL,
    /* check_policy_as */                   krb5_ldap_check_policy_as,
    /* check_policy_tgs */                  NULL,
    /* audit_as_req */                      krb5_ldap_audit_as_req,
    /* refresh_config */                    NULL,
    /* check_allowed_to_delegate */         krb5_ldap_check_allowed_to_delegate

};
