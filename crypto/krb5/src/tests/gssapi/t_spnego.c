/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2010  by the Massachusetts Institute of Technology.
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
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common.h"

static gss_OID_desc mech_krb5_wrong = {
    9, "\052\206\110\202\367\022\001\002\002"
};
gss_OID_set_desc mechset_krb5_wrong = { 1, &mech_krb5_wrong };

/*
 * Test program for SPNEGO and gss_set_neg_mechs
 *
 * Example usage:
 *
 * kinit testuser
 * ./t_spnego host/test.host@REALM testhost.keytab
 */

/* Replace *tok and *len with the concatenation of prefix and *tok. */
static void
prepend(const void *prefix, size_t plen, uint8_t **tok, size_t *len)
{
    uint8_t *newtok;

    newtok = malloc(plen + *len);
    assert(newtok != NULL);
    memcpy(newtok, prefix, plen);
    memcpy(newtok + plen, *tok, *len);
    free(*tok);
    *tok = newtok;
    *len = plen + *len;
}

/* Replace *tok and *len with *tok wrapped in a DER tag with the given tag
 * byte.  *len must be less than 2^16. */
static void
der_wrap(uint8_t tag, uint8_t **tok, size_t *len)
{
    char lenbuf[3];
    uint8_t *wrapped;
    size_t llen;

    if (*len < 128) {
        lenbuf[0] = *len;
        llen = 1;
    } else if (*len < 256) {
        lenbuf[0] = 0x81;
        lenbuf[1] = *len;
        llen = 2;
    } else {
        assert(*len >> 16 == 0);
        lenbuf[0] = 0x82;
        lenbuf[1] = *len >> 8;
        lenbuf[2] = *len & 0xFF;
        llen = 3;
    }
    wrapped = malloc(1 + llen + *len);
    assert(wrapped != NULL);
    *wrapped = tag;
    memcpy(wrapped + 1, lenbuf, llen);
    memcpy(wrapped + 1 + llen, *tok, *len);
    free(*tok);
    *tok = wrapped;
    *len = 1 + llen + *len;
}

/*
 * Create a SPNEGO initiator token for the erroneous Microsoft krb5 mech OID,
 * wrapping a krb5 token ktok.  The token should look like:
 *
 * 60 <len> (GSS framing sequence)
 *   06 06 2B 06 01 05 05 02 (SPNEGO OID)
 *   A0 <len> (NegotiationToken choice 0, negTokenInit)
 *     30 <len> (sequence)
 *       A0 0D (context tag 0, mechTypes)
 *         30 0B (sequence of)
 *           06 09 2A 86 48 82 F7 12 01 02 02 (wrong krb5 OID)
 *       A2 <len> (context tag 2, mechToken)
 *         04 <len> (octet string)
 *           <mech token octets>
 */
static void
create_mskrb5_spnego_token(gss_buffer_t ktok, gss_buffer_desc *tok_out)
{
    uint8_t *tok;
    size_t len;

    len = ktok->length;
    tok = malloc(len);
    assert(tok != NULL);
    memcpy(tok, ktok->value, len);
    /* Wrap the krb5 token in OCTET STRING and [2] tags. */
    der_wrap(0x04, &tok, &len);
    der_wrap(0xA2, &tok, &len);
    /* Prepend the wrong krb5 OID inside OBJECT IDENTIFIER and [0] tags. */
    prepend("\xA0\x0D\x30\x0B\x06\x09\x2A\x86\x48\x82\xF7\x12\x01\x02\x02", 15,
            &tok, &len);
    /* Wrap the previous two things in SEQUENCE and [0] tags. */
    der_wrap(0x30, &tok, &len);
    der_wrap(0xA0, &tok, &len);
    /* Prepend the SPNEGO OID in an OBJECT IDENTIFIER tag. */
    prepend("\x06\x06\x2B\x06\x01\x05\x05\x02", 8, &tok, &len);
    /* Wrap the whole thing in an [APPLICATION 0] tag. */
    der_wrap(0x60, &tok, &len);
    tok_out->value = tok;
    tok_out->length = len;
}

/*
 * Test that the SPNEGO acceptor code accepts and properly reflects back the
 * erroneous Microsoft mech OID in the supportedMech field of the NegTokenResp
 * message.  Use acred as the verifier cred handle.
 */
