/*
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: send_to_kdc.c,v 4.20 90/01/02 13:40:37 jtkohl Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef lint
static char rcsid_send_to_kdc_c[] =
"$Id: send_to_kdc.c,v 1.1 1994/03/21 17:35:39 piero Exp ";
#endif /* lint */
#endif

#include <krb.h>
#include <prot.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/types.h>
#ifdef lint
#include <sys/uio.h>            /* struct iovec to make lint happy */
#endif /* lint */
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <strings.h>

#define S_AD_SZ sizeof(struct sockaddr_in)

/* Used for extracting addresses from routing messages */
#define ROUNDUP(a) \
        ((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sin_len))

extern int errno;
extern int krb_debug;

extern char *malloc(), *calloc(), *realloc();

int krb_udp_port = 0;

static struct sockaddr_in local_addr = { S_AD_SZ,
					 AF_INET
					};

/* CLIENT_KRB_TIMEOUT indicates the time to wait before
 * retrying a server.  It's defined in "krb.h".
 */
static struct timeval timeout = { CLIENT_KRB_TIMEOUT, 0};
static char *prog = "send_to_kdc";
static send_recv();

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
send_to_kdc(pkt,rpkt,realm)
    KTEXT pkt;
    KTEXT rpkt;
    char *realm;
{
    int i, f;
    int no_host; /* was a kerberos host found? */
    int retry;
    int n_hosts;
    int retval;
    int addr_count;
    struct sockaddr_in to;
    struct hostent *host, *hostlist;
    char krbhst[MAX_HSTNM];
    char lrealm[REALM_SZ];

    /*
     * If "realm" is non-null, use that, otherwise get the
     * local realm.
     */
    if (realm)
	(void) strcpy(lrealm, realm);
    else
	if (krb_get_lrealm(lrealm,1)) {
	    if (krb_debug)
		fprintf(stderr, "%s: can't get local realm\n", prog);
	    return(SKDC_CANT);
	}
    if (krb_debug)
        printf("lrealm is %s\n", lrealm);
    if (krb_udp_port == 0) {
        register struct servent *sp;
        if ((sp = getservbyname("kerberos","udp")) == 0) {
            if (krb_debug)
                fprintf(stderr, "%s: Can't get kerberos/udp service\n",
                        prog);
            return(SKDC_CANT);
        }
        krb_udp_port = sp->s_port;
        if (krb_debug)
            printf("krb_udp_port is %d\n", krb_udp_port);
    }
    bzero((char *)&to, S_AD_SZ);
    hostlist = (struct hostent *) malloc(sizeof(struct hostent));
    if (!hostlist)
        return (/*errno */SKDC_CANT);
    if ((f = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        if (krb_debug)
            fprintf(stderr,"%s: Can't open socket\n", prog);
        return(SKDC_CANT);
    }
    /* from now on, exit through rtn label for cleanup */

    no_host = 1;
    /* get an initial allocation */
    n_hosts = 0;
    for (i = 1; krb_get_krbhst(krbhst, lrealm, i) == KSUCCESS; ++i) {
        if (krb_debug) {
            printf("Getting host entry for %s...",krbhst);
            (void) fflush(stdout);
        }
        host = gethostbyname(krbhst);
        if (krb_debug) {
            printf("%s.\n",
                   host ? "Got it" : "Didn't get it");
            (void) fflush(stdout);
        }
        if (!host)
            continue;
        no_host = 0;    /* found at least one */
        n_hosts++;
	/*
	 * Preserve host network addresses to check against later
	 */
        hostlist = (struct hostent *)
            realloc((char *)hostlist,
                    (unsigned)
                    sizeof(struct hostent)*(n_hosts+1));
	if (!hostlist) {
	    fprintf(stderr, "Could not grow hostlist\n");
	    return /*errno */SKDC_CANT;
	}
	bcopy((char *)host, (char *)&hostlist[n_hosts-1],
              sizeof(struct hostent));
	host = &hostlist[n_hosts-1];
/* At least Sun OS version 3.2 (or worse) and Ultrix version 2.2
   (or worse) only return one address ... */
#if (defined(ULTRIX022) || (defined(SunOS) && SunOS < 40))
	{
	    char *cp = malloc((unsigned)host->h_length);
	    if (!cp) {
		retval = /*errno */SKDC_CANT;
		goto rtn;
	    }
	    bcopy((char *)host->h_addr, cp, host->h_length);
	    host->h_addr = cp;
	}
#else /* !(ULTRIX022 || (SunOS < 40)) */
	/*
	 * Make a copy of the entire h_addr_list.
	 */
	{
	    char *addr;
	    char **old_addr_list;
	    addr_count = 0;
	    old_addr_list = host->h_addr_list;
	    while(old_addr_list[addr_count++])
		;
	    host->h_addr_list = (char **)malloc(addr_count+1 * sizeof(char *));
	    if (host->h_addr_list == NULL) {
		fprintf(stderr, "Could not allocate host->h_addr_list\n");
		retval = SKDC_CANT;
		goto rtn;
	    }   
	    if (krb_debug) {
		printf("h_length = %d\n", host->h_length);  
		printf("Number of addresses = %d\n", addr_count);
	    }
	    for (addr_count = 0; old_addr_list[addr_count]; addr_count++) {
		if (krb_debug)
		    printf ("addr[%d] = %s\n", addr_count,
		       inet_ntoa(*(struct in_addr *)old_addr_list[addr_count]));
		addr = (char *)malloc(host->h_length);
		if (addr == NULL) {
		    fprintf(stderr, "Could not allocate address\n");
		    retval = SKDC_CANT;
		    goto rtn;
		}
		bcopy(old_addr_list[addr_count], addr, host->h_length);
		host->h_addr_list[addr_count] = addr;
	    }
	    host->h_addr_list[addr_count] = NULL;
	}       
#endif /* !(ULTRIX022 || (SunOS < 40)) */

        bzero((char *)&hostlist[n_hosts],
              sizeof(struct hostent));
        to.sin_family = host->h_addrtype;
        bcopy(host->h_addr, (char *)&to.sin_addr,
              host->h_length);
        to.sin_port = krb_udp_port;
	if ((retval = krb_bind_local_addr(f)) != KSUCCESS) {
	    fprintf(stderr, "krb_bind_local_addr: %s", krb_err_txt[retval]);
	    retval = SKDC_CANT;
	    goto rtn;
	}
        if (send_recv(pkt, rpkt, f, &to, hostlist)) {
            retval = KSUCCESS;
            goto rtn;
        }
        if (krb_debug) {
            printf("Timeout, error, or wrong descriptor\n");
            (void) fflush(stdout);
        }
    }
    if (no_host) {
	if (krb_debug)
	    fprintf(stderr, "%s: can't find any Kerberos host.\n", prog);
        retval = SKDC_CANT;
        goto rtn;
    }
    /*
     * retry each host in sequence.  Some addresses may be unreachable
     * from where we are, so loop through them as well.
     */     
     for (retry = 0; retry < CLIENT_KRB_RETRY; ++retry) {
          for (host = hostlist; host->h_name != (char *)NULL; host++) {
#if (defined(ULTRIX022) || (defined(SunOS) && SunOS < 40))
	    to.sin_family = host->h_addrtype;
	    bcopy(host->h_addr_list[addr_count], (char *)&to.sin_addr,
		  host->h_length);
	    if (send_recv(pkt, rpkt, f, &to, hostlist)) {
		retval = KSUCCESS;
		goto rtn;
	    }
#else /* !(ULTRIX022 || (SunOS < 40)) */
	    for (addr_count = 0; host->h_addr_list[addr_count]; addr_count++) {
		to.sin_family = host->h_addrtype;
		bcopy(host->h_addr_list[addr_count], (char *)&to.sin_addr,
		      host->h_length);
		if (send_recv(pkt, rpkt, f, &to, hostlist)) {
		    retval = KSUCCESS;
		    goto rtn;
		}
	}
#endif /* !(ULTRIX022 || (SunOS < 40)) */
      }
    }
    retval = SKDC_RETRY;
rtn:
    (void) close(f);
    if (hostlist) {
	if(!no_host) {
            register struct hostent *hp;
            for (hp = hostlist; hp->h_name; hp++)
#if !(defined(ULTRIX022) || (defined(SunOS) && SunOS < 40))
                if (hp->h_addr_list) {
#endif /* ULTRIX022 || SunOS */
                    if (hp->h_addr)
                        free(hp->h_addr);
#if !(defined(ULTRIX022) || (defined(SunOS) && SunOS < 40))
                    free((char *)hp->h_addr_list);
                }
#endif /* ULTRIX022 || SunOS */
        }
        free((char *)hostlist);
    }
    return(retval);
}

/*
 * try to send out and receive message.
 * return 1 on success, 0 on failure
 */

static int
send_recv(pkt,rpkt,f,_to,addrs)
    KTEXT pkt;
    KTEXT rpkt;
    int f;
    struct sockaddr_in *_to;
    struct hostent *addrs;
{
    fd_set readfds;
    register struct hostent *hp;
    struct sockaddr_in from;
    int sin_size;
    int numsent;
    int addr_count;

    if (krb_debug) {
        if (_to->sin_family == AF_INET)
            printf("Sending message to %s...",
                   inet_ntoa(_to->sin_addr));
        else
            printf("Sending message...");
        (void) fflush(stdout);
    }
    if ((numsent = sendto(f,(char *)(pkt->dat), pkt->length, 0,
			  (struct sockaddr *)_to,
                          S_AD_SZ)) != pkt->length) {
        if (krb_debug)
            printf("sent only %d/%d\n",numsent, pkt->length);
        return 0;
    }
    if (krb_debug) {
        printf("Sent\nWaiting for reply...");
        (void) fflush(stdout);
    }
    FD_ZERO(&readfds);
    FD_SET(f, &readfds);
    errno = 0;
    /* select - either recv is ready, or timeout */
    /* see if timeout or error or wrong descriptor */
    if (select(f + 1, &readfds, (fd_set *)0, (fd_set *)0, &timeout) < 1
        || !FD_ISSET(f, &readfds)) {
        if (krb_debug) {
            fprintf(stderr, "select failed: readfds=%x",
                    readfds);
            perror("");
        }
        return 0;
    }
    sin_size = sizeof(from);
    if (recvfrom(f, (char *)(rpkt->dat), sizeof(rpkt->dat), 0,
		 (struct sockaddr *)&from, &sin_size)
        < 0) {
        if (krb_debug)
            perror("recvfrom");
        return 0;
    }
    if (krb_debug) {
        printf("received packet from %s\n", inet_ntoa(from.sin_addr));
        fflush(stdout);
    }
/* At least Sun OS version 3.2 (or worse) and Ultrix version 2.2
   (or worse) only return one address ... */
#if (defined(ULTRIX022) || (defined(SunOS) && SunOS < 40))
    for (hp = addrs; hp->h_name != (char *)NULL; hp++) {
	if (!bcmp(hp->h_addr, (char *)&from.sin_addr.s_addr,
		  hp->h_length)) {
	    if (krb_debug) {
		printf("Received it\n");
		(void) fflush(stdout);
	    }
	    return 1;
	}
	if (krb_debug)
	    fprintf(stderr, "packet not from %s\n",
		    inet_ntoa(*(struct in_addr *)hp->h_addr));
    }
#else /* !(ULTRIX022 || (SunOS < 40)) */
    for (hp = addrs; hp->h_name != (char *)NULL; hp++) {
	for (addr_count = 0; hp->h_addr_list[addr_count]; addr_count++) {
	    if (!bcmp(hp->h_addr_list[addr_count],
		(char *)&from.sin_addr.s_addr, hp->h_length)) {
		if (krb_debug) {
		    printf("Received it\n");
		    (void) fflush(stdout);
		}
		return 1;
	    }
	    if (krb_debug)
		fprintf(stderr, "packet not from %s\n",
		     inet_ntoa(*(struct in_addr *)hp->h_addr_list[addr_count]));
	}
    }
#endif /* !(ULTRIX022 || (SunOS < 40)) */
    if (krb_debug)
	fprintf(stderr, "%s: received packet from wrong host! (%s)\n",
		"send_to_kdc(send_rcv)", inet_ntoa(from.sin_addr));
    return 0;
}


static int
setfixedaddr(s)
	int s;
{
    struct ifa_msghdr *ifa, *ifa0, *ifa_end;
    struct sockaddr_in *cur_addr;
    int tries;
    int i;
    u_long loopback;
    int mib[6] = { CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_IFLIST, 0 };
    size_t len;

    /* Get information about our interfaces */
#define NUMTRIES 10
    tries = 0;

retry:
    len = 0;
    if (sysctl(mib, 6, NULL, &len, NULL, 0) < 0) {
	perror("setfixedaddr: Can't get size of interface table: sysctl");
	return GT_LADDR_IFLIST;
    }
    ifa = (struct ifa_msghdr *)malloc(len);
    if (!ifa) {
	fprintf(stderr, "setfixedaddr: Cannot malloc\n");
	return (KFAILURE);
    }
    if (sysctl(mib, 6, ifa, &len, NULL, 0) < 0) {
	free(ifa);
	if (errno == ENOMEM && tries < NUMTRIES) {
	    /* Table grew between calls */
	    tries++;
	    goto retry;
	}
	else {
	    perror("setfixedaddr: Can't get interface table: sysctl");
	    return GT_LADDR_IFLIST;
	}
    }
    loopback = inet_addr("127.0.0.1");

    ifa0 = ifa;
    for(ifa_end = (struct ifa_msghdr *)((caddr_t)ifa + len);
	ifa < ifa_end;
	(caddr_t)ifa += ifa->ifam_msglen) {
	/* Ignore interface name messages and ensure we have an address */
	if (ifa->ifam_type == RTM_IFINFO || !(ifa->ifam_addrs & RTAX_IFA))
		continue;
	cur_addr = (struct sockaddr_in *)(ifa + 1);
	for (i = 0; i < RTAX_IFA; i++) {
		if (ifa->ifam_addrs & (1 << i))
			ADVANCE((caddr_t)cur_addr, cur_addr);
	}
	if (cur_addr->sin_addr.s_addr != loopback) {
	    local_addr.sin_addr.s_addr = cur_addr->sin_addr.s_addr;
	    break;
	}
    }
    free(ifa0);
    if (ifa >= ifa_end) {
	return GT_LADDR_NVI;
    }
    if (krb_debug) {
	fprintf(stderr, "setfixedaddr: using local address %s\n",
		inet_ntoa(local_addr.sin_addr));
    }
    return (KSUCCESS);
}

int
krb_bind_local_addr(s)
	int s;
{
    int retval;
    if (local_addr.sin_addr.s_addr == INADDR_ANY) {
	/*
	 * We haven't determined the local interface to use
	 * for kerberos server interactions.  Do so now.
	 */
	if ((retval = setfixedaddr(s)) != KSUCCESS)
	    return (retval);
    }
    if (bind(s, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
	perror("krb_bind_local_addr: bind");
	return BND_LADDR_BIND;
    }
    if (krb_debug)
	printf("local_addr = %s\n", inet_ntoa(local_addr.sin_addr));
    return(KSUCCESS);
}

int
krb_get_local_addr(returned_addr)
	struct sockaddr_in *returned_addr;
{
    int retval;
    if (local_addr.sin_addr.s_addr == INADDR_ANY) {
	/*
	 * We haven't determined the local interface to use
	 * for kerberos server interactions.  Do so now.
	 */
	int s;
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	    return GT_LADDR_NOSOCK; 
	}
	if ((retval = setfixedaddr(s)) != KSUCCESS) {
	    close(s);
	    return (retval);
	}
	close(s);
    }
    if (!returned_addr)
	return(KFAILURE);
    *returned_addr = local_addr;
    if (krb_debug)
	printf("local_addr = %s\n", inet_ntoa(local_addr.sin_addr));
    return (KSUCCESS);
}
