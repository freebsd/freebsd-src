/*	$KAME: ping6.c,v 1.54 2000/06/12 16:16:44 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*	BSDI	ping.c,v 2.3 1996/01/21 17:56:50 jch Exp	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Muuss.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)ping.c	8.1 (Berkeley) 6/5/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * Using the InterNet Control Message Protocol (ICMP) "ECHO" facility,
 * measure round-trip-delays and packet loss across network paths.
 *
 * Author -
 *	Mike Muuss
 *	U. S. Army Ballistic Research Laboratory
 *	December, 1983
 *
 * Status -
 *	Public Domain.  Distribution Unlimited.
 * Bugs -
 *	More statistics could always be gathered.
 *	This program has to run SUID to ROOT to access the ICMP socket.
 */
/*
 * NOTE:
 * USE_SIN6_SCOPE_ID assumes that sin6_scope_id has the same semantics
 * as IPV6_PKTINFO.  Some people object it (sin6_scope_id specifies *link* while
 * IPV6_PKTINFO specifies *interface*.  Link is defined as collection of
 * network attached to 1 or more interfaces)
 */

#include <sys/param.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef IPSEC
#include <netinet6/ah.h>
#include <netinet6/ipsec.h>
#endif

#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#include <md5.h>
#else
#include "md5.h"
#endif

#define MAXPACKETLEN	131072
#define	IP6LEN		40
#define ICMP6ECHOLEN	8	/* icmp echo header len excluding time */
#define ICMP6ECHOTMLEN sizeof(struct timeval)
#define ICMP6_NIQLEN	(ICMP6ECHOLEN + 8)
#define ICMP6_NIRLEN	(ICMP6ECHOLEN + 12) /* 64 bits of nonce + 32 bits ttl */
#define	EXTRA		256	/* for AH and various other headers. weird. */
#define	DEFDATALEN	ICMP6ECHOTMLEN
#define MAXDATALEN	MAXPACKETLEN - IP6LEN - ICMP6ECHOLEN
#define	NROUTES		9		/* number of record route slots */

#define	A(bit)		rcvd_tbl[(bit)>>3]	/* identify byte in array */
#define	B(bit)		(1 << ((bit) & 0x07))	/* identify bit in byte */
#define	SET(bit)	(A(bit) |= B(bit))
#define	CLR(bit)	(A(bit) &= (~B(bit)))
#define	TST(bit)	(A(bit) & B(bit))

#define	F_FLOOD		0x0001
#define	F_INTERVAL	0x0002
#define	F_NUMERIC	0x0004
#define	F_PINGFILLED	0x0008
#define	F_QUIET		0x0010
#define	F_RROUTE	0x0020
#define	F_SO_DEBUG	0x0040
#define	F_VERBOSE	0x0100
#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
#define	F_POLICY	0x0400
#else
#define F_AUTHHDR	0x0200
#define F_ENCRYPT	0x0400
#endif /*IPSEC_POLICY_IPSEC*/
#endif /*IPSEC*/
#define F_NODEADDR	0x0800
#define F_FQDN		0x1000
#define F_INTERFACE	0x2000
#define F_SRCADDR	0x4000
#ifdef IPV6_REACHCONF
#define F_REACHCONF	0x8000
#endif
#define F_HOSTNAME	0x10000
#define F_FQDNOLD	0x20000
#define F_NIGROUP	0x40000
u_int options;

#define IN6LEN		sizeof(struct in6_addr)
#define SA6LEN		sizeof(struct sockaddr_in6)
#define DUMMY_PORT      10101

#define SIN6(s) ((struct sockaddr_in6 *)(s))

/*
 * MAX_DUP_CHK is the number of bits in received table, i.e. the maximum
 * number of received sequence numbers we can keep track of.  Change 128
 * to 8192 for complete accuracy...
 */
#define	MAX_DUP_CHK	(8 * 8192)
int mx_dup_ck = MAX_DUP_CHK;
char rcvd_tbl[MAX_DUP_CHK / 8];

struct addrinfo *res;	
struct sockaddr_in6 dst;        /* who to ping6 */
struct sockaddr_in6 src;        /* src addr of this packet */
int datalen = DEFDATALEN;
int s;				/* socket file descriptor */
u_char outpack[MAXPACKETLEN];
char BSPACE = '\b';		/* characters written for flood */
char DOT = '.';
char *hostname;
int ident;			/* process id to identify our packets */
struct in6_addr srcaddr;

/* counters */
long npackets;			/* max packets to transmit */
long nreceived;			/* # of packets we got back */
long nrepeats;			/* number of duplicates */
long ntransmitted;		/* sequence # for outbound packets = #sent */
int interval = 1;		/* interval between packets */
int hoplimit = -1;		/* hoplimit */

/* timing */
int timing;			/* flag to do timing */
double tmin = 999999999.0;	/* minimum round trip time */
double tmax = 0.0;		/* maximum round trip time */
double tsum = 0.0;		/* sum of all times, for doing average */

/* for node addresses */
u_short naflags;

/* for ancillary data(advanced API) */
struct msghdr smsghdr;
struct iovec smsgiov;
char *scmsg = 0;

int	 main __P((int, char *[]));
void	 fill __P((char *, char *));
int	 get_hoplim __P((struct msghdr *));
struct in6_pktinfo *get_rcvpktinfo __P((struct msghdr *));
void	 onalrm __P((int));
void	 oninfo __P((int));
void	 onint __P((int));
void	 pinger __P((void));
const char *pr_addr __P((struct sockaddr_in6 *));
void	 pr_icmph __P((struct icmp6_hdr *, u_char *));
void	 pr_iph __P((struct ip6_hdr *));
void	 pr_nodeaddr __P((struct icmp6_nodeinfo *, int));
void	 pr_pack __P((u_char *, int, struct msghdr *));
void	 pr_exthdrs __P((struct msghdr *));
void	 pr_ip6opt __P((void *));
void	 pr_rthdr __P((void *));
void	 pr_retip __P((struct ip6_hdr *, u_char *));
void	 summary __P((void));
void	 tvsub __P((struct timeval *, struct timeval *));
int	 setpolicy __P((int, char *));
char	*nigroup __P((char *));
void	 usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct itimerval itimer;
	struct sockaddr_in6 from;
	struct timeval timeout;
	struct addrinfo hints;
	fd_set fdset;
	register int cc, i;
	int ch, fromlen, hold, packlen, preload, optval, ret_ga;
	u_char *datap, *packet;
	char *e, *target, *ifname = NULL;
	int ip6optlen = 0;
	struct cmsghdr *scmsgp = NULL;
	int sockbufsize = 0;
	int usepktinfo = 0;
	struct in6_pktinfo *pktinfo = NULL;
#ifdef USE_RFC2292BIS
	struct ip6_rthdr *rthdr = NULL;
#endif
#ifdef IPSEC_POLICY_IPSEC
	char *policy_in = NULL;
	char *policy_out = NULL;
#endif

	/* just to be sure */
	memset(&smsghdr, 0, sizeof(&smsghdr));
	memset(&smsgiov, 0, sizeof(&smsgiov));

	preload = 0;
	datap = &outpack[ICMP6ECHOLEN + ICMP6ECHOTMLEN];
#ifndef IPSEC
	while ((ch = getopt(argc, argv, "a:b:c:dfHh:I:i:l:nNp:qRS:s:vwW")) != EOF)
#else
#ifdef IPSEC_POLICY_IPSEC
	while ((ch = getopt(argc, argv, "a:b:c:dfHh:I:i:l:nNp:qRS:s:vwWP:")) != EOF)
#else
	while ((ch = getopt(argc, argv, "a:b:c:dfHh:I:i:l:nNp:qRS:s:vwWAE")) != EOF)
