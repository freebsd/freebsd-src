/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2024, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file ice_common_txrx.h
 * @brief common Tx/Rx utility functions
 *
 * Contains common utility functions for the Tx/Rx hot path.
 *
 * The functions do depend on the if_pkt_info_t structure. A suitable
 * implementation of this structure must be provided if these functions are to
 * be used without the iflib networking stack.
 */

#ifndef _ICE_COMMON_TXRX_H_
#define _ICE_COMMON_TXRX_H_

#include <netinet/udp.h>
#include <netinet/sctp.h>

/**
 * ice_tso_detect_sparse - detect TSO packets with too many segments
 * @pi: packet information
 *
 * Hardware only transmits packets with a maximum of 8 descriptors. For TSO
 * packets, hardware needs to be able to build the split packets using 8 or
 * fewer descriptors. Additionally, the header must be contained within at
 * most 3 descriptors.
 *
 * To verify this, we walk the headers to find out how many descriptors the
 * headers require (usually 1). Then we ensure that, for each TSO segment, its
 * data plus the headers are contained within 8 or fewer descriptors.
 */
static inline int
ice_tso_detect_sparse(if_pkt_info_t pi)
{
	int count, curseg, i, hlen, segsz, seglen, tsolen, hdrs, maxsegs;
	bus_dma_segment_t *segs = pi->ipi_segs;
	int nsegs = pi->ipi_nsegs;

	curseg = hdrs = 0;

	hlen = pi->ipi_ehdrlen + pi->ipi_ip_hlen + pi->ipi_tcp_hlen;
	tsolen = pi->ipi_len - hlen;

	/* First, count the number of descriptors for the header.
	 * Additionally, make sure it does not span more than 3 segments.
	 */
	i = 0;
	curseg = segs[0].ds_len;
	while (hlen > 0) {
		hdrs++;
		if (hdrs > ICE_MAX_TSO_HDR_SEGS)
			return (1);
		if (curseg == 0) {
			i++;
			if (__predict_false(i == nsegs))
				return (1);

			curseg = segs[i].ds_len;
		}
		seglen = min(curseg, hlen);
		curseg -= seglen;
		hlen -= seglen;
	}

	maxsegs = ICE_MAX_TX_SEGS - hdrs;

	/* We must count the headers, in order to verify that they take up
	 * 3 or fewer descriptors. However, we don't need to check the data
	 * if the total segments is small.
	 */
	if (nsegs <= maxsegs)
		return (0);

	count = 0;

	/* Now check the data to make sure that each TSO segment is made up of
	 * no more than maxsegs descriptors. This ensures that hardware will
	 * be capable of performing TSO offload.
	 */
	while (tsolen > 0) {
		segsz = pi->ipi_tso_segsz;
		while (segsz > 0 && tsolen != 0) {
			count++;
			if (count > maxsegs) {
				return (1);
			}
			if (curseg == 0) {
				i++;
				if (__predict_false(i == nsegs)) {
					return (1);
				}
				curseg = segs[i].ds_len;
			}
			seglen = min(curseg, segsz);
			segsz -= seglen;
			curseg -= seglen;
			tsolen -= seglen;
		}
		count = 0;
	}

	return (0);
}

/**
 * ice_tso_setup - Setup a context descriptor to prepare for a TSO packet
 * @txq: the Tx queue to use
 * @pi: the packet info to prepare for
 *
 * Setup a context descriptor in preparation for sending a Tx packet that
 * requires the TSO offload. Returns the index of the descriptor to use when
 * encapsulating the Tx packet data into descriptors.
 */
