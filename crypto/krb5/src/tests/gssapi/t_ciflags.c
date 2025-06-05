/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/gssapi/t_ciflags.c - GSS_KRB5_CRED_NO_CI_FLAGS_X tests */
/*
 * Copyright (C) 2015 by the Massachusetts Institute of Technology.
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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "common.h"

static void
flagtest(gss_OID mech, gss_cred_id_t icred, gss_name_t tname,
         OM_uint32 inflags, OM_uint32 expflags)
{
    gss_ctx_id_t ictx, actx;
    OM_uint32 major, minor, flags;

    establish_contexts(mech, icred, GSS_C_NO_CREDENTIAL, tname, inflags, &ictx,
                       &actx, NULL, NULL, NULL);

    major = gss_inquire_context(&minor, actx, NULL, NULL, NULL, NULL, &flags,
                                NULL, NULL);
    check_gsserr("gss_inquire_context", major, minor);
    assert(flags == expflags);

    (void)gss_delete_sec_context(&minor, &ictx, NULL);
    (void)gss_delete_sec_context(&minor, &actx, NULL);
}

int
main(int argc, char *argv[])
{
    OM_uint32 minor, major;
    gss_cred_id_t icred;
    gss_name_t tname;
    gss_buffer_desc empty_buffer = GSS_C_EMPTY_BUFFER;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s targetname\n", argv[0]);
        return 1;
    }
    tname = import_name(argv[1]);

    /* With no flags, the initiator asserts conf, integ, trans */
    flagtest(&mech_krb5, GSS_C_NO_CREDENTIAL, tname, 0,
             GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG | GSS_C_TRANS_FLAG);
    flagtest(&mech_spnego, GSS_C_NO_CREDENTIAL, tname, 0,
             GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG | GSS_C_TRANS_FLAG);

    /* The initiator also asserts most flags specified by the caller. */
    flagtest(&mech_krb5, GSS_C_NO_CREDENTIAL, tname, GSS_C_SEQUENCE_FLAG,
             GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG | GSS_C_TRANS_FLAG |
             GSS_C_SEQUENCE_FLAG);
    flagtest(&mech_spnego, GSS_C_NO_CREDENTIAL, tname, GSS_C_SEQUENCE_FLAG,
             GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG | GSS_C_TRANS_FLAG |
             GSS_C_SEQUENCE_FLAG);

    /* Get a normal initiator cred and re-test with no flags. */
    major = gss_acquire_cred(&minor, GSS_C_NO_NAME, GSS_C_INDEFINITE,
                             GSS_C_NO_OID_SET, GSS_C_INITIATE, &icred, NULL,
                             NULL);
    check_gsserr("gss_acquire_cred", major, minor);
    flagtest(&mech_krb5, icred, tname, 0,
             GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG | GSS_C_TRANS_FLAG);
    flagtest(&mech_spnego, icred, tname, 0,
             GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG | GSS_C_TRANS_FLAG);

    /* Suppress confidentiality and integrity flags on the initiator cred and
     * check that they are suppressed, but can still be asserted explicitly. */
    major = gss_set_cred_option(&minor, &icred,
                                (gss_OID)GSS_KRB5_CRED_NO_CI_FLAGS_X,
                                &empty_buffer);
    check_gsserr("gss_set_cred_option", major, minor);
    flagtest(&mech_krb5, icred, tname, 0, GSS_C_TRANS_FLAG);
    flagtest(&mech_krb5, icred, tname, GSS_C_CONF_FLAG,
             GSS_C_CONF_FLAG | GSS_C_TRANS_FLAG);
    flagtest(&mech_krb5, icred, tname, GSS_C_INTEG_FLAG,
             GSS_C_INTEG_FLAG | GSS_C_TRANS_FLAG);
    flagtest(&mech_krb5, icred, tname, GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG,
             GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG | GSS_C_TRANS_FLAG);
    flagtest(&mech_spnego, icred, tname, 0, GSS_C_TRANS_FLAG);
    flagtest(&mech_spnego, icred, tname, GSS_C_INTEG_FLAG,
             GSS_C_INTEG_FLAG | GSS_C_TRANS_FLAG);
    flagtest(&mech_spnego, icred, tname, GSS_C_CONF_FLAG,
             GSS_C_CONF_FLAG | GSS_C_TRANS_FLAG);
    flagtest(&mech_spnego, icred, tname, GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG,
             GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG | GSS_C_TRANS_FLAG);

    (void)gss_release_name(&minor, &tname);
    (void)gss_release_cred(&minor, &icred);
    return 0;
}
