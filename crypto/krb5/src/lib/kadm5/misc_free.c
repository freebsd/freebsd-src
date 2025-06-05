/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 */

#include "k5-int.h"
#include        <kadm5/admin.h>
#include        <stdlib.h>
#include        "server_internal.h"

kadm5_ret_t
kadm5_free_policy_ent(void *server_handle, kadm5_policy_ent_t val)
{
    krb5_tl_data *tl_next;

    _KADM5_CHECK_HANDLE(server_handle);

    if (val == NULL)
        return KADM5_OK;

    free(val->policy);
    free(val->allowed_keysalts);
    for (; val->tl_data; val->tl_data = tl_next) {
        tl_next = val->tl_data->tl_data_next;
        free(val->tl_data->tl_data_contents);
        free(val->tl_data);
    }
    memset(val, 0, sizeof(*val));
    return KADM5_OK;
}

kadm5_ret_t
kadm5_free_name_list(void *server_handle, char **names, int count)
{
    _KADM5_CHECK_HANDLE(server_handle);

    while (count--)
        free(names[count]);
    free(names);
    return KADM5_OK;
}

/* XXX this ought to be in libkrb5.a, but isn't */
kadm5_ret_t krb5_free_key_data_contents(context, key)
    krb5_context context;
    krb5_key_data *key;
{
    int i, idx;

    idx = (key->key_data_ver == 1 ? 1 : 2);
    for (i = 0; i < idx; i++)
        zapfree(key->key_data_contents[i], key->key_data_length[i]);
    return KADM5_OK;
}

kadm5_ret_t kadm5_free_key_data(void *server_handle,
                                krb5_int16 *n_key_data,
                                krb5_key_data *key_data)
{
    kadm5_server_handle_t      handle = server_handle;
    int i, nkeys = (int) *n_key_data;

    _KADM5_CHECK_HANDLE(server_handle);

    if (key_data == NULL)
        return KADM5_OK;

    for (i = 0; i < nkeys; i++)
        krb5_free_key_data_contents(handle->context, &key_data[i]);
    free(key_data);
    return KADM5_OK;
}

kadm5_ret_t
kadm5_free_principal_ent(void *server_handle, kadm5_principal_ent_t val)
{
    kadm5_server_handle_t handle = server_handle;
    krb5_tl_data *tl;
    int i;

    _KADM5_CHECK_HANDLE(server_handle);

    if (!val)
        return KADM5_OK;

    krb5_free_principal(handle->context, val->principal);
    krb5_free_principal(handle->context, val->mod_name);
    free(val->policy);
    if (val->n_key_data) {
        for (i = 0; i < val->n_key_data; i++)
            krb5_free_key_data_contents(handle->context, &val->key_data[i]);
        free(val->key_data);
    }

    while (val->tl_data) {
        tl = val->tl_data->tl_data_next;
        free(val->tl_data->tl_data_contents);
        free(val->tl_data);
        val->tl_data = tl;
    }
    return KADM5_OK;
}

kadm5_ret_t
kadm5_free_strings(void *server_handle, krb5_string_attr *strings,
                   int count)
{
    int i;

    _KADM5_CHECK_HANDLE(server_handle);

    if (!strings)
        return KADM5_OK;

    for (i = 0; i < count; i++) {
        free(strings[i].key);
        free(strings[i].value);
    }
    free(strings);
    return KADM5_OK;
}

kadm5_ret_t
kadm5_free_kadm5_key_data(krb5_context context, int n_key_data,
                          kadm5_key_data *key_data)
{
    int i;

    if (key_data == NULL)
        return KADM5_OK;

    for (i = 0; i < n_key_data; i++) {
        krb5_free_keyblock_contents(context, &key_data[i].key);
        krb5_free_data_contents(context, &key_data[i].salt.data);
    }
    free(key_data);

    return KADM5_OK;
}
