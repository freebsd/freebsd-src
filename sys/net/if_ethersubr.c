/*-
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
#include "opt_inet6.h"
#include "opt_ipx.h"
#include "opt_bdg.h"
#include "opt_mac.h"
#include "opt_netgraph.h"
#include "opt_carp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mbuf.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/if_llc.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/bridge.h>
#include <net/if_bridgevar.h>
#include <net/if_vlan_var.h>

#if defined(INET) || defined(INET6)
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_dummynet.h>
#endif
#ifdef INET6
#include <netinet6/nd6.h>
#endif

#ifdef DEV_CARP
#include <netinet/ip_carp.h>
#endif

#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif
int (*ef_inputp)(struct ifnet*, struct ether_header *eh, struct mbuf *m);
int (*ef_outputp)(struct ifnet *ifp, struct mbuf **mp,
		struct sockaddr *dst, short *tp, int *hlen);

#ifdef NETATALK
#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/at_extern.h>

#define llc_snap_org_code llc_un.type_snap.org_code
#define llc_snap_ether_type llc_un.type_snap.ether_type

extern u_char	at_org_code[3];
extern u_char	aarp_org_code[3];
#endif /* NETATALK */

/* netgraph node hooks for ng_ether(4) */
void	(*ng_ether_input_p)(struct ifnet *ifp, struct mbuf **mp);
void	(*ng_ether_input_orphan_p)(struct ifnet *ifp, struct mbuf *m);
int	(*ng_ether_output_p)(struct ifnet *ifp, struct mbuf **mp);
void	(*ng_ether_attach_p)(struct ifnet *ifp);
void	(*ng_ether_detach_p)(struct ifnet *ifp);

void	(*vlan_input_p)(struct ifnet *, struct mbuf *);

/* bridge support */
int do_bridge;
bridge_in_t *bridge_in_ptr;
bdg_forward_t *bdg_forward_ptr;
bdgtakeifaces_t *bdgtakeifaces_ptr;
struct bdg_softc *ifp2sc;

struct mbuf *(*bridge_input_p)(struct ifnet *, struct mbuf *); 
int	(*bridge_output_p)(struct ifnet *, struct mbuf *, 
		struct sockaddr *, struct rtentry *);
void	(*bridge_dn_p)(struct mbuf *, struct ifnet *);

/* if_lagg(4) support */
struct mbuf *(*lagg_input_p)(struct ifnet *, struct mbuf *); 