#endif /*IPSEC_POLICY_IPSEC*/
#endif
	{
		switch(ch) {
		 case 'a':
		 {
			 char *cp;

			 options |= F_NODEADDR;
			 datalen = 2048; /* XXX: enough? */
			 for (cp = optarg; *cp != '\0'; cp++) {
				 switch(*cp) {
				 case 'a':
					 naflags |= NI_NODEADDR_FLAG_ALL;
					 break;
				 case 'c':
				 case 'C':
					 naflags |= NI_NODEADDR_FLAG_COMPAT;
					 break;
				 case 'l':
				 case 'L':
					 naflags |= NI_NODEADDR_FLAG_LINKLOCAL;
					 break;
				 case 's':
				 case 'S':
					 naflags |= NI_NODEADDR_FLAG_SITELOCAL;
					 break;
				 case 'g':
				 case 'G':
					 naflags |= NI_NODEADDR_FLAG_GLOBAL;
					 break;
				 case 'A': /* experimental. not in the spec */
					 naflags |= NI_NODEADDR_FLAG_ANYCAST;
					 break;
				 default:
					 usage();
					 /*NOTREACHED*/
				 }
			 }
			 break;
		 }
		 case 'b':
#if defined(SO_SNDBUF) && defined(SO_RCVBUF)
			sockbufsize = atoi(optarg);
#else
			err(1,
"-b option ignored: SO_SNDBUF/SO_RCVBUF socket options not supported");
#endif
			break;
		case 'c':
			npackets = strtol(optarg, &e, 10);
			if (npackets <= 0 || *optarg == '\0' || *e != '\0')
				errx(1,
				    "illegal number of packets -- %s", optarg);
			break;
		case 'd':
			options |= F_SO_DEBUG;
			break;
		case 'f':
			if (getuid()) {
				errno = EPERM;
				errx(1, "Must be superuser to flood ping");
			}
			options |= F_FLOOD;
			setbuf(stdout, (char *)NULL);
			break;
		case 'H':
			options |= F_HOSTNAME;
			break;
		case 'h':		/* hoplimit */
			hoplimit = strtol(optarg, &e, 10);
			if (255 < hoplimit || hoplimit < -1)
				errx(1,
				    "illegal hoplimit -- %s", optarg);
			break;
		case 'I':
			ifname = optarg;
			options |= F_INTERFACE;
#ifndef USE_SIN6_SCOPE_ID
			usepktinfo++;
#endif
			break;
		case 'i':		/* wait between sending packets */
			interval = strtol(optarg, &e, 10);
			if (interval <= 0 || *optarg == '\0' || *e != '\0')
				errx(1,
				    "illegal timing interval -- %s", optarg);
			options |= F_INTERVAL;
			break;
		case 'l':
			if (getuid()) {
				errno = EPERM;
				errx(1, "Must be superuser to preload");
			}
			preload = strtol(optarg, &e, 10);
			if (preload < 0 || *optarg == '\0' || *e != '\0')
				errx(1, "illegal preload value -- %s", optarg);
			break;
		case 'n':
			options |= F_NUMERIC;
			break;
		case 'N':
			options |= F_NIGROUP;
			break;
		case 'p':		/* fill buffer with user pattern */
			options |= F_PINGFILLED;
			fill((char *)datap, optarg);
				break;
		case 'q':
			options |= F_QUIET;
			break;
		case 'R':
#ifdef IPV6_REACHCONF
			options |= F_REACHCONF;
			break;
#else
			errx(1, "-R is not supported in this configuration");
#endif
		case 'S':
			/* XXX: use getaddrinfo? */
			if (inet_pton(AF_INET6, optarg, (void *)&srcaddr) != 1)
				errx(1, "invalid IPv6 address: %s", optarg);
			options |= F_SRCADDR;
			usepktinfo++;
			break;
		case 's':		/* size of packet to send */
			datalen = strtol(optarg, &e, 10);
			if (datalen <= 0 || *optarg == '\0' || *e != '\0')
				errx(1, "illegal datalen value -- %s", optarg);
			if (datalen > MAXDATALEN)
				errx(1,
				    "datalen value too large, maximum is %d",
				    MAXDATALEN);
			break;
		case 'v':
			options |= F_VERBOSE;
			break;
		case 'w':
			options |= F_FQDN;
			break;
		case 'W':
			options |= F_FQDNOLD;
			break;
#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
		case 'P':
			options |= F_POLICY;
			if (!strncmp("in", optarg, 2))
				policy_in = strdup(optarg);
			else if (!strncmp("out", optarg, 3))
				policy_out = strdup(optarg);
			else
				errx(1, "invalid security policy");
			break;
#else
		case 'A':
			options |= F_AUTHHDR;
			break;
		case 'E':
			options |= F_ENCRYPT;
			break;
#endif /*IPSEC_POLICY_IPSEC*/
#endif /*IPSEC*/
		default:
			usage();
			/*NOTREACHED*/
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1) {
		usage();
		/*NOTREACHED*/
	}

	if (argc > 1) {
#ifdef USE_SIN6_SCOPE_ID
		ip6optlen += CMSG_SPACE(inet6_rth_space(IPV6_RTHDR_TYPE_0, argc - 1));
#else  /* old advanced API */
		ip6optlen += inet6_rthdr_space(IPV6_RTHDR_TYPE_0, argc - 1);
#endif
	}

	if (options & F_NIGROUP) {
		target = nigroup(argv[argc - 1]);
		if (target == NULL) {
			usage();
			/*NOTREACHED*/
		}
	} else
		target = argv[argc - 1];

	/* getaddrinfo */
	bzero(&hints, sizeof(struct addrinfo));
	if ((options & F_NUMERIC) != 0)
		hints.ai_flags = AI_CANONNAME;
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_ICMPV6;

	ret_ga = getaddrinfo(target, NULL, &hints, &res);
	if (ret_ga) {
		fprintf(stderr, "ping6: %s\n", gai_strerror(ret_ga));
		exit(1);
	}
	if (res->ai_canonname)
		hostname = res->ai_canonname;
	else
		hostname = target;
		
	if (!res->ai_addr)
		errx(1, "getaddrinfo failed");

	(void)memcpy(&dst, res->ai_addr, res->ai_addrlen);

	if (options & F_FLOOD && options & F_INTERVAL)
		errx(1, "-f and -i incompatible options");

	if (datalen >= sizeof(struct timeval))	/* can we time transfer */
		timing = 1;
	packlen = datalen + IP6LEN + ICMP6ECHOLEN + EXTRA;
	if (!(packet = (u_char *)malloc((u_int)packlen)))
		err(1, "Unable to allocate packet");
	if (!(options & F_PINGFILLED))
		for (i = 8; i < datalen; ++i)
			*datap++ = i;

	ident = getpid() & 0xFFFF;

	if ((s = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0)
	  err(1, "socket");

	hold = 1;

	if (options & F_SO_DEBUG)
		(void)setsockopt(s, SOL_SOCKET, SO_DEBUG, (char *)&hold,
		    sizeof(hold));
	optval = IPV6_DEFHLIM;
	if (IN6_IS_ADDR_MULTICAST(&dst.sin6_addr))
		if (setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
			       &optval, sizeof(optval)) == -1)
			err(1, "IPV6_MULTICAST_HOPS");

#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
	if (options & F_POLICY) {
		if (setpolicy(s, policy_in) < 0)
			errx(1, "%s", ipsec_strerror());
		if (setpolicy(s, policy_out) < 0)
			errx(1, "%s", ipsec_strerror());
	}
#else
	if (options & F_AUTHHDR) {
		optval = IPSEC_LEVEL_REQUIRE;
#ifdef IPV6_AUTH_TRANS_LEVEL
		if (setsockopt(s, IPPROTO_IPV6, IPV6_AUTH_TRANS_LEVEL,
				&optval, sizeof(optval)) == -1)
			err(1, "setsockopt(IPV6_AUTH_TRANS_LEVEL)");
#else /* old def */
		if (setsockopt(s, IPPROTO_IPV6, IPV6_AUTH_LEVEL,
				&optval, sizeof(optval)) == -1)
			err(1, "setsockopt(IPV6_AUTH_LEVEL)");
#endif
	}
	if (options & F_ENCRYPT) {
		optval = IPSEC_LEVEL_REQUIRE;
		if (setsockopt(s, IPPROTO_IPV6, IPV6_ESP_TRANS_LEVEL,
				&optval, sizeof(optval)) == -1)
			err(1, "setsockopt(IPV6_ESP_TRANS_LEVEL)");
	}
#endif /*IPSEC_POLICY_IPSEC*/
#endif

#ifdef ICMP6_FILTER
    {
	struct icmp6_filter filt;
	if (!(options & F_VERBOSE)) {
		ICMP6_FILTER_SETBLOCKALL(&filt);
		if ((options & F_FQDN) || (options & F_FQDNOLD) ||
		    (options & F_NODEADDR))
			ICMP6_FILTER_SETPASS(ICMP6_NI_REPLY, &filt);
		else
			ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY, &filt);
	} else {
		ICMP6_FILTER_SETPASSALL(&filt);
	}
	if (setsockopt(s, IPPROTO_ICMPV6, ICMP6_FILTER, &filt,
			sizeof(filt)) < 0)
		err(1, "setsockopt(ICMP6_FILTER)");
    }
