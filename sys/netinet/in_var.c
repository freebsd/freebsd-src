/*
 * Copyright (c) 1982, 1986, 1988 Regents of the University of California.
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
 *
 *	$Id: in_var.c,v 1.1 1993/12/09 03:43:29 wollman Exp $
 */

/*
 * This file attempts to centralize all the various variables that have
 * a hand in controlling the operation of IP and its ULPs.
 */

#include "param.h"
#include "systm.h"
#include "mbuf.h"
#include "domain.h"
#include "protosw.h"
#include "socket.h"
#include "time.h"
#include "net/if.h"
#include "net/route.h"

#include "in.h"
#include "in_systm.h"
#include "ip.h"
#include "in_pcb.h"
#include "in_var.h"
#include "ip_var.h"
#include "ip_icmp.h"
#include "icmp_var.h"


/*
 * IPFORWARDING controls whether the IP layer will forward packets received
 * by us but not addressed to one of our addresses.
 *
 * IPSENDREDIRECTS controls whether the IP layer will send ICMP Redirect
 * messages.
 *
 * GATEWAY turns both of these on, and also allocates more memory for some
 * networking functions.
 */

#ifndef	IPFORWARDING
#ifdef GATEWAY
#define	IPFORWARDING	1	/* forward IP packets not for us */
#else /* not GATEWAY */
#define	IPFORWARDING	0	/* don't forward IP packets not for us */
#endif /* not GATEWAY */
#endif /* not IPFORWARDING */

/*
 * NB: RFC 1122, ``Requirements for Internet Hosts: Communication Layers'',
 * absolutely forbids hosts (which are not acting as gateways) from sending
 * ICMP redirects.
 */
#ifndef	IPSENDREDIRECTS
#ifdef GATEWAY
#define	IPSENDREDIRECTS	1
#else /* not GATEWAY */
#define IPSENDREDIRECTS 0
#endif /* not GATEWAY */
#endif /* not IPSENDREDIRECTS */

int	ipforwarding = IPFORWARDING;
int	ipsendredirects = IPSENDREDIRECTS;
#ifdef DIAGNOSTIC
int	ipprintfs = 0;
#endif

/*
 * ip_protox[] maps from IP protocol number to an index in inetsw[].
 */
u_char	ip_protox[IPPROTO_MAX];

/*
 * ipqmaxlen is the maximum length of the IP input queue.
 * ipintrq is the queue itself.
 */
struct ifqueue ipintrq;
int	ipqmaxlen = IFQ_MAXLEN;

/*
 * the IP reassembly queue
 */
struct	ipq ipq;

/*
 * in_ifaddr points to a linked list of IP interface addresses, managed
 * by the code in in.c.
 */
struct	in_ifaddr *in_ifaddr;			/* first inet address */

/*
 * statistics for netstat and management
 */
struct	ipstat ipstat;

/*
 * ip_id is the next IP packet id number to be assigned (used in fragmentation
 * and reassembly).
 */
u_short ip_id;

/*
 * When acting as a gateway, the IP layer keeps track of how many packets
 * are forwarded for each (in-ifp, out-ifp) pair.  This code needs to get
 * updated or junked now that interfaces can come and go like the wind.
 * (in ip_input.c)
 */
#ifdef GATEWAY
u_long	*ip_ifmatrix;
#endif

/*
 * ipaddr is a sockaddr_in used by various bits of code when they
 * need to convert a `struct in_addr' to a `struct sockaddr_in'.
 *
 * ipforward_rt is a route used when forwarding packets.  It functions
 * as a route cache of order one, if you want to think of it that way.
 */
struct	sockaddr_in ipaddr = { sizeof(ipaddr), AF_INET };
struct	route ipforward_rt;

/*
 * inetctlerrmap[] maps control input commands to errno values.  0 means
 * don't signal error.
 */
u_char inetctlerrmap[PRC_NCMDS] = {
	0,			/* ifdown */
	0,			/* routedead */
	0,			/* #2 */
	0,			/* quench2 */
	0,			/* quench */
	EMSGSIZE,		/* msgsize */
	EHOSTDOWN,		/* hostdead */
	EHOSTUNREACH,		/* hostunreach */
	EHOSTUNREACH,		/* unreachnet */
	EHOSTUNREACH,		/* unreachhost */
	ECONNREFUSED,		/* unreachproto */
	ECONNREFUSED,		/* unreachport */
	EMSGSIZE,		/* old needfrag */
	EHOSTUNREACH,		/* srcfail */
	EHOSTUNREACH,		/* netunknown */
	EHOSTUNREACH,		/* hostunknown */
	EHOSTUNREACH,		/* isolated */
	ECONNREFUSED,		/* net admin. prohibited */
	ECONNREFUSED,		/* host admin. prohibited */
	EHOSTUNREACH,		/* tos net unreachable */
	EHOSTUNREACH,		/* tos host unreachable */
	0,			/* redirect net */
	0,			/* redirect host */
	0,			/* redirect tosnet */
	0,			/* redirect toshost */
	0,			/* time exceeded */
	0,			/* reassembly timeout */
	ENOPROTOOPT,		/* parameter problem */
	ENOPROTOOPT,		/* required option missing */
	0,			/* MTU changed */
	/* NB: this means that this error will only
	   get propagated by in_mtunotify(), which
	   doesn't bother to check. */
};

