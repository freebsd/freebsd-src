/*
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
 *	@(#)if_ethersubr.c	8.1 (Berkeley) 6/10/93
 * $Id: if_ethersubr.c,v 1.46 1998/03/18 01:40:11 wollman Exp $
 */

#include "opt_atalk.h"
#include "opt_inet.h"
#include "opt_ipx.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/if_llc.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#endif

#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
ushort ns_nettype;
int ether_outputdebug = 0;
int ether_inputdebug = 0;
#endif

#ifdef ISO
#include <netiso/argo_debug.h>
#include <netiso/iso.h>
#include <netiso/iso_var.h>
#include <netiso/iso_snpac.h>
#endif

/*#ifdef LLC
#include <netccitt/dll.h>
#include <netccitt/llc_var.h>
#endif*/

#if defined(LLC) && defined(CCITT)
extern struct ifqueue pkintrq;
#endif

#ifdef NETATALK
#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/at_extern.h>

#define llc_snap_org_code llc_un.type_snap.org_code
#define llc_snap_ether_type llc_un.type_snap.ether_type

extern u_char	at_org_code[3];
extern u_char	aarp_org_code[3];
#endif /* NETATALK */

#include "vlan.h"
#if NVLAN > 0
#include <net/if_vlan_var.h>
#endif /* NVLAN > 0 */

static	int ether_resolvemulti __P((struct ifnet *, struct sockaddr **, 
				    struct sockaddr *));
u_char	etherbroadcastaddr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
#define senderr(e) { error = (e); goto bad;}

/*
 * Ethernet output routine.
 * Encapsulate a packet of type family for the local net.
 * Use trailer local net encapsulation if enough data in first
 * packet leaves a multiple of 512 bytes of data in remainder.
 * Assumes that ifp is actually pointer to arpcom structure.
 */
