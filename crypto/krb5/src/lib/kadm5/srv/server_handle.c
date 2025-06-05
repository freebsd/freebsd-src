/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include <krb5.h>
#include <kadm5/admin.h>
#include "server_internal.h"

int _kadm5_check_handle(void *handle)
{
    CHECK_HANDLE(handle);
    return 0;
}
