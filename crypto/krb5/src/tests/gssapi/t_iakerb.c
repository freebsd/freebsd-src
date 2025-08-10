/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/gssapi/t_iakerb.c - IAKERB tests */
/*
 * Copyright (C) 2024, 2025 by the Massachusetts Institute of Technology.
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
#include <string.h>
#include "common.h"

int
main(int argc, char **argv)
{
    OM_uint32 major, minor;
    const char *password;
    gss_name_t iname, tname, aname;
    gss_cred_id_t icred, acred;
    gss_ctx_id_t ictx, actx;
    gss_buffer_desc pwbuf;

    if (argc != 5) {
        fprintf(stderr, "Usage: %s initiatorname password|- targetname "
                "acceptorname\n", argv[0]);
        return 1;
    }

    iname = import_name(argv[1]);
    password = argv[2];
    tname = import_name(argv[3]);
    aname = import_name(argv[4]);

    if (strcmp(password, "-") != 0) {
        pwbuf.value = (void *)password;
        pwbuf.length = strlen(password);
        major = gss_acquire_cred_with_password(&minor, iname, &pwbuf, 0,
                                               &mechset_iakerb, GSS_C_INITIATE,
                                               &icred, NULL, NULL);
        check_gsserr("gss_acquire_cred_with_password", major, minor);
    } else {
        major = gss_acquire_cred(&minor, iname, GSS_C_INDEFINITE,
                                 &mechset_iakerb, GSS_C_INITIATE, &icred, NULL,
                                 NULL);
        check_gsserr("gss_acquire_cred(iname)", major, minor);
    }

    major = gss_acquire_cred(&minor, aname, GSS_C_INDEFINITE, &mechset_iakerb,
                             GSS_C_ACCEPT, &acred, NULL, NULL);
    check_gsserr("gss_acquire_cred(aname)", major, minor);

    establish_contexts(&mech_iakerb, icred, acred, tname, 0, &ictx, &actx,
                       NULL, NULL, NULL);

    (void)gss_release_name(&minor, &iname);
    (void)gss_release_name(&minor, &tname);
    (void)gss_release_name(&minor, &aname);
    (void)gss_release_cred(&minor, &icred);
    (void)gss_release_cred(&minor, &acred);
    (void)gss_delete_sec_context(&minor, &ictx, NULL);
    (void)gss_delete_sec_context(&minor, &actx, NULL);
    return 0;
}
