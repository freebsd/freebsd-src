/*	$KAME: ping6.c,v 1.126 2001/05/17 03:39:08 itojun Exp $	*/

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
 * as IPV6_PKTINFO.  Some people object it (sin6_scope_id specifies *link*
 * while IPV6_PKTINFO specifies *interface*.  Link is defined as collection of
 * network attached to 1 or more interfaces)
 */

#include <sys/param.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdb.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__)
#include <math.h>
#endif
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
/* FQDN case, 64 bits of nonce + 32 bits ttl */
#define ICMP6_NIRLEN	(ICMP6ECHOLEN + 12)
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
#define F_SUPTYPES	0x80000
#define F_NOMINMTU	0x100000
#define F_NOUSERDATA	(F_NODEADDR | F_FQDN | F_FQDNOLD | F_SUPTYPES)
u_int options;

#define IN6LEN		sizeof(struct in6_addr)
#define SA6LEN		sizeof(struct sockaddr_in6)
#define DUMMY_PORT	10101

#define SIN6(s)	((struct sockaddr_in6 *)(s))

/*
 * MAX_DUP_CHK is the number of bits in received table, i.e. the maximum
 * number of received sequence numbers we can keep track of.  Change 128
 * to 8192 for complete accuracy...
 */
#define	MAX_DUP_CHK	(8 * 8192)
int mx_dup_ck = MAX_DUP_CHK;
char rcvd_tbl[MAX_DUP_CHK / 8];

struct addrinfo *res;
struct sockaddr_in6 dst;	/* who to ping6 */
struct sockaddr_in6 src;	/* src addr of this packet */
int datalen = DEFDATALEN;
int s;				/* socket file descriptor */
u_char outpack[MAXPACKETLEN];
char BSPACE = '\b';		/* characters written for flood */
char DOT = '.';
char *hostname;
int ident;			/* process id to identify our packets */
u_int8_t nonce[8];		/* nonce field for node information */
struct in6_addr srcaddr;
int hoplimit = -1;		/* hoplimit */
int pathmtu = 0;		/* path MTU for the destination.  0 = unspec. */

/* counters */
long npackets;			/* max packets to transmit */
long nreceived;			/* # of packets we got back */
long nrepeats;			/* number of duplicates */
long ntransmitted;		/* sequence # for outbound packets = #sent */
struct timeval interval = {1, 0}; /* interval between packets */

/* timing */
int timing;			/* flag to do timing */
double tmin = 999999999.0;	/* minimum round trip time */
double tmax = 0.0;		/* maximum round trip time */
double tsum = 0.0;		/* sum of all times, for doing average */
#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__)
double tsumsq = 0.0;		/* sum of all times squared, for std. dev. */
#endif

/* for node addresses */
u_short naflags;

/* for ancillary data(advanced API) */
struct msghdr smsghdr;
struct iovec smsgiov;
char *scmsg = 0;

volatile int signo;
volatile sig_atomic_t seenalrm;
volatile sig_atomic_t seenint;
#ifdef SIGINFO
volatile sig_atomic_t seeninfo;
#endif

int	 main __P((int, char *[]));
void	 fill __P((char *, char *));
int	 get_hoplim __P((struct msghdr *));
int	 get_pathmtu __P((struct msghdr *));
void	 set_pathmtu __P((int));
struct in6_pktinfo *get_rcvpktinfo __P((struct msghdr *));
void	 onsignal __P((int));
void	 retransmit __P((void));
void	 onint __P((int));
size_t	 pingerlen __P((void));
int	 pinger __P((void));
const char *pr_addr __P((struct sockaddr *, int));
void	 pr_icmph __P((struct icmp6_hdr *, u_char *));
void	 pr_iph __P((struct ip6_hdr *));
void	 pr_suptypes __P((struct icmp6_nodeinfo *, size_t));
void	 pr_nodeaddr __P((struct icmp6_nodeinfo *, int));
int	 myechoreply __P((const struct icmp6_hdr *));
int	 mynireply __P((const struct icmp6_nodeinfo *));
char *dnsdecode __P((const u_char **, const u_char *, const u_char *,
	u_char *, size_t));
void	 pr_pack __P((u_char *, int, struct msghdr *));
void	 pr_exthdrs __P((struct msghdr *));
void	 pr_ip6opt __P((void *));
void	 pr_rthdr __P((void *));
int	 pr_bitrange __P((u_int32_t, int, int));
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
	struct timeval timeout, *tv;
	struct addrinfo hints;
	fd_set *fdmaskp;
	int fdmasks;
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
	double intval;
	size_t rthlen;

	/* just to be sure */
	memset(&smsghdr, 0, sizeof(smsghdr));
	memset(&smsgiov, 0, sizeof(smsgiov));

	preload = 0;
	datap = &outpack[ICMP6ECHOLEN + ICMP6ECHOTMLEN];
