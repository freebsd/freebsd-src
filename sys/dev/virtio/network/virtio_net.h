/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of IBM nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL IBM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _VIRTIO_NET_H
#define _VIRTIO_NET_H

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/endian.h>

/* The feature bitmap for virtio net */
#define VIRTIO_NET_F_CSUM		 (1ULL <<  0) /* Host handles pkts w/ partial csum */
#define VIRTIO_NET_F_GUEST_CSUM		 (1ULL <<  1) /* Guest handles pkts w/ partial csum*/
#define VIRTIO_NET_F_CTRL_GUEST_OFFLOADS (1ULL <<  2) /* Dynamic offload configuration. */
#define VIRTIO_NET_F_MTU		 (1ULL <<  3) /* Initial MTU advice */
#define VIRTIO_NET_F_MAC		 (1ULL <<  5) /* Host has given MAC address. */
#define VIRTIO_NET_F_GSO		 (1ULL <<  6) /* Host handles pkts w/ any GSO type */
#define VIRTIO_NET_F_GUEST_TSO4		 (1ULL <<  7) /* Guest can handle TSOv4 in. */
#define VIRTIO_NET_F_GUEST_TSO6		 (1ULL <<  8) /* Guest can handle TSOv6 in. */
#define VIRTIO_NET_F_GUEST_ECN		 (1ULL <<  9) /* Guest can handle TSO[6] w/ ECN in. */
#define VIRTIO_NET_F_GUEST_UFO		 (1ULL << 10) /* Guest can handle UFO in. */
#define VIRTIO_NET_F_HOST_TSO4		 (1ULL << 11) /* Host can handle TSOv4 in. */
#define VIRTIO_NET_F_HOST_TSO6		 (1ULL << 12) /* Host can handle TSOv6 in. */
#define VIRTIO_NET_F_HOST_ECN		 (1ULL << 13) /* Host can handle TSO[6] w/ ECN in. */
#define VIRTIO_NET_F_HOST_UFO		 (1ULL << 14) /* Host can handle UFO in. */
#define VIRTIO_NET_F_MRG_RXBUF		 (1ULL << 15) /* Host can merge receive buffers. */
#define VIRTIO_NET_F_STATUS		 (1ULL << 16) /* virtio_net_config.status available*/
#define VIRTIO_NET_F_CTRL_VQ		 (1ULL << 17) /* Control channel available */
#define VIRTIO_NET_F_CTRL_RX		 (1ULL << 18) /* Control channel RX mode support */
#define VIRTIO_NET_F_CTRL_VLAN		 (1ULL << 19) /* Control channel VLAN filtering */
#define VIRTIO_NET_F_CTRL_RX_EXTRA	 (1ULL << 20) /* Extra RX mode control support */
#define VIRTIO_NET_F_GUEST_ANNOUNCE	 (1ULL << 21) /* Announce device on network */
#define VIRTIO_NET_F_MQ			 (1ULL << 22) /* Device supports Receive Flow Steering */
#define VIRTIO_NET_F_CTRL_MAC_ADDR	 (1ULL << 23) /* Set MAC address */
#define VIRTIO_NET_F_SPEED_DUPLEX	 (1ULL << 63) /* Device set linkspeed and duplex */

/* virtio net feature flag descriptions for use with printf(9) %b identifier. */
#define VIRTIO_NET_FEATURE_BITS \
    "\20\200CSUM\201GUEST_CSUM\202CTRL_GUEST_OFFLOADS\203MTU\205MAC\206GSO" \
    "\207GUEST_TSO4\210GUEST_TSO6\211GUEST_ECN\212GUEST_UFO\213HOST_TSO4" \
    "\214HOST_TSO6\215HOST_ECN\216HOST_UFO\217MRG_RXBUF\220STATUS\221CTRL_VQ" \
    "\222CTRL_RX\223CTRL_VLAN\224CTRL_RX_EXTRA\225GUEST_ANNOUNCE\226MQ" \
    "\227CTRL_MAC_ADDR\277SPEED_DUPLEX"

#define VIRTIO_NET_S_LINK_UP	1	/* Link is up */
#define VIRTIO_NET_S_ANNOUNCE	2	/* Announcement is needed */