static void
test_mskrb_oid(gss_name_t tname, gss_cred_id_t acred)
{
    OM_uint32 major, minor;
    gss_ctx_id_t ictx = GSS_C_NO_CONTEXT, actx = GSS_C_NO_CONTEXT;
    gss_buffer_desc atok = GSS_C_EMPTY_BUFFER, ktok = GSS_C_EMPTY_BUFFER, stok;
    const unsigned char *atok_oid;

    /*
     * Our SPNEGO mech no longer acquires creds for the wrong mech OID, so we
     * have to construct a SPNEGO token ourselves.
     */
    major = gss_init_sec_context(&minor, GSS_C_NO_CREDENTIAL, &ictx, tname,
                                 &mech_krb5, 0, GSS_C_INDEFINITE,
                                 GSS_C_NO_CHANNEL_BINDINGS, &atok, NULL, &ktok,
                                 NULL, NULL);
    check_gsserr("gss_init_sec_context(mskrb)", major, minor);
    assert(major == GSS_S_COMPLETE);
    create_mskrb5_spnego_token(&ktok, &stok);

    /*
     * Look directly at the DER encoding of the response token.  Since we
     * didn't request mutual authentication, the SPNEGO reply will contain no
     * underlying mech token; therefore, the encoding of the correct
     * NegotiationToken response is completely predictable:
     *
     *   A1 14 (choice 1, length 20, meaning negTokenResp)
     *     30 12 (sequence, length 18)
     *       A0 03 (context tag 0, length 3)
     *         0A 01 00 (enumerated value 0, meaning accept-completed)
     *       A1 0B (context tag 1, length 11)
     *         06 09 (object identifier, length 9)
     *           2A 86 48 82 F7 12 01 02 02 (the erroneous krb5 OID)
     *
     * So we can just compare the length to 22 and the nine bytes at offset 13
     * to the expected OID.
     */
    major = gss_accept_sec_context(&minor, &actx, acred, &stok,
                                   GSS_C_NO_CHANNEL_BINDINGS, NULL,
                                   NULL, &atok, NULL, NULL, NULL);
    check_gsserr("gss_accept_sec_context(mskrb)", major, minor);
    assert(atok.length == 22);
    atok_oid = (unsigned char *)atok.value + 13;
    assert(memcmp(atok_oid, mech_krb5_wrong.elements, 9) == 0);

    (void)gss_delete_sec_context(&minor, &ictx, NULL);
    (void)gss_delete_sec_context(&minor, &actx, NULL);
    (void)gss_release_buffer(&minor, &ktok);
    (void)gss_release_buffer(&minor, &atok);
    free(stok.value);
}

/* Check that we return a compatibility NegTokenInit2 message containing
 * NegHints for an empty initiator token. */
static void
test_neghints()
{
    OM_uint32 major, minor;
    gss_buffer_desc itok = GSS_C_EMPTY_BUFFER, atok;
    gss_ctx_id_t actx = GSS_C_NO_CONTEXT;
    const char *expected =
        /* RFC 2743 token framing: [APPLICATION 0] IMPLICIT SEQUENCE followed
         * by OBJECT IDENTIFIER and the SPNEGO OID */
        "\x60\x47\x06\x06" "\x2B\x06\x01\x05\x05\x02"
        /* [0] SEQUENCE for the NegotiationToken negtokenInit choice */
        "\xA0\x3D\x30\x3B"
        /* [0] MechTypeList containing the krb5 OID */
        "\xA0\x0D\x30\x0B\x06\x09" "\x2A\x86\x48\x86\xF7\x12\x01\x02\x02"
        /* [3] NegHints containing [0] GeneralString containing the dummy
         * hintName string defined in [MS-SPNG] */
        "\xA3\x2A\x30\x28\xA0\x26\x1B\x24"
        "not_defined_in_RFC4178@please_ignore";

    /* Produce a hint token. */
    major = gss_accept_sec_context(&minor, &actx, GSS_C_NO_CREDENTIAL, &itok,
                                   GSS_C_NO_CHANNEL_BINDINGS, NULL, NULL,
                                   &atok, NULL, NULL, NULL);
    check_gsserr("gss_accept_sec_context(neghints)", major, minor);

    /* Verify it against the expected contents, which are fixed as long as we
     * only list the krb5 mech in the token. */
    assert(atok.length == strlen(expected));
    assert(memcmp(atok.value, expected, atok.length) == 0);

    (void)gss_release_buffer(&minor, &atok);
    (void)gss_delete_sec_context(&minor, &actx, NULL);
}