#endif /*ICMP6_FILTER*/

	/* let the kerel pass extension headers of incoming packets */
	/* TODO: implement parsing routine */
	if ((options & F_VERBOSE) != 0) {
		int opton = 1;

#ifdef IPV6_RECVRTHDR
		if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVRTHDR, &opton,
			       sizeof(opton)))
			err(1, "setsockopt(IPV6_RECVRTHDR)");
#else  /* old adv. API */
		if (setsockopt(s, IPPROTO_IPV6, IPV6_RTHDR, &opton,
			       sizeof(opton)))
			err(1, "setsockopt(IPV6_RTHDR)");
#endif
#ifdef IPV6_RECVHOPOPTS
		if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVHOPOPTS, &opton,
			       sizeof(opton)))
			err(1, "setsockopt(IPV6_RECVHOPOPTS)");
#else  /* old adv. API */
		if (setsockopt(s, IPPROTO_IPV6, IPV6_HOPOPTS, &opton,
			       sizeof(opton)))
			err(1, "setsockopt(IPV6_HOPOPTS)");
#endif
#ifdef IPV6_RECVDSTOPTS
		if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVDSTOPTS, &opton,
			       sizeof(opton)))
			err(1, "setsockopt(IPV6_RECVDSTOPTS)");
#else  /* olad adv. API */
		if (setsockopt(s, IPPROTO_IPV6, IPV6_DSTOPTS, &opton,
			       sizeof(opton)))
			err(1, "setsockopt(IPV6_DSTOPTS)");
#endif
#ifdef IPV6_RECVRTHDRDSTOPTS
		if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVRTHDRDSTOPTS, &opton,
			       sizeof(opton)))
			err(1, "setsockopt(IPV6_RECVRTHDRDSTOPTS)");
#endif
	}

/*
	optval = 1;
	if (IN6_IS_ADDR_MULTICAST(&dst.sin6_addr))
		if (setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
			       &optval, sizeof(optval)) == -1)
			err(1, "IPV6_MULTICAST_LOOP");
*/

	/* Specify the outgoing interface and/or the source address */
	if (usepktinfo)
		ip6optlen += CMSG_SPACE(sizeof(struct in6_pktinfo));

	if (hoplimit != -1)
		ip6optlen += CMSG_SPACE(sizeof(int));

#ifdef IPV6_REACHCONF
	if (options & F_REACHCONF)
		ip6optlen += CMSG_SPACE(0);
#endif

	/* set IP6 packet options */
	if (ip6optlen) {
		if ((scmsg = (char *)malloc(ip6optlen)) == 0)
			errx(1, "can't allocate enough memory");
		smsghdr.msg_control = (caddr_t)scmsg;
		smsghdr.msg_controllen = ip6optlen;
		scmsgp = (struct cmsghdr *)scmsg;
	}
	if (usepktinfo) {
		pktinfo = (struct in6_pktinfo *)(CMSG_DATA(scmsgp));
		memset(pktinfo, 0, sizeof(*pktinfo));
		scmsgp->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
		scmsgp->cmsg_level = IPPROTO_IPV6;
		scmsgp->cmsg_type = IPV6_PKTINFO;
		scmsgp = CMSG_NXTHDR(&smsghdr, scmsgp);
	}

	/* set the outgoing interface */
	if (ifname) {
#ifndef USE_SIN6_SCOPE_ID
		/* pktinfo must have already been allocated */
		if ((pktinfo->ipi6_ifindex = if_nametoindex(ifname)) == 0)
			    errx(1, "%s: invalid interface name", ifname);
#else
		if ((dst.sin6_scope_id = if_nametoindex(ifname)) == 0)
			errx(1, "%s: invalid interface name", ifname);
#endif
	}
	/* set the source address */
	if (options & F_SRCADDR)/* pktinfo must be valid */
		pktinfo->ipi6_addr = srcaddr;

	if (hoplimit != -1) {
		scmsgp->cmsg_len = CMSG_LEN(sizeof(int));
		scmsgp->cmsg_level = IPPROTO_IPV6;
		scmsgp->cmsg_type = IPV6_HOPLIMIT;
		*(int *)(CMSG_DATA(scmsgp)) = hoplimit;

		scmsgp = CMSG_NXTHDR(&smsghdr, scmsgp);
	}
#ifdef IPV6_REACHCONF
	if (options & F_REACHCONF) {
		scmsgp->cmsg_len = CMSG_LEN(0);
		scmsgp->cmsg_level = IPPROTO_IPV6;
		scmsgp->cmsg_type = IPV6_REACHCONF;

		scmsgp = CMSG_NXTHDR(&smsghdr, scmsgp);
	}
