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
 * $FreeBSD$
 */

#include "opt_atalk.h"
#include "opt_inet.h"
#include "opt_ipx.h"
#include "opt_bdg.h"
#include "opt_netgraph.h"

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

#ifdef BRIDGE
#include <net/bridge.h>
#endif

#include "vlan.h"
#if NVLAN > 0
#include <net/if_vlan_var.h>
#endif /* NVLAN > 0 */

static	int ether_resolvemulti __P((struct ifnet *, struct sockaddr **, 
				    struct sockaddr *));
u_char	etherbroadcastaddr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
#define senderr(e) do { error = (e); goto bad;} while (0)
#define IFP2AC(IFP) ((struct arpcom *)IFP)

#ifdef NETGRAPH
#include <netgraph/ng_ether.h>
#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>

static	void	ngether_init(void* ignored);
static void	ngether_send(struct arpcom *ac,
			struct ether_header *eh, struct mbuf *m);
static	ng_constructor_t	ngether_constructor;
static	ng_rcvmsg_t		ngether_rcvmsg;
static	ng_shutdown_t		ngether_rmnode;
static	ng_newhook_t		ngether_newhook;
static	ng_connect_t		ngether_connect;
static	ng_rcvdata_t		ngether_rcvdata;
static	ng_disconnect_t		ngether_disconnect;

static struct ng_type typestruct = {
	NG_VERSION,
	NG_ETHER_NODE_TYPE,
	NULL,
	ngether_constructor,
	ngether_rcvmsg,
	ngether_rmnode,
	ngether_newhook,
	NULL,
	ngether_connect,
	ngether_rcvdata,
	ngether_rcvdata,
	ngether_disconnect 
};

#define AC2NG(AC) ((node_p)((AC)->ac_ng))
#define NGEF_DIVERT NGF_TYPE1	/* all packets sent to netgraph */
#endif /* NETGRAPH */

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
	int s, error = 0, hdrcmplt = 0;
 	u_char esrc[6], edst[6];
	register struct mbuf *m = m0;
	register struct rtentry *rt;
	register struct ether_header *eh;
	int off, len = m->m_pkthdr.len, loop_copy = 0;
	int hlen;	/* link layer header lenght */
	struct arpcom *ac = IFP2AC(ifp);

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
	hlen = ETHER_HDR_LEN;
	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:
		if (!arpresolve(ac, rt, m, dst, edst, rt0))
			return (0);	/* if not yet resolved */
		off = m->m_pkthdr.len - m->m_len;
		type = htons(ETHERTYPE_IP);
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
	case AF_APPLETALK:
	  {
	    struct at_ifaddr *aa;

	    if ((aa = at_ifawithnet((struct sockaddr_at *)dst)) == NULL) {
		    goto bad;
	    }
	    if (!aarpresolve(ac, m, (struct sockaddr_at *)dst, edst))
		    return (0);
	    /*
	     * In the phase 2 case, need to prepend an mbuf for the llc header.
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
		hlen = sizeof(struct llc) + ETHER_HDR_LEN;
	    } else {
		type = htons(ETHERTYPE_AT);
	    }
	    break;
	  }
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
		/*
		 * XXX if ns_thishost is the same as the node's ethernet
		 * address then just the default code will catch this anyhow.
		 * So I'm not sure if this next clause should be here at all?
		 * [JRE]
		 */
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
			m->m_flags |= M_BCAST;
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
			bcopy(LLADDR(sdl), (char *)edst, sizeof(edst));
		} else goto bad; /* Not a link interface ? Funny ... */
		if (*edst & 1)
			loop_copy = 1;
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

	case pseudo_AF_HDRCMPLT:
		hdrcmplt = 1;
		eh = (struct ether_header *)dst->sa_data;
		(void)memcpy(esrc, eh->ether_shost, sizeof (esrc));
		/* FALLTHROUGH */

	case AF_UNSPEC:
		loop_copy = -1; /* if this is for us, don't do it */
		eh = (struct ether_header *)dst->sa_data;
 		(void)memcpy(edst, eh->ether_dhost, sizeof (edst));
		type = eh->ether_type;
		break;

	default:
		printf("%s%d: can't handle af%d\n", ifp->if_name, ifp->if_unit,
			dst->sa_family);
		senderr(EAFNOSUPPORT);
	}

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
	if (hdrcmplt)
		(void)memcpy(eh->ether_shost, esrc,
			sizeof(eh->ether_shost));
	else
		(void)memcpy(eh->ether_shost, ac->ac_enaddr,
			sizeof(eh->ether_shost));

	/*
	 * If a simplex interface, and the packet is being sent to our
	 * Ethernet address or a broadcast address, loopback a copy.
	 * XXX To make a simplex device behave exactly like a duplex
	 * device, we should copy in the case of sending to our own
	 * ethernet address (thus letting the original actually appear
	 * on the wire). However, we don't do that here for security
	 * reasons and compatibility with the original behavior.
	 */
	if ((ifp->if_flags & IFF_SIMPLEX) && (loop_copy != -1)) {
		if ((m->m_flags & M_BCAST) || (loop_copy > 0)) {
			struct mbuf *n = m_copy(m, 0, (int)M_COPYALL);

			(void) if_simloop(ifp, n, dst, hlen);
		} else if (bcmp(eh->ether_dhost,
		    eh->ether_shost, ETHER_ADDR_LEN) == 0) {
			(void) if_simloop(ifp, m, dst, hlen);
			return (0);	/* XXX */
		}
	}