int
main(int argc, char *argv[])
{
    OM_uint32 minor, major, flags;
    gss_cred_id_t verifier_cred_handle = GSS_C_NO_CREDENTIAL;
    gss_cred_id_t initiator_cred_handle = GSS_C_NO_CREDENTIAL;
    gss_OID_set actual_mechs = GSS_C_NO_OID_SET;
    gss_ctx_id_t initiator_context, acceptor_context;
    gss_name_t target_name, source_name = GSS_C_NO_NAME;
    gss_OID mech = GSS_C_NO_OID;
    gss_OID_desc pref_oids[2];
    gss_OID_set_desc pref_mechs;

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s target_name [keytab]\n", argv[0]);
        exit(1);
    }

    target_name = import_name(argv[1]);

    if (argc >= 3) {
        major = krb5_gss_register_acceptor_identity(argv[2]);
        check_gsserr("krb5_gss_register_acceptor_identity", major, 0);
    }

    /* Get default initiator cred. */
    major = gss_acquire_cred(&minor, GSS_C_NO_NAME, GSS_C_INDEFINITE,
                             &mechset_spnego, GSS_C_INITIATE,
                             &initiator_cred_handle, NULL, NULL);
    check_gsserr("gss_acquire_cred(initiator)", major, minor);

    /*
     * The following test is designed to exercise SPNEGO reselection on the
     * client and server.  Unfortunately, it no longer does so after tickets
     * #8217 and #8021, since SPNEGO now only acquires a single krb5 cred and
     * there is no way to expand the underlying creds with gss_set_neg_mechs().
     * To fix this we need gss_acquire_cred_with_cred() or some other way to
     * turn a cred with a specifically requested mech set into a SPNEGO cred.
     */

    /* Make the initiator prefer IAKERB and offer krb5 as an alternative. */
    pref_oids[0] = mech_iakerb;
    pref_oids[1] = mech_krb5;
    pref_mechs.count = 2;
    pref_mechs.elements = pref_oids;
    major = gss_set_neg_mechs(&minor, initiator_cred_handle, &pref_mechs);
    check_gsserr("gss_set_neg_mechs(initiator)", major, minor);

    /* Get default acceptor cred. */
    major = gss_acquire_cred(&minor, GSS_C_NO_NAME, GSS_C_INDEFINITE,
                             &mechset_spnego, GSS_C_ACCEPT,
                             &verifier_cred_handle, &actual_mechs, NULL);
    check_gsserr("gss_acquire_cred(acceptor)", major, minor);

    /* Restrict the acceptor to krb5 (which will force a reselection). */
    major = gss_set_neg_mechs(&minor, verifier_cred_handle, &mechset_krb5);
    check_gsserr("gss_set_neg_mechs(acceptor)", major, minor);

    flags = GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG;
    establish_contexts(&mech_spnego, initiator_cred_handle,
                       verifier_cred_handle, target_name, flags,
                       &initiator_context, &acceptor_context, &source_name,
                       &mech, NULL);

    display_canon_name("Source name", source_name, &mech_krb5);
    display_oid("Source mech", mech);

    /* Test acceptance of the erroneous Microsoft krb5 OID, with and without an
     * acceptor cred. */
    test_mskrb_oid(target_name, verifier_cred_handle);
    test_mskrb_oid(target_name, GSS_C_NO_CREDENTIAL);

    test_neghints();

    (void)gss_delete_sec_context(&minor, &initiator_context, NULL);
    (void)gss_delete_sec_context(&minor, &acceptor_context, NULL);
    (void)gss_release_name(&minor, &source_name);
    (void)gss_release_name(&minor, &target_name);
    (void)gss_release_cred(&minor, &initiator_cred_handle);
    (void)gss_release_cred(&minor, &verifier_cred_handle);
    (void)gss_release_oid_set(&minor, &actual_mechs);

    return 0;
}
