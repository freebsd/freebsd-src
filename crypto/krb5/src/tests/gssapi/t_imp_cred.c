/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/gssapi/t_imp_cred.c - krb5_gss_import_cred test harness */
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

/*
 * Test program for krb5_gss_import_cred, intended to be run from a Python test
 * script.  Creates an initiator credential for the default ccache and an
 * acceptor principal for the default keytab (possibly using a specified keytab
 * principal), and performs a one-token context exchange using a specified
 * target principal.  If the exchange is successful, queries the context for
 * the acceptor name and prints it.  If any call is unsuccessful, displays an
 * error message.  Exits with status 0 if all operations are successful, or 1
 * if not.
 *
 * Usage: ./t_imp_cred target-princ [keytab-princ]
 */

#include "k5-platform.h"
#include <krb5.h>

#include "common.h"

int
main(int argc, char *argv[])
{
    OM_uint32 minor, major, flags;
    gss_cred_id_t initiator_cred, acceptor_cred;
    gss_ctx_id_t initiator_context, acceptor_context;
    gss_name_t target_name;
    krb5_context context = NULL;
    krb5_ccache cc;
    krb5_keytab kt;
    krb5_principal princ = NULL;
    krb5_error_code ret;

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s targetname [acceptorprinc]\n", argv[0]);
        return 1;
    }

    /* Import the target name. */
    target_name = import_name(argv[1]);

    /* Acquire the krb5 objects we need. */
    ret = krb5_init_context(&context);
    check_k5err(context, "krb5_init_context", ret);
    ret = krb5_cc_default(context, &cc);
    check_k5err(context, "krb5_cc_default", ret);
    ret = krb5_kt_default(context, &kt);
    check_k5err(context, "krb5_kt_default", ret);
    if (argc >= 3) {
        ret = krb5_parse_name(context, argv[2], &princ);
        check_k5err(context, "krb5_parse_name", ret);
    }

    /* Get initiator cred. */
    major = gss_krb5_import_cred(&minor, cc, NULL, NULL, &initiator_cred);
    check_gsserr("gss_krb5_import_cred (initiator)", major, minor);

    /* Get acceptor cred. */
    major = gss_krb5_import_cred(&minor, NULL, princ, kt, &acceptor_cred);
    check_gsserr("gss_krb5_import_cred (acceptor)", major, minor);

    flags = GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG;
    establish_contexts(&mech_krb5, initiator_cred, acceptor_cred, target_name,
                       flags, &initiator_context, &acceptor_context, NULL,
                       NULL, NULL);

    krb5_cc_close(context, cc);
    krb5_kt_close(context, kt);
    krb5_free_principal(context, princ);
    krb5_free_context(context);
    (void)gss_release_name(&minor, &target_name);
    (void)gss_release_cred(&minor, &initiator_cred);
    (void)gss_release_cred(&minor, &acceptor_cred);
    (void)gss_delete_sec_context(&minor, &initiator_context, NULL);
    (void)gss_delete_sec_context(&minor, &acceptor_context, NULL);
    return 0;
}
