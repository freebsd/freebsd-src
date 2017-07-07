/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/locate_kdc.c - Get addresses for realm KDCs and other servers */
/*
 * Copyright 1990,2000,2001,2002,2003,2004,2006,2008 Massachusetts Institute of
 * Technology.  All Rights Reserved.
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
#include "fake-addrinfo.h"
#include "os-proto.h"
#ifdef KRB5_DNS_LOOKUP
#ifdef WSHELPER
#include <wshelper.h>
#else /* WSHELPER */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <netdb.h>
#endif /* WSHELPER */
#include "dnsglue.h"

#define DEFAULT_LOOKUP_KDC 1
#if KRB5_DNS_LOOKUP_REALM
#define DEFAULT_LOOKUP_REALM 1
#else
#define DEFAULT_LOOKUP_REALM 0
#endif
#define DEFAULT_URI_LOOKUP TRUE

static int
maybe_use_dns (krb5_context context, const char *name, int defalt)
{
    krb5_error_code code;
    char * value = NULL;
    int use_dns = 0;

    code = profile_get_string(context->profile, KRB5_CONF_LIBDEFAULTS,
                              name, 0, 0, &value);
    if (value == 0 && code == 0) {
        code = profile_get_string(context->profile, KRB5_CONF_LIBDEFAULTS,
                                  KRB5_CONF_DNS_FALLBACK, 0, 0, &value);
    }
    if (code)
        return defalt;

    if (value == 0)
        return defalt;

    use_dns = _krb5_conf_boolean(value);
    profile_release_string(value);
    return use_dns;
}

static krb5_boolean
use_dns_uri(krb5_context ctx)
{
    krb5_error_code ret;
    int use;

    ret = profile_get_boolean(ctx->profile, KRB5_CONF_LIBDEFAULTS,
                              KRB5_CONF_DNS_URI_LOOKUP, NULL,
                              DEFAULT_URI_LOOKUP, &use);
    return ret ? DEFAULT_URI_LOOKUP : use;
}

int
_krb5_use_dns_kdc(krb5_context context)
{
    return maybe_use_dns(context, KRB5_CONF_DNS_LOOKUP_KDC,
                         DEFAULT_LOOKUP_KDC);
}

int
_krb5_use_dns_realm(krb5_context context)
{
    return maybe_use_dns(context, KRB5_CONF_DNS_LOOKUP_REALM,
                         DEFAULT_LOOKUP_REALM);
}

#endif /* KRB5_DNS_LOOKUP */

/* Free up everything pointed to by the serverlist structure, but don't
 * free the structure itself. */
void
k5_free_serverlist (struct serverlist *list)
{
    size_t i;

    for (i = 0; i < list->nservers; i++) {
        free(list->servers[i].hostname);
        free(list->servers[i].uri_path);
    }
    free(list->servers);
    list->servers = NULL;
    list->nservers = 0;
}

#include <stdarg.h>
static inline void
Tprintf(const char *fmt, ...)
{
#ifdef TEST
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
#endif
}

/* Make room for a new server entry in list and return a pointer to the new
 * entry.  (Do not increment list->nservers.) */
static struct server_entry *
new_server_entry(struct serverlist *list)
{
    struct server_entry *newservers, *entry;
    size_t newspace = (list->nservers + 1) * sizeof(struct server_entry);

    newservers = realloc(list->servers, newspace);
    if (newservers == NULL)
        return NULL;
    list->servers = newservers;
    entry = &newservers[list->nservers];
    memset(entry, 0, sizeof(*entry));
    entry->master = -1;
    return entry;
}

/* Add an address entry to list. */
static int
add_addr_to_list(struct serverlist *list, k5_transport transport, int family,
                 size_t addrlen, struct sockaddr *addr)
{
    struct server_entry *entry;

    entry = new_server_entry(list);
    if (entry == NULL)
        return ENOMEM;
    entry->transport = transport;
    entry->family = family;
    entry->hostname = NULL;
    entry->uri_path = NULL;
    entry->addrlen = addrlen;
    memcpy(&entry->addr, addr, addrlen);
    list->nservers++;
    return 0;
}