struct virtio_net_config {
	/* The config defining mac address (if VIRTIO_NET_F_MAC) */
	uint8_t		mac[ETHER_ADDR_LEN];
	/* See VIRTIO_NET_F_STATUS and VIRTIO_NET_S_* above */
	uint16_t	status;
	/* Maximum number of each of transmit and receive queues;
	 * see VIRTIO_NET_F_MQ and VIRTIO_NET_CTRL_MQ.
	 * Legal values are between 1 and 0x8000.
	 */
	uint16_t	max_virtqueue_pairs;
	/* Default maximum transmit unit advice */
	uint16_t	mtu;
	/*
	 * speed, in units of 1Mb. All values 0 to INT_MAX are legal.
	 * Any other value stands for unknown.
	 */
	uint32_t	speed;
	/*
	 * 0x00 - half duplex
	 * 0x01 - full duplex
	 * Any other value stands for unknown.
	 */
	uint8_t		duplex;
} __packed;

/*
 * This header comes first in the scatter-gather list.  If you don't
 * specify GSO or CSUM features, you can simply ignore the header.
 *
 * This is bitwise-equivalent to the legacy struct virtio_net_hdr_mrg_rxbuf,
 * only flattened.
 */
struct virtio_net_hdr_v1 {
#define VIRTIO_NET_HDR_F_NEEDS_CSUM	1	/* Use csum_start, csum_offset */
#define VIRTIO_NET_HDR_F_DATA_VALID	2	/* Csum is valid */
	uint8_t flags;
#define VIRTIO_NET_HDR_GSO_NONE		0	/* Not a GSO frame */
#define VIRTIO_NET_HDR_GSO_TCPV4	1	/* GSO frame, IPv4 TCP (TSO) */
#define VIRTIO_NET_HDR_GSO_UDP		3	/* GSO frame, IPv4 UDP (UFO) */
#define VIRTIO_NET_HDR_GSO_TCPV6	4	/* GSO frame, IPv6 TCP */
#define VIRTIO_NET_HDR_GSO_ECN		0x80	/* TCP has ECN set */
	uint8_t gso_type;
	uint16_t hdr_len;	/* Ethernet + IP + tcp/udp hdrs */
	uint16_t gso_size;	/* Bytes to append to hdr_len per frame */
	uint16_t csum_start;	/* Position to start checksumming from */
	uint16_t csum_offset;	/* Offset after that to place checksum */
	uint16_t num_buffers;	/* Number of merged rx buffers */
};

/*
 * This header comes first in the scatter-gather list.
 * For legacy virtio, if VIRTIO_F_ANY_LAYOUT is not negotiated, it must
 * be the first element of the scatter-gather list.  If you don't
 * specify GSO or CSUM features, you can simply ignore the header.
 */
struct virtio_net_hdr {
	/* See VIRTIO_NET_HDR_F_* */
	uint8_t	flags;
	/* See VIRTIO_NET_HDR_GSO_* */
	uint8_t gso_type;
	uint16_t hdr_len;	/* Ethernet + IP + tcp/udp hdrs */
	uint16_t gso_size;	/* Bytes to append to hdr_len per frame */
	uint16_t csum_start;	/* Position to start checksumming from */
	uint16_t csum_offset;	/* Offset after that to place checksum */
};

/*
 * This is the version of the header to use when the MRG_RXBUF
 * feature has been negotiated.
 */
struct virtio_net_hdr_mrg_rxbuf {
	struct virtio_net_hdr hdr;
	uint16_t num_buffers;	/* Number of merged rx buffers */
};

/*
 * Control virtqueue data structures
 *
 * The control virtqueue expects a header in the first sg entry
 * and an ack/status response in the last entry.  Data for the
 * command goes in between.
 */
struct virtio_net_ctrl_hdr {
	uint8_t class;
	uint8_t cmd;
} __packed;

#define VIRTIO_NET_OK	0
#define VIRTIO_NET_ERR	1

/*
 * Control the RX mode, ie. promiscuous, allmulti, etc...
 * All commands require an "out" sg entry containing a 1 byte
 * state value, zero = disable, non-zero = enable.  Commands
 * 0 and 1 are supported with the VIRTIO_NET_F_CTRL_RX feature.
 * Commands 2-5 are added with VIRTIO_NET_F_CTRL_RX_EXTRA.
 */
