/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/misc/test_getsockname.c */
/*
 * Copyright (C) 1995 by the Massachusetts Institute of Technology.
 * All rights reserved.
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
 * test_getsockname.c
 *
 * This routine demonstrates a bug in the socket emulation library of
 * Solaris and other monstrosities that uses STREAMS.  On other
 * machines with a real networking layer, it prints the local
 * interface address that is used to send a message to a specific
 * host.  On Solaris, it prints out 0.0.0.0.
 */

#include "autoconf.h"
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>

int
main(argc, argv)
    int argc;
    char *argv[];
{
    int sock;
    GETSOCKNAME_ARG3_TYPE i;
    struct hostent *host;
    struct sockaddr_in s_sock;          /* server address */
    struct sockaddr_in c_sock;          /* client address */

    char *hostname;

    if (argc == 2) {
        hostname = argv[1];
    } else {
        fprintf(stderr, "Usage: %s hostname\n", argv[0]);
        exit(1);
    }

    /* Look up server host */
    if ((host = gethostbyname(hostname)) == (struct hostent *) 0) {
        fprintf(stderr, "%s: unknown host\n", hostname);
        exit(1);
    }

    /* Set server's address */
    (void) memset(&s_sock, 0, sizeof(s_sock));

    memcpy(&s_sock.sin_addr, host->h_addr, sizeof(s_sock.sin_addr));
#ifdef DEBUG
    printf("s_sock.sin_addr is %s\n", inet_ntoa(s_sock.sin_addr));
#endif
    s_sock.sin_family = AF_INET;
    s_sock.sin_port = htons(5555);

    /* Open a socket */
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    memset(&c_sock, 0, sizeof(c_sock));
    c_sock.sin_family = AF_INET;

    /* Bind it to set the address; kernel will fill in port # */
    if (bind(sock, (struct sockaddr *)&c_sock, sizeof(c_sock)) < 0) {
        perror("bind");
        exit(1);
    }

    /* "connect" the datagram socket; this is necessary to get a local address
       properly bound for getsockname() below. */
    if (connect(sock, (struct sockaddr *)&s_sock, sizeof(s_sock)) == -1) {
        perror("connect");
        exit(1);
    }

    /* Get my address */
    memset(&c_sock, 0, sizeof(c_sock));
    i = sizeof(c_sock);
    if (getsockname(sock, (struct sockaddr *)&c_sock, &i) < 0) {
        perror("getsockname");
        exit(1);
    }

    printf("My interface address is: %s\n", inet_ntoa(c_sock.sin_addr));

    exit(0);
}
