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
 *	@(#)ipx_if.h
 *
 * $FreeBSD$
 */

#ifndef _NETIPX_IPX_IF_H_
#define	_NETIPX_IPX_IF_H_

/*
 * Interface address.  One of these structures
 * is allocated for each interface with an internet address.
 * The ifaddr structure contains the protocol-independent part
 * of the structure and is assumed to be first.
 */

struct ipx_ifaddr {
	struct	ifaddr ia_ifa;		/* protocol-independent info */
#define	ia_ifp		ia_ifa.ifa_ifp
#define	ia_flags	ia_ifa.ifa_flags
	struct	ipx_ifaddr *ia_next;	/* next in list of ipx addresses */
	struct	sockaddr_ipx ia_addr;	/* reserve space for my address */
	struct	sockaddr_ipx ia_dstaddr;	/* space for my broadcast address */
#define ia_broadaddr	ia_dstaddr
	struct	sockaddr_ipx ia_netmask;	/* space for my network mask */
};

struct	ipx_aliasreq {
	char	ifra_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	struct	sockaddr_ipx ifra_addr;
	struct	sockaddr_ipx ifra_broadaddr;
#define ifra_dstaddr ifra_broadaddr
};
/*
 * Given a pointer to an ipx_ifaddr (ifaddr),
 * return a pointer to the addr as a sockadd_ipx.
 */

#define	IA_SIPX(ia) (&(((struct ipx_ifaddr *)(ia))->ia_addr))

/* This is not the right place for this but where is? */

#define ETHERTYPE_IPX_8022	0x00e0	/* Ethernet_802.2 */
#define ETHERTYPE_IPX_8023	0x0000	/* Ethernet_802.3 */
#define ETHERTYPE_IPX_II	0x8137	/* Ethernet_II */
#define ETHERTYPE_IPX_SNAP	0x8137	/* Ethernet_SNAP */

#define	ETHERTYPE_IPX		0x8137	/* Only  Ethernet_II Available */

#ifdef	IPXIP
struct ipxip_req {
	struct sockaddr rq_ipx;	/* must be ipx format destination */
	struct sockaddr rq_ip;	/* must be ip format gateway */
	short rq_flags;
};
#endif

#ifdef	_KERNEL
extern struct	ifqueue	ipxintrq;	/* IPX input packet queue */
extern struct	ipx_ifaddr *ipx_ifaddr;

struct ipx_ifaddr *ipx_iaonnetof(struct ipx_addr *dst);
#endif

#endif /* !_NETIPX_IPX_IF_H_ */
