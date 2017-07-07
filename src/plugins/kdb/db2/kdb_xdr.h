/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef _KDB2_XDR_H
#define _KDB2_XDR_H

#include "kdb.h"

krb5_error_code
krb5_encode_princ_dbkey(krb5_context context, krb5_data *key,
                        krb5_const_principal principal);

krb5_error_code
krb5_decode_princ_entry(krb5_context context, krb5_data *content,
                        krb5_db_entry **entry);

void
krb5_dbe_free(krb5_context context, krb5_db_entry *entry);

krb5_error_code
krb5_encode_princ_entry(krb5_context context, krb5_data *content,
                        krb5_db_entry *entry);

#endif
