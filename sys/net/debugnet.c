/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Isilon Systems, LLC.
 * Copyright (c) 2005-2014 Sandvine Incorporated. All rights reserved.
 * Copyright (c) 2000 Darrell Anderson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_options.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <machine/in_cksum.h>
#include <machine/pcb.h>

#include <net/debugnet.h>
#define	DEBUGNET_INTERNAL
#include <net/debugnet_int.h>

FEATURE(debugnet, "Debugnet support");

SYSCTL_NODE(_net, OID_AUTO, debugnet, CTLFLAG_RD, NULL,
    "debugnet parameters");

unsigned debugnet_debug;
SYSCTL_UINT(_net_debugnet, OID_AUTO, debug, CTLFLAG_RWTUN,
    &debugnet_debug, 0,
    "Debug message verbosity (0: off; 1: on; 2: verbose)");

int debugnet_npolls = 2000;
SYSCTL_INT(_net_debugnet, OID_AUTO, npolls, CTLFLAG_RWTUN,
    &debugnet_npolls, 0,
    "Number of times to poll before assuming packet loss (0.5ms per poll)");
int debugnet_nretries = 10;
SYSCTL_INT(_net_debugnet, OID_AUTO, nretries, CTLFLAG_RWTUN,
    &debugnet_nretries, 0,
    "Number of retransmit attempts before giving up");

static bool g_debugnet_pcb_inuse;
static struct debugnet_pcb g_dnet_pcb;

/*
 * Simple accessors for opaque PCB.
 */
const unsigned char *
debugnet_get_gw_mac(const struct debugnet_pcb *pcb)
{
	MPASS(g_debugnet_pcb_inuse && pcb == &g_dnet_pcb &&
	    pcb->dp_state >= DN_STATE_HAVE_GW_MAC);
	return (pcb->dp_gw_mac.octet);
}

/*
 * Start of network primitives, beginning with output primitives.
 */

/*
 * Handles creation of the ethernet header, then places outgoing packets into
 * the tx buffer for the NIC
 *
 * Parameters:
 *	m	The mbuf containing the packet to be sent (will be freed by
 *		this function or the NIC driver)
 *	ifp	The interface to send on
 *	dst	The destination ethernet address (source address will be looked
 *		up using ifp)
 *	etype	The ETHERTYPE_* value for the protocol that is being sent
 *
 * Returns:
 *	int	see errno.h, 0 for success
 */
int
debugnet_ether_output(struct mbuf *m, struct ifnet *ifp, struct ether_addr dst,
    u_short etype)
{
	struct ether_header *eh;

	if (((ifp->if_flags & (IFF_MONITOR | IFF_UP)) != IFF_UP) ||
	    (ifp->if_drv_flags & IFF_DRV_RUNNING) != IFF_DRV_RUNNING) {
		if_printf(ifp, "%s: interface isn't up\n", __func__);
		m_freem(m);
		return (ENETDOWN);
	}

	/* Fill in the ethernet header. */
	M_PREPEND(m, ETHER_HDR_LEN, M_NOWAIT);
	if (m == NULL) {
		printf("%s: out of mbufs\n", __func__);
		return (ENOBUFS);
	}
	eh = mtod(m, struct ether_header *);
	memcpy(eh->ether_shost, IF_LLADDR(ifp), ETHER_ADDR_LEN);
	memcpy(eh->ether_dhost, dst.octet, ETHER_ADDR_LEN);
	eh->ether_type = htons(etype);
	return (ifp->if_debugnet_methods->dn_transmit(ifp, m));
}

/*
 * Unreliable transmission of an mbuf chain to the debugnet server
 * Note: can't handle fragmentation; fails if the packet is larger than
 *	 ifp->if_mtu after adding the UDP/IP headers
 *
 * Parameters:
 *	pcb	The debugnet context block
 *	m	mbuf chain
 *
 * Returns:
 *	int	see errno.h, 0 for success
 */
