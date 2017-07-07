/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/resolve/resolve.c */
/*
 * Copyright 1995 by the Massachusetts Institute of Technology.
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
 * A simple program to test the functionality of the resolver library.
 * It simply will try to get the IP address of the host, and then look
 * up the name from the address. If the resulting name does not contain the
 * domain name, then the resolve library is broken.
 *
 * Warning: It is possible to fool this program into thinking everything is
 * alright by a clever use of /etc/hosts - but this is better than nothing.
 *
 * Usage:
 *   resolve [hostname]
 *
 *   When invoked with no arguments, gethostname is used for the local host.
 *
 */

/* This program tests the resolve library and sees if it is broken... */

#include "autoconf.h"
#include <stdio.h>

#if STDC_HEADERS
#include <string.h>
#else
#ifndef HAVE_STRCHR
#define strchr index
#endif
char *strchr();
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <netinet/in.h>
#include <netdb.h>

int
main(argc, argv)
    int argc;
    char **argv;
{
    char myname[MAXHOSTNAMELEN+1];
    char *ptr, *fqdn;
    struct in_addr addrcopy;
    struct hostent *host;
    int quiet = 0;

    argc--; argv++;
    while (argc) {
        if ((strcmp(*argv, "--quiet") == 0) ||
            (strcmp(*argv, "-q") == 0)) {
            quiet++;
        } else
            break;
        argc--; argv++;
    }

    if (argc >= 1) {
        strncpy(myname, *argv, MAXHOSTNAMELEN);
    } else {
        if(gethostname(myname, MAXHOSTNAMELEN)) {
            perror("gethostname failure");
            exit(1);
        }
    }

    myname[MAXHOSTNAMELEN] = '\0';  /* for safety */

    /* Look up the address... */
    if (!quiet)
        printf("Hostname:  %s\n", myname);


    /* Set the hosts db to close each time - effectively rewinding file */
    sethostent(0);

    if((host = gethostbyname (myname)) == NULL) {
        fprintf(stderr,
                "Could not look up address for hostname '%s' - fatal\n",
                myname);
        exit(2);
    }

    fqdn = strdup(host->h_name);
    if (fqdn == NULL) {
        perror("strdup");
        exit(2);
    }

    ptr = host->h_addr_list[0];
#define UC(a) (((int)a)&0xff)
    if (!quiet)
        printf("Host address: %d.%d.%d.%d\n",
               UC(ptr[0]), UC(ptr[1]), UC(ptr[2]), UC(ptr[3]));

    memcpy(&addrcopy.s_addr, ptr, 4);

    /* Convert back to full name */
    if ((host = gethostbyaddr(&addrcopy.s_addr, 4, AF_INET)) == NULL) {
        if (!quiet)
            fprintf(stderr, "Error looking up IP address\n");
    } else {
        free(fqdn);
        fqdn = strdup(host->h_name);
        if (fqdn == NULL) {
            perror("strdup");
            exit (2);
        }
    }

    if (quiet)
        printf("%s\n", fqdn);
    else
        printf("FQDN: %s\n", fqdn);

    if (!quiet)
        printf("Resolve library appears to have passed the test\n");

    /* All ok */
    exit(0);

}
