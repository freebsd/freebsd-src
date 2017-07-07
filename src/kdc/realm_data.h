/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/realm_data.h */
/*
 * Copyright (C) 2012 by the Massachusetts Institute of Technology.
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

#ifndef REALM_DATA_H
#define REALM_DATA_H

typedef struct __kdc_realm_data {
    /*
     * General Kerberos per-realm data.
     */
    char *              realm_name;     /* Realm name                       */
/* XXX the real context should go away once the db_context is done.
 * The db_context is then associated with the realm keytab using
 * krb5_ktkdb_resolv(). There should be nothing in the context which
 * cannot span multiple realms -- proven */
    krb5_context        realm_context;  /* Context to be used for realm     */
    krb5_keytab         realm_keytab;   /* keytab to be used for this realm */
    char *              realm_hostbased; /* referral services for NT-UNKNOWN */
    char *              realm_no_referral; /* non-referral services         */
    /*
     * Database per-realm data.
     */
    char *              realm_stash;    /* Stash file name for realm        */
    char *              realm_mpname;   /* Master principal name for realm  */
    krb5_principal      realm_mprinc;   /* Master principal for realm       */
    /*
     * Note realm_mkey is mkey read from stash or keyboard and may not be the
     * latest.
     */
    krb5_keyblock       realm_mkey;     /* Master key for this realm        */
    /*
     * TGS per-realm data.
     */
    krb5_principal      realm_tgsprinc; /* TGS principal for this realm     */
    /*
     * Other per-realm data.
     */
    char                *realm_listen;  /* Per-realm KDC UDP listen */
    char                *realm_tcp_listen; /* Per-realm KDC TCP listen */
    /*
     * Per-realm parameters.
     */
    krb5_deltat         realm_maxlife;  /* Maximum ticket life for realm    */
    krb5_deltat         realm_maxrlife; /* Maximum renewable life for realm */
    krb5_boolean        realm_reject_bad_transit; /* Accept unverifiable transited_realm ? */
    krb5_boolean        realm_restrict_anon;  /* Anon to local TGT only */
    krb5_boolean        realm_assume_des_crc_sess;  /* Assume princs support des-cbc-crc for session keys */
} kdc_realm_t;

struct server_handle {
    kdc_realm_t **kdc_realmlist;
    int kdc_numrealms;
    krb5_context kdc_err_context;
};

kdc_realm_t *find_realm_data(struct server_handle *, char *, krb5_ui_4);
kdc_realm_t *setup_server_realm(struct server_handle *, krb5_principal);

/*
 * These macros used to refer to a global pointer to the active realm state
 * structure for a request.  They now refer to a local variable that must be
 * properly declared in each function that uses these macros.
 */
#define kdc_context                     kdc_active_realm->realm_context
#define tgs_server                      kdc_active_realm->realm_tgsprinc

#endif  /* REALM_DATA_H */
