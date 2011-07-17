/*	$NetBSD: if_arcsubr.c,v 1.36 2001/06/14 05:44:23 itojun Exp $	*/
/*	$FreeBSD$ */

/*-
 * Copyright (c) 1994, 1995 Ignatios Souvatzis
 * Copyright (c) 1982, 1989, 1993
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
 * from: NetBSD: if_ethersubr.c,v 1.9 1994/06/29 06:36:11 cgd Exp
 *       @(#)if_ethersubr.c	8.1 (Berkeley) 6/10/93
 *
 */
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipx.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/errno.h>
#include <sys/syslog.h>

#include <machine/cpu.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_arc.h>
#include <net/if_arp.h>
#include <net/bpf.h>
#include <net/if_llatbl.h>

#if defined(INET) || defined(INET6)
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#endif

#ifdef INET6
#include <netinet6/nd6.h>
#endif

#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif

#define ARCNET_ALLOW_BROKEN_ARP

static struct mbuf *arc_defrag(struct ifnet *, struct mbuf *);
static int arc_resolvemulti(struct ifnet *, struct sockaddr **,
			    struct sockaddr *);

u_int8_t  arcbroadcastaddr = 0;

#define ARC_LLADDR(ifp)	(*(u_int8_t *)IF_LLADDR(ifp))

#define senderr(e) { error = (e); goto bad;}
#define SIN(s)	((struct sockaddr_in *)s)
#define SIPX(s)	((struct sockaddr_ipx *)s)

/*
 * ARCnet output routine.
 * Encapsulate a packet of type family for the local net.
 * Assumes that ifp is actually pointer to arccom structure.
 */
int
arc_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
    struct route *ro)
{
	struct arc_header	*ah;
	int			error;
	u_int8_t		atype, adst;
	int			loop_copy = 0;
	int			isphds;
#if defined(INET) || defined(INET6)
	struct llentry		*lle;
#endif

	if (!((ifp->if_flags & IFF_UP) &&
	    (ifp->if_drv_flags & IFF_DRV_RUNNING)))
		return(ENETDOWN); /* m, m1 aren't initialized yet */

	error = 0;

	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:

		/*
		 * For now, use the simple IP addr -> ARCnet addr mapping
		 */
		if (m->m_flags & (M_BCAST|M_MCAST))
			adst = arcbroadcastaddr; /* ARCnet broadcast address */
		else if (ifp->if_flags & IFF_NOARP)
			adst = ntohl(SIN(dst)->sin_addr.s_addr) & 0xFF;
		else {
			error = arpresolve(ifp, ro ? ro->ro_rt : NULL,
			                   m, dst, &adst, &lle);
			if (error)
				return (error == EWOULDBLOCK ? 0 : error);
		}

		atype = (ifp->if_flags & IFF_LINK0) ?
			ARCTYPE_IP_OLD : ARCTYPE_IP;
		break;
	case AF_ARP:
	{
		struct arphdr *ah;
		ah = mtod(m, struct arphdr *);
		ah->ar_hrd = htons(ARPHRD_ARCNET);

		loop_copy = -1; /* if this is for us, don't do it */

		switch(ntohs(ah->ar_op)) {
		case ARPOP_REVREQUEST:
		case ARPOP_REVREPLY:
			atype = ARCTYPE_REVARP;
			break;
		case ARPOP_REQUEST:
		case ARPOP_REPLY:
		default:
			atype = ARCTYPE_ARP;
			break;
		}

		if (m->m_flags & M_BCAST)
			bcopy(ifp->if_broadcastaddr, &adst, ARC_ADDR_LEN);
		else
			bcopy(ar_tha(ah), &adst, ARC_ADDR_LEN);
        
	}
	break;
#endif
#ifdef INET6
	case AF_INET6:
		error = nd6_storelladdr(ifp, m, dst, (u_char *)&adst, &lle);
		if (error)
			return (error);
		atype = ARCTYPE_INET6;
		break;
#endif
#ifdef IPX
	case AF_IPX:
		adst = SIPX(dst)->sipx_addr.x_host.c_host[5];
		atype = ARCTYPE_IPX;
		if (adst == 0xff)
			adst = arcbroadcastaddr;
		break;
#endif

