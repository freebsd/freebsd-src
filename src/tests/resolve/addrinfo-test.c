/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/resolve/addrinfo-test.c */
/*
 * Copyright 2004 by the Massachusetts Institute of Technology.
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

/*
 * A simple program to test the functionality of the getaddrinfo function.
 *
 * Usage:
 *   addrinfo-test [-t|-u|-R|-I] [-d|-s|-r] [-p port] [-P] [hostname]
 *
 *   When invoked with no arguments, NULL is used for the node name,
 *   which (at least with a non-null "port") means a socket address
 *   is desired that can be used with connect() or bind() (depending
 *   on whether "-P" is given).
 */

#include <k5-platform.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h> /* needed for IPPROTO_* on NetBSD */
#ifdef USE_FAKE_ADDRINFO
#include "fake-addrinfo.h"
#endif

static const char *protoname (int p) {
    static char buf[30];

#define X(N) if (p == IPPROTO_ ## N) return #N

    X(TCP);
    X(UDP);
    X(ICMP);
#ifdef IPPROTO_IPV6
    X(IPV6);
#endif
#ifdef IPPROTO_GRE
    X(GRE);
#endif
#ifdef IPPROTO_NONE
    X(NONE);
#endif
    X(RAW);
#ifdef IPPROTO_COMP
    X(COMP);
#endif

    snprintf(buf, sizeof(buf), " %-2d", p);
    return buf;
}

static const char *socktypename (int t) {
    static char buf[30];
    switch (t) {
    case SOCK_DGRAM: return "DGRAM";
    case SOCK_STREAM: return "STREAM";
    case SOCK_RAW: return "RAW";
    case SOCK_RDM: return "RDM";
    case SOCK_SEQPACKET: return "SEQPACKET";
    }
    snprintf(buf, sizeof(buf), " %-2d", t);
    return buf;
}

static char *whoami;

static void usage () {
    fprintf(stderr,
            "usage:\n"
            "\t%s [ options ] [host]\n"
            "options:\n"
            "\t-t\tspecify protocol IPPROTO_TCP\n"
            "\t-u\tspecify protocol IPPROTO_UDP\n"
            "\t-R\tspecify protocol IPPROTO_RAW\n"
            "\t-I\tspecify protocol IPPROTO_ICMP\n"
            "\n"
            "\t-d\tspecify socket type SOCK_DGRAM\n"
            "\t-s\tspecify socket type SOCK_STREAM\n"
            "\t-r\tspecify socket type SOCK_RAW\n"
            "\n"
            "\t-4\tspecify address family AF_INET\n"
#ifdef AF_INET6
            "\t-6\tspecify address family AF_INET6\n"
#endif
            "\n"
            "\t-p P\tspecify port P (service name or port number)\n"
            "\t-N\thostname is numeric, skip DNS query\n"
            "\t-n\tservice/port is numeric (sets AI_NUMERICSERV)\n"
            "\t-P\tset AI_PASSIVE\n"
            "\n"
            "default: protocol 0, socket type 0, address family 0, null port\n"
            ,
            whoami);
    /* [ -t | -u | -R | -I ] [ -d | -s | -r ] [ -p port ] */
    exit (1);
}

static const char *familyname (int f) {
    static char buf[30];
    switch (f) {
    default:
        snprintf(buf, sizeof(buf), "AF %d", f);
        return buf;
    case AF_INET: return "AF_INET";
#ifdef AF_INET6
    case AF_INET6: return "AF_INET6";
#endif
    }
}

#define eaistr(X) (X == EAI_SYSTEM ? strerror(errno) : gai_strerror(X))

