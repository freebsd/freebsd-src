/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include <kadm5/admin.h>
#include <com_err.h>
#include <stdio.h>
#include <krb5.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#define TEST_NUM    1000

int main()
{
    kadm5_ret_t ret;
    krb5_keyblock  *keys[TEST_NUM];
    krb5_principal tprinc;
    krb5_keyblock  *newkey;
    krb5_context context;
    void *server_handle;

    int    x, i;

    kadm5_init_krb5_context(&context);

    krb5_parse_name(context, "testuser", &tprinc);
    ret = kadm5_init(context, "admin", "admin", KADM5_ADMIN_SERVICE, NULL,
                     KADM5_STRUCT_VERSION, KADM5_API_VERSION_4, NULL,
                     &server_handle);
    if(ret != KADM5_OK) {
        com_err("test", ret, "init");
        exit(2);
    }
    for(x = 0; x < TEST_NUM; x++) {
        kadm5_randkey_principal(server_handle, tprinc, &keys[x], NULL);
        for(i = 0; i < x; i++) {
            if (!memcmp(newkey->contents, keys[i]->contents, newkey->length))
                puts("match found");
        }
    }
    kadm5_destroy(server_handle);
    exit(0);
}
