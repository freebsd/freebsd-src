/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/hostream_domain.c - domain hostrealm module */
/*
 * Copyright (C) 1990,1991,2002,2008,2009,2013 by the Massachusetts Institute
 * of Technology.  All rights reserved.
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
 * This file implements the built-in domain module for the hostrealm interface,
 * which uses domain-based heuristics to determine the fallback realm of a
 * host.
 */

#include "k5-int.h"
#include "os-proto.h"
#include <krb5/hostrealm_plugin.h>
#include <ctype.h>

static krb5_error_code
domain_fallback_realm(krb5_context context, krb5_hostrealm_moddata data,
                      const char *host, char ***realms_out)
{
    krb5_error_code ret;
    struct serverlist slist;
    krb5_data drealm;
    char *uhost = NULL, *p;
    const char *suffix, *dot;
    int limit;

    *realms_out = NULL;

    /* These heuristics don't apply to address literals. */
    if (k5_is_numeric_address(host))
        return KRB5_PLUGIN_NO_HANDLE;

    /* Make an uppercase copy of host. */
    uhost = strdup(host);
    if (uhost == NULL)
        return ENOMEM;
    for (p = uhost; *p != '\0'; p++) {
        if (islower((unsigned char)*p))
            *p = toupper((unsigned char)*p);
    }

    /*
     * Try searching domain suffixes as realms.  This heuristic is turned off
     * by default.  If DNS lookups for KDCs are enabled (as they are by
     * default), an attacker could control which domain component is used as
     * the realm for a host.
     *
     * A realm_try_domains value of -1 (the default) means not to search at
     * all, a value of 0 means to try only the full domain itself, 1 means to
     * also try the parent domain, etc..  We will stop searching when we reach
     * a suffix with only one label.
     */
    ret = profile_get_integer(context->profile, KRB5_CONF_LIBDEFAULTS,
                              KRB5_CONF_REALM_TRY_DOMAINS, 0, -1, &limit);
    if (ret)
        return ret;
    suffix = uhost;
    while (limit-- >= 0 && (dot = strchr(suffix, '.')) != NULL) {
        drealm = string2data((char *)suffix);
        if (k5_locate_kdc(context, &drealm, &slist, FALSE, FALSE) == 0) {
            k5_free_serverlist(&slist);
            ret = k5_make_realmlist(suffix, realms_out);
            goto cleanup;
        }
        suffix = dot + 1;
    }

    /*
     * If that didn't succeed, use the upper-cased parent domain of the
     * hostname, regardless of whether we can actually look it up as a realm.
     */
    dot = strchr(uhost, '.');
    if (dot != NULL)
        ret = k5_make_realmlist(dot + 1, realms_out);
    else
        ret = KRB5_PLUGIN_NO_HANDLE;

cleanup:
    free(uhost);
    return ret;
}

static void
domain_free_realmlist(krb5_context context, krb5_hostrealm_moddata data,
                       char **list)
{
    krb5_free_host_realm(context, list);
}

krb5_error_code
hostrealm_domain_initvt(krb5_context context, int maj_ver, int min_ver,
                         krb5_plugin_vtable vtable)
{
    krb5_hostrealm_vtable vt = (krb5_hostrealm_vtable)vtable;

    vt->name = "domain";
    vt->fallback_realm = domain_fallback_realm;
    vt->free_list = domain_free_realmlist;
    return 0;
}
