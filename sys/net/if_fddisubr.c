/*
 * Copyright (c) 1995, 1996
 *	Matt Thomas <matt@3am-software.com>.  All rights reserved.
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
 *	from: if_ethersubr.c,v 1.5 1994/12/13 22:31:45 wollman Exp
 * $FreeBSD$
 */

#include "opt_atalk.h"
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipx.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/malloc.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/if_llc.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#if defined(INET) || defined(INET6)
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#endif
#ifdef INET6
#include <netinet6/nd6.h>
#endif
#include <netinet/if_fddi.h>

#ifdef IPX
#include <netipx/ipx.h> 
#include <netipx/ipx_if.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#ifdef DECNET
#include <netdnet/dn.h>
#endif

#ifdef NETATALK
#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/at_extern.h>

extern u_char	at_org_code[ 3 ];
extern u_char	aarp_org_code[ 3 ];
#endif /* NETATALK */

static	int fddi_resolvemulti(struct ifnet *, struct sockaddr **,
			      struct sockaddr *);

#define	IFP2AC(IFP)	((struct arpcom *)IFP)
#define	senderr(e)	{ error = (e); goto bad; }

/*
 * FDDI output routine.
 * Encapsulate a packet of type family for the local net.
 * Use trailer local net encapsulation if enough data in first
 * packet leaves a multiple of 512 bytes of data in remainder.
 * Assumes that ifp is actually pointer to arpcom structure.
 */