#endif

	if (argc > 1) {	/* some intermediate addrs are specified */
		int hops, error;
#ifdef USE_RFC2292BIS
		int rthdrlen;
#endif

#ifdef USE_RFC2292BIS
		rthdrlen = inet6_rth_space(IPV6_RTHDR_TYPE_0, argc - 1);
		scmsgp->cmsg_len = CMSG_LEN(rthdrlen);
		scmsgp->cmsg_level = IPPROTO_IPV6;
		scmsgp->cmsg_type = IPV6_RTHDR;
		rthdr = (struct ip6_rthdr *)CMSG_DATA(scmsgp);
		rthdr = inet6_rth_init((void *)rthdr, rthdrlen,
				       IPV6_RTHDR_TYPE_0, argc - 1);
		if (rthdr == NULL)
			errx(1, "can't initialize rthdr");
#else  /* old advanced API */
		if ((scmsgp = (struct cmsghdr *)inet6_rthdr_init(scmsgp,
								 IPV6_RTHDR_TYPE_0)) == 0)
			errx(1, "can't initialize rthdr");
#endif /* USE_RFC2292BIS */

		for (hops = 0; hops < argc - 1; hops++) {
			struct addrinfo *iaip;

			if ((error = getaddrinfo(argv[hops], NULL, &hints, &iaip)))
				errx(1, "%s", gai_strerror(error));
			if (SIN6(res->ai_addr)->sin6_family != AF_INET6)
				errx(1,
				     "bad addr family of an intermediate addr");

#ifdef USE_RFC2292BIS
			if (inet6_rth_add(rthdr,
					  &(SIN6(iaip->ai_addr))->sin6_addr))
				errx(1, "can't add an intermediate node");
#else  /* old advanced API */
			if (inet6_rthdr_add(scmsgp,
					    &(SIN6(iaip->ai_addr))->sin6_addr,
					    IPV6_RTHDR_LOOSE))
				errx(1, "can't add an intermediate node");
#endif /* USE_RFC2292BIS */
		}

#ifndef USE_RFC2292BIS
		if (inet6_rthdr_lasthop(scmsgp, IPV6_RTHDR_LOOSE))
			errx(1, "can't set the last flag");
#endif

		scmsgp = CMSG_NXTHDR(&smsghdr, scmsgp);
	}

	{
		/*
		 * source selection
		 */
		int dummy, len = sizeof(src);
		
		if ((dummy = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
			err(1, "UDP socket");

		src.sin6_family = AF_INET6;
		src.sin6_addr = dst.sin6_addr;
		src.sin6_port = ntohs(DUMMY_PORT);
		src.sin6_scope_id = dst.sin6_scope_id;


#ifdef USE_SIN6_SCOPE_ID
		src.sin6_scope_id = dst.sin6_scope_id;
#endif

#ifdef USE_RFC2292BIS
		if (pktinfo &&
		    setsockopt(dummy, IPPROTO_IPV6, IPV6_PKTINFO,
			       (void *)pktinfo, sizeof(*pktinfo)))
			err(1, "UDP setsockopt(IPV6_PKTINFO)");

		if (hoplimit != -1 &&
		    setsockopt(dummy, IPPROTO_IPV6, IPV6_HOPLIMIT,
			       (void *)&hoplimit, sizeof(hoplimit)))
			err(1, "UDP setsockopt(IPV6_HOPLIMIT)");

		if (rthdr &&
		    setsockopt(dummy, IPPROTO_IPV6, IPV6_RTHDR,
			       (void *)rthdr, (rthdr->ip6r_len + 1) << 3))
			err(1, "UDP setsockopt(IPV6_RTHDR)");
#else  /* old advanced API */
		if (smsghdr.msg_control &&
		    setsockopt(dummy, IPPROTO_IPV6, IPV6_PKTOPTIONS,
			       (void *)smsghdr.msg_control,
			       smsghdr.msg_controllen)) {
			err(1, "UDP setsockopt(IPV6_PKTOPTIONS)");
		}
#endif
				
		if (connect(dummy, (struct sockaddr *)&src, len) < 0)
			err(1, "UDP connect");
	
		if (getsockname(dummy, (struct sockaddr *)&src, &len) < 0)
			err(1, "getsockname");

		close(dummy);
	}

#if defined(SO_SNDBUF) && defined(SO_RCVBUF)
	if (sockbufsize) {
		if (datalen > sockbufsize)
			warnx("you need -b to increase socket buffer size");
		if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, &sockbufsize,
			       sizeof(sockbufsize)) < 0)
			err(1, "setsockopt(SO_SNDBUF)");
		if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, &sockbufsize,
			       sizeof(sockbufsize)) < 0)
			err(1, "setsockopt(SO_RCVBUF)");
	}
	else {
		if (datalen > 8 * 1024)	/*XXX*/
			warnx("you need -b to increase socket buffer size");
		/*
		 * When pinging the broadcast address, you can get a lot of
		 * answers. Doing something so evil is useful if you are trying
		 * to stress the ethernet, or just want to fill the arp cache
		 * to get some stuff for /etc/ethers.
		 */
		hold = 48 * 1024;
		setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&hold, sizeof(hold));
	}
#endif

	optval = 1;
#ifndef USE_SIN6_SCOPE_ID
#ifdef IPV6_RECVPKTINFO
	if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVPKTINFO, &optval,
		       sizeof(optval)) < 0)
		warn("setsockopt(IPV6_RECVPKTINFO)"); /* XXX err? */
#else  /* old adv. API */
	if (setsockopt(s, IPPROTO_IPV6, IPV6_PKTINFO, &optval,
		       sizeof(optval)) < 0)
		warn("setsockopt(IPV6_PKTINFO)"); /* XXX err? */
#endif
#endif /* USE_SIN6_SCOPE_ID */
#ifdef IPV6_RECVHOPLIMIT
	if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &optval,
		       sizeof(optval)) < 0)
		warn("setsockopt(IPV6_RECVHOPLIMIT)"); /* XXX err? */
#else  /* old adv. API */
	if (setsockopt(s, IPPROTO_IPV6, IPV6_HOPLIMIT, &optval,
		       sizeof(optval)) < 0)
		warn("setsockopt(IPV6_HOPLIMIT)"); /* XXX err? */
#endif

	printf("PING6(%d=40+8+%d bytes) ", datalen + 48, datalen);
	printf("%s --> ", pr_addr(&src));
	printf("%s\n", pr_addr(&dst));

	while (preload--)		/* Fire off them quickies. */
		pinger();

	(void)signal(SIGINT, onint);
	(void)signal(SIGINFO, oninfo);

	if ((options & F_FLOOD) == 0) {
		(void)signal(SIGALRM, onalrm);
		itimer.it_interval.tv_sec = interval;
		itimer.it_interval.tv_usec = 0;
		itimer.it_value.tv_sec = 0;
		itimer.it_value.tv_usec = 1;
		(void)setitimer(ITIMER_REAL, &itimer, NULL);
	}

	FD_ZERO(&fdset);
	timeout.tv_sec = 0;
	timeout.tv_usec = 10000;
	for (;;) {
		struct msghdr m;
		struct cmsghdr *cm;
		u_char buf[1024];
		struct iovec iov[2];

		if (options & F_FLOOD) {
			pinger();
			FD_SET(s, &fdset);
			if (select(s + 1, &fdset, NULL, NULL, &timeout) < 1)
				continue;
		}
		fromlen = sizeof(from);

		m.msg_name = (caddr_t)&from;
		m.msg_namelen = sizeof(from);
		memset(&iov, 0, sizeof(iov));
		iov[0].iov_base = (caddr_t)packet;
		iov[0].iov_len = packlen;
		m.msg_iov = iov;
		m.msg_iovlen = 1;
		cm = (struct cmsghdr *)buf;
		m.msg_control = (caddr_t)buf;
		m.msg_controllen = sizeof(buf);

		if ((cc = recvmsg(s, &m, 0)) < 0) {
			if (errno == EINTR)
				continue;
			warn("recvfrom");
			continue;
		}

		pr_pack(packet, cc, &m);
		if (npackets && nreceived >= npackets)
			break;
	}
	summary();
	exit(nreceived == 0);
}

/*
 * onalrm --
 *	This routine transmits another ping6.
 */
/* ARGSUSED */
void
onalrm(signo)
	int signo;
{
	struct itimerval itimer;

	if (!npackets || ntransmitted < npackets) {
		pinger();
		return;
	}

	/*
	 * If we're not transmitting any more packets, change the timer
	 * to wait two round-trip times if we've received any packets or
	 * ten seconds if we haven't.
	 */
#define	MAXWAIT		10
	if (nreceived) {
		itimer.it_value.tv_sec =  2 * tmax / 1000;
		if (itimer.it_value.tv_sec == 0)
			itimer.it_value.tv_sec = 1;
	} else
		itimer.it_value.tv_sec = MAXWAIT;
	itimer.it_interval.tv_sec = 0;
	itimer.it_interval.tv_usec = 0;
	itimer.it_value.tv_usec = 0;

	(void)signal(SIGALRM, onint);
	(void)setitimer(ITIMER_REAL, &itimer, NULL);
}

/*
 * pinger --
 *	Compose and transmit an ICMP ECHO REQUEST packet.  The IP packet
 * will be added on by the kernel.  The ID field is our UNIX process ID,
 * and the sequence number is an ascending integer.  The first 8 bytes
 * of the data portion are used to hold a UNIX "timeval" struct in VAX
 * byte-order, to compute the round-trip time.
 */
