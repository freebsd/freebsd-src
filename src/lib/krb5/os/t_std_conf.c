/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * t_std_conf.c --- This program tests standard Krb5 routines which pull
 *      values from the krb5 config file(s).
 */

#include "k5-int.h"
#include "fake-addrinfo.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "os-proto.h"

static void
test_get_default_realm(krb5_context ctx)
{
    char    *realm;
    krb5_error_code retval;

    retval = krb5_get_default_realm(ctx, &realm);
    if (retval) {
        com_err("krb5_get_default_realm", retval, 0);
        return;
    }
    printf("krb5_get_default_realm() returned '%s'\n", realm);
    free(realm);
}

static void
test_set_default_realm(krb5_context ctx, char *realm)
{
    krb5_error_code retval;

    retval = krb5_set_default_realm(ctx, realm);
    if (retval) {
        com_err("krb5_set_default_realm", retval, 0);
        return;
    }
    printf("krb5_set_default_realm(%s)\n", realm);
}

static void
test_get_default_ccname(krb5_context ctx)
{
    const char      *ccname;

    ccname = krb5_cc_default_name(ctx);
    if (ccname)
        printf("krb5_cc_default_name() returned '%s'\n", ccname);
    else
        printf("krb5_cc_default_name() returned NULL\n");
}

static void
test_set_default_ccname(krb5_context ctx, char *ccname)
{
    krb5_error_code retval;

    retval = krb5_cc_set_default_name(ctx, ccname);
    if (retval) {
        com_err("krb5_set_default_ccname", retval, 0);
        return;
    }
    printf("krb5_set_default_ccname(%s)\n", ccname);
}

static void
test_locate_kdc(krb5_context ctx, char *realm)
{
    struct serverlist servers;
    size_t i;
    int get_masters = FALSE;
    krb5_data rlm;
    krb5_error_code retval;

    rlm.data = realm;
    rlm.length = strlen(realm);
    retval = k5_locate_kdc(ctx, &rlm, &servers, get_masters, FALSE);
    if (retval) {
        com_err("krb5_locate_kdc", retval, 0);
        return;
    }
    printf("krb_locate_kdc(%s) returned:", realm);
    for (i = 0; i < servers.nservers; i++) {
        struct server_entry *entry = &servers.servers[i];
        if (entry->hostname) {
            printf(" host:%s/%d", entry->hostname, entry->port);
            continue;
        }
        switch (entry->family) {
        case AF_INET:
        {
            struct sockaddr_in *s_sin = (struct sockaddr_in *)&entry->addr;
            printf(" inet:%s/%d", inet_ntoa(s_sin->sin_addr),
                   ntohs(s_sin->sin_port));
        }
        break;
        case AF_INET6:
        {
            struct sockaddr_in6 *s_sin6 = (struct sockaddr_in6 *)&entry->addr;
            int j;
            printf(" inet6");
            for (j = 0; j < 8; j++)
                printf(":%x",
                       (s_sin6->sin6_addr.s6_addr[2*j] * 256
                        + s_sin6->sin6_addr.s6_addr[2*j+1]));
            printf("/%d", ntohs(s_sin6->sin6_port));
            break;
        }
        default:
            printf(" unknown-af-%d", entry->family);
            break;
        }
    }
    k5_free_serverlist(&servers);
    printf("\n");
}

static void
test_get_host_realm(krb5_context ctx, char *host)
{
    char **realms, **cpp;
    krb5_error_code retval;

    retval = krb5_get_host_realm(ctx, host, &realms);
    if (retval) {
        com_err("krb5_get_host_realm", retval, 0);
        return;
    }
    printf("krb_get_host_realm(%s) returned:", host);
    if (realms == 0) {
        printf(" (null)\n");
        return;
    }
    if (realms[0] == 0) {
        printf(" (none)\n");
        free(realms);
        return;
    }
    for (cpp = realms; *cpp; cpp++) {
        printf(" '%s'", *cpp);
        free(*cpp);
    }
    free(realms);
    printf("\n");
}

static void
test_get_realm_domain(krb5_context ctx, char *realm)
{
    krb5_error_code retval;
    char    *domain;

    retval = krb5_get_realm_domain(ctx, realm, &domain);
    if (retval) {
        com_err("krb5_get_realm_domain", retval, 0);
        return;
    }
    printf("krb5_get_realm_domain(%s) returned '%s'\n", realm, domain);
    free(domain);
}

static void
usage(char *progname)
{
    fprintf(stderr, "%s: Usage: %s [-dc] [-k realm] [-r host] [-C ccname] [-D realm]\n",
            progname, progname);
    exit(1);
}

int
main(int argc, char **argv)
{
    int     c;
    krb5_context    ctx;
    krb5_error_code retval;
    extern char *optarg;

    retval = krb5_init_context(&ctx);
    if (retval) {
        fprintf(stderr, "krb5_init_context returned error %u\n",
                retval);
        exit(1);
    }

    while ((c = getopt(argc, argv, "cdr:C:D:l:s:")) != -1) {
        switch (c) {
        case 'c':           /* Get default ccname */
            test_get_default_ccname(ctx);
            break;
        case 'd': /* Get default realm */
            test_get_default_realm(ctx);
            break;
        case 'l':
            test_locate_kdc(ctx, optarg);
            break;
        case 'r':
            test_get_host_realm(ctx, optarg);
            break;
        case 's':
            test_set_default_realm(ctx, optarg);
            break;
        case 'C':
            test_set_default_ccname(ctx, optarg);
            break;
        case 'D':
            test_get_realm_domain(ctx, optarg);
            break;
        default:
            usage(argv[0]);
        }
    }


    krb5_free_context(ctx);
    exit(0);
}
