/*
 *
 * Copyright 1989 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * Simple UDP-based sample client program.  For demonstration.
 * This program performs no useful function.
 */

#include "sample.h"
RCSID("$Id: simple_client.c,v 1.15 1999/11/13 06:29:01 assar Exp $");

#define MSG "hi, Jennifer!"		/* message text */

static int
talkto(char *hostname, char *service, int port)
{
  int flags = 0;			/* flags for sendto() */
  long len;
  u_long cksum = 0L;		/* cksum not used */
  char c_realm[REALM_SZ];		/* local Kerberos realm */
  char *s_realm;			/* server's Kerberos realm */

  KTEXT_ST k;			/* Kerberos data */
  KTEXT ktxt = &k;

  int sock, i;
  struct hostent *host;
  struct sockaddr_in s_sock;	/* server address */
  char myhostname[MaxHostNameLen]; /* local hostname */

  /* for krb_mk_safe/priv */
  struct sockaddr_in c_sock;	/* client address */
  CREDENTIALS c;			/* ticket & session key */
  CREDENTIALS *cred = &c;

  /* for krb_mk_priv */
  des_key_schedule sched;		/* session key schedule */

  /* Look up server host */
  if ((host = gethostbyname(hostname)) == NULL) {
    fprintf(stderr, "%s: unknown host \n", hostname);
    return 1;
  }

  /* Set server's address */
  memset(&s_sock, 0, sizeof(s_sock));
  memcpy(&s_sock.sin_addr, host->h_addr, sizeof(s_sock.sin_addr));
  s_sock.sin_family = AF_INET;
  if (port)
    s_sock.sin_port = port;
  else
    s_sock.sin_port = k_getportbyname (service, "tcp", htons(SAMPLE_PORT));

  if (gethostname(myhostname, sizeof(myhostname)) < 0) {
    warn("gethostname");
    return 1;
  }

  if ((host = gethostbyname(myhostname)) == NULL) {
    fprintf(stderr, "%s: unknown host\n", myhostname);
    return 1;
  }

  /* Open a socket */
  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    warn("socket SOCK_DGRAM");
    return 1;
  }

  memset(&c_sock, 0, sizeof(c_sock));
  memcpy(&c_sock.sin_addr, host->h_addr, sizeof(c_sock.sin_addr));
  c_sock.sin_family = AF_INET;

  /* Bind it to set the address; kernel will fill in port # */
  if (bind(sock, (struct sockaddr *)&c_sock, sizeof(c_sock)) < 0) {
    warn("bind");
    return 1;
  }
	
  /* Get local realm, not needed, just an example */
  if ((i = krb_get_lrealm(c_realm, 1)) != KSUCCESS) {
    fprintf(stderr, "can't find local Kerberos realm\n");
    return 1;
  }
  printf("Local Kerberos realm is %s\n", c_realm);

  /* Get Kerberos realm of host */
  s_realm = krb_realmofhost(hostname);

  /* PREPARE KRB_MK_REQ MESSAGE */

  /* Get credentials for server, create krb_mk_req message */
  if ((i = krb_mk_req(ktxt, service, hostname, s_realm, cksum))
      != KSUCCESS) {
    fprintf(stderr, "%s\n", krb_get_err_text(i));
    return 1;
  }
  printf("Got credentials for %s.\n", service);

  /* Send authentication info to server */
  i = sendto(sock, (char *)ktxt->dat, ktxt->length, flags,
	     (struct sockaddr *)&s_sock, sizeof(s_sock));
  if (i < 0)
    warn("sending datagram message");
  printf("Sent authentication data: %d bytes\n", i);

  /* PREPARE KRB_MK_SAFE MESSAGE */

  /* Get my address */
  memset(&c_sock, 0, sizeof(c_sock));
  i = sizeof(c_sock);
  if (getsockname(sock, (struct sockaddr *)&c_sock, &i) < 0) {
    warn("getsockname");
    return 1;
  }

  /* Get session key */
  i = krb_get_cred(service, hostname, s_realm, cred);
  if (i != KSUCCESS)
    return 1;

  /* Make the safe message */
  len = krb_mk_safe(MSG, ktxt->dat, strlen(MSG)+1,
		    &cred->session, &c_sock, &s_sock);

  /* Send it */
  i = sendto(sock, (char *)ktxt->dat, (int) len, flags,
	     (struct sockaddr *)&s_sock, sizeof(s_sock));
  if (i < 0)
    warn("sending safe message");
  printf("Sent checksummed message: %d bytes\n", i);

  /* PREPARE KRB_MK_PRIV MESSAGE */

#ifdef NOENCRYPTION
  memset(sched, 0, sizeof(sched));
#else
  /* Get key schedule for session key */
  des_key_sched(&cred->session, sched);
#endif

  /* Make the encrypted message */
  len = krb_mk_priv(MSG, ktxt->dat, strlen(MSG)+1,
		    sched, &cred->session, &c_sock, &s_sock);

  /* Send it */
  i = sendto(sock, (char *)ktxt->dat, (int) len, flags,
	     (struct sockaddr *)&s_sock, sizeof(s_sock));
  if (i < 0)
    warn("sending encrypted message");
  printf("Sent encrypted message: %d bytes\n", i);
  return 0;
}

static void
usage (void)
{
  fprintf (stderr, "Usage: %s [-s service] [-p port] hostname\n",
	   __progname);
  exit (1);
}

int
main(int argc, char **argv)
{
  int ret = 0;
  int port = 0;
  char service[SNAME_SZ];
  struct servent *serv;
  int c;

  set_progname (argv[0]);

  strlcpy (service, SAMPLE_SERVICE, sizeof(service));

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

  while (argc-- > 0)
    ret &= talkto (*argv++, service, port);
  return ret;
}
