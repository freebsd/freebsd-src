/*
 * Copyright (c) 1988, 1989, 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000
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
 *     traceroute to nis.nsf.net (35.1.1.48), 64 hops max, 40 byte packets
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
 *     traceroute to allspice.lcs.mit.edu (18.26.0.115), 64 hops max, 40 byte packets
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
 * opposed to data to be wrapped in an ip datagram).  See the README
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
#include <sys/capsicum.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/sctp.h>
#include <netinet/sctp_header.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#include <arpa/inet.h>

#include <libcasper.h>
#include <casper/cap_net.h>

#ifdef	IPSEC
#include <net/route.h>
#include <netipsec/ipsec.h>	/* XXX */
#endif	/* IPSEC */

#include <ctype.h>
#include <capsicum_helpers.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <memory.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sysexits.h>
#include <assert.h>
#include <ifaddrs.h>

#include "traceroute.h"

/* Maximum number of gateways (include room for one noop) */
#define NGATEWAYS ((int)((MAX_IPOPTLEN - IPOPT_MINOFF - 1) / sizeof(uint32_t)))

/* What a GRE packet header looks like */
struct grehdr {
	uint16_t   flags;
	uint16_t   proto;
	uint16_t   length;	/* PPTP version of these fields */
	uint16_t   callId;
};

/* Data section of the probe packet */
struct outdata {
	u_char seq;		/* sequence number of this packet */
	u_char ttl;		/* ttl packet left with */
	struct timeval tv;	/* time packet left */
};

/* Descriptor structure for each outgoing protocol we support */
struct outproto {
	/* name of protocol */
	char const	*name;
	/* An ascii key for the bytes of the header */
	char const	*key;
	/* IP protocol number */
	u_char		 num;
	/* max size of protocol header */
	u_short		 hdrlen;
	/* default base protocol-specific "port" */
	u_short		 port;
	/* check an incoming packet */
	int		(*check)(struct context *ctx, u_char const *, int);
};

/* For GRE, we prepare what looks like a PPTP packet */
#define GRE_PPTP_PROTO	0x880b

struct ip const *hip = NULL;		/* Quoted IP header */
int hiplen = 0;

/* loose source route gateway list (including room for final destination) */
uint32_t gwlist[NGATEWAYS + 1];

unsigned packlen;		/* total length of packet */
int protlen;			/* length of protocol part of packet */
int minpacket;			/* min ip packet size */
int maxpacket = 32 * 1024;	/* max ip packet size */
int pmtu;			/* Path MTU Discovery (RFC1191) */

static const char devnull[] = "/dev/null";

int doipcksum = 1;		/* calculate ip checksums by default */
int optlen;			/* length of ip options */

/* Forwards */
u_short		 p_cksum(struct ip const *, u_short const *, int, int);
static int	 packet_ok(struct context *, int,
			   struct sockaddr_in *, int);
char const	*pr_type(u_char);
static void	print(struct context *, u_char const *, int,
		      struct sockaddr_in const *);
#ifdef	IPSEC
int		setpolicy(int so, char const *policy);
#endif
struct outproto *setproto(char const *);
int		str2val(const char *, const char *, int, int);
void		tvsub(struct timeval *, struct timeval const *);
void		pkt_compare(const u_char *, int, const u_char *, int);

static void	udp_prep(struct context *, struct outdata *);
static int	udp_check(struct context *, const u_char *, int);
static void	udplite_prep(struct context *, struct outdata *);
static int	udplite_check(struct context *, const u_char *, int);
static void	tcp_prep(struct context *, struct outdata *);
static int	tcp_check(struct context *, const u_char *, int);
static void	sctp_prep(struct context *, struct outdata *);
static int	sctp_check(struct context *, const u_char *, int);
static void	gre_prep(struct context *, struct outdata *);
static int	gre_check(struct context *, const u_char *, int);
static void	gen_prep(struct context *, struct outdata *);
static int	gen_check(struct context *, const u_char *, int);
static void	icmp_prep(struct context *, struct outdata *);
static int	icmp_check(struct context *, const u_char *, int);

/* List of supported protocols. The first one is the default. The last
   one is the handler for generic protocols not explicitly listed. */
