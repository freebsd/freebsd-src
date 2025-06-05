/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
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

#include <stdio.h>
#include <stdlib.h>

#include "common.h"

/* Display a usage error message and exit. */
static void
usage(void)
{
    fprintf(stderr, "Usage: t_export_cred [-k|-s] [-i initiatorname] "
            "[-a acceptorname] targetname\n");
    exit(1);
}

int
main(int argc, char *argv[])
{
    OM_uint32 major, minor, flags;
    gss_name_t initiator_name = GSS_C_NO_NAME, acceptor_name = GSS_C_NO_NAME;
    gss_name_t target_name;
    gss_cred_id_t initiator_cred, acceptor_cred, delegated_cred;
    gss_ctx_id_t initiator_context = GSS_C_NO_CONTEXT;
    gss_ctx_id_t acceptor_context = GSS_C_NO_CONTEXT;
    gss_OID mech = GSS_C_NO_OID;
    gss_OID_set mechs = GSS_C_NO_OID_SET;
    char optchar;

    /* Parse arguments. */
    argv++;
    while (*argv != NULL && **argv == '-') {
        optchar = (*argv)[1];
        argv++;
        if (optchar == 'i') {
            if (*argv == NULL)
                usage();
            initiator_name = import_name(*argv++);
        } else if (optchar == 'a') {
            if (*argv == NULL)
                usage();
            acceptor_name = import_name(*argv++);
        } else if (optchar == 'k') {
            mech = &mech_krb5;
            mechs = &mechset_krb5;
        } else if (optchar == 's') {
            mech = &mech_spnego;
            mechs = &mechset_spnego;
        } else {
            usage();
        }
    }
    if (*argv == NULL || *(argv + 1) != NULL)
        usage();
    target_name = import_name(argv[0]);

    /* Get initiator cred and export/import it. */
    major = gss_acquire_cred(&minor, initiator_name, GSS_C_INDEFINITE, mechs,
                             GSS_C_INITIATE, &initiator_cred, NULL, NULL);
    check_gsserr("gss_acquire_cred(initiator)", major, minor);
    export_import_cred(&initiator_cred);

    /* Get acceptor cred and export/import it. */
    major = gss_acquire_cred(&minor, acceptor_name, GSS_C_INDEFINITE, mechs,
                             GSS_C_ACCEPT, &acceptor_cred, NULL, NULL);
    check_gsserr("gss_acquire_cred(acceptor)", major, minor);
    export_import_cred(&acceptor_cred);

    /* Initiate and accept a security context (one-token exchange only),
     * delegating credentials. */
    flags = GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG | GSS_C_CONF_FLAG |
        GSS_C_INTEG_FLAG | GSS_C_DELEG_FLAG;
    establish_contexts(mech, initiator_cred, acceptor_cred, target_name, flags,
                       &initiator_context, &acceptor_context, NULL, NULL,
                       &delegated_cred);

    /* Import, release, export, and store delegated creds */
    export_import_cred(&delegated_cred);
    major = gss_store_cred(&minor, delegated_cred, GSS_C_INITIATE,
                           GSS_C_NULL_OID, 1, 1, NULL, NULL);
    check_gsserr("gss_store_cred", major, minor);

    (void)gss_release_name(&minor, &initiator_name);
    (void)gss_release_name(&minor, &acceptor_name);
    (void)gss_release_name(&minor, &target_name);
    (void)gss_release_cred(&minor, &initiator_cred);
    (void)gss_release_cred(&minor, &acceptor_cred);
    (void)gss_release_cred(&minor, &delegated_cred);
    (void)gss_delete_sec_context(&minor, &initiator_context, NULL);
    (void)gss_delete_sec_context(&minor, &acceptor_context, NULL);
    return 0;
}
