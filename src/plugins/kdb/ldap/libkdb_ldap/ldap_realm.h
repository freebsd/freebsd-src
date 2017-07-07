/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/ldap/libkdb_ldap/ldap_realm.h */
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

#ifndef _LDAP_REALM_H
#define _LDAP_REALM_H 1

/* realm specific mask */
#define LDAP_REALM_SUBTREE            0x0001
#define LDAP_REALM_SEARCHSCOPE        0x0002
/* 0x0004 was LDAP_REALM_POLICYREFERENCE but it was unused */
#define LDAP_REALM_UPENABLED          0x0008
#define LDAP_REALM_LDAPSERVERS        0x0010
#define LDAP_REALM_KDCSERVERS         0x0020
#define LDAP_REALM_ADMINSERVERS       0x0040
#define LDAP_REALM_PASSWDSERVERS      0x0080
#define LDAP_REALM_MAXTICKETLIFE      0x0100
#define LDAP_REALM_MAXRENEWLIFE       0x0200
#define LDAP_REALM_KRBTICKETFLAGS     0x0400
#define LDAP_REALM_CONTREF            0x0800

extern char *policy_attributes[];

extern char *realm_attributes[];

/* realm container structure */

typedef struct _krb5_ldap_realm_params {
    char          *realmdn;
    char          *realm_name;
    char          **subtree;
    char          *containerref;
    int           search_scope;
    int           upenabled;
    int           subtreecount;
    krb5_int32    max_life;
    krb5_int32    max_renewable_life;
    krb5_int32    tktflags;
    char          **kdcservers;
    char          **adminservers;
    char          **passwdservers;
    krb5_tl_data  *tl_data;
    long          mask;
} krb5_ldap_realm_params;


krb5_error_code
krb5_ldap_list_realm(krb5_context , char ***);

krb5_error_code
krb5_ldap_delete_realm(krb5_context, char *);

krb5_error_code
krb5_ldap_modify_realm(krb5_context, krb5_ldap_realm_params *, int);

krb5_error_code
krb5_ldap_create_realm(krb5_context, krb5_ldap_realm_params *, int);

krb5_error_code
krb5_ldap_read_realm_params(krb5_context, char *, krb5_ldap_realm_params **,
                            int *);

void
krb5_ldap_free_realm_params(krb5_ldap_realm_params *);

krb5_error_code
krb5_ldap_delete_realm_1(krb5_context, char *, char **);

char *
ldap_filter_correct(char *);

#endif
