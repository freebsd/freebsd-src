/*
 *
 * Copyright 1989 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * Simple UDP-based server application.  For demonstration.
 * This program performs no useful function.
 */

#include "sample.h"

RCSID("$Id: simple_server.c,v 1.11 1999/11/13 06:29:24 assar Exp $");

static void
usage (void)
{
    fprintf (stderr, "Usage: %s [-p port] [-s service] [-t srvtab]\n",
	     __progname);
    exit (1);
}

int
main(int argc, char **argv)
{
    char service[SNAME_SZ];
    char instance[INST_SZ];
    int port;
    char srvtab[MaxPathLen];
    struct sockaddr_in addr, otheraddr;
    int c;
    int sock;
    int i;
    int len;
    KTEXT_ST k;
    KTEXT ktxt = &k;
    AUTH_DAT ad;
    MSG_DAT msg_data;
    des_key_schedule sched;

    set_progname (argv[0]);
    strlcpy (service, SAMPLE_SERVICE, sizeof(service));
    strlcpy (instance, "*", sizeof(instance));
    *srvtab = '\0';
    port = 0;

    while ((c = getopt (argc, argv, "p:s:t:")) != -1)
	switch (c) {
	case 'p' : {
	    struct servent *sp;

	    sp = getservbyname (optarg, "udp");
	    if (sp)
		port = sp->s_port;
	    else
		port = htons(atoi(optarg));
	    break;
	}
	case 's' :
	    strlcpy (service, optarg, sizeof(service));
	    break;
	case 't' :
	    strlcpy (srvtab, optarg, sizeof(srvtab));
	    break;
	case '?' :
	default :
	    usage ();
	}

    if(port == 0)
	port = k_getportbyname (SAMPLE_SERVICE, "udp", htons(SAMPLE_PORT));

    memset (&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = port;

    sock = socket (AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
	err (1, "socket");

    if (bind (sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	err (1, "bind");

    /* GET KRB_MK_REQ MESSAGE */

    i = read(sock, ktxt->dat, MAX_KTXT_LEN);
    if (i < 0)
	err (1, "read");

    printf("Received %d bytes\n", i);
    ktxt->length = i;

    /* Check authentication info */
    i = krb_rd_req(ktxt, service, instance, 0, &ad, "");
    if (i != KSUCCESS)
	errx (1, "krb_rd_req: %s", krb_get_err_text(i));
    printf("Got authentication info from %s%s%s@%s\n", ad.pname,
	   *ad.pinst ? "." : "", ad.pinst, ad.prealm);
	
    /* GET KRB_MK_SAFE MESSAGE */

    /* use "recvfrom" so we know client's address */
    len = sizeof(otheraddr);
    i = recvfrom(sock, ktxt->dat, MAX_KTXT_LEN, 0,
		 (struct sockaddr *)&otheraddr, &len);
    if (i < 0)
	err (1, "recvfrom");
    printf("Received %d bytes\n", i);

    /* Verify the checksummed message */
    i = krb_rd_safe(ktxt->dat, i, &ad.session, &otheraddr,
		    &addr, &msg_data);
    if (i != KSUCCESS)
	errx (1, "krb_rd_safe: %s", krb_get_err_text(i));
    printf("Safe message is: %s\n", msg_data.app_data);
	
    /* NOW GET ENCRYPTED MESSAGE */

#ifdef NOENCRYPTION
    memset(sched, 0, sizeof(sched));
#else
    /* need key schedule for session key */
    des_key_sched(&ad.session, sched);
#endif

    /* use "recvfrom" so we know client's address */
    len = sizeof(otheraddr);
    i = recvfrom(sock, ktxt->dat, MAX_KTXT_LEN, 0,
		 (struct sockaddr *)&otheraddr, &len);
    if (i < 0)
	err (1, "recvfrom");
    printf("Received %d bytes\n", i);
    i = krb_rd_priv(ktxt->dat, i, sched, &ad.session, &otheraddr,
		    &addr, &msg_data);
    if (i != KSUCCESS)
	errx (1, "krb_rd_priv: %s", krb_get_err_text(i));
    printf("Decrypted message is: %s\n", msg_data.app_data);
    return(0);
}