#define VIRTIO_NET_CTRL_RX	0
#define VIRTIO_NET_CTRL_RX_PROMISC	0
#define VIRTIO_NET_CTRL_RX_ALLMULTI	1
#define VIRTIO_NET_CTRL_RX_ALLUNI	2
#define VIRTIO_NET_CTRL_RX_NOMULTI	3
#define VIRTIO_NET_CTRL_RX_NOUNI	4
#define VIRTIO_NET_CTRL_RX_NOBCAST	5

/*
 * Control the MAC filter table.
 *
 * The MAC filter table is managed by the hypervisor, the guest should
 * assume the size is infinite.  Filtering should be considered
 * non-perfect, ie. based on hypervisor resources, the guest may
 * received packets from sources not specified in the filter list.
 *
 * In addition to the class/cmd header, the TABLE_SET command requires
 * two out scatterlists.  Each contains a 4 byte count of entries followed
 * by a concatenated byte stream of the ETH_ALEN MAC addresses.  The
 * first sg list contains unicast addresses, the second is for multicast.
 * This functionality is present if the VIRTIO_NET_F_CTRL_RX feature
 * is available.
 *
 * The ADDR_SET command requests one out scatterlist, it contains a
 * 6 bytes MAC address. This functionality is present if the
 * VIRTIO_NET_F_CTRL_MAC_ADDR feature is available.
 */
struct virtio_net_ctrl_mac {
	uint32_t	entries;
	uint8_t		macs[][ETHER_ADDR_LEN];
} __packed;

#define VIRTIO_NET_CTRL_MAC	1
#define VIRTIO_NET_CTRL_MAC_TABLE_SET	0
#define VIRTIO_NET_CTRL_MAC_ADDR_SET	1

/*
 * Control VLAN filtering
 *
 * The VLAN filter table is controlled via a simple ADD/DEL interface.
 * VLAN IDs not added may be filtered by the hypervisor.  Del is the
 * opposite of add.  Both commands expect an out entry containing a 2
 * byte VLAN ID.  VLAN filtering is available with the
 * VIRTIO_NET_F_CTRL_VLAN feature bit.
 */
#define VIRTIO_NET_CTRL_VLAN	2
#define VIRTIO_NET_CTRL_VLAN_ADD	0
#define VIRTIO_NET_CTRL_VLAN_DEL	1

/*
 * Control link announce acknowledgement
 *
 * The command VIRTIO_NET_CTRL_ANNOUNCE_ACK is used to indicate that
 * driver has received the notification; device would clear the
 * VIRTIO_NET_S_ANNOUNCE bit in the status field after it receives
 * this command.
 */
#define VIRTIO_NET_CTRL_ANNOUNCE	3
#define VIRTIO_NET_CTRL_ANNOUNCE_ACK	0

/*
 * Control Receive Flow Steering
 *
 * The command VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET enables Receive Flow
 * Steering, specifying the number of the transmit and receive queues
 * that will be used. After the command is consumed and acked by the
 * device, the device will not steer new packets on receive virtqueues
 * other than specified nor read from transmit virtqueues other than
 * specified. Accordingly, driver should not transmit new packets on
 * virtqueues other than specified.
 */
struct virtio_net_ctrl_mq {
	uint16_t	virtqueue_pairs;
} __packed;

#define VIRTIO_NET_CTRL_MQ	4
#define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET		0
#define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_MIN		1
#define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_MAX		0x8000

/*
 * Control network offloads
 *
 * Reconfigures the network offloads that Guest can handle.
 *
 * Available with the VIRTIO_NET_F_CTRL_GUEST_OFFLOADS feature bit.
 *
 * Command data format matches the feature bit mask exactly.
 *
 * See VIRTIO_NET_F_GUEST_* for the list of offloads
 * that can be enabled/disabled.
 */
#define VIRTIO_NET_CTRL_GUEST_OFFLOADS		5
#define VIRTIO_NET_CTRL_GUEST_OFFLOADS_SET	0

#if defined(INET) || defined(INET6)
static inline void
virtio_net_rx_csum_needs_csum(struct mbuf *m, bool isipv6, int protocol,
    struct virtio_net_hdr *hdr)
{
	/*
	 * The packet is likely from another VM on the same host or from the
	 * host that itself performed checksum offloading so Tx/Rx is basically
	 * a memcpy and the checksum has little value so far.
	 */