static const u_char etherbroadcastaddr[ETHER_ADDR_LEN] =
			{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static	int ether_resolvemulti(struct ifnet *, struct sockaddr **,
		struct sockaddr *);

/* XXX: should be in an arp support file, not here */
MALLOC_DEFINE(M_ARPCOM, "arpcom", "802.* interface internals");

#define senderr(e) do { error = (e); goto bad;} while (0)

#if defined(INET) || defined(INET6)
int
ether_ipfw_chk(struct mbuf **m0, struct ifnet *dst,
	struct ip_fw **rule, int shared);
static int ether_ipfw;
#endif

/*
 * Ethernet output routine.
 * Encapsulate a packet of type family for the local net.
 * Use trailer local net encapsulation if enough data in first
 * packet leaves a multiple of 512 bytes of data in remainder.
 */
int
ether_output(struct ifnet *ifp, struct mbuf *m,
	struct sockaddr *dst, struct rtentry *rt0)
{
	short type;
	int error, hdrcmplt = 0;
	u_char esrc[ETHER_ADDR_LEN], edst[ETHER_ADDR_LEN];
	struct ether_header *eh;
	int loop_copy = 1;
	int hlen;	/* link layer header length */

#ifdef MAC
	error = mac_check_ifnet_transmit(ifp, m);
	if (error)
		senderr(error);
#endif

	if (ifp->if_flags & IFF_MONITOR)
		senderr(ENETDOWN);
	if (!((ifp->if_flags & IFF_UP) &&
	    (ifp->if_drv_flags & IFF_DRV_RUNNING)))
		senderr(ENETDOWN);

	hlen = ETHER_HDR_LEN;
	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:
		error = arpresolve(ifp, rt0, m, dst, edst);
		if (error)
			return (error == EWOULDBLOCK ? 0 : error);
		type = htons(ETHERTYPE_IP);
		break;
	case AF_ARP:
	{
		struct arphdr *ah;
		ah = mtod(m, struct arphdr *);
		ah->ar_hrd = htons(ARPHRD_ETHER);

		loop_copy = 0; /* if this is for us, don't do it */

		switch(ntohs(ah->ar_op)) {
		case ARPOP_REVREQUEST:
		case ARPOP_REVREPLY:
			type = htons(ETHERTYPE_REVARP);
			break;
		case ARPOP_REQUEST:
		case ARPOP_REPLY:
		default:
			type = htons(ETHERTYPE_ARP);
			break;
		}

		if (m->m_flags & M_BCAST)
			bcopy(ifp->if_broadcastaddr, edst, ETHER_ADDR_LEN);
		else
			bcopy(ar_tha(ah), edst, ETHER_ADDR_LEN);

	}
	break;
#endif
#ifdef INET6
	case AF_INET6:
		error = nd6_storelladdr(ifp, rt0, m, dst, (u_char *)edst);
		if (error)
			return error;
		type = htons(ETHERTYPE_IPV6);
		break;
#endif
#ifdef IPX
	case AF_IPX:
		if (ef_outputp) {
		    error = ef_outputp(ifp, &m, dst, &type, &hlen);
		    if (error)
			goto bad;
		} else
		    type = htons(ETHERTYPE_IPX);
		bcopy((caddr_t)&(((struct sockaddr_ipx *)dst)->sipx_addr.x_host),
		    (caddr_t)edst, sizeof (edst));
		break;
#endif
#ifdef NETATALK
	case AF_APPLETALK:
	  {
	    struct at_ifaddr *aa;

	    if ((aa = at_ifawithnet((struct sockaddr_at *)dst)) == NULL)
		    senderr(EHOSTUNREACH); /* XXX */
	    if (!aarpresolve(ifp, m, (struct sockaddr_at *)dst, edst))
		    return (0);
	    /*
	     * In the phase 2 case, need to prepend an mbuf for the llc header.
	     */
	    if ( aa->aa_flags & AFA_PHASE2 ) {
		struct llc llc;

		M_PREPEND(m, LLC_SNAPFRAMELEN, M_DONTWAIT);
		if (m == NULL)
			senderr(ENOBUFS);
		llc.llc_dsap = llc.llc_ssap = LLC_SNAP_LSAP;
		llc.llc_control = LLC_UI;
		bcopy(at_org_code, llc.llc_snap_org_code, sizeof(at_org_code));
		llc.llc_snap_ether_type = htons( ETHERTYPE_AT );
		bcopy(&llc, mtod(m, caddr_t), LLC_SNAPFRAMELEN);
		type = htons(m->m_pkthdr.len);
		hlen = LLC_SNAPFRAMELEN + ETHER_HDR_LEN;
	    } else {
		type = htons(ETHERTYPE_AT);
	    }
	    break;
	  }
#endif /* NETATALK */

	case pseudo_AF_HDRCMPLT:
		hdrcmplt = 1;
		eh = (struct ether_header *)dst->sa_data;
		(void)memcpy(esrc, eh->ether_shost, sizeof (esrc));
		/* FALLTHROUGH */

	case AF_UNSPEC:
		loop_copy = 0; /* if this is for us, don't do it */
		eh = (struct ether_header *)dst->sa_data;
		(void)memcpy(edst, eh->ether_dhost, sizeof (edst));
		type = eh->ether_type;
		break;

	default:
		if_printf(ifp, "can't handle af%d\n", dst->sa_family);
		senderr(EAFNOSUPPORT);
	}

	/*
	 * Add local net header.  If no space in first mbuf,
	 * allocate another.
	 */
	M_PREPEND(m, ETHER_HDR_LEN, M_DONTWAIT);
	if (m == NULL)
		senderr(ENOBUFS);
	eh = mtod(m, struct ether_header *);
	(void)memcpy(&eh->ether_type, &type,
		sizeof(eh->ether_type));
	(void)memcpy(eh->ether_dhost, edst, sizeof (edst));
	if (hdrcmplt)
		(void)memcpy(eh->ether_shost, esrc,
			sizeof(eh->ether_shost));
	else
		(void)memcpy(eh->ether_shost, IFP2ENADDR(ifp),
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
	if ((ifp->if_flags & IFF_SIMPLEX) && loop_copy &&
	    m_tag_find(m, PACKET_TAG_PF_ROUTED, NULL) == NULL) {
		int csum_flags = 0;

		if (m->m_pkthdr.csum_flags & CSUM_IP)
			csum_flags |= (CSUM_IP_CHECKED|CSUM_IP_VALID);
		if (m->m_pkthdr.csum_flags & CSUM_DELAY_DATA)
			csum_flags |= (CSUM_DATA_VALID|CSUM_PSEUDO_HDR);

		if (m->m_flags & M_BCAST) {
			struct mbuf *n;

			if ((n = m_copy(m, 0, (int)M_COPYALL)) != NULL) {
				n->m_pkthdr.csum_flags |= csum_flags;
				if (csum_flags & CSUM_DATA_VALID)
					n->m_pkthdr.csum_data = 0xffff;
				(void)if_simloop(ifp, n, dst->sa_family, hlen);
			} else
				ifp->if_iqdrops++;
		} else if (bcmp(eh->ether_dhost, eh->ether_shost,
				ETHER_ADDR_LEN) == 0) {
			m->m_pkthdr.csum_flags |= csum_flags;
			if (csum_flags & CSUM_DATA_VALID)
				m->m_pkthdr.csum_data = 0xffff;
			(void) if_simloop(ifp, m, dst->sa_family, hlen);
			return (0);	/* XXX */
		}
	}

       /*
	* Bridges require special output handling.
	*/
	if (ifp->if_bridge) {
		BRIDGE_OUTPUT(ifp, m, error);
		return (error);
	}

#ifdef DEV_CARP
	if (ifp->if_carp &&
	    (error = carp_output(ifp, m, dst, NULL)))
		goto bad;
#endif

	/* Handle ng_ether(4) processing, if any */
	if (IFP2AC(ifp)->ac_netgraph != NULL) {
		if ((error = (*ng_ether_output_p)(ifp, &m)) != 0) {
bad:			if (m != NULL)
				m_freem(m);
			return (error);
		}
		if (m == NULL)
			return (0);
	}

	/* Continue with link-layer output */
	return ether_output_frame(ifp, m);
}

/*
 * Ethernet link layer output routine to send a raw frame to the device.
 *
 * This assumes that the 14 byte Ethernet header is present and contiguous
 * in the first mbuf (if BRIDGE'ing).
 */
int
ether_output_frame(struct ifnet *ifp, struct mbuf *m)
{
#if defined(INET) || defined(INET6)
	struct ip_fw *rule = ip_dn_claim_rule(m);
#else
	void *rule = NULL;
#endif
	int error;

	if (rule == NULL && BDG_ACTIVE(ifp)) {
		/*
		 * Beware, the bridge code notices the null rcvif and
		 * uses that identify that it's being called from
		 * ether_output as opposd to ether_input.  Yech.
		 */
		m->m_pkthdr.rcvif = NULL;
		m = bdg_forward_ptr(m, ifp);
		if (m != NULL)
			m_freem(m);
		return (0);
	}
#if defined(INET) || defined(INET6)
	if (IPFW_LOADED && ether_ipfw != 0) {
		if (ether_ipfw_chk(&m, ifp, &rule, 0) == 0) {
			if (m) {
				m_freem(m);
				return EACCES;	/* pkt dropped */
			} else
				return 0;	/* consumed e.g. in a pipe */
		}
	}
#endif

	/*
	 * Queue message on interface, update output statistics if
	 * successful, and start output if interface not yet active.
	 */
	IFQ_HANDOFF(ifp, m, error);
	return (error);
}

#if defined(INET) || defined(INET6)
/*
 * ipfw processing for ethernet packets (in and out).
 * The second parameter is NULL from ether_demux, and ifp from
 * ether_output_frame. This section of code could be used from
 * bridge.c as well as long as we use some extra info
 * to distinguish that case from ether_output_frame();
 */
int
ether_ipfw_chk(struct mbuf **m0, struct ifnet *dst,
	struct ip_fw **rule, int shared)
{
	struct ether_header *eh;
	struct ether_header save_eh;
	struct mbuf *m;
	int i;
	struct ip_fw_args args;

	if (*rule != NULL && fw_one_pass)
		return 1; /* dummynet packet, already partially processed */

	/*
	 * I need some amt of data to be contiguous, and in case others need
	 * the packet (shared==1) also better be in the first mbuf.
	 */
	m = *m0;
	i = min( m->m_pkthdr.len, max_protohdr);
	if ( shared || m->m_len < i) {
		m = m_pullup(m, i);
		if (m == NULL) {
			*m0 = m;
			return 0;
		}
	}
	eh = mtod(m, struct ether_header *);
	save_eh = *eh;			/* save copy for restore below */
	m_adj(m, ETHER_HDR_LEN);	/* strip ethernet header */

	args.m = m;		/* the packet we are looking at		*/
	args.oif = dst;		/* destination, if any			*/
	args.rule = *rule;	/* matching rule to restart		*/
	args.next_hop = NULL;	/* we do not support forward yet	*/
	args.eh = &save_eh;	/* MAC header for bridged/MAC packets	*/
	args.inp = NULL;	/* used by ipfw uid/gid/jail rules	*/
	i = ip_fw_chk_ptr(&args);
	m = args.m;
	if (m != NULL) {
		/*
		 * Restore Ethernet header, as needed, in case the
		 * mbuf chain was replaced by ipfw.
		 */
		M_PREPEND(m, ETHER_HDR_LEN, M_DONTWAIT);
		if (m == NULL) {
			*m0 = m;
			return 0;
		}
		if (eh != mtod(m, struct ether_header *))
			bcopy(&save_eh, mtod(m, struct ether_header *),
				ETHER_HDR_LEN);
	}
	*m0 = m;
	*rule = args.rule;

	if (i == IP_FW_DENY) /* drop */
		return 0;

	KASSERT(m != NULL, ("ether_ipfw_chk: m is NULL"));

	if (i == IP_FW_PASS) /* a PASS rule.  */
		return 1;

	if (DUMMYNET_LOADED && (i == IP_FW_DUMMYNET)) {
		/*
		 * Pass the pkt to dummynet, which consumes it.
		 * If shared, make a copy and keep the original.
		 */
		if (shared) {
			m = m_copypacket(m, M_DONTWAIT);
			if (m == NULL)
				return 0;
		} else {
			/*
			 * Pass the original to dummynet and
			 * nothing back to the caller
			 */
			*m0 = NULL ;
		}
		ip_dn_io_ptr(m, dst ? DN_TO_ETH_OUT: DN_TO_ETH_DEMUX, &args);
		return 0;
	}
	/*
	 * XXX at some point add support for divert/forward actions.
	 * If none of the above matches, we have to drop the pkt.
	 */
	return 0;
}
#endif

/*
 * Process a received Ethernet packet; the packet is in the
 * mbuf chain m with the ethernet header at the front.
 */
static void
ether_input(struct ifnet *ifp, struct mbuf *m)
{
	struct ether_header *eh;
	u_short etype;

	/*
	 * Do consistency checks to verify assumptions
	 * made by code past this point.
	 */
	if ((m->m_flags & M_PKTHDR) == 0) {
		if_printf(ifp, "discard frame w/o packet header\n");
		ifp->if_ierrors++;
		m_freem(m);
		return;
	}
	if (m->m_len < ETHER_HDR_LEN) {
		/* XXX maybe should pullup? */
		if_printf(ifp, "discard frame w/o leading ethernet "
				"header (len %u pkt len %u)\n",
				m->m_len, m->m_pkthdr.len);
		ifp->if_ierrors++;
		m_freem(m);
		return;
	}
	eh = mtod(m, struct ether_header *);
	etype = ntohs(eh->ether_type);
	if (m->m_pkthdr.rcvif == NULL) {
		if_printf(ifp, "discard frame w/o interface pointer\n");
		ifp->if_ierrors++;
		m_freem(m);
		return;
	}
#ifdef DIAGNOSTIC
	if (m->m_pkthdr.rcvif != ifp) {
		if_printf(ifp, "Warning, frame marked as received on %s\n",
			m->m_pkthdr.rcvif->if_xname);
	}
#endif

#ifdef MAC
	/*
	 * Tag the mbuf with an appropriate MAC label before any other
	 * consumers can get to it.
	 */
	mac_create_mbuf_from_ifnet(ifp, m);
#endif

	/*
	 * Give bpf a chance at the packet.
	 */
	ETHER_BPF_MTAP(ifp, m);

	/* If the CRC is still on the packet, trim it off. */
	if (m->m_flags & M_HASFCS) {
		m_adj(m, -ETHER_CRC_LEN);
		m->m_flags &= ~M_HASFCS;
	}

	ifp->if_ibytes += m->m_pkthdr.len;

	if (ifp->if_flags & IFF_MONITOR) {
		/*
		 * Interface marked for monitoring; discard packet.
		 */
		m_freem(m);
		return;
	}

	/* Handle input from a lagg(4) port */
	if (ifp->if_type == IFT_IEEE8023ADLAG) {
		KASSERT(lagg_input_p != NULL,
		    ("%s: if_lagg not loaded!", __func__));
		m = (*lagg_input_p)(ifp, m);
		if (m != NULL)
			ifp = m->m_pkthdr.rcvif;
		else
			return;
	}

	/* Handle ng_ether(4) processing, if any */
	if (IFP2AC(ifp)->ac_netgraph != NULL) {
		(*ng_ether_input_p)(ifp, &m);
		if (m == NULL)
			return;
	}

	/*
	 * Tap the packet off here for a bridge.  bridge_input()
	 * will return NULL if it has consumed the packet, otherwise
	 * it gets processed as normal.  Note that bridge_input()
	 * will always return the original packet if we need to
	 * process it locally.
	 */
	if (ifp->if_bridge) {
		BRIDGE_INPUT(ifp, m);
		if (m == NULL)
			return;
	}

	/* Check for bridging mode */
	if (BDG_ACTIVE(ifp) )
		if ((m = bridge_in_ptr(ifp, m)) == NULL)
			return;

	/* First chunk of an mbuf contains good entropy */
	if (harvest.ethernet)
		random_harvest(m, 16, 3, 0, RANDOM_NET);
	ether_demux(ifp, m);
}

/*
 * Upper layer processing for a received Ethernet packet.
 */
void
ether_demux(struct ifnet *ifp, struct mbuf *m)
{
	struct ether_header *eh;
	int isr;
	u_short ether_type;
#if defined(NETATALK)
	struct llc *l;
#endif
#if defined(INET) || defined(INET6)
	struct ip_fw *rule = ip_dn_claim_rule(m);
#endif

	KASSERT(ifp != NULL, ("ether_demux: NULL interface pointer"));

	eh = mtod(m, struct ether_header *);
	ether_type = ntohs(eh->ether_type);

#if defined(INET) || defined(INET6)
	if (rule)	/* packet was already bridged */
		goto post_stats;
#endif

	if (!(BDG_ACTIVE(ifp)) && !(ifp->if_bridge) &&
	    !((ether_type == ETHERTYPE_VLAN || m->m_flags & M_VLANTAG) &&
	    ifp->if_nvlans > 0)) {
#ifdef DEV_CARP
		/*
		 * XXX: Okay, we need to call carp_forus() and - if it is for
		 * us jump over code that does the normal check
		 * "IFP2ENADDR(ifp) == ether_dhost". The check sequence is a bit
		 * different from OpenBSD, so we jump over as few code as
		 * possible, to catch _all_ sanity checks. This needs
		 * evaluation, to see if the carp ether_dhost values break any
		 * of these checks!
		 */
		if (ifp->if_carp && carp_forus(ifp->if_carp, eh->ether_dhost))
			goto pre_stats;
#endif
		/*
		 * Discard packet if upper layers shouldn't see it because it
		 * was unicast to a different Ethernet address. If the driver
		 * is working properly, then this situation can only happen
		 * when the interface is in promiscuous mode.
		 *
		 * If VLANs are active, and this packet has a VLAN tag, do
		 * not drop it here but pass it on to the VLAN layer, to
		 * give them a chance to consider it as well (e. g. in case
		 * bridging is only active on a VLAN).  They will drop it if
		 * it's undesired.
		 */
		if ((ifp->if_flags & IFF_PROMISC) != 0
		    && !ETHER_IS_MULTICAST(eh->ether_dhost)
		    && bcmp(eh->ether_dhost,
		      IFP2ENADDR(ifp), ETHER_ADDR_LEN) != 0
		    && (ifp->if_flags & IFF_PPROMISC) == 0) {
			    m_freem(m);
			    return;
		}
	}

#ifdef DEV_CARP
pre_stats:
#endif
	/* Discard packet if interface is not up */
	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}
	if (ETHER_IS_MULTICAST(eh->ether_dhost)) {
		if (bcmp(etherbroadcastaddr, eh->ether_dhost,
		    sizeof(etherbroadcastaddr)) == 0)
			m->m_flags |= M_BCAST;
		else
			m->m_flags |= M_MCAST;
	}
	if (m->m_flags & (M_BCAST|M_MCAST))
		ifp->if_imcasts++;

#if defined(INET) || defined(INET6)
post_stats:
	if (IPFW_LOADED && ether_ipfw != 0) {
		if (ether_ipfw_chk(&m, NULL, &rule, 0) == 0) {
			if (m)
				m_freem(m);
			return;
		}
	}
#endif

	/*
	 * Check to see if the device performed the VLAN decapsulation and
	 * provided us with the tag.
	 */
	if (m->m_flags & M_VLANTAG) {
		/*
		 * If no VLANs are configured, drop.
		 */
		if (ifp->if_nvlans == 0) {
			ifp->if_noproto++;
			m_freem(m);
			return;
		}
		/*
		 * vlan_input() will either recursively call ether_input()
		 * or drop the packet.
		 */
		KASSERT(vlan_input_p != NULL,("ether_input: VLAN not loaded!"));
		(*vlan_input_p)(ifp, m);
		return;
	}

	/*
	 * Handle protocols that expect to have the Ethernet header
	 * (and possibly FCS) intact.
	 */
	switch (ether_type) {
	case ETHERTYPE_VLAN:
		if (ifp->if_nvlans != 0) {
			KASSERT(vlan_input_p,("ether_input: VLAN not loaded!"));
			(*vlan_input_p)(ifp, m);
		} else {
			ifp->if_noproto++;
			m_freem(m);
		}
		return;
	}

	/* Strip off Ethernet header. */
	m_adj(m, ETHER_HDR_LEN);

	/* If the CRC is still on the packet, trim it off. */
	if (m->m_flags & M_HASFCS) {
		m_adj(m, -ETHER_CRC_LEN);
		m->m_flags &= ~M_HASFCS;
	}

	switch (ether_type) {
#ifdef INET
	case ETHERTYPE_IP:
		if (ip_fastforward(m))
			return;
		isr = NETISR_IP;
		break;

	case ETHERTYPE_ARP:
		if (ifp->if_flags & IFF_NOARP) {
			/* Discard packet if ARP is disabled on interface */
			m_freem(m);
			return;
		}
		isr = NETISR_ARP;
		break;
#endif
#ifdef IPX
	case ETHERTYPE_IPX:
		if (ef_inputp && ef_inputp(ifp, eh, m) == 0)
			return;
		isr = NETISR_IPX;
		break;
#endif
#ifdef INET6
	case ETHERTYPE_IPV6:
		isr = NETISR_IPV6;
		break;
#endif
#ifdef NETATALK
	case ETHERTYPE_AT:
		isr = NETISR_ATALK1;
		break;
	case ETHERTYPE_AARP:
		isr = NETISR_AARP;
		break;
#endif /* NETATALK */
	default:
#ifdef IPX
		if (ef_inputp && ef_inputp(ifp, eh, m) == 0)
			return;
#endif /* IPX */
#if defined(NETATALK)
		if (ether_type > ETHERMTU)
			goto discard;
		l = mtod(m, struct llc *);
		if (l->llc_dsap == LLC_SNAP_LSAP &&
		    l->llc_ssap == LLC_SNAP_LSAP &&
		    l->llc_control == LLC_UI) {
			if (bcmp(&(l->llc_snap_org_code)[0], at_org_code,
			    sizeof(at_org_code)) == 0 &&
			    ntohs(l->llc_snap_ether_type) == ETHERTYPE_AT) {
				m_adj(m, LLC_SNAPFRAMELEN);
				isr = NETISR_ATALK2;
				break;
			}
			if (bcmp(&(l->llc_snap_org_code)[0], aarp_org_code,
			    sizeof(aarp_org_code)) == 0 &&
			    ntohs(l->llc_snap_ether_type) == ETHERTYPE_AARP) {
				m_adj(m, LLC_SNAPFRAMELEN);
				isr = NETISR_AARP;
				break;
			}
		}
#endif /* NETATALK */
		goto discard;
	}
	netisr_dispatch(isr, m);
	return;

discard:
	/*
	 * Packet is to be discarded.  If netgraph is present,
	 * hand the packet to it for last chance processing;
	 * otherwise dispose of it.
	 */
	if (IFP2AC(ifp)->ac_netgraph != NULL) {
		/*
		 * Put back the ethernet header so netgraph has a
		 * consistent view of inbound packets.
		 */
		M_PREPEND(m, ETHER_HDR_LEN, M_DONTWAIT);
		(*ng_ether_input_orphan_p)(ifp, m);
		return;
	}
	m_freem(m);
}

