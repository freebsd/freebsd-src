/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include <kadm5/admin.h>
#include <com_err.h>
#include <stdio.h>
#include <krb5.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <unistd.h>
#include <netinet/in.h>
#include <kadm5/client_internal.h>
#include <string.h>

#define TEST_NUM    25

int main()
{
    kadm5_ret_t ret;
    char   *cp;
    int    x;
    void *server_handle;
    kadm5_server_handle_t handle;
    krb5_context context;

    ret = kadm5_init_krb5_context(&context);
    if (ret != 0) {
        com_err("test", ret, "context init");
        exit(2);
    }
    for(x = 0; x < TEST_NUM; x++) {
        ret = kadm5_init(context, "admin", "admin", KADM5_ADMIN_SERVICE, 0,
                         KADM5_STRUCT_VERSION, KADM5_API_VERSION_4, NULL,
                         &server_handle);
        if(ret != KADM5_OK) {
            com_err("test", ret, "init");
            exit(2);
        }
        handle = (kadm5_server_handle_t) server_handle;
        cp = strdup(strchr(handle->cache_name, ':') + 1);
        kadm5_destroy(server_handle);
        if(access(cp, F_OK) == 0) {
            puts("ticket cache not destroyed");
            exit(2);
        }
        free(cp);
    }
    krb5_free_context(context);
    exit(0);
}
