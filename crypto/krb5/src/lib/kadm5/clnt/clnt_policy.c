/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 *
 * $Header$
 */

#include    <gssrpc/rpc.h>
#include    <kadm5/admin.h>
#include    <kadm5/kadm_rpc.h>
#include    "client_internal.h"
#include        <stdlib.h>
#include        <string.h>
#include        <errno.h>

kadm5_ret_t
kadm5_create_policy(void *server_handle,
                    kadm5_policy_ent_t policy, long mask)
{
    cpol_arg            arg;
    generic_ret         r = { 0, 0 };
    kadm5_server_handle_t handle = server_handle;

    CHECK_HANDLE(server_handle);

    if(policy == (kadm5_policy_ent_t) NULL)
        return EINVAL;

    arg.mask = mask;
    arg.api_version = handle->api_version;
    memcpy(&arg.rec, policy, sizeof(kadm5_policy_ent_rec));
    if (create_policy_2(&arg, &r, handle->clnt))
        return KADM5_RPC_ERROR;
    return r.code;
}

kadm5_ret_t
kadm5_delete_policy(void *server_handle, char *name)
{
    dpol_arg            arg;
    generic_ret         r = { 0, 0 };
    kadm5_server_handle_t handle = server_handle;

    CHECK_HANDLE(server_handle);

    if(name == NULL)
        return EINVAL;

    arg.name = name;
    arg.api_version = handle->api_version;

    if (delete_policy_2(&arg, &r, handle->clnt))
        return KADM5_RPC_ERROR;
    return r.code;
}

kadm5_ret_t
kadm5_modify_policy(void *server_handle,
                    kadm5_policy_ent_t policy, long mask)
{
    mpol_arg            arg;
    generic_ret         r = { 0, 0 };
    kadm5_server_handle_t handle = server_handle;

    CHECK_HANDLE(server_handle);

    if(policy == (kadm5_policy_ent_t) NULL)
        return EINVAL;

    arg.mask = mask;
    arg.api_version = handle->api_version;

    memcpy(&arg.rec, policy, sizeof(kadm5_policy_ent_rec));
    if (modify_policy_2(&arg, &r, handle->clnt))
        return KADM5_RPC_ERROR;
    return r.code;
}

kadm5_ret_t
kadm5_get_policy(void *server_handle, char *name, kadm5_policy_ent_t ent)
{
    gpol_arg        arg;
    gpol_ret        r;
    kadm5_server_handle_t handle = server_handle;

    memset(ent, 0, sizeof(*ent));

    CHECK_HANDLE(server_handle);

    arg.name = name;
    arg.api_version = handle->api_version;

    if(name == NULL)
        return EINVAL;

    memset(&r, 0, sizeof(gpol_ret));
    if (get_policy_2(&arg, &r, handle->clnt))
        return KADM5_RPC_ERROR;
    if (r.code == 0)
        memcpy(ent, &r.rec, sizeof(r.rec));
    return r.code;
}

kadm5_ret_t
kadm5_get_policies(void *server_handle,
                   char *exp, char ***pols, int *count)
{
    gpols_arg   arg;
    gpols_ret   r;
    kadm5_server_handle_t handle = server_handle;

    CHECK_HANDLE(server_handle);

    if(pols == NULL || count == NULL)
        return EINVAL;
    arg.exp = exp;
    arg.api_version = handle->api_version;
    memset(&r, 0, sizeof(gpols_ret));
    if (get_pols_2(&arg, &r, handle->clnt))
        return KADM5_RPC_ERROR;
    if (r.code == 0) {
        *count = r.count;
        *pols = r.pols;
    } else {
        *count = 0;
        *pols = NULL;
    }

    return r.code;
}