int
fddi_output(ifp, m, dst, rt0)
	struct ifnet *ifp;
	struct mbuf *m;
	struct sockaddr *dst;
	struct rtentry *rt0;
{
	u_int16_t type;
	int loop_copy = 0, error = 0, hdrcmplt = 0;
 	u_char esrc[6], edst[6];
	struct rtentry *rt;
	struct fddi_header *fh;
	struct arpcom *ac = IFP2AC(ifp);

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
		senderr(ENETDOWN);
	getmicrotime(&ifp->if_lastchange);
	if ((rt = rt0) != NULL) {
		if ((rt->rt_flags & RTF_UP) == 0) {
			if ((rt0 = rt = rtalloc1(dst, 1, 0UL)) != NULL)
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
	case AF_INET: {
		if (!arpresolve(ifp, rt, m, dst, edst, rt0))
			return (0);	/* if not yet resolved */
		type = htons(ETHERTYPE_IP);
		break;
	}
#endif
#ifdef INET6
	case AF_INET6:
		if (!nd6_storelladdr(&ac->ac_if, rt, m, dst, (u_char *)edst)) {
			/* Something bad happened */
			return (0);
		}
		type = htons(ETHERTYPE_IPV6);
		break;
#endif
#ifdef IPX
	case AF_IPX:
		type = htons(ETHERTYPE_IPX);
 		bcopy((caddr_t)&(((struct sockaddr_ipx *)dst)->sipx_addr.x_host),
		    (caddr_t)edst, sizeof (edst));
		break;
#endif
#ifdef NETATALK
	case AF_APPLETALK: {
	    struct at_ifaddr *aa;
            if (!aarpresolve(ac, m, (struct sockaddr_at *)dst, edst))
                return (0);
	    /*
	     * ifaddr is the first thing in at_ifaddr
	     */
	    if ((aa = at_ifawithnet( (struct sockaddr_at *)dst)) == 0)
		goto bad;
	    
	    /*
	     * In the phase 2 case, we need to prepend an mbuf for the llc header.
	     * Since we must preserve the value of m, which is passed to us by
	     * value, we m_copy() the first mbuf, and use it for our llc header.
	     */
	    if (aa->aa_flags & AFA_PHASE2) {
		struct llc llc;

		M_PREPEND(m, sizeof(struct llc), M_TRYWAIT);
		if (m == 0)
			senderr(ENOBUFS);
		llc.llc_dsap = llc.llc_ssap = LLC_SNAP_LSAP;
		llc.llc_control = LLC_UI;
		bcopy(at_org_code, llc.llc_snap.org_code, sizeof(at_org_code));
		llc.llc_snap.ether_type = htons(ETHERTYPE_AT);
		bcopy(&llc, mtod(m, caddr_t), sizeof(struct llc));
		type = 0;
	    } else {
		type = htons(ETHERTYPE_AT);
	    }
	    break;
	}
#endif /* NETATALK */
#ifdef NS
	case AF_NS:
		type = htons(ETHERTYPE_NS);
 		bcopy((caddr_t)&(((struct sockaddr_ns *)dst)->sns_addr.x_host),
		    (caddr_t)edst, sizeof (edst));
		break;
#endif

	case pseudo_AF_HDRCMPLT:
	{
		struct ether_header *eh;
		hdrcmplt = 1;
		eh = (struct ether_header *)dst->sa_data;
 		(void)memcpy((caddr_t)esrc, (caddr_t)eh->ether_shost, sizeof (esrc));
		/* FALLTHROUGH */
	}

	case AF_UNSPEC:
	{
		struct ether_header *eh;
		loop_copy = -1;
		eh = (struct ether_header *)dst->sa_data;
 		(void)memcpy((caddr_t)edst, (caddr_t)eh->ether_dhost, sizeof (edst));
		if (*edst & 1)
			m->m_flags |= (M_BCAST|M_MCAST);
		type = eh->ether_type;
		break;
	}

	case AF_IMPLINK:
	{
		fh = mtod(m, struct fddi_header *);
		error = EPROTONOSUPPORT;
		switch (fh->fddi_fc & (FDDIFC_C|FDDIFC_L|FDDIFC_F)) {
			case FDDIFC_LLC_ASYNC: {
				/* legal priorities are 0 through 7 */
				if ((fh->fddi_fc & FDDIFC_Z) > 7)
			        	goto bad;
				break;
			}
			case FDDIFC_LLC_SYNC: {
				/* FDDIFC_Z bits reserved, must be zero */
				if (fh->fddi_fc & FDDIFC_Z)
					goto bad;
				break;
			}
			case FDDIFC_SMT: {
				/* FDDIFC_Z bits must be non zero */
				if ((fh->fddi_fc & FDDIFC_Z) == 0)
					goto bad;
				break;
			}
			default: {
				/* anything else is too dangerous */
               	 		goto bad;
			}
		}
		error = 0;
		if (fh->fddi_dhost[0] & 1)
			m->m_flags |= (M_BCAST|M_MCAST);
		goto queue_it;
	}
	default:
		printf("%s%d: can't handle af%d\n", ifp->if_name, ifp->if_unit,
			dst->sa_family);
		senderr(EAFNOSUPPORT);
	}

	if (type != 0) {
		struct llc *l;
		M_PREPEND(m, sizeof (struct llc), M_DONTWAIT);
		if (m == 0)
			senderr(ENOBUFS);
		l = mtod(m, struct llc *);
		l->llc_control = LLC_UI;
		l->llc_dsap = l->llc_ssap = LLC_SNAP_LSAP;
		l->llc_snap.org_code[0] = l->llc_snap.org_code[1] = l->llc_snap.org_code[2] = 0;
		(void)memcpy((caddr_t) &l->llc_snap.ether_type, (caddr_t) &type,
			sizeof(u_int16_t));
	}

	/*
	 * Add local net header.  If no space in first mbuf,
	 * allocate another.
	 */
	M_PREPEND(m, sizeof (struct fddi_header), M_DONTWAIT);
	if (m == 0)
		senderr(ENOBUFS);
	fh = mtod(m, struct fddi_header *);
	fh->fddi_fc = FDDIFC_LLC_ASYNC|FDDIFC_LLC_PRIO4;
 	(void)memcpy((caddr_t)fh->fddi_dhost, (caddr_t)edst, sizeof (edst));
  queue_it:
	if (hdrcmplt)
		(void)memcpy((caddr_t)fh->fddi_shost, (caddr_t)esrc,
			sizeof(fh->fddi_shost));
	else
		(void)memcpy((caddr_t)fh->fddi_shost, (caddr_t)ac->ac_enaddr,
			sizeof(fh->fddi_shost));
	/*
	 * If a simplex interface, and the packet is being sent to our
	 * Ethernet address or a broadcast address, loopback a copy.
	 * XXX To make a simplex device behave exactly like a duplex
	 * device, we should copy in the case of sending to our own
	 * ethernet address (thus letting the original actually appear
	 * on the wire). However, we don't do that here for security
	 * reasons and compatibility with the original behavior.
	 */
	if ((ifp->if_flags & IFF_SIMPLEX) &&
	   (loop_copy != -1)) {
		if ((m->m_flags & M_BCAST) || loop_copy) {
			struct mbuf *n = m_copy(m, 0, (int)M_COPYALL);

			(void) if_simloop(ifp,
				n, dst->sa_family, sizeof(struct fddi_header));
	     	} else if (bcmp(fh->fddi_dhost,
		    fh->fddi_shost, sizeof(fh->fddi_shost)) == 0) {
			(void) if_simloop(ifp,
				m, dst->sa_family, sizeof(struct fddi_header));
			return (0);	/* XXX */
		}
	}

	if (! IF_HANDOFF(&ifp->if_snd, m, ifp))
		senderr(ENOBUFS);
	return (error);

bad:
	if (m)
		m_freem(m);
	return (error);
}

/*
 * Process a received FDDI packet;
 * the packet is in the mbuf chain m without
 * the fddi header, which is provided separately.
 */
void
fddi_input(ifp, fh, m)
	struct ifnet *ifp;
	struct fddi_header *fh;
	struct mbuf *m;
{
	struct ifqueue *inq;
	struct llc *l;

	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}
	getmicrotime(&ifp->if_lastchange);
	ifp->if_ibytes += m->m_pkthdr.len + sizeof (*fh);
	if (fh->fddi_dhost[0] & 1) {
		if (bcmp((caddr_t)fddibroadcastaddr, (caddr_t)fh->fddi_dhost,
		    sizeof(fddibroadcastaddr)) == 0)
			m->m_flags |= M_BCAST;
		else
			m->m_flags |= M_MCAST;
		ifp->if_imcasts++;
	} else if ((ifp->if_flags & IFF_PROMISC)
	    && bcmp(IFP2AC(ifp)->ac_enaddr, (caddr_t)fh->fddi_dhost,
		    sizeof(fh->fddi_dhost)) != 0) {
		m_freem(m);
		return;
	}

#ifdef M_LINK0
	/*
	 * If this has a LLC priority of 0, then mark it so upper
	 * layers have a hint that it really came via a FDDI/Ethernet
	 * bridge.
	 */
	if ((fh->fddi_fc & FDDIFC_LLC_PRIO7) == FDDIFC_LLC_PRIO0)
		m->m_flags |= M_LINK0;
#endif

