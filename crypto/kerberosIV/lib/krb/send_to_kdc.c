/* 
  Copyright (C) 1989 by the Massachusetts Institute of Technology

   Export of this software from the United States of America is assumed
   to require a specific license from the United States Government.
   It is the responsibility of any person or organization contemplating
   export to obtain such a license before exporting.

WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
distribute this software and its documentation for any purpose and
without fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation, and that
the name of M.I.T. not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.  M.I.T. makes no representations about the suitability of
this software for any purpose.  It is provided "as is" without express
or implied warranty.

  */

#include "krb_locl.h"

RCSID("$Id: send_to_kdc.c,v 1.39 1997/05/15 21:02:31 joda Exp $");

struct host {
  struct sockaddr_in addr;
  int proto;
};

static const char *prog = "send_to_kdc";
static send_recv(KTEXT pkt, KTEXT rpkt, int f,
		 struct sockaddr_in *_to, struct host *addrs,
		 int h_hosts);

/*
 * This file contains two routines, send_to_kdc() and send_recv().
 * send_recv() is a static routine used by send_to_kdc().
 */

/*
 * send_to_kdc() sends a message to the Kerberos authentication
 * server(s) in the given realm and returns the reply message.
 * The "pkt" argument points to the message to be sent to Kerberos;
 * the "rpkt" argument will be filled in with Kerberos' reply.
 * The "realm" argument indicates the realm of the Kerberos server(s)
 * to transact with.  If the realm is null, the local realm is used.
 *
 * If more than one Kerberos server is known for a given realm,
 * different servers will be queried until one of them replies.
 * Several attempts (retries) are made for each server before
 * giving up entirely.
 *
 * If an answer was received from a Kerberos host, KSUCCESS is
 * returned.  The following errors can be returned:
 *
 * SKDC_CANT    - can't get local realm
 *              - can't find "kerberos" in /etc/services database
 *              - can't open socket
 *              - can't bind socket
 *              - all ports in use
 *              - couldn't find any Kerberos host
 *
 * SKDC_RETRY   - couldn't get an answer from any Kerberos server,
 *		  after several retries
 */

int
send_to_kdc(KTEXT pkt, KTEXT rpkt, char *realm)
{
    int i;
    int no_host; /* was a kerberos host found? */
    int retry;
    int n_hosts;
    int retval;
    struct hostent *host;
    char lrealm[REALM_SZ];
    struct krb_host *k_host;
    struct host *hosts = malloc(sizeof(*hosts));

    if (hosts == NULL)
      return SKDC_CANT;

    /*
     * If "realm" is non-null, use that, otherwise get the
     * local realm.
     */
    if (realm)
	strcpy(lrealm, realm);
    else
	if (krb_get_lrealm(lrealm,1)) {
	    if (krb_debug)
		krb_warning("%s: can't get local realm\n", prog);
	    return(SKDC_CANT);
	}
    if (krb_debug)
      krb_warning("lrealm is %s\n", lrealm);

    no_host = 1;
    /* get an initial allocation */
    n_hosts = 0;
    for (i = 1; (k_host = krb_get_host(i, lrealm, 0)); ++i) {
	char *p;

        if (krb_debug)
	  krb_warning("Getting host entry for %s...", k_host->host);
        host = gethostbyname(k_host->host);
        if (krb_debug) {
	  krb_warning("%s.\n",
		      host ? "Got it" : "Didn't get it");
        }
        if (!host)
            continue;
        no_host = 0;    /* found at least one */
	while ((p = *(host->h_addr_list)++)) {
	    hosts = realloc(hosts, sizeof(*hosts) * (n_hosts + 1));
	    if (hosts == NULL)
		return SKDC_CANT;
	    memset (&hosts[n_hosts].addr, 0, sizeof(hosts[n_hosts].addr));
	    hosts[n_hosts].addr.sin_family = host->h_addrtype;
	    hosts[n_hosts].addr.sin_port = htons(k_host->port);
	    hosts[n_hosts].proto = k_host->proto;
	    memcpy(&hosts[n_hosts].addr.sin_addr, p,
		   sizeof(hosts[n_hosts].addr.sin_addr));
	    ++n_hosts;
	    if (send_recv(pkt, rpkt, hosts[n_hosts-1].proto,
			  &hosts[n_hosts-1].addr, hosts, n_hosts)) {
		retval = KSUCCESS;
		goto rtn;
	    }
	    if (krb_debug) {
		krb_warning("Timeout, error, or wrong descriptor\n");
	    }
	}
    }
    if (no_host) {
	if (krb_debug)
	    krb_warning("%s: can't find any Kerberos host.\n",
			prog);
        retval = SKDC_CANT;
        goto rtn;
    }
    /* retry each host in sequence */
    for (retry = 0; retry < CLIENT_KRB_RETRY; ++retry) {
	for (i = 0; i < n_hosts; ++i) {
	    if (send_recv(pkt, rpkt,
			  hosts[i].proto,
			  &hosts[i].addr,
			  hosts,
			  n_hosts)) {
		retval = KSUCCESS;
		goto rtn;
	    }
        }
    }
    retval = SKDC_RETRY;
rtn:
    free(hosts);
    return(retval);
}

