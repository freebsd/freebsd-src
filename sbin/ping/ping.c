/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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

#if 0
#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)ping.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
/*
 *			P I N G . C
 *
 * Using the Internet Control Message Protocol (ICMP) "ECHO" facility,
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

#include <sys/param.h>		/* NB: we rely on this for <sys/types.h> */
#include <sys/capsicum.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <arpa/inet.h>

#include <libcasper.h>
#include <casper/cap_dns.h>

#ifdef IPSEC
#include <netipsec/ipsec.h>
#endif /*IPSEC*/

#include <capsicum_helpers.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stddef.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#include "main.h"
#include "ping.h"
#include "utils.h"

#define	INADDR_LEN	((int)sizeof(in_addr_t))
#define	TIMEVAL_LEN	((int)sizeof(struct tv32))
#define	MASK_LEN	(ICMP_MASKLEN - ICMP_MINLEN)
#define	TS_LEN		(ICMP_TSLEN - ICMP_MINLEN)
#define	DEFDATALEN	56		/* default data length */
#define	FLOOD_BACKOFF	20000		/* usecs to back off if F_FLOOD mode */
					/* runs out of buffer space */
#define	MAXIPLEN	(sizeof(struct ip) + MAX_IPOPTLEN)
#define	MAXICMPLEN	(ICMP_ADVLENMIN + MAX_IPOPTLEN)
#define	MAXWAIT		10000		/* max ms to wait for response */
#define	MAXALARM	(60 * 60)	/* max seconds for alarm timeout */
#define	MAXTOS		255

#define	A(bit)		rcvd_tbl[(bit)>>3]	/* identify byte in array */
#define	B(bit)		(1 << ((bit) & 0x07))	/* identify bit in byte */
#define	SET(bit)	(A(bit) |= B(bit))
#define	CLR(bit)	(A(bit) &= (~B(bit)))
#define	TST(bit)	(A(bit) & B(bit))

struct tv32 {
	int32_t tv32_sec;
	int32_t tv32_nsec;
};

/* various options */
#define	F_FLOOD		0x0001
#define	F_INTERVAL	0x0002
#define	F_PINGFILLED	0x0008
#define	F_QUIET		0x0010
#define	F_RROUTE	0x0020
#define	F_SO_DEBUG	0x0040
#define	F_SO_DONTROUTE	0x0080
#define	F_VERBOSE	0x0100
#define	F_QUIET2	0x0200
#define	F_NOLOOP	0x0400
#define	F_MTTL		0x0800
#define	F_MIF		0x1000
#define	F_AUDIBLE	0x2000
#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
#define F_POLICY	0x4000
#endif /*IPSEC_POLICY_IPSEC*/
#endif /*IPSEC*/
#define	F_TTL		0x8000
#define	F_MISSED	0x10000
#define	F_ONCE		0x20000
#define	F_HDRINCL	0x40000
#define	F_MASK		0x80000
#define	F_TIME		0x100000
#define	F_SWEEP		0x200000
#define	F_WAITTIME	0x400000
#define	F_IP_VLAN_PCP	0x800000
#define	F_DOT		0x1000000

/*
 * MAX_DUP_CHK is the number of bits in received table, i.e. the maximum
 * number of received sequence numbers we can keep track of.  Change 128
 * to 8192 for complete accuracy...
 */
#define	MAX_DUP_CHK	(8 * 128)
static int mx_dup_ck = MAX_DUP_CHK;
static char rcvd_tbl[MAX_DUP_CHK / 8];

static struct sockaddr_in whereto;	/* who to ping */
static int datalen = DEFDATALEN;
static int maxpayload;
static int ssend;		/* send socket file descriptor */
static int srecv;		/* receive socket file descriptor */
static u_char outpackhdr[IP_MAXPACKET], *outpack;
static char BBELL = '\a';	/* characters written for MISSED and AUDIBLE */
static char BSPACE = '\b';	/* characters written for flood */
static const char *DOT = ".";
static size_t DOTlen = 1;
static size_t DOTidx = 0;
static char *shostname;
static int ident;		/* process id to identify our packets */
static int uid;			/* cached uid for micro-optimization */
static u_char icmp_type = ICMP_ECHO;
static u_char icmp_type_rsp = ICMP_ECHOREPLY;
static int phdr_len = 0;
static int send_len;

/* counters */
static long nmissedmax;		/* max value of ntransmitted - nreceived - 1 */
static long npackets;		/* max packets to transmit */
static long snpackets;			/* max packets to transmit in one sweep */
static long sntransmitted;	/* # of packets we sent in this sweep */
static int sweepmax;		/* max value of payload in sweep */
static int sweepmin = 0;	/* start value of payload in sweep */
static int sweepincr = 1;	/* payload increment in sweep */
static int interval = 1000;	/* interval between packets, ms */
static int waittime = MAXWAIT;	/* timeout for each packet */

static cap_channel_t *capdns;

static void fill(char *, char *);
static cap_channel_t *capdns_setup(void);
static void pinger(void);
static char *pr_addr(struct in_addr);
static char *pr_ntime(n_time);
static void pr_icmph(struct icmp *, struct ip *, const u_char *const);
static void pr_iph(struct ip *, const u_char *);
static void pr_pack(char *, ssize_t, struct sockaddr_in *, struct timespec *);

