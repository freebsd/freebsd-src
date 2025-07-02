/* -*- mode: c; indent-tabs-mode: nil -*- */
/* tests/gssapi/t_s4u2proxy_deleg.c - Test S4U2Proxy after krb5 auth */
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
#include <string.h>

#include "common.h"

/*
 * Usage: ./t_s4u2proxy_krb5 [--spnego] client_cache storage_cache
 *                                      [accname|-] service1 service2
 *
 * This program performs a regular Kerberos or SPNEGO authentication from the
 * default principal of client_cache to service1.  If that authentication
 * yields delegated credentials, the program stores those credentials in
 * sorage_ccache and uses that cache to perform a second authentication to
 * service2 using S4U2Proxy.
 *
 * The default keytab must contain keys for service1 and service2.  The default
 * ccache must contain a TGT for service1.  This program assumes that krb5 or
 * SPNEGO authentication requires only one token exchange.
 */

int
main(int argc, char *argv[])
{
    const char *client_ccname, *storage_ccname, *accname, *service1, *service2;
    krb5_context context = NULL;
    krb5_error_code ret;
    krb5_boolean use_spnego = FALSE;
    krb5_ccache storage_ccache = NULL;
    krb5_principal client_princ = NULL;
    OM_uint32 minor, major, flags;
    gss_buffer_desc buf = GSS_C_EMPTY_BUFFER;
    gss_OID mech;
    gss_OID_set mechs;
    gss_name_t acceptor_name = GSS_C_NO_NAME, client_name = GSS_C_NO_NAME;
    gss_name_t service1_name = GSS_C_NO_NAME, service2_name = GSS_C_NO_NAME;
    gss_cred_id_t service1_cred = GSS_C_NO_CREDENTIAL;
    gss_cred_id_t deleg_cred = GSS_C_NO_CREDENTIAL;
    gss_ctx_id_t initiator_context, acceptor_context;

    /* Parse arguments. */
    if (argc >= 2 && strcmp(argv[1], "--spnego") == 0) {
        use_spnego = TRUE;
        argc--;
        argv++;
    }
    if (argc != 6) {
        fprintf(stderr, "./t_s4u2proxy_krb5 [--spnego] client_ccache "
                "storage_ccache [accname|-] service1 service2\n");
        return 1;
    }
    client_ccname = argv[1];
    storage_ccname = argv[2];
    accname = argv[3];
    service1 = argv[4];
    service2 = argv[5];

    mech = use_spnego ? &mech_spnego : &mech_krb5;
    mechs = use_spnego ? &mechset_spnego : &mechset_krb5;
    ret = krb5_init_context(&context);
    check_k5err(context, "krb5_init_context", ret);

    /* Get GSS_C_BOTH acceptor credentials, using the default ccache. */
    acceptor_name = GSS_C_NO_NAME;
    if (strcmp(accname, "-") != 0)
        acceptor_name = import_name(service1);
    major = gss_acquire_cred(&minor, acceptor_name, GSS_C_INDEFINITE,
                             mechs, GSS_C_BOTH, &service1_cred, NULL, NULL);
    check_gsserr("gss_acquire_cred(service1)", major, minor);

    /* Establish contexts using the client ccache. */
    service1_name = import_name(service1);
    major = gss_krb5_ccache_name(&minor, client_ccname, NULL);
    check_gsserr("gss_krb5_ccache_name(1)", major, minor);
    flags = GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG;
    establish_contexts(mech, GSS_C_NO_CREDENTIAL, service1_cred, service1_name,
                       flags, &initiator_context, &acceptor_context,
                       &client_name, NULL, &deleg_cred);

    /* Display and remember the client principal. */
    major = gss_display_name(&minor, client_name, &buf, NULL);
    check_gsserr("gss_display_name(1)", major, minor);
    printf("auth1: %.*s\n", (int)buf.length, (char *)buf.value);
    /* Assumes buffer is null-terminated, which in our implementation it is. */
    ret = krb5_parse_name(context, buf.value, &client_princ);
    check_k5err(context, "krb5_parse_name", ret);
    (void)gss_release_buffer(&minor, &buf);

    if (deleg_cred == GSS_C_NO_CREDENTIAL) {
        printf("no credential delegated.\n");
        goto cleanup;
    }

    /* Take the opportunity to test cred export/import on the synthesized
     * S4U2Proxy delegated cred. */
    export_import_cred(&deleg_cred);

    /* Store the delegated credentials. */
    ret = krb5_cc_resolve(context, storage_ccname, &storage_ccache);
    check_k5err(context, "krb5_cc_resolve", ret);
    ret = krb5_cc_initialize(context, storage_ccache, client_princ);
    check_k5err(context, "krb5_cc_initialize", ret);
    major = gss_krb5_copy_ccache(&minor, deleg_cred, storage_ccache);
    check_gsserr("gss_krb5_copy_ccache", major, minor);
    ret = krb5_cc_close(context, storage_ccache);
    check_k5err(context, "krb5_cc_close", ret);

    (void)gss_delete_sec_context(&minor, &initiator_context, GSS_C_NO_BUFFER);
    (void)gss_delete_sec_context(&minor, &acceptor_context, GSS_C_NO_BUFFER);
    (void)gss_release_name(&minor, &client_name);
    (void)gss_release_cred(&minor, &deleg_cred);

    /* Establish contexts using the storage ccache. */
    service2_name = import_name(service2);
    major = gss_krb5_ccache_name(&minor, storage_ccname, NULL);
    check_gsserr("gss_krb5_ccache_name(2)", major, minor);
    establish_contexts(mech, GSS_C_NO_CREDENTIAL, GSS_C_NO_CREDENTIAL,
                       service2_name, flags, &initiator_context,
                       &acceptor_context, &client_name, NULL, &deleg_cred);

    major = gss_display_name(&minor, client_name, &buf, NULL);
    check_gsserr("gss_display_name(2)", major, minor);
    printf("auth2: %.*s\n", (int)buf.length, (char *)buf.value);
    (void)gss_release_buffer(&minor, &buf);

cleanup:
    (void)gss_release_name(&minor, &acceptor_name);
    (void)gss_release_name(&minor, &client_name);
    (void)gss_release_name(&minor, &service1_name);
    (void)gss_release_name(&minor, &service2_name);
    (void)gss_release_cred(&minor, &service1_cred);
    (void)gss_release_cred(&minor, &deleg_cred);
    (void)gss_delete_sec_context(&minor, &initiator_context, GSS_C_NO_BUFFER);
    (void)gss_delete_sec_context(&minor, &acceptor_context, GSS_C_NO_BUFFER);
    krb5_free_principal(context, client_princ);
    krb5_free_context(context);
    return 0;
}
