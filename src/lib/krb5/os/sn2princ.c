/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/sn2princ.c */
/*
 * Copyright 1991,2002 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/* Convert a hostname and service name to a principal in the "standard"
 * form. */

#include "k5-int.h"
#include "os-proto.h"
#include "fake-addrinfo.h"
#include <ctype.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#if !defined(DEFAULT_RDNS_LOOKUP)
#define DEFAULT_RDNS_LOOKUP 1
#endif

static krb5_boolean
use_reverse_dns(krb5_context context)
{
    krb5_error_code ret;
    int value;

    ret = profile_get_boolean(context->profile, KRB5_CONF_LIBDEFAULTS,
                              KRB5_CONF_RDNS, NULL, DEFAULT_RDNS_LOOKUP,
                              &value);
    if (ret)
        return DEFAULT_RDNS_LOOKUP;

    return value;
}

/* Append a domain suffix to host and return the result in allocated memory.
 * Return NULL if no suffix is configured or on failure. */
static char *
qualify_shortname(krb5_context context, const char *host)
{
    krb5_error_code ret;
    char *fqdn = NULL, *prof_domain = NULL, *os_domain = NULL;
    const char *domain;

    ret = profile_get_string(context->profile, KRB5_CONF_LIBDEFAULTS,
                             KRB5_CONF_QUALIFY_SHORTNAME, NULL, NULL,
                             &prof_domain);
    if (ret)
        return NULL;

#ifdef KRB5_DNS_LOOKUP
    if (prof_domain == NULL)
        os_domain = k5_primary_domain();
#endif

    domain = (prof_domain != NULL) ? prof_domain : os_domain;
    if (domain != NULL && *domain != '\0') {
        if (asprintf(&fqdn, "%s.%s", host, domain) < 0)
            fqdn = NULL;
    }

    profile_release_string(prof_domain);
    free(os_domain);
    return fqdn;
}

static krb5_error_code
expand_hostname(krb5_context context, const char *host, krb5_boolean use_dns,
                char **canonhost_out)
{
    struct addrinfo *ai = NULL, hint;
    char namebuf[NI_MAXHOST], *qualified = NULL, *copy, *p;
    int err;
    const char *canonhost;

    *canonhost_out = NULL;

    canonhost = host;
    if (use_dns) {
        /* Try a forward lookup of the hostname. */
        memset(&hint, 0, sizeof(hint));
        hint.ai_flags = AI_CANONNAME;
        err = getaddrinfo(host, 0, &hint, &ai);
        if (err == EAI_MEMORY)
            goto cleanup;
        if (!err && ai->ai_canonname != NULL)
            canonhost = ai->ai_canonname;

        if (!err && use_reverse_dns(context)) {
            /* Try a reverse lookup of the address. */
            err = getnameinfo(ai->ai_addr, ai->ai_addrlen, namebuf,
                              sizeof(namebuf), NULL, 0, NI_NAMEREQD);
            if (err == EAI_MEMORY)
                goto cleanup;
            if (!err)
                canonhost = namebuf;
        }
    }

    /* If we didn't use DNS and the name is just one component, try to add a
     * domain suffix. */
    if (canonhost == host && strchr(host, '.') == NULL) {
        qualified = qualify_shortname(context, host);
        if (qualified != NULL)
            canonhost = qualified;
    }

    copy = strdup(canonhost);
    if (copy == NULL)
        goto cleanup;

    /* Convert the hostname to lower case. */
    for (p = copy; *p != '\0'; p++) {
        if (isupper((unsigned char)*p))
            *p = tolower((unsigned char)*p);
    }

    /* Remove any trailing dot. */
    if (copy[0] != '\0') {
        p = copy + strlen(copy) - 1;
        if (*p == '.')
            *p = '\0';
    }

    *canonhost_out = copy;

cleanup:
    /* We only return success or ENOMEM. */
    if (ai != NULL)
        freeaddrinfo(ai);
    free(qualified);
    return (*canonhost_out == NULL) ? ENOMEM : 0;
}

krb5_error_code KRB5_CALLCONV
krb5_expand_hostname(krb5_context context, const char *host,
                     char **canonhost_out)
{
    int use_dns = (context->dns_canonicalize_hostname == CANONHOST_TRUE);

    return expand_hostname(context, host, use_dns, canonhost_out);
}

/* Split data into hostname and trailer (:port or :instance).  Trailers are
 * used in MSSQLSvc principals. */
static void
split_trailer(const krb5_data *data, krb5_data *host, krb5_data *trailer)
{
    char *p = memchr(data->data, ':', data->length);
    unsigned int tlen = (p == NULL) ? 0 : data->length - (p - data->data);

    /* Make sure we have a single colon followed by one or more characters.  An
     * IPv6 address will have more than one colon, so don't accept that. */
    if (p == NULL || tlen == 1 || memchr(p + 1, ':', tlen - 1) != NULL) {
        *host = *data;
        *trailer = make_data("", 0);
    } else {
        *host = make_data(data->data, p - data->data);
        *trailer = make_data(p, tlen);
    }
}