	case AF_UNSPEC:
		loop_copy = -1;
		ah = (struct arc_header *)dst->sa_data;
		adst = ah->arc_dhost;
		atype = ah->arc_type;

		if (atype == ARCTYPE_ARP) {
			atype = (ifp->if_flags & IFF_LINK0) ?
			    ARCTYPE_ARP_OLD: ARCTYPE_ARP;

#ifdef ARCNET_ALLOW_BROKEN_ARP
			/*
			 * XXX It's not clear per RFC826 if this is needed, but
			 * "assigned numbers" say this is wrong.
			 * However, e.g., AmiTCP 3.0Beta used it... we make this
			 * switchable for emergency cases. Not perfect, but...
			 */
			if (ifp->if_flags & IFF_LINK2)
				mtod(m, struct arphdr *)->ar_pro = atype - 1;
#endif
		}
		break;

	default:
		if_printf(ifp, "can't handle af%d\n", dst->sa_family);
		senderr(EAFNOSUPPORT);
	}

	isphds = arc_isphds(atype);
	M_PREPEND(m, isphds ? ARC_HDRNEWLEN : ARC_HDRLEN, M_DONTWAIT);
	if (m == 0)
		senderr(ENOBUFS);
	ah = mtod(m, struct arc_header *);
	ah->arc_type = atype;
	ah->arc_dhost = adst;
	ah->arc_shost = ARC_LLADDR(ifp);
	if (isphds) {
		ah->arc_flag = 0;
		ah->arc_seqid = 0;
	}

	if ((ifp->if_flags & IFF_SIMPLEX) && (loop_copy != -1)) {
		if ((m->m_flags & M_BCAST) || (loop_copy > 0)) {
			struct mbuf *n = m_copy(m, 0, (int)M_COPYALL);

			(void) if_simloop(ifp, n, dst->sa_family, ARC_HDRLEN);
		} else if (ah->arc_dhost == ah->arc_shost) {
			(void) if_simloop(ifp, m, dst->sa_family, ARC_HDRLEN);
			return (0);     /* XXX */
		}
	}

	BPF_MTAP(ifp, m);

	error = ifp->if_transmit(ifp, m);

	return (error);

bad:
	if (m)
		m_freem(m);
	return (error);
}

void
arc_frag_init(struct ifnet *ifp)
{
	struct arccom *ac;

	ac = (struct arccom *)ifp->if_l2com;
	ac->curr_frag = 0;
}