static int
debugnet_udp_output(struct debugnet_pcb *pcb, struct mbuf *m)
{
	struct udphdr *udp;

	MPASS(pcb->dp_state >= DN_STATE_HAVE_GW_MAC);

	M_PREPEND(m, sizeof(*udp), M_NOWAIT);
	if (m == NULL) {
		printf("%s: out of mbufs\n", __func__);
		return (ENOBUFS);
	}

	udp = mtod(m, void *);
	udp->uh_ulen = htons(m->m_pkthdr.len);
	/* Use this src port so that the server can connect() the socket */
	udp->uh_sport = htons(pcb->dp_client_ack_port);
	udp->uh_dport = htons(pcb->dp_server_port);
	/* Computed later (protocol-dependent). */
	udp->uh_sum = 0;

	return (debugnet_ip_output(pcb, m));
}

/*
 * Dummy free function for debugnet clusters.
 */
static void
debugnet_mbuf_free(struct mbuf *m __unused)
{
}

/*
 * Construct and reliably send a debugnet packet.  May fail from a resource
 * shortage or extreme number of unacknowledged retransmissions.  Wait for
 * an acknowledgement before returning.  Splits packets into chunks small
 * enough to be sent without fragmentation (looks up the interface MTU)
 *
 * Parameters:
 *	type	debugnet packet type (HERALD, FINISHED, ...)
 *	data	data
 *	datalen	data size (bytes)
 *	auxdata	optional auxiliary information
 *
 * Returns:
 *	int see errno.h, 0 for success
 */
int
debugnet_send(struct debugnet_pcb *pcb, uint32_t type, const void *data,
    uint32_t datalen, const struct debugnet_proto_aux *auxdata)
{
	struct debugnet_msg_hdr *dn_msg_hdr;
	struct mbuf *m, *m2;
	uint64_t want_acks;
	uint32_t i, pktlen, sent_so_far;
	int retries, polls, error;

	want_acks = 0;
	pcb->dp_rcvd_acks = 0;
	retries = 0;

retransmit:
	/* Chunks can be too big to fit in packets. */
	for (i = sent_so_far = 0; sent_so_far < datalen ||
	    (i == 0 && datalen == 0); i++) {
		pktlen = datalen - sent_so_far;

		/* Bound: the interface MTU (assume no IP options). */
		pktlen = min(pktlen, pcb->dp_ifp->if_mtu -
		    sizeof(struct udpiphdr) - sizeof(struct debugnet_msg_hdr));

		/*
		 * Check if it is retransmitting and this has been ACKed
		 * already.
		 */
		if ((pcb->dp_rcvd_acks & (1 << i)) != 0) {
			sent_so_far += pktlen;
			continue;
		}

		/*
		 * Get and fill a header mbuf, then chain data as an extended
		 * mbuf.
		 */
		m = m_gethdr(M_NOWAIT, MT_DATA);
		if (m == NULL) {
			printf("%s: Out of mbufs\n", __func__);
			return (ENOBUFS);
		}
		m->m_len = sizeof(struct debugnet_msg_hdr);
		m->m_pkthdr.len = sizeof(struct debugnet_msg_hdr);
		MH_ALIGN(m, sizeof(struct debugnet_msg_hdr));
		dn_msg_hdr = mtod(m, struct debugnet_msg_hdr *);
		dn_msg_hdr->mh_seqno = htonl(pcb->dp_seqno + i);
		dn_msg_hdr->mh_type = htonl(type);
		dn_msg_hdr->mh_len = htonl(pktlen);

		if (auxdata != NULL) {
			dn_msg_hdr->mh_offset =
			    htobe64(auxdata->dp_offset_start + sent_so_far);
			dn_msg_hdr->mh_aux2 = htobe32(auxdata->dp_aux2);
		} else {
			dn_msg_hdr->mh_offset = htobe64(sent_so_far);
			dn_msg_hdr->mh_aux2 = 0;
		}

		if (pktlen != 0) {
			m2 = m_get(M_NOWAIT, MT_DATA);
			if (m2 == NULL) {
				m_freem(m);
				printf("%s: Out of mbufs\n", __func__);
				return (ENOBUFS);
			}
			MEXTADD(m2, __DECONST(char *, data) + sent_so_far,
			    pktlen, debugnet_mbuf_free, NULL, NULL, 0,
			    EXT_DISPOSABLE);
			m2->m_len = pktlen;

			m_cat(m, m2);
			m->m_pkthdr.len += pktlen;
		}
		error = debugnet_udp_output(pcb, m);
		if (error != 0)
			return (error);

		/* Note that we're waiting for this packet in the bitfield. */
		want_acks |= (1 << i);
		sent_so_far += pktlen;
	}
	if (i >= DEBUGNET_MAX_IN_FLIGHT)
		printf("Warning: Sent more than %d packets (%d). "
		    "Acknowledgements will fail unless the size of "
		    "rcvd_acks/want_acks is increased.\n",
		    DEBUGNET_MAX_IN_FLIGHT, i);

	/*
	 * Wait for acks.  A *real* window would speed things up considerably.
	 */
	polls = 0;
	while (pcb->dp_rcvd_acks != want_acks) {
		if (polls++ > debugnet_npolls) {
			if (retries++ > debugnet_nretries)
				return (ETIMEDOUT);
			printf(". ");
			goto retransmit;
		}
		debugnet_network_poll(pcb->dp_ifp);
		DELAY(500);
	}
	pcb->dp_seqno += i;
	return (0);
}

