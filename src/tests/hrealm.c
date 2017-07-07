/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/hrealm.c - Test harness for host-realm interfaces */
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
 * This program is intended to be run from a python script as:
 *
 *     hrealm -h|-f|-d [hostname]
 *
 * Calls krb5_get_host_realm, krb5_get_fallback_host_realm, or
 * krb5_default_realm depending on the option given.  For the first two
 * choices, hostname or NULL is passed as the argument.  The results are
 * displayed one per line.
 */

#include "k5-int.h"

static krb5_context ctx;

static void
check(krb5_error_code code)
{
    const char *errmsg;

    if (code) {
        errmsg = krb5_get_error_message(ctx, code);
        fprintf(stderr, "%s\n", errmsg);
        krb5_free_error_message(ctx, errmsg);
        exit(1);
    }
}

static void
display(char **realms)
{
    while (realms != NULL && *realms != NULL)
        printf("%s\n", *realms++);
}

int
main(int argc, char **argv)
{
    krb5_data d;
    char **realms, *realm;

    check(krb5_init_context(&ctx));

    /* Parse arguments. */
    if (argc < 2 || argc > 3)
        abort();

    if (strcmp(argv[1], "-d") == 0) {
        check(krb5_get_default_realm(ctx, &realm));
        printf("%s\n", realm);
        krb5_free_default_realm(ctx, realm);
    } else if (strcmp(argv[1], "-h") == 0) {
        check(krb5_get_host_realm(ctx, argv[2], &realms));
        display(realms);
        krb5_free_host_realm(ctx, realms);
    } else if (strcmp(argv[1], "-f") == 0) {
        assert(argc == 3);
        d = string2data(argv[2]);
        check(krb5_get_fallback_host_realm(ctx, &d, &realms));
        display(realms);
        krb5_free_host_realm(ctx, realms);
    } else {
        abort();
    }
    krb5_free_context(ctx);
    return 0;
}
