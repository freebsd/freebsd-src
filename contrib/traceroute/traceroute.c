/*
 * Copyright (c) 1988, 1989, 1991, 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static const char copyright[] =
    "@(#) Copyright (c) 1988, 1989, 1991, 1994, 1995, 1996\n\
The Regents of the University of California.  All rights reserved.\n";
#if 0
static const char rcsid[] =
    "@(#)$Header: traceroute.c,v 1.43 96/09/27 20:08:10 leres Exp $ (LBL)";
#endif
static const char rcsid[] =
    "$FreeBSD$";
#endif

/*
 * traceroute host  - trace the route ip packets follow going to "host".
 *
 * Attempt to trace the route an ip packet would follow to some
 * internet host.  We find out intermediate hops by launching probe
 * packets with a small ttl (time to live) then listening for an
 * icmp "time exceeded" reply from a gateway.  We start our probes
 * with a ttl of one and increase by one until we get an icmp "port
 * unreachable" (which means we got to "host") or hit a max (which
 * defaults to net.inet.ip.ttl hops & can be changed with the -m flag).
 * Three probes (change with -q flag) are sent at each ttl setting and
 * a line is printed showing the ttl, address of the gateway and
 * round trip time of each probe.  If the probe answers come from
 * different gateways, the address of each responding system will
 * be printed.  If there is no response within a 5 sec. timeout
 * interval (changed with the -w flag), a "*" is printed for that
 * probe.
 *
 * Probe packets are UDP format.  We don't want the destination
 * host to process them so the destination port is set to an
 * unlikely value (if some clod on the destination is using that
 * value, it can be changed with the -p flag).
 *
 * A sample use might be:
 *
 *     [yak 71]% traceroute nis.nsf.net.
 *     traceroute to nis.nsf.net (35.1.1.48), 64 hops max, 56 byte packet
 *      1  helios.ee.lbl.gov (128.3.112.1)  19 ms  19 ms  0 ms
 *      2  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  39 ms  19 ms
 *      3  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  39 ms  19 ms
 *      4  ccngw-ner-cc.Berkeley.EDU (128.32.136.23)  39 ms  40 ms  39 ms
 *      5  ccn-nerif22.Berkeley.EDU (128.32.168.22)  39 ms  39 ms  39 ms
 *      6  128.32.197.4 (128.32.197.4)  40 ms  59 ms  59 ms
 *      7  131.119.2.5 (131.119.2.5)  59 ms  59 ms  59 ms
 *      8  129.140.70.13 (129.140.70.13)  99 ms  99 ms  80 ms
 *      9  129.140.71.6 (129.140.71.6)  139 ms  239 ms  319 ms
 *     10  129.140.81.7 (129.140.81.7)  220 ms  199 ms  199 ms
 *     11  nic.merit.edu (35.1.1.48)  239 ms  239 ms  239 ms
 *
 * Note that lines 2 & 3 are the same.  This is due to a buggy
 * kernel on the 2nd hop system -- lbl-csam.arpa -- that forwards
 * packets with a zero ttl.
 *
 * A more interesting example is:
 *
 *     [yak 72]% traceroute allspice.lcs.mit.edu.
 *     traceroute to allspice.lcs.mit.edu (18.26.0.115), 64 hops max
 *      1  helios.ee.lbl.gov (128.3.112.1)  0 ms  0 ms  0 ms
 *      2  lilac-dmc.Berkeley.EDU (128.32.216.1)  19 ms  19 ms  19 ms
 *      3  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  19 ms  19 ms
 *      4  ccngw-ner-cc.Berkeley.EDU (128.32.136.23)  19 ms  39 ms  39 ms
 *      5  ccn-nerif22.Berkeley.EDU (128.32.168.22)  20 ms  39 ms  39 ms
 *      6  128.32.197.4 (128.32.197.4)  59 ms  119 ms  39 ms
 *      7  131.119.2.5 (131.119.2.5)  59 ms  59 ms  39 ms
 *      8  129.140.70.13 (129.140.70.13)  80 ms  79 ms  99 ms
 *      9  129.140.71.6 (129.140.71.6)  139 ms  139 ms  159 ms
 *     10  129.140.81.7 (129.140.81.7)  199 ms  180 ms  300 ms
 *     11  129.140.72.17 (129.140.72.17)  300 ms  239 ms  239 ms
 *     12  * * *
 *     13  128.121.54.72 (128.121.54.72)  259 ms  499 ms  279 ms
 *     14  * * *
 *     15  * * *
 *     16  * * *
 *     17  * * *
 *     18  ALLSPICE.LCS.MIT.EDU (18.26.0.115)  339 ms  279 ms  279 ms
 *
 * (I start to see why I'm having so much trouble with mail to
 * MIT.)  Note that the gateways 12, 14, 15, 16 & 17 hops away
 * either don't send ICMP "time exceeded" messages or send them
 * with a ttl too small to reach us.  14 - 17 are running the
 * MIT C Gateway code that doesn't send "time exceeded"s.  God
 * only knows what's going on with 12.
 *
 * The silent gateway 12 in the above may be the result of a bug in
 * the 4.[23]BSD network code (and its derivatives):  4.x (x <= 3)
 * sends an unreachable message using whatever ttl remains in the
 * original datagram.  Since, for gateways, the remaining ttl is
 * zero, the icmp "time exceeded" is guaranteed to not make it back
 * to us.  The behavior of this bug is slightly more interesting
 * when it appears on the destination system:
 *
 *      1  helios.ee.lbl.gov (128.3.112.1)  0 ms  0 ms  0 ms
 *      2  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  19 ms  39 ms
 *      3  lilac-dmc.Berkeley.EDU (128.32.216.1)  19 ms  39 ms  19 ms
 *      4  ccngw-ner-cc.Berkeley.EDU (128.32.136.23)  39 ms  40 ms  19 ms
 *      5  ccn-nerif35.Berkeley.EDU (128.32.168.35)  39 ms  39 ms  39 ms
 *      6  csgw.Berkeley.EDU (128.32.133.254)  39 ms  59 ms  39 ms
 *      7  * * *
 *      8  * * *
 *      9  * * *
 *     10  * * *
 *     11  * * *
 *     12  * * *
 *     13  rip.Berkeley.EDU (128.32.131.22)  59 ms !  39 ms !  39 ms !
 *
 * Notice that there are 12 "gateways" (13 is the final
 * destination) and exactly the last half of them are "missing".
 * What's really happening is that rip (a Sun-3 running Sun OS3.5)
 * is using the ttl from our arriving datagram as the ttl in its
 * icmp reply.  So, the reply will time out on the return path
 * (with no notice sent to anyone since icmp's aren't sent for
 * icmp's) until we probe with a ttl that's at least twice the path
 * length.  I.e., rip is really only 7 hops away.  A reply that
 * returns with a ttl of 1 is a clue this problem exists.
 * Traceroute prints a "!" after the time if the ttl is <= 1.
 * Since vendors ship a lot of obsolete (DEC's Ultrix, Sun 3.x) or
 * non-standard (HPUX) software, expect to see this problem
 * frequently and/or take care picking the target host of your
 * probes.
 *
 * Other possible annotations after the time are !H, !N, !P (got a host,
 * network or protocol unreachable, respectively), !S or !F (source
 * route failed or fragmentation needed -- neither of these should
 * ever occur and the associated gateway is busted if you see one).  If
 * almost all the probes result in some kind of unreachable, traceroute
 * will give up and exit.
 *
 * Notes
 * -----
 * This program must be run by root or be setuid.  (I suggest that
 * you *don't* make it setuid -- casual use could result in a lot
 * of unnecessary traffic on our poor, congested nets.)
 *
 * This program requires a kernel mod that does not appear in any
 * system available from Berkeley:  A raw ip socket using proto
 * IPPROTO_RAW must interpret the data sent as an ip datagram (as
 * opposed to data to be wrapped in a ip datagram).  See the README
 * file that came with the source to this program for a description
 * of the mods I made to /sys/netinet/raw_ip.c.  Your mileage may
 * vary.  But, again, ANY 4.x (x < 4) BSD KERNEL WILL HAVE TO BE
 * MODIFIED TO RUN THIS PROGRAM.
 *
 * The udp port usage may appear bizarre (well, ok, it is bizarre).
 * The problem is that an icmp message only contains 8 bytes of
 * data from the original datagram.  8 bytes is the size of a udp
 * header so, if we want to associate replies with the original
 * datagram, the necessary information must be encoded into the
 * udp header (the ip id could be used but there's no way to
 * interlock with the kernel's assignment of ip id's and, anyway,
 * it would have taken a lot more kernel hacking to allow this
 * code to set the ip id).  So, to allow two or more users to
 * use traceroute simultaneously, we use this task's pid as the
 * source port (the high bit is set to move the port number out
 * of the "likely" range).  To keep track of which probe is being
 * replied to (so times and/or hop counts don't get confused by a
 * reply that was delayed in transit), we increment the destination
 * port number before each probe.
 *
 * Don't use this as a coding example.  I was trying to find a
 * routing problem and this code sort-of popped out after 48 hours
 * without sleep.  I was amazed it ever compiled, much less ran.
 *
 * I stole the idea for this program from Steve Deering.  Since
 * the first release, I've learned that had I attended the right
 * IETF working group meetings, I also could have stolen it from Guy
 * Almes or Matt Mathis.  I don't know (or care) who came up with
 * the idea first.  I envy the originators' perspicacity and I'm
 * glad they didn't keep the idea a secret.
 *
 * Tim Seaver, Ken Adelman and C. Philip Wood provided bug fixes and/or
 * enhancements to the original distribution.
 *
 * I've hacked up a round-trip-route version of this that works by
 * sending a loose-source-routed udp datagram through the destination
 * back to yourself.  Unfortunately, SO many gateways botch source
 * routing, the thing is almost worthless.  Maybe one day...
 *
 *  -- Van Jacobson (van@ee.lbl.gov)
 *     Tue Dec 20 03:50:13 PST 1988
 */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/socket.h>
