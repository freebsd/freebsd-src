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

#include "opt_ddb.h"
#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/eventhandler.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#ifdef DDB
#include <ddb/ddb.h>
#include <ddb/db_lex.h>
#endif

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/route.h>
#include <net/route/nhop.h>

#include <netinet/in.h>
#include <netinet/in_fib.h>
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

SYSCTL_NODE(_net, OID_AUTO, debugnet, CTLFLAG_RD | CTLFLAG_MPSAFE, NULL,
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
int debugnet_fib = RT_DEFAULT_FIB;
SYSCTL_INT(_net_debugnet, OID_AUTO, fib, CTLFLAG_RWTUN,
    &debugnet_fib, 0,
    "Fib to use when sending dump");

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
	udp->uh_sport = htons(pcb->dp_client_port);
	udp->uh_dport = htons(pcb->dp_server_port);
	/* Computed later (protocol-dependent). */
	udp->uh_sum = 0;

	return (debugnet_ip_output(pcb, m));
}

int
debugnet_ack_output(struct debugnet_pcb *pcb, uint32_t seqno /* net endian */)
{
	struct debugnet_ack *dn_ack;
	struct mbuf *m;

	DNETDEBUG("Acking with seqno %u\n", ntohl(seqno));

	m = m_gethdr(M_NOWAIT, MT_DATA);
	if (m == NULL) {
		printf("%s: Out of mbufs\n", __func__);
		return (ENOBUFS);
	}
	m->m_len = sizeof(*dn_ack);
	m->m_pkthdr.len = sizeof(*dn_ack);
	MH_ALIGN(m, sizeof(*dn_ack));
	dn_ack = mtod(m, void *);
	dn_ack->da_seqno = seqno;

	return (debugnet_udp_output(pcb, m));
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

	if (pcb->dp_state == DN_STATE_REMOTE_CLOSED)
		return (ECONNRESET);

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
		debugnet_network_poll(pcb);
		DELAY(500);
		if (pcb->dp_state == DN_STATE_REMOTE_CLOSED)
			return (ECONNRESET);
	}
	pcb->dp_seqno += i;
	return (0);
}

/*
 * Network input primitives.
 */

/*
 * Just introspect the header enough to fire off a seqno ack and validate
 * length fits.
 */
static void
debugnet_handle_rx_msg(struct debugnet_pcb *pcb, struct mbuf **mb)
{
	const struct debugnet_msg_hdr *dnh;
	struct mbuf *m;
	int error;

	m = *mb;

	if (m->m_pkthdr.len < sizeof(*dnh)) {
		DNETDEBUG("ignoring small debugnet_msg packet\n");
		return;
	}

	/* Get ND header. */
	if (m->m_len < sizeof(*dnh)) {
		m = m_pullup(m, sizeof(*dnh));
		*mb = m;
		if (m == NULL) {
			DNETDEBUG("m_pullup failed\n");
			return;
		}
	}
	dnh = mtod(m, const void *);

	if (ntohl(dnh->mh_len) + sizeof(*dnh) > m->m_pkthdr.len) {
		DNETDEBUG("Dropping short packet.\n");
		return;
	}

	/*
	 * If the issue is transient (ENOBUFS), sender should resend.  If
	 * non-transient (like driver objecting to rx -> tx from the same
	 * thread), not much else we can do.
	 */
	error = debugnet_ack_output(pcb, dnh->mh_seqno);
	if (error != 0)
		return;

	if (ntohl(dnh->mh_type) == DEBUGNET_FINISHED) {
		printf("Remote shut down the connection on us!\n");
		pcb->dp_state = DN_STATE_REMOTE_CLOSED;

		/*
		 * Continue through to the user handler so they are signalled
		 * not to wait for further rx.
		 */
	}

	pcb->dp_rx_handler(pcb, mb);
}