/* Add a hostname entry to list. */
static int
add_host_to_list(struct serverlist *list, const char *hostname, int port,
                 k5_transport transport, int family, const char *uri_path,
                 int master)
{
    struct server_entry *entry;

    entry = new_server_entry(list);
    if (entry == NULL)
        return ENOMEM;
    entry->transport = transport;
    entry->family = family;
    entry->hostname = strdup(hostname);
    if (entry->hostname == NULL)
        goto oom;
    if (uri_path != NULL) {
        entry->uri_path = strdup(uri_path);
        if (entry->uri_path == NULL)
            goto oom;
    }
    entry->port = port;
    entry->master = master;
    list->nservers++;
    return 0;
oom:
    free(entry->hostname);
    entry->hostname = NULL;
    return ENOMEM;
}

static void
parse_uri_if_https(const char *host_or_uri, k5_transport *transport,
                   const char **host, const char **uri_path)
{
    char *cp;

    if (strncmp(host_or_uri, "https://", 8) == 0) {
        *transport = HTTPS;
        *host = host_or_uri + 8;

        cp = strchr(*host, '/');
        if (cp != NULL) {
            *cp = '\0';
            *uri_path = cp + 1;
        }
    }
}

/* Return true if server is identical to an entry in list. */
static krb5_boolean
server_list_contains(struct serverlist *list, struct server_entry *server)
{
    struct server_entry *ent;

    for (ent = list->servers; ent < list->servers + list->nservers; ent++) {
        if (server->hostname != NULL && ent->hostname != NULL &&
            strcmp(server->hostname, ent->hostname) == 0)
            return TRUE;
        if (server->hostname == NULL && ent->hostname == NULL &&
            server->addrlen == ent->addrlen &&
            memcmp(&server->addr, &ent->addr, server->addrlen) == 0)
            return TRUE;
    }
    return FALSE;
}

static krb5_error_code
locate_srv_conf_1(krb5_context context, const krb5_data *realm,
                  const char * name, struct serverlist *serverlist,
                  k5_transport transport, int udpport)
{
    const char *realm_srv_names[4];
    char **hostlist = NULL, *realmstr = NULL, *host = NULL;
    const char *hostspec;
    krb5_error_code code;
    int i, default_port;

    Tprintf("looking in krb5.conf for realm %s entry %s; ports %d,%d\n",
            realm->data, name, udpport);

    realmstr = k5memdup0(realm->data, realm->length, &code);
    if (realmstr == NULL)
        goto cleanup;

    realm_srv_names[0] = KRB5_CONF_REALMS;
    realm_srv_names[1] = realmstr;
    realm_srv_names[2] = name;
    realm_srv_names[3] = 0;
    code = profile_get_values(context->profile, realm_srv_names, &hostlist);
    if (code) {
        Tprintf("config file lookup failed: %s\n", error_message(code));
        if (code == PROF_NO_SECTION || code == PROF_NO_RELATION)
            code = 0;
        goto cleanup;
    }

    for (i = 0; hostlist[i]; i++) {
        int port_num;
        k5_transport this_transport = transport;
        const char *uri_path = NULL;

        hostspec = hostlist[i];
        Tprintf("entry %d is '%s'\n", i, hostspec);

        parse_uri_if_https(hostspec, &this_transport, &hostspec, &uri_path);

        default_port = (this_transport == HTTPS) ? 443 : udpport;
        code = k5_parse_host_string(hostspec, default_port, &host, &port_num);
        if (code == 0 && host == NULL)
            code = EINVAL;
        if (code)
            goto cleanup;

        code = add_host_to_list(serverlist, host, port_num, this_transport,
                                AF_UNSPEC, uri_path, -1);
        if (code)
            goto cleanup;

        free(host);
        host = NULL;
    }

cleanup:
    free(realmstr);
    free(host);
    profile_free_list(hostlist);
    return code;
}

#ifdef TEST
static krb5_error_code
krb5_locate_srv_conf(krb5_context context, const krb5_data *realm,
                     const char *name, struct serverlist *al, int udpport)
{
    krb5_error_code ret;

    ret = locate_srv_conf_1(context, realm, name, al, TCP_OR_UDP, udpport);
    if (ret)
        return ret;
    if (al->nservers == 0)        /* Couldn't resolve any KDC names */
        return KRB5_REALM_CANT_RESOLVE;
    return 0;
}
#endif

