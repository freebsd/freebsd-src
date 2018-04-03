/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kadmin/server/auth_self.c - self-service kadm5_auth module */
/*
 * Copyright (C) 2017 by the Massachusetts Institute of Technology.
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
#include <kadm5/admin.h>
#include <krb5/kadm5_auth_plugin.h>
#include "auth.h"

/* Authorize a principal to operate on itself.  Applies to cpw, chrand,
 * purgekeys, getprinc, and getstrs. */
static krb5_error_code
self_compare(krb5_context context, kadm5_auth_moddata data,
             krb5_const_principal client, krb5_const_principal target)
{
    if (krb5_principal_compare(context, client, target))
        return 0;
    return KRB5_PLUGIN_NO_HANDLE;
}

/* Authorize a principal to get the policy record for its own policy. */
static krb5_error_code
self_getpol(krb5_context context, kadm5_auth_moddata data,
            krb5_const_principal client, const char *policy,
            const char *client_policy)
{
    if (client_policy != NULL && strcmp(policy, client_policy) == 0)
        return 0;
    return KRB5_PLUGIN_NO_HANDLE;
}

krb5_error_code
kadm5_auth_self_initvt(krb5_context context, int maj_ver, int min_ver,
                       krb5_plugin_vtable vtable)
{
    kadm5_auth_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (kadm5_auth_vtable)vtable;
    vt->name = "self";
    vt->cpw = self_compare;
    vt->chrand = self_compare;
    vt->purgekeys = self_compare;
    vt->getprinc = self_compare;
    vt->getstrs = self_compare;
    vt->getpol = self_getpol;
    return 0;
}