#ifndef IPSEC
#define ADDOPTS
#else
#ifdef IPSEC_POLICY_IPSEC
#define ADDOPTS	"P:"
#else
#define ADDOPTS	"AE"
#endif /*IPSEC_POLICY_IPSEC*/
#endif
	while ((ch = getopt(argc, argv,
	    "a:b:c:dfHh:I:i:l:mnNp:qRS:s:tvwW" ADDOPTS)) != -1) {
#undef ADDOPTS
		switch (ch) {
		case 'a':
		{
			char *cp;

			options &= ~F_NOUSERDATA;
			options |= F_NODEADDR;
			for (cp = optarg; *cp != '\0'; cp++) {
				switch (*cp) {
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
#ifdef NI_NODEADDR_FLAG_ANYCAST
					naflags |= NI_NODEADDR_FLAG_ANYCAST;
					break;
#else
					errx(1,
"-a A is not supported on the platform");
					/*NOTREACHED*/
#endif
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
			intval = strtod(optarg, &e);
			if (*optarg == '\0' || *e != '\0')
				errx(1, "illegal timing interval %s", optarg);
			if (intval < 1 && getuid()) {
				errx(1, "%s: only root may use interval < 1s",
				    strerror(EPERM));
			}
			interval.tv_sec = (long)intval;
			interval.tv_usec =
			    (long)((intval - interval.tv_sec) * 1000000);
			if (interval.tv_sec < 0)
				errx(1, "illegal timing interval %s", optarg);
			/* less than 1/hz does not make sense */
			if (interval.tv_sec == 0 && interval.tv_usec < 10000) {
				warnx("too small interval, raised to 0.01");
				interval.tv_usec = 10000;
			}
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
		case 'm':
#ifdef IPV6_USE_MIN_MTU
			options |= F_NOMINMTU;
			break;
#else
			errx(1, "-%c is not supported on this platform", ch);
			/*NOTREACHED*/
#endif
		case 'n':
			options &= ~F_HOSTNAME;
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
			if (datalen > MAXDATALEN) {
				errx(1,
				    "datalen value too large, maximum is %d",
				    MAXDATALEN);
			}
			break;
		case 't':
			options &= ~F_NOUSERDATA;
			options |= F_SUPTYPES;
			break;
		case 'v':
			options |= F_VERBOSE;
			break;
		case 'w':
			options &= ~F_NOUSERDATA;
			options |= F_FQDN;
			break;
		case 'W':
			options &= ~F_NOUSERDATA;
			options |= F_FQDNOLD;
			break;
#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
		case 'P':
			options |= F_POLICY;
			if (!strncmp("in", optarg, 2)) {
				if ((policy_in = strdup(optarg)) == NULL)
					errx(1, "strdup");
			} else if (!strncmp("out", optarg, 3)) {
				if ((policy_out = strdup(optarg)) == NULL)
					errx(1, "strdup");
			} else
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
#ifdef IPV6_RECVRTHDR	/* 2292bis */
		rthlen = CMSG_SPACE(inet6_rth_space(IPV6_RTHDR_TYPE_0,
		    argc - 1));
#else  /* RFC2292 */
		rthlen = inet6_rthdr_space(IPV6_RTHDR_TYPE_0, argc - 1);
#endif
		if (rthlen == 0) {
			errx(1, "too many intermediate hops");
			/*NOTREACHED*/
		}
		ip6optlen += rthlen;
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

	if ((s = socket(res->ai_family, res->ai_socktype,
	    res->ai_protocol)) < 0)
		err(1, "socket");

	/*
	 * let the kerel pass extension headers of incoming packets,
	 * for privileged socket options
	 */
	if ((options & F_VERBOSE) != 0) {
		int opton = 1;

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
#else  /* old adv. API */
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

	/* revoke root privilege */
	seteuid(getuid());
	setuid(getuid());

	if (options & F_FLOOD && options & F_INTERVAL)
		errx(1, "-f and -i incompatible options");

	if ((options & F_NOUSERDATA) == 0) {
		if (datalen >= sizeof(struct timeval)) {
			/* we can time transfer */
			timing = 1;
		} else
			timing = 0;
		/* in F_VERBOSE case, we may get non-echoreply packets*/
		if (options & F_VERBOSE)
			packlen = 2048 + IP6LEN + ICMP6ECHOLEN + EXTRA;
		else
			packlen = datalen + IP6LEN + ICMP6ECHOLEN + EXTRA;
	} else {
		/* suppress timing for node information query */
		timing = 0;
		datalen = 2048;
		packlen = 2048 + IP6LEN + ICMP6ECHOLEN + EXTRA;
	}

	if (!(packet = (u_char *)malloc((u_int)packlen)))
		err(1, "Unable to allocate packet");
	if (!(options & F_PINGFILLED))
		for (i = ICMP6ECHOLEN; i < packlen; ++i)
			*datap++ = i;

	ident = getpid() & 0xFFFF;
#ifndef __OpenBSD__
	gettimeofday(&timeout, NULL);
	srand((unsigned int)(timeout.tv_sec ^ timeout.tv_usec ^ (long)ident));
	memset(nonce, 0, sizeof(nonce));
	for (i = 0; i < sizeof(nonce); i += sizeof(int))
		*((int *)&nonce[i]) = rand();
#else
	memset(nonce, 0, sizeof(nonce));
	for (i = 0; i < sizeof(nonce); i += sizeof(u_int32_t))
		*((u_int32_t *)&nonce[i]) = arc4random();
#endif

	hold = 1;

	if (options & F_SO_DEBUG)
		(void)setsockopt(s, SOL_SOCKET, SO_DEBUG, (char *)&hold,
		    sizeof(hold));
	optval = IPV6_DEFHLIM;
	if (IN6_IS_ADDR_MULTICAST(&dst.sin6_addr))
		if (setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
		    &optval, sizeof(optval)) == -1)
			err(1, "IPV6_MULTICAST_HOPS");
#ifdef IPV6_USE_MIN_MTU
	if ((options & F_NOMINMTU) == 0) {
		optval = 1;
		if (setsockopt(s, IPPROTO_IPV6, IPV6_USE_MIN_MTU,
		    &optval, sizeof(optval)) == -1)
			err(1, "setsockopt(IPV6_USE_MIN_MTU)");
	}
#ifdef IPV6_RECVPATHMTU
	else {
		optval = 1;
		if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVPATHMTU,
		    &optval, sizeof(optval)) == -1)
			err(1, "setsockopt(IPV6_RECVPATHMTU)");
	}
#endif /* IPV6_RECVPATHMTU */
#endif /* IPV6_USE_MIN_MTU */

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
		    (options & F_NODEADDR) || (options & F_SUPTYPES))
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

			if ((error = getaddrinfo(argv[hops], NULL, &hints,
			    &iaip)))
				errx(1, "%s", gai_strerror(error));
			if (SIN6(iaip->ai_addr)->sin6_family != AF_INET6)
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
			freeaddrinfo(iaip);
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
		    (void *)smsghdr.msg_control, smsghdr.msg_controllen))
			err(1, "UDP setsockopt(IPV6_PKTOPTIONS)");
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
		setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&hold,
		    sizeof(hold));
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

	printf("PING6(%lu=40+8+%lu bytes) ", (unsigned long)(40 + pingerlen()),
	    (unsigned long)(pingerlen() - 8));
	printf("%s --> ", pr_addr((struct sockaddr *)&src, sizeof(src)));
	printf("%s\n", pr_addr((struct sockaddr *)&dst, sizeof(dst)));

	while (preload--)		/* Fire off them quickies. */
		(void)pinger();

	(void)signal(SIGINT, onsignal);