/*
 * Convert Ethernet address to printable (loggable) representation.
 * This routine is for compatibility; it's better to just use
 *
 *	printf("%6D", <pointer to address>, ":");
 *
 * since there's no static buffer involved.
 */
char *
ether_sprintf(const u_char *ap)
{
	static char etherbuf[18];
	snprintf(etherbuf, sizeof (etherbuf), "%6D", ap, ":");
	return (etherbuf);
}

/*
 * Perform common duties while attaching to interface list
 */
void
ether_ifattach(struct ifnet *ifp, const u_int8_t *llc)
{
	int i;
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;

	ifp->if_addrlen = ETHER_ADDR_LEN;
	ifp->if_hdrlen = ETHER_HDR_LEN;
	if_attach(ifp);
	ifp->if_mtu = ETHERMTU;
	ifp->if_output = ether_output;
	ifp->if_input = ether_input;
	ifp->if_resolvemulti = ether_resolvemulti;
	if (ifp->if_baudrate == 0)
		ifp->if_baudrate = IF_Mbps(10);		/* just a default */
	ifp->if_broadcastaddr = etherbroadcastaddr;

	ifa = ifaddr_byindex(ifp->if_index);
	KASSERT(ifa != NULL, ("%s: no lladdr!\n", __func__));
	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	sdl->sdl_type = IFT_ETHER;
	sdl->sdl_alen = ifp->if_addrlen;
	bcopy(llc, LLADDR(sdl), ifp->if_addrlen);
	/*
	 * XXX: This doesn't belong here; we do it until
	 * XXX:  all drivers are cleaned up
	 */
	if (llc != IFP2ENADDR(ifp))
		bcopy(llc, IFP2ENADDR(ifp), ifp->if_addrlen);

	bpfattach(ifp, DLT_EN10MB, ETHER_HDR_LEN);
	if (ng_ether_attach_p != NULL)
		(*ng_ether_attach_p)(ifp);
	if (BDG_LOADED)
		bdgtakeifaces_ptr();

	/* Announce Ethernet MAC address if non-zero. */
	for (i = 0; i < ifp->if_addrlen; i++)
		if (llc[i] != 0)
			break; 
	if (i != ifp->if_addrlen)
		if_printf(ifp, "Ethernet address: %6D\n", llc, ":");
	if (debug_mpsafenet && (ifp->if_flags & IFF_NEEDSGIANT) != 0)
		if_printf(ifp, "if_start running deferred for Giant\n");
}

