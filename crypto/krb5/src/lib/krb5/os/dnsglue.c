/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/dnsglue.c */
/*
 * Copyright 2004, 2009 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include "os-proto.h"

#ifdef KRB5_DNS_LOOKUP

#ifndef _WIN32

#include "dnsglue.h"
#ifdef __APPLE__
#include <dns.h>
#endif

/*
 * Only use res_ninit() if there's also a res_ndestroy(), to avoid
 * memory leaks (Linux & Solaris) and outright corruption (AIX 4.x,
 * 5.x).  While we're at it, make sure res_nsearch() is there too.
 *
 * In any case, it is probable that platforms having broken
 * res_ninit() will have thread safety hacks for res_init() and _res.
 */

/*
 * Opaque handle
 */
struct krb5int_dns_state {
    int nclass;
    int ntype;
    void *ansp;
    int anslen;
    int ansmax;
#if HAVE_NS_INITPARSE
    int cur_ans;
    ns_msg msg;
#else
    unsigned char *ptr;
    unsigned short nanswers;
#endif
};

#if !HAVE_NS_INITPARSE
static int initparse(struct krb5int_dns_state *);
#endif

/*
 * Define macros to use the best available DNS search functions.  INIT_HANDLE()
 * returns true if handle initialization is successful, false if it is not.
 * SEARCH() returns the length of the response or -1 on error.
 * PRIMARY_DOMAIN() returns the first search domain in allocated memory.
 * DECLARE_HANDLE() must be used last in the declaration list since it may
 * evaluate to nothing.
 */

#if defined(__APPLE__)

/* Use the macOS interfaces dns_open, dns_search, and dns_free. */
#define DECLARE_HANDLE(h) dns_handle_t h
#define INIT_HANDLE(h) ((h = dns_open(NULL)) != NULL)
#define SEARCH(h, n, c, t, a, l) dns_search(h, n, c, t, a, l, NULL, NULL)
#define PRIMARY_DOMAIN(h) dns_search_list_domain(h, 0)
#define DESTROY_HANDLE(h) dns_free(h)

#elif HAVE_RES_NINIT && HAVE_RES_NSEARCH

/* Use res_ninit, res_nsearch, and res_ndestroy or res_nclose. */
#define DECLARE_HANDLE(h) struct __res_state h
#define INIT_HANDLE(h) (memset(&h, 0, sizeof(h)), res_ninit(&h) == 0)
#define SEARCH(h, n, c, t, a, l) res_nsearch(&h, n, c, t, a, l)
#define PRIMARY_DOMAIN(h) ((h.dnsrch[0] == NULL) ? NULL : strdup(h.dnsrch[0]))
#if HAVE_RES_NDESTROY
#define DESTROY_HANDLE(h) res_ndestroy(&h)
#else
#define DESTROY_HANDLE(h) res_nclose(&h)
#endif

#else

/* Use res_init and res_search. */
#define DECLARE_HANDLE(h)
#define INIT_HANDLE(h) (res_init() == 0)
#define SEARCH(h, n, c, t, a, l) res_search(n, c, t, a, l)
#define PRIMARY_DOMAIN(h) \
    ((_res.defdname == NULL) ? NULL : strdup(_res.defdname))
#define DESTROY_HANDLE(h)

#endif

/*
 * krb5int_dns_init()
 *
 * Initialize an opaque handle.  Do name lookup and initial parsing of
 * reply, skipping question section.  Prepare to iterate over answer
 * section.  Returns -1 on error, 0 on success.
 */
int
krb5int_dns_init(struct krb5int_dns_state **dsp,
                 char *host, int nclass, int ntype)
{
    struct krb5int_dns_state *ds;
    int len, ret;
    size_t nextincr, maxincr;
    unsigned char *p;
    DECLARE_HANDLE(h);

    *dsp = ds = malloc(sizeof(*ds));
    if (ds == NULL)
        return -1;

    ret = -1;
    ds->nclass = nclass;
    ds->ntype = ntype;
    ds->ansp = NULL;
    ds->anslen = 0;
    ds->ansmax = 0;
    nextincr = 4096;
    maxincr = INT_MAX;

#if HAVE_NS_INITPARSE
    ds->cur_ans = 0;
#endif

    if (!INIT_HANDLE(h))
        return -1;

    do {
        p = (ds->ansp == NULL)
            ? malloc(nextincr) : realloc(ds->ansp, nextincr);

        if (p == NULL) {
            ret = -1;
            goto errout;
        }
        ds->ansp = p;
        ds->ansmax = nextincr;

        len = SEARCH(h, host, ds->nclass, ds->ntype, ds->ansp, ds->ansmax);
        if ((size_t) len > maxincr) {
            ret = -1;
            goto errout;
        }
        while (nextincr < (size_t) len)
            nextincr *= 2;
        if (len < 0 || nextincr > maxincr) {
            ret = -1;
            goto errout;
        }
    } while (len > ds->ansmax);

    ds->anslen = len;
#if HAVE_NS_INITPARSE
    ret = ns_initparse(ds->ansp, ds->anslen, &ds->msg);
#else
    ret = initparse(ds);
#endif
    if (ret < 0)
        goto errout;

    ret = 0;

errout:
    DESTROY_HANDLE(h);
    if (ret < 0) {
        if (ds->ansp != NULL) {
            free(ds->ansp);
            ds->ansp = NULL;
        }
    }

    return ret;
}

