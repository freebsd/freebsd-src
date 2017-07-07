/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* clients/kswitch/kswitch.c - Switch primary credential cache */
/*
 * Copyright 2011 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "k5-int.h"
#include <locale.h>

extern int optind;
extern char *optarg;

#ifndef _WIN32
#define GET_PROGNAME(x) (strrchr((x), '/') ? strrchr((x), '/')+1 : (x))
#else
#define GET_PROGNAME(x) max(max(strrchr((x), '/'), strrchr((x), '\\')) + 1,(x))
#endif

static char *progname;

static void
usage(void)
{
    fprintf(stderr, _("Usage: %s {-c cache_name | -p principal}\n"), progname);
    fprintf(stderr, _("\t-c specify name of credentials cache\n"));
    fprintf(stderr, _("\t-p specify name of principal\n"));
    exit(2);
}

int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_error_code ret;
    int c;
    krb5_ccache cache = NULL;
    krb5_principal princ = NULL;
    const char *cache_name = NULL, *princ_name = NULL;
    krb5_boolean errflag = FALSE;

    setlocale(LC_ALL, "");
    progname = GET_PROGNAME(argv[0]);

    while ((c = getopt(argc, argv, "c:p:")) != -1) {
        switch (c) {
        case 'c':
        case 'p':
            if (cache_name || princ_name) {
                fprintf(stderr, _("Only one -c or -p option allowed\n"));
                errflag = TRUE;
            } else if (c == 'c') {
                cache_name = optarg;
            } else {
                princ_name = optarg;
            }
            break;
        case '?':
        default:
            errflag = TRUE;
            break;
        }
    }

    if (optind != argc)
        errflag = TRUE;

    if (!cache_name && !princ_name) {
        fprintf(stderr, _("One of -c or -p must be specified\n"));
        errflag = TRUE;
    }

    if (errflag)
        usage();

    ret = krb5_init_context(&context);
    if (ret) {
        com_err(progname, ret, _("while initializing krb5"));
        exit(1);
    }

    if (cache_name) {
        ret = krb5_cc_resolve(context, cache_name, &cache);
        if (ret != 0) {
            com_err(progname, ret, _("while resolving %s"), cache_name);
            exit(1);
        }
    } else {
        ret = krb5_parse_name(context, princ_name, &princ);
        if (ret) {
            com_err(progname, ret, _("while parsing principal name %s"),
                    princ_name);
            exit(1);
        }
        ret = krb5_cc_cache_match(context, princ, &cache);
        if (ret) {
            com_err(progname, ret, _("while searching for ccache for %s"),
                    princ_name);
            exit(1);
        }
        krb5_free_principal(context, princ);
    }

    ret = krb5_cc_switch(context, cache);
    if (ret != 0) {
        com_err(progname, ret, _("while switching to credential cache"));
        exit(1);
    }

    krb5_cc_close(context, cache);
    krb5_free_context(context);
    return 0;
}
