/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* clients/kdestroy/kdestroy.c - Destroy contents of credential cache */
/*
 * Copyright 1990 by the Massachusetts Institute of Technology.
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

#include "k5-platform.h"
#include <krb5.h>
#include <com_err.h>
#include <locale.h>
#include <string.h>
#include <stdio.h>

#ifdef __STDC__
#define BELL_CHAR '\a'
#else
#define BELL_CHAR '\007'
#endif

extern int optind;
extern char *optarg;

#ifndef _WIN32
#define GET_PROGNAME(x) (strrchr((x), '/') ? strrchr((x), '/')+1 : (x))
#else
#define GET_PROGNAME(x) max(max(strrchr((x), '/'), strrchr((x), '\\')) + 1,(x))
#endif

char *progname;


static void usage()
{
#define KRB_AVAIL_STRING(x) ((x)?"available":"not available")

    fprintf(stderr, _("Usage: %s [-A] [-q] [-c cache_name]\n"), progname);
    fprintf(stderr, _("\t-A destroy all credential caches in collection\n"));
    fprintf(stderr, _("\t-q quiet mode\n"));
    fprintf(stderr, _("\t-c specify name of credentials cache\n"));
    exit(2);
}

/* Print a warning if there are still un-destroyed caches in the collection. */
static void
print_remaining_cc_warning(krb5_context context)
{
    krb5_error_code retval;
    krb5_ccache cache;
    krb5_cccol_cursor cursor;

    retval = krb5_cccol_cursor_new(context, &cursor);
    if (retval) {
        com_err(progname, retval, _("while listing credential caches"));
        exit(1);
    }

    retval = krb5_cccol_cursor_next(context, cursor, &cache);
    if (retval == 0 && cache != NULL) {
        fprintf(stderr,
                _("Other credential caches present, use -A to destroy all\n"));
        krb5_cc_close(context, cache);
    }

    krb5_cccol_cursor_free(context, &cursor);
}

int
main(argc, argv)
    int argc;
    char **argv;
{
    krb5_context kcontext;
    krb5_error_code retval;
    int c;
    krb5_ccache cache = NULL;
    krb5_cccol_cursor cursor;
    char *cache_name = NULL;
    int code = 0;
    int errflg = 0;
    int quiet = 0;
    int all = 0;

    setlocale(LC_ALL, "");
    progname = GET_PROGNAME(argv[0]);

    while ((c = getopt(argc, argv, "54Aqc:")) != -1) {
        switch (c) {
        case 'A':
            all = 1;
            break;
        case 'q':
            quiet = 1;
            break;
        case 'c':
            if (cache_name) {
                fprintf(stderr, _("Only one -c option allowed\n"));
                errflg++;
            } else {
                cache_name = optarg;
            }
            break;
        case '4':
            fprintf(stderr, _("Kerberos 4 is no longer supported\n"));
            exit(3);
            break;
        case '5':
            break;
        case '?':
        default:
            errflg++;
            break;
        }
    }

    if (optind != argc)
        errflg++;

    if (errflg) {
        usage();
    }

    retval = krb5_init_context(&kcontext);
    if (retval) {
        com_err(progname, retval, _("while initializing krb5"));
        exit(1);
    }

    if (all) {
        code = krb5_cccol_cursor_new(kcontext, &cursor);
        if (code) {
            com_err(progname, code, _("while listing credential caches"));
            exit(1);
        }
        while ((code = krb5_cccol_cursor_next(kcontext, cursor,
                                              &cache)) == 0 && cache != NULL) {
            code = krb5_cc_get_full_name(kcontext, cache, &cache_name);
            if (code) {
                com_err(progname, code, _("composing ccache name"));
                exit(1);
            }
            code = krb5_cc_destroy(kcontext, cache);
            if (code && code != KRB5_FCC_NOFILE) {
                com_err(progname, code, _("while destroying cache %s"),
                        cache_name);
            }
            krb5_free_string(kcontext, cache_name);
        }
        krb5_cccol_cursor_free(kcontext, &cursor);
        krb5_free_context(kcontext);
        return 0;
    }

    if (cache_name) {
        code = krb5_cc_resolve (kcontext, cache_name, &cache);
        if (code != 0) {
            com_err(progname, code, _("while resolving %s"), cache_name);
            exit(1);
        }
    } else {
        code = krb5_cc_default(kcontext, &cache);
        if (code) {
            com_err(progname, code, _("while getting default ccache"));
            exit(1);
        }
    }

    code = krb5_cc_destroy (kcontext, cache);
    if (code != 0) {
        com_err (progname, code, _("while destroying cache"));
        if (code != KRB5_FCC_NOFILE) {
            if (quiet)
                fprintf(stderr, _("Ticket cache NOT destroyed!\n"));
            else {
                fprintf(stderr, _("Ticket cache %cNOT%c destroyed!\n"),
                        BELL_CHAR, BELL_CHAR);
            }
            errflg = 1;
        }
    }

    if (!quiet && !errflg)
        print_remaining_cc_warning(kcontext);

    krb5_free_context(kcontext);
    return errflg;
}