#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif
#include <sys/time.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>

#include <arpa/inet.h>

#ifdef	IPSEC
#include <net/route.h>
#include <netinet6/ipsec.h>	/* XXX */
#endif	/* IPSEC */

#include <ctype.h>
#include <errno.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <memory.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gnuc.h"
#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

/* Maximum number of gateways (include room for one noop) */
#define NGATEWAYS ((int)((MAX_IPOPTLEN - IPOPT_MINOFF - 1) / sizeof(u_int32_t)))

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN	64
#endif

#define Fprintf (void)fprintf
#define Printf (void)printf

/* What a GRE packet header looks like */
struct grehdr {
	u_int16_t   flags;
	u_int16_t   proto;
	u_int16_t   length;	/* PPTP version of these fields */
	u_int16_t   callId;
};
#ifndef IPPROTO_GRE
#define IPPROTO_GRE	47
#endif

/* For GRE, we prepare what looks like a PPTP packet */
#define GRE_PPTP_PROTO	0x880b

/* Data section of the probe packet */
struct outdata {
	u_char seq;		/* sequence number of this packet */
	u_char ttl;		/* ttl packet left with */
	struct timeval tv;	/* time packet left */
	int    optlen;		/* length of ip options */
};

/* Descriptor structure for each outgoing protocol we support */
struct outproto {
	char	*name;		/* name of protocol */
	u_char	num;		/* IP protocol number */
	u_short	hdrlen;		/* max size of protocol header */
	u_short	port;		/* default base protocol-specific "port" */
	void	(*prepare)(struct outdata *);
				/* finish preparing an outgoing packet */
	int	(*check)(const u_char *, int);
				/* check an incoming packet */
};