	l = mtod(m, struct llc *);
	switch (l->llc_dsap) {
#if defined(INET) || defined(INET6) || defined(NS) || defined(DECNET) || defined(IPX) || defined(NETATALK)
	case LLC_SNAP_LSAP:
	{
		u_int16_t type;
		if (l->llc_control != LLC_UI || l->llc_ssap != LLC_SNAP_LSAP)
			goto dropanyway;
#ifdef NETATALK
		if (Bcmp(&(l->llc_snap.org_code)[0], at_org_code,
			 sizeof(at_org_code)) == 0 &&
		 	ntohs(l->llc_snap.ether_type) == ETHERTYPE_AT) {
		    inq = &atintrq2;
		    m_adj( m, sizeof( struct llc ));
		    schednetisr(NETISR_ATALK);
		    break;
		}

		if (Bcmp(&(l->llc_snap.org_code)[0], aarp_org_code,
			 sizeof(aarp_org_code)) == 0 &&
			ntohs(l->llc_snap.ether_type) == ETHERTYPE_AARP) {
		    m_adj( m, sizeof( struct llc ));
		    aarpinput(IFP2AC(ifp), m); /* XXX */
		    return;
		}
#endif /* NETATALK */
		if (l->llc_snap.org_code[0] != 0 || l->llc_snap.org_code[1] != 0|| l->llc_snap.org_code[2] != 0)
			goto dropanyway;
		type = ntohs(l->llc_snap.ether_type);
		m_adj(m, 8);
		switch (type) {
#ifdef INET
		case ETHERTYPE_IP:
			if (ipflow_fastforward(m))
				return;
			schednetisr(NETISR_IP);
			inq = &ipintrq;
			break;

		case ETHERTYPE_ARP:
			if (ifp->if_flags & IFF_NOARP)
				goto dropanyway;
			schednetisr(NETISR_ARP);
			inq = &arpintrq;
			break;
#endif
#ifdef INET6
		case ETHERTYPE_IPV6:
			schednetisr(NETISR_IPV6);
			inq = &ip6intrq;
			break;
#endif
#ifdef IPX      
		case ETHERTYPE_IPX: 
			schednetisr(NETISR_IPX);
			inq = &ipxintrq;
			break;  
#endif   
#ifdef NS
		case ETHERTYPE_NS:
			schednetisr(NETISR_NS);
			inq = &nsintrq;
			break;
#endif
#ifdef DECNET
		case ETHERTYPE_DECNET:
			schednetisr(NETISR_DECNET);
			inq = &decnetintrq;
			break;
#endif
#ifdef NETATALK 
		case ETHERTYPE_AT:
	                schednetisr(NETISR_ATALK);
			inq = &atintrq1;
			break;
	        case ETHERTYPE_AARP:
			/* probably this should be done with a NETISR as well */
			aarpinput(IFP2AC(ifp), m); /* XXX */
			return;
#endif /* NETATALK */
		default:
			/* printf("fddi_input: unknown protocol 0x%x\n", type); */
			ifp->if_noproto++;
			goto dropanyway;
		}
		break;
	}
#endif /* INET || NS */
		
	default:
		/* printf("fddi_input: unknown dsap 0x%x\n", l->llc_dsap); */
		ifp->if_noproto++;
	dropanyway:
		m_freem(m);
		return;
	}

