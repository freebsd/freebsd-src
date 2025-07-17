/* windows/ms2mit/mit2ms.c */
/*
 * Copyright (C) 2003,2004 by the Massachusetts Institute of Technology.
 * All rights reserved.
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

#include "krb5.h"
#include <stdio.h>
#include <string.h>
#include "k5-platform.h"

static char *prog;

static void
xusage(void)
{
    fprintf(stderr, "xusage: %s [-c ccache]\n", prog);
    exit(1);
}

void
main(
    int argc,
    char *argv[]
    )
{
    krb5_context kcontext;
    krb5_error_code code;
    krb5_ccache ccache=NULL;
    krb5_ccache mslsa_ccache=NULL;
    krb5_cc_cursor cursor;
    krb5_creds creds;
    krb5_principal princ;
    int initial_ticket = 0;
    int option;
    char * ccachestr = 0;

    prog = strrchr(argv[0], '/');
    prog = prog ? (prog + 1) : argv[0];

    while ((option = getopt(argc, argv, "c:h")) != -1) {
        switch (option) {
        case 'c':
            ccachestr = optarg;
            break;
        case 'h':
        default:
            xusage();
            break;
        }
    }

    if (code = krb5_init_context(&kcontext)) {
        com_err(argv[0], code, "while initializing kerberos library");
        exit(1);
    }

    if (ccachestr)
        code = krb5_cc_resolve(kcontext, ccachestr, &ccache);
    else
        code = krb5_cc_default(kcontext, &ccache);
    if (code) {
        com_err(argv[0], code, "while getting default ccache");
        krb5_free_principal(kcontext, princ);
        krb5_free_context(kcontext);
        exit(1);
    }

    /* Enumerate tickets from cache looking for an initial ticket */
    if ((code = krb5_cc_start_seq_get(kcontext, ccache, &cursor))) {
        com_err(argv[0], code, "while initiating the cred sequence of MS LSA ccache");
        krb5_cc_close(kcontext, ccache);
        krb5_free_context(kcontext);
        exit(1);
    }

    while (!(code = krb5_cc_next_cred(kcontext, ccache, &cursor, &creds)))
    {
        if ( creds.ticket_flags & TKT_FLG_INITIAL ) {
            krb5_free_cred_contents(kcontext, &creds);
            initial_ticket = 1;
            break;
        }
        krb5_free_cred_contents(kcontext, &creds);
    }
    krb5_cc_end_seq_get(kcontext, ccache, &cursor);

    if ( !initial_ticket ) {
        fprintf(stderr, "%s: Initial Ticket Getting Tickets are not available from the MIT default cache\n",
                argv[0]);
        krb5_cc_close(kcontext, ccache);
        krb5_free_context(kcontext);
        exit(1);
    }

    if (code = krb5_cc_get_principal(kcontext, ccache, &princ)) {
        com_err(argv[0], code, "while obtaining default MIT principal");
        krb5_cc_close(kcontext, ccache);
        krb5_free_context(kcontext);
        exit(1);
    }

    if (code = krb5_cc_resolve(kcontext, "MSLSA:", &mslsa_ccache)) {
        com_err(argv[0], code, "while opening MS LSA ccache");
        krb5_cc_close(kcontext, ccache);
        krb5_free_context(kcontext);
        exit(1);
    }

    if (code = krb5_cc_copy_creds(kcontext, ccache, mslsa_ccache)) {
        com_err (argv[0], code, "while copying default MIT ccache to MSLSA ccache");
        krb5_free_principal(kcontext, princ);
        krb5_cc_close(kcontext, ccache);
        krb5_cc_close(kcontext, mslsa_ccache);
        krb5_free_context(kcontext);
        exit(1);
    }

    krb5_free_principal(kcontext, princ);
    krb5_cc_close(kcontext, ccache);
    krb5_cc_close(kcontext, mslsa_ccache);
    krb5_free_context(kcontext);
    return(0);
}
