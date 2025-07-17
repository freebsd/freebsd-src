/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/gssapi/t_inq_cred.c - Test program for gss_inquire_cred behavior */
/*
 * Copyright 2012 by the Massachusetts Institute of Technology.
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

/*
 * Test program for gss_inquire_cred, intended to be run from a Python test
 * script.  Acquires credentials, inquires them, and prints the resulting name
 * and lifetime.
 *
 * Usage: ./t_inq_cred [-k|-s] [-a|-b|-i] [initiatorname]
 *
 * By default no mechanism is specified when acquiring credentials; -k
 * indicates the krb5 mech and -s indicates SPNEGO.  By default or with -i,
 * initiator credentials are acquired; -a indicates acceptor credentials and -b
 * indicates credentials of both types.  The credential is acquired with no
 * name by default; a krb5 principal name or host-based name (prefixed with
 * "gss:") may be supplied as an argument.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static void
usage(void)
{
    fprintf(stderr,
            "Usage: t_inq_cred [-k|-s] [-a|-b|-i] [princ|gss:service@host]\n");
    exit(1);
}

int
main(int argc, char *argv[])
{
    OM_uint32 minor, major, lifetime;
    gss_cred_usage_t cred_usage = GSS_C_INITIATE;
    gss_OID_set mechs = GSS_C_NO_OID_SET;
    gss_cred_id_t cred = GSS_C_NO_CREDENTIAL;
    gss_name_t name = GSS_C_NO_NAME;
    gss_buffer_desc buf;
    const char *name_arg = NULL;
    char opt;

    while (argc > 1 && argv[1][0] == '-') {
        opt = argv[1][1];
        argc--, argv++;
        if (opt == 'a')
            cred_usage = GSS_C_ACCEPT;
        else if (opt == 'b')
            cred_usage = GSS_C_BOTH;
        else if (opt == 'i')
            cred_usage = GSS_C_INITIATE;
        else if (opt == 'k')
            mechs = &mechset_krb5;
        else if (opt == 's')
            mechs = &mechset_spnego;
        else
            usage();
    }
    if (argc > 2)
        usage();
    if (argc > 1)
        name_arg = argv[1];

    /* Import the name, if given. */
    if (name_arg != NULL)
        name = import_name(name_arg);

    /* Acquire a credential. */
    major = gss_acquire_cred(&minor, name, GSS_C_INDEFINITE, mechs, cred_usage,
                             &cred, NULL, NULL);
    check_gsserr("gss_acquire_cred", major, minor);

    /* Inquire about the credential. */
    (void)gss_release_name(&minor, &name);
    major = gss_inquire_cred(&minor, cred, &name, &lifetime, NULL, NULL);
    check_gsserr("gss_inquire_cred", major, minor);

    /* Get a display form of the name. */
    buf.value = NULL;
    buf.length = 0;
    major = gss_display_name(&minor, name, &buf, NULL);
    check_gsserr("gss_display_name", major, minor);

    printf("name: %.*s\n", (int)buf.length, (char *)buf.value);
    printf("lifetime: %d\n", (int)lifetime);

    (void)gss_release_cred(&minor, &cred);
    (void)gss_release_name(&minor, &name);
    (void)gss_release_buffer(&minor, &buf);
    return 0;
}