/*
 * try to send out and receive message.
 * return 1 on success, 0 on failure
 */

static int
send_recv_it(KTEXT pkt, KTEXT rpkt, int stream, int f, 
	     struct sockaddr_in *_to, struct host *addrs, int n_hosts)
{
    fd_set readfds;
    int numsent;
    
    /* CLIENT_KRB_TIMEOUT indicates the time to wait before
     * retrying a server.  It's defined in "krb.h".
     */
    struct timeval timeout;
    timeout.tv_sec = CLIENT_KRB_TIMEOUT;
    timeout.tv_usec = 0;

    if (krb_debug) {
        if (_to->sin_family == AF_INET)
	    krb_warning("Sending message to %s...",
			inet_ntoa(_to->sin_addr));
        else
	    krb_warning("Sending message...");
    }
    if(stream){
	unsigned char tmp[4];
	krb_put_int(pkt->length, tmp, 4);
	if((numsent = send(f, tmp, 4, 0)) != 4){
	    if (krb_debug)
		krb_warning("sent only %d/%d\n", numsent, 4);
	    return 0;
	}
    }
    if ((numsent = send(f, pkt->dat, pkt->length, 0)) != pkt->length) {
        if (krb_debug)
            krb_warning("sent only %d/%d\n",numsent, pkt->length);
        return 0;
    }
    if (krb_debug)
	krb_warning("Sent\nWaiting for reply...");
    FD_ZERO(&readfds);
    FD_SET(f, &readfds);
    /* select - either recv is ready, or timeout */
    /* see if timeout or error or wrong descriptor */
    if (select(f + 1, &readfds, 0, 0, &timeout) < 1
        || !FD_ISSET(f, &readfds)) {
        if (krb_debug)
            krb_warning("select failed: errno = %d", errno);
        return 0;
    }
    if(stream){
	if(krb_net_read(f, rpkt->dat, sizeof(rpkt->dat)) <= 0)
	    return 0;
    }else{
	if (recv (f, rpkt->dat, sizeof(rpkt->dat), 0) < 0) {
	    if (krb_debug)
		krb_warning("recvfrom: errno = %d\n", errno);
	    return 0;
	}
    }
    return 1;
}

static int
send_recv(KTEXT pkt, KTEXT rpkt, int proto, struct sockaddr_in *_to,
	  struct host *addrs, int n_hosts)
{
    int f;
    int ret = 0;
    if(proto == IPPROTO_UDP)
	f = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    else if(proto == IPPROTO_TCP)
	f = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    else{
	krb_warning("Unknown protocol `%d'.\n", proto);
	return 0;
    }
    
    if(connect(f, (struct sockaddr*)_to, sizeof(*_to)) < 0)
	krb_warning("Connecting socket: errno = %d\n", errno);
    else
	ret = send_recv_it(pkt, rpkt, proto == IPPROTO_TCP, f, 
			   _to, addrs, n_hosts);
    
    close(f);
    return ret;
}