/*
 * Network input primitives.
 */

static void
debugnet_handle_ack(struct debugnet_pcb *pcb, struct mbuf **mb, uint16_t sport)
{
	const struct debugnet_ack *dn_ack;
	struct mbuf *m;
	uint32_t rcv_ackno;

	m = *mb;

	if (m->m_pkthdr.len < sizeof(*dn_ack)) {
		DNETDEBUG("ignoring small ACK packet\n");
		return;
	}
	/* Get Ack. */
	if (m->m_len < sizeof(*dn_ack)) {
		m = m_pullup(m, sizeof(*dn_ack));
		*mb = m;
		if (m == NULL) {
			DNETDEBUG("m_pullup failed\n");
			return;
		}
	}
	dn_ack = mtod(m, const void *);

	/* Debugnet processing. */
	/*
	 * Packet is meant for us.  Extract the ack sequence number and the
	 * port number if necessary.
	 */
	rcv_ackno = ntohl(dn_ack->da_seqno);
	if (pcb->dp_state < DN_STATE_GOT_HERALD_PORT) {
		pcb->dp_server_port = sport;
		pcb->dp_state = DN_STATE_GOT_HERALD_PORT;
	}
	if (rcv_ackno >= pcb->dp_seqno + DEBUGNET_MAX_IN_FLIGHT)
		printf("%s: ACK %u too far in future!\n", __func__, rcv_ackno);
	else if (rcv_ackno >= pcb->dp_seqno) {
		/* We're interested in this ack. Record it. */
		pcb->dp_rcvd_acks |= 1 << (rcv_ackno - pcb->dp_seqno);
	}
}

void
debugnet_handle_udp(struct debugnet_pcb *pcb, struct mbuf **mb)
{
	const struct udphdr *udp;
	struct mbuf *m;
	uint16_t sport;

	/* UDP processing. */

	m = *mb;
	if (m->m_pkthdr.len < sizeof(*udp)) {
		DNETDEBUG("ignoring small UDP packet\n");
		return;
	}

	/* Get UDP headers. */
	if (m->m_len < sizeof(*udp)) {
		m = m_pullup(m, sizeof(*udp));
		*mb = m;
		if (m == NULL) {
			DNETDEBUG("m_pullup failed\n");
			return;
		}
	}
	udp = mtod(m, const void *);

	/* For now, the only UDP packets we expect to receive are acks. */
	if (ntohs(udp->uh_dport) != pcb->dp_client_ack_port) {
		DNETDEBUG("not on the expected ACK port.\n");
		return;
	}
	sport = ntohs(udp->uh_sport);

	m_adj(m, sizeof(*udp));
	debugnet_handle_ack(pcb, mb, sport);
}

/*
 * Handler for incoming packets directly from the network adapter
 * Identifies the packet type (IP or ARP) and passes it along to one of the
 * helper functions debugnet_handle_ip or debugnet_handle_arp.
 *
 * It needs to partially replicate the behaviour of ether_input() and
 * ether_demux().
 *
 * Parameters:
 *	ifp	the interface the packet came from
 *	m	an mbuf containing the packet received
 */
