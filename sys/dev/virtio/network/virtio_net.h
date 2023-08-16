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

/* The feature bitmap for virtio net */
#define VIRTIO_NET_F_CSUM		 0x000001 /* Host handles pkts w/ partial csum */
#define VIRTIO_NET_F_GUEST_CSUM		 0x000002 /* Guest handles pkts w/ partial csum*/
#define VIRTIO_NET_F_CTRL_GUEST_OFFLOADS 0x000004 /* Dynamic offload configuration. */
#define VIRTIO_NET_F_MTU		 0x000008 /* Initial MTU advice */
#define VIRTIO_NET_F_MAC		 0x000020 /* Host has given MAC address. */
#define VIRTIO_NET_F_GSO		 0x000040 /* Host handles pkts w/ any GSO type */
#define VIRTIO_NET_F_GUEST_TSO4		 0x000080 /* Guest can handle TSOv4 in. */
#define VIRTIO_NET_F_GUEST_TSO6		 0x000100 /* Guest can handle TSOv6 in. */
#define VIRTIO_NET_F_GUEST_ECN		 0x000200 /* Guest can handle TSO[6] w/ ECN in. */
#define VIRTIO_NET_F_GUEST_UFO		 0x000400 /* Guest can handle UFO in. */
#define VIRTIO_NET_F_HOST_TSO4		 0x000800 /* Host can handle TSOv4 in. */
#define VIRTIO_NET_F_HOST_TSO6		 0x001000 /* Host can handle TSOv6 in. */
#define VIRTIO_NET_F_HOST_ECN		 0x002000 /* Host can handle TSO[6] w/ ECN in. */
#define VIRTIO_NET_F_HOST_UFO		 0x004000 /* Host can handle UFO in. */
#define VIRTIO_NET_F_MRG_RXBUF		 0x008000 /* Host can merge receive buffers. */
#define VIRTIO_NET_F_STATUS		 0x010000 /* virtio_net_config.status available*/
#define VIRTIO_NET_F_CTRL_VQ		 0x020000 /* Control channel available */
#define VIRTIO_NET_F_CTRL_RX		 0x040000 /* Control channel RX mode support */
#define VIRTIO_NET_F_CTRL_VLAN		 0x080000 /* Control channel VLAN filtering */
#define VIRTIO_NET_F_CTRL_RX_EXTRA	 0x100000 /* Extra RX mode control support */
#define VIRTIO_NET_F_GUEST_ANNOUNCE	 0x200000 /* Announce device on network */
#define VIRTIO_NET_F_MQ			 0x400000 /* Device supports Receive Flow Steering */
#define VIRTIO_NET_F_CTRL_MAC_ADDR	 0x800000 /* Set MAC address */
#define VIRTIO_NET_F_SPEED_DUPLEX	 (1ULL << 63) /* Device set linkspeed and duplex */

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
 * driver has recevied the notification; device would clear the
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

/*
 * Use the checksum offset in the VirtIO header to set the
 * correct CSUM_* flags.
 */
static inline int
virtio_net_rx_csum_by_offset(struct mbuf *m, uint16_t eth_type, int ip_start,
			struct virtio_net_hdr *hdr)
{
#if defined(INET) || defined(INET6)
	int offset = hdr->csum_start + hdr->csum_offset;
#endif

	/* Only do a basic sanity check on the offset. */
	switch (eth_type) {
#if defined(INET)
	case ETHERTYPE_IP:
		if (__predict_false(offset < ip_start + sizeof(struct ip)))
			return (1);
		break;
#endif
#if defined(INET6)
	case ETHERTYPE_IPV6:
		if (__predict_false(offset < ip_start + sizeof(struct ip6_hdr)))
			return (1);
		break;
#endif
	default:
		/* Here we should increment the rx_csum_bad_ethtype counter. */
		return (1);
	}