int
ping(int argc, char *const *argv)
{
	struct sockaddr_in from, sock_in;
	struct in_addr ifaddr;
	struct timespec last, intvl;
	struct iovec iov;
	struct msghdr msg;
	struct sigaction si_sa;
	size_t sz;
	u_char *datap, packet[IP_MAXPACKET] __aligned(4);
	const char *errstr;
	char *ep, *source, *target, *payload;
	struct hostent *hp;
#ifdef IPSEC_POLICY_IPSEC
	char *policy_in, *policy_out;
#endif
	struct sockaddr_in *to;
	double t;
	u_long alarmtimeout;
	long long ltmp;
	int almost_done, ch, df, hold, i, icmp_len, mib[4], preload;
	int ssend_errno, srecv_errno, tos, ttl, pcp;
	char ctrl[CMSG_SPACE(sizeof(struct timespec))];
	char hnamebuf[MAXHOSTNAMELEN], snamebuf[MAXHOSTNAMELEN];
#ifdef IP_OPTIONS
	char rspace[MAX_IPOPTLEN];	/* record route space */
#endif
	unsigned char loop, mttl;

	payload = source = NULL;
#ifdef IPSEC_POLICY_IPSEC
	policy_in = policy_out = NULL;
#endif
	cap_rights_t rights;

	/*
	 * Do the stuff that we need root priv's for *first*, and
	 * then drop our setuid bit.  Save error reporting for
	 * after arg parsing.
	 *
	 * Historicaly ping was using one socket 's' for sending and for
	 * receiving. After capsicum(4) related changes we use two
	 * sockets. It was done for special ping use case - when user
	 * issue ping on multicast or broadcast address replies come
	 * from different addresses, not from the address we
	 * connect(2)'ed to, and send socket do not receive those
	 * packets.
	 */
	ssend = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	ssend_errno = errno;
	srecv = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	srecv_errno = errno;

	if (setuid(getuid()) != 0)
		err(EX_NOPERM, "setuid() failed");
	uid = getuid();

	if (ssend < 0) {
		errno = ssend_errno;
		err(EX_OSERR, "ssend socket");
	}

	if (srecv < 0) {
		errno = srecv_errno;
		err(EX_OSERR, "srecv socket");
	}

	alarmtimeout = df = preload = tos = pcp = 0;

	outpack = outpackhdr + sizeof(struct ip);
	while ((ch = getopt(argc, argv, PING4OPTS)) != -1) {
		switch(ch) {
		case '.':
			options |= F_DOT;
			if (optarg != NULL) {
				DOT = optarg;
				DOTlen = strlen(optarg);
			}
			break;
		case '4':
			/* This option is processed in main(). */
			break;
		case 'A':
			options |= F_MISSED;
			break;
		case 'a':
			options |= F_AUDIBLE;
			break;
		case 'C':
			options |= F_IP_VLAN_PCP;
			ltmp = strtonum(optarg, -1, 7, &errstr);
			if (errstr != NULL)
				errx(EX_USAGE, "invalid PCP: `%s'", optarg);
			pcp = ltmp;
			break;
		case 'c':
			ltmp = strtonum(optarg, 1, LONG_MAX, &errstr);
			if (errstr != NULL)
				errx(EX_USAGE,
				    "invalid count of packets to transmit: `%s'",
				    optarg);
			npackets = (long)ltmp;
			break;
		case 'D':
			options |= F_HDRINCL;
			df = 1;
			break;
		case 'd':
			options |= F_SO_DEBUG;
			break;
		case 'f':
			if (uid) {
				errno = EPERM;
				err(EX_NOPERM, "-f flag");
			}
			options |= F_FLOOD;
			options |= F_DOT;
			setbuf(stdout, (char *)NULL);
			break;
		case 'G': /* Maximum packet size for ping sweep */
			ltmp = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr != NULL) {
				errx(EX_USAGE, "invalid packet size: `%s'",
				    optarg);
			}
			sweepmax = (int)ltmp;
			if (uid != 0 && sweepmax > DEFDATALEN) {
				errc(EX_NOPERM, EPERM,
				    "packet size too large: %d > %u",
				    sweepmax, DEFDATALEN);
			}
			options |= F_SWEEP;
			break;
		case 'g': /* Minimum packet size for ping sweep */
			ltmp = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr != NULL) {
				errx(EX_USAGE, "invalid packet size: `%s'",
				    optarg);
			}
			sweepmin = (int)ltmp;
			if (uid != 0 && sweepmin > DEFDATALEN) {
				errc(EX_NOPERM, EPERM,
				    "packet size too large: %d > %u",
				    sweepmin, DEFDATALEN);
			}
			options |= F_SWEEP;
			break;
		case 'H':
			options |= F_HOSTNAME;
			break;
		case 'h': /* Packet size increment for ping sweep */
			ltmp = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr != NULL) {
				errx(EX_USAGE, "invalid packet size: `%s'",
				    optarg);
			}
			sweepincr = (int)ltmp;
			if (uid != 0 && sweepincr > DEFDATALEN) {
				errc(EX_NOPERM, EPERM,
				    "packet size too large: %d > %u",
				    sweepincr, DEFDATALEN);
			}
			options |= F_SWEEP;
			break;
		case 'I':		/* multicast interface */
			if (inet_aton(optarg, &ifaddr) == 0)
				errx(EX_USAGE,
				    "invalid multicast interface: `%s'",
				    optarg);
			options |= F_MIF;
			break;
		case 'i':		/* wait between sending packets */
			t = strtod(optarg, &ep) * 1000.0;
			if (*ep || ep == optarg || t > (double)INT_MAX)
				errx(EX_USAGE, "invalid timing interval: `%s'",
				    optarg);
			options |= F_INTERVAL;
			interval = (int)t;
			if (uid && interval < 1000) {
				errno = EPERM;
				err(EX_NOPERM, "-i interval too short");
			}
			break;
		case 'L':
			options |= F_NOLOOP;
			loop = 0;
			break;
		case 'l':
			ltmp = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL)
				errx(EX_USAGE,
				    "invalid preload value: `%s'", optarg);
			if (uid) {
				errno = EPERM;
				err(EX_NOPERM, "-l flag");
			}
			preload = (int)ltmp;
			break;
		case 'M':
			switch(optarg[0]) {
			case 'M':
			case 'm':
				options |= F_MASK;
				break;
			case 'T':
			case 't':
				options |= F_TIME;
				break;
			default:
				errx(EX_USAGE, "invalid message: `%c'", optarg[0]);
				break;
			}
			break;
		case 'm':		/* TTL */
			ltmp = strtonum(optarg, 0, MAXTTL, &errstr);
			if (errstr != NULL)
				errx(EX_USAGE, "invalid TTL: `%s'", optarg);
			ttl = (int)ltmp;
			options |= F_TTL;
			break;
		case 'n':
			options &= ~F_HOSTNAME;
			break;
		case 'o':
			options |= F_ONCE;
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
#endif /*IPSEC_POLICY_IPSEC*/
#endif /*IPSEC*/
		case 'p':		/* fill buffer with user pattern */
			options |= F_PINGFILLED;
			payload = optarg;
			break;
		case 'Q':
			options |= F_QUIET2;
			break;
		case 'q':
			options |= F_QUIET;
			break;
		case 'R':
			options |= F_RROUTE;
			break;
		case 'r':
			options |= F_SO_DONTROUTE;
			break;
		case 'S':
			source = optarg;
			break;
		case 's':		/* size of packet to send */
			ltmp = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL)
				errx(EX_USAGE, "invalid packet size: `%s'",
				    optarg);
			datalen = (int)ltmp;
			if (uid != 0 && datalen > DEFDATALEN) {
				errno = EPERM;
				err(EX_NOPERM,
				    "packet size too large: %d > %u",
				    datalen, DEFDATALEN);
			}
			break;
		case 'T':		/* multicast TTL */
			ltmp = strtonum(optarg, 0, MAXTTL, &errstr);
			if (errstr != NULL)
				errx(EX_USAGE, "invalid multicast TTL: `%s'",
				    optarg);
			mttl = (unsigned char)ltmp;
			options |= F_MTTL;
			break;
		case 't':
			alarmtimeout = strtoul(optarg, &ep, 0);
			if ((alarmtimeout < 1) || (alarmtimeout == ULONG_MAX))
				errx(EX_USAGE, "invalid timeout: `%s'",
				    optarg);
			if (alarmtimeout > MAXALARM)
				errx(EX_USAGE, "invalid timeout: `%s' > %d",
				    optarg, MAXALARM);
			{
				struct itimerval itv;

				timerclear(&itv.it_interval);
				timerclear(&itv.it_value);
				itv.it_value.tv_sec = (time_t)alarmtimeout;
				if (setitimer(ITIMER_REAL, &itv, NULL) != 0)
					err(1, "setitimer");
			}
			break;
		case 'v':
			options |= F_VERBOSE;
			break;
		case 'W':		/* wait ms for answer */
			t = strtod(optarg, &ep);
			if (*ep || ep == optarg || t > (double)INT_MAX)
				errx(EX_USAGE, "invalid timing interval: `%s'",
				    optarg);
			options |= F_WAITTIME;
			waittime = (int)t;
			break;
		case 'z':
			options |= F_HDRINCL;
			ltmp = strtol(optarg, &ep, 0);
			if (*ep || ep == optarg || ltmp > MAXTOS || ltmp < 0)
				errx(EX_USAGE, "invalid TOS: `%s'", optarg);
			tos = ltmp;
			break;
		default:
			usage();
		}
	}

	if (argc - optind != 1)
		usage();
	target = argv[optind];

	switch (options & (F_MASK|F_TIME)) {
	case 0: break;
	case F_MASK:
		icmp_type = ICMP_MASKREQ;
		icmp_type_rsp = ICMP_MASKREPLY;
		phdr_len = MASK_LEN;
		if (!(options & F_QUIET))
			(void)printf("ICMP_MASKREQ\n");
		break;
	case F_TIME:
		icmp_type = ICMP_TSTAMP;
		icmp_type_rsp = ICMP_TSTAMPREPLY;
		phdr_len = TS_LEN;
		if (!(options & F_QUIET))
			(void)printf("ICMP_TSTAMP\n");
		break;
	default:
		errx(EX_USAGE, "ICMP_TSTAMP and ICMP_MASKREQ are exclusive.");
		break;
	}
	icmp_len = sizeof(struct ip) + ICMP_MINLEN + phdr_len;
	if (options & F_RROUTE)
		icmp_len += MAX_IPOPTLEN;
	maxpayload = IP_MAXPACKET - icmp_len;
	if (datalen > maxpayload)
		errx(EX_USAGE, "packet size too large: %d > %d", datalen,
		    maxpayload);
	send_len = icmp_len + datalen;
	datap = &outpack[ICMP_MINLEN + phdr_len + TIMEVAL_LEN];
	if (options & F_PINGFILLED) {
		fill((char *)datap, payload);
	}
	capdns = capdns_setup();
	if (source) {
		bzero((char *)&sock_in, sizeof(sock_in));
		sock_in.sin_family = AF_INET;
		if (inet_aton(source, &sock_in.sin_addr) != 0) {
			shostname = source;
		} else {
			hp = cap_gethostbyname2(capdns, source, AF_INET);
			if (!hp)
				errx(EX_NOHOST, "cannot resolve %s: %s",
				    source, hstrerror(h_errno));

			sock_in.sin_len = sizeof sock_in;
			if ((unsigned)hp->h_length > sizeof(sock_in.sin_addr) ||
			    hp->h_length < 0)
				errx(1, "gethostbyname2: illegal address");
			memcpy(&sock_in.sin_addr, hp->h_addr_list[0],
			    sizeof(sock_in.sin_addr));
			(void)strncpy(snamebuf, hp->h_name,
			    sizeof(snamebuf) - 1);
			snamebuf[sizeof(snamebuf) - 1] = '\0';
			shostname = snamebuf;
		}
		if (bind(ssend, (struct sockaddr *)&sock_in, sizeof sock_in) ==
		    -1)
			err(1, "bind");
	}

	bzero(&whereto, sizeof(whereto));
	to = &whereto;
	to->sin_family = AF_INET;
	to->sin_len = sizeof *to;
	if (inet_aton(target, &to->sin_addr) != 0) {
		hostname = target;
	} else {
		hp = cap_gethostbyname2(capdns, target, AF_INET);
		if (!hp)
			errx(EX_NOHOST, "cannot resolve %s: %s",
			    target, hstrerror(h_errno));

		if ((unsigned)hp->h_length > sizeof(to->sin_addr))
			errx(1, "gethostbyname2 returned an illegal address");
		memcpy(&to->sin_addr, hp->h_addr_list[0], sizeof to->sin_addr);
		(void)strncpy(hnamebuf, hp->h_name, sizeof(hnamebuf) - 1);
		hnamebuf[sizeof(hnamebuf) - 1] = '\0';
		hostname = hnamebuf;
	}

	/* From now on we will use only reverse DNS lookups. */
