/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include <kadm5/admin.h>
#include "server_internal.h"

kadm5_ret_t kadm5_chpass_principal_util(void *server_handle,
                                        krb5_principal princ,
                                        char *new_pw,
                                        char **ret_pw,
                                        char *msg_ret,
                                        unsigned int msg_len)
{
    kadm5_server_handle_t handle = server_handle;

    CHECK_HANDLE(server_handle);
    return _kadm5_chpass_principal_util(handle, handle->lhandle, princ,
                                        new_pw, ret_pw, msg_ret, msg_len);
}
