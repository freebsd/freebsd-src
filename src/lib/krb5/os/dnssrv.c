/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/dnssrv.c - Perform DNS SRV queries */
/*
 * Copyright 1990,2000,2001,2002,2003 by the Massachusetts Institute of Technology.
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

#include "autoconf.h"
#ifdef KRB5_DNS_LOOKUP

#include "dnsglue.h"

/*
 * Lookup a KDC via DNS SRV records
 */

void
krb5int_free_srv_dns_data (struct srv_dns_entry *p)
{
    struct srv_dns_entry *next;
    while (p) {
        next = p->next;
        free(p->host);
        free(p);
        p = next;
    }
}

/* Construct a DNS label of the form "service.[protocol.]realm.", placing the
 * result into fixed_buf.  protocol may be NULL. */
static krb5_error_code
prepare_lookup_buf(const krb5_data *realm, const char *service,
                   const char *protocol, char *fixed_buf, size_t bufsize)
{
    struct k5buf buf;

    if (memchr(realm->data, 0, realm->length))
        return EINVAL;

    k5_buf_init_fixed(&buf, fixed_buf, bufsize);
    k5_buf_add_fmt(&buf, "%s.", service);
    if (protocol != NULL)
        k5_buf_add_fmt(&buf, "%s.", protocol);
    k5_buf_add_len(&buf, realm->data, realm->length);

    /*
     * Realm names don't (normally) end with ".", but if the query doesn't end
     * with "." and doesn't get an answer as is, the resolv code will try
     * appending the local domain.  Since the realm names are absolutes, let's
     * stop that.
     */

    if (buf.len > 0 && ((char *)buf.data)[buf.len - 1] != '.')
        k5_buf_add(&buf, ".");

    return k5_buf_status(&buf);
}

/* Insert new into the list *head, ordering by priority.  Weight is not
 * currently used. */
static void
place_srv_entry(struct srv_dns_entry **head, struct srv_dns_entry *new)
{
    struct srv_dns_entry *entry;

    if (*head == NULL || (*head)->priority > new->priority) {
        new->next = *head;
        *head = new;
        return;
    }

    for (entry = *head; entry != NULL; entry = entry->next) {
        /*
         * Insert an entry into the next spot if there is no next entry (we're
         * at the end), or if the next entry has a higher priority (lower
         * priorities are preferred).
         */
        if (entry->next == NULL || entry->next->priority > new->priority) {
            new->next = entry->next;
            entry->next = new;
            break;
        }
    }
}

/* Query the URI RR, collecting weight, priority, and target. */
krb5_error_code
k5_make_uri_query(const krb5_data *realm, const char *service,
                  struct srv_dns_entry **answers)
{
    const unsigned char *p = NULL, *base = NULL;
    char host[MAXDNAME];
    int size, ret, rdlen;
    unsigned short priority, weight;
    struct krb5int_dns_state *ds = NULL;
    struct srv_dns_entry *head = NULL, *uri = NULL;

    *answers = NULL;

    /* Construct service.realm. */
    ret = prepare_lookup_buf(realm, service, NULL, host, sizeof(host));
    if (ret)
        return 0;

    size = krb5int_dns_init(&ds, host, C_IN, T_URI);
    if (size < 0)
        goto out;

    for (;;) {
        ret = krb5int_dns_nextans(ds, &base, &rdlen);
        if (ret < 0 || base == NULL)
            goto out;

        p = base;

        SAFE_GETUINT16(base, rdlen, p, 2, priority, out);
        SAFE_GETUINT16(base, rdlen, p, 2, weight, out);

        uri = k5alloc(sizeof(*uri), &ret);
        if (uri == NULL)
            goto out;

        uri->priority = priority;
        uri->weight = weight;
        /* rdlen - 4 bytes remain after the priority and weight. */
        uri->host = k5memdup0(p, rdlen - 4, &ret);
        if (uri->host == NULL) {
            ret = errno;
            goto out;
        }

        place_srv_entry(&head, uri);
    }

out:
    krb5int_dns_fini(ds);
    *answers = head;
    return 0;
}

/*
 * Do DNS SRV query, return results in *answers.
 *
 * Make a best effort to return all the data we can.  On memory or decoding
 * errors, just return what we've got.  Always return 0, currently.
 */

krb5_error_code
krb5int_make_srv_query_realm(const krb5_data *realm,
                             const char *service,
                             const char *protocol,
                             struct srv_dns_entry **answers)
{
    const unsigned char *p = NULL, *base = NULL;
    char host[MAXDNAME];
    int size, ret, rdlen, nlen;
    unsigned short priority, weight, port;
    struct krb5int_dns_state *ds = NULL;
    struct srv_dns_entry *head = NULL, *srv = NULL;

    /*
     * First off, build a query of the form:
     *
     * service.protocol.realm
     *
     * which will most likely be something like:
     *
     * _kerberos._udp.REALM
     *
     */

    ret = prepare_lookup_buf(realm, service, protocol, host, sizeof(host));
    if (ret)
        return 0;

#ifdef TEST
    fprintf(stderr, "sending DNS SRV query for %s\n", host);
#endif

    size = krb5int_dns_init(&ds, host, C_IN, T_SRV);
    if (size < 0)
        goto out;

    for (;;) {
        ret = krb5int_dns_nextans(ds, &base, &rdlen);
        if (ret < 0 || base == NULL)
            goto out;

        p = base;

        SAFE_GETUINT16(base, rdlen, p, 2, priority, out);
        SAFE_GETUINT16(base, rdlen, p, 2, weight, out);
        SAFE_GETUINT16(base, rdlen, p, 2, port, out);

        /*
         * RFC 2782 says the target is never compressed in the reply;
         * do we believe that?  We need to flatten it anyway, though.
         */
        nlen = krb5int_dns_expand(ds, p, host, sizeof(host));
        if (nlen < 0 || !INCR_OK(base, rdlen, p, nlen))
            goto out;

        /*
         * We got everything!  Insert it into our list, but make sure
         * it's in the right order.  Right now we don't do anything
         * with the weight field
         */

        srv = malloc(sizeof(struct srv_dns_entry));
        if (srv == NULL)
            goto out;

        srv->priority = priority;
        srv->weight = weight;
        srv->port = port;
        /* The returned names are fully qualified.  Don't let the
         * local resolver code do domain search path stuff. */
        if (asprintf(&srv->host, "%s.", host) < 0) {
            free(srv);
            goto out;
        }

        place_srv_entry(&head, srv);
    }

out:
    krb5int_dns_fini(ds);
    *answers = head;
    return 0;
}
#endif