#ifdef WITH_CASPER
	if (capdns != NULL) {
		const char *types[1];

		types[0] = "ADDR2NAME";
		if (cap_dns_type_limit(capdns, types, 1) < 0)
			err(1, "unable to limit access to system.dns service");
	}
#endif
	if (connect(ssend, (struct sockaddr *)&whereto, sizeof(whereto)) != 0)
		err(1, "connect");

	if (options & F_FLOOD && options & F_INTERVAL)
		errx(EX_USAGE, "-f and -i: incompatible options");

	if (options & F_FLOOD && IN_MULTICAST(ntohl(to->sin_addr.s_addr)))
		errx(EX_USAGE,
		    "-f flag cannot be used with multicast destination");
	if (options & (F_MIF | F_NOLOOP | F_MTTL)
	    && !IN_MULTICAST(ntohl(to->sin_addr.s_addr)))
		errx(EX_USAGE,
		    "-I, -L, -T flags cannot be used with unicast destination");

	if (datalen >= TIMEVAL_LEN)	/* can we time transfer */
		timing = 1;

	if ((options & (F_PINGFILLED | F_SWEEP)) == 0)
		for (i = TIMEVAL_LEN; i < datalen; ++i)
			*datap++ = i;

	ident = getpid() & 0xFFFF;

	hold = 1;
	if (options & F_SO_DEBUG) {
		(void)setsockopt(ssend, SOL_SOCKET, SO_DEBUG, (char *)&hold,
		    sizeof(hold));
		(void)setsockopt(srecv, SOL_SOCKET, SO_DEBUG, (char *)&hold,
		    sizeof(hold));
	}
	if (options & F_SO_DONTROUTE)
		(void)setsockopt(ssend, SOL_SOCKET, SO_DONTROUTE, (char *)&hold,
		    sizeof(hold));
	if (options & F_IP_VLAN_PCP) {
		(void)setsockopt(ssend, IPPROTO_IP, IP_VLAN_PCP, (char *)&pcp,
		    sizeof(pcp));
	}
#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
	if (options & F_POLICY) {
		char *buf;
		if (policy_in != NULL) {
			buf = ipsec_set_policy(policy_in, strlen(policy_in));
			if (buf == NULL)
				errx(EX_CONFIG, "%s", ipsec_strerror());
			if (setsockopt(srecv, IPPROTO_IP, IP_IPSEC_POLICY,
					buf, ipsec_get_policylen(buf)) < 0)
				err(EX_CONFIG,
				    "ipsec policy cannot be configured");
			free(buf);
		}

		if (policy_out != NULL) {
			buf = ipsec_set_policy(policy_out, strlen(policy_out));
			if (buf == NULL)
				errx(EX_CONFIG, "%s", ipsec_strerror());
			if (setsockopt(ssend, IPPROTO_IP, IP_IPSEC_POLICY,
					buf, ipsec_get_policylen(buf)) < 0)
				err(EX_CONFIG,
				    "ipsec policy cannot be configured");
			free(buf);
		}
	}