u_char	packet[512];		/* last inbound (icmp) packet */

struct ip *outip;		/* last output ip packet */
u_char *outprot;		/* last output inner protocol packet */

/* loose source route gateway list (including room for final destination) */
u_int32_t gwlist[NGATEWAYS + 1];

int s;				/* receive (icmp) socket file descriptor */
int sndsock;			/* send (udp) socket file descriptor */

struct sockaddr whereto;	/* Who to try to reach */
int packlen;			/* total length of packet */
int protlen;			/* length of protocol part of packet */
int maxpacket = 32 * 1024;	/* max ip packet size */

char *prog;
char *source;
char *hostname;

int nprobes = 3;
int min_ttl = 1;
int max_ttl;
u_short ident;
u_short port;			/* protocol specific base "port" */

int options;			/* socket options */
int verbose;
int waittime = 5;		/* time to wait for response (in seconds) */
int nflag;			/* print addresses numerically */

extern int optind;
extern int opterr;
extern char *optarg;

/* Forwards */
double	deltaT(struct timeval *, struct timeval *);
u_short	in_cksum(u_short *, int);
char	*inetname(struct in_addr);
int	main(int, char **);
int	packet_ok(u_char *, int, struct sockaddr_in *, int);
char	*pr_type(u_char);
void	print(u_char *, int, struct sockaddr_in *);
char	*getaddr(u_int32_t *, char *);
char	*getsin(struct sockaddr_in *, char *);
char	*savestr(const char *);
#ifdef	IPSEC
int	setpolicy __P((int so, char *policy));
#endif
void	send_probe(int, int);
void	tvsub(struct timeval *, struct timeval *);
__dead	void usage(void);
int	wait_for_reply(int, struct sockaddr_in *, struct timeval *);

void	udp_prep(struct outdata *);
int	udp_check(const u_char *, int);
void	tcp_prep(struct outdata *);
int	tcp_check(const u_char *, int);
void	gre_prep(struct outdata *);
int	gre_check(const u_char *, int);
void	gen_prep(struct outdata *);
int	gen_check(const u_char *, int);
void	icmp_prep(struct outdata *);
int	icmp_check(const u_char *, int);

/* List of supported protocols. The first one is the default. The last
   one is the handler for generic protocols not explicitly listed. */
struct	outproto protos[] = {
	{
		"udp",
		IPPROTO_UDP,
		sizeof(struct udphdr),
		32768 + 666,
		udp_prep,
		udp_check
	},
	{
		"tcp",
		IPPROTO_TCP,
		sizeof(struct tcphdr),
		32768 + 666,
		tcp_prep,
		tcp_check
	},
	{
		"gre",
		IPPROTO_GRE,
		sizeof(struct grehdr),
		GRE_PPTP_PROTO,
		gre_prep,
		gre_check
	},
	{
		"icmp",
		IPPROTO_ICMP,
		sizeof(struct icmp),
		0,
		icmp_prep,
		icmp_check
	},
	{
		NULL,
		0,
		2 * sizeof(u_short),
		0,
		gen_prep,
		gen_check
	},
};
struct	outproto *proto = &protos[0];