#ifdef SIGINFO
	(void)signal(SIGINFO, onsignal);
#endif

	if ((options & F_FLOOD) == 0) {
		(void)signal(SIGALRM, onsignal);
		itimer.it_interval = interval;
		itimer.it_value = interval;
		(void)setitimer(ITIMER_REAL, &itimer, NULL);
		if (ntransmitted == 0)
			retransmit();
	}

	fdmasks = howmany(s + 1, NFDBITS) * sizeof(fd_mask);
	if ((fdmaskp = malloc(fdmasks)) == NULL)
		err(1, "malloc");

	signo = seenalrm = seenint = 0;
#ifdef SIGINFO
	seeninfo = 0;
#endif

	for (;;) {
		struct msghdr m;
		struct cmsghdr *cm;
		u_char buf[1024];
		struct iovec iov[2];

		/* signal handling */
		if (seenalrm) {
			retransmit();
			seenalrm = 0;
			continue;
		}
		if (seenint) {
			onint(SIGINT);
			seenint = 0;
			continue;
		}
#ifdef SIGINFO
		if (seeninfo) {
			summary();
			seeninfo = 0;
			continue;
		}
#endif

		if (options & F_FLOOD) {
			(void)pinger();
			timeout.tv_sec = 0;
			timeout.tv_usec = 10000;
			tv = &timeout;
		} else
			tv = NULL;
		memset(fdmaskp, 0, fdmasks);
		FD_SET(s, fdmaskp);
		cc = select(s + 1, fdmaskp, NULL, NULL, tv);
		if (cc < 0) {
			if (errno != EINTR) {
				warn("select");
				sleep(1);
			}
			continue;
		} else if (cc == 0)
			continue;

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

		cc = recvmsg(s, &m, 0);
		if (cc < 0) {
			if (errno != EINTR) {
				warn("recvmsg");
				sleep(1);
			}
			continue;
		} else if (cc == 0) {
			int mtu;

			/*
			 * receive control messages only. Process the
			 * exceptions (currently the only possiblity is
			 * a path MTU notification.)
			 */
			if ((mtu = get_pathmtu(&m)) > 0) {
				if ((options & F_VERBOSE) != 0) {
					printf("new path MTU (%d) is "
					    "notified\n", mtu);
				}
				set_pathmtu(mtu);
			}
			continue;
		} else {
			/*
			 * an ICMPv6 message (probably an echoreply) arrived.
			 */
			pr_pack(packet, cc, &m);
		}
		if (npackets && nreceived >= npackets)
			break;
	}
	summary();
	exit(nreceived == 0);
}

void
onsignal(sig)
	int sig;
{
	signo = sig;
	switch (sig) {
	case SIGALRM:
		seenalrm++;
		break;
	case SIGINT:
		seenint++;
		break;
#ifdef SIGINFO
	case SIGINFO:
		seeninfo++;
		break;
#endif
	}
}

/*
 * retransmit --
 *	This routine transmits another ping6.
 */