int
ether_output(ifp, m0, dst, rt0)
	register struct ifnet *ifp;
	struct mbuf *m0;
	struct sockaddr *dst;
	struct rtentry *rt0;
{
	short type;
	int s, error = 0;
 	u_char edst[6];
	register struct mbuf *m = m0;
	register struct rtentry *rt;
	struct mbuf *mcopy = (struct mbuf *)0;
	register struct ether_header *eh;
	int off, len = m->m_pkthdr.len;
	struct arpcom *ac = (struct arpcom *)ifp;
#ifdef NETATALK
	struct at_ifaddr *aa;
#endif NETATALK

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
		senderr(ENETDOWN);
	rt = rt0;
	if (rt) {
		if ((rt->rt_flags & RTF_UP) == 0) {
			rt0 = rt = rtalloc1(dst, 1, 0UL);
			if (rt0)
				rt->rt_refcnt--;
			else
				senderr(EHOSTUNREACH);
		}
		if (rt->rt_flags & RTF_GATEWAY) {
			if (rt->rt_gwroute == 0)
				goto lookup;
			if (((rt = rt->rt_gwroute)->rt_flags & RTF_UP) == 0) {
				rtfree(rt); rt = rt0;
			lookup: rt->rt_gwroute = rtalloc1(rt->rt_gateway, 1,
							  0UL);
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
		if (!arpresolve(ac, rt, m, dst, edst, rt0))
			return (0);	/* if not yet resolved */
		/* If broadcasting on a simplex interface, loopback a copy */
		if ((m->m_flags & M_BCAST) && (ifp->if_flags & IFF_SIMPLEX))
			mcopy = m_copy(m, 0, (int)M_COPYALL);
		off = m->m_pkthdr.len - m->m_len;
		type = htons(ETHERTYPE_IP);
		break;
#endif
#ifdef IPX
	case AF_IPX:
		{
		struct ifaddr *ia;

		type = htons(ETHERTYPE_IPX);
 		bcopy((caddr_t)&(((struct sockaddr_ipx *)dst)->sipx_addr.x_host),
		    (caddr_t)edst, sizeof (edst));

		for(ia = ifp->if_addrhead.tqh_first; ia != 0;
		    ia = ia->ifa_link.tqe_next) {
			if(ia->ifa_addr->sa_family == AF_IPX &&
			    !bcmp((caddr_t)edst,
				  (caddr_t)&((struct ipx_ifaddr *)ia)->ia_addr.sipx_addr.x_host,
				  sizeof(edst)) )
				return (looutput(ifp, m, dst, rt));
			}

		/* If broadcasting on a simplex interface, loopback a copy */
		if ((m->m_flags & M_BCAST) && (ifp->if_flags & IFF_SIMPLEX))
			mcopy = m_copy(m, 0, (int)M_COPYALL);
		break;
		}
#endif
#ifdef NETATALK
	case AF_APPLETALK:
	    {
		struct sockaddr_at *sat = (struct sockaddr_at *)dst;

		/*
		 * super hack..
		 * Most of this loopback code should move into the appletalk
		 * code, but it's here for now.. remember to move it! [JRE]
		 * This may not get the same interface we started with,
		 * fix asap. XXX
		 */
		aa = at_ifawithnet( sat );
		if (aa == NULL) {
			goto bad;
		}
		if( aa->aa_ifa.ifa_ifp != ifp ) {
			(*aa->aa_ifa.ifa_ifp->if_output)(aa->aa_ifa.ifa_ifp,
							m,dst,rt);
		}
		if (((sat->sat_addr.s_net == ATADDR_ANYNET)
		  && (sat->sat_addr.s_node == ATADDR_ANYNODE))
		|| ((sat->sat_addr.s_net == aa->aa_addr.sat_addr.s_net )
		  && (sat->sat_addr.s_node == aa->aa_addr.sat_addr.s_node))) {
			(void) looutput(ifp, m, dst, rt);
			return(0);
		}

        	if (!aarpresolve(ac, m, (struct sockaddr_at *)dst, edst)) {
#ifdef NETATALKDEBUG
                	extern char *prsockaddr(struct sockaddr *);
                	printf("aarpresolv: failed for %s\n", prsockaddr(dst));
#endif /* NETATALKDEBUG */
                	return (0);
        	}

		/*
		 * If broadcasting on a simplex interface, loopback a copy
		 */
		if ((m->m_flags & M_BCAST) && (ifp->if_flags & IFF_SIMPLEX))
			mcopy = m_copy(m, 0, (int)M_COPYALL);
	    }
	    /*
	     * In the phase 2 case, we need to prepend an mbuf for the llc header.
	     * Since we must preserve the value of m, which is passed to us by
	     * value, we m_copy() the first mbuf, and use it for our llc header.
	     */
	    if ( aa->aa_flags & AFA_PHASE2 ) {
		struct llc llc;

		M_PREPEND(m, sizeof(struct llc), M_WAIT);
		len += sizeof(struct llc);
		llc.llc_dsap = llc.llc_ssap = LLC_SNAP_LSAP;
		llc.llc_control = LLC_UI;
		bcopy(at_org_code, llc.llc_snap_org_code, sizeof(at_org_code));
		llc.llc_snap_ether_type = htons( ETHERTYPE_AT );
		bcopy(&llc, mtod(m, caddr_t), sizeof(struct llc));
		type = htons(m->m_pkthdr.len);
	    } else {
		type = htons(ETHERTYPE_AT);
	    }
	    break;
#endif NETATALK
#ifdef NS
	case AF_NS:
		switch(ns_nettype){
		default:
		case 0x8137: /* Novell Ethernet_II Ethernet TYPE II */
			type = 0x8137;
			break;
		case 0x0: /* Novell 802.3 */
			type = htons( m->m_pkthdr.len);
			break;
		case 0xe0e0: /* Novell 802.2 and Token-Ring */
			M_PREPEND(m, 3, M_WAIT);
			type = htons( m->m_pkthdr.len);
			cp = mtod(m, u_char *);
			*cp++ = 0xE0;
			*cp++ = 0xE0;
			*cp++ = 0x03;
			break;
		}
 		bcopy((caddr_t)&(((struct sockaddr_ns *)dst)->sns_addr.x_host),
		    (caddr_t)edst, sizeof (edst));
		if (!bcmp((caddr_t)edst, (caddr_t)&ns_thishost, sizeof(edst))){
			m->m_pkthdr.rcvif = ifp;
			schednetisr(NETISR_NS);
			inq = &nsintrq;
			s = splimp();
			if (IF_QFULL(inq)) {
				IF_DROP(inq);
				m_freem(m);
			} else
				IF_ENQUEUE(inq, m);
			splx(s);
			return (error);
		}
		if (!bcmp((caddr_t)edst, (caddr_t)&ns_broadhost, sizeof(edst))){
			m2 = m_copy(m, 0, (int)M_COPYALL);
			m2->m_pkthdr.rcvif = ifp;
			schednetisr(NETISR_NS);
			inq = &nsintrq;
			s = splimp();
			if (IF_QFULL(inq)) {
				IF_DROP(inq);
				m_freem(m2);
			} else
				IF_ENQUEUE(inq, m2);
			splx(s);
		}
		/* If broadcasting on a simplex interface, loopback a copy */
		if ((m->m_flags & M_BCAST) && (ifp->if_flags & IFF_SIMPLEX)){
			mcopy = m_copy(m, 0, (int)M_COPYALL);
		}
		break;
#endif /* NS */
#ifdef	ISO
	case AF_ISO: {
		int	snpalen;
		struct	llc *l;
		register struct sockaddr_dl *sdl;

		if (rt && (sdl = (struct sockaddr_dl *)rt->rt_gateway) &&
		    sdl->sdl_family == AF_LINK && sdl->sdl_alen > 0) {
			bcopy(LLADDR(sdl), (caddr_t)edst, sizeof(edst));
		} else if (error =
			    iso_snparesolve(ifp, (struct sockaddr_iso *)dst,
					    (char *)edst, &snpalen))
			goto bad; /* Not Resolved */
		/* If broadcasting on a simplex interface, loopback a copy */
		if (*edst & 1)
			m->m_flags |= (M_BCAST|M_MCAST);
		if ((m->m_flags & M_BCAST) && (ifp->if_flags & IFF_SIMPLEX) &&
		    (mcopy = m_copy(m, 0, (int)M_COPYALL))) {
			M_PREPEND(mcopy, sizeof (*eh), M_DONTWAIT);
			if (mcopy) {
				eh = mtod(mcopy, struct ether_header *);
				bcopy((caddr_t)edst,
				      (caddr_t)eh->ether_dhost, sizeof (edst));
				bcopy((caddr_t)ac->ac_enaddr,
				      (caddr_t)eh->ether_shost, sizeof (edst));
			}
		}
		M_PREPEND(m, 3, M_DONTWAIT);
		if (m == NULL)
			return (0);
		type = htons(m->m_pkthdr.len);
		l = mtod(m, struct llc *);
		l->llc_dsap = l->llc_ssap = LLC_ISO_LSAP;
		l->llc_control = LLC_UI;
		len += 3;
		IFDEBUG(D_ETHER)
			int i;
			printf("unoutput: sending pkt to: ");
			for (i=0; i<6; i++)
				printf("%x ", edst[i] & 0xff);
			printf("\n");
		ENDDEBUG
		} break;
#endif /* ISO */
#ifdef	LLC
/*	case AF_NSAP: */
	case AF_CCITT: {
		register struct sockaddr_dl *sdl =
			(struct sockaddr_dl *) rt -> rt_gateway;

		if (sdl && sdl->sdl_family == AF_LINK
		    && sdl->sdl_alen > 0) {
			bcopy(LLADDR(sdl), (char *)edst,
				sizeof(edst));
		} else goto bad; /* Not a link interface ? Funny ... */
		if ((ifp->if_flags & IFF_SIMPLEX) && (*edst & 1) &&
		    (mcopy = m_copy(m, 0, (int)M_COPYALL))) {
			M_PREPEND(mcopy, sizeof (*eh), M_DONTWAIT);
			if (mcopy) {
				eh = mtod(mcopy, struct ether_header *);
				bcopy((caddr_t)edst,
				      (caddr_t)eh->ether_dhost, sizeof (edst));
				bcopy((caddr_t)ac->ac_enaddr,
				      (caddr_t)eh->ether_shost, sizeof (edst));
			}
		}
		type = htons(m->m_pkthdr.len);
#ifdef LLC_DEBUG
		{
			int i;
			register struct llc *l = mtod(m, struct llc *);

			printf("ether_output: sending LLC2 pkt to: ");
			for (i=0; i<6; i++)
				printf("%x ", edst[i] & 0xff);
			printf(" len 0x%x dsap 0x%x ssap 0x%x control 0x%x\n",
			       type & 0xff, l->llc_dsap & 0xff, l->llc_ssap &0xff,
			       l->llc_control & 0xff);

		}
#endif /* LLC_DEBUG */
		} break;
#endif /* LLC */

	case AF_UNSPEC:
		eh = (struct ether_header *)dst->sa_data;
 		(void)memcpy(edst, eh->ether_dhost, sizeof (edst));
		type = eh->ether_type;
		break;

	default:
		printf("%s%d: can't handle af%d\n", ifp->if_name, ifp->if_unit,
			dst->sa_family);
		senderr(EAFNOSUPPORT);
	}


	if (mcopy)
		(void) looutput(ifp, mcopy, dst, rt);
	/*
	 * Add local net header.  If no space in first mbuf,
	 * allocate another.
	 */
	M_PREPEND(m, sizeof (struct ether_header), M_DONTWAIT);
	if (m == 0)
		senderr(ENOBUFS);
	eh = mtod(m, struct ether_header *);
	(void)memcpy(&eh->ether_type, &type,
		sizeof(eh->ether_type));
 	(void)memcpy(eh->ether_dhost, edst, sizeof (edst));
 	(void)memcpy(eh->ether_shost, ac->ac_enaddr,
	    sizeof(eh->ether_shost));
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
	IF_ENQUEUE(&ifp->if_snd, m);
	if ((ifp->if_flags & IFF_OACTIVE) == 0)
		(*ifp->if_start)(ifp);
	splx(s);
	ifp->if_obytes += len + sizeof (struct ether_header);
	if (m->m_flags & M_MCAST)
		ifp->if_omcasts++;
	return (error);

bad:
	if (m)
		m_freem(m);
	return (error);
}

/*
 * Process a received Ethernet packet;
 * the packet is in the mbuf chain m without
 * the ether header, which is provided separately.
 */
void
ether_input(ifp, eh, m)
	struct ifnet *ifp;
	register struct ether_header *eh;
	struct mbuf *m;
{
	register struct ifqueue *inq;
	u_short ether_type;
	int s;
#if defined (ISO) || defined (LLC) || defined(NETATALK)
	register struct llc *l;
#endif

	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}
	ifp->if_ibytes += m->m_pkthdr.len + sizeof (*eh);
	if (eh->ether_dhost[0] & 1) {
		if (bcmp((caddr_t)etherbroadcastaddr, (caddr_t)eh->ether_dhost,
			 sizeof(etherbroadcastaddr)) == 0)
			m->m_flags |= M_BCAST;
		else
			m->m_flags |= M_MCAST;
	}
	if (m->m_flags & (M_BCAST|M_MCAST))
		ifp->if_imcasts++;

	ether_type = ntohs(eh->ether_type);

#if NVLAN > 0
	if (ether_type == vlan_proto) {
		if (vlan_input(eh, m) < 0)
			ifp->if_data.ifi_noproto++;
		return;
	}
#endif /* NVLAN > 0 */

	switch (ether_type) {
#ifdef INET
	case ETHERTYPE_IP:
		schednetisr(NETISR_IP);
		inq = &ipintrq;
		break;

	case ETHERTYPE_ARP:
		schednetisr(NETISR_ARP);
		inq = &arpintrq;
		break;
#endif
#ifdef IPX
	case ETHERTYPE_IPX:
		schednetisr(NETISR_IPX);
		inq = &ipxintrq;
		break;
#endif
#ifdef NS
	case 0x8137: /* Novell Ethernet_II Ethernet TYPE II */
		schednetisr(NETISR_NS);
		inq = &nsintrq;
		break;

#endif /* NS */
#ifdef NETATALK
        case ETHERTYPE_AT:
                schednetisr(NETISR_ATALK);
                inq = &atintrq1;
                break;
        case ETHERTYPE_AARP:
		/* probably this should be done with a NETISR as well */
                aarpinput((struct arpcom *)ifp, m); /* XXX */
                return;
#endif NETATALK
	default:
#ifdef NS
		checksum = mtod(m, ushort *);
		/* Novell 802.3 */
		if ((ether_type <= ETHERMTU) &&
			((*checksum == 0xffff) || (*checksum == 0xE0E0))){
			if(*checksum == 0xE0E0) {
				m->m_pkthdr.len -= 3;
				m->m_len -= 3;
				m->m_data += 3;
			}
				schednetisr(NETISR_NS);
				inq = &nsintrq;
				break;
		}
#endif /* NS */
#if defined (ISO) || defined (LLC) || defined(NETATALK)
		if (ether_type > ETHERMTU)
			goto dropanyway;
		l = mtod(m, struct llc *);
		switch (l->llc_dsap) {
#ifdef NETATALK
		case LLC_SNAP_LSAP:
		    switch (l->llc_control) {
		    case LLC_UI:
			if (l->llc_ssap != LLC_SNAP_LSAP)
			    goto dropanyway;
	
			if (Bcmp(&(l->llc_snap_org_code)[0], at_org_code,
				   sizeof(at_org_code)) == 0 &&
			     ntohs(l->llc_snap_ether_type) == ETHERTYPE_AT) {
			    inq = &atintrq2;
			    m_adj( m, sizeof( struct llc ));
			    schednetisr(NETISR_ATALK);
			    break;
			}

			if (Bcmp(&(l->llc_snap_org_code)[0], aarp_org_code,
				   sizeof(aarp_org_code)) == 0 &&
			     ntohs(l->llc_snap_ether_type) == ETHERTYPE_AARP) {
			    m_adj( m, sizeof( struct llc ));
			    aarpinput((struct arpcom *)ifp, m); /* XXX */
			    return;
			}
		
		    default:
			goto dropanyway;
		    }
		    break;
#endif NETATALK	
#ifdef	ISO
		case LLC_ISO_LSAP:
			switch (l->llc_control) {
			case LLC_UI:
				/* LLC_UI_P forbidden in class 1 service */
				if ((l->llc_dsap == LLC_ISO_LSAP) &&
				    (l->llc_ssap == LLC_ISO_LSAP)) {
					/* LSAP for ISO */
					if (m->m_pkthdr.len > ether_type)
						m_adj(m, ether_type - m->m_pkthdr.len);
					m->m_data += 3;		/* XXX */
					m->m_len -= 3;		/* XXX */
					m->m_pkthdr.len -= 3;	/* XXX */
					M_PREPEND(m, sizeof *eh, M_DONTWAIT);
					if (m == 0)
						return;
					*mtod(m, struct ether_header *) = *eh;
					IFDEBUG(D_ETHER)
						printf("clnp packet");
					ENDDEBUG
					schednetisr(NETISR_ISO);
					inq = &clnlintrq;
					break;
				}
				goto dropanyway;

			case LLC_XID:
			case LLC_XID_P:
				if(m->m_len < 6)
					goto dropanyway;
				l->llc_window = 0;
				l->llc_fid = 9;
				l->llc_class = 1;
				l->llc_dsap = l->llc_ssap = 0;
				/* Fall through to */
			case LLC_TEST:
			case LLC_TEST_P:
			{
				struct sockaddr sa;
				register struct ether_header *eh2;
				int i;
				u_char c = l->llc_dsap;

				l->llc_dsap = l->llc_ssap;
				l->llc_ssap = c;
				if (m->m_flags & (M_BCAST | M_MCAST))
					bcopy((caddr_t)ac->ac_enaddr,
					      (caddr_t)eh->ether_dhost, 6);
				sa.sa_family = AF_UNSPEC;
				sa.sa_len = sizeof(sa);
				eh2 = (struct ether_header *)sa.sa_data;
				for (i = 0; i < 6; i++) {
					eh2->ether_shost[i] = c = eh->ether_dhost[i];
					eh2->ether_dhost[i] =
						eh->ether_dhost[i] = eh->ether_shost[i];
					eh->ether_shost[i] = c;
				}
				ifp->if_output(ifp, m, &sa, NULL);
				return;
			}
			default:
				m_freem(m);
				return;
			}
			break;
#endif /* ISO */
#ifdef LLC
		case LLC_X25_LSAP:
		{
			if (m->m_pkthdr.len > ether_type)
				m_adj(m, ether_type - m->m_pkthdr.len);
			M_PREPEND(m, sizeof(struct sdl_hdr) , M_DONTWAIT);
			if (m == 0)
				return;
			if ( !sdl_sethdrif(ifp, eh->ether_shost, LLC_X25_LSAP,
					    eh->ether_dhost, LLC_X25_LSAP, 6,
					    mtod(m, struct sdl_hdr *)))
				panic("ETHER cons addr failure");
			mtod(m, struct sdl_hdr *)->sdlhdr_len = ether_type;
#ifdef LLC_DEBUG
				printf("llc packet\n");
#endif /* LLC_DEBUG */
			schednetisr(NETISR_CCITT);
			inq = &llcintrq;
			break;
		}
#endif /* LLC */
		dropanyway:
		default:
			m_freem(m);
			return;
		}
#else /* ISO || LLC || NETATALK */
	    m_freem(m);
	    return;
#endif /* ISO || LLC || NETATALK */
	}

	s = splimp();
	if (IF_QFULL(inq)) {
		IF_DROP(inq);
		m_freem(m);
	} else
		IF_ENQUEUE(inq, m);
	splx(s);
}

/*
 * Perform common duties while attaching to interface list
 */
void
ether_ifattach(ifp)
	register struct ifnet *ifp;
{
	register struct ifaddr *ifa;
	register struct sockaddr_dl *sdl;

	ifp->if_type = IFT_ETHER;
	ifp->if_addrlen = 6;
	ifp->if_hdrlen = 14;
	ifp->if_mtu = ETHERMTU;
	ifp->if_resolvemulti = ether_resolvemulti;
	if (ifp->if_baudrate == 0)
	    ifp->if_baudrate = 10000000;
	ifa = ifnet_addrs[ifp->if_index - 1];
	if (ifa == 0) {
		printf("ether_ifattach: no lladdr!\n");
		return;
	}
	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	sdl->sdl_type = IFT_ETHER;
	sdl->sdl_alen = ifp->if_addrlen;
	bcopy(((struct arpcom *)ifp)->ac_enaddr, LLADDR(sdl), ifp->if_addrlen);
}

SYSCTL_NODE(_net_link, IFT_ETHER, ether, CTLFLAG_RW, 0, "Ethernet");

int
ether_ioctl(ifp, command, data)
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
			arp_ifinit((struct arpcom *)ifp, ifa);
			break;
#endif
#ifdef IPX
		/*
		 * XXX - This code is probably wrong
		 */
		case AF_IPX:
			{
			register struct ipx_addr *ina = &(IA_SIPX(ifa)->sipx_addr);
			struct arpcom *ac = (struct arpcom *) (ifp->if_softc);

			if (ipx_nullhost(*ina))
				ina->x_host =
				    *(union ipx_host *) 
			            ac->ac_enaddr;
			else {
				bcopy((caddr_t) ina->x_host.c_host,
				      (caddr_t) ac->ac_enaddr,
				      sizeof(ac->ac_enaddr));
			}

			/*
			 * Set new address
			 */
			ifp->if_init(ifp->if_softc);
			break;
			}
#endif
#ifdef NS
		/*
		 * XXX - This code is probably wrong
		 */
		case AF_NS:
		{
			register struct ns_addr *ina = &(IA_SNS(ifa)->sns_addr);
			struct arpcom *ac = (struct arpcom *) (ifp->if_softc);

			if (ns_nullhost(*ina))
				ina->x_host =
				    *(union ns_host *) (ac->ac_enaddr);
			else {
				bcopy((caddr_t) ina->x_host.c_host,
				      (caddr_t) ac->ac_enaddr,
				      sizeof(ac->ac_enaddr));
			}

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

			sa = (struct sockaddr *) & ifr->ifr_data;
			bcopy(((struct arpcom *)ifp->if_softc)->ac_enaddr,
			      (caddr_t) sa->sa_data, ETHER_ADDR_LEN);
		}
		break;