void
pinger()
{
	struct icmp6_hdr *icp;
	struct iovec iov[2];
	int i, cc;

	icp = (struct icmp6_hdr *)outpack;
	memset(icp, 0, sizeof(*icp));
	icp->icmp6_code = 0;
	icp->icmp6_cksum = 0;
	icp->icmp6_seq = ntransmitted++;		/* htons later */
	icp->icmp6_id = htons(ident);			/* ID */

	CLR(icp->icmp6_seq % mx_dup_ck);
	icp->icmp6_seq = htons(icp->icmp6_seq);

	if (options & F_FQDN) {
		icp->icmp6_type = ICMP6_NI_QUERY;
		icp->icmp6_code = ICMP6_NI_SUBJ_IPV6;
		/* XXX: overwrite icmp6_id */
		((struct icmp6_nodeinfo *)icp)->ni_qtype = htons(NI_QTYPE_FQDN);
		((struct icmp6_nodeinfo *)icp)->ni_flags = htons(0);
		if (timing)
			(void)gettimeofday((struct timeval *)
					   &outpack[ICMP6ECHOLEN], NULL);
		memcpy(&outpack[ICMP6_NIQLEN], &dst.sin6_addr,
		    sizeof(dst.sin6_addr));
		cc = ICMP6_NIQLEN + sizeof(dst.sin6_addr);
		datalen = 0;
	} else if (options & F_FQDNOLD) {
		/* packet format in 03 draft - no Subject data on queries */
		icp->icmp6_type = ICMP6_NI_QUERY;
		/* code field is always 0 */
		/* XXX: overwrite icmp6_id */
		((struct icmp6_nodeinfo *)icp)->ni_qtype = htons(NI_QTYPE_FQDN);
		((struct icmp6_nodeinfo *)icp)->ni_flags = htons(0);
		if (timing)
			(void)gettimeofday((struct timeval *)
					   &outpack[ICMP6ECHOLEN], NULL);
		cc = ICMP6_NIQLEN;
		datalen = 0;
	} else if (options & F_NODEADDR) {
		icp->icmp6_type = ICMP6_NI_QUERY;
		icp->icmp6_code = ICMP6_NI_SUBJ_IPV6;
		/* XXX: overwrite icmp6_id */
		((struct icmp6_nodeinfo *)icp)->ni_qtype =
		    htons(NI_QTYPE_NODEADDR);
		((struct icmp6_nodeinfo *)icp)->ni_flags = htons(0);
		if (timing)
			(void)gettimeofday((struct timeval *)
					   &outpack[ICMP6ECHOLEN], NULL);
		memcpy(&outpack[ICMP6_NIQLEN], &dst.sin6_addr,
		    sizeof(dst.sin6_addr));
		cc = ICMP6_NIQLEN + sizeof(dst.sin6_addr);
		datalen = 0;
		((struct icmp6_nodeinfo *)icp)->ni_flags = naflags;
	}
	else {
		icp->icmp6_type = ICMP6_ECHO_REQUEST;
		if (timing)
			(void)gettimeofday((struct timeval *)
					   &outpack[ICMP6ECHOLEN], NULL);
		cc = ICMP6ECHOLEN + datalen;
	}

	smsghdr.msg_name = (caddr_t)&dst;
	smsghdr.msg_namelen = sizeof(dst);
	memset(&iov, 0, sizeof(iov));
	iov[0].iov_base = (caddr_t)outpack;
	iov[0].iov_len = cc;
	smsghdr.msg_iov = iov;
	smsghdr.msg_iovlen = 1;

	i = sendmsg(s, &smsghdr, 0);

	if (i < 0 || i != cc)  {
		if (i < 0)
			warn("sendmsg");
		(void)printf("ping6: wrote %s %d chars, ret=%d\n",
		    hostname, cc, i);
	}
	if (!(options & F_QUIET) && options & F_FLOOD)
		(void)write(STDOUT_FILENO, &DOT, 1);
}

/*
 * pr_pack --
 *	Print out the packet, if it came from us.  This logic is necessary
 * because ALL readers of the ICMP socket get a copy of ALL ICMP packets
 * which arrive ('tis only fair).  This permits multiple copies of this
 * program to be run without having intermingled output (or statistics!).
 */
void
pr_pack(buf, cc, mhdr)
	u_char *buf;
	int cc;
	struct msghdr *mhdr;
{
#define safeputc(c)	printf((isprint((c)) ? "%c" : "\\%03o"), c)
	struct icmp6_hdr *icp;
	int i;
	int hoplim;
	struct sockaddr_in6 *from = (struct sockaddr_in6 *)mhdr->msg_name;
	u_char *cp = NULL, *dp, *end = buf + cc;
	struct in6_pktinfo *pktinfo = NULL;
	struct timeval tv, *tp;
	double triptime = 0;
	int dupflag;
	size_t off;
	int oldfqdn;

	(void)gettimeofday(&tv, NULL);

	if (cc < sizeof(struct icmp6_hdr)) {
		if (options & F_VERBOSE)
			warnx("packet too short (%d bytes) from %s\n", cc,
			    pr_addr(from));
		return;
	}
	icp = (struct icmp6_hdr *)buf;
	off = 0;

	if ((hoplim = get_hoplim(mhdr)) == -1) {
		warnx("failed to get receiving hop limit");
		return;
	}
	if ((pktinfo = get_rcvpktinfo(mhdr)) == NULL) {
		warnx("failed to get receiving pakcet information");
		return;
	}

	if (icp->icmp6_type == ICMP6_ECHO_REPLY) {
		/* XXX the following line overwrites the original packet */
		icp->icmp6_seq = ntohs(icp->icmp6_seq);
		if (ntohs(icp->icmp6_id) != ident)
			return;			/* It was not our ECHO */
		++nreceived;
		if (timing) {
			tp = (struct timeval *)(icp + 1);
			tvsub(&tv, tp);
			triptime = ((double)tv.tv_sec) * 1000.0 +
			    ((double)tv.tv_usec) / 1000.0;
			tsum += triptime;
			if (triptime < tmin)
				tmin = triptime;
			if (triptime > tmax)
				tmax = triptime;
		}

		if (TST(icp->icmp6_seq % mx_dup_ck)) {
			++nrepeats;
			--nreceived;
			dupflag = 1;
		} else {
			SET(icp->icmp6_seq % mx_dup_ck);
			dupflag = 0;
		}

		if (options & F_QUIET)
			return;

		if (options & F_FLOOD)
			(void)write(STDOUT_FILENO, &BSPACE, 1);
		else {
			(void)printf("%d bytes from %s, icmp_seq=%u", cc,
				     pr_addr(from),
				     icp->icmp6_seq);
			(void)printf(" hlim=%d", hoplim);
			if ((options & F_VERBOSE) != 0) {
				struct sockaddr_in6 dstsa;

				memset(&dstsa, 0, sizeof(dstsa));
				dstsa.sin6_family = AF_INET6;
				dstsa.sin6_len = sizeof(dstsa);
				dstsa.sin6_scope_id = pktinfo->ipi6_ifindex;
				dstsa.sin6_addr = pktinfo->ipi6_addr;
				(void)printf(" dst=%s", pr_addr(&dstsa));
			}
			if (timing)
				(void)printf(" time=%g ms", triptime);
			if (dupflag)
				(void)printf("(DUP!)");
			/* check the data */
			cp = buf + off + ICMP6ECHOLEN + ICMP6ECHOTMLEN;
			dp = outpack + ICMP6ECHOLEN + ICMP6ECHOTMLEN;
			for (i = 8; cp < end; ++i, ++cp, ++dp) {
				if (*cp != *dp) {
					(void)printf("\nwrong data byte #%d should be 0x%x but was 0x%x", i, *dp, *cp);
					break;
				}
			}
		}
	} else if (icp->icmp6_type == ICMP6_NI_REPLY) { /* ICMP6_NI_REPLY */
		struct icmp6_nodeinfo *ni = (struct icmp6_nodeinfo *)(buf + off);

		(void)printf("%d bytes from %s: ", cc,
			     pr_addr(from));

		switch(ntohs(ni->ni_qtype)) {
		 case NI_QTYPE_NOOP:
			 printf("NodeInfo NOOP");
			 break;
		 case NI_QTYPE_SUPTYPES:
			 printf("NodeInfo Supported Qtypes");
			 break;
		 case NI_QTYPE_NODEADDR:
			 pr_nodeaddr(ni, end - (u_char *)ni);
			 break;
		 case NI_QTYPE_FQDN:
		 default:	/* XXX: for backward compatibility */
			cp = (u_char *)ni + ICMP6_NIRLEN;
			if (buf[off + ICMP6_NIRLEN] ==
			    cc - off - ICMP6_NIRLEN - 1)
				oldfqdn = 1;
			else
				oldfqdn = 0;
			if (oldfqdn) {
				cp++;
				while (cp < end) {
					safeputc(*cp & 0xff);
					cp++;
				}
			} else {
				while (cp < end) {
					i = *cp++;
					if (i) {
						if (i > end - cp) {
							printf("???");
							break;
						}
						while (i-- && cp < end) {
							safeputc(*cp & 0xff);
							cp++;
						}
						if (cp + 1 < end && *cp)
							printf(".");
					} else {
						if (cp == end) {
							/* FQDN */
							printf(".");
						} else if (cp + 1 == end &&
							   *cp == '\0') {
							/* truncated */
						} else {
							/* invalid */
							printf("???");
						}
						break;
					}
				}
			}
			if (options & F_VERBOSE) {
				int32_t ttl;
				int comma = 0;

				(void)printf(" (");	/*)*/

				switch(ni->ni_code) {
				case ICMP6_NI_REFUSED:
					(void)printf("refused");
					comma++;
					break;
				case ICMP6_NI_UNKNOWN:
					(void)printf("unknwon qtype");
					comma++;
					break;
				}

				if ((end - (u_char *)ni) < ICMP6_NIRLEN) {
					/* case of refusion, unkown */
					goto fqdnend;
				}
				ttl = (int32_t)ntohl(*(u_long *)&buf[off+ICMP6ECHOLEN+8]);
				if (comma)
					printf(",");
				if (!(ni->ni_flags & NI_FQDN_FLAG_VALIDTTL))
					(void)printf("TTL=%d:meaningless",
						     (int)ttl);
				else {
					if (ttl < 0) {
						(void)printf("TTL=%d:invalid",
						    ttl);
					} else
						(void)printf("TTL=%d", ttl);
				}
				comma++;

				if (oldfqdn) {
					if (comma)
						printf(",");
					printf("03 draft");
					comma++;
				} else {
					cp = (u_char *)ni + ICMP6_NIRLEN;
					if (cp == end) {
						if (comma)
							printf(",");
						printf("no name");
						comma++;
					}
				}

				if (buf[off + ICMP6_NIRLEN] !=
				    cc - off - ICMP6_NIRLEN - 1 && oldfqdn) {
					if (comma)
						printf(",");
					(void)printf("invalid namelen:%d/%lu",
						     buf[off + ICMP6_NIRLEN],
						     (u_long)cc - off - ICMP6_NIRLEN - 1);
					comma++;
				}
				/*(*/
				putchar(')');
			}
		 fqdnend:
			;
		}
	} else {
		/* We've got something other than an ECHOREPLY */
		if (!(options & F_VERBOSE))
			return;
		(void)printf("%d bytes from %s: ", cc,
			     pr_addr(from));
		pr_icmph(icp, end);
	}