	KASSERT(protocol == IPPROTO_TCP || protocol == IPPROTO_UDP,
	    ("%s: unsupported IP protocol %d", __func__, protocol));

	/*
	 * Just forward the order to compute the checksum by setting
	 * the corresponding mbuf flag (e.g., CSUM_TCP).
	 */
	switch (protocol) {
	case IPPROTO_TCP:
		m->m_pkthdr.csum_flags |= (isipv6 ? CSUM_TCP_IPV6 : CSUM_TCP);
		break;
	case IPPROTO_UDP:
		m->m_pkthdr.csum_flags |= (isipv6 ? CSUM_UDP_IPV6 : CSUM_UDP);
		break;
	}
	m->m_pkthdr.csum_data = hdr->csum_offset;
}

static inline void
virtio_net_rx_csum_data_valid(struct mbuf *m, int protocol)
{
	KASSERT(protocol == IPPROTO_TCP || protocol == IPPROTO_UDP,
	    ("%s: unsupported IP protocol %d", __func__, protocol));

	m->m_pkthdr.csum_flags |= CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
	m->m_pkthdr.csum_data = 0xFFFF;
}

#define VIRTIO_NET_RX_CSUM_INACCESSIBLE_IPPROTO 1
#define VIRTIO_NET_RX_CSUM_BAD_ETHTYPE 2
#define VIRTIO_NET_RX_CSUM_BAD_IPPROTO 3

/*
 * For a packet received over the VirtIO channel, it checks the given
 * VirtIO header and sets the appropriate CSUM_* flags in the given mbuf.
 *
 * Unfortunately, the information provided is not directly useful to us. The
 * VirtIO header gives the offset of the checksum, which is all Linux needs, but
 * this is not how FreeBSD does things. We are forced to peek inside the packet
 * a bit.
 *
 * It would be nice if VirtIO gave us the L4 protocol or if FreeBSD
 * could accept the offsets and let the stack figure it out.
 *
 * @param m	mbuf of the packet where CSUM_* flags might need to be set.
 * @param hdr	VirtIO header of the received packet that needs to be checked
 *              with its field values stored in the byte order this machine
 *              uses (i.e., readable without a byte swap).
 *
 * @return 0 on success, or one of the VIRTIO_NET_RX_CSUM_* error codes.
 */
static inline int
virtio_net_rx_csum(struct mbuf *m, struct virtio_net_hdr *hdr)
{
	const struct ether_header *eh;
	int hoff, protocol;
	uint16_t etype;
	bool isipv6;

	KASSERT(hdr->flags &
	    (VIRTIO_NET_HDR_F_NEEDS_CSUM | VIRTIO_NET_HDR_F_DATA_VALID),
	    ("%s: missing checksum offloading flag %x", __func__, hdr->flags));

	eh = mtod(m, const struct ether_header *);
	etype = ntohs(eh->ether_type);
	if (etype == ETHERTYPE_VLAN) {
		/* TODO BMV: Handle QinQ. */
		const struct ether_vlan_header *evh =
		    mtod(m, const struct ether_vlan_header *);
		etype = ntohs(evh->evl_proto);
		hoff = sizeof(struct ether_vlan_header);
	} else
		hoff = sizeof(struct ether_header);

	/* Check whether ethernet type is IP or IPv6, and get protocol. */
	switch (etype) {
#if defined(INET)
	case ETHERTYPE_IP:
		if (__predict_false(m->m_len < hoff + sizeof(struct ip))) {
			return (VIRTIO_NET_RX_CSUM_INACCESSIBLE_IPPROTO);
		} else {
			struct ip *ip = (struct ip *)(m->m_data + hoff);
			protocol = ip->ip_p;
		}
		isipv6 = false;
		break;
#endif
#if defined(INET6)
	case ETHERTYPE_IPV6:
		if (__predict_false(m->m_len < hoff + sizeof(struct ip6_hdr))
		    || ip6_lasthdr(m, hoff, IPPROTO_IPV6, &protocol) < 0) {
			return (VIRTIO_NET_RX_CSUM_INACCESSIBLE_IPPROTO);
		}
		isipv6 = true;
		break;
#endif
	default:
		return (VIRTIO_NET_RX_CSUM_BAD_ETHTYPE);
	}

	/* Check whether protocol is TCP or UDP. */
	switch (protocol) {
	case IPPROTO_TCP:
	case IPPROTO_UDP:
		break;
	default:
		/*
		 * FreeBSD does not support checksum offloading of this
		 * protocol here.
		 */
		return (VIRTIO_NET_RX_CSUM_BAD_IPPROTO);
	}

	if (hdr->flags & VIRTIO_NET_HDR_F_NEEDS_CSUM)
		virtio_net_rx_csum_needs_csum(m, isipv6, protocol, hdr);
	else /* VIRTIO_NET_HDR_F_DATA_VALID */
		virtio_net_rx_csum_data_valid(m, protocol);

	return (0);
}
#endif

