/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/localauth/test/main.c - test modules for localauth interface */
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

/* This file implements two testing localauth modules, each implementing
 * clearly recognizable behavior for the localauth test script. */

#include "k5-int.h"
#include <krb5/localauth_plugin.h>

struct krb5_localauth_moddata_st {
    int a;
    int b;
};

static krb5_error_code
init_test(krb5_context context, krb5_localauth_moddata *data_out)
{
    krb5_localauth_moddata d;

    *data_out = NULL;
    d = malloc(sizeof(*d));
    if (d == NULL)
        return ENOMEM;
    d->a = 3;
    d->b = 4;
    *data_out = d;
    return 0;
}

static void
fini_test(krb5_context context, krb5_localauth_moddata data)
{
    assert(data->a == 3);
    assert(data->b == 4);
    free(data);
}

static krb5_error_code
an2ln_test(krb5_context context, krb5_localauth_moddata data, const char *type,
           const char *residual, krb5_const_principal aname, char **lname_out)
{
    krb5_error_code ret;
    char *lname = NULL;

    *lname_out = NULL;
    if (data != NULL) {
        assert(data->a == 3);
        assert(data->b == 4);
    }
    if (type == NULL) {
        /* Map any three-component test/___/___ principal to its realm name. */
        if (aname->length == 3 && data_eq_string(aname->data[0], "test")) {
            lname = k5memdup0(aname->realm.data, aname->realm.length, &ret);
            if (lname == NULL)
                return ret;
        }
    } else if (strcmp(type, "TYPEA") == 0) {
        /* Map any two-component principal to its second component. */
        if (aname->length == 2) {
            lname = k5memdup0(aname->data[1].data, aname->data[1].length,
                              &ret);
            if (lname == NULL)
                return ret;
        }
    } else {
        assert(strcmp(type, "TYPEB") == 0);
        /* Map to the residual string. */
        lname = strdup(residual == NULL ? "(null)" : residual);
        if (lname == NULL)
            return ENOMEM;
    }
    if (lname == NULL)
        return KRB5_LNAME_NOTRANS;
    *lname_out = lname;
    return 0;
}

static krb5_error_code
userok_test(krb5_context context, krb5_localauth_moddata data,
            krb5_const_principal aname, const char *lname)
{
    if (data != NULL) {
        assert(data->a == 3);
        assert(data->b == 4);
    }

    /* Return success if the number of components in the principal is equal to
     * the length of the local name. */
    if ((size_t)aname->length == strlen(lname))
        return 0;

    /* Pass control down if the first component is "pass". */
    if (aname->length >= 1 && data_eq_string(aname->data[0], "pass"))
        return KRB5_PLUGIN_NO_HANDLE;

    /* Otherwise reject. */
    return EPERM;
}

static void
freestr(krb5_context context, krb5_localauth_moddata data, char *str)
{
    free(str);
}

krb5_error_code
localauth_test1_initvt(krb5_context context, int maj_ver, int min_ver,
                       krb5_plugin_vtable vtable);
krb5_error_code
localauth_test2_initvt(krb5_context context, int maj_ver, int min_ver,
                       krb5_plugin_vtable vtable);

krb5_error_code
localauth_test1_initvt(krb5_context context, int maj_ver, int min_ver,
                       krb5_plugin_vtable vtable)
{
    krb5_localauth_vtable vt = (krb5_localauth_vtable)vtable;

    vt->init = init_test;
    vt->fini = fini_test;
    vt->name = "test1";
    vt->an2ln = an2ln_test;
    vt->userok = userok_test;
    vt->free_string = freestr;
    return 0;
}

krb5_error_code
localauth_test2_initvt(krb5_context context, int maj_ver, int min_ver,
                       krb5_plugin_vtable vtable)
{
    krb5_localauth_vtable vt = (krb5_localauth_vtable)vtable;
    static const char *types[] = { "TYPEA", "TYPEB", NULL };

    vt->name = "test2";
    vt->an2ln_types = types;
    vt->an2ln = an2ln_test;
    vt->free_string = freestr;
    return 0;
}