	if (!(options & F_FLOOD)) {
		(void)putchar('\n');
		if (options & F_VERBOSE)
			pr_exthdrs(mhdr);
		(void)fflush(stdout);
	}
#undef safeputc
}

void
pr_exthdrs(mhdr)
	struct msghdr *mhdr;
{
	struct cmsghdr *cm;

	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(mhdr); cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(mhdr, cm)) {
		if (cm->cmsg_level != IPPROTO_IPV6)
			continue;

		switch(cm->cmsg_type) {
		case IPV6_HOPOPTS:
			printf("  HbH Options: ");
			pr_ip6opt(CMSG_DATA(cm));
			break;
		case IPV6_DSTOPTS:
#ifdef IPV6_RTHDRDSTOPTS
		case IPV6_RTHDRDSTOPTS:
#endif
			printf("  Dst Options: ");
			pr_ip6opt(CMSG_DATA(cm));
			break;
		case IPV6_RTHDR:
			printf("  Routing: ");
			pr_rthdr(CMSG_DATA(cm));
			break;
		}
	}
}

#ifdef USE_RFC2292BIS
void
pr_ip6opt(void *extbuf)
{
	struct ip6_hbh *ext;
	int currentlen;
	u_int8_t type;
	size_t extlen, len;
	void *databuf;
	size_t offset;
	u_int16_t value2;
	u_int32_t value4;

	ext = (struct ip6_hbh *)extbuf;
	extlen = (ext->ip6h_len + 1) * 8;
	printf("nxt %u, len %u (%d bytes)\n", ext->ip6h_nxt,
	       ext->ip6h_len, extlen);

	currentlen = 0;
	while (1) {
		currentlen = inet6_opt_next(extbuf, extlen, currentlen,
					    &type, &len, &databuf);
		if (currentlen == -1)
			break;
		switch (type) {
		/*
		 * Note that inet6_opt_next automatically skips any padding
		 * optins.
		 */
		case IP6OPT_JUMBO:
			offset = 0;
			offset = inet6_opt_get_val(databuf, offset,
						   &value4, sizeof(value4));
			printf("    Jumbo Payload Opt: Length %u\n",
			       (unsigned int)ntohl(value4));
			break;
		case IP6OPT_ROUTER_ALERT:
			offset = 0;
			offset = inet6_opt_get_val(databuf, offset,
						   &value2, sizeof(value2));
			printf("    Router Alert Opt: Type %u\n",
			       ntohs(value2));
			break;
		default:
			printf("    Received Opt %u len %u\n", type, len);
			break;
		}
	}
	return;
}
#else  /* !USE_RFC2292BIS */
/* ARGSUSED */
void
pr_ip6opt(void *extbuf)
{
	putchar('\n');
	return;
}
#endif /* USE_RFC2292BIS */

#ifdef USE_RFC2292BIS
void
pr_rthdr(void *extbuf)
{
	struct in6_addr *in6;
	char ntopbuf[INET6_ADDRSTRLEN];
	struct ip6_rthdr *rh = (struct ip6_rthdr *)extbuf;
	int i, segments;

	/* print fixed part of the header */
	printf("nxt %u, len %u (%d bytes), type %u, ", rh->ip6r_nxt,
	       rh->ip6r_len, (rh->ip6r_len + 1) << 3, rh->ip6r_type);
	if ((segments = inet6_rth_segments(extbuf)) >= 0)
		printf("%d segments, ", segments);
	else
		printf("segments unknown, ");
	printf("%d left\n", rh->ip6r_segleft);

	for (i = 0; i < segments; i++) {
		in6 = inet6_rth_getaddr(extbuf, i);
		if (in6 == NULL)
			printf("   [%d]<NULL>\n", i);
		else
			printf("   [%d]%s\n", i,
			       inet_ntop(AF_INET6, in6,
					 ntopbuf, sizeof(ntopbuf)));
	}

	return;
	
}
#else  /* !USE_RFC2292BIS */
/* ARGSUSED */
void
pr_rthdr(void *extbuf)
{
	putchar('\n');
	return;
}
#endif /* USE_RFC2292BIS */


void
pr_nodeaddr(ni, nilen)
	struct icmp6_nodeinfo *ni; /* ni->qtype must be NODEADDR */
	int nilen;
{
	struct in6_addr *ia6 = (struct in6_addr *)(ni + 1);
	char ntop_buf[INET6_ADDRSTRLEN];

	nilen -= sizeof(struct icmp6_nodeinfo);

	if (options & F_VERBOSE) {
		switch(ni->ni_code) {
		 case ICMP6_NI_REFUSED:
			 (void)printf("refused");
			 break;
		 case ICMP6_NI_UNKNOWN:
			 (void)printf("unknown qtype");
			 break;
		}
		if (ni->ni_flags & NI_NODEADDR_FLAG_ALL)
			(void)printf(" truncated");
	}
	putchar('\n');
	if (nilen <= 0)
		printf("  no address\n");
	for (; nilen > 0; nilen -= sizeof(*ia6), ia6 += 1) {
		printf("  %s\n",
		       inet_ntop(AF_INET6, ia6, ntop_buf, sizeof(ntop_buf)));
	}
}