#endif /*IPSEC_POLICY_IPSEC*/
#endif /*IPSEC*/

	if (options & F_HDRINCL) {
		struct ip ip;

		memcpy(&ip, outpackhdr, sizeof(ip));
		if (!(options & (F_TTL | F_MTTL))) {
			mib[0] = CTL_NET;
			mib[1] = PF_INET;
			mib[2] = IPPROTO_IP;
			mib[3] = IPCTL_DEFTTL;
			sz = sizeof(ttl);
			if (sysctl(mib, 4, &ttl, &sz, NULL, 0) == -1)
				err(1, "sysctl(net.inet.ip.ttl)");
		}
		setsockopt(ssend, IPPROTO_IP, IP_HDRINCL, &hold, sizeof(hold));
		ip.ip_v = IPVERSION;
		ip.ip_hl = sizeof(struct ip) >> 2;
		ip.ip_tos = tos;
		ip.ip_id = 0;
		ip.ip_off = htons(df ? IP_DF : 0);
		ip.ip_ttl = ttl;
		ip.ip_p = IPPROTO_ICMP;
		ip.ip_src.s_addr = source ? sock_in.sin_addr.s_addr : INADDR_ANY;
		ip.ip_dst = to->sin_addr;
		memcpy(outpackhdr, &ip, sizeof(ip));
        }

	/*
	 * Here we enter capability mode. Further down access to global
	 * namespaces (e.g filesystem) is restricted (see capsicum(4)).
	 * We must connect(2) our socket before this point.
	 */
	caph_cache_catpages();
	if (caph_enter_casper() < 0)
		err(1, "caph_enter_casper");

	cap_rights_init(&rights, CAP_RECV, CAP_EVENT, CAP_SETSOCKOPT);
	if (caph_rights_limit(srecv, &rights) < 0)
		err(1, "cap_rights_limit srecv");
	cap_rights_init(&rights, CAP_SEND, CAP_SETSOCKOPT);
	if (caph_rights_limit(ssend, &rights) < 0)
		err(1, "cap_rights_limit ssend");

	/* record route option */
	if (options & F_RROUTE) {
#ifdef IP_OPTIONS
		bzero(rspace, sizeof(rspace));
		rspace[IPOPT_OPTVAL] = IPOPT_RR;
		rspace[IPOPT_OLEN] = sizeof(rspace) - 1;
		rspace[IPOPT_OFFSET] = IPOPT_MINOFF;
		rspace[sizeof(rspace) - 1] = IPOPT_EOL;
		if (setsockopt(ssend, IPPROTO_IP, IP_OPTIONS, rspace,
		    sizeof(rspace)) < 0)
			err(EX_OSERR, "setsockopt IP_OPTIONS");
#else
		errx(EX_UNAVAILABLE,
		    "record route not available in this implementation");
#endif /* IP_OPTIONS */
	}

	if (options & F_TTL) {
		if (setsockopt(ssend, IPPROTO_IP, IP_TTL, &ttl,
		    sizeof(ttl)) < 0) {
			err(EX_OSERR, "setsockopt IP_TTL");
		}
	}
	if (options & F_NOLOOP) {
		if (setsockopt(ssend, IPPROTO_IP, IP_MULTICAST_LOOP, &loop,
		    sizeof(loop)) < 0) {
			err(EX_OSERR, "setsockopt IP_MULTICAST_LOOP");
		}
	}
	if (options & F_MTTL) {
		if (setsockopt(ssend, IPPROTO_IP, IP_MULTICAST_TTL, &mttl,
		    sizeof(mttl)) < 0) {
			err(EX_OSERR, "setsockopt IP_MULTICAST_TTL");
		}
	}
	if (options & F_MIF) {
		if (setsockopt(ssend, IPPROTO_IP, IP_MULTICAST_IF, &ifaddr,
		    sizeof(ifaddr)) < 0) {
			err(EX_OSERR, "setsockopt IP_MULTICAST_IF");
		}
	}
#ifdef SO_TIMESTAMP
	{
		int on = 1;
		int ts_clock = SO_TS_MONOTONIC;
		if (setsockopt(srecv, SOL_SOCKET, SO_TIMESTAMP, &on,
		    sizeof(on)) < 0)
			err(EX_OSERR, "setsockopt SO_TIMESTAMP");
		if (setsockopt(srecv, SOL_SOCKET, SO_TS_CLOCK, &ts_clock,
		    sizeof(ts_clock)) < 0)
			err(EX_OSERR, "setsockopt SO_TS_CLOCK");
	}
#endif
	if (sweepmax) {
		if (sweepmin > sweepmax)
			errx(EX_USAGE,
	    "Maximum packet size must be no less than the minimum packet size");

		if (sweepmax > maxpayload - TIMEVAL_LEN)
			errx(EX_USAGE, "Invalid sweep maximum");

		if (datalen != DEFDATALEN)
			errx(EX_USAGE,
		    "Packet size and ping sweep are mutually exclusive");

		if (npackets > 0) {
			snpackets = npackets;
			npackets = 0;
		} else
			snpackets = 1;
		datalen = sweepmin;
		send_len = icmp_len + sweepmin;
	}
	if (options & F_SWEEP && !sweepmax)
		errx(EX_USAGE, "Maximum sweep size must be specified");

	/*
	 * When pinging the broadcast address, you can get a lot of answers.
	 * Doing something so evil is useful if you are trying to stress the
	 * ethernet, or just want to fill the arp cache to get some stuff for
	 * /etc/ethers.  But beware: RFC 1122 allows hosts to ignore broadcast
	 * or multicast pings if they wish.
	 */

	/*
	 * XXX receive buffer needs undetermined space for mbuf overhead
	 * as well.
	 */
	hold = IP_MAXPACKET + 128;
	(void)setsockopt(srecv, SOL_SOCKET, SO_RCVBUF, (char *)&hold,
	    sizeof(hold));
	/* CAP_SETSOCKOPT removed */
	cap_rights_init(&rights, CAP_RECV, CAP_EVENT);
	if (caph_rights_limit(srecv, &rights) < 0)
		err(1, "cap_rights_limit srecv setsockopt");
	if (uid == 0)
		(void)setsockopt(ssend, SOL_SOCKET, SO_SNDBUF, (char *)&hold,
		    sizeof(hold));
	/* CAP_SETSOCKOPT removed */
	cap_rights_init(&rights, CAP_SEND);
	if (caph_rights_limit(ssend, &rights) < 0)
		err(1, "cap_rights_limit ssend setsockopt");

	if (to->sin_family == AF_INET) {
		(void)printf("PING %s (%s)", hostname,
		    inet_ntoa(to->sin_addr));
		if (source)
			(void)printf(" from %s", shostname);
		if (sweepmax)
			(void)printf(": (%d ... %d) data bytes\n",
			    sweepmin, sweepmax);
		else
			(void)printf(": %d data bytes\n", datalen);

	} else {
		if (sweepmax)
			(void)printf("PING %s: (%d ... %d) data bytes\n",
			    hostname, sweepmin, sweepmax);
		else
			(void)printf("PING %s: %d data bytes\n", hostname, datalen);
	}

	/*
	 * Use sigaction() instead of signal() to get unambiguous semantics,
	 * in particular with SA_RESTART not set.
	 */

	sigemptyset(&si_sa.sa_mask);
	si_sa.sa_flags = 0;
	si_sa.sa_handler = onsignal;
	if (sigaction(SIGINT, &si_sa, 0) == -1)
		err(EX_OSERR, "sigaction SIGINT");
	seenint = 0;
	if (sigaction(SIGINFO, &si_sa, 0) == -1)
		err(EX_OSERR, "sigaction SIGINFO");
	seeninfo = 0;
	if (alarmtimeout > 0) {
		if (sigaction(SIGALRM, &si_sa, 0) == -1)
			err(EX_OSERR, "sigaction SIGALRM");
	}

	bzero(&msg, sizeof(msg));
	msg.msg_name = (caddr_t)&from;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