/*
 * Perform common duties while detaching an Ethernet interface
 */
void
ether_ifdetach(struct ifnet *ifp)
{
	if (IFP2AC(ifp)->ac_netgraph != NULL)
		(*ng_ether_detach_p)(ifp);

	bpfdetach(ifp);
	if_detach(ifp);
	if (BDG_LOADED)
		bdgtakeifaces_ptr();
}

SYSCTL_DECL(_net_link);
SYSCTL_NODE(_net_link, IFT_ETHER, ether, CTLFLAG_RW, 0, "Ethernet");
#if defined(INET) || defined(INET6)
SYSCTL_INT(_net_link_ether, OID_AUTO, ipfw, CTLFLAG_RW,
	    &ether_ipfw,0,"Pass ether pkts through firewall");
#endif

#if 0
/*
 * This is for reference.  We have a table-driven version
 * of the little-endian crc32 generator, which is faster
 * than the double-loop.
 */
uint32_t
ether_crc32_le(const uint8_t *buf, size_t len)
{
	size_t i;
	uint32_t crc;
	int bit;
	uint8_t data;

	crc = 0xffffffff;	/* initial value */

	for (i = 0; i < len; i++) {
		for (data = *buf++, bit = 0; bit < 8; bit++, data >>= 1)
			carry = (crc ^ data) & 1;
			crc >>= 1;
			if (carry)
				crc = (crc ^ ETHER_CRC_POLY_LE);
	}

	return (crc);
}
#else
uint32_t
ether_crc32_le(const uint8_t *buf, size_t len)
{
	static const uint32_t crctab[] = {
		0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
	};
	size_t i;
	uint32_t crc;

	crc = 0xffffffff;	/* initial value */

	for (i = 0; i < len; i++) {
		crc ^= buf[i];
		crc = (crc >> 4) ^ crctab[crc & 0xf];
		crc = (crc >> 4) ^ crctab[crc & 0xf];
	}

	return (crc);
}
#endif

