/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2009  by the Massachusetts Institute of Technology.
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
 * Test program for protocol transition (S4U2Self) and constrained delegation
 * (S4U2Proxy)
 *
 * Note: because of name canonicalization, the following tips may help
 * when configuring with Active Directory:
 *
 * - Create a computer account FOO$
 * - Set the UPN to host/foo.domain (no suffix); this is necessary to
 *   be able to send an AS-REQ as this principal, otherwise you would
 *   need to use the canonical name (FOO$), which will cause principal
 *   comparison errors in gss_accept_sec_context().
 * - Add a SPN of host/foo.domain
 * - Configure the computer account to support constrained delegation with
 *   protocol transition (Trust this computer for delegation to specified
 *   services only / Use any authentication protocol)
 * - Add host/foo.domain to the keytab (possibly easiest to do this
 *   with ktadd)
 *
 * For S4U2Proxy to work the TGT must be forwardable too.
 *
 * Usage eg:
 *
 * kinit -k -t test.keytab -f 'host/test.win.mit.edu@WIN.MIT.EDU'
 * ./t_s4u p:delegtest@WIN.MIT.EDU p:HOST/WIN-EQ7E4AA2WR8.win.mit.edu@WIN.MIT.EDU test.keytab
 */

static int use_spnego = 0;

static void
test_prf(gss_ctx_id_t initiatorContext, gss_ctx_id_t acceptorContext,
         int flags)
{
    gss_buffer_desc constant;
    OM_uint32 major, minor;
    unsigned int i;
    gss_buffer_desc initiatorPrf;
    gss_buffer_desc acceptorPrf;

    constant.value = "gss prf test";
    constant.length = strlen((char *)constant.value);

    initiatorPrf.value = NULL;
    acceptorPrf.value = NULL;

    major = gss_pseudo_random(&minor, initiatorContext, flags, &constant, 19,
                              &initiatorPrf);
    check_gsserr("gss_pseudo_random", major, minor);

    printf("%s\n", flags == GSS_C_PRF_KEY_FULL ?
           "PRF_KEY_FULL" : "PRF_KEY_PARTIAL");

    printf("Initiator PRF: ");
    for (i = 0; i < initiatorPrf.length; i++)
        printf("%02x ", ((char *)initiatorPrf.value)[i] & 0xFF);
    printf("\n");

    major = gss_pseudo_random(&minor, acceptorContext, flags, &constant, 19,
                              &acceptorPrf);
    check_gsserr("gss_pseudo_random", major, minor);

    printf("Acceptor  PRF: ");
    for (i = 0; i < acceptorPrf.length; i++)
        printf("%02x ", ((char *)acceptorPrf.value)[i] & 0xFF);
    printf("\n");

    if (acceptorPrf.length != initiatorPrf.length ||
        memcmp(acceptorPrf.value, initiatorPrf.value, initiatorPrf.length)) {
        fprintf(stderr, "Initiator and acceptor PRF output does not match\n");
        exit(1);
    }

    (void)gss_release_buffer(&minor, &initiatorPrf);
    (void)gss_release_buffer(&minor, &acceptorPrf);
}

static void
init_accept_sec_context(gss_cred_id_t claimant_cred_handle,
                        gss_cred_id_t verifier_cred_handle,
                        gss_cred_id_t *deleg_cred_handle)
{
    OM_uint32 major, minor, flags;
    gss_name_t source_name = GSS_C_NO_NAME, target_name = GSS_C_NO_NAME;
    gss_ctx_id_t initiator_context, acceptor_context;
    gss_OID mech;

    *deleg_cred_handle = GSS_C_NO_CREDENTIAL;

    major = gss_inquire_cred(&minor, verifier_cred_handle, &target_name, NULL,
                             NULL, NULL);
    check_gsserr("gss_inquire_cred", major, minor);
    display_canon_name("Target name", target_name, &mech_krb5);

    mech = use_spnego ? &mech_spnego : &mech_krb5;
    display_oid("Target mech", mech);

    flags = GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG;
    establish_contexts(mech, claimant_cred_handle, verifier_cred_handle,
                       target_name, flags, &initiator_context,
                       &acceptor_context, &source_name, NULL,
                       deleg_cred_handle);

    test_prf(initiator_context, acceptor_context, GSS_C_PRF_KEY_FULL);
    test_prf(initiator_context, acceptor_context, GSS_C_PRF_KEY_PARTIAL);

    (void)gss_release_name(&minor, &source_name);
    (void)gss_delete_sec_context(&minor, &acceptor_context, NULL);
    (void)gss_delete_sec_context(&minor, &initiator_context, NULL);
}