#if HAVE_NS_INITPARSE
/*
 * krb5int_dns_nextans - get next matching answer record
 *
 * Sets pp to NULL if no more records.  Returns -1 on error, 0 on
 * success.
 */
int
krb5int_dns_nextans(struct krb5int_dns_state *ds,
                    const unsigned char **pp, int *lenp)
{
    int len;
    ns_rr rr;

    *pp = NULL;
    *lenp = 0;
    while (ds->cur_ans < ns_msg_count(ds->msg, ns_s_an)) {
        len = ns_parserr(&ds->msg, ns_s_an, ds->cur_ans, &rr);
        if (len < 0)
            return -1;
        ds->cur_ans++;
        if (ds->nclass == (int)ns_rr_class(rr)
            && ds->ntype == (int)ns_rr_type(rr)) {
            *pp = ns_rr_rdata(rr);
            *lenp = ns_rr_rdlen(rr);
            return 0;
        }
    }
    return 0;
}
#endif

/*
 * krb5int_dns_expand - wrapper for dn_expand()
 */
int
krb5int_dns_expand(struct krb5int_dns_state *ds, const unsigned char *p,
                   char *buf, int len)
{

#if HAVE_NS_NAME_UNCOMPRESS
    return ns_name_uncompress(ds->ansp,
                              (unsigned char *)ds->ansp + ds->anslen,
                              p, buf, (size_t)len);
#else
    return dn_expand(ds->ansp,
                     (unsigned char *)ds->ansp + ds->anslen,
                     p, buf, len);
#endif
}

/*
 * Free stuff.
 */
void
krb5int_dns_fini(struct krb5int_dns_state *ds)
{
    if (ds == NULL)
        return;
    if (ds->ansp != NULL)
        free(ds->ansp);
    free(ds);
}

/*
 * Compat routines for BIND 4
 */
#if !HAVE_NS_INITPARSE

/*
 * initparse
 *
 * Skip header and question section of reply.  Set a pointer to the
 * beginning of the answer section, and prepare to iterate over
 * answer records.
 */
static int
initparse(struct krb5int_dns_state *ds)
{
    HEADER *hdr;
    unsigned char *p;
    unsigned short nqueries, nanswers;
    int len;
#if !HAVE_DN_SKIPNAME
    char host[MAXDNAME];
#endif

    if ((size_t) ds->anslen < sizeof(HEADER))
        return -1;

    hdr = (HEADER *)ds->ansp;
    p = ds->ansp;
    nqueries = ntohs((unsigned short)hdr->qdcount);
    nanswers = ntohs((unsigned short)hdr->ancount);
    p += sizeof(HEADER);

    /*
     * Skip query records.
     */
    while (nqueries--) {
#if HAVE_DN_SKIPNAME
        len = dn_skipname(p, (unsigned char *)ds->ansp + ds->anslen);
#else
        len = dn_expand(ds->ansp, (unsigned char *)ds->ansp + ds->anslen,
                        p, host, sizeof(host));
#endif
        if (len < 0 || !INCR_OK(ds->ansp, ds->anslen, p, len + 4))
            return -1;
        p += len + 4;
    }
    ds->ptr = p;
    ds->nanswers = nanswers;
    return 0;
}

/*
 * krb5int_dns_nextans() - get next answer record
 *
 * Sets pp to NULL if no more records.
 */
int
krb5int_dns_nextans(struct krb5int_dns_state *ds,
                    const unsigned char **pp, int *lenp)
{
    int len;
    unsigned char *p;
    unsigned short ntype, nclass, rdlen;
#if !HAVE_DN_SKIPNAME
    char host[MAXDNAME];
#endif

    *pp = NULL;
    *lenp = 0;
    p = ds->ptr;

    while (ds->nanswers--) {
#if HAVE_DN_SKIPNAME
        len = dn_skipname(p, (unsigned char *)ds->ansp + ds->anslen);
#else
        len = dn_expand(ds->ansp, (unsigned char *)ds->ansp + ds->anslen,
                        p, host, sizeof(host));
#endif
        if (len < 0 || !INCR_OK(ds->ansp, ds->anslen, p, len))
            return -1;
        p += len;
        SAFE_GETUINT16(ds->ansp, ds->anslen, p, 2, ntype, out);
        /* Also skip 4 bytes of TTL */
        SAFE_GETUINT16(ds->ansp, ds->anslen, p, 6, nclass, out);
        SAFE_GETUINT16(ds->ansp, ds->anslen, p, 2, rdlen, out);

        if (!INCR_OK(ds->ansp, ds->anslen, p, rdlen))
            return -1;
        if (rdlen > INT_MAX)
            return -1;
        if (nclass == ds->nclass && ntype == ds->ntype) {
            *pp = p;
            *lenp = rdlen;
            ds->ptr = p + rdlen;
            return 0;
        }
        p += rdlen;
    }
    return 0;
out:
    return -1;
}

