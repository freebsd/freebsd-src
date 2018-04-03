/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/gssapi/t_lifetime.c - display cred and context lifetimes */
/*
 * Copyright (C) 2017 by the Massachusetts Institute of Technology.
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

/*
 * Using the default credential, exercise the GSS functions which accept or
 * produce lifetimes.  Display the following results, one per line, as ASCII
 * integers or the string "indefinite":
 *
 *   initiator cred lifetime according to gss_acquire_cred()
 *   initiator cred lifetime according to gss_inquire_cred()
 *   acceptor cred lifetime according to gss_acquire_cred()
 *   acceptor cred lifetime according to gss_inquire_cred()
 *   initiator context lifetime according to gss_init_sec_context()
 *   initiator context lifetime according to gss_inquire_context()
 *   initiator context lifetime according to gss_context_time()
 *   acceptor context lifetime according to gss_init_sec_context()
 *   acceptor context lifetime according to gss_inquire_context()
 *   acceptor context lifetime according to gss_context_time()
 */

static void
display_time(OM_uint32 tval)
{
    if (tval == GSS_C_INDEFINITE)
        puts("indefinite");
    else
        printf("%u\n", (unsigned int)tval);
}

int
main(int argc, char *argv[])
{
    OM_uint32 minor, major;
    gss_cred_id_t icred, acred;
    gss_name_t tname;
    gss_ctx_id_t ictx = GSS_C_NO_CONTEXT, actx = GSS_C_NO_CONTEXT;
    gss_buffer_desc itok = GSS_C_EMPTY_BUFFER, atok = GSS_C_EMPTY_BUFFER;
    OM_uint32 time_req = GSS_C_INDEFINITE, time_rec;

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s targetname [time_req]\n", argv[0]);
        return 1;
    }
    tname = import_name(argv[1]);
    if (argc >= 3)
        time_req = atoll(argv[2]);

    /* Get initiator cred and display its lifetime according to
     * gss_acquire_cred and gss_inquire_cred. */
    major = gss_acquire_cred(&minor, GSS_C_NO_NAME, time_req, &mechset_krb5,
                             GSS_C_INITIATE, &icred, NULL, &time_rec);
    check_gsserr("gss_acquire_cred(initiate)", major, minor);
    display_time(time_rec);
    major = gss_inquire_cred(&minor, icred, NULL, &time_rec, NULL, NULL);
    check_gsserr("gss_inquire_cred(initiate)", major, minor);
    display_time(time_rec);

    /* Get acceptor cred and display its lifetime according to gss_acquire_cred
     * and gss_inquire_cred. */
    major = gss_acquire_cred(&minor, GSS_C_NO_NAME, time_req, &mechset_krb5,
                             GSS_C_ACCEPT, &acred, NULL, &time_rec);
    check_gsserr("gss_acquire_cred(accept)", major, minor);
    display_time(time_rec);
    major = gss_inquire_cred(&minor, acred, NULL, &time_rec, NULL, NULL);
    check_gsserr("gss_inquire_cred(accept)", major, minor);
    display_time(time_rec);

    /* Make an initiator context and display its lifetime according to
     * gss_init_sec_context, gss_inquire_context, and gss_context_time. */
    major = gss_init_sec_context(&minor, icred, &ictx, tname, &mech_krb5, 0,
                                 time_req, GSS_C_NO_CHANNEL_BINDINGS, &atok,
                                 NULL, &itok, NULL, &time_rec);
    check_gsserr("gss_init_sec_context", major, minor);
    assert(major == GSS_S_COMPLETE);
    display_time(time_rec);
    major = gss_inquire_context(&minor, ictx, NULL, NULL, &time_rec, NULL,
                                NULL, NULL, NULL);
    check_gsserr("gss_inquire_context(initiate)", major, minor);
    display_time(time_rec);
    major = gss_context_time(&minor, ictx, &time_rec);
    check_gsserr("gss_context_time(initiate)", major, minor);
    display_time(time_rec);

    major = gss_accept_sec_context(&minor, &actx, acred, &itok,
                                   GSS_C_NO_CHANNEL_BINDINGS, NULL,
                                   NULL, &atok, NULL, &time_rec, NULL);
    check_gsserr("gss_accept_sec_context", major, minor);
    assert(major == GSS_S_COMPLETE);
    display_time(time_rec);
    major = gss_inquire_context(&minor, actx, NULL, NULL, &time_rec, NULL,
                                NULL, NULL, NULL);
    check_gsserr("gss_inquire_context(accept)", major, minor);
    display_time(time_rec);
    major = gss_context_time(&minor, actx, &time_rec);
    check_gsserr("gss_context_time(accept)", major, minor);
    display_time(time_rec);

    (void)gss_release_buffer(&minor, &itok);
    (void)gss_release_buffer(&minor, &atok);
    (void)gss_release_name(&minor, &tname);
    (void)gss_release_cred(&minor, &icred);
    (void)gss_release_cred(&minor, &acred);
    (void)gss_delete_sec_context(&minor, &ictx, NULL);
    (void)gss_delete_sec_context(&minor, &actx, NULL);
    return 0;
}