struct	outproto protos[] = {
	{
		"udp",
		"spt dpt len sum",
		IPPROTO_UDP,
		sizeof(struct udphdr),
		32768 + 666,
		udp_check
	},
	{
		"udplite",
		"spt dpt cov sum",
		IPPROTO_UDPLITE,
		sizeof(struct udphdr),
		32768 + 666,
		udplite_check
	},
	{
		"tcp",
		"spt dpt seq     ack     xxflwin sum urp",
		IPPROTO_TCP,
		sizeof(struct tcphdr),
		32768 + 666,
		tcp_check
	},
	{
		"sctp",
		"spt dpt vtag    crc     tyfllen tyfllen ",
		IPPROTO_SCTP,
		sizeof(struct sctphdr),
		32768 + 666,
		sctp_check
	},
#ifdef XXX
	{
		"gre",
		"flg pro len clid",
		IPPROTO_GRE,
		sizeof(struct grehdr),
		GRE_PPTP_PROTO,
		gre_check
	},
#endif
	{
		"icmp",
		"typ cod sum ",
		IPPROTO_ICMP,
		sizeof(struct icmp),
		0,
		icmp_check
	},
	{
		NULL,
		"",
		0,
		2 * sizeof(u_short),
		0,
		gen_check
	},
};
struct	outproto *proto = NULL;

static struct outproto *
find_proto(int protocol_number) {
	for (size_t i = 0; i < sizeof(protos) / sizeof(*protos); ++i) {
		if (protos[i].num == protocol_number)
			return &protos[i];
	}

	return NULL;
}

const char *ip_hdr_key = "vhtslen id  off tlprsum srcip   dstip   opts";