#endif /* !HAVE_NS_INITPARSE */
#endif /* not _WIN32 */

/* Construct a DNS label of the form "prefix[.name.]".  name may be NULL. */
static char *
txt_lookup_name(const char *prefix, const char *name)
{
    struct k5buf buf;

    k5_buf_init_dynamic(&buf);

    if (name == NULL || name[0] == '\0') {
        k5_buf_add(&buf, prefix);
    } else {
        k5_buf_add_fmt(&buf, "%s.%s", prefix, name);

        /*
         * Realm names don't (normally) end with ".", but if the query doesn't
         * end with "." and doesn't get an answer as is, the resolv code will
         * try appending the local domain.  Since the realm names are
         * absolutes, let's stop that.
         *
         * But only if a name has been specified.  If we are performing a
         * search on the prefix alone then the intention is to allow the local
         * domain or domain search lists to be expanded.
         */

        if (buf.len > 0 && ((char *)buf.data)[buf.len - 1] != '.')
            k5_buf_add(&buf, ".");
    }

    return k5_buf_cstring(&buf);
}

/*
 * Try to look up a TXT record pointing to a Kerberos realm
 */

#ifdef _WIN32

#include <windns.h>

krb5_error_code
k5_try_realm_txt_rr(krb5_context context, const char *prefix, const char *name,
                    char **realm)
{
    krb5_error_code ret = 0;
    char *txtname = NULL;
    PDNS_RECORD rr = NULL;
    DNS_STATUS st;

    *realm = NULL;

    txtname = txt_lookup_name(prefix, name);
    if (txtname == NULL)
        return ENOMEM;

    st = DnsQuery_UTF8(txtname, DNS_TYPE_TEXT, DNS_QUERY_STANDARD, NULL,
                       &rr, NULL);
    if (st != ERROR_SUCCESS || rr == NULL) {
        TRACE_TXT_LOOKUP_NOTFOUND(context, txtname);
        ret = KRB5_ERR_HOST_REALM_UNKNOWN;
        goto cleanup;
    }

    *realm = strdup(rr->Data.TXT.pStringArray[0]);
    if (*realm == NULL)
        ret = ENOMEM;
    TRACE_TXT_LOOKUP_SUCCESS(context, txtname, *realm);

cleanup:
    free(txtname);
    if (rr != NULL)
        DnsRecordListFree(rr, DnsFreeRecordList);
    return ret;
}

char *
k5_primary_domain()
{
    return NULL;
}

#else /* _WIN32 */

krb5_error_code
k5_try_realm_txt_rr(krb5_context context, const char *prefix, const char *name,
                    char **realm)
{
    krb5_error_code retval = KRB5_ERR_HOST_REALM_UNKNOWN;
    const unsigned char *p, *base;
    char *txtname = NULL;
    int ret, rdlen, len;
    struct krb5int_dns_state *ds = NULL;

    /*
     * Form our query, and send it via DNS
     */

    txtname = txt_lookup_name(prefix, name);
    if (txtname == NULL)
        return ENOMEM;
    ret = krb5int_dns_init(&ds, txtname, C_IN, T_TXT);
    if (ret < 0) {
        TRACE_TXT_LOOKUP_NOTFOUND(context, txtname);
        goto errout;
    }

    ret = krb5int_dns_nextans(ds, &base, &rdlen);
    if (ret < 0 || base == NULL)
        goto errout;

    p = base;
    if (!INCR_OK(base, rdlen, p, 1))
        goto errout;
    len = *p++;
    *realm = malloc((size_t)len + 1);
    if (*realm == NULL) {
        retval = ENOMEM;
        goto errout;
    }
    strncpy(*realm, (const char *)p, (size_t)len);
    (*realm)[len] = '\0';
    /* Avoid a common error. */
    if ( (*realm)[len-1] == '.' )
        (*realm)[len-1] = '\0';
    retval = 0;
    TRACE_TXT_LOOKUP_SUCCESS(context, txtname, *realm);

errout:
    krb5int_dns_fini(ds);
    free(txtname);
    return retval;
}

char *
k5_primary_domain()
{
    char *domain;
    DECLARE_HANDLE(h);

    if (!INIT_HANDLE(h))
        return NULL;
    domain = PRIMARY_DOMAIN(h);
    DESTROY_HANDLE(h);
    return domain;
}

#endif /* not _WIN32 */
#endif /* KRB5_DNS_LOOKUP */