	case SIOCSIFMTU:
		/*
		 * Set the interface MTU.
		 */
		if (ifr->ifr_mtu > ETHERMTU) {
			error = EINVAL;
		} else {
			ifp->if_mtu = ifr->ifr_mtu;
		}
		break;
	}
	return (error);
}

int
ether_resolvemulti(ifp, llsa, sa)
	struct ifnet *ifp;
	struct sockaddr **llsa;
	struct sockaddr *sa;
{
	struct sockaddr_dl *sdl;
	struct sockaddr_in *sin;
	u_char *e_addr;

	switch(sa->sa_family) {
	case AF_LINK:
		/* 
		 * No mapping needed. Just check that it's a valid MC address.
		 */
		sdl = (struct sockaddr_dl *)sa;
		e_addr = LLADDR(sdl);
		if ((e_addr[0] & 1) != 1)
			return EADDRNOTAVAIL;
		*llsa = 0;
		return 0;

#ifdef INET
	case AF_INET:
		sin = (struct sockaddr_in *)sa;
		if (!IN_MULTICAST(ntohl(sin->sin_addr.s_addr)))
			return EADDRNOTAVAIL;
		MALLOC(sdl, struct sockaddr_dl *, sizeof *sdl, M_IFMADDR,
		       M_WAITOK);
		sdl->sdl_len = sizeof *sdl;
		sdl->sdl_family = AF_LINK;
		sdl->sdl_index = ifp->if_index;
		sdl->sdl_type = IFT_ETHER;
		sdl->sdl_nlen = 0;
		sdl->sdl_alen = ETHER_ADDR_LEN;
		sdl->sdl_slen = 0;
		e_addr = LLADDR(sdl);
		ETHER_MAP_IP_MULTICAST(&sin->sin_addr, e_addr);
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
