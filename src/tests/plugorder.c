/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/plugorder.c - Test harness to display the order of loaded plugins */
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
 * This file registers a few dummy built-in pwqual modules, then prints out the
 * order of pwqual modules returned by k5_plugin_load_all.  The choice of the
 * pwqual interface is mostly arbitrary; it is an interface which libkrb5
 * itself doesn't use, for which we have a test module.
 */

#include "k5-int.h"
#include <krb5/pwqual_plugin.h>

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

static krb5_error_code
blt1(krb5_context context, int maj_ver, int min_ver, krb5_plugin_vtable vtable)
{
    ((krb5_pwqual_vtable)vtable)->name = "blt1";
    return 0;
}

static krb5_error_code
blt2(krb5_context context, int maj_ver, int min_ver, krb5_plugin_vtable vtable)
{
    ((krb5_pwqual_vtable)vtable)->name = "blt2";
    return 0;
}

static krb5_error_code
blt3(krb5_context context, int maj_ver, int min_ver, krb5_plugin_vtable vtable)
{
    ((krb5_pwqual_vtable)vtable)->name = "blt3";
    return 0;
}

int
main()
{
    krb5_plugin_initvt_fn *modules = NULL, *mod;
    struct krb5_pwqual_vtable_st vt;

    check(krb5_init_context(&ctx));
    check(k5_plugin_register(ctx, PLUGIN_INTERFACE_PWQUAL, "blt1", blt1));
    check(k5_plugin_register(ctx, PLUGIN_INTERFACE_PWQUAL, "blt2", blt2));
    check(k5_plugin_register(ctx, PLUGIN_INTERFACE_PWQUAL, "blt3", blt3));
    check(k5_plugin_load_all(ctx, PLUGIN_INTERFACE_PWQUAL, &modules));
    for (mod = modules; *mod != NULL; mod++) {
        check((*mod)(ctx, 1, 1, (krb5_plugin_vtable)&vt));
        printf("%s\n", vt.name);
    }
    k5_plugin_free_modules(ctx, modules);
    return 0;
}
