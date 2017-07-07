/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/dejagnu/t_inetd.c */
/*
 * Copyright 1991 by the Massachusetts Institute of Technology.
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
 * A simple program to simulate starting a process from inetd.
 *
 * Unlike a proper inetd situation, environment variables are passed
 * to the client.
 *
 * usage: t_inetd port program argv0 ...
 */

#include "autoconf.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "com_err.h"


char *progname;

static void usage()
{
    fprintf(stderr, "%s: port program argv0 argv1 ...\n", progname);
    exit(1);
}

int
main(argc, argv)
    int argc;
    char **argv;
{
    unsigned short port;
    char *path;
    int sock, acc;
    int one = 1;
    struct sockaddr_in l_inaddr, f_inaddr;  /* local, foreign address */
    socklen_t namelen = sizeof(f_inaddr);
#ifdef POSIX_SIGNALS
    struct sigaction csig;
#endif

    progname = argv[0];

    if(argc <= 3) usage();

    if(atoi(argv[1]) == 0) usage();

    port = htons(atoi(argv[1]));
    path = argv[2];

    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        com_err(progname, errno, "creating socket");
        exit(3);
    }

    (void) setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&one,
                      sizeof (one));

    memset(&l_inaddr, 0, sizeof(l_inaddr));
    l_inaddr.sin_family = AF_INET;
    l_inaddr.sin_addr.s_addr = 0;
    l_inaddr.sin_port = port;

    if (bind(sock, (struct sockaddr *)&l_inaddr, sizeof(l_inaddr))) {
        com_err(progname, errno, "binding socket");
        exit(3);
    }

    if (listen(sock, 1) == -1) {
        com_err(progname, errno, "listening");
        exit(3);
    }

    printf("Ready!\n");
    fflush(stdout);
    if ((acc = accept(sock, (struct sockaddr *)&f_inaddr,
                      &namelen)) == -1) {
        com_err(progname, errno, "accepting");
        exit(3);
    }

    dup2(acc, 0);
    dup2(acc, 1);
    dup2(acc, 2);
    close(sock);
    sock = 0;

    /* Don't wait for a child signal... Otherwise dejagnu gets confused */
#ifdef POSIX_SIGNALS
    csig.sa_handler = (RETSIGTYPE (*)())0;
    sigemptyset(&csig.sa_mask);
    csig.sa_flags = 0;
    sigaction(SIGCHLD, &csig, (struct sigaction *)0);
#else
    signal(SIGCHLD, SIG_IGN);
#endif

    if(execv(path, &argv[3]))
        fprintf(stderr, "t_inetd: Could not exec %s\n", path);
    exit(1);
}
