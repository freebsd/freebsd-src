/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 2020 by Red Hat, Inc.
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
#include <assert.h>

#include "common.h"

/*
 * Establish contexts (without and with GSS_C_DCE_STYLE) with the default
 * initiator name, a specified principal name as target name, initiator
 * bindings, and acceptor bindings.  If any call is unsuccessful, display an
 * error message.  Output "yes" or "no" to indicate whether the contexts were
 * reported as channel-bound on the acceptor.  Exit with status 0 if all
 * operations are successful, or 1 if not.
 *
 * Usage: ./t_bindings [-s] targetname icb acb
 *
 * An icb or abc value of "-" will not specify channel bindings.
 */

int
main(int argc, char *argv[])
{
    OM_uint32 minor, flags1, flags2;
    gss_name_t target_name;
    gss_ctx_id_t ictx, actx;
    struct gss_channel_bindings_struct icb_data = {0}, acb_data = {0};
    gss_channel_bindings_t icb = GSS_C_NO_CHANNEL_BINDINGS;
    gss_channel_bindings_t acb = GSS_C_NO_CHANNEL_BINDINGS;
    gss_OID_desc *mech;

    argv++;
    argc--;
    if (*argv != NULL && strcmp(*argv, "-s") == 0) {
        mech = &mech_spnego;
        argv++;
        argc--;
    } else {
        mech = &mech_krb5;
    }

    if (argc != 3) {
        fprintf(stderr, "Usage: t_bindings [-s] targetname icb acb\n");
        return 1;
    }

    target_name = import_name(argv[0]);

    if (strcmp(argv[1], "-") != 0) {
        icb_data.application_data.length = strlen(argv[1]);
        icb_data.application_data.value = argv[1];
        icb = &icb_data;
    }

    if (strcmp(argv[2], "-") != 0) {
        acb_data.application_data.length = strlen(argv[2]);
        acb_data.application_data.value = argv[2];
        acb = &acb_data;
    }

    establish_contexts_ex(mech, GSS_C_NO_CREDENTIAL, GSS_C_NO_CREDENTIAL,
                          target_name, 0, &ictx, &actx, icb, acb, &flags1,
                          NULL, NULL, NULL);

    /* Try again with GSS_C_DCE_STYLE */
    (void)gss_delete_sec_context(&minor, &ictx, NULL);
    (void)gss_delete_sec_context(&minor, &actx, NULL);

    establish_contexts_ex(mech, GSS_C_NO_CREDENTIAL, GSS_C_NO_CREDENTIAL,
                          target_name, GSS_C_DCE_STYLE, &ictx, &actx, icb, acb,
                          &flags2, NULL, NULL, NULL);
    assert((flags1 & GSS_C_CHANNEL_BOUND_FLAG) ==
           (flags2 & GSS_C_CHANNEL_BOUND_FLAG));
    printf("%s\n", (flags1 & GSS_C_CHANNEL_BOUND_FLAG) ? "yes" : "no");

    (void)gss_delete_sec_context(&minor, &ictx, NULL);
    (void)gss_delete_sec_context(&minor, &actx, NULL);
    (void)gss_release_name(&minor, &target_name);

    return 0;
}
