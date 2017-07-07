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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static int use_spnego = 0;

static void
test_greet_authz_data(gss_name_t *name)
{
    OM_uint32 major, minor;
    gss_buffer_desc attr;
    gss_buffer_desc value;
    gss_name_t canon;

    major = gss_canonicalize_name(&minor, *name, &mech_krb5, &canon);
    check_gsserr("gss_canonicalize_name", major, minor);

    attr.value = "greet:greeting";
    attr.length = strlen((char *)attr.value);

    value.value = "Hello, acceptor world!";
    value.length = strlen((char *)value.value);

    major = gss_set_name_attribute(&minor, canon, 1, &attr, &value);
    if (major == GSS_S_UNAVAILABLE) {
        (void)gss_release_name(&minor, &canon);
        return;
    }
    check_gsserr("gss_set_name_attribute", major, minor);
    gss_release_name(&minor, name);
    *name = canon;
}

static void
init_accept_sec_context(gss_cred_id_t claimant_cred_handle,
                        gss_cred_id_t verifier_cred_handle,
                        gss_cred_id_t *deleg_cred_handle)
{
    OM_uint32 major, minor, flags;
    gss_name_t source_name = GSS_C_NO_NAME, target_name = GSS_C_NO_NAME;
    gss_ctx_id_t initiator_context, acceptor_context;
    gss_OID mech = GSS_C_NO_OID;

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
                       &acceptor_context, &source_name, &mech,
                       deleg_cred_handle);

    display_canon_name("Source name", source_name, &mech_krb5);
    display_oid("Source mech", mech);
    enumerate_attributes(source_name, 1);

    (void)gss_release_name(&minor, &source_name);
    (void)gss_release_name(&minor, &target_name);
    (void)gss_delete_sec_context(&minor, &initiator_context, NULL);
    (void)gss_delete_sec_context(&minor, &acceptor_context, NULL);
}

static void
check_ticket_count(gss_cred_id_t cred, int expected)
{
    krb5_error_code ret;
    krb5_context context = NULL;
    krb5_creds kcred;
    krb5_cc_cursor cur;
    krb5_ccache ccache;
    int count = 0;
    gss_key_value_set_desc store;
    gss_key_value_element_desc elem;
    OM_uint32 major, minor;
    const char *ccname = "MEMORY:count";

    store.count = 1;
    store.elements = &elem;
    elem.key = "ccache";
    elem.value = ccname;
    major = gss_store_cred_into(&minor, cred, GSS_C_INITIATE, &mech_krb5, 1, 0,
                                &store, NULL, NULL);
    check_gsserr("gss_store_cred_into", major, minor);

    ret = krb5_init_context(&context);
    check_k5err(context, "krb5_init_context", ret);

    ret = krb5_cc_resolve(context, ccname, &ccache);
    check_k5err(context, "krb5_cc_resolve", ret);

    ret = krb5_cc_start_seq_get(context, ccache, &cur);
    check_k5err(context, "krb5_cc_start_seq_get", ret);

    while (!krb5_cc_next_cred(context, ccache, &cur, &kcred)) {
        if (!krb5_is_config_principal(context, kcred.server))
            count++;
        krb5_free_cred_contents(context, &kcred);
    }

    ret = krb5_cc_end_seq_get(context, ccache, &cur);
    check_k5err(context, "krb5_cc_end_seq_get", ret);

    if (expected != count) {
        printf("Expected %d tickets but got %d\n", expected, count);
        exit(1);
    }

    krb5_cc_destroy(context, ccache);
    krb5_free_context(context);
}