int
traceroute4(struct context *ctx)
{
	int code;
	int i;
	int seq = 0;
#ifdef XXX
	int tos = 0, settos = 0;
	u_short off = 0;
#endif
	int lsrr = 0;
	int sump = 0;

	setlinebuf(stdout);

	if ((proto = find_proto(ctx->options->protocol)) == NULL)
		errx(EX_SOFTWARE, "unknown protocol");

	if (!doipcksum)
		warnx("Warning: ip checksums disabled");

	if (lsrr > 0)
		optlen = (lsrr + 1) * sizeof(gwlist[0]);

	// XXX: check packet size

	if (ctx->options->first_ttl > ctx->options->max_ttl)
		errx(EX_USAGE,
		     "first ttl (%d) may not be greater than max ttl (%d)",
		     ctx->options->first_ttl, ctx->options->max_ttl);

	if (ctx->options->first_ttl > 1)
		printf("Skipping %d intermediate hops\n",
		       ctx->options->first_ttl - 1);

	struct sockaddr_in const *destination4 = 
		(struct sockaddr_in const *)ctx->destination;
	assert(destination4->sin_family == AF_INET);

	if ((proto->num == IPPROTO_SCTP) && (packlen & 3U))
		errx(EX_USAGE, "packet length must be a multiple of 4");

#ifdef XXX
	if (settos)
		outip->ip_tos = tos;
	if (ctx->options->detect_ecn_bleaching) {
		outip->ip_tos &= ~IPTOS_ECN_MASK;
		outip->ip_tos |= IPTOS_ECN_ECT1;
	}
#endif

#if	defined(IPSEC) && defined(IPSEC_POLICY_IPSEC)
	if (setpolicy(ctx->rcvsock, "in bypass") < 0)
		errx(1, "%s", ipsec_strerror());

	if (setpolicy(ctx->rcvsock, "out bypass") < 0)
		errx(1, "%s", ipsec_strerror());
#endif	/* defined(IPSEC) && defined(IPSEC_POLICY_IPSEC) */

	int plen = ctx->options->packetlen;
	if (setsockopt(ctx->sendsock, SOL_SOCKET, SO_SNDBUF,
		       &plen, sizeof(plen)) < 0)
		err(EX_OSERR, "SO_SNDBUF");

#if	defined(IPSEC) && defined(IPSEC_POLICY_IPSEC)
	if (setpolicy(ctx->sendsock, "in bypass") < 0)
		errx(EX_OSERR, "%s", ipsec_strerror());

	if (setpolicy(ctx->sendsock, "out bypass") < 0)
		errx(EX_OSERR, "%s", ipsec_strerror());
#endif	/* defined(IPSEC) && defined(IPSEC_POLICY_IPSEC) */

	for (unsigned ttl = ctx->options->first_ttl;
	     ttl <= ctx->options->max_ttl;
	     ++ttl) {
		uint32_t lastaddr = 0;
		int gotlastaddr = 0;
		int got_there = 0;
		int sentfirst = 0;
		unsigned probe = 0, loss = 0, unreachable = 0;

		(void)printf("%2d ", ttl);
		for (probe = 0, loss = 0;
		     probe < ctx->options->nprobes;
		     ++probe) {
			int cc;
			struct timeval t1, t2;
			struct ip *ip;
			struct outdata outdata;
			struct iovec mvec;
			struct msghdr msg;
			struct sockaddr_in from;

			mvec.iov_base = ctx->packet;
			mvec.iov_len = sizeof(ctx->packet);

			memset(&msg, 0, sizeof(msg));
			msg.msg_name = &from;
			msg.msg_namelen = sizeof(from);
			msg.msg_iov = &mvec;
			msg.msg_iovlen = 1;

			if (sentfirst && ctx->options->pause_msecs > 0)
				usleep(ctx->options->pause_msecs * 1000);
			/* Prepare outgoing data */
			outdata.seq = ++seq;
			outdata.ttl = ttl;

			/* Avoid alignment problems by copying bytewise: */
			(void)gettimeofday(&t1, NULL);
			memcpy(&outdata.tv, &t1, sizeof(outdata.tv));

			/* Send packet */
			send_probe(ctx, seq, ttl);
			++sentfirst;

			/* Wait for a reply */
			while ((cc = wait_for_reply(ctx, &msg)) != 0) {
				double T;
				int precis;

				(void)gettimeofday(&t2, NULL);
				i = packet_ok(ctx, cc, &from, seq);
				/* Skip short packet */
				if (i == 0)
					continue;
				if (!gotlastaddr ||
				    from.sin_addr.s_addr != lastaddr) {
					if (gotlastaddr)
						printf("\n   ");
					print(ctx, ctx->packet, cc, &from);
					lastaddr = from.sin_addr.s_addr;
					++gotlastaddr;
				}
				T = deltaT(&t1, &t2);
				if (T >= 1000.0)
					precis = 0;
				else if (T >= 100.0)
					precis = 1;
				else if (T >= 10.0)
					precis = 2;
				else
					precis = 3;
				(void)printf("  %.*f ms", precis, T);
				if (ctx->options->detect_ecn_bleaching) {
					u_char ecn = hip->ip_tos & IPTOS_ECN_MASK;
					switch (ecn) {
					case IPTOS_ECN_ECT1:
						(void)printf(" (ecn=passed)");
						break;
					case IPTOS_ECN_NOTECT:
						(void)printf(" (ecn=bleached)");
						break;
					case IPTOS_ECN_CE:
						(void)printf(" (ecn=congested)");
						break;
					default:
						(void)printf(" (ecn=mangled)");
						break;
					}
				}
#ifdef XXX
				if (ctx->options->icmp_diff) {
					(void)printf("\n");
					(void)printf("%*.*s%s\n",
					    -(outip->ip_hl << 3),
					    outip->ip_hl << 3,
					    ip_hdr_key,
					    proto->key);
					pkt_compare((void *)outip, packlen,
					    (void *)hip, hiplen);
				}
#endif
				if (i == -2) {
					ip = (struct ip *)ctx->packet;
					if (ip->ip_ttl <= 1)
						(void)printf(" !");
					++got_there;
					break;
				}
				/* time exceeded in transit */
				if (i == -1)
					break;
				code = i - 1;
				switch (code) {

				case ICMP_UNREACH_PORT:
					ip = (struct ip *)ctx->packet;
					if (ip->ip_ttl <= 1)
						(void)printf(" !");
					++got_there;
					break;

				case ICMP_UNREACH_NET:
					++unreachable;
					(void)printf(" !N");
					break;

				case ICMP_UNREACH_HOST:
					++unreachable;
					(void)printf(" !H");
					break;

				case ICMP_UNREACH_PROTOCOL:
					++got_there;
					(void)printf(" !P");
					break;

				case ICMP_UNREACH_NEEDFRAG:
					++unreachable;
					(void)printf(" !F-%d", pmtu);
					break;

				case ICMP_UNREACH_SRCFAIL:
					++unreachable;
					(void)printf(" !S");
					break;

				case ICMP_UNREACH_NET_UNKNOWN:
					++unreachable;
					(void)printf(" !U");
					break;

				case ICMP_UNREACH_HOST_UNKNOWN:
					++unreachable;
					(void)printf(" !W");
					break;

				case ICMP_UNREACH_ISOLATED:
					++unreachable;
					(void)printf(" !I");
					break;

				case ICMP_UNREACH_NET_PROHIB:
					++unreachable;
					(void)printf(" !A");
					break;

				case ICMP_UNREACH_HOST_PROHIB:
					++unreachable;
					(void)printf(" !Z");
					break;

				case ICMP_UNREACH_TOSNET:
					++unreachable;
					(void)printf(" !Q");
					break;

				case ICMP_UNREACH_TOSHOST:
					++unreachable;
					(void)printf(" !T");
					break;

				case ICMP_UNREACH_FILTER_PROHIB:
					++unreachable;
					(void)printf(" !X");
					break;

				case ICMP_UNREACH_HOST_PRECEDENCE:
					++unreachable;
					(void)printf(" !V");
					break;

				case ICMP_UNREACH_PRECEDENCE_CUTOFF:
					++unreachable;
					(void)printf(" !C");
					break;

				default:
					++unreachable;
					(void)printf(" !<%d>", code);
					break;
				}
				break;
			}
			if (cc == 0) {
				loss++;
				(void)printf(" *");
			}
			(void)fflush(stdout);
		}
		if (sump) {
			(void)printf(" (%d%% loss)",
				     (loss * 100) / ctx->options->nprobes);
		}
		putchar('\n');
		if (got_there ||
		    (unreachable > 0
		     && unreachable >= ctx->options->nprobes - 1))
			break;
	}

	return (0);
}

