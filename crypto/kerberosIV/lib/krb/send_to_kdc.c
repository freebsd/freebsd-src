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
#include <base64.h>

RCSID("$Id: send_to_kdc.c,v 1.71.2.1 2000/10/10 12:47:21 assar Exp $");

struct host {
    struct sockaddr_in addr;
    const char *hostname;
    enum krb_host_proto proto;
};

static int send_recv(KTEXT pkt, KTEXT rpkt, struct host *host);

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

/* always use the admin server */
static int krb_use_admin_server_flag = 0;

static int client_timeout = -1;

int
krb_use_admin_server(int flag)
{
    int old = krb_use_admin_server_flag;
    krb_use_admin_server_flag = flag;
    return old;
}

#define PROXY_VAR "krb4_proxy"

static int
expand (struct host **ptr, size_t sz)
{
    void *tmp;

    tmp = realloc (*ptr, sz) ;
    if (tmp == NULL)
	return SKDC_CANT;
    *ptr = tmp;
    return 0;
}

int
send_to_kdc(KTEXT pkt, KTEXT rpkt, const char *realm)
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
    const char *proxy = krb_get_config_string (PROXY_VAR);

    if (hosts == NULL)
	return SKDC_CANT;

    if (client_timeout == -1) {
	const char *to;

	client_timeout = CLIENT_KRB_TIMEOUT;
	to = krb_get_config_string ("kdc_timeout");
	if (to != NULL) {
	    int tmp;
	    char *end;

	    tmp = strtol (to, &end, 0);
	    if (end != to)
		client_timeout = tmp;
	}
    }

    /*
     * If "realm" is non-null, use that, otherwise get the
     * local realm.
     */
    if (realm == NULL) {
	if (krb_get_lrealm(lrealm,1)) {
	    if (krb_debug)
		krb_warning("send_to_kdc: can't get local realm\n");
	    return(SKDC_CANT);
	}
	realm = lrealm;
    }
    if (krb_debug)
	krb_warning("lrealm is %s\n", realm);

    no_host = 1;
    /* get an initial allocation */
    n_hosts = 0;
    for (i = 1;
	 (k_host = krb_get_host(i, realm, krb_use_admin_server_flag)); 
	 ++i) {
	char *p;
	char **addr_list;
	int j;
	int n_addrs;
	struct host *tmp;

	if (k_host->proto == PROTO_HTTP && proxy != NULL) {
	    n_addrs = 1;
	    no_host = 0;

	    retval = expand (&hosts, (n_hosts + n_addrs) * sizeof(*hosts));
	    if (retval)
		goto rtn;

	    memset (&hosts[n_hosts].addr, 0, sizeof(struct sockaddr_in));
	    hosts[n_hosts].addr.sin_port = htons(k_host->port);
	    hosts[n_hosts].proto         = k_host->proto;
	    hosts[n_hosts].hostname      = k_host->host;
	} else {
	    if (krb_debug)
		krb_warning("Getting host entry for %s...", k_host->host);
	    host = gethostbyname(k_host->host);
	    if (krb_debug) {
		krb_warning("%s.\n",
			    host ? "Got it" : "Didn't get it");
	    }
	    if (host == NULL)
		continue;
	    no_host = 0;    /* found at least one */

	    n_addrs = 0;
	    for (addr_list = host->h_addr_list;
		 *addr_list != NULL;
		 ++addr_list)
		++n_addrs;

	    retval = expand (&hosts, (n_hosts + n_addrs) * sizeof(*hosts));
	    if (retval)
		goto rtn;

	    for (addr_list = host->h_addr_list, j = 0;
		 (p = *addr_list) != NULL;
		 ++addr_list, ++j) {
		memset (&hosts[n_hosts + j].addr, 0,
			sizeof(struct sockaddr_in));
		hosts[n_hosts + j].addr.sin_family = host->h_addrtype;
		hosts[n_hosts + j].addr.sin_port   = htons(k_host->port);
		hosts[n_hosts + j].proto           = k_host->proto;
		hosts[n_hosts + j].hostname        = k_host->host;
		memcpy(&hosts[n_hosts + j].addr.sin_addr, p,
		       sizeof(struct in_addr));
	    }
	}

	for (j = 0; j < n_addrs; ++j) {
	    if (send_recv(pkt, rpkt, &hosts[n_hosts + j])) {
		retval = KSUCCESS;
		goto rtn;
	    }
	    if (krb_debug) {
		krb_warning("Timeout, error, or wrong descriptor\n");
	    }
	}
	n_hosts += j;
    }
    if (no_host) {
	if (krb_debug)
	    krb_warning("send_to_kdc: can't find any Kerberos host.\n");
        retval = SKDC_CANT;
        goto rtn;
    }
    /* retry each host in sequence */
    for (retry = 0; retry < CLIENT_KRB_RETRY; ++retry) {
	for (i = 0; i < n_hosts; ++i) {
	    if (send_recv(pkt, rpkt, &hosts[i])) {
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

static int
udp_socket(void)
{
    return socket(AF_INET, SOCK_DGRAM, 0);
}

static int
udp_connect(int s, struct host *host)
{
    if(krb_debug) {
	krb_warning("connecting to %s (%s) udp, port %d\n", 
		    host->hostname,
		    inet_ntoa(host->addr.sin_addr),
		    ntohs(host->addr.sin_port));
    }
    return connect(s, (struct sockaddr*)&host->addr, sizeof(host->addr));
}

static int
udp_send(int s, struct host *host, KTEXT pkt)
{
    if(krb_debug) {
	krb_warning("sending %d bytes to %s (%s), udp port %d\n", 
		    pkt->length,
		    host->hostname,
		    inet_ntoa(host->addr.sin_addr), 
		    ntohs(host->addr.sin_port));
    }
    return send(s, pkt->dat, pkt->length, 0);
}

static int
tcp_socket(void)
{
    return socket(AF_INET, SOCK_STREAM, 0);
}

static int
tcp_connect(int s, struct host *host)
{
    if(krb_debug) {
	krb_warning("connecting to %s (%s), tcp port %d\n", 
		    host->hostname,
		    inet_ntoa(host->addr.sin_addr), 
		    ntohs(host->addr.sin_port));
    }
    return connect(s, (struct sockaddr*)&host->addr, sizeof(host->addr));
}

static int
tcp_send(int s, struct host *host, KTEXT pkt)
{
    unsigned char len[4];

    if(krb_debug) {
	krb_warning("sending %d bytes to %s (%s), tcp port %d\n", 
		    pkt->length,
		    host->hostname,
		    inet_ntoa(host->addr.sin_addr), 
		    ntohs(host->addr.sin_port));
    }
    krb_put_int(pkt->length, len, sizeof(len), 4);
    if(send(s, len, sizeof(len), 0) != sizeof(len))
	return -1;
    return send(s, pkt->dat, pkt->length, 0);
}

static int
udptcp_recv(void *buf, size_t len, KTEXT rpkt)
{
    int pktlen = min(len, MAX_KTXT_LEN);

    if(krb_debug)
	krb_warning("recieved %lu bytes on udp/tcp socket\n", 
		    (unsigned long)len);
    memcpy(rpkt->dat, buf, pktlen);
    rpkt->length = pktlen;
    return 0;
}

static int
url_parse(const char *url, char *host, size_t len, short *port)
{
    const char *p;
    size_t n;

    if(strncmp(url, "http://", 7))
	return -1;
    url += 7;
    p = strchr(url, ':');
    if(p) {
	char *end;

	*port = htons(strtol(p + 1, &end, 0));
	if (end == p + 1)
	    return -1;
	n = p - url;
    } else {
	*port = k_getportbyname ("http", "tcp", htons(80));
	p = strchr(url, '/');
	if (p)
	    n = p - url;
	else
	    n = strlen(url);
    }
    if (n >= len)
	return -1;
    memcpy(host, url, n);
    host[n] = '\0';
    return 0;
}

static int
http_connect(int s, struct host *host)
{
    const char *proxy = krb_get_config_string(PROXY_VAR);
    char proxy_host[MaxHostNameLen];
    short port;
    struct hostent *hp;
    struct sockaddr_in sin;

    if(proxy == NULL) {
	if(krb_debug)
	    krb_warning("Not using proxy.\n");
	return tcp_connect(s, host);
    }
    if(url_parse(proxy, proxy_host, sizeof(proxy_host), &port) < 0)
	return -1;
    hp = gethostbyname(proxy_host);
    if(hp == NULL)
	return -1;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    memcpy(&sin.sin_addr, hp->h_addr, sizeof(sin.sin_addr));
    sin.sin_port = port;
    if(krb_debug) {
	krb_warning("connecting to proxy on %s (%s) port %d\n", 
		    proxy_host, inet_ntoa(sin.sin_addr), ntohs(port));
    }
    return connect(s, (struct sockaddr*)&sin, sizeof(sin));
}

static int
http_send(int s, struct host *host, KTEXT pkt)
{
    const char *proxy = krb_get_config_string (PROXY_VAR);
    char *str;
    char *msg;

    if(base64_encode(pkt->dat, pkt->length, &str) < 0)
	return -1;
    if(proxy != NULL) {
	if(krb_debug) {
	    krb_warning("sending %d bytes to %s, tcp port %d (via proxy)\n", 
			pkt->length,
			host->hostname,
			ntohs(host->addr.sin_port));
	}
	asprintf(&msg, "GET http://%s:%d/%s HTTP/1.0\r\n\r\n",
		 host->hostname,
		 ntohs(host->addr.sin_port),
		 str);
    } else {
	if(krb_debug) {
	    krb_warning("sending %d bytes to %s (%s), http port %d\n", 
			pkt->length,
			host->hostname,
			inet_ntoa(host->addr.sin_addr), 
			ntohs(host->addr.sin_port));
	}
	asprintf(&msg, "GET %s HTTP/1.0\r\n\r\n", str);
    }
    free(str);

    if (msg == NULL)
	return -1;
	
    if(send(s, msg, strlen(msg), 0) != strlen(msg)){
	free(msg);
	return -1;
    }
    free(msg);
    return 0;
}

static int
http_recv(void *buf, size_t len, KTEXT rpkt)
{
    char *p;
    char *tmp = malloc(len + 1);

    if (tmp == NULL)
	return -1;
    memcpy(tmp, buf, len);
    tmp[len] = 0;
    p = strstr(tmp, "\r\n\r\n");
    if(p == NULL){
	free(tmp);
	return -1;
    }
    p += 4;
    if(krb_debug)
	krb_warning("recieved %lu bytes on http socket\n", 
		    (unsigned long)((tmp + len) - p));
    if((tmp + len) - p > MAX_KTXT_LEN) {
	free(tmp);
	return -1;
    }
    if (strncasecmp (tmp, "HTTP/1.0 2", 10) != 0
	&& strncasecmp (tmp, "HTTP/1.1 2", 10) != 0) {
	free (tmp);
	return -1;
    }
    memcpy(rpkt->dat, p, (tmp + len) - p);
    rpkt->length = (tmp + len) - p;
    free(tmp);
    return 0;
}

static struct proto_descr {
    int proto;
    int stream_flag;
    int (*socket)(void);
    int (*connect)(int, struct host *host);
    int (*send)(int, struct host *host, KTEXT);
    int (*recv)(void*, size_t, KTEXT);
} protos[] = {
    { PROTO_UDP, 0, udp_socket, udp_connect, udp_send, udptcp_recv },
    { PROTO_TCP, 1, tcp_socket, tcp_connect, tcp_send, udptcp_recv },
    { PROTO_HTTP, 1, tcp_socket, http_connect, http_send, http_recv }
};

static int
send_recv(KTEXT pkt, KTEXT rpkt, struct host *host)
{
    int i;
    int s;
    unsigned char buf[MAX_KTXT_LEN];
    int offset = 0;
    
    for(i = 0; i < sizeof(protos) / sizeof(protos[0]); i++){
	if(protos[i].proto == host->proto)
	    break;
    }
    if(i == sizeof(protos) / sizeof(protos[0]))
	return FALSE;
    if((s = (*protos[i].socket)()) < 0)
	return FALSE;
    if((*protos[i].connect)(s, host) < 0) {
	close(s);
	return FALSE;
    }
    if((*protos[i].send)(s, host, pkt) < 0) {
	close(s);
	return FALSE;
    }
    do{
	fd_set readfds;
	struct timeval timeout;
	int len;
	timeout.tv_sec = client_timeout;
	timeout.tv_usec = 0;
	FD_ZERO(&readfds);
	if (s >= FD_SETSIZE) {
	    if (krb_debug)
		krb_warning("fd too large\n");
	    close (s);
	    return FALSE;
	}
	FD_SET(s, &readfds);
	
	/* select - either recv is ready, or timeout */
	/* see if timeout or error or wrong descriptor */
	if(select(s + 1, &readfds, 0, 0, &timeout) < 1 
	   || !FD_ISSET(s, &readfds)) {
	    if (krb_debug)
		krb_warning("select failed: errno = %d\n", errno);
	    close(s);
	    return FALSE;
	}
	len = recv(s, buf + offset, sizeof(buf) - offset, 0);
	if (len < 0) {
	    close(s);
	    return FALSE;
	}
	if(len == 0)
	    break;
	offset += len;
    } while(protos[i].stream_flag);
    close(s);
    if((*protos[i].recv)(buf, offset, rpkt) < 0)
	return FALSE;
    return TRUE;
}

/* The configuration line "hosts: dns files" in /etc/nsswitch.conf is
 * rumored to avoid triggering this bug. */
#if defined(linux) && defined(HAVE__DNS_GETHOSTBYNAME) && 0
/* Linux libc 5.3 is broken probably somewhere in nsw_hosts.o,
 * for now keep this kludge. */
static
struct hostent *gethostbyname(const char *name)
{
  return (void *)_dns_gethostbyname(name);
}
#endif