#ifdef SO_TIMESTAMP
	msg.msg_control = (caddr_t)ctrl;
	msg.msg_controllen = sizeof(ctrl);
#endif
	iov.iov_base = packet;
	iov.iov_len = IP_MAXPACKET;

	if (preload == 0)
		pinger();		/* send the first ping */
	else {
		if (npackets != 0 && preload > npackets)
			preload = npackets;
		while (preload--)	/* fire off them quickies */
			pinger();
	}
	(void)clock_gettime(CLOCK_MONOTONIC, &last);

	if (options & F_FLOOD) {
		intvl.tv_sec = 0;
		intvl.tv_nsec = 10000000;
	} else {
		intvl.tv_sec = interval / 1000;
		intvl.tv_nsec = interval % 1000 * 1000000;
	}

	almost_done = 0;
	while (seenint == 0) {
		struct timespec now, timeout;
		fd_set rfds;
		int n;
		ssize_t cc;

		/* signal handling */
		if (seeninfo) {
			pr_summary(stderr);
			seeninfo = 0;
			continue;
		}
		if ((unsigned)srecv >= FD_SETSIZE)
			errx(EX_OSERR, "descriptor too large");
		FD_ZERO(&rfds);
		FD_SET(srecv, &rfds);
		(void)clock_gettime(CLOCK_MONOTONIC, &now);
		timespecadd(&last, &intvl, &timeout);
		timespecsub(&timeout, &now, &timeout);
		if (timeout.tv_sec < 0)
			timespecclear(&timeout);

		n = pselect(srecv + 1, &rfds, NULL, NULL, &timeout, NULL);
		if (n < 0)
			continue;	/* EINTR */
		if (n == 1) {
			struct timespec *tv = NULL;
#ifdef SO_TIMESTAMP
			struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
#endif
			msg.msg_namelen = sizeof(from);
			if ((cc = recvmsg(srecv, &msg, 0)) < 0) {
				if (errno == EINTR)
					continue;
				warn("recvmsg");
				continue;
			}
			/* If we have a 0 byte read from recvfrom continue */
			if (cc == 0)
				continue;
#ifdef SO_TIMESTAMP
			if (cmsg != NULL &&
			    cmsg->cmsg_level == SOL_SOCKET &&
			    cmsg->cmsg_type == SCM_TIMESTAMP &&
			    cmsg->cmsg_len == CMSG_LEN(sizeof *tv)) {
				/* Copy to avoid alignment problems: */
				memcpy(&now, CMSG_DATA(cmsg), sizeof(now));
				tv = &now;
			}
#endif
			if (tv == NULL) {
				(void)clock_gettime(CLOCK_MONOTONIC, &now);
				tv = &now;
			}
			pr_pack((char *)packet, cc, &from, tv);
			if ((options & F_ONCE && nreceived) ||
			    (npackets && nreceived >= npackets))
				break;
		}
		if (n == 0 || (options & F_FLOOD)) {
			if (sweepmax && sntransmitted == snpackets) {
				if (datalen + sweepincr > sweepmax)
					break;
				for (i = 0; i < sweepincr; i++)
					*datap++ = i;
				datalen += sweepincr;
				send_len = icmp_len + datalen;
				sntransmitted = 0;
			}
			if (!npackets || ntransmitted < npackets)
				pinger();
			else {
				if (almost_done)
					break;
				almost_done = 1;
				/*
				 * If we're not transmitting any more packets,
				 * change the timer to wait two round-trip times
				 * if we've received any packets or (waittime)
				 * milliseconds if we haven't.
				 */
				intvl.tv_nsec = 0;
				if (nreceived) {
					intvl.tv_sec = 2 * tmax / 1000;
					if (intvl.tv_sec == 0)
						intvl.tv_sec = 1;
				} else {
					intvl.tv_sec = waittime / 1000;
					intvl.tv_nsec =
					    waittime % 1000 * 1000000;
				}
			}
			(void)clock_gettime(CLOCK_MONOTONIC, &last);
			if (ntransmitted - nreceived - 1 > nmissedmax) {
				nmissedmax = ntransmitted - nreceived - 1;
				if (options & F_MISSED)
					(void)write(STDOUT_FILENO, &BBELL, 1);
			}
		}
	}
	pr_summary(stdout);

	exit(nreceived ? 0 : 2);
}

/*
 * pinger --
 *	Compose and transmit an ICMP ECHO REQUEST packet.  The IP packet
 * will be added on by the kernel.  The ID field is our UNIX process ID,
 * and the sequence number is an ascending integer.  The first TIMEVAL_LEN
 * bytes of the data portion are used to hold a UNIX "timespec" struct in
 * host byte-order, to compute the round-trip time.
 */
static void
pinger(void)
{
	struct timespec now;
	struct tv32 tv32;
	struct icmp icp;
	int cc, i;
	u_char *packet;

	packet = outpack;
	memcpy(&icp, outpack, ICMP_MINLEN + phdr_len);
	icp.icmp_type = icmp_type;
	icp.icmp_code = 0;
	icp.icmp_cksum = 0;
	icp.icmp_seq = htons(ntransmitted);
	icp.icmp_id = ident;			/* ID */

	CLR(ntransmitted % mx_dup_ck);

	if ((options & F_TIME) || timing) {
		(void)clock_gettime(CLOCK_MONOTONIC, &now);
		/*
		 * Truncate seconds down to 32 bits in order
		 * to fit the timestamp within 8 bytes of the
		 * packet. We're only concerned with
		 * durations, not absolute times.
		 */
		tv32.tv32_sec = (uint32_t)htonl(now.tv_sec);
		tv32.tv32_nsec = (uint32_t)htonl(now.tv_nsec);
		if (options & F_TIME)
			icp.icmp_otime = htonl((now.tv_sec % (24*60*60))
				* 1000 + now.tv_nsec / 1000000);
		if (timing)
			bcopy((void *)&tv32,
			    (void *)&outpack[ICMP_MINLEN + phdr_len],
			    sizeof(tv32));
	}

	memcpy(outpack, &icp, ICMP_MINLEN + phdr_len);

	cc = ICMP_MINLEN + phdr_len + datalen;

	/* compute ICMP checksum here */
	icp.icmp_cksum = in_cksum(outpack, cc);
	/* Update icmp_cksum in the raw packet data buffer. */
	memcpy(outpack + offsetof(struct icmp, icmp_cksum), &icp.icmp_cksum,
	    sizeof(icp.icmp_cksum));

	if (options & F_HDRINCL) {
		struct ip ip;

		cc += sizeof(struct ip);
		ip.ip_len = htons(cc);
		/* Update ip_len in the raw packet data buffer. */
		memcpy(outpackhdr + offsetof(struct ip, ip_len), &ip.ip_len,
		    sizeof(ip.ip_len));
		ip.ip_sum = in_cksum(outpackhdr, cc);
		/* Update ip_sum in the raw packet data buffer. */
		memcpy(outpackhdr + offsetof(struct ip, ip_sum), &ip.ip_sum,
		    sizeof(ip.ip_sum));
		packet = outpackhdr;
	}
	i = send(ssend, (char *)packet, cc, 0);
	if (i < 0 || i != cc)  {
		if (i < 0) {
			if (options & F_FLOOD && errno == ENOBUFS) {
				usleep(FLOOD_BACKOFF);
				return;
			}
			warn("sendto");
		} else {
			warn("%s: partial write: %d of %d bytes",
			     hostname, i, cc);
		}
	}
	ntransmitted++;
	sntransmitted++;
	if (!(options & F_QUIET) && options & F_DOT)
		(void)write(STDOUT_FILENO, &DOT[DOTidx++ % DOTlen], 1);
}