#if	defined(IPSEC) && defined(IPSEC_POLICY_IPSEC)
int
setpolicy(int so, char const *policy)
{
	char *buf;

	buf = ipsec_set_policy(policy, strlen(policy));
	if (buf == NULL) {
		warnx("%s", ipsec_strerror());
		return (-1);
	}
	(void)setsockopt(so, IPPROTO_IP, IP_IPSEC_POLICY,
		buf, ipsec_get_policylen(buf));

	free(buf);

	return (0);
}
#endif

/*
 * Convert an ICMP "type" field to a printable string.
 */
const char *
pr_type(u_char t)
{
	static char const * const ttab[] = {
	"Echo Reply",	"ICMP 1",	"ICMP 2",	"Dest Unreachable",
	"Source Quench", "Redirect",	"ICMP 6",	"ICMP 7",
	"Echo",		"ICMP 9",	"ICMP 10",	"Time Exceeded",
	"Param Problem", "Timestamp",	"Timestamp Reply", "Info Request",
	"Info Reply"
	};
	static size_t const num_types = sizeof(ttab) / sizeof(*ttab);

	if (t > num_types)
		return ("OUT-OF-RANGE");

	return (ttab[t]);
}

static int
packet_ok(struct context *ctx, int cc,
	  struct sockaddr_in *from, int seq)
{
	struct icmp const *icp;
	u_char type, code;
	int hlen;
	struct ip const *ip;

