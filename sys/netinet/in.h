/*
 * Copyright (c) 1982, 1986, 1990 Regents of the University of California.
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
 *	from: @(#)in.h	7.11 (Berkeley) 4/20/91
 *	$Id: in.h,v 1.8 1994/05/26 22:42:14 jkh Exp $
 */

#ifndef _NETINET_IN_H_
#define _NETINET_IN_H_

/*
 * Constants and structures defined by the internet system,
 * Per RFC 790, September 1981.
 */

/*
 * Protocols
 */
#define	IPPROTO_IP		0		/* dummy for IP */
#define	IPPROTO_ICMP		1		/* control message protocol */
#define	IPPROTO_IGMP		2		/* ground control protocol */
#define	IPPROTO_GGP		3		/* gateway^2 (deprecated) */
#define	IPPROTO_TCP		6		/* tcp */
#define	IPPROTO_EGP		8		/* exterior gateway protocol */
#define	IPPROTO_PUP		12		/* pup */
#define	IPPROTO_UDP		17		/* user datagram protocol */
#define	IPPROTO_IDP		22		/* xns idp */
#define	IPPROTO_TP		29 		/* tp-4 w/ class negotiation */
#define	IPPROTO_EON		80		/* ISO cnlp */

#define	IPPROTO_RAW		255		/* raw IP packet */
#define	IPPROTO_MAX		256


/*
 * Local port number conventions:
 * Ports < IPPORT_RESERVED are reserved for
 * privileged processes (e.g. root).
 * Ports > IPPORT_USERRESERVED are reserved
 * for servers, not necessarily privileged.
 */
#define	IPPORT_RESERVED		1024
#define	IPPORT_USERRESERVED	5000

/*
 * Internet address (a structure for historical reasons)
 */
struct in_addr {
	u_long s_addr;
};

/*
 * Definitions of bits in internet address integers.
 * On subnets, the decomposition of addresses to host and net parts
 * is done according to subnet mask, not the masks here.
 */
#define	IN_CLASSA(i)		(((u_long)(i) & 0x80000000UL) == 0)
#define	IN_CLASSA_NET		0xff000000UL
#define	IN_CLASSA_NSHIFT	24
#define	IN_CLASSA_HOST		0x00ffffffUL
#define	IN_CLASSA_MAX		128

#define	IN_CLASSB(i)		(((u_long)(i) & 0xc0000000UL) == 0x80000000UL)
#define	IN_CLASSB_NET		0xffff0000UL
#define	IN_CLASSB_NSHIFT	16
#define	IN_CLASSB_HOST		0x0000ffffUL
#define	IN_CLASSB_MAX		65536

#define	IN_CLASSC(i)		(((u_long)(i) & 0xe0000000UL) == 0xc0000000UL)
#define	IN_CLASSC_NET		0xffffff00UL
#define	IN_CLASSC_NSHIFT	8
#define	IN_CLASSC_HOST		0x000000ffUL

#define	IN_CLASSD(i)		(((u_long)(i) & 0xf0000000UL) == 0xe0000000UL)
#define	IN_CLASSD_NET		0xf0000000UL	/* These ones aren't really */
#define	IN_CLASSD_NSHIFT	28		/* net and host fields, bit */
#define	IN_CLASSD_HOST		0x0fffffffUL	/* routing needn't know. */
#define	IN_MULTICAST(i)		IN_CLASSD(i)

#define	IN_EXPERIMENTAL(i)	(((u_long)(i) & 0xe0000000UL) == 0xe0000000UL)
#define	IN_BADCLASS(i)		(((u_long)(i) & 0xf0000000UL) == 0xf0000000UL)

#define	INADDR_ANY		0x00000000UL
#ifndef INADDR_LOOPBACK
#define	INADDR_LOOPBACK		0x7f000001UL
#endif
#define	INADDR_BROADCAST	0xffffffffUL	/* must be masked */
#ifndef KERNEL
#define	INADDR_NONE		0xffffffffUL		/* -1 return */
#endif

