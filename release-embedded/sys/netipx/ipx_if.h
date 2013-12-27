/*-
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

 * Copyright (c) 1995, Mike Mitchell
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
#ifdef _KERNEL
struct ipx_ifaddr {
	struct	ifaddr ia_ifa;		/* protocol-independent info */
#define	ia_ifp		ia_ifa.ifa_ifp
#define	ia_flags	ia_ifa.ifa_flags
	TAILQ_ENTRY(ipx_ifaddr)	ia_link;	/* list of IPv6 addresses */
	struct	sockaddr_ipx ia_addr;	/* reserve space for my address */
	struct	sockaddr_ipx ia_dstaddr;	/* space for my broadcast address */
#define ia_broadaddr	ia_dstaddr
	struct	sockaddr_ipx ia_netmask;	/* space for my network mask */
};
#endif /* _KERNEL */

struct	ipx_aliasreq {
	char	ifra_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	struct	sockaddr_ipx ifra_addr;
	struct	sockaddr_ipx ifra_broadaddr;
#define ifra_dstaddr ifra_broadaddr
};

/*
 * List of ipx_ifaddr's.
 */
TAILQ_HEAD(ipx_ifaddrhead, ipx_ifaddr);

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

#ifdef	_KERNEL
extern struct rwlock		 ipx_ifaddr_rw;
extern struct ipx_ifaddrhead	 ipx_ifaddrhead;

#define	IPX_IFADDR_LOCK_INIT()		rw_init(&ipx_ifaddr_rw, "ipx_ifaddr_rw")
#define	IPX_IFADDR_LOCK_ASSERT()	rw_assert(&ipx_ifaddr_rw, RA_LOCKED)
#define	IPX_IFADDR_RLOCK()		rw_rlock(&ipx_ifaddr_rw)
#define	IPX_IFADDR_RUNLOCK()		rw_runlock(&ipx_ifaddr_rw)
#define	IPX_IFADDR_WLOCK()		rw_wlock(&ipx_ifaddr_rw)
#define	IPX_IFADDR_WUNLOCK()		rw_wunlock(&ipx_ifaddr_rw)
#define	IPX_IFADDR_RLOCK_ASSERT()	rw_assert(&ipx_ifaddr_rw, RA_WLOCKED)

struct ipx_ifaddr	*ipx_iaonnetof(struct ipx_addr *dst);
#endif

#endif /* !_NETIPX_IPX_IF_H_ */
