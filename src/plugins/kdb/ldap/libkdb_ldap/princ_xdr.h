/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#ifndef _PRINC_XDR_H
#define _PRINC_XDR_H 1

#include <krb5.h>
#include <kdb.h>
#include <kadm5/server_internal.h>

void
ldap_osa_free_princ_ent(osa_princ_ent_t val);

krb5_error_code
krb5_lookup_tl_kadm_data(krb5_tl_data *tl_data,
                         osa_princ_ent_rec *princ_entry);

krb5_error_code
krb5_update_tl_kadm_data(krb5_context context, krb5_db_entry *entry,
                         osa_princ_ent_rec *princ_entry);

#endif