#define	INADDR_UNSPEC_GROUP	0xe0000000UL	/* 224.0.0.0 */
#define	INADDR_ALLHOSTS_GROUP	0xe0000001UL	/* 244.0.0.1 */
#define	INADDR_MAX_LOCAL_GROUP	0xe00000ffUL	/* 244.0.0.255 */

#define	IN_LOOPBACKNET		127			/* official! */

/*
 * Define a macro to stuff the loopback address into an Internet address.
 */
#define	IN_SET_LOOPBACK_ADDR(a) { \
	(a)->sin_addr.s_addr = htonl(INADDR_LOOPBACK); \
	(a)->sin_family = AF_INET; }

/*
 * Socket address, internet style.
 */
struct sockaddr_in {
	u_char	sin_len;
	u_char	sin_family;
	u_short	sin_port;
	struct	in_addr sin_addr;
	char	sin_zero[8];
};

/*
 * Structure used to describe IP options.
 * Used to store options internally, to pass them to a process,
 * or to restore options retrieved earlier.
 * The ip_dst is used for the first-hop gateway when using a source route
 * (this gets put into the header proper).
 */
struct ip_opts {
	struct	in_addr ip_dst;		/* first hop, 0 w/o src rt */
	char	ip_opts[40];		/* actually variable in size */
};

/*
 * Options for use with [gs]etsockopt at the IP level.
 * First word of comment is data type; bool is stored in int.
 */
#define	IP_OPTIONS	1	/* buf/ip_opts; set/get IP per-packet options */
#define	IP_HDRINCL	2	/* int; header is included with data (raw) */
#define	IP_TOS		3	/* int; IP type of service and precedence */
#define	IP_TTL		4	/* int; IP time to live */
#define	IP_RECVOPTS	5	/* bool; receive all IP options w/datagram */
#define	IP_RECVRETOPTS	6	/* bool; receive IP options for response */
#define	IP_RECVDSTADDR	7	/* bool; receive IP dst addr w/datagram */
#define	IP_RETOPTS	8	/* ip_opts; set/get IP per-packet options */
#define	IP_MULTICAST_IF	9	/* set/get IP multicast interfcae */
#define	IP_MULTICAST_TTL 10	/* set/get IP multicast timetolive */
#define	IP_MULTICAST_LOOP 11	/* set/get IP m'cast loopback */
#define	IP_ADD_MEMBERSHIP 12	/* add an IP group membership */
#define	IP_DROP_MEMBERSHIP 13	/* drop an IP group membership */

#define	IP_DEFAULT_MULTICAST_TTL 1	/* normally limit m'casts to 1 hop */
#define	IP_DEFAULT_MULTICAST_LOOP 1	/* normally hear sens if a member */
#define	IP_MAX_MEMBERSHIPS	20	/* per socket; must fit in one mbuf */

/*
 * Argument structure for IP_ADD_MEMBERSHIP and IP_DROP_MEMBERSHIP.
 */
struct	ip_mreq {
	struct in_addr	imr_multiaddr;	/* IP multicast address of group */
	struct in_addr	imr_interface;	/* local IP address of interface */
} ;

#ifdef KERNEL
/* From in.c: */
extern struct in_addr in_makeaddr(u_long, u_long);
extern u_long in_netof(struct in_addr);
extern void in_sockmaskof(struct in_addr, struct sockaddr_in *);
extern u_long in_lnaof(struct in_addr);
extern int in_localaddr(struct in_addr);
extern int in_canforward(struct in_addr);
struct socket; struct ifnet;
extern int in_control(struct socket *, int, caddr_t, struct ifnet *);
struct in_ifaddr;
extern int in_broadcast(struct in_addr);

#endif /* KERNEL */

#endif	/* _NETINET_IN_H_ */