	(void) IF_HANDOFF(inq, m, NULL);
}
/*
 * Perform common duties while attaching to interface list
 */

void
fddi_ifattach(ifp)
	struct ifnet *ifp;
{
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;

	ifp->if_type = IFT_FDDI;
	ifp->if_addrlen = 6;
	ifp->if_hdrlen = 21;
	ifp->if_mtu = FDDIMTU;
	ifp->if_resolvemulti = fddi_resolvemulti;
	ifp->if_baudrate = 100000000;
#ifdef IFF_NOTRAILERS
	ifp->if_flags |= IFF_NOTRAILERS;
#endif
	ifp->if_broadcastaddr = fddibroadcastaddr;
	ifa = ifaddr_byindex(ifp->if_index);
	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	sdl->sdl_type = IFT_FDDI;
	sdl->sdl_alen = ifp->if_addrlen;
	bcopy(IFP2AC(ifp)->ac_enaddr, LLADDR(sdl), ifp->if_addrlen);
}

static int
fddi_resolvemulti(ifp, llsa, sa)
	struct ifnet *ifp;
	struct sockaddr **llsa;
	struct sockaddr *sa;
{
	struct sockaddr_dl *sdl;
	struct sockaddr_in *sin;
#ifdef INET6
	struct sockaddr_in6 *sin6;
#endif
	u_char *e_addr;

	switch(sa->sa_family) {
	case AF_LINK:
		/*
		 * No mapping needed. Just check that it's a valid MC address.
		 */
		sdl = (struct sockaddr_dl *)sa;
		e_addr = LLADDR(sdl);
		if ((e_addr[0] & 1) != 1)
			return (EADDRNOTAVAIL);
		*llsa = 0;
		return (0);

#ifdef INET
	case AF_INET:
		sin = (struct sockaddr_in *)sa;
		if (!IN_MULTICAST(ntohl(sin->sin_addr.s_addr)))
			return (EADDRNOTAVAIL);
		MALLOC(sdl, struct sockaddr_dl *, sizeof *sdl, M_IFMADDR,
		       M_WAITOK);
		sdl->sdl_len = sizeof *sdl;
		sdl->sdl_family = AF_LINK;
		sdl->sdl_index = ifp->if_index;
		sdl->sdl_type = IFT_FDDI;
		sdl->sdl_nlen = 0;
		sdl->sdl_alen = ETHER_ADDR_LEN;	/* XXX */
		sdl->sdl_slen = 0;
		e_addr = LLADDR(sdl);
		ETHER_MAP_IP_MULTICAST(&sin->sin_addr, e_addr);
		*llsa = (struct sockaddr *)sdl;
		return (0);
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
			return (0);
		}
		if (!IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr))
			return (EADDRNOTAVAIL);
		MALLOC(sdl, struct sockaddr_dl *, sizeof *sdl, M_IFMADDR,
		       M_WAITOK);
		sdl->sdl_len = sizeof *sdl;
		sdl->sdl_family = AF_LINK;
		sdl->sdl_index = ifp->if_index;
		sdl->sdl_type = IFT_FDDI;
		sdl->sdl_nlen = 0;
		sdl->sdl_alen = ETHER_ADDR_LEN;	/* XXX */
		sdl->sdl_slen = 0;
		e_addr = LLADDR(sdl);
		ETHER_MAP_IPV6_MULTICAST(&sin6->sin6_addr, e_addr);
		*llsa = (struct sockaddr *)sdl;
		return (0);
#endif

	default:
		/*
		 * Well, the text isn't quite right, but it's the name
		 * that counts...
		 */
		return (EAFNOSUPPORT);
	}
}
