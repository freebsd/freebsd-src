/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* appl/user_user/client.c - Other end of user-user client/server pair */
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

#include "k5-int.h"
#include "com_err.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

int main (int argc, char *argv[])
{
    int s;
    register int retval, i;
    char *hname;          /* full name of server */
    char **srealms;       /* realm(s) of server */
    char *princ;          /* principal in credentials cache */
    struct servent *serv;
    struct hostent *host;
    struct sockaddr_in serv_net_addr, cli_net_addr;
    krb5_ccache cc;
    krb5_creds creds, *new_creds;
    krb5_data reply, msg, princ_data;
    krb5_auth_context auth_context = NULL;
    krb5_ticket * ticket = NULL;
    krb5_context context;
    unsigned short port;

    if (argc < 2 || argc > 4) {
        fputs ("usage: uu-client <hostname> [message [port]]\n", stderr);
        return 1;
    }

    retval = krb5_init_context(&context);
    if (retval) {
        com_err(argv[0], retval, "while initializing krb5");
        exit(1);
    }

    if (argc == 4) {
        port = htons(atoi(argv[3]));
    }
    else if ((serv = getservbyname ("uu-sample", "tcp")) == NULL)
    {
        fputs ("uu-client: unknown service \"uu-sample/tcp\"\n", stderr);
        return 2;
    } else {
        port = serv->s_port;
    }

    if ((host = gethostbyname (argv[1])) == NULL) {
        fprintf (stderr, "uu-client: can't get address of host \"%s\".\n",
                 argv[1]);
        return 3;
    }

    if (host->h_addrtype != AF_INET) {
        fprintf (stderr, "uu-client: bad address type %d for \"%s\".\n",
                 host->h_addrtype, argv[1]);
        return 3;
    }

    hname = strdup (host->h_name);

#ifndef USE_STDOUT
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        com_err ("uu-client", errno, "creating socket");
        return 4;
    } else {
        cli_net_addr.sin_family = AF_INET;
        cli_net_addr.sin_port = 0;
        cli_net_addr.sin_addr.s_addr = 0;
        if (bind (s, (struct sockaddr *)&cli_net_addr,
                  sizeof (cli_net_addr)) < 0) {
            com_err ("uu-client", errno, "binding socket");
            return 4;
        }
    }

    serv_net_addr.sin_family = AF_INET;
    serv_net_addr.sin_port = port;

    i = 0;
    while (1) {
        if (host->h_addr_list[i] == 0) {
            fprintf (stderr, "uu-client: unable to connect to \"%s\"\n", hname);
            return 5;
        }

        memcpy (&serv_net_addr.sin_addr, host->h_addr_list[i++],
                sizeof(serv_net_addr.sin_addr));

        if (connect(s, (struct sockaddr *)&serv_net_addr,
                    sizeof (serv_net_addr)) == 0)
            break;
        com_err ("uu-client", errno, "connecting to \"%s\" (%s).",
                 hname, inet_ntoa(serv_net_addr.sin_addr));
    }
#else
    s = 1;
