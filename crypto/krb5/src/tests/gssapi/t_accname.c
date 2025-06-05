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

/*
 * Test program for acceptor names, intended to be run from a Python test
 * script.  Establishes contexts with the default initiator name, a specified
 * principal name as target name, and a specified host-based name as acceptor
 * name (or GSS_C_NO_NAME if no acceptor name is given).  If the exchange is
 * successful, queries the context for the acceptor name and prints it.  If any
 * call is unsuccessful, displays an error message.  Exits with status 0 if all
 * operations are successful, or 1 if not.
 *
 * Usage: ./t_accname targetname [acceptorname]
 */

int
main(int argc, char *argv[])
{
    OM_uint32 minor, major, flags;
    gss_cred_id_t acceptor_cred;
    gss_name_t target_name, acceptor_name = GSS_C_NO_NAME, real_acceptor_name;
    gss_buffer_desc namebuf;
    gss_ctx_id_t initiator_context, acceptor_context;

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s targetname [acceptorname]\n", argv[0]);
        return 1;
    }

    /* Import target and acceptor names. */
    target_name = import_name(argv[1]);
    if (argc >= 3)
        acceptor_name = import_name(argv[2]);

    /* Get acceptor cred. */
    major = gss_acquire_cred(&minor, acceptor_name, GSS_C_INDEFINITE,
                             GSS_C_NO_OID_SET, GSS_C_ACCEPT,
                             &acceptor_cred, NULL, NULL);
    check_gsserr("gss_acquire_cred", major, minor);

    flags = GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG;
    establish_contexts(&mech_krb5, GSS_C_NO_CREDENTIAL, acceptor_cred,
                       target_name, flags, &initiator_context,
                       &acceptor_context, NULL, NULL, NULL);

    major = gss_inquire_context(&minor, acceptor_context, NULL,
                                &real_acceptor_name, NULL, NULL, NULL, NULL,
                                NULL);
    check_gsserr("gss_inquire_context", major, minor);

    namebuf.value = NULL;
    namebuf.length = 0;
    major = gss_display_name(&minor, real_acceptor_name, &namebuf, NULL);
    check_gsserr("gss_display_name", major, minor);

    printf("%.*s\n", (int)namebuf.length, (char *)namebuf.value);

    (void)gss_release_name(&minor, &target_name);
    (void)gss_release_name(&minor, &acceptor_name);
    (void)gss_release_name(&minor, &real_acceptor_name);
    (void)gss_release_cred(&minor, &acceptor_cred);
    (void)gss_delete_sec_context(&minor, &initiator_context, NULL);
    (void)gss_delete_sec_context(&minor, &acceptor_context, NULL);
    (void)gss_release_buffer(&minor, &namebuf);
    return 0;
}