#ifdef BRIDGE
	if (do_bridge) {
		struct mbuf *m0 = m ;

		if (m->m_pkthdr.rcvif)
			m->m_pkthdr.rcvif = NULL ;
		ifp = bridge_dst_lookup(m);
		bdg_forward(&m0, ifp);
		if (m0)
			m_freem(m0);
		return (0);
	}
#endif
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

#ifdef	NETGRAPH
	{
		struct arpcom *ac = IFP2AC(ifp);
		if (AC2NG(ac) && (AC2NG(ac)->flags & NGEF_DIVERT)) {
			ngether_send(ac, eh, m);
			return;
		}
	}
#endif	/* NETGRAPH */
		
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
		if (ipflow_fastforward(m))
			return;
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
                aarpinput(IFP2AC(ifp), m); /* XXX */
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
			    aarpinput(IFP2AC(ifp), m); /* XXX */
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
#ifdef	NETGRAPH
			ngether_send(IFP2AC(ifp), eh, m);
#else	/* NETGRAPH */
			m_freem(m);
#endif	/* NETGRAPH */
			return;
		}
#else /* ISO || LLC || NETATALK */
#ifdef	NETGRAPH
	    ngether_send(IFP2AC(ifp), eh, m);
#else	/* NETGRAPH */
	    m_freem(m);
#endif	/* NETGRAPH */
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
	bcopy((IFP2AC(ifp))->ac_enaddr, LLADDR(sdl), ifp->if_addrlen);
#ifdef	NETGRAPH
	ngether_init(ifp);
#endif /* NETGRAPH */
}

SYSCTL_DECL(_net_link);
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
			arp_ifinit(IFP2AC(ifp), ifa);
			break;