static inline int
ice_tso_setup(struct ice_tx_queue *txq, if_pkt_info_t pi)
{
	struct ice_tx_ctx_desc		*txd;
	u32				cmd, mss, type, tsolen;
	int				idx;
	u64				type_cmd_tso_mss;

	idx = pi->ipi_pidx;
	txd = (struct ice_tx_ctx_desc *)&txq->tx_base[idx];
	tsolen = pi->ipi_len - (pi->ipi_ehdrlen + pi->ipi_ip_hlen + pi->ipi_tcp_hlen);

	type = ICE_TX_DESC_DTYPE_CTX;
	cmd = ICE_TX_CTX_DESC_TSO;
	/* TSO MSS must not be less than 64 */
	if (pi->ipi_tso_segsz < ICE_MIN_TSO_MSS) {
		txq->stats.mss_too_small++;
		pi->ipi_tso_segsz = ICE_MIN_TSO_MSS;
	}
	mss = pi->ipi_tso_segsz;

	type_cmd_tso_mss = ((u64)type << ICE_TXD_CTX_QW1_DTYPE_S) |
	    ((u64)cmd << ICE_TXD_CTX_QW1_CMD_S) |
	    ((u64)tsolen << ICE_TXD_CTX_QW1_TSO_LEN_S) |
	    ((u64)mss << ICE_TXD_CTX_QW1_MSS_S);
	txd->qw1 = htole64(type_cmd_tso_mss);

	txd->tunneling_params = htole32(0);
	txq->tso++;

	return ((idx + 1) & (txq->desc_count-1));
}

/**
 * ice_tx_setup_offload - Setup register values for performing a Tx offload
 * @txq: The Tx queue, used to track checksum offload stats
 * @pi: the packet info to program for
 * @cmd: the cmd register value to update
 * @off: the off register value to update
 *
 * Based on the packet info provided, update the cmd and off values for
 * enabling Tx offloads. This depends on the packet type and which offloads
 * have been requested.
 *
 * We also track the total number of times that we've requested hardware
 * offload a particular type of checksum for debugging purposes.
 */
static inline void
ice_tx_setup_offload(struct ice_tx_queue *txq, if_pkt_info_t pi, u32 *cmd, u32 *off)
{
	u32 remaining_csum_flags = pi->ipi_csum_flags;

	switch (pi->ipi_etype) {
#ifdef INET
		case ETHERTYPE_IP:
			if (pi->ipi_csum_flags & ICE_CSUM_IP) {
				*cmd |= ICE_TX_DESC_CMD_IIPT_IPV4_CSUM;
				txq->stats.cso[ICE_CSO_STAT_TX_IP4]++;
				remaining_csum_flags &= ~CSUM_IP;
			} else
				*cmd |= ICE_TX_DESC_CMD_IIPT_IPV4;
			break;
#endif
#ifdef INET6
		case ETHERTYPE_IPV6:
			*cmd |= ICE_TX_DESC_CMD_IIPT_IPV6;
			/*
			 * This indicates that the IIPT flag was set to the IPV6 value;
			 * there's no checksum for IPv6 packets.
			 */
			txq->stats.cso[ICE_CSO_STAT_TX_IP6]++;
			break;
#endif
		default:
			txq->stats.cso[ICE_CSO_STAT_TX_L3_ERR]++;
			break;
	}

	*off |= (pi->ipi_ehdrlen >> 1) << ICE_TX_DESC_LEN_MACLEN_S;
	*off |= (pi->ipi_ip_hlen >> 2) << ICE_TX_DESC_LEN_IPLEN_S;

	if (!(remaining_csum_flags & ~ICE_RX_CSUM_FLAGS))
		return;

	switch (pi->ipi_ipproto) {
		case IPPROTO_TCP:
			if (pi->ipi_csum_flags & ICE_CSUM_TCP) {
				*cmd |= ICE_TX_DESC_CMD_L4T_EOFT_TCP;
				*off |= (pi->ipi_tcp_hlen >> 2) <<
				    ICE_TX_DESC_LEN_L4_LEN_S;
				txq->stats.cso[ICE_CSO_STAT_TX_TCP]++;
			}
			break;
		case IPPROTO_UDP:
			if (pi->ipi_csum_flags & ICE_CSUM_UDP) {
				*cmd |= ICE_TX_DESC_CMD_L4T_EOFT_UDP;
				*off |= (sizeof(struct udphdr) >> 2) <<
				    ICE_TX_DESC_LEN_L4_LEN_S;
				txq->stats.cso[ICE_CSO_STAT_TX_UDP]++;
			}
			break;
		case IPPROTO_SCTP:
			if (pi->ipi_csum_flags & ICE_CSUM_SCTP) {
				*cmd |= ICE_TX_DESC_CMD_L4T_EOFT_SCTP;
				*off |= (sizeof(struct sctphdr) >> 2) <<
				    ICE_TX_DESC_LEN_L4_LEN_S;
				txq->stats.cso[ICE_CSO_STAT_TX_SCTP]++;
			}
			break;
		default:
			txq->stats.cso[ICE_CSO_STAT_TX_L4_ERR]++;
			break;
	}
}

