/*	$NetBSD: if_arcsubr.c,v 1.36 2001/06/14 05:44:23 itojun Exp $	*/
/*	$FreeBSD$ */

/*
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
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

#if defined(INET) || defined(INET6)
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#endif

#ifdef INET6
#include <netinet6/nd6.h>
#endif

MODULE_VERSION(arcnet, 1);

#define ARCNET_ALLOW_BROKEN_ARP

static struct mbuf *arc_defrag(struct ifnet *, struct mbuf *);

u_int8_t  arcbroadcastaddr = 0;

#define senderr(e) { error = (e); goto bad;}
#define SIN(s) ((struct sockaddr_in *)s)

/*
 * ARCnet output routine.
 * Encapsulate a packet of type family for the local net.
 * Assumes that ifp is actually pointer to arccom structure.
 */
int
arc_output(ifp, m, dst, rt0)
	struct ifnet *ifp;
	struct mbuf *m;
	struct sockaddr *dst;
	struct rtentry *rt0;
{
	struct mbuf		*mcopy;
	struct rtentry		*rt;
	struct arccom		*ac;
	struct arc_header	*ah;
	struct arphdr		*arph;
	int			error;
	u_int8_t		atype, adst;
#if __FreeBSD_version < 500000
	int			s;
#endif

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
		return(ENETDOWN); /* m, m1 aren't initialized yet */

	error = 0;
	ac = (struct arccom *)ifp;
	mcopy = NULL;

	if ((rt = rt0)) {
		if ((rt->rt_flags & RTF_UP) == 0) {
			if ((rt0 = rt = rtalloc1(dst, 1, 0UL)))
				rt->rt_refcnt--;
			else
				senderr(EHOSTUNREACH);
		}
		if (rt->rt_flags & RTF_GATEWAY) {
			if (rt->rt_gwroute == 0)
				goto lookup;
			if (((rt = rt->rt_gwroute)->rt_flags & RTF_UP) == 0) {
				rtfree(rt); rt = rt0;
			lookup: rt->rt_gwroute = rtalloc1(rt->rt_gateway, 1, 0UL);
				if ((rt = rt->rt_gwroute) == 0)
					senderr(EHOSTUNREACH);
			}
		}
		if (rt->rt_flags & RTF_REJECT)
			if (rt->rt_rmx.rmx_expire == 0 ||
			    time_second < rt->rt_rmx.rmx_expire)
				senderr(rt == rt0 ? EHOSTDOWN : EHOSTUNREACH);
	}

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
		else if (!arpresolve(ifp, rt, m, dst, &adst, rt0))
			return 0;	/* not resolved yet */

		/* If broadcasting on a simplex interface, loopback a copy */
		if ((m->m_flags & (M_BCAST|M_MCAST)) &&
		    (ifp->if_flags & IFF_SIMPLEX))
			mcopy = m_copy(m, 0, (int)M_COPYALL);
		atype = (ifp->if_flags & IFF_LINK0) ?
			ARCTYPE_IP_OLD : ARCTYPE_IP;
		break;
#endif
#ifdef INET6
	case AF_INET6:
#ifdef OLDIP6OUTPUT
		if (!nd6_resolve(ifp, rt, m, dst, (u_char *)&adst))
			return(0);	/* if not yet resolves */
#else
		if (!nd6_storelladdr(ifp, rt, m, dst, (u_char *)&adst))
			return(0); /* it must be impossible, but... */
#endif /* OLDIP6OUTPUT */
		atype = ARCTYPE_INET6;
		break;
#endif

	case AF_UNSPEC:
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
			arph = mtod(m, struct arphdr *);
			if (ifp->if_flags & IFF_LINK2)
				arph->ar_pro = atype - 1;
#endif
		}
		break;

	default:
		printf("%s%d: can't handle af%d\n", ifp->if_name, ifp->if_unit,
		    dst->sa_family);
		senderr(EAFNOSUPPORT);
	}

	if (mcopy)
		(void) if_simloop(ifp, mcopy, dst->sa_family, 0);

	M_PREPEND(m, ARC_HDRLEN, M_DONTWAIT);
	if (m == 0)
		senderr(ENOBUFS);
	ah = mtod(m, struct arc_header *);
	ah->arc_type = atype;
	ah->arc_dhost = adst;
	ah->arc_shost = *IF_LLADDR(ifp);

	if (ifp->if_bpf)
		bpf_mtap(ifp, m);

