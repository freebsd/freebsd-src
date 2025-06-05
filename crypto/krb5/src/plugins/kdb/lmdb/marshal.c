/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/kdb/kdb_xdr.c */
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

#include "k5-int.h"
#include "k5-input.h"
#include <kdb.h>
#include "klmdb-int.h"

static void
put_tl_data(struct k5buf *buf, const krb5_tl_data *tl)
{
    for (; tl != NULL; tl = tl->tl_data_next) {
        k5_buf_add_uint16_le(buf, tl->tl_data_type);
        k5_buf_add_uint16_le(buf, tl->tl_data_length);
        k5_buf_add_len(buf, tl->tl_data_contents, tl->tl_data_length);
    }
}

krb5_error_code
klmdb_encode_princ(krb5_context context, const krb5_db_entry *entry,
                   uint8_t **enc_out, size_t *len_out)
{
    struct k5buf buf;
    const krb5_key_data *kd;
    int i, j;

    *enc_out = NULL;
    *len_out = 0;

    k5_buf_init_dynamic(&buf);

    k5_buf_add_uint32_le(&buf, entry->attributes);
    k5_buf_add_uint32_le(&buf, entry->max_life);
    k5_buf_add_uint32_le(&buf, entry->max_renewable_life);
    k5_buf_add_uint32_le(&buf, entry->expiration);
    k5_buf_add_uint32_le(&buf, entry->pw_expiration);
    k5_buf_add_uint16_le(&buf, entry->n_tl_data);
    k5_buf_add_uint16_le(&buf, entry->n_key_data);
    put_tl_data(&buf, entry->tl_data);
    for (i = 0; i < entry->n_key_data; i++) {
        kd = &entry->key_data[i];
        k5_buf_add_uint16_le(&buf, kd->key_data_ver);
        k5_buf_add_uint16_le(&buf, kd->key_data_kvno);
        for (j = 0; j < kd->key_data_ver; j++) {
            k5_buf_add_uint16_le(&buf, kd->key_data_type[j]);
            k5_buf_add_uint16_le(&buf, kd->key_data_length[j]);
            if (kd->key_data_length[j] > 0) {
                k5_buf_add_len(&buf, kd->key_data_contents[j],
                               kd->key_data_length[j]);
            }
        }
    }

    if (k5_buf_status(&buf) != 0)
        return ENOMEM;

    *enc_out = buf.data;
    *len_out = buf.len;
    return 0;
}

void
klmdb_encode_princ_lockout(krb5_context context, const krb5_db_entry *entry,
                           uint8_t buf[LOCKOUT_RECORD_LEN])
{
    store_32_le(entry->last_success, buf);
    store_32_le(entry->last_failed, buf + 4);
    store_32_le(entry->fail_auth_count, buf + 8);
}

krb5_error_code
klmdb_encode_policy(krb5_context context, const osa_policy_ent_rec *pol,
                    uint8_t **enc_out, size_t *len_out)
{
    struct k5buf buf;

    *enc_out = NULL;
    *len_out = 0;

    k5_buf_init_dynamic(&buf);
    k5_buf_add_uint32_le(&buf, pol->pw_min_life);
    k5_buf_add_uint32_le(&buf, pol->pw_max_life);
    k5_buf_add_uint32_le(&buf, pol->pw_min_length);
    k5_buf_add_uint32_le(&buf, pol->pw_min_classes);
    k5_buf_add_uint32_le(&buf, pol->pw_history_num);
    k5_buf_add_uint32_le(&buf, pol->pw_max_fail);
    k5_buf_add_uint32_le(&buf, pol->pw_failcnt_interval);
    k5_buf_add_uint32_le(&buf, pol->pw_lockout_duration);
    k5_buf_add_uint32_le(&buf, pol->attributes);
    k5_buf_add_uint32_le(&buf, pol->max_life);
    k5_buf_add_uint32_le(&buf, pol->max_renewable_life);

    if (pol->allowed_keysalts == NULL) {
        k5_buf_add_uint32_le(&buf, 0);
    } else {
        k5_buf_add_uint32_le(&buf, strlen(pol->allowed_keysalts));
        k5_buf_add(&buf, pol->allowed_keysalts);
    }

    k5_buf_add_uint16_le(&buf, pol->n_tl_data);
    put_tl_data(&buf, pol->tl_data);

    if (k5_buf_status(&buf) != 0)
        return ENOMEM;

    *enc_out = buf.data;
    *len_out = buf.len;
    return 0;
}

static krb5_error_code
get_tl_data(struct k5input *in, size_t count, krb5_tl_data **tl)
{
    krb5_error_code ret;
    const uint8_t *contents;
    size_t i, len;

    for (i = 0; i < count; i++) {
        *tl = k5alloc(sizeof(**tl), &ret);
        if (*tl == NULL)
            return ret;
        (*tl)->tl_data_type = k5_input_get_uint16_le(in);
        len = (*tl)->tl_data_length = k5_input_get_uint16_le(in);
        contents = k5_input_get_bytes(in, len);
        if (contents == NULL)
            return KRB5_KDB_TRUNCATED_RECORD;
        (*tl)->tl_data_contents = k5memdup(contents, len, &ret);
        if ((*tl)->tl_data_contents == NULL)
            return ret;
        tl = &(*tl)->tl_data_next;
    }

    return 0;
}