#ifdef KRB5_DNS_LOOKUP
static krb5_error_code
locate_srv_dns_1(const krb5_data *realm, const char *service,
                 const char *protocol, struct serverlist *serverlist)
{
    struct srv_dns_entry *head = NULL, *entry = NULL;
    krb5_error_code code = 0;
    k5_transport transport;

    code = krb5int_make_srv_query_realm(realm, service, protocol, &head);
    if (code)
        return 0;

    if (head == NULL)
        return 0;

    /* Check for the "." case indicating no support.  */
    if (head->next == NULL && head->host[0] == '\0') {
        code = KRB5_ERR_NO_SERVICE;
        goto cleanup;
    }

    for (entry = head; entry != NULL; entry = entry->next) {
        transport = (strcmp(protocol, "_tcp") == 0) ? TCP : UDP;
        code = add_host_to_list(serverlist, entry->host, entry->port,
                                transport, AF_UNSPEC, NULL, -1);
        if (code)
            goto cleanup;
    }

cleanup:
    krb5int_free_srv_dns_data(head);
    return code;
}
#endif

#include <krb5/locate_plugin.h>

#if TARGET_OS_MAC
static const char *objdirs[] = { KRB5_PLUGIN_BUNDLE_DIR,
                                 LIBDIR "/krb5/plugins/libkrb5",
                                 NULL }; /* should be a list */
#else
static const char *objdirs[] = { LIBDIR "/krb5/plugins/libkrb5", NULL };
#endif

struct module_callback_data {
    int out_of_mem;
    struct serverlist *list;
};

static int
module_callback(void *cbdata, int socktype, struct sockaddr *sa)
{
    struct module_callback_data *d = cbdata;
    size_t addrlen;
    k5_transport transport;

    if (socktype != SOCK_STREAM && socktype != SOCK_DGRAM)
        return 0;
    if (sa->sa_family == AF_INET)
        addrlen = sizeof(struct sockaddr_in);
    else if (sa->sa_family == AF_INET6)
        addrlen = sizeof(struct sockaddr_in6);
    else
        return 0;
    transport = (socktype == SOCK_STREAM) ? TCP : UDP;
    if (add_addr_to_list(d->list, transport, sa->sa_family, addrlen,
                         sa) != 0) {
        /* Assumes only error is ENOMEM.  */
        d->out_of_mem = 1;
        return 1;
    }
    return 0;
}

static krb5_error_code
module_locate_server(krb5_context ctx, const krb5_data *realm,
                     struct serverlist *serverlist,
                     enum locate_service_type svc, k5_transport transport)
{
    struct krb5plugin_service_locate_result *res = NULL;
    krb5_error_code code;
    struct krb5plugin_service_locate_ftable *vtbl = NULL;
    void **ptrs;
    char *realmz;               /* NUL-terminated realm */
    int socktype, i;
    struct module_callback_data cbdata = { 0, };
    const char *msg;

    Tprintf("in module_locate_server\n");
    cbdata.list = serverlist;
    if (!PLUGIN_DIR_OPEN(&ctx->libkrb5_plugins)) {

        code = krb5int_open_plugin_dirs(objdirs, NULL, &ctx->libkrb5_plugins,
                                        &ctx->err);
        if (code)
            return KRB5_PLUGIN_NO_HANDLE;
    }

    code = krb5int_get_plugin_dir_data(&ctx->libkrb5_plugins,
                                       "service_locator", &ptrs, &ctx->err);
    if (code) {
        Tprintf("error looking up plugin symbols: %s\n",
                (msg = krb5_get_error_message(ctx, code)));
        krb5_free_error_message(ctx, msg);
        return KRB5_PLUGIN_NO_HANDLE;
    }