/*
 * pr_pack --
 *	Print out the packet, if it came from us.  This logic is necessary
 * because ALL readers of the ICMP socket get a copy of ALL ICMP packets
 * which arrive ('tis only fair).  This permits multiple copies of this
 * program to be run without having intermingled output (or statistics!).
 */
static void
pr_pack(char *buf, ssize_t cc, struct sockaddr_in *from, struct timespec *tv)
{
	struct in_addr ina;
	u_char *cp, *dp, l;
	struct icmp icp;
	struct ip ip;
	const u_char *icmp_data_raw;
	ssize_t icmp_data_raw_len;
	double triptime;
	int dupflag, i, j, recv_len;
	int8_t hlen;
	uint16_t seq;
	static int old_rrlen;
	static char old_rr[MAX_IPOPTLEN];
	struct ip oip;
	u_char oip_header_len;
	struct icmp oicmp;
	const u_char *oicmp_raw;

	/*
	 * Get size of IP header of the received packet.
	 * The header length is contained in the lower four bits of the first
	 * byte and represents the number of 4 byte octets the header takes up.
	 *
	 * The IHL minimum value is 5 (20 bytes) and its maximum value is 15
	 * (60 bytes).
	 */
	memcpy(&l, buf, sizeof(l));
	hlen = (l & 0x0f) << 2;

	/* Reject IP packets with a short header */
	if (hlen < (int8_t) sizeof(struct ip)) {
		if (options & F_VERBOSE)
			warn("IHL too short (%d bytes) from %s", hlen,
			     inet_ntoa(from->sin_addr));
		return;
	}

	memcpy(&ip, buf, sizeof(struct ip));

	/* Check packet has enough data to carry a valid ICMP header */
	recv_len = cc;
	if (cc < hlen + ICMP_MINLEN) {
		if (options & F_VERBOSE)
			warn("packet too short (%zd bytes) from %s", cc,
			     inet_ntoa(from->sin_addr));
		return;
	}

	icmp_data_raw_len = cc - (hlen + offsetof(struct icmp, icmp_data));
	icmp_data_raw = buf + hlen + offsetof(struct icmp, icmp_data);

	/* Now the ICMP part */
	cc -= hlen;
	memcpy(&icp, buf + hlen, MIN((ssize_t)sizeof(icp), cc));
	if (icp.icmp_type == icmp_type_rsp) {
		if (icp.icmp_id != ident)
			return;			/* 'Twas not our ECHO */
		++nreceived;
		triptime = 0.0;
		if (timing) {
			struct timespec tv1;
			struct tv32 tv32;
			const u_char *tp;

			tp = icmp_data_raw + phdr_len;

			if ((size_t)(cc - ICMP_MINLEN - phdr_len) >=
			    sizeof(tv1)) {
				/* Copy to avoid alignment problems: */
				memcpy(&tv32, tp, sizeof(tv32));
				tv1.tv_sec = ntohl(tv32.tv32_sec);
				tv1.tv_nsec = ntohl(tv32.tv32_nsec);
				timespecsub(tv, &tv1, tv);
				triptime = ((double)tv->tv_sec) * 1000.0 +
				    ((double)tv->tv_nsec) / 1000000.0;
				if (triptime < 0) {
					warnx("time of day goes back (%.3f ms),"
					    " clamping time to 0",
					    triptime);
					triptime = 0;
				}
				tsum += triptime;
				tsumsq += triptime * triptime;
				if (triptime < tmin)
					tmin = triptime;
				if (triptime > tmax)
					tmax = triptime;
			} else
				timing = 0;
		}

		seq = ntohs(icp.icmp_seq);

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

		if (options & F_WAITTIME && triptime > waittime) {
			++nrcvtimeout;
			return;
		}

		if (options & F_DOT)
			(void)write(STDOUT_FILENO, &BSPACE, 1);
		else {
			(void)printf("%zd bytes from %s: icmp_seq=%u", cc,
			    pr_addr(from->sin_addr), seq);
			(void)printf(" ttl=%d", ip.ip_ttl);
			if (timing)
				(void)printf(" time=%.3f ms", triptime);
			if (dupflag)
				(void)printf(" (DUP!)");
			if (options & F_AUDIBLE)
				(void)write(STDOUT_FILENO, &BBELL, 1);
			if (options & F_MASK) {
				/* Just prentend this cast isn't ugly */
				(void)printf(" mask=%s",
					inet_ntoa(*(struct in_addr *)&(icp.icmp_mask)));
			}
			if (options & F_TIME) {
				(void)printf(" tso=%s", pr_ntime(icp.icmp_otime));
				(void)printf(" tsr=%s", pr_ntime(icp.icmp_rtime));
				(void)printf(" tst=%s", pr_ntime(icp.icmp_ttime));
			}
			if (recv_len != send_len) {
                        	(void)printf(
				     "\nwrong total length %d instead of %d",
				     recv_len, send_len);
			}
			/* check the data */
			cp = (u_char*)(buf + hlen + offsetof(struct icmp,
				icmp_data) + phdr_len);
			dp = &outpack[ICMP_MINLEN + phdr_len];
			cc -= ICMP_MINLEN + phdr_len;
			i = 0;
			if (timing) {   /* don't check variable timestamp */
				cp += TIMEVAL_LEN;
				dp += TIMEVAL_LEN;
				cc -= TIMEVAL_LEN;
				i += TIMEVAL_LEN;
			}
			for (; i < datalen && cc > 0; ++i, ++cp, ++dp, --cc) {
				if (*cp != *dp) {
	(void)printf("\nwrong data byte #%d should be 0x%x but was 0x%x",
	    i, *dp, *cp);
					(void)printf("\ncp:");
					cp = (u_char*)(buf + hlen +
					    offsetof(struct icmp, icmp_data));
					for (i = 0; i < datalen; ++i, ++cp) {
						if ((i % 16) == 8)
							(void)printf("\n\t");
						(void)printf(" %2x", *cp);
					}
					(void)printf("\ndp:");
					cp = &outpack[ICMP_MINLEN];
					for (i = 0; i < datalen; ++i, ++cp) {
						if ((i % 16) == 8)
							(void)printf("\n\t");
						(void)printf(" %2x", *cp);
					}
					break;
				}
			}
		}
	} else {
		/*
		 * We've got something other than an ECHOREPLY.
		 * See if it's a reply to something that we sent.
		 * We can compare IP destination, protocol,
		 * and ICMP type and ID.
		 *
		 * Only print all the error messages if we are running
		 * as root to avoid leaking information not normally
		 * available to those not running as root.
		 */

		/*
		 * If we don't have enough bytes for a quoted IP header and an
		 * ICMP header then stop.
		 */
		if (icmp_data_raw_len <
				(ssize_t)(sizeof(struct ip) + sizeof(struct icmp))) {
			if (options & F_VERBOSE)
				warnx("quoted data too short (%zd bytes) from %s",
					icmp_data_raw_len, inet_ntoa(from->sin_addr));
			return;
		}

		memcpy(&oip_header_len, icmp_data_raw, sizeof(oip_header_len));
		oip_header_len = (oip_header_len & 0x0f) << 2;

		/* Reject IP packets with a short header */
		if (oip_header_len < sizeof(struct ip)) {
			if (options & F_VERBOSE)
				warnx("inner IHL too short (%d bytes) from %s",
					oip_header_len, inet_ntoa(from->sin_addr));
			return;
		}

		/*
		 * Check against the actual IHL length, to protect against
		 * quoated packets carrying IP options.
		 */
		if (icmp_data_raw_len <
				(ssize_t)(oip_header_len + sizeof(struct icmp))) {
			if (options & F_VERBOSE)
				warnx("inner packet too short (%zd bytes) from %s",
				     icmp_data_raw_len, inet_ntoa(from->sin_addr));
			return;
		}

		memcpy(&oip, icmp_data_raw, sizeof(struct ip));
		oicmp_raw = icmp_data_raw + oip_header_len;
		memcpy(&oicmp, oicmp_raw, sizeof(struct icmp));

		if (((options & F_VERBOSE) && uid == 0) ||
		    (!(options & F_QUIET2) &&
		     (oip.ip_dst.s_addr == whereto.sin_addr.s_addr) &&
		     (oip.ip_p == IPPROTO_ICMP) &&
		     (oicmp.icmp_type == ICMP_ECHO) &&
		     (oicmp.icmp_id == ident))) {
		    (void)printf("%zd bytes from %s: ", cc,
			pr_addr(from->sin_addr));
		    pr_icmph(&icp, &oip, icmp_data_raw);
		} else
		    return;
	}

	/* Display any IP options */
	cp = (u_char *)buf + sizeof(struct ip);

	for (; hlen > (int)sizeof(struct ip); --hlen, ++cp)
		switch (*cp) {
		case IPOPT_EOL:
			hlen = 0;
			break;
		case IPOPT_LSRR:
		case IPOPT_SSRR:
			(void)printf(*cp == IPOPT_LSRR ?
			    "\nLSRR: " : "\nSSRR: ");
			j = cp[IPOPT_OLEN] - IPOPT_MINOFF + 1;
			hlen -= 2;
			cp += 2;
			if (j >= INADDR_LEN &&
			    j <= hlen - (int)sizeof(struct ip)) {
				for (;;) {
					bcopy(++cp, &ina.s_addr, INADDR_LEN);
					if (ina.s_addr == 0)
						(void)printf("\t0.0.0.0");
					else
						(void)printf("\t%s",
						     pr_addr(ina));
					hlen -= INADDR_LEN;
					cp += INADDR_LEN - 1;
					j -= INADDR_LEN;
					if (j < INADDR_LEN)
						break;
					(void)putchar('\n');
				}
			} else
				(void)printf("\t(truncated route)");
			break;
		case IPOPT_RR:
			j = cp[IPOPT_OLEN];		/* get length */
			i = cp[IPOPT_OFFSET];		/* and pointer */
			hlen -= 2;
			cp += 2;
			if (i > j)
				i = j;
			i = i - IPOPT_MINOFF + 1;
			if (i < 0 || i > (hlen - (int)sizeof(struct ip))) {
				old_rrlen = 0;
				continue;
			}
			if (i == old_rrlen
			    && !bcmp((char *)cp, old_rr, i)
			    && !(options & F_DOT)) {
				(void)printf("\t(same route)");
				hlen -= i;
				cp += i;
				break;
			}
			old_rrlen = i;
			bcopy((char *)cp, old_rr, i);
			(void)printf("\nRR: ");
			if (i >= INADDR_LEN &&
			    i <= hlen - (int)sizeof(struct ip)) {
				for (;;) {
					bcopy(++cp, &ina.s_addr, INADDR_LEN);
					if (ina.s_addr == 0)
						(void)printf("\t0.0.0.0");
					else
						(void)printf("\t%s",
						     pr_addr(ina));
					hlen -= INADDR_LEN;
					cp += INADDR_LEN - 1;
					i -= INADDR_LEN;
					if (i < INADDR_LEN)
						break;
					(void)putchar('\n');
				}
			} else
				(void)printf("\t(truncated route)");
			break;
		case IPOPT_NOP:
			(void)printf("\nNOP");
			break;
		default:
			(void)printf("\nunknown option %x", *cp);
			break;
		}
	if (!(options & F_DOT)) {
		(void)putchar('\n');
		(void)fflush(stdout);
	}
}