struct mbuf *
arc_frag_next(struct ifnet *ifp)
{
	struct arccom *ac;
	struct mbuf *m;
	struct arc_header *ah;

	ac = (struct arccom *)ifp->if_l2com;
	if ((m = ac->curr_frag) == 0) {
		int tfrags;

		/* dequeue new packet */
		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == 0)
			return 0;

		ah = mtod(m, struct arc_header *);
		if (!arc_isphds(ah->arc_type))
			return m;

		++ac->ac_seqid;		/* make the seqid unique */
		tfrags = (m->m_pkthdr.len + ARC_MAX_DATA - 1) / ARC_MAX_DATA;
		ac->fsflag = 2 * tfrags - 3;
		ac->sflag = 0;
		ac->rsflag = ac->fsflag;
		ac->arc_dhost = ah->arc_dhost;
		ac->arc_shost = ah->arc_shost;
		ac->arc_type = ah->arc_type;

		m_adj(m, ARC_HDRNEWLEN);
		ac->curr_frag = m;
	}

	/* split out next fragment and return it */
	if (ac->sflag < ac->fsflag) {
		/* we CAN'T have short packets here */
		ac->curr_frag = m_split(m, ARC_MAX_DATA, M_DONTWAIT);
		if (ac->curr_frag == 0) {
			m_freem(m);
			return 0;
		}

		M_PREPEND(m, ARC_HDRNEWLEN, M_DONTWAIT);
		if (m == 0) {
			m_freem(ac->curr_frag);
			ac->curr_frag = 0;
			return 0;
		}

		ah = mtod(m, struct arc_header *);
		ah->arc_flag = ac->rsflag;
		ah->arc_seqid = ac->ac_seqid;

		ac->sflag += 2;
		ac->rsflag = ac->sflag;
	} else if ((m->m_pkthdr.len >=
	    ARC_MIN_FORBID_LEN - ARC_HDRNEWLEN + 2) &&
	    (m->m_pkthdr.len <=
	    ARC_MAX_FORBID_LEN - ARC_HDRNEWLEN + 2)) {
		ac->curr_frag = 0;

		M_PREPEND(m, ARC_HDRNEWLEN_EXC, M_DONTWAIT);
		if (m == 0)
			return 0;

		ah = mtod(m, struct arc_header *);
		ah->arc_flag = 0xFF;
		ah->arc_seqid = 0xFFFF;
		ah->arc_type2 = ac->arc_type;
		ah->arc_flag2 = ac->sflag;
		ah->arc_seqid2 = ac->ac_seqid;
	} else {
		ac->curr_frag = 0;

		M_PREPEND(m, ARC_HDRNEWLEN, M_DONTWAIT);
		if (m == 0)
			return 0;

		ah = mtod(m, struct arc_header *);
		ah->arc_flag = ac->sflag;
		ah->arc_seqid = ac->ac_seqid;
	}

	ah->arc_dhost = ac->arc_dhost;
	ah->arc_shost = ac->arc_shost;
	ah->arc_type = ac->arc_type;

	return m;
}

/*
 * Defragmenter. Returns mbuf if last packet found, else
 * NULL. frees imcoming mbuf as necessary.
 */

static __inline struct mbuf *
arc_defrag(struct ifnet *ifp, struct mbuf *m)
{
	struct arc_header *ah, *ah1;
	struct arccom *ac;
	struct ac_frag *af;
	struct mbuf *m1;
	char *s;
	int newflen;
	u_char src,dst,typ;

	ac = (struct arccom *)ifp->if_l2com;

	if (m->m_len < ARC_HDRNEWLEN) {
		m = m_pullup(m, ARC_HDRNEWLEN);
		if (m == NULL) {
			++ifp->if_ierrors;
			return NULL;
		}
	}

	ah = mtod(m, struct arc_header *);
	typ = ah->arc_type;

	if (!arc_isphds(typ))
		return m;

	src = ah->arc_shost;
	dst = ah->arc_dhost;

	if (ah->arc_flag == 0xff) {
		m_adj(m, 4);

		if (m->m_len < ARC_HDRNEWLEN) {
			m = m_pullup(m, ARC_HDRNEWLEN);
			if (m == NULL) {
				++ifp->if_ierrors;
				return NULL;
			}
		}

		ah = mtod(m, struct arc_header *);
	}

	af = &ac->ac_fragtab[src];
	m1 = af->af_packet;
	s = "debug code error";

	if (ah->arc_flag & 1) {
		/*
		 * first fragment. We always initialize, which is
		 * about the right thing to do, as we only want to
		 * accept one fragmented packet per src at a time.
		 */
		if (m1 != NULL)
			m_freem(m1);

		af->af_packet = m;
		m1 = m;
		af->af_maxflag = ah->arc_flag;
		af->af_lastseen = 0;
		af->af_seqid = ah->arc_seqid;

		return NULL;
		/* notreached */
	} else {
		/* check for unfragmented packet */
		if (ah->arc_flag == 0)
			return m;

		/* do we have a first packet from that src? */
		if (m1 == NULL) {
			s = "no first frag";
			goto outofseq;
		}

		ah1 = mtod(m1, struct arc_header *);

		if (ah->arc_seqid != ah1->arc_seqid) {
			s = "seqid differs";
			goto outofseq;
		}

		if (typ != ah1->arc_type) {
			s = "type differs";
			goto outofseq;
		}

		if (dst != ah1->arc_dhost) {
			s = "dest host differs";
			goto outofseq;
		}

		/* typ, seqid and dst are ok here. */

		if (ah->arc_flag == af->af_lastseen) {
			m_freem(m);
			return NULL;
		}

		if (ah->arc_flag == af->af_lastseen + 2) {
			/* ok, this is next fragment */
			af->af_lastseen = ah->arc_flag;
			m_adj(m,ARC_HDRNEWLEN);

			/*
			 * m_cat might free the first mbuf (with pkthdr)
			 * in 2nd chain; therefore:
			 */

			newflen = m->m_pkthdr.len;

			m_cat(m1,m);

			m1->m_pkthdr.len += newflen;

			/* is it the last one? */
			if (af->af_lastseen > af->af_maxflag) {
				af->af_packet = NULL;
				return(m1);
			} else
				return NULL;
		}
		s = "other reason";
		/* if all else fails, it is out of sequence, too */
	}
outofseq:
	if (m1) {
		m_freem(m1);
		af->af_packet = NULL;
	}

	if (m)
		m_freem(m);

	log(LOG_INFO,"%s: got out of seq. packet: %s\n",
	    ifp->if_xname, s);

	return NULL;
}

