/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/ldap/libkdb_ldap/ldap_tkt_policy.h */
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

#ifndef _LDAP_POLICY_H
#define _LDAP_POLICY_H 1

/* policy specific mask */

#define LDAP_POLICY_MAXTKTLIFE        0x0001
#define LDAP_POLICY_MAXRENEWLIFE      0x0002
#define LDAP_POLICY_TKTFLAGS          0x0004
#define LDAP_POLICY_COUNT             0x0008
/* policy object structure */

typedef struct _krb5_ldap_policy_params {
    char                  *policy;
    long                  mask;
    long                  maxtktlife;
    long                  maxrenewlife;
    long                  tktflags;
    krb5_tl_data          *tl_data;
}krb5_ldap_policy_params;

krb5_error_code
krb5_ldap_create_policy(krb5_context, krb5_ldap_policy_params *, int);

krb5_error_code
krb5_ldap_modify_policy(krb5_context, krb5_ldap_policy_params *, int);

krb5_error_code
krb5_ldap_read_policy(krb5_context, char *, krb5_ldap_policy_params **, int *);

krb5_error_code
krb5_ldap_delete_policy(krb5_context, char *);

krb5_error_code
krb5_ldap_clear_policy(krb5_context, char *);

krb5_error_code
krb5_ldap_list_policy(krb5_context, char *, char ***);

krb5_error_code
krb5_ldap_free_policy(krb5_context, krb5_ldap_policy_params *);

krb5_error_code
krb5_ldap_change_count(krb5_context, char *, int);

#endif