	/*
	 * Use the offset to determine the appropriate CSUM_* flags. This is
	 * a bit dirty, but we can get by with it since the checksum offsets
	 * happen to be different. We assume the host host does not do IPv4
	 * header checksum offloading.
	 */
	switch (hdr->csum_offset) {
	case offsetof(struct udphdr, uh_sum):
	case offsetof(struct tcphdr, th_sum):
		m->m_pkthdr.csum_flags |= CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
		m->m_pkthdr.csum_data = 0xFFFF;
		break;
	default:
		/* Here we should increment the rx_csum_bad_offset counter. */
		return (1);
	}

	return (0);
}

static inline int
virtio_net_rx_csum_by_parse(struct mbuf *m, uint16_t eth_type, int ip_start,
		       struct virtio_net_hdr *hdr)
{
	int offset, proto;

	switch (eth_type) {
#if defined(INET)
	case ETHERTYPE_IP: {
		struct ip *ip;
		if (__predict_false(m->m_len < ip_start + sizeof(struct ip)))
			return (1);
		ip = (struct ip *)(m->m_data + ip_start);
		proto = ip->ip_p;
		offset = ip_start + (ip->ip_hl << 2);
		break;
	}
#endif
#if defined(INET6)
	case ETHERTYPE_IPV6:
		if (__predict_false(m->m_len < ip_start +
		    sizeof(struct ip6_hdr)))
			return (1);
		offset = ip6_lasthdr(m, ip_start, IPPROTO_IPV6, &proto);
		if (__predict_false(offset < 0))
			return (1);
		break;
#endif
	default:
		/* Here we should increment the rx_csum_bad_ethtype counter. */
		return (1);
	}

	switch (proto) {
	case IPPROTO_TCP:
		if (__predict_false(m->m_len < offset + sizeof(struct tcphdr)))
			return (1);
		m->m_pkthdr.csum_flags |= CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
		m->m_pkthdr.csum_data = 0xFFFF;
		break;
	case IPPROTO_UDP:
		if (__predict_false(m->m_len < offset + sizeof(struct udphdr)))
			return (1);
		m->m_pkthdr.csum_flags |= CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
		m->m_pkthdr.csum_data = 0xFFFF;
		break;
	default:
		/*
		 * For the remaining protocols, FreeBSD does not support
		 * checksum offloading, so the checksum will be recomputed.
		 */
#if 0
		if_printf(ifp, "cksum offload of unsupported "
		    "protocol eth_type=%#x proto=%d csum_start=%d "
		    "csum_offset=%d\n", __func__, eth_type, proto,
		    hdr->csum_start, hdr->csum_offset);
#endif
		break;
	}

	return (0);
}

/*
 * Set the appropriate CSUM_* flags. Unfortunately, the information
 * provided is not directly useful to us. The VirtIO header gives the
 * offset of the checksum, which is all Linux needs, but this is not
 * how FreeBSD does things. We are forced to peek inside the packet
 * a bit.
 *
 * It would be nice if VirtIO gave us the L4 protocol or if FreeBSD
 * could accept the offsets and let the stack figure it out.
 */
static inline int
virtio_net_rx_csum(struct mbuf *m, struct virtio_net_hdr *hdr)
{
	struct ether_header *eh;
	struct ether_vlan_header *evh;
	uint16_t eth_type;
	int offset, error;

	if ((hdr->flags & (VIRTIO_NET_HDR_F_NEEDS_CSUM |
	    VIRTIO_NET_HDR_F_DATA_VALID)) == 0) {
		return (0);
	}

	eh = mtod(m, struct ether_header *);
	eth_type = ntohs(eh->ether_type);
	if (eth_type == ETHERTYPE_VLAN) {
		/* BMV: We should handle nested VLAN tags too. */
		evh = mtod(m, struct ether_vlan_header *);
		eth_type = ntohs(evh->evl_proto);
		offset = sizeof(struct ether_vlan_header);
	} else
		offset = sizeof(struct ether_header);

	if (hdr->flags & VIRTIO_NET_HDR_F_NEEDS_CSUM)
		error = virtio_net_rx_csum_by_offset(m, eth_type, offset, hdr);
	else
		error = virtio_net_rx_csum_by_parse(m, eth_type, offset, hdr);

	return (error);
}

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
		/* Here we should increment the tx_csum_bad_ethtype counter. */
		return (EINVAL);
	}

	return (0);
}