/*
 * return 1 if Packet Header Definition Standard, else 0.
 * For now: old IP, old ARP aren't obviously. Lacking correct information,
 * we guess that besides new IP and new ARP also IPX and APPLETALK are PHDS.
 * (Apple and Novell corporations were involved, among others, in PHDS work).
 * Easiest is to assume that everybody else uses that, too.
 */
int
arc_isphds(u_int8_t type)
{
	return (type != ARCTYPE_IP_OLD &&
		type != ARCTYPE_ARP_OLD &&
		type != ARCTYPE_DIAGNOSE);
}

/*
 * Process a received Arcnet packet;
 * the packet is in the mbuf chain m with
 * the ARCnet header.
 */
void
arc_input(struct ifnet *ifp, struct mbuf *m)
{
	struct arc_header *ah;
	int isr;
	u_int8_t atype;

	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}

	/* possibly defragment: */
	m = arc_defrag(ifp, m);
	if (m == NULL)
		return;

	BPF_MTAP(ifp, m);

	ah = mtod(m, struct arc_header *);
	/* does this belong to us? */
	if ((ifp->if_flags & IFF_PROMISC) == 0
	    && ah->arc_dhost != arcbroadcastaddr
	    && ah->arc_dhost != ARC_LLADDR(ifp)) {
		m_freem(m);
		return;
	}

	ifp->if_ibytes += m->m_pkthdr.len;

	if (ah->arc_dhost == arcbroadcastaddr) {
		m->m_flags |= M_BCAST|M_MCAST;
		ifp->if_imcasts++;
	}

	atype = ah->arc_type;
	switch (atype) {
#ifdef INET
	case ARCTYPE_IP:
		m_adj(m, ARC_HDRNEWLEN);
		if ((m = ip_fastforward(m)) == NULL)
			return;
		isr = NETISR_IP;
		break;

	case ARCTYPE_IP_OLD:
		m_adj(m, ARC_HDRLEN);
		if ((m = ip_fastforward(m)) == NULL)
			return;
		isr = NETISR_IP;
		break;

	case ARCTYPE_ARP:
		if (ifp->if_flags & IFF_NOARP) {
			/* Discard packet if ARP is disabled on interface */
			m_freem(m);
			return;
		}
		m_adj(m, ARC_HDRNEWLEN);
		isr = NETISR_ARP;
#ifdef ARCNET_ALLOW_BROKEN_ARP
		mtod(m, struct arphdr *)->ar_pro = htons(ETHERTYPE_IP);
#endif
		break;

	case ARCTYPE_ARP_OLD:
		if (ifp->if_flags & IFF_NOARP) {
			/* Discard packet if ARP is disabled on interface */
			m_freem(m);
			return;
		}
		m_adj(m, ARC_HDRLEN);
		isr = NETISR_ARP;
#ifdef ARCNET_ALLOW_BROKEN_ARP
		mtod(m, struct arphdr *)->ar_pro = htons(ETHERTYPE_IP);
#endif
		break;
#endif
#ifdef INET6
	case ARCTYPE_INET6:
		m_adj(m, ARC_HDRNEWLEN);
		isr = NETISR_IPV6;
		break;
#endif
#ifdef IPX
	case ARCTYPE_IPX:
		m_adj(m, ARC_HDRNEWLEN);
		isr = NETISR_IPX;
		break;
#endif
	default:
		m_freem(m);
		return;
	}
	M_SETFIB(m, ifp->if_fib);
	netisr_dispatch(isr, m);
}

