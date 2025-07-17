/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/hostrealm/test/main.c - test module for host-realm interface */
/*
 * Copyright (C) 2010,2013 by the Massachusetts Institute of Technology.
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
 * This file implements two hostrealm modules named "test1" and "test2".
 *
 * The first module returns multi-element lists.  For host_realm and
 * fallback_realm, it returns a list of the host's components in order.  For
 * default_realm, it returns a list containing "one" and "two".
 *
 * The second module tests error handling.  For host_realm and fallback_realm,
 * it returns a fatal error on hosts beginning with 'z', a list containing "a"
 * for hosts beginning with 'a', and passes control to later modules otherwise.
 * For default_realm, it returns a fatal error.
 */

#include <k5-int.h>
#include <krb5/hostrealm_plugin.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static krb5_error_code
split_comps(krb5_context context, krb5_hostrealm_moddata data,
            const char *host, char ***realms_out)
{
    krb5_error_code ret;
    const char *p, *q;
    char *word = NULL, **list = NULL, **newptr;
    size_t count = 0;

    *realms_out = NULL;
    if (*host == '\0')
        return KRB5_PLUGIN_NO_HANDLE;
    p = host;
    while (TRUE) {
        q = strchr(p, '.');
        word = (q == NULL) ? strdup(p) : k5memdup0(p, q - p, &ret);
        if (word == NULL)
            goto oom;
        newptr = realloc(list, (count + 2) * sizeof(*list));
        if (newptr == NULL)
            goto oom;
        list = newptr;
        list[count++] = word;
        list[count] = NULL;
        word = NULL;
        if (q == NULL)
            break;
        p = q + 1;
    }
    *realms_out = list;
    return 0;

oom:
    krb5_free_host_realm(context, list);
    free(word);
    return ENOMEM;
}

static krb5_error_code
multi_defrealm(krb5_context context, krb5_hostrealm_moddata data,
               char ***realms_out)
{
    char **list = NULL, *one = NULL, *two = NULL;

    *realms_out = NULL;
    one = strdup("one");
    if (one == NULL)
        goto oom;
    two = strdup("two");
    if (two == NULL)
        goto oom;
    list = calloc(3, sizeof(*list));
    if (list == NULL)
        goto oom;
    list[0] = one;
    list[1] = two;
    list[2] = NULL;
    *realms_out = list;
    return 0;

oom:
    free(one);
    free(two);
    free(list);
    return ENOMEM;
}

static krb5_error_code
maybe_realm(krb5_context context, krb5_hostrealm_moddata data,
            const char *host, char ***realms_out)
{
    char **list, *a;

    *realms_out = NULL;
    if (*host == 'z')
        return KRB5_ERR_NO_SERVICE;
    if (*host != 'a')
        return KRB5_PLUGIN_NO_HANDLE;
    a = strdup("a");
    if (a == NULL)
        return ENOMEM;
    list = calloc(2, sizeof(*list));
    if (list == NULL) {
        free(a);
        return ENOMEM;
    }
    list[0] = a;
    list[1] = NULL;
    *realms_out = list;
    return 0;
}

static krb5_error_code
error(krb5_context context, krb5_hostrealm_moddata data, char ***realms_out)
{
    *realms_out = NULL;
    return KRB5_ERR_NO_SERVICE;
}

static void
free_realmlist(krb5_context context, krb5_hostrealm_moddata data,
               char **list)
{
    krb5_free_host_realm(context, list);
}

krb5_error_code
hostrealm_test1_initvt(krb5_context context, int maj_ver, int min_ver,
                       krb5_plugin_vtable vtable);
krb5_error_code
hostrealm_test2_initvt(krb5_context context, int maj_ver, int min_ver,
                       krb5_plugin_vtable vtable);

krb5_error_code
hostrealm_test1_initvt(krb5_context context, int maj_ver, int min_ver,
                       krb5_plugin_vtable vtable)
{
    krb5_hostrealm_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_hostrealm_vtable)vtable;
    vt->name = "test1";
    vt->host_realm = split_comps;
    vt->fallback_realm = split_comps;
    vt->default_realm = multi_defrealm;
    vt->free_list = free_realmlist;
    return 0;
}

krb5_error_code
hostrealm_test2_initvt(krb5_context context, int maj_ver, int min_ver,
                       krb5_plugin_vtable vtable)
{
    krb5_hostrealm_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_hostrealm_vtable)vtable;
    vt->name = "test2";
    vt->host_realm = maybe_realm;
    vt->default_realm = error;
    vt->free_list = free_realmlist;
    return 0;
}