#define VIRTIO_NET_TX_OFFLOAD_UNKNOWN_ETHTYPE 1
#define VIRTIO_NET_TX_OFFLOAD_PROTO_MISMATCH 2
#define VIRTIO_NET_TX_OFFLOAD_TSO_NOT_TCP 3
#define VIRTIO_NET_TX_OFFLOAD_TSO_WITHOUT_CSUM 4
#define VIRTIO_NET_TX_OFFLOAD_TSO_ECN_UNEXPECTED 5

#define VIRTIO_NET_TX_MODERN_LE(modern, val) (modern ? htole16(val) : val)

/*
 * BMV: This can go away once we finally have offsets in the mbuf header.
 */
static inline int
virtio_net_tx_offload_ctx(struct mbuf *m, int *etype, int *proto, int *start)
{
	struct ether_vlan_header *evh;
#if defined(INET) || defined(INET6)
	int offset;
#endif

	evh = mtod(m, struct ether_vlan_header *);
	if (evh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		/* BMV: We should handle nested VLAN tags too. */
		*etype = ntohs(evh->evl_proto);
#if defined(INET) || defined(INET6)
		offset = sizeof(struct ether_vlan_header);
#endif
	} else {
		*etype = ntohs(evh->evl_encap_proto);
#if defined(INET) || defined(INET6)
		offset = sizeof(struct ether_header);
#endif
	}

	switch (*etype) {
#if defined(INET)
	case ETHERTYPE_IP: {
		struct ip *ip, iphdr;
		if (__predict_false(m->m_len < offset + sizeof(struct ip))) {
			m_copydata(m, offset, sizeof(struct ip),
			    (caddr_t) &iphdr);
			ip = &iphdr;
		} else
			ip = (struct ip *)(m->m_data + offset);
		*proto = ip->ip_p;
		*start = offset + (ip->ip_hl << 2);
		break;
	}
#endif
#if defined(INET6)
	case ETHERTYPE_IPV6:
		*proto = -1;
		*start = ip6_lasthdr(m, offset, IPPROTO_IPV6, proto);
		/* Assert the network stack sent us a valid packet. */
		KASSERT(*start > offset,
		    ("%s: mbuf %p start %d offset %d proto %d", __func__, m,
		    *start, offset, *proto));
		break;
#endif
	default:
		return (VIRTIO_NET_TX_OFFLOAD_UNKNOWN_ETHTYPE);
	}

	return (0);
}

static inline int
virtio_net_tx_offload_tso(struct ifnet *ifp, struct mbuf *m, int eth_type,
    int offset, struct virtio_net_hdr *hdr, bool tso_ecn, bool modern)
{
	static struct timeval lastecn;
	static int curecn;
	struct tcphdr *tcp, tcphdr;

	if (__predict_false(m->m_len < offset + sizeof(struct tcphdr))) {
		m_copydata(m, offset, sizeof(struct tcphdr), (caddr_t) &tcphdr);
		tcp = &tcphdr;
	} else
		tcp = (struct tcphdr *)(m->m_data + offset);

	/*
	 * Set VirtIO header fields with the correct byte order.
	 * In modern mode, this is little endian (LE).
	 * In legacy mode, this is the endianness of the guest, which means a
	 * FreeBSD guest can use its native endianness and a host must use the
	 * guests endianness. However, since a FreeBSD host with bhyve runs only
	 * on LE systems and supports only LE guests, no conversion is required.
	 */
	hdr->hdr_len = VIRTIO_NET_TX_MODERN_LE(modern,
	    offset + (tcp->th_off << 2));
	hdr->gso_size = VIRTIO_NET_TX_MODERN_LE(modern, m->m_pkthdr.tso_segsz);
	hdr->gso_type = eth_type == ETHERTYPE_IP ? VIRTIO_NET_HDR_GSO_TCPV4 :
	    VIRTIO_NET_HDR_GSO_TCPV6;

	if (__predict_false(tcp_get_flags(tcp) & TH_CWR)) {
		/*
		 * Drop if VIRTIO_NET_F_HOST_ECN was not negotiated. In
		 * FreeBSD, ECN support is not on a per-interface basis,
		 * but globally via the net.inet.tcp.ecn.enable sysctl
		 * knob. The default is off.
		 */
		if (!tso_ecn) {
			if (ppsratecheck(&lastecn, &curecn, 1))
				if_printf(ifp,
				    "TSO with ECN not negotiated with host\n");
			return (VIRTIO_NET_TX_OFFLOAD_TSO_ECN_UNEXPECTED);
		}
		hdr->gso_type |= VIRTIO_NET_HDR_GSO_ECN;
	}

	return (0);
}