/*
 * Register (new) link level address.
 */
void
arc_storelladdr(struct ifnet *ifp, u_int8_t lla)
{
	ARC_LLADDR(ifp) = lla;
}

/*
 * Perform common duties while attaching to interface list
 */
void
arc_ifattach(struct ifnet *ifp, u_int8_t lla)
{
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;
	struct arccom *ac;

	if_attach(ifp);
	ifp->if_addrlen = 1;
	ifp->if_hdrlen = ARC_HDRLEN;
	ifp->if_mtu = 1500;
	ifp->if_resolvemulti = arc_resolvemulti;
	if (ifp->if_baudrate == 0)
		ifp->if_baudrate = 2500000;
#if __FreeBSD_version < 500000
	ifa = ifnet_addrs[ifp->if_index - 1];
#else
	ifa = ifp->if_addr;
#endif
	KASSERT(ifa != NULL, ("%s: no lladdr!\n", __func__));
	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	sdl->sdl_type = IFT_ARCNET;
	sdl->sdl_alen = ifp->if_addrlen;

	if (ifp->if_flags & IFF_BROADCAST)
		ifp->if_flags |= IFF_MULTICAST|IFF_ALLMULTI;

	ac = (struct arccom *)ifp->if_l2com;
	ac->ac_seqid = (time_second) & 0xFFFF; /* try to make seqid unique */
	if (lla == 0) {
		/* XXX this message isn't entirely clear, to me -- cgd */
		log(LOG_ERR,"%s: link address 0 reserved for broadcasts.  Please change it and ifconfig %s down up\n",
		   ifp->if_xname, ifp->if_xname);
	}
	arc_storelladdr(ifp, lla);

	ifp->if_broadcastaddr = &arcbroadcastaddr;

	bpfattach(ifp, DLT_ARCNET, ARC_HDRLEN);
}

void
arc_ifdetach(struct ifnet *ifp)
{
	bpfdetach(ifp);
	if_detach(ifp);
}

int
arc_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct ifaddr *ifa = (struct ifaddr *) data;
	struct ifreq *ifr = (struct ifreq *) data;
	int error = 0;

	switch (command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			ifp->if_init(ifp->if_softc);	/* before arpwhohas */
			arp_ifinit(ifp, ifa);
			break;
#endif
#ifdef IPX
		/*
		 * XXX This code is probably wrong
		 */
		case AF_IPX:
		{
			struct ipx_addr *ina = &(IA_SIPX(ifa)->sipx_addr);

			if (ipx_nullhost(*ina))
				ina->x_host.c_host[5] = ARC_LLADDR(ifp);
			else
				arc_storelladdr(ifp, ina->x_host.c_host[5]);

			/*
			 * Set new address
			 */
			ifp->if_init(ifp->if_softc);
			break;
		}
#endif
		default:
			ifp->if_init(ifp->if_softc);
			break;
		}
		break;

	case SIOCGIFADDR:
		{
			struct sockaddr *sa;

			sa = (struct sockaddr *) &ifr->ifr_data;
			*(u_int8_t *)sa->sa_data = ARC_LLADDR(ifp);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifr == NULL)
			error = EAFNOSUPPORT;
		else {
			switch (ifr->ifr_addr.sa_family) {
			case AF_INET:
			case AF_INET6:
				error = 0;
				break;
			default:
				error = EAFNOSUPPORT;
				break;
			}
		}
		break;

	case SIOCSIFMTU:
		/*
		 * Set the interface MTU.
		 * mtu can't be larger than ARCMTU for RFC1051
		 * and can't be larger than ARC_PHDS_MTU
		 */
		if (((ifp->if_flags & IFF_LINK0) && ifr->ifr_mtu > ARCMTU) ||
		    ifr->ifr_mtu > ARC_PHDS_MAXMTU)
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;
	}

	return (error);
}

