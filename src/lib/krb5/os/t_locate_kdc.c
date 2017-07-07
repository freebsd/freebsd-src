/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "k5-platform.h"
#include "port-sockets.h"
#include <sys/types.h>
#include <com_err.h>

#define TEST
#include "fake-addrinfo.h"
#include "dnsglue.c"
#include "dnssrv.c"
#include "locate_kdc.c"

enum {
    LOOKUP_CONF = 3,
    LOOKUP_DNS,
    LOOKUP_WHATEVER
} how = LOOKUP_WHATEVER;

const char *prog;

struct serverlist sl;

static void
kfatal (krb5_error_code err)
{
    com_err (prog, err, "- exiting");
    exit (1);
}

static const char *
ttypename (k5_transport ttype)
{
    static char buf[20];
    switch (ttype) {
    case TCP_OR_UDP:
        return "tcp or udp";
    case TCP:
        return "tcp";
    case UDP:
        return "udp";
    case HTTPS:
        return "https";
    default:
        snprintf(buf, sizeof(buf), "?%d", ttype);
        return buf;
    }
}

static void
print_addrs (void)
{
    size_t i;
    int err;

    printf("%d servers:\n", (int)sl.nservers);
    for (i = 0; i < sl.nservers; i++) {
        struct server_entry *entry = &sl.servers[i];
        char hostbuf[NI_MAXHOST], srvbuf[NI_MAXSERV];

        if (entry->hostname != NULL) {
            printf("%d: h:%s t:%s p:%d m:%d P:%s\n", (int)i,
                   entry->hostname, ttypename(entry->transport),
                   entry->port, entry->master,
                   entry->uri_path ? entry->uri_path : "");
            continue;
        }
        err = getnameinfo((struct sockaddr *)&entry->addr, entry->addrlen,
                          hostbuf, sizeof(hostbuf), srvbuf, sizeof(srvbuf),
                          NI_NUMERICHOST | NI_NUMERICSERV);
        if (err) {
            printf("%2d: getnameinfo returns error %d=%s\n", (int)i, err,
                   gai_strerror(err));
        } else {
            printf("%2d: address %s\t%s\tport %s\n", (int)i, hostbuf,
                   ttypename(entry->transport), srvbuf);
        }
    }
}

int
main (int argc, char *argv[])
{
    char *p, *realmname;
    krb5_data realm;
    krb5_context ctx;
    krb5_error_code err;
    int master = 0;

    p = strrchr (argv[0], '/');
    if (p)
        prog = p+1;
    else
        prog = argv[0];

    switch (argc) {
    case 2:
        /* foo $realm */
        realmname = argv[1];
        break;
    case 3:
        if (!strcmp (argv[1], "-c"))
            how = LOOKUP_CONF;
        else if (!strcmp (argv[1], "-d"))
            how = LOOKUP_DNS;
        else if (!strcmp (argv[1], "-m"))
            master = 1;
        else
            goto usage;
        realmname = argv[2];
        break;
    default:
    usage:
        fprintf (stderr, "%s: usage: %s [-c | -d | -m] realm\n", prog, prog);
        return 1;
    }

    err = krb5_init_context (&ctx);
    if (err)
        kfatal (err);

    realm.data = realmname;
    realm.length = strlen (realmname);

    switch (how) {
    case LOOKUP_CONF:
        err = krb5_locate_srv_conf(ctx, &realm, "kdc", &sl, htons(88));
        break;

    case LOOKUP_DNS:
        err = locate_srv_dns_1(&realm, "_kerberos", "_udp", &sl);
        break;

    case LOOKUP_WHATEVER:
        err = k5_locate_kdc(ctx, &realm, &sl, master, FALSE);
        break;
    }
    if (err) kfatal (err);
    print_addrs();

    k5_free_serverlist(&sl);
    krb5_free_context(ctx);
    return 0;
}