#if __FreeBSD_version < 500000
	s = splimp();

	/*
	 * Queue message on interface, and start output if interface
	 * not yet active.
	 */
	if (IF_QFULL(&ifp->if_snd)) {
		IF_DROP(&ifp->if_snd);
		splx(s);
		senderr(ENOBUFS);
	}
	ifp->if_obytes += m->m_pkthdr.len;
	IF_ENQUEUE(&ifp->if_snd, m);
	if ((ifp->if_flags & IFF_OACTIVE) == 0)
		(*ifp->if_start)(ifp);
	splx(s);
#else
	if (!IF_HANDOFF(&ifp->if_snd, m, ifp)) {
		m = 0;
		senderr(ENOBUFS);
	}
#endif

	return (error);

bad:
	if (m)
		m_freem(m);
	return (error);
}

void
arc_frag_init(ifp)
	struct ifnet *ifp;
{
	struct arccom *ac;

	ac = (struct arccom *)ifp;
	ac->curr_frag = 0;
}

struct mbuf *
arc_frag_next(ifp)
	struct ifnet *ifp;
{
	struct arccom *ac;
	struct mbuf *m;
	struct arc_header *ah;

	ac = (struct arccom *)ifp;
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
		tfrags = (m->m_pkthdr.len + 503) / 504;
		ac->fsflag = 2 * tfrags - 3;
		ac->sflag = 0;
		ac->rsflag = ac->fsflag;
		ac->arc_dhost = ah->arc_dhost;
		ac->arc_shost = ah->arc_shost;
		ac->arc_type = ah->arc_type;

