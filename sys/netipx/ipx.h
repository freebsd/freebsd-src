/*
 * Copyright (c) 1995, Mike Mitchell
 * Copyright (c) 1984, 1985, 1986, 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)ipx.h
 *
 * $Id: ipx.h,v 1.7 1996/01/30 22:58:48 mpp Exp $
 */

#ifndef _NETIPX_IPX_H_
#define	_NETIPX_IPX_H_

/*
 * Constants and Structures
 */

/*
 * Protocols
 */
#define IPXPROTO_UNKWN		0	/* Unknown */
#define IPXPROTO_RI		1	/* RIP Routing Information */
#define IPXPROTO_PXP		4	/* IPX Packet Exchange Protocol */
#define IPXPROTO_SPX		5	/* SPX Sequenced Packet */
#define IPXPROTO_NCP		17	/* NCP NetWare Core */
#define IPXPROTO_NETBIOS	20	/* Propagated Packet */
#define IPXPROTO_RAW		255	/* Placemarker*/
#define IPXPROTO_MAX		256	/* Placemarker*/

/*
 * Port/Socket numbers: network standard functions
 */

#define IPXPORT_RI		1	/* NS RIP Routing Information */
#define IPXPORT_ECHO		2	/* NS Echo */
#define IPXPORT_RE		3	/* NS Router Error */
#define IPXPORT_NCP		0x0451	/* NW NCP Core Protocol */
#define IPXPORT_SAP		0x0452	/* NW SAP Service Advertising */
#define IPXPORT_RIP		0x0453	/* NW RIP Routing Information */
#define IPXPORT_NETBIOS		0x0455	/* NW NetBIOS */
#define IPXPORT_DIAGS		0x0456	/* NW Diagnostics */
/*
 * Ports < IPXPORT_RESERVED are reserved for privileged
 */
#define IPXPORT_RESERVED	0x4000
/*
 * Ports > IPXPORT_WELLKNOWN are reserved for privileged
 * processes (e.g. root).
 */
#define IPXPORT_WELLKNOWN	0x6000

/* flags passed to ipx_outputfl as last parameter */

#define	IPX_FORWARDING		0x1	/* most of ipx header exists */
#define	IPX_ROUTETOIF		0x10	/* same as SO_DONTROUTE */
#define	IPX_ALLOWBROADCAST	SO_BROADCAST	/* can send broadcast packets */

#define IPX_MAXHOPS		15

/* flags passed to get/set socket option */
#define	SO_HEADERS_ON_INPUT	1
#define	SO_HEADERS_ON_OUTPUT	2
#define	SO_DEFAULT_HEADERS	3
#define	SO_LAST_HEADER		4
#define	SO_IPXIP_ROUTE		5
#define SO_SEQNO		6
#define	SO_ALL_PACKETS		7
#define SO_MTU			8
#define SO_IPXTUN_ROUTE		9

/*
 * IPX addressing
 */
union ipx_host {
	u_char	c_host[6];
	u_short	s_host[3];
};

union ipx_net {
	u_char	c_net[4];
	u_short	s_net[2];
};

union ipx_net_u {
	union	ipx_net	net_e;
	u_long		long_e;
};

struct ipx_addr {
	union ipx_net	x_net;
	union ipx_host	x_host;
	u_short		x_port;
};

/*
 * Socket address
 */
struct sockaddr_ipx {
	u_char		sipx_len;
	u_char		sipx_family;
	struct ipx_addr	sipx_addr;
	char		sipx_zero[2];
};
#define sipx_port sipx_addr.x_port

/*
 * Definitions for IPX Internetwork Packet Exchange Protocol
 */
struct ipx {
	u_short	ipx_sum;	/* Checksum */
	u_short	ipx_len;	/* Length, in bytes, including header */
	u_char	ipx_tc;		/* Transport Control (i.e. hop count) */
	u_char	ipx_pt;		/* Packet Type (i.e. level 2 protocol) */
	struct ipx_addr	ipx_dna;	/* Destination Network Address */
	struct ipx_addr	ipx_sna;	/* Source Network Address */
};

#ifdef vax
#define ipx_netof(a) (*(long *) & ((a).x_net)) /* XXX - not needed */
#endif
#define ipx_neteqnn(a,b) \
	(((a).s_net[0] == (b).s_net[0]) && ((a).s_net[1] == (b).s_net[1]))
#define ipx_neteq(a,b) ipx_neteqnn((a).x_net, (b).x_net)
#define satoipx_addr(sa) (((struct sockaddr_ipx *)&(sa))->sipx_addr)
#define ipx_hosteqnh(s,t) ((s).s_host[0] == (t).s_host[0] && \
	(s).s_host[1] == (t).s_host[1] && (s).s_host[2] == (t).s_host[2])
#define ipx_hosteq(s,t) (ipx_hosteqnh((s).x_host,(t).x_host))
#define ipx_nullnet(x) (((x).x_net.s_net[0]==0) && ((x).x_net.s_net[1]==0))
#define ipx_nullhost(x) (((x).x_host.s_host[0] == 0) && \
	((x).x_host.s_host[1] == 0) && ((x).x_host.s_host[2] == 0))
#define ipx_wildnet(x) (((x).x_net.s_net[0] == 0xffff) && \
	((x).x_net.s_net[1] == 0xffff))
#define ipx_wildhost(x) (((x).x_host.s_host[0] == 0xffff) && \
	((x).x_host.s_host[1] == 0xffff) && ((x).x_host.s_host[2] == 0xffff))

#include <sys/cdefs.h>

__BEGIN_DECLS
struct	ipx_addr ipx_addr __P((const char *));
char	*ipx_ntoa __P((struct ipx_addr));
__END_DECLS

#endif /* !_NETIPX_IPX_H_ */