void
retransmit()
{
	struct itimerval itimer;

	if (pinger() == 0)
		return;

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
size_t
pingerlen()
{
	size_t l;

	if (options & F_FQDN)
		l = ICMP6_NIQLEN + sizeof(dst.sin6_addr);
	else if (options & F_FQDNOLD)
		l = ICMP6_NIQLEN;
	else if (options & F_NODEADDR)
		l = ICMP6_NIQLEN + sizeof(dst.sin6_addr);
	else if (options & F_SUPTYPES)
		l = ICMP6_NIQLEN;
	else
		l = ICMP6ECHOLEN + datalen;

	return l;
}

int
pinger()
{
	struct icmp6_hdr *icp;
	struct iovec iov[2];
	int i, cc;
	struct icmp6_nodeinfo *nip;
	int seq;

	if (npackets && ntransmitted >= npackets)
		return(-1);	/* no more transmission */

	icp = (struct icmp6_hdr *)outpack;
	nip = (struct icmp6_nodeinfo *)outpack;
	memset(icp, 0, sizeof(*icp));
	icp->icmp6_cksum = 0;
	seq = ntransmitted++;
	CLR(seq % mx_dup_ck);

	if (options & F_FQDN) {
		icp->icmp6_type = ICMP6_NI_QUERY;
		icp->icmp6_code = ICMP6_NI_SUBJ_IPV6;
		nip->ni_qtype = htons(NI_QTYPE_FQDN);
		nip->ni_flags = htons(0);

		memcpy(nip->icmp6_ni_nonce, nonce,
		    sizeof(nip->icmp6_ni_nonce));
		*(u_int16_t *)nip->icmp6_ni_nonce = ntohs(seq);

		memcpy(&outpack[ICMP6_NIQLEN], &dst.sin6_addr,
		    sizeof(dst.sin6_addr));
		cc = ICMP6_NIQLEN + sizeof(dst.sin6_addr);
		datalen = 0;
	} else if (options & F_FQDNOLD) {
		/* packet format in 03 draft - no Subject data on queries */
		icp->icmp6_type = ICMP6_NI_QUERY;
		icp->icmp6_code = 0;	/* code field is always 0 */
		nip->ni_qtype = htons(NI_QTYPE_FQDN);
		nip->ni_flags = htons(0);

		memcpy(nip->icmp6_ni_nonce, nonce,
		    sizeof(nip->icmp6_ni_nonce));
		*(u_int16_t *)nip->icmp6_ni_nonce = ntohs(seq);

		cc = ICMP6_NIQLEN;
		datalen = 0;
	} else if (options & F_NODEADDR) {
		icp->icmp6_type = ICMP6_NI_QUERY;
		icp->icmp6_code = ICMP6_NI_SUBJ_IPV6;
		nip->ni_qtype = htons(NI_QTYPE_NODEADDR);
		nip->ni_flags = naflags;

		memcpy(nip->icmp6_ni_nonce, nonce,
		    sizeof(nip->icmp6_ni_nonce));
		*(u_int16_t *)nip->icmp6_ni_nonce = ntohs(seq);

		memcpy(&outpack[ICMP6_NIQLEN], &dst.sin6_addr,
		    sizeof(dst.sin6_addr));
		cc = ICMP6_NIQLEN + sizeof(dst.sin6_addr);
		datalen = 0;
	} else if (options & F_SUPTYPES) {
		icp->icmp6_type = ICMP6_NI_QUERY;
		icp->icmp6_code = ICMP6_NI_SUBJ_FQDN;	/*empty*/
		nip->ni_qtype = htons(NI_QTYPE_SUPTYPES);
		/* we support compressed bitmap */
		nip->ni_flags = NI_SUPTYPE_FLAG_COMPRESS;

		memcpy(nip->icmp6_ni_nonce, nonce,
		    sizeof(nip->icmp6_ni_nonce));
		*(u_int16_t *)nip->icmp6_ni_nonce = ntohs(seq);
		cc = ICMP6_NIQLEN;
		datalen = 0;
	} else {
		icp->icmp6_type = ICMP6_ECHO_REQUEST;
		icp->icmp6_code = 0;
		icp->icmp6_id = htons(ident);
		icp->icmp6_seq = ntohs(seq);
		if (timing)
			(void)gettimeofday((struct timeval *)
					   &outpack[ICMP6ECHOLEN], NULL);
		cc = ICMP6ECHOLEN + datalen;
	}

#ifdef DIAGNOSTIC
	if (pingerlen() != cc)
		errx(1, "internal error; length mismatch");
#endif

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

	return(0);
}

int
myechoreply(icp)
	const struct icmp6_hdr *icp;
{
	if (ntohs(icp->icmp6_id) == ident)
		return 1;
	else
		return 0;
}

int
mynireply(nip)
	const struct icmp6_nodeinfo *nip;
{
	if (memcmp(nip->icmp6_ni_nonce + sizeof(u_int16_t),
	    nonce + sizeof(u_int16_t),
	    sizeof(nonce) - sizeof(u_int16_t)) == 0)
		return 1;
	else
		return 0;
}

char *
dnsdecode(sp, ep, base, buf, bufsiz)
	const u_char **sp;
	const u_char *ep;
	const u_char *base;	/*base for compressed name*/
	u_char *buf;
	size_t bufsiz;
{
	int i;
	const u_char *cp;
	char cresult[MAXDNAME + 1];
	const u_char *comp;
	int l;

	cp = *sp;
	*buf = '\0';

	if (cp >= ep)
		return NULL;
	while (cp < ep) {
		i = *cp;
		if (i == 0 || cp != *sp) {
			if (strlcat(buf, ".", bufsiz) >= bufsiz)
				return NULL;	/*result overrun*/
		}
		if (i == 0)
			break;
		cp++;

		if ((i & 0xc0) == 0xc0 && cp - base > (i & 0x3f)) {
			/* DNS compression */
			if (!base)
				return NULL;

			comp = base + (i & 0x3f);
			if (dnsdecode(&comp, cp, base, cresult,
			    sizeof(cresult)) == NULL)
				return NULL;
			if (strlcat(buf, cresult, bufsiz) >= bufsiz)
				return NULL;	/*result overrun*/
			break;
		} else if ((i & 0x3f) == i) {
			if (i > ep - cp)
				return NULL;	/*source overrun*/
			while (i-- > 0 && cp < ep) {
				l = snprintf(cresult, sizeof(cresult),
				    isprint(*cp) ? "%c" : "\\%03o", *cp & 0xff);
				if (l >= sizeof(cresult))
					return NULL;
				if (strlcat(buf, cresult, bufsiz) >= bufsiz)
					return NULL;	/*result overrun*/
				cp++;
			}
		} else
			return NULL;	/*invalid label*/
	}
	if (i != 0)
		return NULL;	/*not terminated*/
	cp++;
	*sp = cp;
	return buf;
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
	struct icmp6_nodeinfo *ni;
	int i;
	int hoplim;
	struct sockaddr *from;
	int fromlen;
	u_char *cp = NULL, *dp, *end = buf + cc;
	struct in6_pktinfo *pktinfo = NULL;
	struct timeval tv, *tp;
	double triptime = 0;
	int dupflag;
	size_t off;
	int oldfqdn;
	u_int16_t seq;
	char dnsname[MAXDNAME + 1];

	(void)gettimeofday(&tv, NULL);

	if (!mhdr || !mhdr->msg_name ||
	    mhdr->msg_namelen != sizeof(struct sockaddr_in6) ||
	    ((struct sockaddr *)mhdr->msg_name)->sa_family != AF_INET6) {
		if (options & F_VERBOSE)
			warnx("invalid peername\n");
		return;
	}
	from = (struct sockaddr *)mhdr->msg_name;
	fromlen = mhdr->msg_namelen;
	if (cc < sizeof(struct icmp6_hdr)) {
		if (options & F_VERBOSE)
			warnx("packet too short (%d bytes) from %s\n", cc,
			    pr_addr(from, fromlen));
		return;
	}
	icp = (struct icmp6_hdr *)buf;
	ni = (struct icmp6_nodeinfo *)buf;
	off = 0;

	if ((hoplim = get_hoplim(mhdr)) == -1) {
		warnx("failed to get receiving hop limit");
		return;
	}
	if ((pktinfo = get_rcvpktinfo(mhdr)) == NULL) {
		warnx("failed to get receiving pakcet information");
		return;
	}

	if (icp->icmp6_type == ICMP6_ECHO_REPLY && myechoreply(icp)) {
		seq = ntohs(icp->icmp6_seq);
		++nreceived;
		if (timing) {
			tp = (struct timeval *)(icp + 1);
			tvsub(&tv, tp);
			triptime = ((double)tv.tv_sec) * 1000.0 +
			    ((double)tv.tv_usec) / 1000.0;
			tsum += triptime;
#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__)
			tsumsq += triptime * triptime;
#endif
			if (triptime < tmin)
				tmin = triptime;
			if (triptime > tmax)
				tmax = triptime;
		}

		if (TST(seq % mx_dup_ck)) {
			++nrepeats;
			--nreceived;
			dupflag = 1;
		} else {
			SET(seq % mx_dup_ck);
			dupflag = 0;
		}

		if (options & F_QUIET)
			return;

		if (options & F_FLOOD)
			(void)write(STDOUT_FILENO, &BSPACE, 1);
		else {
			(void)printf("%d bytes from %s, icmp_seq=%u", cc,
			    pr_addr(from, fromlen), seq);
			(void)printf(" hlim=%d", hoplim);
			if ((options & F_VERBOSE) != 0) {
				struct sockaddr_in6 dstsa;

				memset(&dstsa, 0, sizeof(dstsa));
				dstsa.sin6_family = AF_INET6;
#ifdef SIN6_LEN
				dstsa.sin6_len = sizeof(dstsa);
#endif
				dstsa.sin6_scope_id = pktinfo->ipi6_ifindex;
				dstsa.sin6_addr = pktinfo->ipi6_addr;
				(void)printf(" dst=%s",
				    pr_addr((struct sockaddr *)&dstsa,
				    sizeof(dstsa)));
			}
			if (timing)
				(void)printf(" time=%.3f ms", triptime);
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
	} else if (icp->icmp6_type == ICMP6_NI_REPLY && mynireply(ni)) {
		seq = ntohs(*(u_int16_t *)ni->icmp6_ni_nonce);
		++nreceived;
		if (TST(seq % mx_dup_ck)) {
			++nrepeats;
			--nreceived;
			dupflag = 1;
		} else {
			SET(seq % mx_dup_ck);
			dupflag = 0;
		}

		if (options & F_QUIET)
			return;

		(void)printf("%d bytes from %s: ", cc, pr_addr(from, fromlen));

		switch (ntohs(ni->ni_code)) {
		case ICMP6_NI_SUCCESS:
			break;
		case ICMP6_NI_REFUSED:
			printf("refused, type 0x%x", ntohs(ni->ni_type));
			goto fqdnend;
		case ICMP6_NI_UNKNOWN:
			printf("unknown, type 0x%x", ntohs(ni->ni_type));
			goto fqdnend;
		default:
			printf("unknown code 0x%x, type 0x%x",
			    ntohs(ni->ni_code), ntohs(ni->ni_type));
			goto fqdnend;
		}

		switch (ntohs(ni->ni_qtype)) {
		case NI_QTYPE_NOOP:
			printf("NodeInfo NOOP");
			break;
		case NI_QTYPE_SUPTYPES:
			pr_suptypes(ni, end - (u_char *)ni);
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
				cp++;	/* skip length */
				while (cp < end) {
					safeputc(*cp & 0xff);
					cp++;
				}
			} else {
				i = 0;
				while (cp < end) {
					if (dnsdecode((const u_char **)&cp, end,
					    (const u_char *)(ni + 1), dnsname,
					    sizeof(dnsname)) == NULL) {
						printf("???");
						break;
					}
					/*
					 * name-lookup special handling for
					 * truncated name
					 */
					if (cp + 1 <= end && !*cp &&
					    strlen(dnsname) > 0) {
						dnsname[strlen(dnsname) - 1] = '\0';
						cp++;
					}
					printf("%s%s", i > 0 ? "," : "",
					    dnsname);
				}
			}
			if (options & F_VERBOSE) {
				int32_t ttl;
				int comma = 0;

				(void)printf(" (");	/*)*/

				switch (ni->ni_code) {
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
					/* case of refusion, unknown */
					/*(*/
					putchar(')');
					goto fqdnend;
				}
				ttl = (int32_t)ntohl(*(u_long *)&buf[off+ICMP6ECHOLEN+8]);
				if (comma)
					printf(",");
				if (!(ni->ni_flags & NI_FQDN_FLAG_VALIDTTL)) {
					(void)printf("TTL=%d:meaningless",
					    (int)ttl);
				} else {
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
		(void)printf("%d bytes from %s: ", cc, pr_addr(from, fromlen));
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

		switch (cm->cmsg_type) {
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
	printf("nxt %u, len %u (%lu bytes)\n", ext->ip6h_nxt,
	    (unsigned int)ext->ip6h_len, (unsigned long)extlen);

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
			    (u_int32_t)ntohl(value4));
			break;
		case IP6OPT_ROUTER_ALERT:
			offset = 0;
			offset = inet6_opt_get_val(databuf, offset,
						   &value2, sizeof(value2));
			printf("    Router Alert Opt: Type %u\n",
			    ntohs(value2));
			break;
		default:
			printf("    Received Opt %u len %lu\n",
			    type, (unsigned long)len);
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
		else {
			if (!inet_ntop(AF_INET6, in6, ntopbuf,
			    sizeof(ntopbuf)))
				strncpy(ntopbuf, "?", sizeof(ntopbuf));
			printf("   [%d]%s\n", i, ntopbuf);
		}
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

int
pr_bitrange(v, s, ii)
	u_int32_t v;
	int s;
	int ii;
{
	int off;
	int i;

	off = 0;
	while (off < 32) {
		/* shift till we have 0x01 */
		if ((v & 0x01) == 0) {
			if (ii > 1)
				printf("-%u", s + off - 1);
			ii = 0;
			switch (v & 0x0f) {
			case 0x00:
				v >>= 4;
				off += 4;
				continue;
			case 0x08:
				v >>= 3;
				off += 3;
				continue;
			case 0x04: case 0x0c:
				v >>= 2;
				off += 2;
				continue;
			default:
				v >>= 1;
				off += 1;
				continue;
			}
		}

		/* we have 0x01 with us */
		for (i = 0; i < 32 - off; i++) {
			if ((v & (0x01 << i)) == 0)
				break;
		}
		if (!ii)
			printf(" %u", s + off);
		ii += i;
		v >>= i; off += i;
	}
	return ii;
}

void
pr_suptypes(ni, nilen)
	struct icmp6_nodeinfo *ni; /* ni->qtype must be SUPTYPES */
	size_t nilen;
{
	size_t clen;
	u_int32_t v;
	const u_char *cp, *end;
	u_int16_t cur;
	struct cbit {
		u_int16_t words;	/*32bit count*/
		u_int16_t skip;
	} cbit;
#define MAXQTYPES	(1 << 16)
	size_t off;
	int b;

	cp = (u_char *)(ni + 1);
	end = ((u_char *)ni) + nilen;
	cur = 0;
	b = 0;

	printf("NodeInfo Supported Qtypes");
	if (options & F_VERBOSE) {
		if (ni->ni_flags & NI_SUPTYPE_FLAG_COMPRESS)
			printf(", compressed bitmap");
		else
			printf(", raw bitmap");
	}

	while (cp < end) {
		clen = (size_t)(end - cp);
		if ((ni->ni_flags & NI_SUPTYPE_FLAG_COMPRESS) == 0) {
			if (clen == 0 || clen > MAXQTYPES / 8 ||
			    clen % sizeof(v)) {
				printf("???");
				return;
			}
		} else {
			if (clen < sizeof(cbit) || clen % sizeof(v))
				return;
			memcpy(&cbit, cp, sizeof(cbit));
			if (sizeof(cbit) + ntohs(cbit.words) * sizeof(v) >
			    clen)
				return;
			cp += sizeof(cbit);
			clen = ntohs(cbit.words) * sizeof(v);
			if (cur + clen * 8 + (u_long)ntohs(cbit.skip) * 32 >
			    MAXQTYPES)
				return;
		}

		for (off = 0; off < clen; off += sizeof(v)) {
			memcpy(&v, cp + off, sizeof(v));
			v = (u_int32_t)ntohl(v);
			b = pr_bitrange(v, (int)(cur + off * 8), b);
		}
		/* flush the remaining bits */
		b = pr_bitrange(0, (int)(cur + off * 8), b);

		cp += clen;
		cur += clen * 8;
		if ((ni->ni_flags & NI_SUPTYPE_FLAG_COMPRESS) != 0)
			cur += ntohs(cbit.skip) * 32;
	}
}

void
pr_nodeaddr(ni, nilen)
	struct icmp6_nodeinfo *ni; /* ni->qtype must be NODEADDR */
	int nilen;
{
	u_char *cp = (u_char *)(ni + 1);
	char ntop_buf[INET6_ADDRSTRLEN];
	int withttl = 0;

	nilen -= sizeof(struct icmp6_nodeinfo);

	if (options & F_VERBOSE) {
		switch (ni->ni_code) {
		case ICMP6_NI_REFUSED:
			(void)printf("refused");
			break;
		case ICMP6_NI_UNKNOWN:
			(void)printf("unknown qtype");
			break;
		}
		if (ni->ni_flags & NI_NODEADDR_FLAG_TRUNCATE)
			(void)printf(" truncated");
	}
	putchar('\n');
	if (nilen <= 0)
		printf("  no address\n");

	/*
	 * In icmp-name-lookups 05 and later, TTL of each returned address
	 * is contained in the resposne. We try to detect the version
	 * by the length of the data, but note that the detection algorithm
	 * is incomplete. We assume the latest draft by default.
	 */
	if (nilen % (sizeof(u_int32_t) + sizeof(struct in6_addr)) == 0)
		withttl = 1;
	while (nilen > 0) {
		u_int32_t ttl;

		if (withttl) {
			/* XXX: alignment? */
			ttl = (u_int32_t)ntohl(*(u_int32_t *)cp);
			cp += sizeof(u_int32_t);
			nilen -= sizeof(u_int32_t);
		}

		if (inet_ntop(AF_INET6, cp, ntop_buf, sizeof(ntop_buf)) ==
		    NULL)
			strncpy(ntop_buf, "?", sizeof(ntop_buf));
		printf("  %s", ntop_buf);
		if (withttl) {
			if (ttl == 0xffffffff) {
				/*
				 * XXX: can this convention be applied to all
				 * type of TTL (i.e. non-ND TTL)?
				 */
				printf("(TTL=infty)");
			}
			else
				printf("(TTL=%u)", ttl);
		}
		putchar('\n');

		nilen -= sizeof(struct in6_addr);
		cp += sizeof(struct in6_addr);
	}
}

int
get_hoplim(mhdr)
	struct msghdr *mhdr;
{
	struct cmsghdr *cm;

	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(mhdr); cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(mhdr, cm)) {
		if (cm->cmsg_len == 0)
			return(-1);

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
		if (cm->cmsg_len == 0)
			return(NULL);

		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_PKTINFO &&
		    cm->cmsg_len == CMSG_LEN(sizeof(struct in6_pktinfo)))
			return((struct in6_pktinfo *)CMSG_DATA(cm));
	}

	return(NULL);
}

int
get_pathmtu(mhdr)
	struct msghdr *mhdr;
{
#ifdef IPV6_RECVPATHMTU
	struct cmsghdr *cm;
	struct ip6_mtuinfo *mtuctl = NULL;

	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(mhdr); cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(mhdr, cm)) {
		if (cm->cmsg_len == 0)
			return(0);

		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_PATHMTU &&
		    cm->cmsg_len == CMSG_LEN(sizeof(struct ip6_mtuinfo))) {
			mtuctl = (struct ip6_mtuinfo *)CMSG_DATA(cm);

			/*
			 * If the notified destination is different from
			 * the one we are pinging, just ignore the info.
			 * We check the scope ID only when both notified value
			 * and our own value have non-0 values, because we may
			 * have used the default scope zone ID for sending,
			 * in which case the scope ID value is 0.
			 */
			if (!IN6_ARE_ADDR_EQUAL(&mtuctl->ip6m_addr.sin6_addr,
						&dst.sin6_addr) ||
			    (mtuctl->ip6m_addr.sin6_scope_id &&
			     dst.sin6_scope_id &&
			     mtuctl->ip6m_addr.sin6_scope_id !=
			     dst.sin6_scope_id)) {
				if ((options & F_VERBOSE) != 0) {
					printf("path MTU for %s is notified. "
					       "(ignored)\n",
					   pr_addr((struct sockaddr *)&mtuctl->ip6m_addr,
					   sizeof(mtuctl->ip6m_addr)));
				}
				return(0);
			}

			/*
			 * Ignore an invalid MTU. XXX: can we just believe
			 * the kernel check?
			 */
			if (mtuctl->ip6m_mtu < IPV6_MMTU)
				return(0);

			/* notification for our destination. return the MTU. */
			return((int)mtuctl->ip6m_mtu);
		}
	}
#endif
	return(0);
}

void
set_pathmtu(mtu)
	int mtu;
{
#ifdef IPV6_USE_MTU
	static int firsttime = 1;
	struct cmsghdr *cm;

	if (firsttime) {
		int oldlen = smsghdr.msg_controllen;
		char *oldbuf = smsghdr.msg_control;

		/* XXX: We need to enlarge control message buffer */
		firsttime = 0;	/* prevent further enlargement */

		smsghdr.msg_controllen = oldlen + CMSG_SPACE(sizeof(int));
		if ((smsghdr.msg_control =
		     (char *)malloc(smsghdr.msg_controllen)) == NULL)
			err(1, "set_pathmtu: malloc");
		cm = (struct cmsghdr *)CMSG_FIRSTHDR(&smsghdr);
		cm->cmsg_len = CMSG_LEN(sizeof(int));
		cm->cmsg_level = IPPROTO_IPV6;
		cm->cmsg_type = IPV6_USE_MTU;

		cm = (struct cmsghdr *)CMSG_NXTHDR(&smsghdr, cm);
		if (oldlen)
			memcpy((void *)cm, (void *)oldbuf, oldlen);

		free(oldbuf);
	}

	/*
	 * look for a cmsgptr that points MTU structure.
	 * XXX: this procedure seems redundant at this moment, but we'd better
	 * keep the code generic enough for future extensions.
	 */
	for (cm = CMSG_FIRSTHDR(&smsghdr); cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(&smsghdr, cm)) {
		if (cm->cmsg_len == 0) /* XXX: paranoid check */
			errx(1, "set_pathmtu: internal error");

		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_USE_MTU &&
		    cm->cmsg_len == CMSG_LEN(sizeof(int)))
			break;
	}

	if (cm == NULL)
		errx(1, "set_pathmtu: internal error: no space for path MTU");

	*(int *)CMSG_DATA(cm) = mtu;
#endif
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
 * onint --
 *	SIGINT handler.
 */
/* ARGSUSED */
void
onint(notused)
	int notused;
{
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGALRM, SIG_IGN);

	summary();

	exit(nreceived == 0);
}

/*
 * summary --
 *	Print out statistics.
 */
void
summary()
{

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
		double num = nreceived + nrepeats;
		double avg = tsum / num;
#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__)
		double dev = sqrt(tsumsq / num - avg * avg);
		(void)printf(
		    "round-trip min/avg/max/std-dev = %.3f/%.3f/%.3f/%.3f ms\n",
		    tmin, avg, tmax, dev);
#else
		(void)printf(
		    "round-trip min/avg/max = %.3f/%.3f/%.3f ms\n",
		    tmin, avg, tmax);
#endif
		(void)fflush(stdout);
	}
	(void)fflush(stdout);
}