int
main(int argc, char **argv)
{
	register int op, code;
	register char *cp;
	struct sockaddr_in from;
	register struct sockaddr_in *to = (struct sockaddr_in *)&whereto;
	int on = 1;
	register struct protoent *pe;
	register int ttl, probe, i;
	register int seq = 0;
	register int tos = 0;
	register int lsrr = 0;
	register int optlen = 0;
	int requestPort = -1;
	int sump = 0;
	int sockerrno;

	/*
	 * Do the setuid-required stuff first, then lose priveleges ASAP.
	 * Do error checking for these two calls where they appeared in
	 * the original code.
	 */
	cp = "icmp";
	pe = getprotobyname(cp);
	if (pe) {
		if ((s = socket(AF_INET, SOCK_RAW, pe->p_proto)) < 0)
			sockerrno = errno;
		else if ((sndsock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
			sockerrno = errno;
	}

	setuid(getuid());

#ifdef IPCTL_DEFTTL
	{
		int mib[4] = { CTL_NET, PF_INET, IPPROTO_IP, IPCTL_DEFTTL };
		size_t sz = sizeof(max_ttl);

		if (sysctl(mib, 4, &max_ttl, &sz, NULL, 0) == -1)
			err(1, "sysctl(net.inet.ip.ttl)");
	}
#else
	max_ttl = 30;
#endif

	if ((cp = strrchr(argv[0], '/')) != NULL)
		prog = cp + 1;
	else
		prog = argv[0];

	opterr = 0;
	while ((op = getopt(argc, argv, "Sdnrvg:M:m:P:p:q:s:t:w:")) != EOF)
		switch (op) {

		case 'S':
			sump = 1;
			break;
		case 'd':
			options |= SO_DEBUG;
			break;

		case 'g':
			if (lsrr >= NGATEWAYS) {
				Fprintf(stderr,
				    "%s: No more than %d gateways\n",
				    prog, NGATEWAYS);
				exit(1);
			}
			(void)getaddr(gwlist + lsrr, optarg);
			++lsrr;
			break;

		case 'M':
			min_ttl = atoi(optarg);
			if (min_ttl < 1 || min_ttl > 0xff) {
				Fprintf(stderr, "%s: invalid ttl value %s\n",
				    prog, optarg);
				exit(1);
			}
			break;

		case 'm':
			max_ttl = atoi(optarg);
			if (max_ttl < 1 || max_ttl > 0xff) {
				Fprintf(stderr, "%s: invalid ttl value %s\n",
				    prog, optarg);
				exit(1);
			}
			break;

		case 'n':
			++nflag;
			break;

		case 'P':
			for (i = 0; protos[i].name != NULL; i++) {
				if (strcasecmp(protos[i].name, optarg) == 0) {
					break;
				}
			}
			proto = &protos[i];
			if (proto->name == NULL) {	/* generic handler */
				struct protoent *pe;
				u_long pnum;
				char *eptr;

				/* Determine the IP protocol number */
				if ((pe = getprotobyname(optarg)) != NULL)
					pnum = pe->p_proto;
				else {
					pnum = strtoul(optarg, &eptr, 10);
					if (pnum > 0xff
					    || *optarg == '\0'
					    || *eptr != '\0') {
						Fprintf(stderr, "%s: unknown "
						    "protocol \"%s\"\n",
						    prog, optarg);
						exit(1);
					}
				}
				proto->num = pnum;
			}
			break;

		case 'p':
			requestPort = atoi(optarg);
			if (requestPort <= 0) {
				Fprintf(stderr, "%s: port must be > 0\n", prog);
				exit(1);
			}
			break;

		case 'q':
			nprobes = atoi(optarg);
			if (nprobes <= 0) {
				Fprintf(stderr, "%s: nprobes must be > 0\n",
				    prog);
				exit(1);
			}
			break;

		case 'r':
			options |= SO_DONTROUTE;
			break;

		case 's':
			/*
			 * set the ip source address of the outbound
			 * probe (e.g., on a multi-homed host).
			 */
			source = optarg;
			break;

		case 't':
			tos = atoi(optarg);
			if (tos < 0 || tos > 255) {
				Fprintf(stderr, "%s: tos must be 0 to 255\n",
				    prog);
				exit(1);
			}
			break;

		case 'v':
			++verbose;
			break;

		case 'w':
			waittime = atoi(optarg);
			if (waittime <= 1 || waittime >= 24L * 60 * 60) {
				Fprintf(stderr,
				    "%s: wait must be > 1 sec and < 1 day\n",
				    prog);
				exit(1);
			}
			break;

		default:
			usage();
		}

	/* Set requested port, if any, else default for this protocol */
	port = (requestPort != -1) ? requestPort : proto->port;

	/* Check min vs. max TTL */
	if (min_ttl > max_ttl) {
		Fprintf(stderr, "%s: min ttl must be <= max ttl\n", prog);
		exit(1);
	}

	/* Process destination and optional packet size */
	switch (argc - optind) {

	case 2:
		packlen = atoi(argv[optind + 1]);
		/* Fall thorugh */

	case 1:
		hostname = savestr(getsin(to, argv[optind]));
		break;

	default:
		usage();
	}

#ifdef HAVE_SETLINEBUF
	setlinebuf (stdout);
#else
	setvbuf(stdout, NULL, _IOLBF, 0);
#endif

	if (lsrr > 0)
		optlen = (lsrr + 1) * sizeof(gwlist[0]);
	i = sizeof(*outip) + proto->hdrlen + sizeof(struct outdata) + optlen;
	if (packlen == 0)
		packlen = i;			/* minimum sized packet */
	else if (i > packlen || packlen > maxpacket) {
		Fprintf(stderr, "%s: packet size must be %d <= s <= %d\n",
		    prog, i, maxpacket);
		exit(1);
	}
	protlen = packlen - sizeof(*outip) - optlen;

	outip = (struct ip *)malloc((unsigned)packlen);
	if (outip == NULL) {
		Fprintf(stderr, "%s: malloc: %s\n", prog, strerror(errno));
		exit(1);
	}
	memset((char *)outip, 0, packlen);

	outip->ip_v = IPVERSION;
	outip->ip_tos = tos;
#ifdef BYTESWAP_IP_LEN
	outip->ip_len = htons(packlen);
#else
	outip->ip_len = packlen;
#endif
	outip->ip_p = proto->num;
	outprot = (u_char *)(outip + 1);
#ifdef HAVE_RAW_OPTIONS
	if (lsrr > 0) {
		register u_char *optlist;

		optlist = (u_char *)outprot;
		(u_char *)outprot += optlen;

		/* final hop */
		gwlist[lsrr] = to->sin_addr.s_addr;

		outip->ip_dst.s_addr = gwlist[0];

		/* force 4 byte alignment */
		optlist[0] = IPOPT_NOP;
		/* loose source route option */
		optlist[1] = IPOPT_LSRR;
		i = lsrr * sizeof(gwlist[0]);
		optlist[2] = i + 3;
		/* Pointer to LSRR addresses */
		optlist[3] = IPOPT_MINOFF;
		memcpy(optlist + 4, gwlist + 1, i);
	} else
#endif
		outip->ip_dst = to->sin_addr;

	outip->ip_hl = ((u_char *)outprot - (u_char *)outip) >> 2;

	ident = (getpid() & 0xffff) | 0x8000;

	if (pe == NULL) {
		Fprintf(stderr, "%s: unknown protocol %s\n", prog, cp);
		exit(1);
	}
	if (s < 0) {
		errno = sockerrno;
		Fprintf(stderr, "%s: icmp socket: %s\n", prog, strerror(errno));
		exit(1);
	}
	if (options & SO_DEBUG)
		(void)setsockopt(s, SOL_SOCKET, SO_DEBUG, (char *)&on,
		    sizeof(on));
	if (options & SO_DONTROUTE)
		(void)setsockopt(s, SOL_SOCKET, SO_DONTROUTE, (char *)&on,
		    sizeof(on));

#if	defined(IPSEC) && defined(IPSEC_POLICY_IPSEC)
	if (setpolicy(s, "in bypass") < 0)
		errx(1, "%s", ipsec_strerror());

	if (setpolicy(s, "out bypass") < 0)
		errx(1, "%s", ipsec_strerror());
#endif	/* defined(IPSEC) && defined(IPSEC_POLICY_IPSEC) */

	if (sndsock < 0) {
		errno = sockerrno;
		Fprintf(stderr, "%s: raw socket: %s\n", prog, strerror(errno));
		exit(1);
	}

#if defined(IP_OPTIONS) && !defined(HAVE_RAW_OPTIONS)
	if (lsrr > 0) {
		u_char optlist[MAX_IPOPTLEN];

		cp = "ip";
		if ((pe = getprotobyname(cp)) == NULL) {
			Fprintf(stderr, "%s: unknown protocol %s\n", prog, cp);
			exit(1);
		}

		/* final hop */
		gwlist[lsrr] = to->sin_addr.s_addr;
		++lsrr;

		/* force 4 byte alignment */
		optlist[0] = IPOPT_NOP;
		/* loose source route option */
		optlist[1] = IPOPT_LSRR;
		i = lsrr * sizeof(gwlist[0]);
		optlist[2] = i + 3;
		/* Pointer to LSRR addresses */
		optlist[3] = IPOPT_MINOFF;
		memcpy(optlist + 4, gwlist, i);

		if ((setsockopt(sndsock, pe->p_proto, IP_OPTIONS, optlist,
		    i + sizeof(gwlist[0]))) < 0) {
			Fprintf(stderr, "%s: IP_OPTIONS: %s\n",
			    prog, strerror(errno));
			exit(1);
		    }
	}
#endif

#ifdef SO_SNDBUF
	if (setsockopt(sndsock, SOL_SOCKET, SO_SNDBUF, (char *)&packlen,
	    sizeof(packlen)) < 0) {
		Fprintf(stderr, "%s: SO_SNDBUF: %s\n", prog, strerror(errno));
		exit(1);
	}
#endif
#ifdef IP_HDRINCL
	if (setsockopt(sndsock, IPPROTO_IP, IP_HDRINCL, (char *)&on,
	    sizeof(on)) < 0) {
		Fprintf(stderr, "%s: IP_HDRINCL: %s\n", prog, strerror(errno));
		exit(1);
	}
#endif
	if (options & SO_DEBUG)
		(void)setsockopt(sndsock, SOL_SOCKET, SO_DEBUG, (char *)&on,
		    sizeof(on));
	if (options & SO_DONTROUTE)
		(void)setsockopt(sndsock, SOL_SOCKET, SO_DONTROUTE, (char *)&on,
		    sizeof(on));

	if (source != NULL) {
		source = savestr(getsin(&from, source));
		outip->ip_src = from.sin_addr;
#ifndef IP_HDRINCL
		if (bind(sndsock, (struct sockaddr *)&from, sizeof(from)) < 0) {
			Fprintf(stderr, "%s: bind: %s\n",
			    prog, strerror(errno));
			exit (1);
		}
#endif
	}

#if	defined(IPSEC) && defined(IPSEC_POLICY_IPSEC)
	if (setpolicy(sndsock, "in bypass") < 0)
		errx(1, "%s", ipsec_strerror());

	if (setpolicy(sndsock, "out bypass") < 0)
		errx(1, "%s", ipsec_strerror());
#endif	/* defined(IPSEC) && defined(IPSEC_POLICY_IPSEC) */

	Fprintf(stderr, "%s to %s (%s)",
	    prog, hostname, inet_ntoa(to->sin_addr));
	if (source)
		Fprintf(stderr, " from %s", source);
	Fprintf(stderr, ", %d hops max, %d byte packets\n", max_ttl, packlen);
	(void)fflush(stderr);

	for (ttl = min_ttl; ttl <= max_ttl; ++ttl) {
		u_int32_t lastaddr = 0;
		int got_there = 0;
		int unreachable = 0;
		int loss;

		Printf("%2d ", ttl);
		for (probe = 0, loss = 0; probe < nprobes; ++probe) {
			register int cc;
			struct timeval t1, t2;
			struct timezone tz;
			register struct ip *ip;
			struct outdata outdata;

			/* Prepare outgoing data */
			outdata.seq = ++seq;
			outdata.ttl = ttl;
			outdata.optlen = optlen;

			/* Avoid alignment problems by copying bytewise: */
			(void)gettimeofday(&t1, &tz);
			memcpy(&outdata.tv, &t1, sizeof(outdata.tv));

			/* Finalize and send packet */
			(*proto->prepare)(&outdata);
			send_probe(seq, ttl);

			/* Wait for a reply */
			while ((cc = wait_for_reply(s, &from, &t1)) != 0) {
				double T;
				int precis;

				(void)gettimeofday(&t2, &tz);
				i = packet_ok(packet, cc, &from, seq);
				/* Skip short packet */
				if (i == 0)
					continue;
				if (from.sin_addr.s_addr != lastaddr) {
					print(packet, cc, &from);
					lastaddr = from.sin_addr.s_addr;
				}
				T = deltaT(&t1, &t2);
#ifdef SANE_PRECISION
				if (T >= 1000.0)
					precis = 0;
				else if (T >= 100.0)
					precis = 1;
				else if (T >= 10.0)
					precis = 2;
				else
#endif
					precis = 3;
				Printf("  %.*f ms", precis, T);
				if (i == -2) {
#ifndef ARCHAIC
					ip = (struct ip *)packet;
					if (ip->ip_ttl <= 1)
						Printf(" !");
#endif

					++got_there;
					break;
				}

				/* time exceeded in transit */
				if (i == -1)
					break;
					
				code = i - 1;
				switch (code) {

				case ICMP_UNREACH_PORT:
#ifndef ARCHAIC
					ip = (struct ip *)packet;
					if (ip->ip_ttl <= 1)
						Printf(" !");
#endif
					++got_there;
					break;

				case ICMP_UNREACH_NET:
					++unreachable;
					Printf(" !N");
					break;

				case ICMP_UNREACH_HOST:
					++unreachable;
					Printf(" !H");
					break;

				case ICMP_UNREACH_PROTOCOL:
					++got_there;
					Printf(" !P");
					break;

				case ICMP_UNREACH_NEEDFRAG:
					++unreachable;
					Printf(" !F");
					break;

				case ICMP_UNREACH_SRCFAIL:
					++unreachable;
					Printf(" !S");
					break;

/* rfc1716 */
#ifndef ICMP_UNREACH_FILTER_PROHIB
#define ICMP_UNREACH_FILTER_PROHIB	13	/* admin prohibited filter */
#endif
				case ICMP_UNREACH_FILTER_PROHIB:
					++unreachable;
					Printf(" !X");
					break;

				default:
					++unreachable;
					Printf(" !<%d>", code);
					break;
				}
				break;
			}
			if (cc == 0) {
				loss++;
				Printf(" *");
			}
			(void)fflush(stdout);
		}
		if (sump) {
			Printf(" (%d%% loss)", (loss * 100) / nprobes);
		}
		putchar('\n');
		if (got_there ||
		    (unreachable > 0 && unreachable >= nprobes - 1))
			break;
	}
	exit(0);
}

int
wait_for_reply(register int sock, register struct sockaddr_in *fromp,
    register struct timeval *tp)
{
	fd_set *fdsp;
	size_t nfds;
	struct timeval now, wait;
	struct timezone tz;
	register int cc = 0;
	register int error;
	int fromlen = sizeof(*fromp);

	nfds = howmany(sock + 1, NFDBITS);
	if ((fdsp = malloc(nfds * sizeof(fd_mask))) == NULL)
		err(1, "malloc");
	memset(fdsp, 0, nfds * sizeof(fd_mask));
	FD_SET(sock, fdsp);

	wait.tv_sec = tp->tv_sec + waittime;
	wait.tv_usec = tp->tv_usec;
	(void)gettimeofday(&now, &tz);
	tvsub(&wait, &now);
	if (wait.tv_sec < 0) {
		wait.tv_sec = 0;
		wait.tv_usec = 1;
	}

	error = select(sock + 1, fdsp, NULL, NULL, &wait);
	if (error == -1 && errno == EINVAL) {
		Fprintf(stderr, "%s: botched select() args\n", prog);
		exit(1);
	}
	if (error > 0)
		cc = recvfrom(s, (char *)packet, sizeof(packet), 0,
			    (struct sockaddr *)fromp, &fromlen);

	free(fdsp);
	return(cc);
}

#if	defined(IPSEC) && defined(IPSEC_POLICY_IPSEC)
int
setpolicy(so, policy)
	int so;
	char *policy;
{
	char *buf;

	buf = ipsec_set_policy(policy, strlen(policy));
	if (buf == NULL) {
		warnx("%s", ipsec_strerror());
		return -1;
	}
	(void)setsockopt(so, IPPROTO_IP, IP_IPSEC_POLICY,
		buf, ipsec_get_policylen(buf));

	free(buf);

	return 0;
}
#endif

void
send_probe(int seq, int ttl)
{
	register int i;

	outip->ip_ttl = ttl;
	outip->ip_id = htons(ident + seq);

	i = sendto(sndsock, (char *)outip, packlen, 0, &whereto,
		   sizeof(whereto));
	if (i < 0 || i != packlen)  {
		if (i < 0)
			Fprintf(stderr, "%s: sendto: %s\n",
			    prog, strerror(errno));
		Printf("%s: wrote %s %d chars, ret=%d\n",
		    prog, hostname, packlen, i);
		(void)fflush(stdout);
	}
}

double
deltaT(struct timeval *t1p, struct timeval *t2p)
{
	register double dt;

	dt = (double)(t2p->tv_sec - t1p->tv_sec) * 1000.0 +
	     (double)(t2p->tv_usec - t1p->tv_usec) / 1000.0;
	return (dt);
}

/*
 * Convert an ICMP "type" field to a printable string.
 */
char *
pr_type(register u_char t)
{
	static char *ttab[] = {
	"Echo Reply",	"ICMP 1",	"ICMP 2",	"Dest Unreachable",
	"Source Quench", "Redirect",	"ICMP 6",	"ICMP 7",
	"Echo",		"ICMP 9",	"ICMP 10",	"Time Exceeded",
	"Param Problem", "Timestamp",	"Timestamp Reply", "Info Request",
	"Info Reply"
	};

	if (t > 16)
		return("OUT-OF-RANGE");

	return(ttab[t]);
}

int
packet_ok(register u_char *buf, int cc, register struct sockaddr_in *from,
    register int seq)
{
	register struct icmp *icp;
	register u_char type, code;
	register int hlen;
#ifndef ARCHAIC
	register struct ip *ip;

	ip = (struct ip *) buf;
	hlen = ip->ip_hl << 2;
	if (cc < hlen + ICMP_MINLEN) {
		if (verbose)
			Printf("packet too short (%d bytes) from %s\n", cc,
				inet_ntoa(from->sin_addr));
		return (0);
	}
	cc -= hlen;
	icp = (struct icmp *)(buf + hlen);
#else
	icp = (struct icmp *)buf;
#endif
	type = icp->icmp_type;
	code = icp->icmp_code;
	if (type == ICMP_ECHOREPLY
	    && proto->num == IPPROTO_ICMP
	    && (*proto->check)((u_char *)icp,seq))
		return -2;
	if ((type == ICMP_TIMXCEED && code == ICMP_TIMXCEED_INTRANS) ||
	    type == ICMP_UNREACH) {
		struct ip *hip;
		u_char *inner;

		hip = &icp->icmp_ip;
		hlen = hip->ip_hl << 2;
		inner = (u_char *)((u_char *)hip + hlen);
		if (hlen + 12 <= cc
		    && hip->ip_p == proto->num
		    && (*proto->check)(inner, seq))
			return (type == ICMP_TIMXCEED ? -1 : code + 1);
	}
#ifndef ARCHAIC
	if (verbose) {
		register int i;
		u_int32_t *lp = (u_int32_t *)&icp->icmp_ip;

		Printf("\n%d bytes from %s to ", cc, inet_ntoa(from->sin_addr));
		Printf("%s: icmp type %d (%s) code %d\n",
		    inet_ntoa(ip->ip_dst), type, pr_type(type), icp->icmp_code);
		for (i = 4; i < cc ; i += sizeof(*lp))
			Printf("%2d: x%8.8x\n", i, *lp++);
	}
#endif
	return(0);
}

void
icmp_prep(struct outdata *outdata)
{
	struct icmp *const icmpheader = (struct icmp *) outprot;

	icmpheader->icmp_type = ICMP_ECHO;
	icmpheader->icmp_id = htons(ident);
	icmpheader->icmp_seq = htons(outdata->seq);

	icmpheader->icmp_cksum = 0;
	icmpheader->icmp_cksum = in_cksum((u_short *)icmpheader,
	    packlen - (sizeof(*outip) + outdata->optlen));
	if (icmpheader->icmp_cksum == 0)
		icmpheader->icmp_cksum = 0xffff;

}

int
icmp_check(const u_char *data, int seq)
{
	struct icmp *const icmpheader = (struct icmp *) data;

	return (icmpheader->icmp_id == htons(ident)
	    && icmpheader->icmp_seq == htons(seq));
}

void
udp_prep(struct outdata *outdata)
{
	struct udphdr *const udp = (struct udphdr *) outprot;

	udp->uh_sport = htons(ident);
	udp->uh_dport = htons(port + outdata->seq);
	udp->uh_ulen = htons((u_short)protlen);
}

int
udp_check(const u_char *data, int seq)
{
	struct udphdr *const udp = (struct udphdr *) data;

	return (ntohs(udp->uh_sport) == ident
	    && ntohs(udp->uh_dport) == port + seq);
}

void
tcp_prep(struct outdata *outdata)
{
	struct tcphdr *const tcp = (struct tcphdr *) outprot;

	tcp->th_sport = htons(ident);
	tcp->th_dport = htons(port + outdata->seq);
	tcp->th_seq = (tcp->th_sport << 16) | tcp->th_dport;
	tcp->th_ack = 0;
	tcp->th_off = 5;
	tcp->th_flags = TH_SYN;
}

int
tcp_check(const u_char *data, int seq)
{
	struct tcphdr *const tcp = (struct tcphdr *) data;

	return (ntohs(tcp->th_sport) == ident
	    && ntohs(tcp->th_dport) == port + seq);
}

void
gre_prep(struct outdata *outdata)
{
	struct grehdr *const gre = (struct grehdr *) outprot;

	gre->flags = htons(0x2001);
	gre->proto = htons(port);
	gre->length = 0;
	gre->callId = htons(ident + outdata->seq);
}

int
gre_check(const u_char *data, int seq)
{
	struct grehdr *const gre = (struct grehdr *) data;

	return(ntohs(gre->proto) == port
	    && ntohs(gre->callId) == ident + seq);
}

void
gen_prep(struct outdata *outdata)
{
	u_int16_t *const ptr = (u_int16_t *) outprot;

	ptr[0] = htons(ident);
	ptr[1] = htons(port + outdata->seq);
}

int
gen_check(const u_char *data, int seq)
{
	u_int16_t *const ptr = (u_int16_t *) data;

	return(ntohs(ptr[0]) == ident
	    && ntohs(ptr[1]) == port + seq);
}

void
print(register u_char *buf, register int cc, register struct sockaddr_in *from)
{
	register struct ip *ip;
	register int hlen;

	ip = (struct ip *) buf;
	hlen = ip->ip_hl << 2;
	cc -= hlen;

	if (nflag)
		Printf(" %s", inet_ntoa(from->sin_addr));
	else
		Printf(" %s (%s)", inetname(from->sin_addr),
		    inet_ntoa(from->sin_addr));

	if (verbose)
		Printf(" %d bytes to %s", cc, inet_ntoa (ip->ip_dst));
}

/*
 * Checksum routine for Internet Protocol family headers (C Version)
 */
u_short
in_cksum(register u_short *addr, register int len)
{
	register int nleft = len;
	register u_short *w = addr;
	register u_short answer;
	register int sum = 0;

	/*
	 *  Our algorithm is simple, using a 32 bit accumulator (sum),
	 *  we add sequential 16 bit words to it, and at the end, fold
	 *  back all the carry bits from the top 16 bits into the lower
	 *  16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1)
		sum += *(u_char *)w;

	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return (answer);
}

/*
 * Subtract 2 timeval structs:  out = out - in.
 * Out is assumed to be within about LONG_MAX seconds of in.
 */
void
tvsub(register struct timeval *out, register struct timeval *in)
{

	if ((out->tv_usec -= in->tv_usec) < 0)   {
		--out->tv_sec;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}

/*
 * Construct an Internet address representation.
 * If the nflag has been supplied, give
 * numeric value, otherwise try for symbolic name.
 */
char *
inetname(struct in_addr in)
{
	register char *cp;
	register struct hostent *hp;
	static int first = 1;
	static char domain[MAXHOSTNAMELEN + 1], line[MAXHOSTNAMELEN + 1];

	if (first && !nflag) {
		first = 0;
		if (gethostname(domain, MAXHOSTNAMELEN) == 0 &&
		    (cp = strchr(domain, '.')) != NULL) {
			(void)strncpy(domain, cp + 1, sizeof(domain) - 1);
			domain[sizeof(domain) - 1] = '\0';
		} else
			domain[0] = '\0';
	}
	if (!nflag && in.s_addr != INADDR_ANY) {
		hp = gethostbyaddr((char *)&in, sizeof(in), AF_INET);
		if (hp != NULL) {
			if ((cp = strchr(hp->h_name, '.')) != NULL &&
			    strcmp(cp + 1, domain) == 0)
				*cp = '\0';
			(void)strncpy(line, hp->h_name, sizeof(line) - 1);
			line[sizeof(line) - 1] = '\0';
			return (line);
		}
	}
	return (inet_ntoa(in));
}

char *
getaddr(register u_int32_t *ap, register char *hostname)
{
	register struct hostent *hp;

	*ap = inet_addr(hostname);
	if ((int32_t)*ap != -1)
		return (hostname);

	hp = gethostbyname(hostname);
	if (hp == NULL) {
		Fprintf(stderr, "%s: unknown host %s\n", prog, hostname);
		exit(1);
	}
	if (hp->h_addrtype != AF_INET || hp->h_length != 4) {
		Fprintf(stderr, "%s: bad host %s\n", prog, hostname);
		exit(1);
	}
	memcpy((caddr_t)ap, hp->h_addr, hp->h_length);
	return (hp->h_name);
}

char *
getsin(register struct sockaddr_in *sin, register char *hostname)
{

	memset(sin, 0, sizeof(*sin));
	sin->sin_family = AF_INET;
	return (getaddr((u_int32_t *)&sin->sin_addr.s_addr, hostname));
}

char *
savestr(register const char *str)
{
	register char *cp;

	cp = strdup(str);
	if (cp == NULL) {
		Fprintf(stderr, "%s: strdup: %s\n", prog, strerror(errno));
		exit(1);
	}
	return (cp);
}

__dead void
usage(void)
{
	extern char version[];

	Fprintf(stderr, "Version %s\n", version);
	Fprintf(stderr, "Usage: %s [-Sdnrv] [-w wait] [-m max_ttl] [-M min_ttl] \
[-P proto]\n\t [-p port#] [-q nqueries] [-t tos] [-s src_addr] [-g gateway] \
\n\t host [data_size]\n", prog);
	exit(1);
}
