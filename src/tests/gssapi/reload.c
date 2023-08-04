/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/gssapi/reload.c - test loading libgssapi_krb5 twice */
/*
 * Copyright (C) 2020 by the Massachusetts Institute of Technology.
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
 * This is a regression test for ticket #8614.  It ensures that libgssapi_krb5
 * can be loaded multiple times in the same process when libkrb5support is held
 * open by another library.
 */

#include <gssapi/gssapi.h>
#include <stdio.h>
#include <dlfcn.h>
#include <assert.h>

/* Load libgssapi_krb5, briefly use it (to force the initializer to run), and
 * close it. */
static void
load_gssapi(void)
{
    void *gssapi;
    OM_uint32 (*indmechs)(OM_uint32 *, gss_OID_set *);
    OM_uint32 (*reloidset)(OM_uint32 *, gss_OID_set *);
    OM_uint32 major, minor;
    gss_OID_set mechs;

    gssapi = dlopen("libgssapi_krb5.so", RTLD_NOW | RTLD_LOCAL);
    assert(gssapi != NULL);
    indmechs = dlsym(gssapi, "gss_indicate_mechs");
    reloidset = dlsym(gssapi, "gss_release_oid_set");
    assert(indmechs != NULL && reloidset != NULL);
    major = (*indmechs)(&minor, &mechs);
    assert(major == 0);
    (*reloidset)(&minor, &mechs);
    dlclose(gssapi);
}

int
main()
{
    void *support;

    /* Hold open libkrb5support to ensure that thread-local state remains */
    support = dlopen("libkrb5support.so", RTLD_NOW | RTLD_LOCAL);
    if (support == NULL) {
        fprintf(stderr, "Error loading libkrb5support: %s\n", dlerror());
        return 1;
    }

    load_gssapi();
    load_gssapi();

    dlclose(support);
    return 0;
}
