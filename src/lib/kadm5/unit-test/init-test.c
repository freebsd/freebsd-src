/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include <kadm5/admin.h>
#include <com_err.h>
#include <stdio.h>
#include <stdlib.h>
#include <krb5.h>
#include <string.h>

int main()
{
    kadm5_ret_t ret;
    void *server_handle;
    kadm5_config_params params;
    krb5_context context;

    memset(&params, 0, sizeof(params));
    params.mask |= KADM5_CONFIG_NO_AUTH;
    ret = kadm5_init_krb5_context(&context);
    if (ret != 0) {
        com_err("init-test", ret, "while initializing krb5 context");
        exit(1);
    }
    ret = kadm5_init(context, "admin", "admin", NULL, &params,
                     KADM5_STRUCT_VERSION, KADM5_API_VERSION_4, NULL,
                     &server_handle);
    if (!ret)
        (void)kadm5_destroy(server_handle);
    krb5_free_context(context);
    if (ret == KADM5_RPC_ERROR) {
        exit(0);
    }
    else if (ret != 0) {
        com_err("init-test", ret, "while initializing without auth");
        exit(1);
    } else {
        fprintf(stderr, "Unexpected success while initializing without auth!\n");
        exit(1);
    }
}