static void
get_default_cred(const char *keytab_name, gss_OID_set mechs,
                 gss_cred_id_t *impersonator_cred_handle)
{
    OM_uint32 major = GSS_S_FAILURE, minor;
    krb5_error_code ret;
    krb5_context context = NULL;
    krb5_keytab keytab = NULL;
    krb5_principal keytab_principal = NULL;
    krb5_ccache ccache = NULL;

    if (keytab_name != NULL) {
        ret = krb5_init_context(&context);
        check_k5err(context, "krb5_init_context", ret);

        ret = krb5_kt_resolve(context, keytab_name, &keytab);
        check_k5err(context, "krb5_kt_resolve", ret);

        ret = krb5_cc_default(context, &ccache);
        check_k5err(context, "krb5_cc_default", ret);

        ret = krb5_cc_get_principal(context, ccache, &keytab_principal);
        check_k5err(context, "krb5_cc_get_principal", ret);

        major = gss_krb5_import_cred(&minor, ccache, keytab_principal, keytab,
                                     impersonator_cred_handle);
        check_gsserr("gss_krb5_import_cred", major, minor);

        krb5_free_principal(context, keytab_principal);
        krb5_cc_close(context, ccache);
        krb5_kt_close(context, keytab);
        krb5_free_context(context);
    } else {
        major = gss_acquire_cred(&minor, GSS_C_NO_NAME, GSS_C_INDEFINITE,
                                 mechs, GSS_C_BOTH, impersonator_cred_handle,
                                 NULL, NULL);
        check_gsserr("gss_acquire_cred", major, minor);
    }
}

int
main(int argc, char *argv[])
{
    OM_uint32 minor, major;
    gss_cred_id_t impersonator_cred_handle = GSS_C_NO_CREDENTIAL;
    gss_cred_id_t user_cred_handle = GSS_C_NO_CREDENTIAL;
    gss_cred_id_t delegated_cred_handle = GSS_C_NO_CREDENTIAL;
    gss_name_t user = GSS_C_NO_NAME, target = GSS_C_NO_NAME;
    gss_OID_set mechs, actual_mechs = GSS_C_NO_OID_SET;
    uid_t uid;

    if (argc < 2 || argc > 5) {
        fprintf(stderr, "Usage: %s [--spnego] [user] "
                "[proxy-target] [keytab]\n", argv[0]);
        fprintf(stderr, "       proxy-target and keytab are optional\n");
        exit(1);
    }

    if (strcmp(argv[1], "--spnego") == 0) {
        use_spnego++;
        argc--;
        argv++;
    }

    user = import_name(argv[1]);

    major = gss_pname_to_uid(&minor, user, NULL, &uid);
    check_gsserr("gss_pname_to_uid(user)", major, minor);

    if (argc > 2 && strcmp(argv[2], "-") != 0)
        target = import_name(argv[2]);

    mechs = use_spnego ? &mechset_spnego : &mechset_krb5;

    get_default_cred((argc > 3) ? argv[3] : NULL, mechs,
                     &impersonator_cred_handle);

    printf("Protocol transition tests follow\n");
    printf("-----------------------------------\n\n");

    /* get S4U2Self cred */
    major = gss_acquire_cred_impersonate_name(&minor, impersonator_cred_handle,
                                              user, GSS_C_INDEFINITE, mechs,
                                              GSS_C_INITIATE,
                                              &user_cred_handle, &actual_mechs,
                                              NULL);
    check_gsserr("gss_acquire_cred_impersonate_name", major, minor);

    /* Try to store it in default ccache */
    major = gss_store_cred(&minor, user_cred_handle, GSS_C_INITIATE,
                           &mechs->elements[0], 1, 1, NULL, NULL);
    check_gsserr("gss_store_cred", major, minor);

    init_accept_sec_context(user_cred_handle, impersonator_cred_handle,
                            &delegated_cred_handle);

    printf("\n");

    (void)gss_release_name(&minor, &user);
    (void)gss_release_name(&minor, &target);
    (void)gss_release_cred(&minor, &delegated_cred_handle);
    (void)gss_release_cred(&minor, &impersonator_cred_handle);
    (void)gss_release_cred(&minor, &user_cred_handle);
    (void)gss_release_oid_set(&minor, &actual_mechs);
    return 0;
}
