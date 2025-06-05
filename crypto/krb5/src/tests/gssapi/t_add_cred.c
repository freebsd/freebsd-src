/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/gssapi/t_add_cred.c - gss_add_cred() tests */
/*
 * Copyright (C) 2018 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This program tests the mechglue behavior of gss_add_cred().  It relies on a
 * krb5 keytab and credentials being present so that initiator and acceptor
 * credentials can be acquired, but does not use them to initiate or accept any
 * requests.
 */

#include <stdio.h>
#include <assert.h>

#include "common.h"

int
main()
{
    OM_uint32 minor, major;
    gss_cred_id_t cred1, cred2;
    gss_cred_usage_t usage;
    gss_name_t name;

    /* Check that we get the expected error if we pass neither an input nor an
     * output cred handle. */
    major = gss_add_cred(&minor, GSS_C_NO_CREDENTIAL, GSS_C_NO_NAME,
                         &mech_krb5, GSS_C_INITIATE, GSS_C_INDEFINITE,
                         GSS_C_INDEFINITE, NULL, NULL, NULL, NULL);
    assert(major == (GSS_S_CALL_INACCESSIBLE_WRITE | GSS_S_NO_CRED));

    /* Regression test for #8737: make sure that desired_name is honored when
     * creating a credential by passing in a non-matching name. */
    name = import_name("p:does/not/match@WRONG_REALM");
    major = gss_add_cred(&minor, GSS_C_NO_CREDENTIAL, name, &mech_krb5,
                         GSS_C_INITIATE, GSS_C_INDEFINITE, GSS_C_INDEFINITE,
                         &cred1, NULL, NULL, NULL);
    assert(major == GSS_S_NO_CRED);
    gss_release_name(&minor, &name);

    /* Create cred1 with a krb5 initiator cred by passing an output handle but
     * no input handle. */
    major = gss_add_cred(&minor, GSS_C_NO_CREDENTIAL, GSS_C_NO_NAME,
                         &mech_krb5, GSS_C_INITIATE, GSS_C_INDEFINITE,
                         GSS_C_INDEFINITE, &cred1, NULL, NULL, NULL);
    assert(major == GSS_S_COMPLETE);

    /* Verify that cred1 has the expected mechanism creds. */
    major = gss_inquire_cred_by_mech(&minor, cred1, &mech_krb5, NULL, NULL,
                                     NULL, &usage);
    assert(major == GSS_S_COMPLETE && usage == GSS_C_INITIATE);
    major = gss_inquire_cred_by_mech(&minor, cred1, &mech_iakerb, NULL, NULL,
                                     NULL, &usage);
    assert(major == GSS_S_NO_CRED);

    /* Check that we get the expected error if we try to add another krb5 mech
     * cred to cred1. */
    major = gss_add_cred(&minor, cred1, GSS_C_NO_NAME, &mech_krb5,
                         GSS_C_INITIATE, GSS_C_INDEFINITE, GSS_C_INDEFINITE,
                         NULL, NULL, NULL, NULL);
    assert(major == GSS_S_DUPLICATE_ELEMENT);

    /* Add an IAKERB acceptor mech cred to cred1. */
    major = gss_add_cred(&minor, cred1, GSS_C_NO_NAME, &mech_iakerb,
                         GSS_C_ACCEPT, GSS_C_INDEFINITE, GSS_C_INDEFINITE,
                         NULL, NULL, NULL, NULL);
    assert(major == GSS_S_COMPLETE);

    /* Verify cred1 mechanism creds. */
    major = gss_inquire_cred_by_mech(&minor, cred1, &mech_krb5, NULL, NULL,
                                     NULL, &usage);
    assert(major == GSS_S_COMPLETE && usage == GSS_C_INITIATE);
    major = gss_inquire_cred_by_mech(&minor, cred1, &mech_iakerb, NULL, NULL,
                                     NULL, &usage);
    assert(major == GSS_S_COMPLETE && usage == GSS_C_ACCEPT);

    /* Start over with another new cred. */
    gss_release_cred(&minor, &cred1);
    major = gss_add_cred(&minor, GSS_C_NO_CREDENTIAL, GSS_C_NO_NAME,
                         &mech_krb5, GSS_C_ACCEPT, GSS_C_INDEFINITE,
                         GSS_C_INDEFINITE, &cred1, NULL, NULL, NULL);
    assert(major == GSS_S_COMPLETE);

    /* Create an expanded cred by passing both an output handle and an input
     * handle. */
    major = gss_add_cred(&minor, cred1, GSS_C_NO_NAME, &mech_iakerb,
                         GSS_C_INITIATE, GSS_C_INDEFINITE, GSS_C_INDEFINITE,
                         &cred2, NULL, NULL, NULL);
    assert(major == GSS_S_COMPLETE);

    /* Verify mechanism creds in cred1 and cred2. */
    major = gss_inquire_cred_by_mech(&minor, cred1, &mech_krb5, NULL, NULL,
                                     NULL, &usage);
    assert(major == GSS_S_COMPLETE && usage == GSS_C_ACCEPT);
    major = gss_inquire_cred_by_mech(&minor, cred1, &mech_iakerb, NULL, NULL,
                                     NULL, &usage);
    assert(major == GSS_S_NO_CRED);
    major = gss_inquire_cred_by_mech(&minor, cred2, &mech_krb5, NULL, NULL,
                                     NULL, &usage);
    assert(major == GSS_S_COMPLETE && usage == GSS_C_ACCEPT);
    major = gss_inquire_cred_by_mech(&minor, cred2, &mech_iakerb, NULL, NULL,
                                     NULL, &usage);
    assert(major == GSS_S_COMPLETE && usage == GSS_C_INITIATE);

    gss_release_cred(&minor, &cred1);
    gss_release_cred(&minor, &cred2);

    return 0;
}
