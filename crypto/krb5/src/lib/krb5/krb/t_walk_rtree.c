/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * t_walk_rtree.c --- test krb5_walk_realm_tree
 */

#include "k5-int.h"
#include <stdio.h>
#include "com_err.h"

int
main(int argc, char **argv)
{
    krb5_data client, server;
    char    realm_branch_char = '.';
    krb5_principal *tree, *p;
    char *name;
    krb5_error_code retval;
    krb5_context context;

    krb5_init_context(&context);

    if (argc < 3 || argc > 4) {
        fprintf(stderr,
                "Usage: %s client-realm server-realm [sep_char]\n",
                argv[0]);
        exit(99);
    }
    client.data = argv[1];
    client.length = strlen(client.data);

    server.data = argv[2];
    server.length = strlen(server.data);

    if (argc == 4)
        realm_branch_char = argv[3][0];

    retval = krb5_walk_realm_tree(context, &client, &server, &tree,
                                  realm_branch_char);
    if (retval) {
        com_err("krb5_walk_realm_tree", retval, " ");
        exit(1);
    }

    for (p = tree; *p; p++) {
        retval = krb5_unparse_name(context, *p, &name);
        if (retval) {
            com_err("krb5_unprase_name", retval, " ");
            exit(2);
        }
        printf("%s\n", name);
        free(name);
    }

    krb5_free_realm_tree(context, tree);
    krb5_free_context(context);

    exit(0);
}
