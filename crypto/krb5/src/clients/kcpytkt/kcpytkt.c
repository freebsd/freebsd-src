/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <krb5.h>
#include "k5-platform.h"

static char *prog;
static int quiet = 0;

static void
xusage()
{
    fprintf(stderr, "xusage: %s [-c from_ccache] [-e etype] [-f flags] "
            "dest_ccache service1 service2 ...\n", prog);
    exit(1);
}

static void
do_kcpytkt(int argc, char *argv[], char *fromccachestr, char *etypestr,
           int flags);

int
main(int argc, char *argv[])
{
    int option;
    char *etypestr = NULL, *fromccachestr = NULL;
    int flags = 0;

    prog = strrchr(argv[0], '/');
    prog = (prog != NULL) ? prog + 1 : argv[0];

    while ((option = getopt(argc, argv, "c:e:f:hq")) != -1) {
        switch (option) {
        case 'c':
            fromccachestr = optarg;
            break;
        case 'e':
            etypestr = optarg;
            break;
        case 'f':
            flags = atoi(optarg);
            break;
        case 'q':
            quiet = 1;
            break;
        case 'h':
        default:
            xusage();
            break;
        }
    }

    if (argc - optind < 2)
        xusage();

    do_kcpytkt(argc - optind, argv + optind, fromccachestr, etypestr, flags);
    return 0;
}

static void
do_kcpytkt(int count, char *names[], const char *fromccachestr, char *etypestr,
           int flags)
{
    krb5_context context;
    krb5_error_code ret;
    krb5_enctype etype;
    krb5_ccache fromccache, destccache;
    krb5_principal me;
    krb5_creds in_creds, out_creds;
    int i, errors, retflags;
    char *princ;

    ret = krb5_init_context(&context);
    if (ret) {
        com_err(prog, ret, "while initializing krb5 library");
        exit(1);
    }
    if (etypestr != NULL) {
        ret = krb5_string_to_enctype(etypestr, &etype);
        if (ret) {
            com_err(prog, ret, "while converting etype");
            exit(1);
        }
        retflags = KRB5_TC_MATCH_SRV_NAMEONLY | KRB5_TC_SUPPORTED_KTYPES;
    } else {
        etype = 0;
        retflags = KRB5_TC_MATCH_SRV_NAMEONLY;
    }

    if (fromccachestr != NULL)
        ret = krb5_cc_resolve(context, fromccachestr, &fromccache);
    else
        ret = krb5_cc_default(context, &fromccache);
    if (ret) {
        com_err(prog, ret, "while opening source ccache");
        exit(1);
    }

    ret = krb5_cc_get_principal(context, fromccache, &me);
    if (ret) {
        com_err(prog, ret, "while getting client principal name");
        exit(1);
    }

    ret = krb5_cc_resolve(context, names[0], &destccache);
    if (ret) {
        com_err(prog, ret, "while opening destination cache");
        exit(1);
    }

    errors = 0;

    for (i = 1; i < count; i++) {
        memset(&in_creds, 0, sizeof(in_creds));

        in_creds.client = me;

        ret = krb5_parse_name(context, names[i], &in_creds.server);
        if (ret) {
            if (!quiet) {
                fprintf(stderr, "%s: %s while parsing principal name\n",
                        names[i], error_message(ret));
            }
            errors++;
            continue;
        }

        ret = krb5_unparse_name(context, in_creds.server, &princ);
        if (ret) {
            fprintf(stderr, "%s: %s while printing principal name\n",
                    names[i], error_message(ret));
            errors++;
            continue;
        }

        in_creds.keyblock.enctype = etype;

        ret = krb5_cc_retrieve_cred(context, fromccache, retflags,
                                    &in_creds, &out_creds);
        if (ret) {
            fprintf(stderr, "%s: %s while retrieving credentials\n",
                    princ, error_message(ret));
            krb5_free_unparsed_name(context, princ);
            errors++;
            continue;
        }

        ret = krb5_cc_store_cred(context, destccache, &out_creds);
        krb5_free_principal(context, in_creds.server);
        if (ret) {
            fprintf(stderr, "%s: %s while removing credentials\n",
                    princ, error_message(ret));
            krb5_free_cred_contents(context, &out_creds);
            krb5_free_unparsed_name(context, princ);
            errors++;
            continue;
        }

        krb5_free_unparsed_name(context, princ);
        krb5_free_cred_contents(context, &out_creds);
    }

    krb5_free_principal(context, me);
    krb5_cc_close(context, fromccache);
    krb5_cc_close(context, destccache);
    krb5_free_context(context);

    if (errors)
        exit(1);

    exit(0);
}