/*
 * For a packet to be transmitted over the VirtIO channel, it checks the
 * CSUM_* flags in the mbuf and sets the appropriate flags in the VirtIO header.
 * In case of an error, it frees the mbuf and sets the pointer referenced by mp
 * to NULL.
 *
 * @param ifp		ifnet struct of outgoing interface.
 * @param mp		mbuf on which the CSUM_* flags needs to be checked.
 * @param hdr		VirtIO header to be filled for the outgoing packet.
 * @param tso_ecn	true if ECN has been negotiated between host and guest.
 * @param modern	true if VirtIO modern mode is used.
 *
 * @return 0 on success, or one of the VIRTIO_NET_TX_OFFLOAD_* error codes.
 */
static inline int
virtio_net_tx_offload(struct ifnet *ifp, struct mbuf **mp,
    struct virtio_net_hdr *hdr, bool tso_ecn, bool modern)
{
	int flags, etype, csum_start, proto, error;
	struct mbuf *m;

	m = *mp;
	flags = m->m_pkthdr.csum_flags;

	error = virtio_net_tx_offload_ctx(m, &etype, &proto, &csum_start);
	if (error != 0)
		goto drop;

	if (flags & (CSUM_TCP | CSUM_UDP | CSUM_TCP_IPV6 | CSUM_UDP_IPV6)) {
		/* Sanity check the parsed mbuf matches the offload flags. */
		if (__predict_false((flags & (CSUM_TCP | CSUM_UDP) &&
		    etype != ETHERTYPE_IP) ||
		    (flags & (CSUM_TCP_IPV6 | CSUM_UDP_IPV6) &&
		    etype != ETHERTYPE_IPV6))) {
			error = VIRTIO_NET_TX_OFFLOAD_PROTO_MISMATCH;
			goto drop;
		}

		/*
		 * Set VirtIO header fields with the correct byte order.
		 * See comment in virtio_net_tx_offload_tso()
		 */
		hdr->flags |= VIRTIO_NET_HDR_F_NEEDS_CSUM;
		hdr->csum_start = VIRTIO_NET_TX_MODERN_LE(modern, csum_start);
		hdr->csum_offset = VIRTIO_NET_TX_MODERN_LE(modern,
		    m->m_pkthdr.csum_data);
	}

	if (flags & (CSUM_IP_TSO | CSUM_IP6_TSO)) {
		/*
		 * Sanity check the parsed mbuf IP protocol is TCP, and
		 * VirtIO TSO reqires the checksum offloading above.
		 */
		if (__predict_false(proto != IPPROTO_TCP)) {
			error = VIRTIO_NET_TX_OFFLOAD_TSO_NOT_TCP;
			goto drop;
		} else if (__predict_false((hdr->flags &
		    VIRTIO_NET_HDR_F_NEEDS_CSUM) == 0)) {
			error = VIRTIO_NET_TX_OFFLOAD_TSO_WITHOUT_CSUM;
			goto drop;
		}

		error = virtio_net_tx_offload_tso(ifp, m, etype, csum_start,
		    hdr, tso_ecn, modern);
		if (error != 0)
			goto drop;
	}

	return (error);

drop:
	m_freem(m);
	*mp = NULL;
	return (error);
}

#endif /* _VIRTIO_NET_H */
