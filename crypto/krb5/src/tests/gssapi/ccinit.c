/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/gssapi/ccinit.c - Initialize an empty ccache */
/*
 * Copyright (C) 2012 by the Massachusetts Institute of Technology.
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
 * This program initializes a ccache without attempting to get credentials in
 * it.  It is used to test some finer points of gss_acquire_cred behavior.
 */

#include "k5-int.h"

static void
check(krb5_error_code code)
{
    if (code != 0) {
        com_err("ccinit", code, NULL);
        abort();
    }
}

int
main(int argc, char **argv)
{
    const char *ccname, *princname;
    krb5_context context;
    krb5_principal princ;
    krb5_ccache ccache;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s ccname princname\n", argv[0]);
        return 1;
    }
    ccname = argv[1];
    princname = argv[2];

    check(krb5_init_context(&context));
    check(krb5_parse_name(context, princname, &princ));
    check(krb5_cc_resolve(context, ccname, &ccache));
    check(krb5_cc_initialize(context, ccache, princ));
    krb5_cc_close(context, ccache);
    krb5_free_principal(context, princ);
    krb5_free_context(context);
    return 0;
}