#endif

    retval = krb5_cc_default(context, &cc);
    if (retval) {
        com_err("uu-client", retval, "getting credentials cache");
        return 6;
    }

    memset (&creds, 0, sizeof(creds));

    retval = krb5_cc_get_principal(context, cc, &creds.client);
    if (retval) {
        com_err("uu-client", retval, "getting principal name");
        return 6;
    }

    retval = krb5_unparse_name(context, creds.client, &princ);
    if (retval) {
        com_err("uu-client", retval, "printing principal name");
        return 7;
    }
    else
        fprintf(stderr, "uu-client: client principal is \"%s\".\n", princ);

    retval = krb5_get_host_realm(context, hname, &srealms);
    if (retval) {
        com_err("uu-client", retval, "getting realms for \"%s\"", hname);
        return 7;
    }

    retval =
        krb5_build_principal_ext(context, &creds.server,
                                 krb5_princ_realm(context,
                                                  creds.client)->length,
                                 krb5_princ_realm(context,
                                                  creds.client)->data,
                                 6, "krbtgt",
                                 krb5_princ_realm(context,
                                                  creds.client)->length,
                                 krb5_princ_realm(context,
                                                  creds.client)->data,
                                 0);
    if (retval) {
        com_err("uu-client", retval, "setting up tgt server name");
        return 7;
    }

    /* Get TGT from credentials cache */
    retval = krb5_get_credentials(context, KRB5_GC_CACHED, cc,
                                  &creds, &new_creds);
    if (retval) {
        com_err("uu-client", retval, "getting TGT");
        return 6;
    }

    i = strlen(princ) + 1;

    fprintf(stderr, "uu-client: sending %d bytes\n",
            new_creds->ticket.length + i);
    princ_data.data = princ;
    princ_data.length = i;                /* include null terminator for
                                             server's convenience */
    retval = krb5_write_message(context, (krb5_pointer) &s, &princ_data);
    if (retval) {
        com_err("uu-client", retval, "sending principal name to server");
        return 8;
    }

    free(princ);

    retval = krb5_write_message(context, (krb5_pointer) &s,
                                &new_creds->ticket);
    if (retval) {
        com_err("uu-client", retval, "sending ticket to server");
        return 8;
    }

    retval = krb5_read_message(context, (krb5_pointer) &s, &reply);
    if (retval) {
        com_err("uu-client", retval, "reading reply from server");
        return 9;
    }

    retval = krb5_auth_con_init(context, &auth_context);
    if (retval) {
        com_err("uu-client", retval, "initializing the auth_context");
        return 9;
    }

    retval =
        krb5_auth_con_genaddrs(context, auth_context, s,
                               KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR |
                               KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR);
    if (retval) {
        com_err("uu-client", retval, "generating addrs for auth_context");
        return 9;
    }

    retval = krb5_auth_con_setflags(context, auth_context,
                                    KRB5_AUTH_CONTEXT_DO_SEQUENCE);
    if (retval) {
        com_err("uu-client", retval, "initializing the auth_context flags");
        return 9;
    }

    retval = krb5_auth_con_setuseruserkey(context, auth_context,
                                          &new_creds->keyblock);
    if (retval) {
        com_err("uu-client", retval, "setting useruserkey for authcontext");
        return 9;
    }

#if 1
    /* read the ap_req to get the session key */
    retval = krb5_rd_req(context, &auth_context, &reply, creds.client, NULL,
                         NULL, &ticket);
    free(reply.data);
#else
    retval = krb5_recvauth(context, &auth_context, (krb5_pointer)&s, "???",
                           0, /* server */, 0, NULL, &ticket);
#endif

    if (retval) {
        com_err("uu-client", retval, "reading AP_REQ from server");
        return 9;
    }

    retval = krb5_unparse_name(context, ticket->enc_part2->client, &princ);
    if (retval)
        com_err("uu-client", retval, "while unparsing client name");
    else {
        printf("server is named \"%s\"\n", princ);
        free(princ);
    }

    retval = krb5_read_message(context, (krb5_pointer) &s, &reply);
    if (retval) {
        com_err("uu-client", retval, "reading reply from server");
        return 9;
    }

    retval = krb5_rd_safe(context, auth_context, &reply, &msg, NULL);
    if (retval) {
        com_err("uu-client", retval, "decoding reply from server");
        return 10;
    }

    printf ("uu-client: server says \"%s\".\n", msg.data);


    krb5_free_ticket(context, ticket);
    krb5_free_host_realm(context, srealms);
    free(hname);
    krb5_free_cred_contents(context, &creds);
    krb5_free_creds(context, new_creds);
    krb5_free_data_contents(context, &msg);
    krb5_free_data_contents(context, &reply);
    krb5_cc_close(context, cc);
    krb5_auth_con_free(context, auth_context);
    krb5_free_context(context);
    return 0;
}
