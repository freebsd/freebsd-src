/* $FreeBSD$ */

/*
 *
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information,
 * please see the file <mit-copyright.h>.
 *
 * sample_server:
 * A sample Kerberos server, which reads a ticket from a TCP socket,
 * decodes it, and writes back the results (in ASCII) to the client.
 *
 * Usage:
 * sample_server
 *
 * file descriptor 0 (zero) should be a socket connected to the requesting
 * client (this will be correct if this server is started by inetd).
 */

#include "sample.h"

RCSID("$Id: sample_server.c,v 1.14.2.1 2000/06/28 19:08:00 assar Exp $");

static void
usage (void)
{
    fprintf (stderr, "Usage: %s [-i] [-s service] [-t srvtab]\n",
	     __progname);
    exit (1);
}

int
main(int argc, char **argv)
{
    struct sockaddr_in peername, myname;
    int namelen = sizeof(peername);
    int status, count, len;
    long authopts;
    AUTH_DAT auth_data;
    KTEXT_ST clt_ticket;
    des_key_schedule sched;
    char instance[INST_SZ];
    char service[ANAME_SZ];
    char version[KRB_SENDAUTH_VLEN+1];
    char retbuf[512];
    char lname[ANAME_SZ];
    char srvtab[MaxPathLen];
    int c;
    int no_inetd = 0;

    /* open a log connection */

    set_progname (argv[0]);

    roken_openlog(__progname, LOG_ODELAY, LOG_DAEMON);

    strlcpy (service, SAMPLE_SERVICE, sizeof(service));
    *srvtab = '\0';

    while ((c = getopt (argc, argv, "s:t:i")) != -1)
	switch (c) {
	case 's' :
	    strlcpy (service, optarg, sizeof(service));
	    break;
	case 't' :
	    strlcpy (srvtab, optarg, sizeof(srvtab));
	    break;
	case 'i':
	    no_inetd = 1;
	    break;
	case '?' :
	default :
	    usage ();
	}

    if (no_inetd)
	mini_inetd (htons(SAMPLE_PORT));

    /*
     * To verify authenticity, we need to know the address of the
     * client.
     */
    if (getpeername(STDIN_FILENO,
		    (struct sockaddr *)&peername,
		    &namelen) < 0) {
	syslog(LOG_ERR, "getpeername: %m");
	return 1;
    }

    /* for mutual authentication, we need to know our address */
    namelen = sizeof(myname);
    if (getsockname(STDIN_FILENO, (struct sockaddr *)&myname, &namelen) < 0) {
	syslog(LOG_ERR, "getsocknamename: %m");
	return 1;
    }

    /* read the authenticator and decode it.  Using `k_getsockinst' we
     * always get the right instance on a multi-homed host.
     */
    k_getsockinst (STDIN_FILENO, instance, sizeof(instance));

    /* we want mutual authentication */
    authopts = KOPT_DO_MUTUAL;
    status = krb_recvauth(authopts, STDIN_FILENO, &clt_ticket,
			  service, instance, &peername, &myname,
			  &auth_data, srvtab,
			  sched, version);
    if (status != KSUCCESS) {
	snprintf(retbuf, sizeof(retbuf),
		 "Kerberos error: %s\n",
		 krb_get_err_text(status));
	syslog(LOG_ERR, "%s", retbuf);
    } else {
	/* Check the version string (KRB_SENDAUTH_VLEN chars) */
	if (strncmp(version, SAMPLE_VERSION, KRB_SENDAUTH_VLEN)) {
	    /* didn't match the expected version */
	    /* could do something different, but we just log an error
	       and continue */
	    version[8] = '\0';		/* make sure null term */
	    syslog(LOG_ERR, "Version mismatch: '%s' isn't '%s'",
		   version, SAMPLE_VERSION);
	}
	/* now that we have decoded the authenticator, translate
	   the kerberos principal.instance@realm into a local name */
	if (krb_kntoln(&auth_data, lname) != KSUCCESS)
	    strlcpy(lname,
			    "*No local name returned by krb_kntoln*",
			    sizeof(lname));
	/* compose the reply */
	snprintf(retbuf, sizeof(retbuf),
		"You are %s.%s@%s (local name %s),\n at address %s, version %s, cksum %ld\n",
		auth_data.pname,
		auth_data.pinst,
		auth_data.prealm,
		lname,
		inet_ntoa(peername.sin_addr),
		version,
		(long)auth_data.checksum);
    }

    /* write back the response */
    if ((count = write(0, retbuf, (len = strlen(retbuf) + 1))) < 0) {
	syslog(LOG_ERR,"write: %m");
	return 1;
    } else if (count != len) {
	syslog(LOG_ERR, "write count incorrect: %d != %d\n",
		count, len);
	return 1;
    }

    /* close up and exit */
    close(0);
    return 0;
}
