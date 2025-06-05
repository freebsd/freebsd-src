/* windows/ms2mit/ms2mit.c */
/*
 * Copyright (C) 2003 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include "krb5.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static char *prog;

static void
xusage(void)
{
    fprintf(stderr, "xusage: %s [-c ccache]\n", prog);
    exit(1);
}

/* Return true if princ is a local (not cross-realm) krbtgt principal. */
krb5_boolean
is_local_tgt(krb5_principal princ)
{
    return princ->length == 2 &&
        data_eq_string(princ->data[0], KRB5_TGS_NAME) &&
        data_eq(princ->realm, princ->data[1]);
}

/*
 * Check if a ccache has any tickets.
 */
static krb5_error_code
cc_has_tickets(krb5_context kcontext, krb5_ccache ccache, int *has_tickets)
{
    krb5_error_code code;
    krb5_cc_cursor cursor;
    krb5_creds creds;
    krb5_timestamp now = time(0);

    *has_tickets = 0;

    code = krb5_cc_set_flags(kcontext, ccache, KRB5_TC_NOTICKET);
    if (code)
        return code;

    code = krb5_cc_start_seq_get(kcontext, ccache, &cursor);
    if (code)
        return code;

    while (!*has_tickets) {
        code = krb5_cc_next_cred(kcontext, ccache, &cursor, &creds);
        if (code)
            break;

        if (!krb5_is_config_principal(kcontext, creds.server) &&
            ts_after(creds.times.endtime, now))
            *has_tickets = 1;

        krb5_free_cred_contents(kcontext, &creds);
    }
    krb5_cc_end_seq_get(kcontext, ccache, &cursor);

    return 0;
}

int
main(int argc, char *argv[])
{
    krb5_context kcontext = NULL;
    krb5_error_code code;
    krb5_ccache ccache=NULL;
    krb5_ccache mslsa_ccache=NULL;
    krb5_cc_cursor cursor;
    krb5_creds creds;
    krb5_principal princ = NULL;
    int found_tgt = 0;
    int has_tickets;
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
        goto cleanup;
    }

    if (code = krb5_cc_resolve(kcontext, "MSLSA:", &mslsa_ccache)) {
        com_err(argv[0], code, "while opening MS LSA ccache");
        goto cleanup;
    }

    /* Enumerate tickets from cache looking for a TGT */
    if ((code = krb5_cc_start_seq_get(kcontext, mslsa_ccache, &cursor))) {
        com_err(argv[0], code, "while initiating the cred sequence of MS LSA ccache");
        goto cleanup;
    }

    while (!found_tgt) {
        code = krb5_cc_next_cred(kcontext, mslsa_ccache, &cursor, &creds);
        if (code)
            break;

        /* Check if the ticket is a TGT */
        if (is_local_tgt(creds.server))
            found_tgt = 1;

        krb5_free_cred_contents(kcontext, &creds);
    }
    krb5_cc_end_seq_get(kcontext, mslsa_ccache, &cursor);

    if (!found_tgt) {
        fprintf(stderr, "%s: Initial Ticket Getting Tickets are not available from the MS LSA\n",
                argv[0]);
        /* Only set the LSA cache as the default if it actually has tickets. */
        code = cc_has_tickets(kcontext, mslsa_ccache, &has_tickets);
        if (code)
            goto cleanup;

        if (has_tickets)
            code = krb5int_cc_user_set_default_name(kcontext, "MSLSA:");

        goto cleanup;
    }

    if (code = krb5_cc_get_principal(kcontext, mslsa_ccache, &princ)) {
        com_err(argv[0], code, "while obtaining MS LSA principal");
        goto cleanup;
    }

    if (ccachestr)
        code = krb5_cc_resolve(kcontext, ccachestr, &ccache);
    else
        code = krb5_cc_resolve(kcontext, "API:", &ccache);
    if (code) {
        com_err(argv[0], code, "while getting default ccache");
        goto cleanup;
    }
    if (code = krb5_cc_initialize(kcontext, ccache, princ)) {
        com_err (argv[0], code, "when initializing ccache");
        goto cleanup;
    }

    if (code = krb5_cc_copy_creds(kcontext, mslsa_ccache, ccache)) {
        com_err (argv[0], code, "while copying MS LSA ccache to default ccache");
        goto cleanup;
    }

    /* Don't try and set the default cache if the cache name was specified. */
    if (ccachestr == NULL) {
        /* On success set the default cache to API. */
        code = krb5int_cc_user_set_default_name(kcontext, "API:");
        if (code) {
            com_err(argv[0], code, "when setting default to API");
            goto cleanup;
        }
    }

cleanup:
    krb5_free_principal(kcontext, princ);
    if (ccache != NULL)
        krb5_cc_close(kcontext, ccache);
    if (mslsa_ccache != NULL)
        krb5_cc_close(kcontext, mslsa_ccache);
    krb5_free_context(kcontext);
    return code ? 1 : 0;
}