    if (realm->length >= UINT_MAX) {
        krb5int_free_plugin_dir_data(ptrs);
        return ENOMEM;
    }
    realmz = k5memdup0(realm->data, realm->length, &code);
    if (realmz == NULL) {
        krb5int_free_plugin_dir_data(ptrs);
        return code;
    }
    for (i = 0; ptrs[i]; i++) {
        void *blob;

        vtbl = ptrs[i];
        Tprintf("element %d is %p\n", i, ptrs[i]);

        /* For now, don't keep the plugin data alive.  For long-lived
         * contexts, it may be desirable to change that later. */
        code = vtbl->init(ctx, &blob);
        if (code)
            continue;

        socktype = (transport == TCP) ? SOCK_STREAM : SOCK_DGRAM;
        code = vtbl->lookup(blob, svc, realmz, socktype, AF_UNSPEC,
                            module_callback, &cbdata);
        /* Also ask for TCP addresses if we got UDP addresses and want both. */
        if (code == 0 && transport == TCP_OR_UDP) {
            code = vtbl->lookup(blob, svc, realmz, SOCK_STREAM, AF_UNSPEC,
                                module_callback, &cbdata);
            if (code == KRB5_PLUGIN_NO_HANDLE)
                code = 0;
        }
        vtbl->fini(blob);
        if (code == KRB5_PLUGIN_NO_HANDLE) {
            /* Module passes, keep going.  */
            /* XXX */
            Tprintf("plugin doesn't handle this realm (KRB5_PLUGIN_NO_HANDLE)"
                    "\n");
            continue;
        }
        if (code != 0) {
            /* Module encountered an actual error.  */
            Tprintf("plugin lookup routine returned error %d: %s\n",
                    code, error_message(code));
            free(realmz);
            krb5int_free_plugin_dir_data(ptrs);
            return code;
        }
        break;
    }
    if (ptrs[i] == NULL) {
        Tprintf("ran off end of plugin list\n");
        free(realmz);
        krb5int_free_plugin_dir_data(ptrs);
        return KRB5_PLUGIN_NO_HANDLE;
    }
    Tprintf("stopped with plugin #%d, res=%p\n", i, res);

    /* Got something back, yippee.  */
    Tprintf("now have %lu addrs in list %p\n",
            (unsigned long)serverlist->nservers, serverlist);
    free(realmz);
    krb5int_free_plugin_dir_data(ptrs);
    return 0;
}

static krb5_error_code
prof_locate_server(krb5_context context, const krb5_data *realm,
                   struct serverlist *serverlist, enum locate_service_type svc,
                   k5_transport transport)
{
    const char *profname;
    int dflport = 0;
    struct servent *serv;

    switch (svc) {
    case locate_service_kdc:
        profname = KRB5_CONF_KDC;
        /* We used to use /etc/services for these, but enough systems have old,
         * crufty, wrong settings that this is probably better. */
    kdc_ports:
        dflport = KRB5_DEFAULT_PORT;
        break;
    case locate_service_master_kdc:
        profname = KRB5_CONF_MASTER_KDC;
        goto kdc_ports;
    case locate_service_kadmin:
        profname = KRB5_CONF_ADMIN_SERVER;
        dflport = DEFAULT_KADM5_PORT;
        break;
    case locate_service_krb524:
        profname = KRB5_CONF_KRB524_SERVER;
        serv = getservbyname("krb524", "udp");
        dflport = serv ? serv->s_port : 4444;
        break;
    case locate_service_kpasswd:
        profname = KRB5_CONF_KPASSWD_SERVER;
        dflport = DEFAULT_KPASSWD_PORT;
        break;
    default:
        return EBUSY;           /* XXX */
    }

    return locate_srv_conf_1(context, realm, profname, serverlist, transport,
                             dflport);
}

#ifdef KRB5_DNS_LOOKUP

/*
 * Parse the initial part of the URI, first confirming the scheme name.  Get
 * the transport, flags (indicating master status), and host.  The host is
 * either an address or hostname with an optional port, or an HTTPS URL.
 * The format is krb5srv:flags:udp|tcp|kkdcp:host
 *
 * Return a NULL *host_out if there are any problems parsing the URI.
 */
static void
parse_uri_fields(const char *uri, k5_transport *transport_out,
                 const char **host_out, int *master_out)