/*
 * SUBNETSARELOCAL determines where IP subnets are considered to be ``local''
 * or not.  This option is obsolete.
 */
#ifndef SUBNETSARELOCAL
#define	SUBNETSARELOCAL	1
#endif
int subnetsarelocal = SUBNETSARELOCAL;

#ifdef MTUDISC
/*
 * MTUTIMER1 is the number of minutes to wait after having incremented
 * the MTU estimate before trying again.  MTUTIMER2 is the number
 * of minutes to wait after having decremented the MTU estimate
 * before trying to increment it.
 */
#ifndef MTUTIMER1
#define MTUTIMER1 2
#endif
int in_mtutimer1 = MTUTIMER1;

#ifndef MTUTIMER2
#define MTUTIMER2 10
#endif
int in_mtutimer2 = MTUTIMER2;
#endif /* MTUDISC */

/*
 * and a zero in_addr to make some code happy...
 */
struct	in_addr zeroin_addr;

/*
 * ICMPPRINTFS enables some debugging printfs in ip_icmp.c.
 *
 * IPBROADCASTECHO controls whether ICMP Echo Reply packets are sent
 * in response to ICMP Echo packets which were addressed to a multicast
 * or broadcast address.
 *
 * IPMASKAGENT controls whether ICMP Mask Reply packets are sent.
 * It should only be enabled on the machine which is the authoritative
 * mask agent for a subnet.
 */
#ifdef ICMPPRINTFS
int	icmpprintfs = 0;
#endif

#ifndef IPBROADCASTECHO
#define IPBROADCASTECHO 0
#endif
int	ipbroadcastecho = IPBROADCASTECHO;

#ifndef IPMASKAGENT
#define IPMASKAGENT 0
#endif
int	ipmaskagent = IPMASKAGENT;

/*
 * ICMP statistics
 */
struct icmpstat icmpstat;

/*
 * Yet Another sockaddr_in filled in by various routines when convenient.
 */
struct sockaddr_in icmpmask = { 8, 0 };

/*
 * Print out TCP debugging messages on the console.
 */
#ifdef TCPDEBUG
int	tcpconsdebug = 0;
#endif

#include "tcp.h"
#include "tcp_fsm.h"
#include "tcp_seq.h"
#include "tcp_timer.h"
#include "tcp_var.h"
#include "tcpip.h"

/*
 * tcp_ttl is the default IP TTL for TCP segments.
 * tcp_mssdflt is the default max segment size.
 * tcp_rttdflt is the initial round trip time estimate when there is no RTT
 * in the route.
 */
int	tcp_ttl = TCP_TTL;
int 	tcp_mssdflt = TCP_MSS;
int 	tcp_rttdflt = TCPTV_SRTTDFLT / PR_SLOWHZ;

/*
 * When KPROF is defined (god only knows why), TCP keeps track of
 * protocol requests in this matrix.
 */
#ifdef KPROF
int	tcp_acounts[TCP_NSTATES][PRU_NREQ];
#endif

/*
 * tcp_keepidle is the a fraction of the length of non-response time in a 
 * in a keepalive situation after which TCP abandons the connection.
 *
 * tcp_keepintvl is the interval between keepalives.
 *
 * tcp_maxidle is the time after which a connection will be dropped in
 * certain states.  It is computed as `TCPTV_KEEPCNT * tcp_keepintvl'.
 */
int	tcp_keepidle = TCPTV_KEEP_IDLE;
int	tcp_keepintvl = TCPTV_KEEPINTVL;
int	tcp_maxidle;

/*
 * tcp_sendspace and tcp_recvspace are the default send and receive window
 * sizes, respectively.  These are obsolescent (this information should
 * be set by the route).
 */
#ifdef	TCP_SMALLSPACE
u_long	tcp_sendspace = 1024*4;
u_long	tcp_recvspace = 1024*4;
#else
u_long	tcp_sendspace = 1024*16;
u_long	tcp_recvspace = 1024*16;
#endif	/* TCP_SMALLSPACE */

#include "udp.h"
#include "udp_var.h"

/*
 * udpcksum tells whether to do UDP checksums.  It should always be
 * turned on, except as required for compatibility with ancient
 * 4.2-based systems like SunOS 3.5 and Ultrix 2.0.
 */
#ifndef	COMPAT_42
int	udpcksum = 1;
#else
int	udpcksum = 0;		/* XXX */
#endif


/*
 * udp_ttl is the default IP TTL for UDP packets.
 */
int	udp_ttl = UDP_TTL;

/*
 * UDP statistics for netstat.
 */
struct	udpstat udpstat;

/*
 * udp_sendspace is the maximum datagram size the UDP layer is willing to
 * attempt to transmit.
 *
 * udp_recvspace is the amount of buffer space the UDP layer will
 * reserve for holding received packets.
 */
u_long	udp_sendspace = 9216;		/* really max datagram size */
u_long	udp_recvspace = 40 * (1024 + sizeof(struct sockaddr_in));
					/* 40 1K datagrams */


