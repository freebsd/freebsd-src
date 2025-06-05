/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* appl/sample/sclient/sclient.c */
/*
 * Copyright 1990,1991 by the Massachusetts Institute of Technology.
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
 *
 * Sample Kerberos v5 client.
 *
 * Usage: sample_client hostname
 */

#include "krb5.h"
#include "com_err.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "fake-addrinfo.h" /* not everyone implements getaddrinfo yet */

#include <signal.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <stdlib.h>

#include "../sample.h"

#ifndef GETSOCKNAME_ARG3_TYPE
#define GETSOCKNAME_ARG3_TYPE int
#endif

static int
net_read(int fd, char *buf, int len)
{
    int cc, len2 = 0;

    do {
        cc = SOCKET_READ((SOCKET)fd, buf, len);
        if (cc < 0) {
            if (SOCKET_ERRNO == SOCKET_EINTR)
                continue;

            /* XXX this interface sucks! */
            errno = SOCKET_ERRNO;

            return(cc);          /* errno is already set */
        }
        else if (cc == 0) {
            return(len2);
        } else {
            buf += cc;
            len2 += cc;
            len -= cc;
        }
    } while (len > 0);
    return(len2);
}

int
main(int argc, char *argv[])
{
    struct addrinfo *ap, aihints, *apstart;
    int aierr;
    int sock;
    krb5_context context;
    krb5_data recv_data;
    krb5_data cksum_data;
    krb5_error_code retval;
    krb5_ccache ccdef;
    krb5_principal client, server;
    krb5_error *err_ret;
    krb5_ap_rep_enc_part *rep_ret;
    krb5_auth_context auth_context = 0;
    short xmitlen;
    char *portstr;
    char *service = SAMPLE_SERVICE;

    if (argc != 2 && argc != 3 && argc != 4) {
        fprintf(stderr, "usage: %s <hostname> [port] [service]\n",argv[0]);
        exit(1);
    }

    retval = krb5_init_context(&context);
    if (retval) {
        com_err(argv[0], retval, "while initializing krb5");
        exit(1);
    }

    (void) signal(SIGPIPE, SIG_IGN);

    if (argc > 2)
        portstr = argv[2];
    else
        portstr = SAMPLE_PORT;

    memset(&aihints, 0, sizeof(aihints));
    aihints.ai_socktype = SOCK_STREAM;
    aihints.ai_flags = AI_ADDRCONFIG;
    aierr = getaddrinfo(argv[1], portstr, &aihints, &ap);
    if (aierr) {
        fprintf(stderr, "%s: error looking up host '%s' port '%s'/tcp: %s\n",
                argv[0], argv[1], portstr, gai_strerror(aierr));
        exit(1);
    }
    if (ap == 0) {
        /* Should never happen.  */
        fprintf(stderr, "%s: error looking up host '%s' port '%s'/tcp: no addresses returned?\n",
                argv[0], argv[1], portstr);
        exit(1);
    }

    if (argc > 3) {
        service = argv[3];
    }

    retval = krb5_sname_to_principal(context, argv[1], service,
                                     KRB5_NT_SRV_HST, &server);
    if (retval) {
        com_err(argv[0], retval, "while creating server name for host %s service %s",
                argv[1], service);
        exit(1);
    }

    /* set up the address of the foreign socket for connect() */
    apstart = ap; /* For freeing later */
    for (sock = -1; ap && sock == -1; ap = ap->ai_next) {
        char abuf[NI_MAXHOST], pbuf[NI_MAXSERV];
        char mbuf[NI_MAXHOST + NI_MAXSERV + 64];
        if (getnameinfo(ap->ai_addr, ap->ai_addrlen, abuf, sizeof(abuf),
                        pbuf, sizeof(pbuf), NI_NUMERICHOST | NI_NUMERICSERV)) {
            memset(abuf, 0, sizeof(abuf));
            memset(pbuf, 0, sizeof(pbuf));
            strncpy(abuf, "[error, cannot print address?]",
                    sizeof(abuf)-1);
            strncpy(pbuf, "[?]", sizeof(pbuf)-1);
        }
        memset(mbuf, 0, sizeof(mbuf));
        strncpy(mbuf, "error contacting ", sizeof(mbuf)-1);
        strncat(mbuf, abuf, sizeof(mbuf) - strlen(mbuf) - 1);
        strncat(mbuf, " port ", sizeof(mbuf) - strlen(mbuf) - 1);
        strncat(mbuf, pbuf, sizeof(mbuf) - strlen(mbuf) - 1);
        sock = socket(ap->ai_family, SOCK_STREAM, 0);
        if (sock < 0) {
            fprintf(stderr, "%s: socket: %s\n", mbuf, strerror(errno));
            continue;
        }
        if (connect(sock, ap->ai_addr, ap->ai_addrlen) < 0) {
            fprintf(stderr, "%s: connect: %s\n", mbuf, strerror(errno));
            close(sock);
            sock = -1;
            continue;
        }
        /* connected, yay! */
    }
    if (sock == -1)
        /* Already printed error message above.  */
        exit(1);
    printf("connected\n");

    cksum_data.data = argv[1];
    cksum_data.length = strlen(argv[1]);

    retval = krb5_cc_default(context, &ccdef);
    if (retval) {
        com_err(argv[0], retval, "while getting default ccache");
        exit(1);
    }

    retval = krb5_cc_get_principal(context, ccdef, &client);
    if (retval) {
        com_err(argv[0], retval, "while getting client principal name");
        exit(1);
    }
    retval = krb5_sendauth(context, &auth_context, (krb5_pointer) &sock,
                           SAMPLE_VERSION, client, server,
                           AP_OPTS_MUTUAL_REQUIRED,
                           &cksum_data,
                           0,           /* no creds, use ccache instead */
                           ccdef, &err_ret, &rep_ret, NULL);

    krb5_free_principal(context, server);       /* finished using it */
    krb5_free_principal(context, client);
    krb5_cc_close(context, ccdef);
    if (auth_context) krb5_auth_con_free(context, auth_context);

    if (retval && retval != KRB5_SENDAUTH_REJECTED) {
        com_err(argv[0], retval, "while using sendauth");
        exit(1);
    }
    if (retval == KRB5_SENDAUTH_REJECTED) {
        /* got an error */
        printf("sendauth rejected, error reply is:\n\t\"%*s\"\n",
               err_ret->text.length, err_ret->text.data);
    } else if (rep_ret) {
        /* got a reply */
        krb5_free_ap_rep_enc_part(context, rep_ret);

        printf("sendauth succeeded, reply is:\n");
        if ((retval = net_read(sock, (char *)&xmitlen,
                               sizeof(xmitlen))) <= 0) {
            if (retval == 0)
                errno = ECONNABORTED;
            com_err(argv[0], errno, "while reading data from server");
            exit(1);
        }
        recv_data.length = ntohs(xmitlen);
        if (!(recv_data.data = (char *)malloc((size_t) recv_data.length + 1))) {
            com_err(argv[0], ENOMEM,
                    "while allocating buffer to read from server");
            exit(1);
        }
        if ((retval = net_read(sock, (char *)recv_data.data,
                               recv_data.length)) <= 0) {
            if (retval == 0)
                errno = ECONNABORTED;
            com_err(argv[0], errno, "while reading data from server");
            exit(1);
        }
        recv_data.data[recv_data.length] = '\0';
        printf("reply len %d, contents:\n%s\n",
               recv_data.length,recv_data.data);
        free(recv_data.data);
    } else {
        com_err(argv[0], 0, "no error or reply from sendauth!");
        exit(1);
    }
    freeaddrinfo(apstart);
    krb5_free_context(context);
    exit(0);
}