uint32_t
ether_crc32_be(const uint8_t *buf, size_t len)
{
	size_t i;
	uint32_t crc, carry;
	int bit;
	uint8_t data;

	crc = 0xffffffff;	/* initial value */

	for (i = 0; i < len; i++) {
		for (data = *buf++, bit = 0; bit < 8; bit++, data >>= 1) {
			carry = ((crc & 0x80000000) ? 1 : 0) ^ (data & 0x01);
			crc <<= 1;
			if (carry)
				crc = (crc ^ ETHER_CRC_POLY_BE) | carry;
		}
	}

	return (crc);
}

int
ether_ioctl(struct ifnet *ifp, int command, caddr_t data)
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
		 * XXX - This code is probably wrong
		 */
		case AF_IPX:
			{
			struct ipx_addr *ina = &(IA_SIPX(ifa)->sipx_addr);

			if (ipx_nullhost(*ina))
				ina->x_host =
				    *(union ipx_host *)
				    IFP2ENADDR(ifp);
			else {
				bcopy((caddr_t) ina->x_host.c_host,
				      (caddr_t) IFP2ENADDR(ifp),
				      ETHER_ADDR_LEN);
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
			bcopy(IFP2ENADDR(ifp),
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
	default:
		error = EINVAL;			/* XXX netbsd has ENOTTY??? */
		break;
	}
	return (error);
}

static int
ether_resolvemulti(struct ifnet *ifp, struct sockaddr **llsa,
	struct sockaddr *sa)
{
	struct sockaddr_dl *sdl;
#ifdef INET
	struct sockaddr_in *sin;
#endif
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
		if (!ETHER_IS_MULTICAST(e_addr))
			return EADDRNOTAVAIL;
		*llsa = 0;
		return 0;

#ifdef INET
	case AF_INET:
		sin = (struct sockaddr_in *)sa;
		if (!IN_MULTICAST(ntohl(sin->sin_addr.s_addr)))
			return EADDRNOTAVAIL;
		MALLOC(sdl, struct sockaddr_dl *, sizeof *sdl, M_IFMADDR,
		       M_NOWAIT|M_ZERO);
		if (sdl == NULL)
			return ENOMEM;
		sdl->sdl_len = sizeof *sdl;
		sdl->sdl_family = AF_LINK;
		sdl->sdl_index = ifp->if_index;
		sdl->sdl_type = IFT_ETHER;
		sdl->sdl_alen = ETHER_ADDR_LEN;
		e_addr = LLADDR(sdl);
		ETHER_MAP_IP_MULTICAST(&sin->sin_addr, e_addr);
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
		MALLOC(sdl, struct sockaddr_dl *, sizeof *sdl, M_IFMADDR,
		       M_NOWAIT|M_ZERO);
		if (sdl == NULL)
			return (ENOMEM);
		sdl->sdl_len = sizeof *sdl;
		sdl->sdl_family = AF_LINK;
		sdl->sdl_index = ifp->if_index;
		sdl->sdl_type = IFT_ETHER;
		sdl->sdl_alen = ETHER_ADDR_LEN;
		e_addr = LLADDR(sdl);
		ETHER_MAP_IPV6_MULTICAST(&sin6->sin6_addr, e_addr);
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

static void*
ether_alloc(u_char type, struct ifnet *ifp)
{
	struct arpcom	*ac;
	
	ac = malloc(sizeof(struct arpcom), M_ARPCOM, M_WAITOK | M_ZERO);
	ac->ac_ifp = ifp;

	return (ac);
}

static void
ether_free(void *com, u_char type)
{

	free(com, M_ARPCOM);
}

static int
ether_modevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		if_register_com_alloc(IFT_ETHER, ether_alloc, ether_free);
		break;
	case MOD_UNLOAD:
		if_deregister_com_alloc(IFT_ETHER);
		break;
	default:
		return EOPNOTSUPP;
	}

	return (0);
}

