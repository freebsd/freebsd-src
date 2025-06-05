/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/t_sname_match.c - Unit tests for krb5_sname_match() */
/*
 * Copyright (C) 2016 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"

struct test {
    const char *matchstr;
    const char *princstr;
    krb5_boolean result;
    krb5_boolean ignore_acceptor_hostname;
    krb5_boolean non_host_nametype;
} tests[] = {
    /* If matching is NULL, the result is true for any princ. */
    { NULL, "a/b@R", TRUE },

    /* If matching does not have two components or does not have name type
     * KRB5_NT_SRV_HOST, the result is a direct comparison. */
    { "a@R", "a@R", TRUE },
    { "a@R", "b@R", FALSE },
    { "a/@R", "a/@R", TRUE, FALSE, TRUE },
    { "a/@R", "a/b@R", FALSE, FALSE, TRUE },
    { "a/b@", "a/b@", TRUE, FALSE, TRUE },
    { "a/b@", "a/b@R", FALSE, FALSE, TRUE },
    { "a/b/@R", "a/b/@R", TRUE },
    { "a/b/@R", "a/b/c@R", FALSE },

    /* The number of components must match. */
    { "a/b@R", "a@R", FALSE },
    { "a/b@R", "a/b/@R", FALSE },
    { "a/b@R", "a/b/c@R", FALSE },

    /* If matching's realm is empty, any realm in princ is permitted. */
    { "a/b@", "a/b@", TRUE },
    { "a/b@", "a/b@R", TRUE },
    { "a/b@R", "a/b@R", TRUE },
    { "a/b@R", "a/b@S", FALSE },

    /* matching's first component must match princ's (even if empty). */
    { "/b@R", "/b@R", TRUE },
    { "/b@R", "a/b@R", FALSE },

    /* If matching's second component is empty, any second component in princ
     * is permitted. */
    { "a/@R", "a/@R", TRUE },
    { "a/@R", "a/b@R", TRUE },

    /* If ignore_acceptor_hostname is set, any second component in princ is
     * permitted, even if there is a different second component in matching. */
    { "a/b@R", "a/c@R", TRUE, TRUE },
    { "a/b@R", "c/b@R", FALSE, TRUE },
};

int
main()
{
    size_t i;
    struct test *t;
    krb5_principal matching, princ;
    krb5_context ctx;

    if (krb5_init_context(&ctx) != 0)
        abort();
    for (i = 0; i < sizeof(tests) / sizeof(*tests); i++) {
        t = &tests[i];

        if (t->matchstr != NULL) {
            if (krb5_parse_name(ctx, t->matchstr, &matching) != 0)
                abort();
            if (t->non_host_nametype)
                matching->type = KRB5_NT_PRINCIPAL;
            else
                matching->type = KRB5_NT_SRV_HST;
        } else {
            matching = NULL;
        }
        if (krb5_parse_name(ctx, t->princstr, &princ) != 0)
            abort();

        ctx->ignore_acceptor_hostname = t->ignore_acceptor_hostname;
        if (krb5_sname_match(ctx, matching, princ) != t->result)
            abort();

        krb5_free_principal(ctx, matching);
        krb5_free_principal(ctx, princ);
    }
    krb5_free_context(ctx);
    return 0;
}
