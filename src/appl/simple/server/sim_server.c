/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* appl/simple/server/sim_server.c */
/*
 * Copyright 1989,1991 by the Massachusetts Institute of Technology.
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
 * Usage:
 * sample_server servername
 *
 * Simple UDP-based server application.  For demonstration.
 * This program performs no useful function.
 */

#include "krb5.h"
#include "port-sockets.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "com_err.h"

#include "simple.h"

/* for old Unixes and friends ... */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#define PROGNAME argv[0]

static void
usage(char *name)
{
    fprintf(stderr, "usage: %s [-p port] [-s service] [-S keytab]\n", name);
}

int
main(int argc, char *argv[])
{
    int sock, i;
    socklen_t len;
    int flags = 0;                      /* for recvfrom() */
    int on = 1;
    struct servent *serv;
    struct sockaddr_in s_sock;          /* server's address */
    struct sockaddr_in c_sock;          /* client's address */
    char *cp;
    extern char * optarg;
    int ch;

    short port = 0;             /* If user specifies port */
    krb5_keytab keytab = NULL;  /* Allow specification on command line */
    char *service = SIMPLE_SERVICE;

    krb5_error_code retval;
    krb5_data packet, message;
    unsigned char pktbuf[BUFSIZ];
    krb5_principal sprinc;
    krb5_context context;
    krb5_auth_context auth_context = NULL;
    krb5_address addr;
    krb5_ticket *ticket = NULL;

    retval = krb5_init_context(&context);
    if (retval) {
        com_err(argv[0], retval, "while initializing krb5");
        exit(1);
    }

    /*
     * Parse command line arguments
     *
     */
    while ((ch = getopt(argc, argv, "p:s:S:")) != -1) {
        switch (ch) {
        case 'p':
            port = atoi(optarg);
            break;
        case 's':
            service = optarg;
            break;
        case 'S':
            if ((retval = krb5_kt_resolve(context, optarg, &keytab))) {
                com_err(PROGNAME, retval,
                        "while resolving keytab file %s", optarg);
                exit(2);
            }
            break;

        case '?':
        default:
            usage(PROGNAME);
            exit(1);
            break;
        }
    }

    if ((retval = krb5_sname_to_principal(context, NULL, service,
                                          KRB5_NT_SRV_HST, &sprinc))) {
        com_err(PROGNAME, retval, "while generating service name %s", service);
        exit(1);
    }

    /* Set up server address */
    memset(&s_sock, 0, sizeof(s_sock));
    s_sock.sin_family = AF_INET;
    s_sock.sin_addr.s_addr = INADDR_ANY;

    if (port == 0) {
        /* Look up service */
        if ((serv = getservbyname(SIMPLE_PORT, "udp")) == NULL) {
            fprintf(stderr, "service unknown: %s/udp\n", SIMPLE_PORT);
            exit(1);
        }
        s_sock.sin_port = serv->s_port;
    } else {
        s_sock.sin_port = htons(port);
    }

    /* Open socket */
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("opening datagram socket");
        exit(1);
    }

    /* Let the socket be reused right away */
    (void) setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
                      sizeof(on));

    /* Bind the socket */
    if (bind(sock, (struct sockaddr *)&s_sock, sizeof(s_sock))) {
        perror("binding datagram socket");
        exit(1);
    }

    printf("starting...\n");
    fflush(stdout);

#ifdef DEBUG
    printf("socket has port # %d\n", ntohs(s_sock.sin_port));
#endif

    /* GET KRB_AP_REQ MESSAGE */

    /* use "recvfrom" so we know client's address */
    len = sizeof(struct sockaddr_in);
    if ((i = recvfrom(sock, (char *)pktbuf, sizeof(pktbuf), flags,
                      (struct sockaddr *)&c_sock, &len)) < 0) {
        perror("receiving datagram");
        exit(1);
    }

    printf("Received %d bytes\n", i);
    packet.length = i;
    packet.data = (krb5_pointer) pktbuf;

    /* Check authentication info */
    if ((retval = krb5_rd_req(context, &auth_context, &packet,
                              sprinc, keytab, NULL, &ticket))) {
        com_err(PROGNAME, retval, "while reading request");
        exit(1);
    }
    if ((retval = krb5_unparse_name(context, ticket->enc_part2->client,
                                    &cp))) {
        com_err(PROGNAME, retval, "while unparsing client name");
        exit(1);
    }
    printf("Got authentication info from %s\n", cp);
    free(cp);

    /* Set foreign_addr for rd_safe() and rd_priv() */
    addr.addrtype = ADDRTYPE_INET;
    addr.length = sizeof(c_sock.sin_addr);
    addr.contents = (krb5_octet *)&c_sock.sin_addr;
    if ((retval = krb5_auth_con_setaddrs(context, auth_context,
                                         NULL, &addr))) {
        com_err(PROGNAME, retval, "while setting foreign addr");
        exit(1);
    }

    addr.addrtype = ADDRTYPE_IPPORT;
    addr.length = sizeof(c_sock.sin_port);
    addr.contents = (krb5_octet *)&c_sock.sin_port;
    if ((retval = krb5_auth_con_setports(context, auth_context,
                                         NULL, &addr))) {
        com_err(PROGNAME, retval, "while setting foreign port");
        exit(1);
    }

    /* GET KRB_MK_SAFE MESSAGE */

    /* use "recvfrom" so we know client's address */
    len = sizeof(struct sockaddr_in);
    if ((i = recvfrom(sock, (char *)pktbuf, sizeof(pktbuf), flags,
                      (struct sockaddr *)&c_sock, &len)) < 0) {
        perror("receiving datagram");
        exit(1);
    }
#ifdef DEBUG
    printf("&c_sock.sin_addr is %s\n", inet_ntoa(c_sock.sin_addr));
#endif
    printf("Received %d bytes\n", i);

    packet.length = i;
    packet.data = (krb5_pointer) pktbuf;

    if ((retval = krb5_rd_safe(context, auth_context, &packet,
                               &message, NULL))) {
        com_err(PROGNAME, retval, "while verifying SAFE message");
        exit(1);
    }
    printf("Safe message is: '%.*s'\n", (int) message.length, message.data);

    krb5_free_data_contents(context, &message);

    /* NOW GET ENCRYPTED MESSAGE */

    /* use "recvfrom" so we know client's address */
    len = sizeof(struct sockaddr_in);
    if ((i = recvfrom(sock, (char *)pktbuf, sizeof(pktbuf), flags,
                      (struct sockaddr *)&c_sock, &len)) < 0) {
        perror("receiving datagram");
        exit(1);
    }
    printf("Received %d bytes\n", i);

    packet.length = i;
    packet.data = (krb5_pointer) pktbuf;

    if ((retval = krb5_rd_priv(context, auth_context, &packet,
                               &message, NULL))) {
        com_err(PROGNAME, retval, "while verifying PRIV message");
        exit(1);
    }
    printf("Decrypted message is: '%.*s'\n", (int) message.length,
           message.data);

    krb5_auth_con_free(context, auth_context);
    krb5_free_context(context);

    exit(0);
}