/* based on ether_resolvemulti() */
int
arc_resolvemulti(struct ifnet *ifp, struct sockaddr **llsa,
    struct sockaddr *sa)
{
	struct sockaddr_dl *sdl;
#ifdef INET
	struct sockaddr_in *sin;
#endif
#ifdef INET6
	struct sockaddr_in6 *sin6;
#endif

	switch(sa->sa_family) {
	case AF_LINK:
		/*
		* No mapping needed. Just check that it's a valid MC address.
		*/
		sdl = (struct sockaddr_dl *)sa;
		if (*LLADDR(sdl) != arcbroadcastaddr)
			return EADDRNOTAVAIL;
		*llsa = 0;
		return 0;
#ifdef INET
	case AF_INET:
		sin = (struct sockaddr_in *)sa;
		if (!IN_MULTICAST(ntohl(sin->sin_addr.s_addr)))
			return EADDRNOTAVAIL;
		sdl = malloc(sizeof *sdl, M_IFMADDR,
		       M_NOWAIT | M_ZERO);
		if (sdl == NULL)
			return ENOMEM;
		sdl->sdl_len = sizeof *sdl;
		sdl->sdl_family = AF_LINK;
		sdl->sdl_index = ifp->if_index;
		sdl->sdl_type = IFT_ARCNET;
		sdl->sdl_alen = ARC_ADDR_LEN;
		*LLADDR(sdl) = 0;
		*llsa = (struct sockaddr *)sdl;
		return 0;
#endif
#ifdef INET6
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)sa;
		if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
			/*
			 * An IP6 address of 0 means listen to all
			 * of the Ethernet multicast address used for IP6.
			 * (This is used for multicast routers.)
			 */
			ifp->if_flags |= IFF_ALLMULTI;
			*llsa = 0;
			return 0;
		}
		if (!IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr))
			return EADDRNOTAVAIL;
		sdl = malloc(sizeof *sdl, M_IFMADDR,
		       M_NOWAIT | M_ZERO);
		if (sdl == NULL)
			return ENOMEM;
		sdl->sdl_len = sizeof *sdl;
		sdl->sdl_family = AF_LINK;
		sdl->sdl_index = ifp->if_index;
		sdl->sdl_type = IFT_ARCNET;
		sdl->sdl_alen = ARC_ADDR_LEN;
		*LLADDR(sdl) = 0;
		*llsa = (struct sockaddr *)sdl;
		return 0;
#endif

	default:
		/*
		 * Well, the text isn't quite right, but it's the name
		 * that counts...
		 */
		return EAFNOSUPPORT;
	}
}

MALLOC_DEFINE(M_ARCCOM, "arccom", "ARCNET interface internals");

static void*
arc_alloc(u_char type, struct ifnet *ifp)
{
	struct arccom	*ac;
	
	ac = malloc(sizeof(struct arccom), M_ARCCOM, M_WAITOK | M_ZERO);
	ac->ac_ifp = ifp;

	return (ac);
}

static void
arc_free(void *com, u_char type)
{

	free(com, M_ARCCOM);
}

static int
arc_modevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		if_register_com_alloc(IFT_ARCNET, arc_alloc, arc_free);
		break;
	case MOD_UNLOAD:
		if_deregister_com_alloc(IFT_ARCNET);
		break;
	default:
		return EOPNOTSUPP;
	}

	return (0);
}

static moduledata_t arc_mod = {
	"arcnet",
	arc_modevent,
	0
};

DECLARE_MODULE(arcnet, arc_mod, SI_SUB_INIT_IF, SI_ORDER_ANY);
MODULE_VERSION(arcnet, 1);