{
    k5_transport transport;
    int master = FALSE;

    *transport_out = 0;
    *host_out = NULL;
    *master_out = -1;

    /* Confirm the scheme name. */
    if (strncasecmp(uri, "krb5srv", 7) != 0)
        return;

    uri += 7;
    if (*uri != ':')
        return;

    uri++;
    if (*uri == '\0')
        return;

    /* Check the flags field for supported flags. */
    for (; *uri != ':' && *uri != '\0'; uri++) {
        if (*uri == 'm' || *uri == 'M')
            master = TRUE;
    }
    if (*uri != ':')
        return;

    /* Look for the transport type. */
    uri++;
    if (strncasecmp(uri, "udp", 3) == 0) {
        transport = UDP;
        uri += 3;
    } else if (strncasecmp(uri, "tcp", 3) == 0) {
        transport = TCP;
        uri += 3;
    } else if (strncasecmp(uri, "kkdcp", 5) == 0) {
        /* Currently the only MS-KKDCP transport type is HTTPS. */
        transport = HTTPS;
        uri += 5;
    } else {
        return;
    }

    if (*uri != ':')
        return;

    /* The rest of the URI is the host (with optional port) or URI. */
    *host_out = uri + 1;
    *transport_out = transport;
    *master_out = master;
}

/*
 * Collect a list of servers from DNS URI records, for the requested service
 * and transport type.  Problematic entries are skipped.
 */
static krb5_error_code
locate_uri(const krb5_data *realm, const char *req_service,
           struct serverlist *serverlist, k5_transport req_transport,
           int default_port, krb5_boolean master_only)
{
    krb5_error_code ret;
    k5_transport transport, host_trans;
    struct srv_dns_entry *answers, *entry;
    char *host;
    const char *host_field, *path;
    int port, def_port, master;

    ret = k5_make_uri_query(realm, req_service, &answers);
    if (ret || answers == NULL)
        return ret;

    for (entry = answers; entry != NULL; entry = entry->next) {
        def_port = default_port;
        path = NULL;

        parse_uri_fields(entry->host, &transport, &host_field, &master);
        if (host_field == NULL)
            continue;

        /* TCP_OR_UDP allows entries of any transport type; otherwise
         * we're asking for a match. */
        if (req_transport != TCP_OR_UDP && req_transport != transport)
            continue;

        /* Process a MS-KKDCP target. */
        if (transport == HTTPS) {
            host_trans = 0;
            def_port = 443;
            parse_uri_if_https(host_field, &host_trans, &host_field, &path);
            if (host_trans != HTTPS)
                continue;
        }

        ret = k5_parse_host_string(host_field, def_port, &host, &port);
        if (ret == ENOMEM)
            break;

        if (ret || host == NULL) {
            ret = 0;
            continue;
        }

        ret = add_host_to_list(serverlist, host, port, transport, AF_UNSPEC,
                               path, master);
        free(host);
        if (ret)
            break;
    }

    krb5int_free_srv_dns_data(answers);
    return ret;
}

static krb5_error_code
dns_locate_server_uri(krb5_context context, const krb5_data *realm,
                      struct serverlist *serverlist,
                      enum locate_service_type svc, k5_transport transport)
{
    krb5_error_code ret;
    char *svcname;
    int def_port;
    krb5_boolean find_master = FALSE;

    if (!_krb5_use_dns_kdc(context) || !use_dns_uri(context))
        return 0;

    switch (svc) {
    case locate_service_master_kdc:
        find_master = TRUE;
        /* Fall through */
    case locate_service_kdc:
        svcname = "_kerberos";
        def_port = 88;
        break;
    case locate_service_kadmin:
        svcname = "_kerberos-adm";
        def_port = 749;
        break;
    case locate_service_kpasswd:
        svcname = "_kpasswd";
        def_port = 464;
        break;
    default:
        return 0;
    }

    ret = locate_uri(realm, svcname, serverlist, transport, def_port,
                     find_master);
    if (ret)
        Tprintf("dns URI lookup returned error %d\n", ret);

    return ret;
}

static krb5_error_code
dns_locate_server_srv(krb5_context context, const krb5_data *realm,
                      struct serverlist *serverlist,
                      enum locate_service_type svc, k5_transport transport)
{
    const char *dnsname;
    int use_dns = _krb5_use_dns_kdc(context);
    krb5_error_code code;

    if (!use_dns)
        return 0;

