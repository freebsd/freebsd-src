/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include <stdio.h>
#include <krb5.h>
#include <kadm5/admin.h>
#include <kdb.h>
#include <string.h>

char *whoami;

static void usage()
{
    fprintf(stderr,
            "Usage: %s {shared|exclusive|permanent|release|"
            "get name|wait} ...\n", whoami);
    exit(1);
}

int main(int argc, char **argv)
{
    krb5_error_code ret;
    osa_policy_ent_t entry;
    krb5_context context;
    kadm5_config_params params;
    krb5_error_code kret;

    whoami = argv[0];

    kret = kadm5_init_krb5_context(&context);
    if (kret) {
        com_err(whoami, kret, "while initializing krb5");
        exit(1);
    }

    params.mask = 0;
    ret = kadm5_get_config_params(context, 1, &params, &params);
    if (ret) {
        com_err(whoami, ret, "while retrieving configuration parameters");
        exit(1);
    }
    if (! (params.mask & KADM5_CONFIG_ADBNAME)) {
        com_err(whoami, KADM5_BAD_SERVER_PARAMS,
                "while retrieving configuration parameters");
        exit(1);
    }

    ret = krb5_db_open( context, NULL, KRB5_KDB_OPEN_RW);
    if (ret) {
        com_err(whoami, ret, "while opening database");
        exit(1);
    }

    argc--; argv++;
    while (argc) {
        if (strcmp(*argv, "shared") == 0) {
            ret = krb5_db_lock(context, KRB5_DB_LOCKMODE_SHARED);
            if (ret)
                com_err(whoami, ret, "while getting shared lock");
            else
                printf("shared\n");
        } else if (strcmp(*argv, "exclusive") == 0) {
            ret = krb5_db_lock(context, KRB5_DB_LOCKMODE_EXCLUSIVE );
            if (ret)
                com_err(whoami, ret, "while getting exclusive lock");
            else
                printf("exclusive\n");
        } else if (strcmp(*argv, "permanent") == 0) {
            ret = krb5_db_lock(context, KRB5_DB_LOCKMODE_EXCLUSIVE );
            if (ret)
                com_err(whoami, ret, "while getting permanent lock");
            else
                printf("permanent\n");
        } else if (strcmp(*argv, "release") == 0) {
            ret = krb5_db_unlock(context);
            if (ret)
                com_err(whoami, ret, "while releasing lock");
            else
                printf("released\n");
        } else if (strcmp(*argv, "get") == 0) {
            argc--; argv++;
            if (!argc) usage();
            if ((ret = krb5_db_get_policy(context, *argv, &entry))) {
                com_err(whoami, ret, "while getting policy");
            } else {
                printf("retrieved\n");
                krb5_db_free_policy(context, entry);
            }
        } else if (strcmp(*argv, "wait") == 0) {
            getchar();
        } else {
            fprintf(stderr, "%s: Invalid argument \"%s\"\n",
                    whoami, *argv);
            usage();
        }

        argc--; argv++;
    }

    ret = krb5_db_fini(context);
    if (ret) {
        com_err(whoami, ret, "while closing database");
        exit(1);
    }

    return 0;
}