static void
debugnet_pkt_in(struct ifnet *ifp, struct mbuf *m)
{
	struct ifreq ifr;
	struct ether_header *eh;
	u_short etype;

	/* Ethernet processing. */
	if ((m->m_flags & M_PKTHDR) == 0) {
		DNETDEBUG_IF(ifp, "discard frame without packet header\n");
		goto done;
	}
	if (m->m_len < ETHER_HDR_LEN) {
		DNETDEBUG_IF(ifp,
	    "discard frame without leading eth header (len %u pktlen %u)\n",
		    m->m_len, m->m_pkthdr.len);
		goto done;
	}
	if ((m->m_flags & M_HASFCS) != 0) {
		m_adj(m, -ETHER_CRC_LEN);
		m->m_flags &= ~M_HASFCS;
	}
	eh = mtod(m, struct ether_header *);
	etype = ntohs(eh->ether_type);
	if ((m->m_flags & M_VLANTAG) != 0 || etype == ETHERTYPE_VLAN) {
		DNETDEBUG_IF(ifp, "ignoring vlan packets\n");
		goto done;
	}
	if (if_gethwaddr(ifp, &ifr) != 0) {
		DNETDEBUG_IF(ifp, "failed to get hw addr for interface\n");
		goto done;
	}
	if (memcmp(ifr.ifr_addr.sa_data, eh->ether_dhost,
	    ETHER_ADDR_LEN) != 0 &&
	    (etype != ETHERTYPE_ARP || !ETHER_IS_BROADCAST(eh->ether_dhost))) {
		DNETDEBUG_IF(ifp,
		    "discard frame with incorrect destination addr\n");
		goto done;
	}

	MPASS(g_debugnet_pcb_inuse);

	/* Done ethernet processing. Strip off the ethernet header. */
	m_adj(m, ETHER_HDR_LEN);
	switch (etype) {
	case ETHERTYPE_ARP:
		debugnet_handle_arp(&g_dnet_pcb, &m);
		break;
	case ETHERTYPE_IP:
		debugnet_handle_ip(&g_dnet_pcb, &m);
		break;
	default:
		DNETDEBUG_IF(ifp, "dropping unknown ethertype %hu\n", etype);
		break;
	}
done:
	if (m != NULL)
		m_freem(m);
}

/*
 * Network polling primitive.
 *
 * Instead of assuming that most of the network stack is sane, we just poll the
 * driver directly for packets.
 */
void
debugnet_network_poll(struct ifnet *ifp)
{
	ifp->if_debugnet_methods->dn_poll(ifp, 1000);
}

/*
 * Start of consumer API surface.
 */
void
debugnet_free(struct debugnet_pcb *pcb)
{
	struct ifnet *ifp;

	MPASS(g_debugnet_pcb_inuse);
	MPASS(pcb == &g_dnet_pcb);

	ifp = pcb->dp_ifp;
	ifp->if_input = pcb->dp_drv_input;
	ifp->if_debugnet_methods->dn_event(ifp, DEBUGNET_END);
	debugnet_mbuf_finish();

	g_debugnet_pcb_inuse = false;
	memset(&g_dnet_pcb, 0xfd, sizeof(g_dnet_pcb));
}

int
debugnet_connect(const struct debugnet_conn_params *dcp,
    struct debugnet_pcb **pcb_out)
{
	struct debugnet_pcb *pcb;
	struct ifnet *ifp;
	int error;

	if (g_debugnet_pcb_inuse) {
		printf("%s: Only one connection at a time.\n", __func__);
		return (EBUSY);
	}

	pcb = &g_dnet_pcb;
	*pcb = (struct debugnet_pcb) {
		.dp_state = DN_STATE_INIT,
		.dp_client = dcp->dc_client,
		.dp_server = dcp->dc_server,
		.dp_gateway = dcp->dc_gateway,
		.dp_server_port = dcp->dc_herald_port,	/* Initially */
		.dp_client_ack_port = dcp->dc_client_ack_port,
		.dp_seqno = 1,
		.dp_ifp = dcp->dc_ifp,
	};

	/* Switch to the debugnet mbuf zones. */
	debugnet_mbuf_start();

	ifp = pcb->dp_ifp;
	ifp->if_debugnet_methods->dn_event(ifp, DEBUGNET_START);

	/*
	 * We maintain the invariant that g_debugnet_pcb_inuse is always true
	 * while the debugnet ifp's if_input is overridden with
	 * debugnet_pkt_in.
	 */
	g_debugnet_pcb_inuse = true;

