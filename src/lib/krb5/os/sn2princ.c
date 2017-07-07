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

krb5_error_code KRB5_CALLCONV
krb5_expand_hostname(krb5_context context, const char *host,
                     char **canonhost_out)
{
    struct addrinfo *ai = NULL, hint;
    char namebuf[NI_MAXHOST], *copy, *p;
    int err;
    const char *canonhost;

    *canonhost_out = NULL;

    canonhost = host;
    if (context->dns_canonicalize_hostname) {
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
    return (*canonhost_out == NULL) ? ENOMEM : 0;
}

/* If hostname appears to have a :port or :instance trailer (used in MSSQLSvc
 * principals), return a pointer to the separator.  Otherwise return NULL. */
static const char *
find_trailer(const char *hostname)
{
    const char *p = strchr(hostname, ':');

    /* Look for a single colon followed by one or more characters.  An IPv6
     * address will have more than one colon, so don't accept that. */
    if (p == NULL || p[1] == '\0' || strchr(p + 1, ':') != NULL)
        return NULL;
    return p;
}

krb5_error_code KRB5_CALLCONV
krb5_sname_to_principal(krb5_context context, const char *hostname,
                        const char *sname, krb5_int32 type,
                        krb5_principal *princ_out)
{
    krb5_error_code ret;
    krb5_principal princ;
    const char *realm, *trailer;
    char **hrealms = NULL, *canonhost = NULL, *hostonly = NULL, *concat = NULL;
    char localname[MAXHOSTNAMELEN];

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

    /* If there is a trailer, remove it for now. */
    trailer = find_trailer(hostname);
    if (trailer != NULL) {
        hostonly = k5memdup0(hostname, trailer - hostname, &ret);
        if (hostonly == NULL)
            goto cleanup;
        hostname = hostonly;
    }

    /* Canonicalize the hostname if appropriate. */
    if (type == KRB5_NT_SRV_HST) {
        ret = krb5_expand_hostname(context, hostname, &canonhost);
        if (ret)
            goto cleanup;
        hostname = canonhost;
    }

    /* Find the realm of the host. */
    ret = krb5_get_host_realm(context, hostname, &hrealms);
    if (ret)
        goto cleanup;
    if (hrealms[0] == NULL) {
        ret = KRB5_ERR_HOST_REALM_UNKNOWN;
        goto cleanup;
    }
    realm = hrealms[0];

    /* If there was a trailer, put it back on the end. */
    if (trailer != NULL) {
        if (asprintf(&concat, "%s%s", hostname, trailer) < 0) {
            ret = ENOMEM;
            goto cleanup;
        }
        hostname = concat;
    }

    ret = krb5_build_principal(context, &princ, strlen(realm), realm, sname,
                               hostname, (char *)NULL);
    if (ret)
        goto cleanup;

    princ->type = type;
    *princ_out = princ;

cleanup:
    free(hostonly);
    free(canonhost);
    free(concat);
    krb5_free_host_realm(context, hrealms);
    return ret;
}
