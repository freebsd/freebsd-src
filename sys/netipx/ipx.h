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
 *	@(#)$Id$
 */

#ifndef _NETIPX_IPX_H_
#define _NETIPX_IPX_H_

/*
 * Constants and Structures
 */

/*
 * Protocols
 */
#define IPXPROTO_UNKWN	0		/* Unknown */
#define IPXPROTO_RI	1		/* RIP Routing Information */
#define IPXPROTO_ECHO	2		/* Echo Protocol */
#define IPXPROTO_ERROR	3		/* Error Protocol */
#define IPXPROTO_PXP	4		/* PXP Packet Exchange */
#define IPXPROTO_SPX	5		/* SPX Sequenced Packet */
#define IPXPROTO_NCP	17		/* NCP NetWare Core */
#define IPXPROTO_RAW	255		/* Placemarker*/
#define IPXPROTO_MAX	256		/* Placemarker*/

/*
 * Port/Socket numbers: network standard functions
 */

#define IPXPORT_RI	1		/* NS RIP Routing Information */
#define IPXPORT_ECHO	2		/* NS Echo */
#define IPXPORT_RE	3		/* NS Router Error */
#define IPXPORT_FSP	0x0451		/* NW FSP File Service */
#define IPXPORT_SAP	0x0452		/* NW SAP Service Advertising */
#define IPXPORT_RIP	0x0453		/* NW RIP Routing Information */
#define IPXPORT_NETBIOS	0x0455		/* NW NetBIOS */
#define IPXPORT_DIAGS	0x0456		/* NW Diagnostics */
#define IPXPORT_WDOG	0x4001		/* NW Watchdog Packets */
#define IPXPORT_SHELL	0x4003		/* NW Shell Socket */
#define IPXPORT_MAX	0x8000		/* Maximum User Addressable Port */

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
 * Definitions for IPX Internet Datagram Protocol
 */
struct ipx {
	u_short	ipx_sum;	/* Checksum */
	u_short	ipx_len;	/* Length, in bytes, including header */
	u_char	ipx_tc;		/* Transport Crontrol (i.e. hop count) */
	u_char	ipx_pt;		/* Packet Type (i.e. level 2 protocol) */
	struct ipx_addr	ipx_dna;	/* Destination Network Address */
	struct ipx_addr	ipx_sna;	/* Source Network Address */
};

#ifdef vax
#define ipx_netof(a) (*(long *) & ((a).x_net)) /* XXX - not needed */
#endif
#define ipx_neteqnn(a,b) \
	(((a).s_net[0]==(b).s_net[0]) && ((a).s_net[1]==(b).s_net[1]))
#define ipx_neteq(a,b) ipx_neteqnn((a).x_net, (b).x_net)
#define satoipx_addr(sa) (((struct sockaddr_ipx *)&(sa))->sipx_addr)
#define ipx_hosteqnh(s,t) ((s).s_host[0] == (t).s_host[0] && \
	(s).s_host[1] == (t).s_host[1] && (s).s_host[2] == (t).s_host[2])
#define ipx_hosteq(s,t) (ipx_hosteqnh((s).x_host,(t).x_host))
#define ipx_nullnet(x) (((x).x_net.s_net[0]==0) && ((x).x_net.s_net[1]==0))
#define ipx_nullhost(x) (((x).x_host.s_host[0]==0) && \
	((x).x_host.s_host[1]==0) && ((x).x_host.s_host[2]==0))
#define ipx_wildnet(x) (((x).x_net.s_net[0]==0xffff) && \
	((x).x_net.s_net[1]==0xffff))
#define ipx_wildhost(x) (((x).x_host.s_host[0]==0xffff) && \
	((x).x_host.s_host[1]==0xffff) && ((x).x_host.s_host[2]==0xffff))

#ifdef KERNEL

extern int ipxcksum;
extern struct domain ipxdomain;
extern struct sockaddr_ipx ipx_netmask;
extern struct sockaddr_ipx ipx_hostmask;

extern union ipx_host ipx_thishost;
extern union ipx_net ipx_zeronet;
extern union ipx_host ipx_zerohost;
extern union ipx_net ipx_broadnet;
extern union ipx_host ipx_broadhost;

extern long ipx_pexseq;
extern u_char ipxctlerrmap[];
extern struct ipxpcb ipxrawpcb;

#include <net/if.h>
#include <net/route.h>

#include <sys/cdefs.h>

__BEGIN_DECLS
u_short ipx_cksum __P((struct mbuf *m, int len));
void ipx_input __P((struct mbuf *m, struct ipxpcb *ipxp));
void ipx_abort __P((struct ipxpcb *ipxp));
void ipx_drop __P((struct ipxpcb *ipxp, int errno));
int ipx_output __P((struct ipxpcb *ipxp, struct mbuf *m0));
int ipx_ctloutput __P((int req, struct socket *so, int level, int name, struct mbuf **value));
int ipx_usrreq __P((struct socket *so, int req, struct mbuf *m, struct mbuf *nam, struct mbuf *control));
int ipx_raw_usrreq __P((struct socket *so, int req, struct mbuf *m, struct mbuf *nam, struct mbuf *control));
int ipx_control __P((struct socket *so, int cmd, caddr_t data, struct ifnet *ifp));
void ipx_init __P((void));
void ipxintr __P((void));
void ipx_ctlinput __P((int cmd, caddr_t arg));
void ipx_forward __P((struct mbuf *m));
void ipx_watch_output __P((struct mbuf *m, struct ifnet *ifp));
int ipx_do_route __P((struct ipx_addr *src, struct route *ro));
void ipx_undo_route __P((struct route *ro));
int ipx_outputfl __P((struct mbuf *m0, struct route *ro, int flags));
__END_DECLS

#else

#include <sys/cdefs.h>

__BEGIN_DECLS
extern struct ipx_addr ipx_addr __P((const char *));
extern char *ipx_ntoa __P((struct ipx_addr));
__END_DECLS

#endif

#endif