/*
 * pr_icmph --
 *	Print a descriptive string about an ICMP header.
 */
static void
pr_icmph(struct icmp *icp, struct ip *oip, const u_char *const oicmp_raw)
{

	switch(icp->icmp_type) {
	case ICMP_ECHOREPLY:
		(void)printf("Echo Reply\n");
		/* XXX ID + Seq + Data */
		break;
	case ICMP_UNREACH:
		switch(icp->icmp_code) {
		case ICMP_UNREACH_NET:
			(void)printf("Destination Net Unreachable\n");
			break;
		case ICMP_UNREACH_HOST:
			(void)printf("Destination Host Unreachable\n");
			break;
		case ICMP_UNREACH_PROTOCOL:
			(void)printf("Destination Protocol Unreachable\n");
			break;
		case ICMP_UNREACH_PORT:
			(void)printf("Destination Port Unreachable\n");
			break;
		case ICMP_UNREACH_NEEDFRAG:
			(void)printf("frag needed and DF set (MTU %d)\n",
					ntohs(icp->icmp_nextmtu));
			break;
		case ICMP_UNREACH_SRCFAIL:
			(void)printf("Source Route Failed\n");
			break;
		case ICMP_UNREACH_FILTER_PROHIB:
			(void)printf("Communication prohibited by filter\n");
			break;
		default:
			(void)printf("Dest Unreachable, Bad Code: %d\n",
			    icp->icmp_code);
			break;
		}
		/* Print returned IP header information */
		pr_iph(oip, oicmp_raw);
		break;
	case ICMP_SOURCEQUENCH:
		(void)printf("Source Quench\n");
		pr_iph(oip, oicmp_raw);
		break;
	case ICMP_REDIRECT:
		switch(icp->icmp_code) {
		case ICMP_REDIRECT_NET:
			(void)printf("Redirect Network");
			break;
		case ICMP_REDIRECT_HOST:
			(void)printf("Redirect Host");
			break;
		case ICMP_REDIRECT_TOSNET:
			(void)printf("Redirect Type of Service and Network");
			break;
		case ICMP_REDIRECT_TOSHOST:
			(void)printf("Redirect Type of Service and Host");
			break;
		default:
			(void)printf("Redirect, Bad Code: %d", icp->icmp_code);
			break;
		}
		(void)printf("(New addr: %s)\n", inet_ntoa(icp->icmp_gwaddr));
		pr_iph(oip, oicmp_raw);
		break;
	case ICMP_ECHO:
		(void)printf("Echo Request\n");
		/* XXX ID + Seq + Data */
		break;
	case ICMP_TIMXCEED:
		switch(icp->icmp_code) {
		case ICMP_TIMXCEED_INTRANS:
			(void)printf("Time to live exceeded\n");
			break;
		case ICMP_TIMXCEED_REASS:
			(void)printf("Frag reassembly time exceeded\n");
			break;
		default:
			(void)printf("Time exceeded, Bad Code: %d\n",
			    icp->icmp_code);
			break;
		}
		pr_iph(oip, oicmp_raw);
		break;
	case ICMP_PARAMPROB:
		(void)printf("Parameter problem: pointer = 0x%02x\n",
		    icp->icmp_hun.ih_pptr);
		pr_iph(oip, oicmp_raw);
		break;
	case ICMP_TSTAMP:
		(void)printf("Timestamp\n");
		/* XXX ID + Seq + 3 timestamps */
		break;
	case ICMP_TSTAMPREPLY:
		(void)printf("Timestamp Reply\n");
		/* XXX ID + Seq + 3 timestamps */
		break;
	case ICMP_IREQ:
		(void)printf("Information Request\n");
		/* XXX ID + Seq */
		break;
	case ICMP_IREQREPLY:
		(void)printf("Information Reply\n");
		/* XXX ID + Seq */
		break;
	case ICMP_MASKREQ:
		(void)printf("Address Mask Request\n");
		break;
	case ICMP_MASKREPLY:
		(void)printf("Address Mask Reply\n");
		break;
	case ICMP_ROUTERADVERT:
		(void)printf("Router Advertisement\n");
		break;
	case ICMP_ROUTERSOLICIT:
		(void)printf("Router Solicitation\n");
		break;
	default:
		(void)printf("Bad ICMP type: %d\n", icp->icmp_type);
	}
}