		m_adj(m, ARC_HDRLEN);
		ac->curr_frag = m;
	}

	/* split out next fragment and return it */
	if (ac->sflag < ac->fsflag) {
		/* we CAN'T have short packets here */
		ac->curr_frag = m_split(m, 504, M_DONTWAIT);
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
arc_defrag(ifp, m)
	struct ifnet *ifp;
	struct mbuf *m;
{
	struct arc_header *ah, *ah1;
	struct arccom *ac;
	struct ac_frag *af;
	struct mbuf *m1;
	char *s;
	int newflen;
	u_char src,dst,typ;

	ac = (struct arccom *)ifp;

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

	log(LOG_INFO,"%s%d: got out of seq. packet: %s\n",
	    ifp->if_name, ifp->if_unit, s);

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
arc_isphds(type)
	u_int8_t type;
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
arc_input(ifp, m)
	struct ifnet *ifp;
	struct mbuf *m;
{
	struct arc_header *ah;
	struct ifqueue *inq;
	u_int8_t atype;
#ifdef INET
	struct arphdr *arph;
#endif
#if __FreeBSD_version < 500000
	int s;
#endif

	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}

	/* possibly defragment: */
	m = arc_defrag(ifp, m);
	if (m == NULL)
		return;

	if (ifp->if_bpf)
		bpf_mtap(ifp, m);

	ah = mtod(m, struct arc_header *);

	ifp->if_ibytes += m->m_pkthdr.len;

	if (arcbroadcastaddr == ah->arc_dhost) {
		m->m_flags |= M_BCAST|M_MCAST;
		ifp->if_imcasts++;
	}

	atype = ah->arc_type;
	switch (atype) {
#ifdef INET
	case ARCTYPE_IP:
		m_adj(m, ARC_HDRNEWLEN);
		schednetisr(NETISR_IP);
		inq = &ipintrq;
		break;

	case ARCTYPE_IP_OLD:
		m_adj(m, ARC_HDRLEN);
		schednetisr(NETISR_IP);
		inq = &ipintrq;
		break;

	case ARCTYPE_ARP:
		if (ifp->if_flags & IFF_NOARP) {
			/* Discard packet if ARP is disabled on interface */
			m_freem(m);
			return;
		}
		m_adj(m, ARC_HDRNEWLEN);
		schednetisr(NETISR_ARP);
		inq = &arpintrq;
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
		schednetisr(NETISR_ARP);
		inq = &arpintrq;
		arph = mtod(m, struct arphdr *);
#ifdef ARCNET_ALLOW_BROKEN_ARP
		mtod(m, struct arphdr *)->ar_pro = htons(ETHERTYPE_IP);
#endif
		break;
#endif
#ifdef INET6
	case ARCTYPE_INET6:
		m_adj(m, ARC_HDRNEWLEN);
		schednetisr(NETISR_IPV6);
		inq = &ip6intrq;
		break;
#endif
	default:
		m_freem(m);
		return;
	}

#if __FreeBSD_version < 500000
	s = splimp();
	if (IF_QFULL(inq)) {
		IF_DROP(inq);
		m_freem(m);
	} else
		IF_ENQUEUE(inq, m);
	splx(s);
#else
	IF_HANDOFF(inq, m, NULL);
#endif
}

/*
 * Convert Arcnet address to printable (loggable) representation.
 */
static char digits[] = "0123456789abcdef";
char *
arc_sprintf(ap)
	u_int8_t *ap;
{
	static char arcbuf[3];
	char *cp = arcbuf;

	*cp++ = digits[*ap >> 4];
	*cp++ = digits[*ap++ & 0xf];
	*cp   = 0;
	return (arcbuf);
}

/*
 * Register (new) link level address.
 */
void
arc_storelladdr(ifp, lla)
	struct ifnet *ifp;
	u_int8_t lla;
{
	*IF_LLADDR(ifp) = lla;
}

/*
 * Perform common duties while attaching to interface list
 */
void
arc_ifattach(ifp, lla)
	struct ifnet *ifp;
	u_int8_t lla;
{
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;
	struct arccom *ac;

	if_attach(ifp);
	ifp->if_type = IFT_ARCNET;
	ifp->if_addrlen = 1;
	ifp->if_hdrlen = ARC_HDRLEN;
	ifp->if_mtu = 1500;
	if (ifp->if_baudrate == 0)
		ifp->if_baudrate = 2500000;
#if __FreeBSD_version < 500000
	ifa = ifnet_addrs[ifp->if_index - 1];
#else
	ifa = ifaddr_byindex(ifp->if_index);
#endif
	KASSERT(ifa != NULL, ("%s: no lladdr!\n", __FUNCTION__));
	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	sdl->sdl_type = IFT_ARCNET;
	sdl->sdl_alen = ifp->if_addrlen;

	if (ifp->if_flags & IFF_BROADCAST)
		ifp->if_flags |= IFF_MULTICAST|IFF_ALLMULTI;

	ac = (struct arccom *)ifp;
	ac->ac_seqid = (time_second) & 0xFFFF; /* try to make seqid unique */
	if (lla == 0) {
		/* XXX this message isn't entirely clear, to me -- cgd */
		log(LOG_ERR,"%s%d: link address 0 reserved for broadcasts.  Please change it and ifconfig %s%d down up\n",
		   ifp->if_name, ifp->if_unit, ifp->if_name, ifp->if_unit);
	}
	arc_storelladdr(ifp, lla);

	ifp->if_broadcastaddr = &arcbroadcastaddr;

	bpfattach(ifp, DLT_ARCNET, ARC_HDRLEN);
}

void
arc_ifdetach(ifp)
	struct ifnet *ifp;
{
	bpfdetach(ifp);
	if_detach(ifp);
}

int
arc_ioctl(ifp, command, data)
	struct ifnet *ifp;
	int command;
	caddr_t data;
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
		default:
			ifp->if_init(ifp->if_softc);
			break;
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

#if 0
	case SIOCGIFADDR:
		{
			struct sockaddr *sa;

			sa = (struct sockaddr *) & ifr->ifr_data;
			bcopy(IFP2AC(ifp)->ac_enaddr,
			      (caddr_t) sa->sa_data, ETHER_ADDR_LEN);
		}
		break;
#endif
	}

	return (error);
}