/**
 * ice_rx_checksum - verify hardware checksum is valid or not
 * @rxq: the Rx queue structure
 * @flags: checksum flags to update
 * @data: checksum data to update
 * @status0: descriptor status data
 * @ptype: packet type
 *
 * Determine whether the hardware indicated that the Rx checksum is valid. If
 * so, update the checksum flags and data, informing the stack of the status
 * of the checksum so that it does not spend time verifying it manually.
 */
static void
ice_rx_checksum(struct ice_rx_queue *rxq, uint32_t *flags, uint32_t *data,
		u16 status0, u16 ptype)
{
	const u16 l3_error = (BIT(ICE_RX_FLEX_DESC_STATUS0_XSUM_IPE_S) |
			      BIT(ICE_RX_FLEX_DESC_STATUS0_XSUM_EIPE_S));
	const u16 l4_error = (BIT(ICE_RX_FLEX_DESC_STATUS0_XSUM_L4E_S) |
			      BIT(ICE_RX_FLEX_DESC_STATUS0_XSUM_EUDPE_S));
	const u16 xsum_errors = (l3_error | l4_error |
				 BIT(ICE_RX_FLEX_DESC_STATUS0_IPV6EXADD_S));
	struct ice_rx_ptype_decoded decoded;
	bool is_ipv4, is_ipv6;

	/* No L3 or L4 checksum was calculated */
	if (!(status0 & BIT(ICE_RX_FLEX_DESC_STATUS0_L3L4P_S))) {
		return;
	}

	decoded = ice_decode_rx_desc_ptype(ptype);
	*flags = 0;

	if (!(decoded.known && decoded.outer_ip))
		return;

	is_ipv4 = (decoded.outer_ip == ICE_RX_PTYPE_OUTER_IP) &&
	    (decoded.outer_ip_ver == ICE_RX_PTYPE_OUTER_IPV4);
	is_ipv6 = (decoded.outer_ip == ICE_RX_PTYPE_OUTER_IP) &&
	    (decoded.outer_ip_ver == ICE_RX_PTYPE_OUTER_IPV6);

	/* No checksum errors were reported */
	if (!(status0 & xsum_errors)) {
		if (is_ipv4)
			*flags |= CSUM_L3_CALC | CSUM_L3_VALID;

		switch (decoded.inner_prot) {
		case ICE_RX_PTYPE_INNER_PROT_TCP:
		case ICE_RX_PTYPE_INNER_PROT_UDP:
		case ICE_RX_PTYPE_INNER_PROT_SCTP:
			*flags |= CSUM_L4_CALC | CSUM_L4_VALID;
			*data |= htons(0xffff);
			break;
		default:
			break;
		}

		return;
	}

	/*
	 * Certain IPv6 extension headers impact the validity of L4 checksums.
	 * If one of these headers exist, hardware will set the IPV6EXADD bit
	 * in the descriptor. If the bit is set then pretend like hardware
	 * didn't checksum this packet.
	 */
	if (is_ipv6 && (status0 & BIT(ICE_RX_FLEX_DESC_STATUS0_IPV6EXADD_S))) {
		rxq->stats.cso[ICE_CSO_STAT_RX_IP6_ERR]++;
		return;
	}

	/*
	 * At this point, status0 must have at least one of the l3_error or
	 * l4_error bits set.
	 */

	if (status0 & l3_error) {
		if (is_ipv4) {
			rxq->stats.cso[ICE_CSO_STAT_RX_IP4_ERR]++;
			*flags |= CSUM_L3_CALC;
		} else {
			/* Hardware indicated L3 error but this isn't IPv4? */
			rxq->stats.cso[ICE_CSO_STAT_RX_L3_ERR]++;
		}
		/* don't bother reporting L4 errors if we got an L3 error */
		return;
	} else if (is_ipv4) {
		*flags |= CSUM_L3_CALC | CSUM_L3_VALID;
	}

	if (status0 & l4_error) {
		switch (decoded.inner_prot) {
		case ICE_RX_PTYPE_INNER_PROT_TCP:
			rxq->stats.cso[ICE_CSO_STAT_RX_TCP_ERR]++;
			*flags |= CSUM_L4_CALC;
			break;
		case ICE_RX_PTYPE_INNER_PROT_UDP:
			rxq->stats.cso[ICE_CSO_STAT_RX_UDP_ERR]++;
			*flags |= CSUM_L4_CALC;
			break;
		case ICE_RX_PTYPE_INNER_PROT_SCTP:
			rxq->stats.cso[ICE_CSO_STAT_RX_SCTP_ERR]++;
			*flags |= CSUM_L4_CALC;
			break;
		default:
			/*
			 * Hardware indicated L4 error, but this isn't one of
			 * the expected protocols.
			 */
			rxq->stats.cso[ICE_CSO_STAT_RX_L4_ERR]++;
		}
	}
}

