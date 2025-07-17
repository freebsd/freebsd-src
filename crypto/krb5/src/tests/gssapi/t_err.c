/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/gssapi/t_err.c - Test accept_sec_context error generation */
/*
 * Copyright (C) 2013 by the Massachusetts Institute of Technology.
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
 * This test program verifies that the krb5 gss_accept_sec_context can produce
 * error tokens and that gss_init_sec_context can interpret them.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common.h"

static void
check_replay_error(const char *msg, OM_uint32 major, OM_uint32 minor)
{
    OM_uint32 tmpmin, msg_ctx = 0;
    const char *replay = "Request is a replay";
    gss_buffer_desc m;

    if (major != GSS_S_FAILURE) {
        fprintf(stderr, "%s: expected major code GSS_S_FAILURE\n", msg);
        check_gsserr(msg, major, minor);
        exit(1);
    }

    (void)gss_display_status(&tmpmin, minor, GSS_C_MECH_CODE, GSS_C_NULL_OID,
                             &msg_ctx, &m);
    if (m.length != strlen(replay) || memcmp(m.value, replay, m.length) != 0) {
        fprintf(stderr, "%s: expected replay error; got %.*s\n", msg,
                (int)m.length, (char *)m.value);
        exit(1);
    }
    (void)gss_release_buffer(&tmpmin, &m);
}

int
main(int argc, char *argv[])
{
    OM_uint32 minor, major, flags;
    gss_OID mech = &mech_krb5;
    gss_name_t tname;
    gss_buffer_desc itok, atok;
    gss_ctx_id_t ictx = GSS_C_NO_CONTEXT, actx = GSS_C_NO_CONTEXT;

    argv++;
    if (*argv != NULL && strcmp(*argv, "--spnego") == 0) {
        mech = &mech_spnego;
        argv++;
    }
    if (*argv == NULL || argv[1] != NULL) {
        fprintf(stderr, "Usage: t_err targetname\n");
        return 1;
    }
    tname = import_name(*argv);

    /* Get the initial context token. */
    flags = GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG | GSS_C_MUTUAL_FLAG;
    major = gss_init_sec_context(&minor, GSS_C_NO_CREDENTIAL, &ictx, tname,
                                 mech, flags, GSS_C_INDEFINITE,
                                 GSS_C_NO_CHANNEL_BINDINGS, GSS_C_NO_BUFFER,
                                 NULL, &itok, NULL, NULL);
    check_gsserr("gss_init_sec_context(1)", major, minor);
    assert(major == GSS_S_CONTINUE_NEEDED);

    /* Process this token into an acceptor context, then discard it. */
    major = gss_accept_sec_context(&minor, &actx, GSS_C_NO_CREDENTIAL, &itok,
                                   GSS_C_NO_CHANNEL_BINDINGS, NULL,
                                   NULL, &atok, NULL, NULL, NULL);
    check_gsserr("gss_accept_sec_context(1)", major, minor);
    (void)gss_release_buffer(&minor, &atok);
    (void)gss_delete_sec_context(&minor, &actx, NULL);

    /* Process the same token again, producing a replay error. */
    major = gss_accept_sec_context(&minor, &actx, GSS_C_NO_CREDENTIAL, &itok,
                                   GSS_C_NO_CHANNEL_BINDINGS, NULL,
                                   NULL, &atok, NULL, NULL, NULL);
    check_replay_error("gss_accept_sec_context(2)", major, minor);
    assert(atok.length != 0);

    /* Send the error token back the initiator. */
    (void)gss_release_buffer(&minor, &itok);
    major = gss_init_sec_context(&minor, GSS_C_NO_CREDENTIAL, &ictx, tname,
                                 mech, flags, GSS_C_INDEFINITE,
                                 GSS_C_NO_CHANNEL_BINDINGS, &atok,
                                 NULL, &itok, NULL, NULL);
    check_replay_error("gss_init_sec_context(2)", major, minor);

    (void)gss_release_name(&minor, &tname);
    (void)gss_release_buffer(&minor, &itok);
    (void)gss_release_buffer(&minor, &atok);
    (void)gss_delete_sec_context(&minor, &ictx, NULL);
    (void)gss_delete_sec_context(&minor, &actx, NULL);
    return 0;
}