static krb5_error_code
canonicalize_princ(krb5_context context, struct canonprinc *iter,
                   krb5_boolean use_dns, krb5_const_principal *princ_out)
{
    krb5_error_code ret;
    krb5_data host, trailer;
    char *hostname = NULL, *canonhost = NULL, *combined = NULL;
    char **hrealms = NULL;

    *princ_out = NULL;

    assert(iter->princ->length == 2);
    split_trailer(&iter->princ->data[1], &host, &trailer);

    hostname = k5memdup0(host.data, host.length, &ret);
    if (hostname == NULL)
        goto cleanup;

    if (iter->princ->type == KRB5_NT_SRV_HST) {
        /* Expand the hostname with or without DNS as specified. */
        ret = expand_hostname(context, hostname, use_dns, &canonhost);
        if (ret)
            goto cleanup;
    } else {
        canonhost = strdup(hostname);
        if (canonhost == NULL) {
            ret = ENOMEM;
            goto cleanup;
        }
    }

    /* Add the trailer to the expanded hostname. */
    if (asprintf(&combined, "%s%.*s", canonhost,
                 trailer.length, trailer.data) < 0) {
        combined = NULL;
        ret = ENOMEM;
        goto cleanup;
    }

    /* Don't yield the same host part twice. */
    if (iter->canonhost != NULL && strcmp(iter->canonhost, combined) == 0)
        goto cleanup;

    free(iter->canonhost);
    iter->canonhost = combined;
    combined = NULL;

    /* If the realm is unknown, look up the realm of the expanded hostname. */
    if (iter->princ->realm.length == 0 && !iter->no_hostrealm) {
        ret = krb5_get_host_realm(context, canonhost, &hrealms);
        if (ret)
            goto cleanup;
        if (hrealms[0] == NULL) {
            ret = KRB5_ERR_HOST_REALM_UNKNOWN;
            goto cleanup;
        }
        free(iter->realm);
        if (*hrealms[0] == '\0' && iter->subst_defrealm) {
            ret = krb5_get_default_realm(context, &iter->realm);
            if (ret)
                goto cleanup;
        } else {
            iter->realm = strdup(hrealms[0]);
            if (iter->realm == NULL) {
                ret = ENOMEM;
                goto cleanup;
            }
        }
    }

    iter->copy = *iter->princ;
    if (iter->realm != NULL)
        iter->copy.realm = string2data(iter->realm);
    iter->components[0] = iter->princ->data[0];
    iter->components[1] = string2data(iter->canonhost);
    iter->copy.data = iter->components;
    *princ_out = &iter->copy;

cleanup:
    free(hostname);
    free(canonhost);
    free(combined);
    krb5_free_host_realm(context, hrealms);
    return ret;
}

krb5_error_code
k5_canonprinc(krb5_context context, struct canonprinc *iter,
              krb5_const_principal *princ_out)
{
    krb5_error_code ret;
    int step = ++iter->step;

    *princ_out = NULL;

    /* If the hostname isn't from krb5_sname_to_principal(), the input
     * principal is canonical. */
    if (iter->princ->type != KRB5_NT_SRV_HST || iter->princ->length != 2 ||
        iter->princ->data[1].length == 0) {
        *princ_out = (step == 1) ? iter->princ : NULL;
        return 0;
    }

    /* If we're not doing fallback, the hostname is canonical, but we may need
     * to substitute the default realm. */
    if (context->dns_canonicalize_hostname != CANONHOST_FALLBACK) {
        if (step > 1)
            return 0;
        iter->copy = *iter->princ;
        if (iter->subst_defrealm && iter->copy.realm.length == 0) {
            ret = krb5_get_default_realm(context, &iter->realm);
            if (ret)
                return ret;
            iter->copy = *iter->princ;
            iter->copy.realm = string2data(iter->realm);
        }
        *princ_out = &iter->copy;
        return 0;
    }

    /* Canonicalize without DNS at step 1, with DNS at step 2. */
    if (step > 2)
        return 0;
    return canonicalize_princ(context, iter, step == 2, princ_out);
}

krb5_boolean
k5_sname_compare(krb5_context context, krb5_const_principal sname,
                 krb5_const_principal princ)
{
    krb5_error_code ret;
    struct canonprinc iter = { sname, .subst_defrealm = TRUE };
    krb5_const_principal canonprinc = NULL;
    krb5_boolean match = FALSE;

    while ((ret = k5_canonprinc(context, &iter, &canonprinc)) == 0 &&
           canonprinc != NULL) {
        if (krb5_principal_compare(context, canonprinc, princ)) {
            match = TRUE;
            break;
        }
    }
    free_canonprinc(&iter);
    return match;
}

krb5_error_code KRB5_CALLCONV
krb5_sname_to_principal(krb5_context context, const char *hostname,
                        const char *sname, krb5_int32 type,
                        krb5_principal *princ_out)
{
    krb5_error_code ret;
    krb5_principal princ;
    krb5_const_principal cprinc;
    krb5_boolean use_dns;
    char localname[MAXHOSTNAMELEN];
    struct canonprinc iter = { NULL };

    *princ_out = NULL;

    if (type != KRB5_NT_UNKNOWN && type != KRB5_NT_SRV_HST)
        return KRB5_SNAME_UNSUPP_NAMETYPE;

    /* If hostname is NULL, use the local hostname. */
    if (hostname == NULL) {
        if (gethostname(localname, MAXHOSTNAMELEN) != 0)
            return SOCKET_ERRNO;
        hostname = localname;
    }

    /* If sname is NULL, use "host". */
    if (sname == NULL)
        sname = "host";

    /* Build an initial principal with what we have. */
    ret = krb5_build_principal(context, &princ, 0, KRB5_REFERRAL_REALM,
                               sname, hostname, (char *)NULL);
    if (ret)
        return ret;
    princ->type = type;

    if (type == KRB5_NT_SRV_HST &&
        context->dns_canonicalize_hostname == CANONHOST_FALLBACK) {
        /* Delay canonicalization and realm lookup until use. */
        *princ_out = princ;
        return 0;
    }

    use_dns = (context->dns_canonicalize_hostname == CANONHOST_TRUE);
    iter.princ = princ;
    ret = canonicalize_princ(context, &iter, use_dns, &cprinc);
    if (!ret)
        ret = krb5_copy_principal(context, cprinc, princ_out);
    free_canonprinc(&iter);
    krb5_free_principal(context, princ);
    return ret;
}