int main (int argc, char *argv[])
{
    struct addrinfo *ap, *ap2;
    int err, numerichost = 0, numericserv = 0;
    char *hname, *port = 0, *sep;
    struct addrinfo hints;

    whoami = strrchr(argv[0], '/');
    if (whoami == 0)
        whoami = argv[0];
    else
        whoami = whoami+1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = 0;
    hints.ai_socktype = 0;

    hname = 0;
    hints.ai_family = 0;

    if (argc == 1)
        usage ();

    while (++argv, --argc > 0) {
        char *arg;
        arg = *argv;

        if (*arg != '-')
            hname = arg;
        else if (arg[1] == 0 || arg[2] != 0)
            usage ();
        else
            switch (arg[1]) {
            case 'u':
                hints.ai_protocol = IPPROTO_UDP;
                break;
            case 't':
                hints.ai_protocol = IPPROTO_TCP;
                break;
            case 'R':
                hints.ai_protocol = IPPROTO_RAW;
                break;
            case 'I':
                hints.ai_protocol = IPPROTO_ICMP;
                break;
            case 'd':
                hints.ai_socktype = SOCK_DGRAM;
                break;
            case 's':
                hints.ai_socktype = SOCK_STREAM;
                break;
            case 'r':
                hints.ai_socktype = SOCK_RAW;
                break;
            case 'p':
                if (argv[1] == 0 || argv[1][0] == 0 || argv[1][0] == '-')
                    usage ();
                port = argv[1];
                argc--, argv++;
                break;
            case '4':
                hints.ai_family = AF_INET;
                break;
#ifdef AF_INET6
            case '6':
                hints.ai_family = AF_INET6;
                break;
#endif
            case 'N':
                numerichost = 1;
                break;
            case 'n':
                numericserv = 1;
                break;
            case 'P':
                hints.ai_flags |= AI_PASSIVE;
                break;
            default:
                usage ();
            }
    }

    if (hname && !numerichost)
        hints.ai_flags |= AI_CANONNAME;
    if (numerichost) {
#ifdef AI_NUMERICHOST
        hints.ai_flags |= AI_NUMERICHOST;
#else
        fprintf(stderr, "AI_NUMERICHOST not defined on this platform\n");
        exit(1);
#endif
    }
    if (numericserv) {
#ifdef AI_NUMERICSERV
        hints.ai_flags |= AI_NUMERICSERV;
#else
        fprintf(stderr, "AI_NUMERICSERV not defined on this platform\n");
        exit(1);
#endif
    }

    printf("getaddrinfo(hostname %s, service %s,\n"
           "            hints { ",
           hname ? hname : "(null)", port ? port : "(null)");
    sep = "";
#define Z(FLAG) if (hints.ai_flags & AI_##FLAG) printf("%s%s", sep, #FLAG), sep = "|"
    Z(CANONNAME);
    Z(PASSIVE);
#ifdef AI_NUMERICHOST
    Z(NUMERICHOST);
#endif
#ifdef AI_NUMERICSERV
    Z(NUMERICSERV);
#endif
    if (sep[0] == 0)
        printf ("no-flags");
    if (hints.ai_family)
        printf(" %s", familyname(hints.ai_family));
    if (hints.ai_socktype)
        printf(" SOCK_%s", socktypename(hints.ai_socktype));
    if (hints.ai_protocol)
        printf(" IPPROTO_%s", protoname(hints.ai_protocol));
    printf(" }):\n");

    err = getaddrinfo(hname, port, &hints, &ap);
    if (err) {
        printf("\terror => %s\n", eaistr(err));
        return 1;
    }

    for (ap2 = ap; ap2; ap2 = ap2->ai_next) {
        char hbuf[NI_MAXHOST], pbuf[NI_MAXSERV];
        /* If we don't do this, even AIX's own getnameinfo will reject
           the sockaddr structures.  The sa_len field doesn't get set
           either, on AIX, but getnameinfo won't complain.  */
        if (ap2->ai_addr->sa_family == 0) {
            printf("BAD: sa_family zero! fixing...\n");
            ap2->ai_addr->sa_family = ap2->ai_family;
        } else if (ap2->ai_addr->sa_family != ap2->ai_family) {
            printf("BAD: sa_family != ai_family! fixing...\n");
            ap2->ai_addr->sa_family = ap2->ai_family;
        }
        if (getnameinfo(ap2->ai_addr, ap2->ai_addrlen, hbuf, sizeof(hbuf),
                        pbuf, sizeof(pbuf), NI_NUMERICHOST | NI_NUMERICSERV)) {
            strlcpy(hbuf, "...", sizeof(hbuf));
            strlcpy(pbuf, "...", sizeof(pbuf));
        }
        printf("%p:\n"
               "\tfamily = %s\tproto = %-4s\tsocktype = %s\n",
               (void *) ap2, familyname(ap2->ai_family),
               protoname (ap2->ai_protocol),
               socktypename (ap2->ai_socktype));
        if (ap2->ai_canonname) {
            if (ap2->ai_canonname[0])
                printf("\tcanonname = %s\n", ap2->ai_canonname);
            else
                printf("BAD: ai_canonname is set but empty!\n");
        } else if (ap2 == ap && (hints.ai_flags & AI_CANONNAME)) {
            printf("BAD: first ai_canonname is null!\n");
        }
        printf("\taddr = %-28s\tport = %s\n", hbuf, pbuf);

        err = getnameinfo(ap2->ai_addr, ap2->ai_addrlen, hbuf, sizeof (hbuf),
                          pbuf, sizeof(pbuf), NI_NAMEREQD);
        if (err)
            printf("\tgetnameinfo(NI_NAMEREQD): %s\n", eaistr(err));
        else
            printf("\tgetnameinfo => %s, %s\n", hbuf, pbuf);
    }
    freeaddrinfo(ap);
    return 0;
}
