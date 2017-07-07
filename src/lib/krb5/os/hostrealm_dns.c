/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/hostream_dns.c - dns hostrealm module */
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
 * This file implements the built-in dns module for the hostrealm interface,
 * which uses TXT records in the DNS to determine the default realm or the
 * fallback realm of a host.
 */

#include "k5-int.h"
#include "os-proto.h"
#include <krb5/hostrealm_plugin.h>

#ifdef KRB5_DNS_LOOKUP
#include "dnsglue.h"

/* Try a _kerberos TXT lookup for fqdn and each parent domain; return the
 * resulting realm (caller must free) or NULL. */
static char *
txt_lookup(krb5_context context, const char *fqdn)
{
    char *realm;

    while (fqdn != NULL && *fqdn != '\0') {
        if (k5_try_realm_txt_rr(context, "_kerberos", fqdn, &realm) == 0)
            return realm;
        fqdn = strchr(fqdn, '.');
        if (fqdn != NULL)
            fqdn++;
    }
    return NULL;
}

static krb5_error_code
dns_fallback_realm(krb5_context context, krb5_hostrealm_moddata data,
                   const char *host, char ***realms_out)
{
    krb5_error_code ret;
    char *realm;

    *realms_out = NULL;
    if (!_krb5_use_dns_realm(context) || k5_is_numeric_address(host))
        return KRB5_PLUGIN_NO_HANDLE;

    /* Try a TXT record lookup for each component of host. */
    realm = txt_lookup(context, host);
    if (realm == NULL)
        return KRB5_PLUGIN_NO_HANDLE;
    ret = k5_make_realmlist(realm, realms_out);
    free(realm);
    return ret;
}

static krb5_error_code
dns_default_realm(krb5_context context, krb5_hostrealm_moddata data,
                  char ***realms_out)
{
    krb5_error_code ret;
    char localhost[MAXDNAME + 1], *realm;

    *realms_out = NULL;
    if (!_krb5_use_dns_realm(context))
        return KRB5_PLUGIN_NO_HANDLE;

    ret = krb5int_get_fq_local_hostname(localhost, sizeof(localhost));
    if (ret)
        return ret;

    /* If we don't find a TXT record for localhost or any parent, look for a
     * global record. */
    realm = txt_lookup(context, localhost);
    if (realm == NULL)
        (void)k5_try_realm_txt_rr(context, "_kerberos", NULL, &realm);

    if (realm == NULL)
        return KRB5_PLUGIN_NO_HANDLE;
    ret = k5_make_realmlist(realm, realms_out);
    free(realm);
    return ret;
}

static void
dns_free_realmlist(krb5_context context, krb5_hostrealm_moddata data,
                   char **list)
{
    krb5_free_host_realm(context, list);
}

krb5_error_code
hostrealm_dns_initvt(krb5_context context, int maj_ver, int min_ver,
                     krb5_plugin_vtable vtable)
{
    krb5_hostrealm_vtable vt = (krb5_hostrealm_vtable)vtable;

    vt->name = "dns";
    vt->fallback_realm = dns_fallback_realm;
    vt->default_realm = dns_default_realm;
    vt->free_list = dns_free_realmlist;
    return 0;
}

#else /* KRB5_DNS_LOOKUP */

krb5_error_code
hostrealm_dns_initvt(krb5_context context, int maj_ver, int min_ver,
                     krb5_plugin_vtable vtable)
{
    krb5_hostrealm_vtable vt = (krb5_hostrealm_vtable)vtable;

    vt->name = "dns";
    return 0;
}

#endif /* KRB5_DNS_LOOKUP */