/*
 * pr_iph --
 *	Print an IP header with options.
 */
static void
pr_iph(struct ip *ip, const u_char *cp)
{
	struct in_addr dst_ina, src_ina;
	int hlen;

	hlen = ip->ip_hl << 2;
	cp = cp + sizeof(struct ip);		/* point to options */

	memcpy(&src_ina, &ip->ip_src.s_addr, sizeof(src_ina));
	memcpy(&dst_ina, &ip->ip_dst.s_addr, sizeof(dst_ina));

	(void)printf("Vr HL TOS  Len   ID Flg  off TTL Pro  cks %*s %*s",
	    (int)strlen(inet_ntoa(src_ina)), "Src",
	    (int)strlen(inet_ntoa(dst_ina)), "Dst");
	if (hlen > (int)sizeof(struct ip))
		(void)printf(" Opts");
	(void)putchar('\n');
	(void)printf(" %1x  %1x  %02x %04x %04x",
	    ip->ip_v, ip->ip_hl, ip->ip_tos, ntohs(ip->ip_len),
	    ntohs(ip->ip_id));
	(void)printf("   %1x %04x",
	    (ntohs(ip->ip_off) & 0xe000) >> 13,
	    ntohs(ip->ip_off) & 0x1fff);
	(void)printf("  %02x  %02x %04x", ip->ip_ttl, ip->ip_p,
							    ntohs(ip->ip_sum));
	(void)printf(" %s", inet_ntoa(src_ina));
	(void)printf(" %s", inet_ntoa(dst_ina));
	/* dump any option bytes */
	if (hlen > (int)sizeof(struct ip)) {
		(void)printf(" ");
		while (hlen-- > (int)sizeof(struct ip)) {
			(void)printf("%02x", *cp++);
		}
	}
	(void)putchar('\n');
}

/*
 * pr_addr --
 *	Return an ascii host address as a dotted quad and optionally with
 * a hostname.
 */
static char *
pr_addr(struct in_addr ina)
{
	struct hostent *hp;
	static char buf[16 + 3 + MAXHOSTNAMELEN];

	if (!(options & F_HOSTNAME))
		return inet_ntoa(ina);

	hp = cap_gethostbyaddr(capdns, (char *)&ina, sizeof(ina), AF_INET);

	if (hp == NULL)
		return inet_ntoa(ina);

	(void)snprintf(buf, sizeof(buf), "%s (%s)", hp->h_name,
	    inet_ntoa(ina));
	return(buf);
}

static char *
pr_ntime(n_time timestamp)
{
	static char buf[11];
	int hour, min, sec;

	sec = ntohl(timestamp) / 1000;
	hour = sec / 60 / 60;
	min = (sec % (60 * 60)) / 60;
	sec = (sec % (60 * 60)) % 60;

	(void)snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour, min, sec);

	return (buf);
}

static void
fill(char *bp, char *patp)
{
	char *cp;
	int pat[16];
	u_int ii, jj, kk;

	for (cp = patp; *cp; cp++) {
		if (!isxdigit(*cp))
			errx(EX_USAGE,
			    "patterns must be specified as hex digits");

	}
	ii = sscanf(patp,
	    "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
	    &pat[0], &pat[1], &pat[2], &pat[3], &pat[4], &pat[5], &pat[6],
	    &pat[7], &pat[8], &pat[9], &pat[10], &pat[11], &pat[12],
	    &pat[13], &pat[14], &pat[15]);

	if (ii > 0)
		for (kk = 0; kk <= maxpayload - (TIMEVAL_LEN + ii); kk += ii)
			for (jj = 0; jj < ii; ++jj)
				bp[jj + kk] = pat[jj];
	if (!(options & F_QUIET)) {
		(void)printf("PATTERN: 0x");
		for (jj = 0; jj < ii; ++jj)
			(void)printf("%02x", bp[jj] & 0xFF);
		(void)printf("\n");
	}
}

static cap_channel_t *
capdns_setup(void)
{
	cap_channel_t *capcas, *capdnsloc;
#ifdef WITH_CASPER
	const char *types[2];
	int families[1];
#endif
	capcas = cap_init();
	if (capcas == NULL)
		err(1, "unable to create casper process");
	capdnsloc = cap_service_open(capcas, "system.dns");
	/* Casper capability no longer needed. */
	cap_close(capcas);
	if (capdnsloc == NULL)
		err(1, "unable to open system.dns service");
#ifdef WITH_CASPER
	types[0] = "NAME2ADDR";
	types[1] = "ADDR2NAME";
	if (cap_dns_type_limit(capdnsloc, types, 2) < 0)
		err(1, "unable to limit access to system.dns service");
	families[0] = AF_INET;
	if (cap_dns_family_limit(capdnsloc, families, 1) < 0)
		err(1, "unable to limit access to system.dns service");
#endif
	return (capdnsloc);
}