#endif
#ifdef IPX
		/*
		 * XXX - This code is probably wrong
		 */
		case AF_IPX:
			{
			register struct ipx_addr *ina = &(IA_SIPX(ifa)->sipx_addr);
			struct arpcom *ac = IFP2AC(ifp);

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
			struct arpcom *ac = IFP2AC(ifp);

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
			bcopy(IFP2AC(ifp)->ac_enaddr,
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

#ifdef	NETGRAPH

/***********************************************************************
 * This section contains the methods for the Netgraph interface
 ***********************************************************************/
/* It's Ascii-art time!
 * The ifnet is the first part of the arpcom which must be
 * the first part of the device's softc.. yuk.
 *
 *      +--------------------------+-----+---------+
 *      |   struct ifnet (*ifp)    |     |         |
 *      |                          |     |         |
 *      +--------------------------+     |         |
 *   +--|[ac_ng]     struct arpcom (*ac) |         |
 *   |  +--------------------------------+         |
 *   |  |   struct softc (*ifp->if_softc) (device) |
 *   |  +------------------------------------------+
 *   |               ^
 * AC2NG()           |
 *   |               v
 *   |       +----------------------+
 *   |       |   [private] [flags]  |
 *   +------>| struct ng_node       |
 *           |    [hooks]           | ** we only allow one hook
 *           +----------------------+
 *                   ^
 *                   |
 *                   v
 *           +-------------+
 *           |    [node]   |
 *           |    hook     |
 *           |    [private]|-- *unused*
 *           +-------------+
 */

/*
 * called during interface attaching
 */
static void
ngether_init(void *ifpvoid)
{
	struct	ifnet *ifp = ifpvoid;
	struct arpcom *ac = IFP2AC(ifp);
	static int	ngether_done_init;
	char	namebuf[32];
	node_p node;

	/*
	 * we have found a node, make sure our 'type' is availabe.
	 */
	if (ngether_done_init == 0) {
		if (ng_newtype(&typestruct)) {
			printf("ngether install failed\n");
			return;
		}
		ngether_done_init = 1;
	}
	if (ng_make_node_common(&typestruct, &node) != 0)
		return;
	ac->ac_ng = node;
	node->private = ifp;
	sprintf(namebuf, "%s%d", ifp->if_name, ifp->if_unit);
	ng_name_node(AC2NG(ac), namebuf);
}

/*
 * It is not possible or allowable to create a node of this type.
 * If the hardware exists, it will already have created it.
 */
static	int
ngether_constructor(node_p *nodep)
{
	return (EINVAL);
}

/*
 * Give our ok for a hook to be added...
 * 
 * Allow one hook at a time (rawdata). 
 * It can eiteh rdivert everything or only unclaimed packets.
 */
static	int
ngether_newhook(node_p node, hook_p hook, const char *name)
{

	/* check if there is already a hook */
	if (LIST_FIRST(&(node->hooks)))
		return(EISCONN);
	/*
	 * Check for which mode hook we want.
	 */
	if (strcmp(name, NG_ETHER_HOOK_ORPHAN) != 0) {
		if (strcmp(name, NG_ETHER_HOOK_DIVERT) != 0) {
			return (EINVAL);
		}
		node->flags |= NGEF_DIVERT;
	} else {
		node->flags &= ~NGEF_DIVERT;
	}
	return (0);
}

/*
 * incoming messages.
 * Just respond to the generic TEXT_STATUS message
 */
static	int
ngether_rcvmsg(node_p node,
	struct ng_mesg *msg, const char *retaddr, struct ng_mesg **resp)
{
	struct ifnet	*ifp;
	int error = 0;

	ifp = node->private;
	switch (msg->header.typecookie) {
	    case	NGM_ETHER_COOKIE: 
		error = EINVAL;
		break;
	    case	NGM_GENERIC_COOKIE: 
		switch(msg->header.cmd) {
		    case NGM_TEXT_STATUS: {
			    char	*arg;
			    int pos = 0;
			    int resplen = sizeof(struct ng_mesg) + 512;
			    MALLOC(*resp, struct ng_mesg *, resplen,
					M_NETGRAPH, M_NOWAIT);
			    if (*resp == NULL) { 
				error = ENOMEM;
				break;
			    }       
			    bzero(*resp, resplen);
			    arg = (*resp)->data;

			    /*
			     * Put in the throughput information.
			     */
			    pos = sprintf(arg, "%ld bytes in, %ld bytes out\n",
			    ifp->if_ibytes, ifp->if_obytes);
			    pos += sprintf(arg + pos,
				"%ld output errors\n",
			    	ifp->if_oerrors);
			    pos += sprintf(arg + pos,
				"ierrors = %ld\n",
			    	ifp->if_ierrors);

			    (*resp)->header.version = NG_VERSION;
			    (*resp)->header.arglen = strlen(arg) + 1;
			    (*resp)->header.token = msg->header.token;
			    (*resp)->header.typecookie = NGM_ETHER_COOKIE;
			    (*resp)->header.cmd = msg->header.cmd;
			    strncpy((*resp)->header.cmdstr, "status",
					NG_CMDSTRLEN);
			}
			break;
	    	    default:
		 	error = EINVAL;
		 	break;
		    }
		break;
	    default:
		error = EINVAL;
		break;
	}
	free(msg, M_NETGRAPH);
	return (error);
}

/*
 * Receive a completed ethernet packet.
 * Queue it for output.
 */
static	int
ngether_rcvdata(hook_p hook, struct mbuf *m, meta_p meta)
{
	struct ifnet *ifp;
	int	error = 0;
	int	s;
	struct ether_header *eh;
	
	ifp = hook->node->private;

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
		senderr(ENETDOWN);
	/* drop in the MAC address */
	eh = mtod(m, struct ether_header *);
	bcopy(IFP2AC(ifp)->ac_enaddr, eh->ether_shost, 6);
	/*
	 * If a simplex interface, and the packet is being sent to our
	 * Ethernet address or a broadcast address, loopback a copy.
	 * XXX To make a simplex device behave exactly like a duplex
	 * device, we should copy in the case of sending to our own
	 * ethernet address (thus letting the original actually appear
	 * on the wire). However, we don't do that here for security
	 * reasons and compatibility with the original behavior.
	 */
	if (ifp->if_flags & IFF_SIMPLEX) {
		if (m->m_flags & M_BCAST) {
			struct mbuf *n = m_copy(m, 0, (int)M_COPYALL);

			ng_queue_data(hook, n, meta);
		} else if (bcmp(eh->ether_dhost,
		    eh->ether_shost, ETHER_ADDR_LEN) == 0) {
			ng_queue_data(hook, m, meta);
			return (0);	/* XXX */
		}
	}
	s = splimp();
	/*
	 * Queue message on interface, and start output if interface
	 * not yet active.
	 * XXX if we lookead at the priority in the meta data we could
	 * queue high priority items at the head.
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
	ifp->if_obytes += m->m_pkthdr.len;
	if (m->m_flags & M_MCAST)
		ifp->if_omcasts++;
	return (error);

bad:
	NG_FREE_DATA(m, meta);
	return (error);
}

/*
 * pass an mbuf out to the connected hook
 * More complicated than just an m_prepend, as it tries to save later nodes
 * from needing to do lots of m_pullups.
 */	     
static void
ngether_send(struct arpcom *ac, struct ether_header *eh, struct mbuf *m)
{       
	int room;
	node_p node = AC2NG(ac);
	struct ether_header *eh2;
		
	if (node && LIST_FIRST(&(node->hooks))) {
		/*
		 * Possibly the header is already on the front,
		 */
		eh2 = mtod(m, struct ether_header *) - 1;
		if ( eh == eh2) {
			/*
			 * This is the case so just move the markers back to 
			 * re-include it. We lucked out.
			 * This allows us to avoid a yucky m_pullup
			 * in later nodes if it works.
			 */ 
			m->m_len += sizeof(*eh); 
			m->m_data -= sizeof(*eh);
			m->m_pkthdr.len += sizeof(*eh);
		} else { 
			/*
			 * Alternatively there may be room even though
			 * it is stored somewhere else. If so, copy it in.
			 * This only safe because we KNOW that this packet has
			 * just been generated by an ethernet card, so there
			 * are no aliases to the buffer. (unlike in outgoing
			 * packets).
			 * Nearly all ethernet cards will end up producing mbufs
			 * that fall into these cases. So we are not optimising
			 * contorted cases.
			 */
	      
			if (m->m_flags & M_EXT) {
				room = (mtod(m, caddr_t) - m->m_ext.ext_buf);
				if (room > m->m_ext.ext_size) /* garbage */
					room = 0; /* fail immediatly */
			} else {
				room = (mtod(m, caddr_t) - m->m_pktdat);
			}
			if (room > sizeof (*eh)) {  
				/* we have room, just copy it and adjust */
				m->m_len += sizeof(*eh);
				m->m_data -= sizeof(*eh);
				m->m_pkthdr.len += sizeof(*eh);
				bcopy ((caddr_t)eh, (caddr_t)eh2, sizeof(*eh));
			} else {
				/*
				 * Doing anything more is likely to get more 
				 * expensive than it's worth..
				 * it's probable that everything else is in one
				 * big lump. The next node will do an m_pullup()
				 * for exactly the amount of data it needs and
				 * hopefully everything after that will not
				 * need one. So let's just use m_prepend.
				 */
				m = m_prepend(m, MHLEN, M_DONTWAIT);
				if (m == NULL)
					return;
			}
		}
		ng_queue_data(LIST_FIRST(&(node->hooks)), m, NULL);
	} else {
		m_freem(m);
	}
}

/*
 * do local shutdown processing..
 * This node will refuse to go away, unless the hardware says to..
 * don't unref the node, or remove our name. just clear our links up.
 */
static	int
ngether_rmnode(node_p node)
{
	ng_cutlinks(node);
	node->flags &= ~NG_INVALID; /* bounce back to life */
	return (0);
}

/* already linked */
static	int
ngether_connect(hook_p hook)
{
	/* be really amiable and just say "YUP that's OK by me! " */
	return (0);
}

/*
 * notify on hook disconnection (destruction)
 *
 * For this type, removal of the last lins no effect. The interface can run 
 * independently.
 * Since we have no per-hook information, this is rather simple.
 */
static	int
ngether_disconnect(hook_p hook)
{
	hook->node->flags &= ~NGEF_DIVERT;
	return (0);
}
#endif /* NETGRAPH */

/********************************** END *************************************/
