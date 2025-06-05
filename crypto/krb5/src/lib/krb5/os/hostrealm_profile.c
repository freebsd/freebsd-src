/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/hostream_profile.c - profile hostrealm module */
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
 * This file implements the built-in profile module for the hostrealm
 * interface, which uses profile configuration to determine the local default
 * realm or the authoritative realm of a host.
 */

#include "k5-int.h"
#include "os-proto.h"
#include <krb5/hostrealm_plugin.h>

/*
 * Search progressively shorter suffixes of host in the [domain_realms] section
 * of the profile to find the realm.  For example, given a host a.b.c, try to
 * match a.b.c, then .b.c, then b.c, then .c, then c.  If we don't find a
 * match, return success but set *realm_out to NULL.
 */
static krb5_error_code
profile_host_realm(krb5_context context, krb5_hostrealm_moddata data,
                   const char *host, char ***realms_out)
{
    krb5_error_code ret;
    const char *p;
    char *prof_realm;

    *realms_out = NULL;

    /* Don't look up IP addresses in [domain_realms]. */
    if (k5_is_numeric_address(host))
        return KRB5_PLUGIN_NO_HANDLE;

    /* Look for the host and each suffix in the [domain_realms] section. */
    for (p = host; p != NULL; p = (*p == '.') ? p + 1 : strchr(p, '.')) {
        ret = profile_get_string(context->profile, KRB5_CONF_DOMAIN_REALM, p,
                                 NULL, NULL, &prof_realm);
        if (ret)
            return ret;
        if (prof_realm != NULL) {
            ret = k5_make_realmlist(prof_realm, realms_out);
            profile_release_string(prof_realm);
            return ret;
        }
    }
    return KRB5_PLUGIN_NO_HANDLE;
}

/* Look up the default_realm variable in the [libdefaults] section of the
 * profile. */
static krb5_error_code
profile_default_realm(krb5_context context, krb5_hostrealm_moddata data,
                      char ***realms_out)
{
    krb5_error_code ret;
    char *prof_realm;

    *realms_out = NULL;
    ret = profile_get_string(context->profile, KRB5_CONF_LIBDEFAULTS,
                             KRB5_CONF_DEFAULT_REALM, NULL, NULL, &prof_realm);
    if (ret)
        return ret;
    if (prof_realm == NULL)
        return KRB5_PLUGIN_NO_HANDLE;
    ret = k5_make_realmlist(prof_realm, realms_out);
    profile_release_string(prof_realm);
    return ret;
}

static void
profile_free_realmlist(krb5_context context, krb5_hostrealm_moddata data,
                       char **list)
{
    krb5_free_host_realm(context, list);
}

krb5_error_code
hostrealm_profile_initvt(krb5_context context, int maj_ver, int min_ver,
                         krb5_plugin_vtable vtable)
{
    krb5_hostrealm_vtable vt = (krb5_hostrealm_vtable)vtable;

    vt->name = "profile";
    vt->host_realm = profile_host_realm;
    vt->default_realm = profile_default_realm;
    vt->free_list = profile_free_realmlist;
    return 0;
}