	ip = (struct ip const *) ctx->packet;
	hlen = ip->ip_hl << 2;
	if (cc < hlen + ICMP_MINLEN) {
		if (ctx->options->verbose)
			(void)printf("packet too short (%d bytes) from %s\n",
				     cc, inet_ntoa(from->sin_addr));
		return (0);
	}
	cc -= hlen;
	icp = (struct icmp const *)(ctx->packet + hlen);
	type = icp->icmp_type;
	code = icp->icmp_code;
	/* Path MTU Discovery (RFC1191) */
	if (code != ICMP_UNREACH_NEEDFRAG)
		pmtu = 0;
	else {
		pmtu = ntohs(icp->icmp_nextmtu);
	}
	if (type == ICMP_ECHOREPLY
	    && proto->num == IPPROTO_ICMP
	    && (*proto->check)(ctx, (u_char const *)icp, (u_char)seq))
		return (-2);
	if ((type == ICMP_TIMXCEED && code == ICMP_TIMXCEED_INTRANS) ||
	    type == ICMP_UNREACH) {
		u_char const *inner;

		hip = &icp->icmp_ip;
		hiplen = ((u_char const *)icp + cc) - (u_char const *)hip;
		hlen = hip->ip_hl << 2;
		inner = (u_char const *)((u_char const *)hip + hlen);
		if (hlen + 16 <= cc
		    && hip->ip_p == proto->num
		    && (*proto->check)(ctx, inner, (u_char)seq))
			return (type == ICMP_TIMXCEED ? -1 : code + 1);
	}
	if (ctx->options->verbose) {
		int i;
		uint32_t const *lp = (uint32_t const *)&icp->icmp_ip;

		(void)printf("\n%d bytes from %s to ",
			     cc, inet_ntoa(from->sin_addr));
		(void)printf("%s: icmp type %d (%s) code %d\n",
		    inet_ntoa(ip->ip_dst), type, pr_type(type), icp->icmp_code);
		for (i = 4; i <= cc - ICMP_MINLEN; i += sizeof(*lp))
			(void)printf("%2d: %8.8x\n", i, ntohl(*lp++));
	}
	return (0);
}

int
icmp_check(struct context *ctx, const u_char *data, int seq)
{
	struct icmp *const icmpheader = (struct icmp *) data;

	return (icmpheader->icmp_id == htons(ctx->ident)
	    && icmpheader->icmp_seq == htons(seq));
}

int
udp_check(struct context *ctx, const u_char *data, int seq)
{
	struct udphdr *const udp = (struct udphdr *) data;

	uint16_t sport = ctx->ident;
	uint16_t dport = ctx->options->port;

	if (ctx->options->fixed_port)
		sport += seq;
	else
		dport += seq;

	return ntohs(udp->uh_sport) == sport && ntohs(udp->uh_dport) == dport;
}

int
udplite_check(struct context *ctx, const u_char *data, int seq)
{
	struct udphdr *const udp = (struct udphdr *) data;

	uint16_t sport = ctx->ident;
	uint16_t dport = ctx->options->port;

	if (ctx->options->fixed_port)
		sport += seq;
	else
		dport += seq;

	return ntohs(udp->uh_sport) == sport && ntohs(udp->uh_dport) == dport;
}

int
tcp_check(struct context *ctx, const u_char *data, int seq)
{
	struct tcphdr *const tcp = (struct tcphdr *) data;

	uint16_t dport = ctx->options->port;

	if (!ctx->options->fixed_port)
		dport += seq;

	return (ntohs(tcp->th_sport) == ctx->ident
	    && ntohs(tcp->th_dport) == dport
	    && tcp->th_seq == (tcp_seq)((tcp->th_sport << 16) | tcp->th_dport));
}

int
sctp_check(struct context *ctx, const u_char *data, int seq)
{
	struct sctphdr *const sctp = (struct sctphdr *) data;

	uint16_t dport = ctx->options->port;

	if (!ctx->options->fixed_port)
		dport += seq;

	if (ntohs(sctp->src_port) != ctx->ident ||
	    ntohs(sctp->dest_port) != dport)
		return (0);
	if (protlen < (int)(sizeof(struct sctphdr) +
	    sizeof(struct sctp_init_chunk))) {
		return (sctp->v_tag ==
		    (uint32_t)((sctp->src_port << 16) | sctp->dest_port));
	} else {
		/*
		 * Don't verify the initiate_tag, since it is not available,
		 * most of the time.
		 */
		return (sctp->v_tag == 0);
	}
}

#ifdef XXX
void
gre_prep(struct context *ctx, struct outdata *outdata)
{
	struct grehdr *const gre = (struct grehdr *) outp;

	gre->flags = htons(0x2001);
	gre->proto = htons(ctx->options->port);
	gre->length = 0;
	gre->callId = htons(ctx->ident + outdata->seq);
}
#endif

int
gre_check(struct context *ctx, const u_char *data, int seq)
{
	struct grehdr *const gre = (struct grehdr *) data;

	return (ntohs(gre->proto) == ctx->options->port
	    && ntohs(gre->callId) == ctx->ident + seq);
}