static moduledata_t ether_mod = {
	"ether",
	ether_modevent,
	0
};

void
ether_vlan_mtap(struct bpf_if *bp, struct mbuf *m, void *data, u_int dlen)
{
	struct ether_vlan_header vlan;
	struct mbuf mv, mb;
	struct m_tag *mtag;
	u_int tag;

	KASSERT((m->m_flags & M_VLANTAG) != 0,
	    ("%s: vlan information not present", __func__));
	KASSERT(m->m_len >= sizeof(struct ether_header),
	    ("%s: mbuf not large enough for header", __func__));
	mtag = m_tag_locate(m, MTAG_VLAN, MTAG_VLAN_TAG, NULL);
	KASSERT(mtag != NULL,
	    ("%s: NULL mtag with M_VLANTAG", __func__));
	tag = EVL_VLANOFTAG(VLAN_TAG_VALUE(mtag));
	bcopy(mtod(m, char *), &vlan, sizeof(struct ether_header));
	vlan.evl_proto = vlan.evl_encap_proto;
	vlan.evl_encap_proto = htons(ETHERTYPE_VLAN);
	vlan.evl_tag = htons(tag);
	m->m_len -= sizeof(struct ether_header);
	m->m_data += sizeof(struct ether_header);
	/*
	 * If a data link has been supplied by the caller, then we will need to
	 * re-create a stack allocated mbuf chain with the following structure:
	 *
	 * (1) mbuf #1 will contain the supplied data link
	 * (2) mbuf #2 will contain the vlan header
	 * (3) mbuf #3 will contain the original mbuf's packet data
	 *
	 * Otherwise, submit the packet and vlan header via bpf_mtap2().
	 */
	if (data != NULL) {
		mv.m_next = m;
		mv.m_data = (caddr_t)&vlan;
		mv.m_len = sizeof(vlan);
		mb.m_next = &mv;
		mb.m_data = data;
		mb.m_len = dlen;
		bpf_mtap(bp, &mb);
	} else
		bpf_mtap2(bp, &vlan, sizeof(vlan), m);
	m->m_len += sizeof(struct ether_header);
	m->m_data -= sizeof(struct ether_header);
}

DECLARE_MODULE(ether, ether_mod, SI_SUB_INIT_IF, SI_ORDER_ANY);
MODULE_VERSION(ether, 1);