static void
debugnet_handle_ack(struct debugnet_pcb *pcb, struct mbuf **mb, uint16_t sport)
{
	const struct debugnet_ack *dn_ack;
	struct mbuf *m;
	uint32_t rcv_ackno;

	m = *mb;

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
	uint16_t sport, ulen;

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

	/* We expect to receive UDP packets on the configured client port. */
	if (ntohs(udp->uh_dport) != pcb->dp_client_port) {
		DNETDEBUG("not on the expected port.\n");
		return;
	}

	/* Check that ulen does not exceed actual size of data. */
	ulen = ntohs(udp->uh_ulen);
	if (m->m_pkthdr.len < ulen) {
		DNETDEBUG("ignoring runt UDP packet\n");
		return;
	}

	sport = ntohs(udp->uh_sport);

	m_adj(m, sizeof(*udp));
	ulen -= sizeof(*udp);

	if (ulen == sizeof(struct debugnet_ack)) {
		debugnet_handle_ack(pcb, mb, sport);
		return;
	}

	if (pcb->dp_rx_handler == NULL) {
		if (ulen < sizeof(struct debugnet_ack))
			DNETDEBUG("ignoring small ACK packet\n");
		else
			DNETDEBUG("ignoring unexpected non-ACK packet on "
			    "half-duplex connection.\n");
		return;
	}

	debugnet_handle_rx_msg(pcb, mb);
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
debugnet_network_poll(struct debugnet_pcb *pcb)
{
	struct ifnet *ifp;

	ifp = pcb->dp_ifp;
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
	if (ifp != NULL) {
		if (pcb->dp_drv_input != NULL)
			ifp->if_input = pcb->dp_drv_input;
		if (pcb->dp_event_started)
			ifp->if_debugnet_methods->dn_event(ifp, DEBUGNET_END);
	}
	debugnet_mbuf_finish();

	g_debugnet_pcb_inuse = false;
	memset(&g_dnet_pcb, 0xfd, sizeof(g_dnet_pcb));
}

int
debugnet_connect(const struct debugnet_conn_params *dcp,
    struct debugnet_pcb **pcb_out)
{
	struct debugnet_proto_aux herald_auxdata;
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
		.dp_client_port = dcp->dc_client_port,
		.dp_seqno = 1,
		.dp_ifp = dcp->dc_ifp,
		.dp_rx_handler = dcp->dc_rx_handler,
	};

	/* Switch to the debugnet mbuf zones. */
	debugnet_mbuf_start();

	/* At least one needed parameter is missing; infer it. */
	if (pcb->dp_client == INADDR_ANY || pcb->dp_gateway == INADDR_ANY ||
	    pcb->dp_ifp == NULL) {
		struct sockaddr_in dest_sin, *gw_sin, *local_sin;
		struct ifnet *rt_ifp;
		struct nhop_object *nh;

		memset(&dest_sin, 0, sizeof(dest_sin));
		dest_sin = (struct sockaddr_in) {
			.sin_len = sizeof(dest_sin),
			.sin_family = AF_INET,
			.sin_addr.s_addr = pcb->dp_server,
		};

		CURVNET_SET(vnet0);
		nh = fib4_lookup_debugnet(debugnet_fib, dest_sin.sin_addr, 0,
		    NHR_NONE);
		CURVNET_RESTORE();

		if (nh == NULL) {
			printf("%s: Could not get route for that server.\n",
			    __func__);
			error = ENOENT;
			goto cleanup;
		}

		/* TODO support AF_INET6 */
		if (nh->gw_sa.sa_family == AF_INET)
			gw_sin = &nh->gw4_sa;
		else {
			if (nh->gw_sa.sa_family == AF_LINK)
				DNETDEBUG("Destination address is on link.\n");
			gw_sin = NULL;
		}

		MPASS(nh->nh_ifa->ifa_addr->sa_family == AF_INET);
		local_sin = (struct sockaddr_in *)nh->nh_ifa->ifa_addr;

		rt_ifp = nh->nh_ifp;

		if (pcb->dp_client == INADDR_ANY)
			pcb->dp_client = local_sin->sin_addr.s_addr;
		if (pcb->dp_gateway == INADDR_ANY && gw_sin != NULL)
			pcb->dp_gateway = gw_sin->sin_addr.s_addr;
		if (pcb->dp_ifp == NULL)
			pcb->dp_ifp = rt_ifp;
	}

	ifp = pcb->dp_ifp;

	if (debugnet_debug > 0) {
		char serbuf[INET_ADDRSTRLEN], clibuf[INET_ADDRSTRLEN],
		    gwbuf[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &pcb->dp_server, serbuf, sizeof(serbuf));
		inet_ntop(AF_INET, &pcb->dp_client, clibuf, sizeof(clibuf));
		if (pcb->dp_gateway != INADDR_ANY)
			inet_ntop(AF_INET, &pcb->dp_gateway, gwbuf, sizeof(gwbuf));
		DNETDEBUG("Connecting to %s:%d%s%s from %s:%d on %s\n",
		    serbuf, pcb->dp_server_port,
		    (pcb->dp_gateway == INADDR_ANY) ? "" : " via ",
		    (pcb->dp_gateway == INADDR_ANY) ? "" : gwbuf,
		    clibuf, pcb->dp_client_port, if_name(ifp));
	}

	/* Validate iface is online and supported. */
	if (!DEBUGNET_SUPPORTED_NIC(ifp)) {
		printf("%s: interface '%s' does not support debugnet\n",
		    __func__, if_name(ifp));
		error = ENODEV;
		goto cleanup;
	}
	if ((if_getflags(ifp) & IFF_UP) == 0) {
		printf("%s: interface '%s' link is down\n", __func__,
		    if_name(ifp));
		error = ENXIO;
		goto cleanup;
	}

	ifp->if_debugnet_methods->dn_event(ifp, DEBUGNET_START);
	pcb->dp_event_started = true;

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

	herald_auxdata = (struct debugnet_proto_aux) {
		.dp_offset_start = dcp->dc_herald_offset,
		.dp_aux2 = dcp->dc_herald_aux2,
	};
	error = debugnet_send(pcb, DEBUGNET_HERALD, dcp->dc_herald_data,
	    dcp->dc_herald_datalen, &herald_auxdata);
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

	/*
	 * Bandaid for drivers that (incorrectly) advertise LinkUp before their
	 * dn_init method is available.
	 */
	if (nmbuf == 0 || ncl == 0 || clsize == 0) {
		printf("%s: Bad dn_init result from %s (ifp %p), ignoring.\n",
		    __func__, if_name(ifp), ifp);
		return;
	}
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

/*
 * DDB parsing helpers for debugnet(4) consumers.
 */
#ifdef DDB
struct my_inet_opt {
	bool has_opt;
	const char *printname;
	in_addr_t *result;
};

static int
dn_parse_optarg_ipv4(struct my_inet_opt *opt)
{
	in_addr_t tmp;
	unsigned octet;
	int t;

	tmp = 0;
	for (octet = 0; octet < 4; octet++) {
		t = db_read_token_flags(DRT_WSPACE | DRT_DECIMAL);
		if (t != tNUMBER) {
			db_printf("%s:%s: octet %u expected number; found %d\n",
			    __func__, opt->printname, octet, t);
			return (EINVAL);
		}
		/*
		 * db_lex lexes '-' distinctly from the number itself, but
		 * let's document that invariant.
		 */
		MPASS(db_tok_number >= 0);

		if (db_tok_number > UINT8_MAX) {
			db_printf("%s:%s: octet %u out of range: %jd\n", __func__,
			    opt->printname, octet, (intmax_t)db_tok_number);
			return (EDOM);
		}

		/* Constructed host-endian and converted to network later. */
		tmp = (tmp << 8) | db_tok_number;

		if (octet < 3) {
			t = db_read_token_flags(DRT_WSPACE);
			if (t != tDOT) {
				db_printf("%s:%s: octet %u expected '.'; found"
				    " %d\n", __func__, opt->printname, octet,
				    t);
				return (EINVAL);
			}
		}
	}

	*opt->result = htonl(tmp);
	opt->has_opt = true;
	return (0);
}

int
debugnet_parse_ddb_cmd(const char *cmd, struct debugnet_ddb_config *result)
{
	struct ifnet *ifp;
	int t, error;
	bool want_ifp;
	char ch;

	struct my_inet_opt opt_client = {
		.printname = "client",
		.result = &result->dd_client,
	},
	opt_server = {
		.printname = "server",
		.result = &result->dd_server,
	},
	opt_gateway = {
		.printname = "gateway",
		.result = &result->dd_gateway,
	},
	*cur_inet_opt;

	ifp = NULL;
	memset(result, 0, sizeof(*result));

	/*
	 * command [space] [-] [opt] [[space] [optarg]] ...
	 *
	 * db_command has already lexed 'command' for us.
	 */
	t = db_read_token_flags(DRT_WSPACE);
	if (t == tWSPACE)
		t = db_read_token_flags(DRT_WSPACE);

	while (t != tEOL) {
		if (t != tMINUS) {
			db_printf("%s: Bad syntax; expected '-', got %d\n",
			    cmd, t);
			goto usage;
		}

		t = db_read_token_flags(DRT_WSPACE);
		if (t != tIDENT) {
			db_printf("%s: Bad syntax; expected tIDENT, got %d\n",
			    cmd, t);
			goto usage;
		}

		if (strlen(db_tok_string) > 1) {
			db_printf("%s: Bad syntax; expected single option "
			    "flag, got '%s'\n", cmd, db_tok_string);
			goto usage;
		}

		want_ifp = false;
		cur_inet_opt = NULL;
		switch ((ch = db_tok_string[0])) {
		default:
			DNETDEBUG("Unexpected: '%c'\n", ch);
			/* FALLTHROUGH */
		case 'h':
			goto usage;
		case 'c':
			cur_inet_opt = &opt_client;
			break;
		case 'g':
			cur_inet_opt = &opt_gateway;
			break;
		case 's':
			cur_inet_opt = &opt_server;
			break;
		case 'i':
			want_ifp = true;
			break;
		}

		t = db_read_token_flags(DRT_WSPACE);
		if (t != tWSPACE) {
			db_printf("%s: Bad syntax; expected space after "
			    "flag %c, got %d\n", cmd, ch, t);
			goto usage;
		}

		if (want_ifp) {
			t = db_read_token_flags(DRT_WSPACE);
			if (t != tIDENT) {
				db_printf("%s: Expected interface but got %d\n",
				    cmd, t);
				goto usage;
			}

			CURVNET_SET(vnet0);
			/*
			 * We *don't* take a ref here because the only current
			 * consumer, db_netdump_cmd, does not need it.  It
			 * (somewhat redundantly) extracts the if_name(),
			 * re-lookups the ifp, and takes its own reference.
			 */
			ifp = ifunit(db_tok_string);
			CURVNET_RESTORE();
			if (ifp == NULL) {
				db_printf("Could not locate interface %s\n",
				    db_tok_string);
				goto cleanup;
			}
		} else {
			MPASS(cur_inet_opt != NULL);
			/* Assume IPv4 for now. */
			error = dn_parse_optarg_ipv4(cur_inet_opt);
			if (error != 0)
				goto cleanup;
		}

		/* Skip (mandatory) whitespace after option, if not EOL. */
		t = db_read_token_flags(DRT_WSPACE);
		if (t == tEOL)
			break;
		if (t != tWSPACE) {
			db_printf("%s: Bad syntax; expected space after "
			    "flag %c option; got %d\n", cmd, ch, t);
			goto usage;
		}
		t = db_read_token_flags(DRT_WSPACE);
	}

	if (!opt_server.has_opt) {
		db_printf("%s: need a destination server address\n", cmd);
		goto usage;
	}

	result->dd_has_client = opt_client.has_opt;
	result->dd_has_gateway = opt_gateway.has_opt;
	result->dd_ifp = ifp;

	/* We parsed the full line to tEOL already, or bailed with an error. */
	return (0);

usage:
	db_printf("Usage: %s -s <server> [-g <gateway> -c <localip> "
	    "-i <interface>]\n", cmd);
	error = EINVAL;
	/* FALLTHROUGH */
cleanup:
	db_skip_to_eol();
	return (error);
}
#endif /* DDB */
