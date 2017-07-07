/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include <stdio.h>
#include <kadm5/admin.h>
#include <string.h>

int main(int argc, char **argv)
{
    kadm5_ret_t ret;
    void *server_handle;
    char **names;
    int count, princ, i;
    krb5_context context;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s [-princ|-pol] exp\n", argv[0]);
        exit(1);
    }
    princ = (strcmp(argv[1], "-princ") == 0);

    ret = kadm5_init_krb5_context(&context);
    if (ret != KADM5_OK) {
        com_err("iter-test", ret, "while initializing context");
        exit(1);
    }
    ret = kadm5_init("admin", "admin", KADM5_ADMIN_SERVICE, 0,
                     KADM5_STRUCT_VERSION, KADM5_API_VERSION_4, NULL,
                     &server_handle);
    if (ret != KADM5_OK) {
        com_err("iter-test", ret, "while initializing");
        exit(1);
    }

    if (princ)
        ret = kadm5_get_principals(server_handle, argv[2], &names, &count);
    else
        ret = kadm5_get_policies(server_handle, argv[2], &names, &count);

    if (ret != KADM5_OK) {
        com_err("iter-test", ret, "while retrieving list");
        exit(1);
    }

    for (i = 0; i < count; i++)
        printf("%d: %s\n", i, names[i]);

    kadm5_free_name_list(server_handle, names, count);

    (void) kadm5_destroy(server_handle);

    return 0;
}
