/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/lmdb/klmdb-int.h - internal declarations for LMDB KDB module */
/*
 * Copyright (C) 2018 by the Massachusetts Institute of Technology.
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

#ifndef LMDB_INT_H
#define LMDB_INT_H

/* Length of a principal lockout record (three 32-bit fields) */
#define LOCKOUT_RECORD_LEN 12

krb5_error_code klmdb_encode_princ(krb5_context context,
                                   const krb5_db_entry *entry,
                                   uint8_t **enc_out, size_t *len_out);
void klmdb_encode_princ_lockout(krb5_context context,
                                const krb5_db_entry *entry,
                                uint8_t buf[LOCKOUT_RECORD_LEN]);
krb5_error_code klmdb_encode_policy(krb5_context context,
                                    const osa_policy_ent_rec *pol,
                                    uint8_t **enc_out, size_t *len_out);

krb5_error_code klmdb_decode_princ(krb5_context context,
                                   const void *key, size_t key_len,
                                   const void *enc, size_t enc_len,
                                   krb5_db_entry **entry_out);
void klmdb_decode_princ_lockout(krb5_context context, krb5_db_entry *entry,
                                const uint8_t buf[LOCKOUT_RECORD_LEN]);
krb5_error_code klmdb_decode_policy(krb5_context context,
                                    const void *key, size_t key_len,
                                    const void *enc, size_t enc_len,
                                    osa_policy_ent_t *pol_out);

krb5_error_code klmdb_lockout_check_policy(krb5_context context,
                                           krb5_db_entry *entry,
                                           krb5_timestamp stamp);
krb5_error_code klmdb_lockout_audit(krb5_context context, krb5_db_entry *entry,
                                    krb5_timestamp stamp,
                                    krb5_error_code status,
                                    krb5_boolean disable_last_success,
                                    krb5_boolean disable_lockout);
krb5_error_code klmdb_update_lockout(krb5_context context,
                                     krb5_db_entry *entry,
                                     krb5_timestamp stamp,
                                     krb5_boolean zero_fail_count,
                                     krb5_boolean set_last_success,
                                     krb5_boolean set_last_failure);

krb5_error_code klmdb_get_policy(krb5_context context, char *name,
                                 osa_policy_ent_t *policy);

#endif /* LMDB_INT_H */