static inline int
virtio_net_tx_offload_tso(if_t ifp, struct mbuf *m, int eth_type,
		     int offset, bool allow_ecn, struct virtio_net_hdr *hdr)
{
	static struct timeval lastecn;
	static int curecn;
	struct tcphdr *tcp, tcphdr;

	if (__predict_false(m->m_len < offset + sizeof(struct tcphdr))) {
		m_copydata(m, offset, sizeof(struct tcphdr), (caddr_t) &tcphdr);
		tcp = &tcphdr;
	} else
		tcp = (struct tcphdr *)(m->m_data + offset);

	hdr->hdr_len = offset + (tcp->th_off << 2);
	hdr->gso_size = m->m_pkthdr.tso_segsz;
	hdr->gso_type = eth_type == ETHERTYPE_IP ? VIRTIO_NET_HDR_GSO_TCPV4 :
	    VIRTIO_NET_HDR_GSO_TCPV6;

	if (tcp->th_flags & TH_CWR) {
		/*
		 * Drop if VIRTIO_NET_F_HOST_ECN was not negotiated. In FreeBSD,
		 * ECN support is not on a per-interface basis, but globally via
		 * the net.inet.tcp.ecn.enable sysctl knob. The default is off.
		 */
		if (!allow_ecn) {
			if (ppsratecheck(&lastecn, &curecn, 1))
				if_printf(ifp,
				    "TSO with ECN not negotiated with host\n");
			return (ENOTSUP);
		}
		hdr->gso_type |= VIRTIO_NET_HDR_GSO_ECN;
	}

	/* Here we should increment tx_tso counter. */

	return (0);
}

static inline struct mbuf *
virtio_net_tx_offload(if_t ifp, struct mbuf *m, bool allow_ecn,
		 struct virtio_net_hdr *hdr)
{
	int flags, etype, csum_start, proto, error;

	flags = m->m_pkthdr.csum_flags;

	error = virtio_net_tx_offload_ctx(m, &etype, &proto, &csum_start);
	if (error)
		goto drop;

	if ((etype == ETHERTYPE_IP && (flags & (CSUM_TCP | CSUM_UDP))) ||
	    (etype == ETHERTYPE_IPV6 &&
	        (flags & (CSUM_TCP_IPV6 | CSUM_UDP_IPV6)))) {
		/*
		 * We could compare the IP protocol vs the CSUM_ flag too,
		 * but that really should not be necessary.
		 */
		hdr->flags |= VIRTIO_NET_HDR_F_NEEDS_CSUM;
		hdr->csum_start = csum_start;
		hdr->csum_offset = m->m_pkthdr.csum_data;
		/* Here we should increment the tx_csum counter. */
	}

	if (flags & CSUM_TSO) {
		if (__predict_false(proto != IPPROTO_TCP)) {
			/* Likely failed to correctly parse the mbuf.
			 * Here we should increment the tx_tso_not_tcp
			 * counter. */
			goto drop;
		}

		KASSERT(hdr->flags & VIRTIO_NET_HDR_F_NEEDS_CSUM,
		    ("%s: mbuf %p TSO without checksum offload %#x",
		    __func__, m, flags));

		error = virtio_net_tx_offload_tso(ifp, m, etype, csum_start,
					     allow_ecn, hdr);
		if (error)
			goto drop;
	}

	return (m);

drop:
	m_freem(m);
	return (NULL);
}

#endif /* _VIRTIO_NET_H */
