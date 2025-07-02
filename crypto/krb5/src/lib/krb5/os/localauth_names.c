/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/localauth_names.c - names localauth module */
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

#include "k5-int.h"
#include "os-proto.h"
#include <krb5/localauth_plugin.h>

static krb5_error_code
an2ln_names(krb5_context context, krb5_localauth_moddata data,
            const char *type, const char *residual, krb5_const_principal aname,
            char **lname_out)
{
    krb5_error_code ret;
    char *realm = NULL, *pname = NULL, **mapping_values = NULL;
    const char *hierarchy[5];
    size_t count;

    *lname_out = NULL;

    /*
     * Fetch the profile values for realms-><defaultrealm>->
     * auth_to_local_names-><princname>.  Use the principal name without realm;
     * this is problematic in many multiple-realm environments, but is how
     * we've historically done it.
     */
    ret = krb5_get_default_realm(context, &realm);
    if (ret)
        return KRB5_LNAME_NOTRANS;
    ret = krb5_unparse_name_flags(context, aname,
                                  KRB5_PRINCIPAL_UNPARSE_NO_REALM, &pname);
    if (ret)
        goto cleanup;
    hierarchy[0] = KRB5_CONF_REALMS;
    hierarchy[1] = realm;
    hierarchy[2] = KRB5_CONF_AUTH_TO_LOCAL_NAMES;
    hierarchy[3] = pname;
    hierarchy[4] = NULL;
    ret = profile_get_values(context->profile, hierarchy, &mapping_values);
    if (ret) {
        ret = KRB5_LNAME_NOTRANS;
        goto cleanup;
    }

    /* We found one or more explicit mappings.  Use the last one. */
    for (count = 0; mapping_values[count] != NULL; count++);
    *lname_out = strdup(mapping_values[count - 1]);
    if (*lname_out == NULL)
        ret = ENOMEM;

cleanup:
    free(realm);
    free(pname);
    profile_free_list(mapping_values);
    return ret;
}

static void
freestr(krb5_context context, krb5_localauth_moddata data, char *str)
{
    free(str);
}

krb5_error_code
localauth_names_initvt(krb5_context context, int maj_ver, int min_ver,
                       krb5_plugin_vtable vtable)
{
    krb5_localauth_vtable vt = (krb5_localauth_vtable)vtable;

    vt->name = "names";
    vt->an2ln = an2ln_names;
    vt->free_string = freestr;
    return 0;
}