static void
constrained_delegate(gss_OID_set desired_mechs, gss_name_t target,
                     gss_cred_id_t delegated_cred_handle,
                     gss_cred_id_t verifier_cred_handle)
{
    OM_uint32 major, minor;
    gss_ctx_id_t initiator_context = GSS_C_NO_CONTEXT;
    gss_name_t cred_name = GSS_C_NO_NAME;
    OM_uint32 time_rec, lifetime;
    gss_cred_usage_t usage;
    gss_buffer_desc token;
    gss_OID_set mechs;

    printf("Constrained delegation tests follow\n");
    printf("-----------------------------------\n\n");

    if (gss_inquire_cred(&minor, verifier_cred_handle, &cred_name,
                         &lifetime, &usage, NULL) == GSS_S_COMPLETE) {
        display_canon_name("Proxy name", cred_name, &mech_krb5);
        (void)gss_release_name(&minor, &cred_name);
    }
    display_canon_name("Target name", target, &mech_krb5);
    if (gss_inquire_cred(&minor, delegated_cred_handle, &cred_name,
                         &lifetime, &usage, &mechs) == GSS_S_COMPLETE) {
        display_canon_name("Delegated name", cred_name, &mech_krb5);
        display_oid("Delegated mech", &mechs->elements[0]);
        (void)gss_release_name(&minor, &cred_name);
    }

    printf("\n");

    major = gss_init_sec_context(&minor, delegated_cred_handle,
                                 &initiator_context, target,
                                 mechs ? &mechs->elements[0] : &mech_krb5,
                                 GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG,
                                 GSS_C_INDEFINITE, GSS_C_NO_CHANNEL_BINDINGS,
                                 GSS_C_NO_BUFFER, NULL, &token, NULL,
                                 &time_rec);
    check_gsserr("gss_init_sec_context", major, minor);

    (void)gss_release_buffer(&minor, &token);
    (void)gss_delete_sec_context(&minor, &initiator_context, NULL);

    /* Ensure a second call does not acquire new ticket. */
    major = gss_init_sec_context(&minor, delegated_cred_handle,
                                 &initiator_context, target,
                                 mechs ? &mechs->elements[0] : &mech_krb5,
                                 GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG,
                                 GSS_C_INDEFINITE, GSS_C_NO_CHANNEL_BINDINGS,
                                 GSS_C_NO_BUFFER, NULL, &token, NULL,
                                 &time_rec);
    check_gsserr("gss_init_sec_context", major, minor);

    (void)gss_release_buffer(&minor, &token);
    (void)gss_delete_sec_context(&minor, &initiator_context, NULL);
    (void)gss_release_oid_set(&minor, &mechs);

    /* We expect three tickets: our TGT, the evidence ticket, and the ticket to
     * the target service. */
    check_ticket_count(delegated_cred_handle, 3);
}

int
main(int argc, char *argv[])
{
    OM_uint32 minor, major;
    gss_cred_id_t impersonator_cred_handle = GSS_C_NO_CREDENTIAL;
    gss_cred_id_t user_cred_handle = GSS_C_NO_CREDENTIAL;
    gss_cred_id_t delegated_cred_handle = GSS_C_NO_CREDENTIAL;
    gss_name_t user = GSS_C_NO_NAME, target = GSS_C_NO_NAME;
    gss_OID_set mechs;

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

    if (argc > 2 && strcmp(argv[2], "-"))
        target = import_name(argv[2]);

    if (argc > 3) {
        major = krb5_gss_register_acceptor_identity(argv[3]);
        check_gsserr("krb5_gss_register_acceptor_identity", major, 0);
    }

    /* Get default cred. */
    mechs = use_spnego ? &mechset_spnego : &mechset_krb5;
    major = gss_acquire_cred(&minor, GSS_C_NO_NAME, GSS_C_INDEFINITE, mechs,
                             GSS_C_BOTH, &impersonator_cred_handle, NULL,
                             NULL);
    check_gsserr("gss_acquire_cred", major, minor);

    printf("Protocol transition tests follow\n");
    printf("-----------------------------------\n\n");

    test_greet_authz_data(&user);

    /* Get S4U2Self cred. */
    major = gss_acquire_cred_impersonate_name(&minor, impersonator_cred_handle,
                                              user, GSS_C_INDEFINITE, mechs,
                                              GSS_C_INITIATE,
                                              &user_cred_handle, NULL, NULL);
    check_gsserr("gss_acquire_cred_impersonate_name", major, minor);

    init_accept_sec_context(user_cred_handle, impersonator_cred_handle,
                            &delegated_cred_handle);
    printf("\n");

    if (target != GSS_C_NO_NAME &&
        delegated_cred_handle != GSS_C_NO_CREDENTIAL) {
        constrained_delegate(mechs, target, delegated_cred_handle,
                             impersonator_cred_handle);
    } else if (target != GSS_C_NO_NAME) {
        fprintf(stderr, "Warning: no delegated cred handle returned\n\n");
        fprintf(stderr, "Verify:\n\n");
        fprintf(stderr, " - The TGT for the impersonating service is "
                "forwardable\n");
        fprintf(stderr, " - The T2A4D flag set on the impersonating service's "
                "UAC\n");
        fprintf(stderr, " - The user is not marked sensitive and cannot be "
                "delegated\n");
        fprintf(stderr, "\n");
    }

    (void)gss_release_name(&minor, &user);
    (void)gss_release_name(&minor, &target);
    (void)gss_release_cred(&minor, &delegated_cred_handle);
    (void)gss_release_cred(&minor, &impersonator_cred_handle);
    (void)gss_release_cred(&minor, &user_cred_handle);
    return 0;
}