#ifdef XXX
void
gen_prep(struct context *ctx, struct outdata *outdata)
{
	uint16_t *const ptr = (uint16_t *) outp;

	ptr[0] = htons(ctx->ident);
	ptr[1] = htons(ctx->options->port + outdata->seq);
}
#endif

int
gen_check(struct context *ctx, const u_char *data, int seq)
{
	uint16_t *const ptr = (uint16_t *) data;

	return (ntohs(ptr[0]) == ctx->ident
	    && ntohs(ptr[1]) == ctx->options->port + seq);
}

static void
print(struct context *ctx, u_char const *buf, int cc,
      struct sockaddr_in const *from)
{
	struct ip const *ip;
	int hlen;
	char addr[INET_ADDRSTRLEN];

	ip = (struct ip const *) buf;
	hlen = ip->ip_hl << 2;
	cc -= hlen;

	strlcpy(addr, inet_ntoa(from->sin_addr), sizeof(addr));

	if (ctx->asndb)
		(void)printf(" [AS%u]", as_lookup(ctx->asndb, addr, AF_INET));

	print_hop(ctx, (struct sockaddr *)from);

	if (ctx->options->verbose)
		(void)printf(" %d bytes to %s", cc, inet_ntoa(ip->ip_dst));
}

/*
 * Checksum routine for UDP and TCP headers.
 */
u_short
p_cksum(struct ip const *ip, u_short const *data, int len, int cov)
{
	static struct ipovly ipo;
	u_short sum[2];

	ipo.ih_pr = ip->ip_p;
	ipo.ih_len = htons(len);
	ipo.ih_src = ip->ip_src;
	ipo.ih_dst = ip->ip_dst;

	sum[1] = in_cksum((uint8_t *)&ipo, sizeof(ipo)); /* pseudo ip hdr cksum */
	sum[0] = in_cksum((uint8_t *)data, cov);                    /* payload data cksum */

	return (~in_cksum((uint8_t *)sum, sizeof(sum)));
}

/*
 * Subtract 2 timeval structs:  out = out - in.
 * Out is assumed to be within about LONG_MAX seconds of in.
 */
void
tvsub(struct timeval *out, struct timeval const *in)
{

	if ((out->tv_usec -= in->tv_usec) < 0)   {
		--out->tv_sec;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}

/* String to value with optional min and max. Handles decimal and hex. */
int
str2val(const char *str, const char *what, int mi, int ma)
{
	const char *cp;
	int val;
	char *ep;

	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
		cp = str + 2;
		val = (int)strtol(cp, &ep, 16);
	} else
		val = (int)strtol(str, &ep, 10);

	if (*ep != '\0')
		errx(EX_USAGE, "\"%s\" bad value for %s", str, what);

	if (val < mi && mi >= 0) {
		if (mi == 0)
			errx(EX_USAGE, "%s must be >= %d", what, mi);
		else
			errx(EX_USAGE, "%s must be > %d", what, mi);
	}
	if (val > ma && ma >= 0)
		errx(EX_USAGE, "%s must be <= %d", what, mi);
	return (val);
}

struct outproto *
setproto(char const *pname)
{
	struct outproto *proto;
	int i;

	for (i = 0; protos[i].name != NULL; i++) {
		if (strcasecmp(protos[i].name, pname) == 0) {
			break;
		}
	}
	proto = &protos[i];
	if (proto->name == NULL) {	/* generic handler */
		struct protoent *pe;
		u_long pnum;

		/* Determine the IP protocol number */
		if ((pe = getprotobyname(pname)) != NULL)
			pnum = pe->p_proto;
		else
			pnum = str2val(optarg, "proto number", 1, 255);
		proto->num = pnum;
	}
	return (proto);
}

void
pkt_compare(const u_char *a, int la, const u_char *b, int lb) {
	int l;
	int i;

	for (i = 0; i < la; i++)
		(void)printf("%02x", (unsigned int)a[i]);
	(void)printf("\n");
	l = (la <= lb) ? la : lb;
	for (i = 0; i < l; i++)
		if (a[i] == b[i])
			(void)printf("__");
		else
			(void)printf("%02x", (unsigned int)b[i]);
	for (; i < lb; i++)
		(void)printf("%02x", (unsigned int)b[i]);
	(void)printf("\n");
}