    switch (svc) {
    case locate_service_kdc:
        dnsname = "_kerberos";
        break;
    case locate_service_master_kdc:
        dnsname = "_kerberos-master";
        break;
    case locate_service_kadmin:
        dnsname = "_kerberos-adm";
        break;
    case locate_service_krb524:
        dnsname = "_krb524";
        break;
    case locate_service_kpasswd:
        dnsname = "_kpasswd";
        break;
    default:
        return 0;
    }

    code = 0;
    if (transport == UDP || transport == TCP_OR_UDP) {
        code = locate_srv_dns_1(realm, dnsname, "_udp", serverlist);
        if (code)
            Tprintf("dns udp lookup returned error %d\n", code);
    }
    if ((transport == TCP || transport == TCP_OR_UDP) && code == 0) {
        code = locate_srv_dns_1(realm, dnsname, "_tcp", serverlist);
        if (code)
            Tprintf("dns tcp lookup returned error %d\n", code);
    }
    return code;
}
#endif /* KRB5_DNS_LOOKUP */

/*
 * Try all of the server location methods in sequence.  transport must be
 * TCP_OR_UDP, TCP, or UDP.  It is applied to hostname entries in the profile
 * and affects whether we query modules or DNS for UDP or TCP or both, but does
 * not restrict a method from returning entries of other transports.
 */
static krb5_error_code
locate_server(krb5_context context, const krb5_data *realm,
              struct serverlist *serverlist, enum locate_service_type svc,
              k5_transport transport)
{
    krb5_error_code ret;
    struct serverlist list = SERVERLIST_INIT;

    *serverlist = list;

    /* Try modules.  If a module returns 0 but leaves the list empty, return an
     * empty list. */
    ret = module_locate_server(context, realm, &list, svc, transport);
    if (ret != KRB5_PLUGIN_NO_HANDLE)
        goto done;

    /* Try the profile.  Fall back to DNS if it returns an empty list. */
    ret = prof_locate_server(context, realm, &list, svc, transport);
    if (ret)
        goto done;

#ifdef KRB5_DNS_LOOKUP
    if (list.nservers == 0) {
        ret = dns_locate_server_uri(context, realm, &list, svc, transport);
        if (ret)
            goto done;
    }

    if (list.nservers == 0)
        ret = dns_locate_server_srv(context, realm, &list, svc, transport);
#endif

done:
    if (ret) {
        k5_free_serverlist(&list);
        return ret;
    }
    *serverlist = list;
    return 0;
}

/*
 * Wrapper function for the various backends
 */

krb5_error_code
k5_locate_server(krb5_context context, const krb5_data *realm,
                 struct serverlist *serverlist, enum locate_service_type svc,
                 krb5_boolean no_udp)
{
    krb5_error_code ret;
    k5_transport transport = no_udp ? TCP : TCP_OR_UDP;

    memset(serverlist, 0, sizeof(*serverlist));
    if (realm == NULL || realm->data == NULL || realm->data[0] == 0) {
        k5_setmsg(context, KRB5_REALM_CANT_RESOLVE,
                  "Cannot find KDC for invalid realm name \"\"");
        return KRB5_REALM_CANT_RESOLVE;
    }

    ret = locate_server(context, realm, serverlist, svc, transport);
    if (ret)
        return ret;

    if (serverlist->nservers == 0) {
        k5_free_serverlist(serverlist);
        k5_setmsg(context, KRB5_REALM_UNKNOWN,
                  _("Cannot find KDC for realm \"%.*s\""),
                  realm->length, realm->data);
        return KRB5_REALM_UNKNOWN;
    }
    return 0;
}

krb5_error_code
k5_locate_kdc(krb5_context context, const krb5_data *realm,
              struct serverlist *serverlist, krb5_boolean get_masters,
              krb5_boolean no_udp)
{
    enum locate_service_type stype;

    stype = get_masters ? locate_service_master_kdc : locate_service_kdc;
    return k5_locate_server(context, realm, serverlist, stype, no_udp);
}

krb5_boolean
k5_kdc_is_master(krb5_context context, const krb5_data *realm,
                 struct server_entry *server)
{
    struct serverlist list;
    krb5_boolean found;

    if (server->master != -1)
        return server->master;

    if (locate_server(context, realm, &list, locate_service_master_kdc,
                      server->transport) != 0)
        return FALSE;
    found = server_list_contains(&list, server);
    k5_free_serverlist(&list);
    return found;
}
