/*
 *
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information,
 * please see the file <mit-copyright.h>.
 *
 * sample_client:
 * A sample Kerberos client, which connects to a server on a remote host,
 * at port "sample" (be sure to define it in /etc/services)
 * and authenticates itself to the server. The server then writes back
 * (in ASCII) the authenticated name.
 *
 * Usage:
 * sample_client <hostname> <checksum>
 *
 * <hostname> is the name of the foreign host to contact.
 *
 * <checksum> is an integer checksum to be used for the call to krb_mk_req()
 *	and mutual authentication
 *
 */

#include "sample.h"

RCSID("$Id: sample_client.c,v 1.21 1999/11/13 06:27:01 assar Exp $");

static void
usage (void)
{
  fprintf (stderr, "Usage: %s [-s service] [-p port] hostname checksum\n",
	   __progname);
  exit (1);
}

int
main(int argc, char **argv)
{
    struct hostent *hp;
    struct sockaddr_in sin, lsin;
    char *remote_host;
    int status;
    int namelen;
    int sock = -1;
    KTEXT_ST ticket;
    char buf[512];
    long authopts;
    MSG_DAT msg_data;
    CREDENTIALS cred;
    des_key_schedule sched;
    u_int32_t cksum;
    int c;
    char service[SNAME_SZ];
    u_int16_t port;
    struct servent *serv;
    char **h_addr_list;

    set_progname (argv[0]);
    strlcpy (service, SAMPLE_SERVICE, sizeof(service));
    port = 0;

    while ((c = getopt(argc, argv, "s:p:")) != -1)
	switch(c) {
	case 's' :
	    strlcpy (service, optarg, sizeof(service));
	    break;
	case 'p' :
	    serv = getservbyname (optarg, "tcp");
	    if (serv)
		port = serv->s_port;
	    else
		port = htons(atoi(optarg));
	    break;
	case '?' :
	default :
	    usage();
	}

    argc -= optind;
    argv += optind;

    if (argc != 2)
	usage ();
    
    /* convert cksum to internal rep */
    cksum = atoi(argv[1]);

    printf("Setting checksum to %ld\n", (long)cksum);

    /* clear out the structure first */
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    if (port)
	sin.sin_port = port;
    else
	sin.sin_port = k_getportbyname (service, "tcp", htons(SAMPLE_PORT));

    /* look up the server host */
    hp = gethostbyname(argv[0]);
    if (hp == NULL)
	errx (1, "gethostbyname(%s): %s", argv[0],
	      hstrerror(h_errno));

    /* copy the hostname into non-volatile storage */
    remote_host = strdup(hp->h_name);
    if (remote_host == NULL)
	errx (1, "strdup: out of memory");

    /* set up the address of the foreign socket for connect() */
    sin.sin_family = hp->h_addrtype;

    for (h_addr_list = hp->h_addr_list;
	 *h_addr_list;
	 ++h_addr_list) {
	memcpy(&sin.sin_addr, *h_addr_list, sizeof(sin.sin_addr));
	fprintf (stderr, "Trying %s...\n", inet_ntoa(sin.sin_addr));

	/* open a TCP socket */
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	    err (1, "socket");

	/* connect to the server */
	if (connect(sock, (struct sockaddr *)&sin, sizeof(sin)) >= 0)
	    break;
	close (sock);
    }

    if (*h_addr_list == NULL)
	err (1, "connect");

    /* find out who I am, now that we are connected and therefore bound */
    namelen = sizeof(lsin);
    if (getsockname(sock, (struct sockaddr *) &lsin, &namelen) < 0) {
	close (sock);
	err (1, "getsockname");
    }

    /* call Kerberos library routine to obtain an authenticator,
       pass it over the socket to the server, and obtain mutual
       authentication. */

    authopts = KOPT_DO_MUTUAL;
    status = krb_sendauth(authopts, sock, &ticket,
			  service, remote_host,
			  NULL, cksum, &msg_data, &cred,
			  sched, &lsin, &sin, SAMPLE_VERSION);
    if (status != KSUCCESS)
	errx (1, "cannot authenticate to server: %s",
	      krb_get_err_text(status));

    /* After we send the authenticator to the server, it will write
       back the name we authenticated to. Read what it has to say. */
    status = read(sock, buf, sizeof(buf));
    if (status < 0)
	errx(1, "read");

    /* make sure it's null terminated before printing */
    if (status < sizeof(buf))
	buf[status] = '\0';
    else
	buf[sizeof(buf) - 1] = '\0';

    printf("The server says:\n%s\n", buf);

    close(sock);
    return 0;
}
