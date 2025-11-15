/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/t_ctxprf.c - krb5_init_context_profile() test */
/*
 * Copyright (C) 2024 by the Massachusetts Institute of Technology.
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
 * This program tests the use of krb5_init_context_profile() with a modified
 * profile object.  If run with the single argument "empty", we create and
 * modifiemodify an empty profile; otherwise, we retrieve and modify the
 * profile for a normally created libkrb5 context.  In both cases, we set a
 * realm KDC value in the profile and use k5_locate_kdc() to verify that the
 * setting is reflected in a libkrb5 context created using the modified
 * profile.
 */

#include "k5-int.h"
#include "os-proto.h"

static void
check(long code)
{
    assert(code == 0);
}

int
main(int argc, char **argv)
{
    profile_t p;
    krb5_context ctx;
    const char *names[] = { "realms", "KRBTEST.COM", "kdc", NULL };
    krb5_data realm = string2data("KRBTEST.COM");
    struct serverlist sl;

    if (argc > 1 && strcmp(argv[1], "empty") == 0) {
        check(profile_init(NULL, &p));
    } else {
        check(krb5_init_context(&ctx));
        check(krb5_get_profile(ctx, &p));
        krb5_free_context(ctx);
        profile_clear_relation(p, names);
    }
    check(profile_add_relation(p, names, "ctx.prf.test"));

    check(krb5_init_context_profile(p, 0, &ctx));
    check(k5_locate_kdc(ctx, &realm, &sl, FALSE, FALSE));
    assert(sl.nservers == 1);
    assert(strcmp(sl.servers[0].hostname, "ctx.prf.test") == 0);

    profile_abandon(p);
    k5_free_serverlist(&sl);
    krb5_free_context(ctx);
    return 0;
}