int
get_hoplim(mhdr)
	struct msghdr *mhdr;
{
	struct cmsghdr *cm;

	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(mhdr); cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(mhdr, cm)) {
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_HOPLIMIT &&
		    cm->cmsg_len == CMSG_LEN(sizeof(int)))
			return(*(int *)CMSG_DATA(cm));
	}

	return(-1);
}

struct in6_pktinfo *
get_rcvpktinfo(mhdr)
	struct msghdr *mhdr;
{
	struct cmsghdr *cm;

	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(mhdr); cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(mhdr, cm)) {
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_PKTINFO &&
		    cm->cmsg_len == CMSG_LEN(sizeof(struct in6_pktinfo)))
			return((struct in6_pktinfo *)CMSG_DATA(cm));
	}

	return(NULL);
}

/*
 * tvsub --
 *	Subtract 2 timeval structs:  out = out - in.  Out is assumed to
 * be >= in.
 */
void
tvsub(out, in)
	register struct timeval *out, *in;
{
	if ((out->tv_usec -= in->tv_usec) < 0) {
		--out->tv_sec;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}

/*
 * oninfo --
 *	SIGINFO handler.
 */
/* ARGSUSED */
void
oninfo(notused)
	int notused;
{
	summary();
}

/*
 * onint --
 *	SIGINT handler.
 */
/* ARGSUSED */
void
onint(notused)
	int notused;
{
	summary();

	(void)signal(SIGINT, SIG_DFL);
	(void)kill(getpid(), SIGINT);

	/* NOTREACHED */
	exit(1);
}

/*
 * summary --
 *	Print out statistics.
 */
void
summary()
{
	register int i;

	(void)printf("\n--- %s ping6 statistics ---\n", hostname);
	(void)printf("%ld packets transmitted, ", ntransmitted);
	(void)printf("%ld packets received, ", nreceived);
	if (nrepeats)
		(void)printf("+%ld duplicates, ", nrepeats);
	if (ntransmitted) {
		if (nreceived > ntransmitted)
			(void)printf("-- somebody's printing up packets!");
		else
			(void)printf("%d%% packet loss",
			    (int) (((ntransmitted - nreceived) * 100) /
			    ntransmitted));
	}
	(void)putchar('\n');
	if (nreceived && timing) {
		/* Only display average to microseconds */
		i = 1000.0 * tsum / (nreceived + nrepeats);
		(void)printf("round-trip min/avg/max = %g/%g/%g ms\n",
		    tmin, ((double)i) / 1000.0, tmax);
		(void)fflush(stdout);
	}
}

#ifdef notdef
static char *ttab[] = {
	"Echo Reply",		/* ip + seq + udata */
	"Dest Unreachable",	/* net, host, proto, port, frag, sr + IP */
	"Source Quench",	/* IP */
	"Redirect",		/* redirect type, gateway, + IP  */
	"Echo",
	"Time Exceeded",	/* transit, frag reassem + IP */
	"Parameter Problem",	/* pointer + IP */
	"Timestamp",		/* id + seq + three timestamps */
	"Timestamp Reply",	/* " */
	"Info Request",		/* id + sq */
	"Info Reply"		/* " */
};
#endif

/*
 * pr_icmph --
 *	Print a descriptive string about an ICMP header.
 */
void
pr_icmph(icp, end)
	struct icmp6_hdr *icp;
	u_char *end;
{
	char ntop_buf[INET6_ADDRSTRLEN];

	switch(icp->icmp6_type) {
	case ICMP6_DST_UNREACH:
		switch(icp->icmp6_code) {
		case ICMP6_DST_UNREACH_NOROUTE:
			(void)printf("No Route to Destination\n");
			break;
		case ICMP6_DST_UNREACH_ADMIN:
			(void)printf("Destination Administratively "
				     "Unreachable\n");
			break;
		case ICMP6_DST_UNREACH_BEYONDSCOPE:
			(void)printf("Destination Unreachable Beyond Scope\n");
			break;
		case ICMP6_DST_UNREACH_ADDR:
			(void)printf("Destination Host Unreachable\n");
			break;
		case ICMP6_DST_UNREACH_NOPORT:
			(void)printf("Destination Port Unreachable\n");
			break;
		default:
			(void)printf("Destination Unreachable, Bad Code: %d\n",
			    icp->icmp6_code);
			break;
		}
		/* Print returned IP header information */
		pr_retip((struct ip6_hdr *)(icp + 1), end);
		break;
	case ICMP6_PACKET_TOO_BIG:
		(void)printf("Packet too big mtu = %d\n",
			     (int)ntohl(icp->icmp6_mtu));
		pr_retip((struct ip6_hdr *)(icp + 1), end);
		break;
	case ICMP6_TIME_EXCEEDED:
		switch(icp->icmp6_code) {
		case ICMP6_TIME_EXCEED_TRANSIT:
			(void)printf("Time to live exceeded\n");
			break;
		case ICMP6_TIME_EXCEED_REASSEMBLY:
			(void)printf("Frag reassembly time exceeded\n");
			break;
		default:
			(void)printf("Time exceeded, Bad Code: %d\n",
			    icp->icmp6_code);
			break;
		}
		pr_retip((struct ip6_hdr *)(icp + 1), end);
		break;
	case ICMP6_PARAM_PROB:
		(void)printf("Parameter problem: ");
		switch(icp->icmp6_code) {
		 case ICMP6_PARAMPROB_HEADER:
			 (void)printf("Erroneous Header ");
			 break;
		 case ICMP6_PARAMPROB_NEXTHEADER:
			 (void)printf("Unknown Nextheader ");
			 break;
		 case ICMP6_PARAMPROB_OPTION:
			 (void)printf("Unrecognized Option ");
			 break;
		 default:
			 (void)printf("Bad code(%d) ", icp->icmp6_code);
			 break;
		}
		(void)printf("pointer = 0x%02x\n",
			     (int)ntohl(icp->icmp6_pptr));
		pr_retip((struct ip6_hdr *)(icp + 1), end);
		break;
	case ICMP6_ECHO_REQUEST:
		(void)printf("Echo Request");
		/* XXX ID + Seq + Data */
		break;
	case ICMP6_ECHO_REPLY:
		(void)printf("Echo Reply");
		/* XXX ID + Seq + Data */
		break;
	case ICMP6_MEMBERSHIP_QUERY:
		(void)printf("Listener Query");
		break;
	case ICMP6_MEMBERSHIP_REPORT:
		(void)printf("Listener Report");
		break;
	case ICMP6_MEMBERSHIP_REDUCTION:
		(void)printf("Listener Done");
		break;
	case ND_ROUTER_SOLICIT:
		(void)printf("Router Solicitation");
		break;
	case ND_ROUTER_ADVERT:
		(void)printf("Router Advertisement");
		break;
	case ND_NEIGHBOR_SOLICIT:
		(void)printf("Neighbor Solicitation");
		break;
	case ND_NEIGHBOR_ADVERT:
		(void)printf("Neighbor Advertisement");
		break;
	case ND_REDIRECT:
	{
		struct nd_redirect *red = (struct nd_redirect *)icp;

		(void)printf("Redirect\n");
		(void)printf("Destination: %s",
			     inet_ntop(AF_INET6, &red->nd_rd_dst,
				       ntop_buf, sizeof(ntop_buf)));
		(void)printf("New Target: %s",
			     inet_ntop(AF_INET6, &red->nd_rd_target,
				       ntop_buf, sizeof(ntop_buf)));
		break;
	}
	case ICMP6_NI_QUERY:
		(void)printf("Node Information Query");
		/* XXX ID + Seq + Data */
		break;
	case ICMP6_NI_REPLY:
		(void)printf("Node Information Reply");
		/* XXX ID + Seq + Data */
		break;
	default:
		(void)printf("Bad ICMP type: %d", icp->icmp6_type);
	}
}

/*
 * pr_iph --
 *	Print an IP6 header.
 */
void
pr_iph(ip6)
	struct ip6_hdr *ip6;
{
	u_int32_t flow = ip6->ip6_flow & IPV6_FLOWLABEL_MASK;
	u_int8_t tc;
	char ntop_buf[INET6_ADDRSTRLEN];

	tc = *(&ip6->ip6_vfc + 1); /* XXX */
	tc = (tc >> 4) & 0x0f;
	tc |= (ip6->ip6_vfc << 4);

	printf("Vr TC  Flow Plen Nxt Hlim\n");
	printf(" %1x %02x %05x %04x  %02x   %02x\n",
	       (ip6->ip6_vfc & IPV6_VERSION_MASK) >> 4, tc, (int)ntohl(flow),
	       ntohs(ip6->ip6_plen),
	       ip6->ip6_nxt, ip6->ip6_hlim);
	printf("%s->", inet_ntop(AF_INET6, &ip6->ip6_src,
				  ntop_buf, sizeof(ntop_buf)));
	printf("%s\n", inet_ntop(AF_INET6, &ip6->ip6_dst,
				 ntop_buf, sizeof(ntop_buf)));
}

/*
 * pr_addr --
 *	Return an ascii host address as a dotted quad and optionally with
 * a hostname.
 */
const char *
pr_addr(addr)
	struct sockaddr_in6 *addr;
{
	static char buf[MAXHOSTNAMELEN];
	int flag = 0;

	if ((options & F_HOSTNAME) == 0)
		flag |= NI_NUMERICHOST;
#ifdef KAME_SCOPEID
	flag |= NI_WITHSCOPEID;
#endif

	getnameinfo((struct sockaddr *)addr, addr->sin6_len, buf, sizeof(buf),
		NULL, 0, flag);

	return (buf);
}

/*
 * pr_retip --
 *	Dump some info on a returned (via ICMPv6) IPv6 packet.
 */
void
pr_retip(ip6, end)
	struct ip6_hdr *ip6;
	u_char *end;
{
	u_char *cp = (u_char *)ip6, nh;
	int hlen;

	if (end - (u_char *)ip6 < sizeof(*ip6)) {
		printf("IP6");
		goto trunc;
	}
	pr_iph(ip6);
	hlen = sizeof(*ip6);

	nh = ip6->ip6_nxt;
	cp += hlen;
	while (end - cp >= 8) {
		switch (nh) {
		 case IPPROTO_HOPOPTS:
			 printf("HBH ");
			 hlen = (((struct ip6_hbh *)cp)->ip6h_len+1) << 3;
			 nh = ((struct ip6_hbh *)cp)->ip6h_nxt;
			 break;
		 case IPPROTO_DSTOPTS:
			 printf("DSTOPT ");
			 hlen = (((struct ip6_dest *)cp)->ip6d_len+1) << 3;
			 nh = ((struct ip6_dest *)cp)->ip6d_nxt;
			 break;
		 case IPPROTO_FRAGMENT:
			 printf("FRAG ");
			 hlen = sizeof(struct ip6_frag);
			 nh = ((struct ip6_frag *)cp)->ip6f_nxt;
			 break;
		 case IPPROTO_ROUTING:
			 printf("RTHDR ");
			 hlen = (((struct ip6_rthdr *)cp)->ip6r_len+1) << 3;
			 nh = ((struct ip6_rthdr *)cp)->ip6r_nxt;
			 break;
#ifdef IPSEC
		 case IPPROTO_AH:
			 printf("AH ");
			 hlen = (((struct ah *)cp)->ah_len+2) << 2;
			 nh = ((struct ah *)cp)->ah_nxt;
			 break;
#endif
		 case IPPROTO_ICMPV6:
			 printf("ICMP6: type = %d, code = %d\n",
				*cp, *(cp + 1));
			 return;
		 case IPPROTO_ESP:
			 printf("ESP\n");
			 return;
		 case IPPROTO_TCP:
			 printf("TCP: from port %u, to port %u (decimal)\n",
				(*cp * 256 + *(cp + 1)),
				(*(cp + 2) * 256 + *(cp + 3)));
			 return;
		 case IPPROTO_UDP:
			 printf("UDP: from port %u, to port %u (decimal)\n",
				(*cp * 256 + *(cp + 1)),
				(*(cp + 2) * 256 + *(cp + 3)));
			 return;
		 default:
			 printf("Unknown Header(%d)\n", nh);
			 return;
		}

		if ((cp += hlen) >= end)
			goto trunc;
	}
	if (end - cp < 8)
		goto trunc;

	putchar('\n');
	return;

  trunc:
	printf("...\n");
	return;
}

void
fill(bp, patp)
	char *bp, *patp;
{
	register int ii, jj, kk;
	int pat[16];
	char *cp;

	for (cp = patp; *cp; cp++)
		if (!isxdigit(*cp))
			errx(1, "patterns must be specified as hex digits");
	ii = sscanf(patp,
	    "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
	    &pat[0], &pat[1], &pat[2], &pat[3], &pat[4], &pat[5], &pat[6],
	    &pat[7], &pat[8], &pat[9], &pat[10], &pat[11], &pat[12],
	    &pat[13], &pat[14], &pat[15]);

/* xxx */	
	if (ii > 0)
		for (kk = 0;
		    kk <= MAXDATALEN - (8 + sizeof(struct timeval) + ii);
		    kk += ii)
			for (jj = 0; jj < ii; ++jj)
				bp[jj + kk] = pat[jj];
	if (!(options & F_QUIET)) {
		(void)printf("PATTERN: 0x");
		for (jj = 0; jj < ii; ++jj)
			(void)printf("%02x", bp[jj] & 0xFF);
		(void)printf("\n");
	}
}

#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
int
setpolicy(so, policy)
	int so;
	char *policy;
{
	char *buf;

	if (policy == NULL)
		return 0;	/* ignore */

	buf = ipsec_set_policy(policy, strlen(policy));
	if (buf == NULL)
		errx(1, "%s", ipsec_strerror());
	if (setsockopt(s, IPPROTO_IPV6, IPV6_IPSEC_POLICY,
			buf, ipsec_get_policylen(buf)) < 0)
		warnx("Unable to set IPSec policy");
	free(buf);

	return 0;
}
#endif
#endif

char *
nigroup(name)
	char *name;
{
	char *p;
	MD5_CTX ctxt;
	u_int8_t digest[16];
	char l;
	char hbuf[NI_MAXHOST];
	struct in6_addr in6;

	p = name;
	while (p && *p && *p != '.')
		p++;
	if (p - name > 63)
		return NULL;	/*label too long*/
	l = p - name;

	/* generate 8 bytes of pseudo-random value. */
	bzero(&ctxt, sizeof(ctxt));
	MD5Init(&ctxt);
	MD5Update(&ctxt, &l, sizeof(l));
	MD5Update(&ctxt, name, p - name);
	MD5Final(digest, &ctxt);

	bzero(&in6, sizeof(in6));
	in6.s6_addr[0] = 0xff;
	in6.s6_addr[1] = 0x02;
	in6.s6_addr[11] = 0x02;
	bcopy(digest, &in6.s6_addr[12], 4);

	if (inet_ntop(AF_INET6, &in6, hbuf, sizeof(hbuf)) == NULL)
		return NULL;

	return strdup(hbuf);
}

void
usage()
{
	(void)fprintf(stderr,
"usage: ping6 [-dfHnNqvwW"
#ifdef IPV6_REACHCONF
		      "R"
#endif
#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
		      "] [-P policy"
#else
		      "AE"
#endif
#endif		
		      "] [-a [aAclsg]] [-b sockbufsiz] [-c count] \n\
             [-I interface] [-i wait] [-l preload] [-p pattern] [-S sourceaddr]\n\
             [-s packetsize] [-h hoplimit] [hops...] host\n");
	exit(1);
}