krb5_error_code
klmdb_decode_princ(krb5_context context, const void *key, size_t key_len,
                   const void *enc, size_t enc_len, krb5_db_entry **entry_out)
{
    krb5_error_code ret;
    struct k5input in;
    krb5_db_entry *entry = NULL;
    char *princname = NULL;
    const uint8_t *contents;
    int i, j;
    size_t len;
    krb5_key_data *kd;

    *entry_out = NULL;

    entry = k5alloc(sizeof(*entry), &ret);
    if (entry == NULL)
        goto cleanup;

    princname = k5memdup0(key, key_len, &ret);
    if (princname == NULL)
        goto cleanup;
    ret = krb5_parse_name(context, princname, &entry->princ);
    if (ret)
        goto cleanup;

    k5_input_init(&in, enc, enc_len);
    entry->attributes = k5_input_get_uint32_le(&in);
    entry->max_life = k5_input_get_uint32_le(&in);
    entry->max_renewable_life = k5_input_get_uint32_le(&in);
    entry->expiration = k5_input_get_uint32_le(&in);
    entry->pw_expiration = k5_input_get_uint32_le(&in);
    entry->n_tl_data = k5_input_get_uint16_le(&in);
    entry->n_key_data = k5_input_get_uint16_le(&in);
    if (entry->n_tl_data < 0 || entry->n_key_data < 0) {
        ret = KRB5_KDB_TRUNCATED_RECORD;
        goto cleanup;
    }

    ret = get_tl_data(&in, entry->n_tl_data, &entry->tl_data);
    if (ret)
        goto cleanup;

    if (entry->n_key_data > 0) {
        entry->key_data = k5calloc(entry->n_key_data, sizeof(*entry->key_data),
                                   &ret);
        if (entry->key_data == NULL)
            goto cleanup;
    }
    for (i = 0; i < entry->n_key_data; i++) {
        kd = &entry->key_data[i];
        kd->key_data_ver = k5_input_get_uint16_le(&in);
        kd->key_data_kvno = k5_input_get_uint16_le(&in);
        if (kd->key_data_ver < 0 &&
            kd->key_data_ver > KRB5_KDB_V1_KEY_DATA_ARRAY) {
            ret = KRB5_KDB_BAD_VERSION;
            goto cleanup;
        }
        for (j = 0; j < kd->key_data_ver; j++) {
            kd->key_data_type[j] = k5_input_get_uint16_le(&in);
            len = kd->key_data_length[j] = k5_input_get_uint16_le(&in);
            contents = k5_input_get_bytes(&in, len);
            if (contents == NULL) {
                ret = KRB5_KDB_TRUNCATED_RECORD;
                goto cleanup;
            }
            if (len > 0) {
                kd->key_data_contents[j] = k5memdup(contents, len, &ret);
                if (kd->key_data_contents[j] == NULL)
                    goto cleanup;
            }
        }
    }

    ret = in.status;
    if (ret)
        goto cleanup;

    entry->len = KRB5_KDB_V1_BASE_LENGTH;
    *entry_out = entry;
    entry = NULL;

cleanup:
    free(princname);
    krb5_db_free_principal(context, entry);
    return ret;
}

void
klmdb_decode_princ_lockout(krb5_context context, krb5_db_entry *entry,
                           const uint8_t buf[LOCKOUT_RECORD_LEN])
{
    entry->last_success = load_32_le(buf);
    entry->last_failed = load_32_le(buf + 4);
    entry->fail_auth_count = load_32_le(buf + 8);
}

krb5_error_code
klmdb_decode_policy(krb5_context context, const void *key, size_t key_len,
                    const void *enc, size_t enc_len, osa_policy_ent_t *pol_out)
{
    krb5_error_code ret;
    osa_policy_ent_t pol = NULL;
    struct k5input in;
    const char *str;
    size_t len;

    *pol_out = NULL;
    pol = k5alloc(sizeof(*pol), &ret);
    if (pol == NULL)
        goto error;

    pol->name = k5memdup0(key, key_len, &ret);
    if (pol->name == NULL)
        goto error;

    k5_input_init(&in, enc, enc_len);
    pol->pw_min_life = k5_input_get_uint32_le(&in);
    pol->pw_max_life = k5_input_get_uint32_le(&in);
    pol->pw_min_length = k5_input_get_uint32_le(&in);
    pol->pw_min_classes = k5_input_get_uint32_le(&in);
    pol->pw_history_num = k5_input_get_uint32_le(&in);
    pol->pw_max_fail = k5_input_get_uint32_le(&in);
    pol->pw_failcnt_interval = k5_input_get_uint32_le(&in);
    pol->pw_lockout_duration = k5_input_get_uint32_le(&in);
    pol->attributes = k5_input_get_uint32_le(&in);
    pol->max_life = k5_input_get_uint32_le(&in);
    pol->max_renewable_life = k5_input_get_uint32_le(&in);

    len = k5_input_get_uint32_le(&in);
    if (len > 0) {
        str = (char *)k5_input_get_bytes(&in, len);
        if (str == NULL) {
            ret = KRB5_KDB_TRUNCATED_RECORD;
            goto error;
        }
        pol->allowed_keysalts = k5memdup0(str, len, &ret);
        if (pol->allowed_keysalts == NULL)
            goto error;
    }

    pol->n_tl_data = k5_input_get_uint16_le(&in);
    ret = get_tl_data(&in, pol->n_tl_data, &pol->tl_data);
    if (ret)
        goto error;

    ret = in.status;
    if (ret)
        goto error;

    *pol_out = pol;
    return 0;

error:
    krb5_db_free_policy(context, pol);
    return ret;
}