/*subject type*/
static char *niqcode[] = {
	"IPv6 address",
	"DNS label",	/*or empty*/
	"IPv4 address",
};

/*result code*/
static char *nircode[] = {
	"Success", "Refused", "Unknown",
};


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
	struct nd_redirect *red;
	struct icmp6_nodeinfo *ni;
	char dnsname[MAXDNAME + 1];
	const u_char *cp;
	size_t l;

	switch (icp->icmp6_type) {
	case ICMP6_DST_UNREACH:
		switch (icp->icmp6_code) {
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
		switch (icp->icmp6_code) {
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
		switch (icp->icmp6_code) {
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
		    (u_int32_t)ntohl(icp->icmp6_pptr));
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
		red = (struct nd_redirect *)icp;
		(void)printf("Redirect\n");
		if (!inet_ntop(AF_INET6, &red->nd_rd_dst, ntop_buf,
		    sizeof(ntop_buf)))
			strncpy(ntop_buf, "?", sizeof(ntop_buf));
		(void)printf("Destination: %s", ntop_buf);
		if (!inet_ntop(AF_INET6, &red->nd_rd_target, ntop_buf,
		    sizeof(ntop_buf)))
			strncpy(ntop_buf, "?", sizeof(ntop_buf));
		(void)printf(" New Target: %s", ntop_buf);
		break;
	case ICMP6_NI_QUERY:
		(void)printf("Node Information Query");
		/* XXX ID + Seq + Data */
		ni = (struct icmp6_nodeinfo *)icp;
		l = end - (u_char *)(ni + 1);
		printf(", ");
		switch (ntohs(ni->ni_qtype)) {
		case NI_QTYPE_NOOP:
			(void)printf("NOOP");
			break;
		case NI_QTYPE_SUPTYPES:
			(void)printf("Supported qtypes");
			break;
		case NI_QTYPE_FQDN:
			(void)printf("DNS name");
			break;
		case NI_QTYPE_NODEADDR:
			(void)printf("nodeaddr");
			break;
		case NI_QTYPE_IPV4ADDR:
			(void)printf("IPv4 nodeaddr");
			break;
		default:
			(void)printf("unknown qtype");
			break;
		}
		if (options & F_VERBOSE) {
			switch (ni->ni_code) {
			case ICMP6_NI_SUBJ_IPV6:
				if (l == sizeof(struct in6_addr) &&
				    inet_ntop(AF_INET6, ni + 1, ntop_buf,
				    sizeof(ntop_buf)) != NULL) {
					(void)printf(", subject=%s(%s)",
					    niqcode[ni->ni_code], ntop_buf);
				} else {
#if 1
					/* backward compat to -W */
					(void)printf(", oldfqdn");
#else
					(void)printf(", invalid");
#endif
				}
				break;
			case ICMP6_NI_SUBJ_FQDN:
				if (end == (u_char *)(ni + 1)) {
					(void)printf(", no subject");
					break;
				}
				printf(", subject=%s", niqcode[ni->ni_code]);
				cp = (const u_char *)(ni + 1);
				if (dnsdecode(&cp, end, NULL, dnsname,
				    sizeof(dnsname)) != NULL)
					printf("(%s)", dnsname);
				else
					printf("(invalid)");
				break;
			case ICMP6_NI_SUBJ_IPV4:
				if (l == sizeof(struct in_addr) &&
				    inet_ntop(AF_INET, ni + 1, ntop_buf,
				    sizeof(ntop_buf)) != NULL) {
					(void)printf(", subject=%s(%s)",
					    niqcode[ni->ni_code], ntop_buf);
				} else
					(void)printf(", invalid");
				break;
			default:
				(void)printf(", invalid");
				break;
			}
		}
		break;
	case ICMP6_NI_REPLY:
		(void)printf("Node Information Reply");
		/* XXX ID + Seq + Data */
		ni = (struct icmp6_nodeinfo *)icp;
		printf(", ");
		switch (ntohs(ni->ni_qtype)) {
		case NI_QTYPE_NOOP:
			(void)printf("NOOP");
			break;
		case NI_QTYPE_SUPTYPES:
			(void)printf("Supported qtypes");
			break;
		case NI_QTYPE_FQDN:
			(void)printf("DNS name");
			break;
		case NI_QTYPE_NODEADDR:
			(void)printf("nodeaddr");
			break;
		case NI_QTYPE_IPV4ADDR:
			(void)printf("IPv4 nodeaddr");
			break;
		default:
			(void)printf("unknown qtype");
			break;
		}
		if (options & F_VERBOSE) {
			if (ni->ni_code > sizeof(nircode) / sizeof(nircode[0]))
				printf(", invalid");
			else
				printf(", %s", nircode[ni->ni_code]);
		}
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
	    (ip6->ip6_vfc & IPV6_VERSION_MASK) >> 4, tc, (u_int32_t)ntohl(flow),
	    ntohs(ip6->ip6_plen), ip6->ip6_nxt, ip6->ip6_hlim);
	if (!inet_ntop(AF_INET6, &ip6->ip6_src, ntop_buf, sizeof(ntop_buf)))
		strncpy(ntop_buf, "?", sizeof(ntop_buf));
	printf("%s->", ntop_buf);
	if (!inet_ntop(AF_INET6, &ip6->ip6_dst, ntop_buf, sizeof(ntop_buf)))
		strncpy(ntop_buf, "?", sizeof(ntop_buf));
	printf("%s\n", ntop_buf);
}

/*
 * pr_addr --
 *	Return an ascii host address as a dotted quad and optionally with
 * a hostname.
 */
const char *
pr_addr(addr, addrlen)
	struct sockaddr *addr;
	int addrlen;
{
	static char buf[NI_MAXHOST];
	int flag;

#ifdef NI_WITHSCOPEID
	flag = NI_WITHSCOPEID;
#else
	flag = 0;
#endif
	if ((options & F_HOSTNAME) == 0)
		flag |= NI_NUMERICHOST;

	if (getnameinfo(addr, addrlen, buf, sizeof(buf), NULL, 0, flag) == 0)
		return (buf);
	else
		return "?";
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
	if (setsockopt(s, IPPROTO_IPV6, IPV6_IPSEC_POLICY, buf,
	    ipsec_get_policylen(buf)) < 0)
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
	unsigned char *q;
	MD5_CTX ctxt;
	u_int8_t digest[16];
	u_int8_t c;
	size_t l;
	char hbuf[NI_MAXHOST];
	struct in6_addr in6;

	p = strchr(name, '.');
	if (!p)
		p = name + strlen(name);
	l = p - name;
	if (l > 63 || l > sizeof(hbuf) - 1)
		return NULL;	/*label too long*/
	strncpy(hbuf, name, l);
	hbuf[(int)l] = '\0';

	for (q = name; *q; q++) {
		if (isupper(*q))
			*q = tolower(*q);
	}

	/* generate 8 bytes of pseudo-random value. */
	bzero(&ctxt, sizeof(ctxt));
	MD5Init(&ctxt);
	c = l & 0xff;
	MD5Update(&ctxt, &c, sizeof(c));
	MD5Update(&ctxt, name, l);
	MD5Final(digest, &ctxt);

	if (inet_pton(AF_INET6, "ff02::2:0000:0000", &in6) != 1)
		return NULL;	/*XXX*/
	bcopy(digest, &in6.s6_addr[12], 4);

	if (inet_ntop(AF_INET6, &in6, hbuf, sizeof(hbuf)) == NULL)
		return NULL;

	return strdup(hbuf);
}

void
usage()
{
	(void)fprintf(stderr,
	    "usage: ping6 [-dfH"
#ifdef IPV6_USE_MIN_MTU
	    "m"
#endif
	    "nNqtvwW"
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
	    "] [-a [aAclsg]] [-b sockbufsiz] [-c count] \n"
            "\t[-I interface] [-i wait] [-l preload] [-p pattern] "
	    "[-S sourceaddr]\n"
            "\t[-s packetsize] [-h hoplimit] [hops...] host\n");
	exit(1);
}
