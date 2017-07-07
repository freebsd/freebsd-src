/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include <kadm5/admin.h>
#include <com_err.h>
#include <stdio.h>
#include <krb5.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <unistd.h>
#include <netinet/in.h>
#ifdef CLIENT_TEST
#include <kadm5/client_internal.h>
#else
#include <kadm5/server_internal.h>
#include <kadm5/admin.h>
#endif

int main(int argc, char *argv[])
{
    kadm5_ret_t ret;
    void *server_handle;
    kadm5_server_handle_t handle;
    kadm5_server_handle_rec orig_handle;
    kadm5_policy_ent_rec       pol;
    kadm5_principal_ent_t    princ;
    kadm5_principal_ent_rec  kprinc;
    krb5_keyblock      *key;
    krb5_principal     tprinc;
    krb5_context       context;


    kadm5_init_krb5_context(&context);

    ret = kadm5_init(context, "admin/none", "admin", KADM5_ADMIN_SERVICE, NULL,
                     KADM5_STRUCT_VERSION, KADM5_API_VERSION_4, NULL,
                     &server_handle);
    if(ret != KADM5_OK) {
        com_err("test", ret, "init");
        exit(2);
    }
    handle = (kadm5_server_handle_t) server_handle;
    orig_handle = *handle;
    handle->magic_number = KADM5_STRUCT_VERSION;
    krb5_parse_name(context, "testuser", &tprinc);
    ret = kadm5_get_principal(server_handle, tprinc, &kprinc,
                              KADM5_PRINCIPAL_NORMAL_MASK);
    if(ret != KADM5_BAD_SERVER_HANDLE) {
        fprintf(stderr, "%s -- returned -- %s\n", "get-principal",
                error_message(ret));
        exit(1);
    }

    ret = kadm5_get_policy(server_handle, "pol1", &pol);
    if(ret != KADM5_BAD_SERVER_HANDLE) {
        fprintf(stderr, "%s -- returned -- %s\n", "get-policy",
                error_message(ret));
        exit(1);
    }

    princ = &kprinc;
    ret = kadm5_create_principal(server_handle, princ, KADM5_PRINCIPAL, "pass");
    if(ret != KADM5_BAD_SERVER_HANDLE) {
        fprintf(stderr, "%s -- returned -- %s\n", "create-principal",
                error_message(ret));
        exit(1);
    }

    ret = kadm5_create_policy(server_handle, &pol, KADM5_POLICY);
    if(ret != KADM5_BAD_SERVER_HANDLE) {
        fprintf(stderr, "%s -- returned -- %s\n", "create-policy",
                error_message(ret));
        exit(1);
    }

    ret = kadm5_modify_principal(server_handle, princ, KADM5_PW_EXPIRATION);
    if(ret != KADM5_BAD_SERVER_HANDLE) {
        fprintf(stderr, "%s -- returned -- %s\n", "modify-principal",
                error_message(ret));
        exit(1);
    }

    ret = kadm5_modify_policy(server_handle, &pol, KADM5_PW_MAX_LIFE);
    if(ret != KADM5_BAD_SERVER_HANDLE) {
        fprintf(stderr, "%s -- returned -- %s\n", "modify-policy",
                error_message(ret));
        exit(1);
    }

    ret = kadm5_delete_principal(server_handle, tprinc);
    if(ret != KADM5_BAD_SERVER_HANDLE) {
        fprintf(stderr, "%s -- returned -- %s\n", "delete-principal",
                error_message(ret));
        exit(1);
    }

    ret = kadm5_delete_policy(server_handle, "pol1");
    if(ret != KADM5_BAD_SERVER_HANDLE) {
        fprintf(stderr, "%s -- returned -- %s\n", "delete-policy",
                error_message(ret));
        exit(1);
    }

    ret = kadm5_chpass_principal(server_handle, tprinc, "FooBar");
    if(ret != KADM5_BAD_SERVER_HANDLE) {
        fprintf(stderr, "%s -- returned -- %s\n", "chpass",
                error_message(ret));
        exit(1);
    }
    ret = kadm5_randkey_principal(server_handle, tprinc, &key, NULL);
    if(ret != KADM5_BAD_SERVER_HANDLE) {
        fprintf(stderr, "%s -- returned -- %s\n", "randkey",
                error_message(ret));
        exit(1);
    }

    ret = kadm5_rename_principal(server_handle, tprinc, tprinc);
    if(ret != KADM5_BAD_SERVER_HANDLE) {
        fprintf(stderr, "%s -- returned -- %s\n", "rename",
                error_message(ret));
        exit(1);
    }

    ret = kadm5_destroy(server_handle);
    if(ret != KADM5_BAD_SERVER_HANDLE) {
        fprintf(stderr, "%s -- returned -- %s\n", "destroy",
                error_message(ret));
        exit(1);
    }

    *handle = orig_handle;
    ret = kadm5_destroy(server_handle);
    if (ret != KADM5_OK) {
        fprintf(stderr, "valid %s -- returned -- %s\n", "destroy",
                error_message(ret));
        exit(1);
    }

    krb5_free_principal(context, tprinc);
    krb5_free_context(context);
    exit(0);
}