/**
 * ice_ptype_to_hash - Convert packet type to a hash value
 * @ptype: the packet type to convert
 *
 * Given the packet type, convert to a suitable hashtype to report to the
 * upper stack via the iri_rsstype value of the if_rxd_info_t structure.
 *
 * If the hash type is unknown we'll report M_HASHTYPE_OPAQUE.
 */
static inline int
ice_ptype_to_hash(u16 ptype)
{
	struct ice_rx_ptype_decoded decoded;

	if (ptype >= ARRAY_SIZE(ice_ptype_lkup))
		return M_HASHTYPE_OPAQUE;

	decoded = ice_decode_rx_desc_ptype(ptype);

	if (!decoded.known)
		return M_HASHTYPE_OPAQUE;

	if (decoded.outer_ip == ICE_RX_PTYPE_OUTER_L2)
		return M_HASHTYPE_OPAQUE;

	/* Note: anything that gets to this point is IP */
	if (decoded.outer_ip_ver == ICE_RX_PTYPE_OUTER_IPV6) {
		switch (decoded.inner_prot) {
		case ICE_RX_PTYPE_INNER_PROT_TCP:
			return M_HASHTYPE_RSS_TCP_IPV6;
		case ICE_RX_PTYPE_INNER_PROT_UDP:
			return M_HASHTYPE_RSS_UDP_IPV6;
		default:
			return M_HASHTYPE_RSS_IPV6;
		}
	}
	if (decoded.outer_ip_ver == ICE_RX_PTYPE_OUTER_IPV4) {
		switch (decoded.inner_prot) {
		case ICE_RX_PTYPE_INNER_PROT_TCP:
			return M_HASHTYPE_RSS_TCP_IPV4;
		case ICE_RX_PTYPE_INNER_PROT_UDP:
			return M_HASHTYPE_RSS_UDP_IPV4;
		default:
			return M_HASHTYPE_RSS_IPV4;
		}
	}

	/* We should never get here!! */
	return M_HASHTYPE_OPAQUE;
}
#endif