	/* Make the card use *our* receive callback. */
	pcb->dp_drv_input = ifp->if_input;
	ifp->if_input = debugnet_pkt_in;

	printf("%s: searching for %s MAC...\n", __func__,
	    (dcp->dc_gateway == INADDR_ANY) ? "server" : "gateway");

	error = debugnet_arp_gw(pcb);
	if (error != 0) {
		printf("%s: failed to locate MAC address\n", __func__);
		goto cleanup;
	}
	MPASS(pcb->dp_state == DN_STATE_HAVE_GW_MAC);

	error = debugnet_send(pcb, DEBUGNET_HERALD, dcp->dc_herald_data,
	    dcp->dc_herald_datalen, NULL);
	if (error != 0) {
		printf("%s: failed to herald debugnet server\n", __func__);
		goto cleanup;
	}

	*pcb_out = pcb;
	return (0);

cleanup:
	debugnet_free(pcb);
	return (error);
}

/*
 * Pre-allocated dump-time mbuf tracking.
 *
 * We just track the high water mark we've ever seen and allocate appropriately
 * for that iface/mtu combo.
 */
static struct {
	int nmbuf;
	int ncl;
	int clsize;
} dn_hwm;
static struct mtx dn_hwm_lk;
MTX_SYSINIT(debugnet_hwm_lock, &dn_hwm_lk, "Debugnet HWM lock", MTX_DEF);

static void
dn_maybe_reinit_mbufs(int nmbuf, int ncl, int clsize)
{
	bool any;

	any = false;
	mtx_lock(&dn_hwm_lk);

	if (nmbuf > dn_hwm.nmbuf) {
		any = true;
		dn_hwm.nmbuf = nmbuf;
	} else
		nmbuf = dn_hwm.nmbuf;

	if (ncl > dn_hwm.ncl) {
		any = true;
		dn_hwm.ncl = ncl;
	} else
		ncl = dn_hwm.ncl;

	if (clsize > dn_hwm.clsize) {
		any = true;
		dn_hwm.clsize = clsize;
	} else
		clsize = dn_hwm.clsize;

	mtx_unlock(&dn_hwm_lk);

	if (any)
		debugnet_mbuf_reinit(nmbuf, ncl, clsize);
}

void
debugnet_any_ifnet_update(struct ifnet *ifp)
{
	int clsize, nmbuf, ncl, nrxr;

	if (!DEBUGNET_SUPPORTED_NIC(ifp))
		return;

	ifp->if_debugnet_methods->dn_init(ifp, &nrxr, &ncl, &clsize);
	KASSERT(nrxr > 0, ("invalid receive ring count %d", nrxr));

	/*
	 * We need two headers per message on the transmit side. Multiply by
	 * four to give us some breathing room.
	 */
	nmbuf = ncl * (4 + nrxr);
	ncl *= nrxr;

	dn_maybe_reinit_mbufs(nmbuf, ncl, clsize);
}

/*
 * Unfortunately, the ifnet_arrival_event eventhandler hook is mostly useless
 * for us because drivers tend to if_attach before invoking DEBUGNET_SET().
 *
 * On the other hand, hooking DEBUGNET_SET() itself may still be too early,
 * because the driver is still in attach.  Since we cannot use down interfaces,
 * maybe hooking ifnet_event:IFNET_EVENT_UP is sufficient?  ... Nope, at least
 * with vtnet and dhcpclient that event just never occurs.
 *
 * So that's how I've landed on the lower level ifnet_link_event.
 */

static void
dn_ifnet_event(void *arg __unused, struct ifnet *ifp, int link_state)
{
	if (link_state == LINK_STATE_UP)
		debugnet_any_ifnet_update(ifp);
}

static eventhandler_tag dn_attach_cookie;
static void
dn_evh_init(void *ctx __unused)
{
	dn_attach_cookie = EVENTHANDLER_REGISTER(ifnet_link_event,
	    dn_ifnet_event, NULL, EVENTHANDLER_PRI_ANY);
}
SYSINIT(dn_evh_init, SI_SUB_EVENTHANDLER + 1, SI_ORDER_ANY, dn_evh_init, NULL);
