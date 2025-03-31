/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2024 Google LLC
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* GVE DQO Descriptor formats */

#ifndef _GVE_DESC_DQO_H_
#define _GVE_DESC_DQO_H_

#include "gve_plat.h"

#define GVE_ITR_ENABLE_BIT_DQO BIT(0)
#define GVE_ITR_NO_UPDATE_DQO (3 << 3)
#define GVE_ITR_INTERVAL_DQO_SHIFT 5
#define GVE_ITR_INTERVAL_DQO_MASK ((1 << 12) - 1)
#define GVE_TX_IRQ_RATELIMIT_US_DQO 50
#define GVE_RX_IRQ_RATELIMIT_US_DQO 20

#define GVE_TX_MAX_HDR_SIZE_DQO 255
#define GVE_TX_MIN_TSO_MSS_DQO 88

/*
 * Ringing the doorbell too often can hurt performance.
 *
 * HW requires this value to be at least 8.
 */
#define GVE_RX_BUF_THRESH_DQO 32

/*
 * Start dropping RX fragments if at least these many
 * buffers cannot be posted to the NIC.
 */
#define GVE_RX_DQO_MIN_PENDING_BUFS 128

#define GVE_DQ_NUM_FRAGS_IN_PAGE (PAGE_SIZE / GVE_DEFAULT_RX_BUFFER_SIZE)

/*
 * gve_rx_qpl_buf_id_dqo's 11 bit wide buf_id field limits the total
 * number of pages per QPL to 2048.
 */
#define GVE_RX_NUM_QPL_PAGES_DQO 2048

/* 2K TX buffers for DQO-QPL */
#define GVE_TX_BUF_SHIFT_DQO 11
#define GVE_TX_BUF_SIZE_DQO BIT(GVE_TX_BUF_SHIFT_DQO)
#define GVE_TX_BUFS_PER_PAGE_DQO (PAGE_SIZE >> GVE_TX_BUF_SHIFT_DQO)

#define GVE_TX_NUM_QPL_PAGES_DQO 512

/* Basic TX descriptor (DTYPE 0x0C) */
struct gve_tx_pkt_desc_dqo {
	__le64 buf_addr;

	/* Must be GVE_TX_PKT_DESC_DTYPE_DQO (0xc) */
	uint8_t dtype:5;

	/* Denotes the last descriptor of a packet. */
	uint8_t end_of_packet:1;
	uint8_t checksum_offload_enable:1;

	/* If set, will generate a descriptor completion for this descriptor. */
	uint8_t report_event:1;
	uint8_t reserved0;
	__le16 reserved1;

	/* The TX completion for this packet will contain this tag. */
	__le16 compl_tag;
	uint16_t buf_size:14;
	uint16_t reserved2:2;
} __packed;
_Static_assert(sizeof(struct gve_tx_pkt_desc_dqo) == 16,
    "gve: bad dqo desc struct length");

#define GVE_TX_PKT_DESC_DTYPE_DQO 0xc

/*
 * Maximum number of data descriptors allowed per packet, or per-TSO segment.
 */
#define GVE_TX_MAX_DATA_DESCS_DQO 10
#define GVE_TX_MAX_BUF_SIZE_DQO ((16 * 1024) - 1)
#define GVE_TSO_MAXSIZE_DQO IP_MAXPACKET

_Static_assert(GVE_TX_MAX_BUF_SIZE_DQO * GVE_TX_MAX_DATA_DESCS_DQO >=
    GVE_TSO_MAXSIZE_DQO,
    "gve: bad tso parameters");

/*
 * "report_event" on TX packet descriptors may only be reported on the last
 * descriptor of a TX packet, and they must be spaced apart with at least this
 * value.
 */
#define GVE_TX_MIN_RE_INTERVAL 32

struct gve_tx_context_cmd_dtype {
	uint8_t dtype:5;
	uint8_t tso:1;
	uint8_t reserved1:2;
	uint8_t reserved2;
};

_Static_assert(sizeof(struct gve_tx_context_cmd_dtype) == 2,
    "gve: bad dqo desc struct length");

/*
 * TX Native TSO Context DTYPE (0x05)
 *
 * "flex" fields allow the driver to send additional packet context to HW.
 */
struct gve_tx_tso_context_desc_dqo {
	/* The L4 payload bytes that should be segmented. */
	uint32_t tso_total_len:24;
	uint32_t flex10:8;

	/* Max segment size in TSO excluding headers. */
	uint16_t mss:14;
	uint16_t reserved:2;

	uint8_t header_len; /* Header length to use for TSO offload */
	uint8_t flex11;
	struct gve_tx_context_cmd_dtype cmd_dtype;
	uint8_t flex0;
	uint8_t flex5;
	uint8_t flex6;
	uint8_t flex7;
	uint8_t flex8;
	uint8_t flex9;
} __packed;
_Static_assert(sizeof(struct gve_tx_tso_context_desc_dqo) == 16,
    "gve: bad dqo desc struct length");

#define GVE_TX_TSO_CTX_DESC_DTYPE_DQO 0x5

/* General context descriptor for sending metadata. */
struct gve_tx_general_context_desc_dqo {
	uint8_t flex4;
	uint8_t flex5;
	uint8_t flex6;
	uint8_t flex7;
	uint8_t flex8;
	uint8_t flex9;
	uint8_t flex10;
	uint8_t flex11;
	struct gve_tx_context_cmd_dtype cmd_dtype;
	uint16_t reserved;
	uint8_t flex0;
	uint8_t flex1;
	uint8_t flex2;
	uint8_t flex3;
} __packed;
_Static_assert(sizeof(struct gve_tx_general_context_desc_dqo) == 16,
    "gve: bad dqo desc struct length");

#define GVE_TX_GENERAL_CTX_DESC_DTYPE_DQO 0x4

/*
 * Logical structure of metadata which is packed into context descriptor flex
 * fields.
 */
struct gve_tx_metadata_dqo {
	union {
		struct {
			uint8_t version;

			/*
			 * A zero value means no l4_hash was associated with the
			 * mbuf.
			 */
			uint16_t path_hash:15;

			/*
			 * Should be set to 1 if the flow associated with the
			 * mbuf had a rehash from the TCP stack.
			 */
			uint16_t rehash_event:1;
		}  __packed;
		uint8_t bytes[12];
	};
}  __packed;
_Static_assert(sizeof(struct gve_tx_metadata_dqo) == 12,
    "gve: bad dqo desc struct length");

#define GVE_TX_METADATA_VERSION_DQO 0

/* TX completion descriptor */
struct gve_tx_compl_desc_dqo {
	/* For types 0-4 this is the TX queue ID associated with this
	 * completion.
	 */
	uint16_t id:11;

	/* See: GVE_COMPL_TYPE_DQO* */
	uint16_t type:3;
	uint16_t reserved0:1;

	/* Flipped by HW to notify the descriptor is populated. */
	uint16_t generation:1;
	union {
		/* For descriptor completions, this is the last index fetched
		 * by HW + 1.
		 */
		__le16 tx_head;

		/* For packet completions, this is the completion tag set on the
		 * TX packet descriptors.
		 */
		__le16 completion_tag;
	};
	__le32 reserved1;
} __packed;
_Static_assert(sizeof(struct gve_tx_compl_desc_dqo) == 8,
    "gve: bad dqo desc struct length");

union gve_tx_desc_dqo {
	struct gve_tx_pkt_desc_dqo pkt;
	struct gve_tx_tso_context_desc_dqo tso_ctx;
	struct gve_tx_general_context_desc_dqo general_ctx;
};

#define GVE_COMPL_TYPE_DQO_PKT 0x2 /* Packet completion */
#define GVE_COMPL_TYPE_DQO_DESC 0x4 /* Descriptor completion */

/* Descriptor to post buffers to HW on buffer queue. */
struct gve_rx_desc_dqo {
	__le16 buf_id; /* ID returned in Rx completion descriptor */
	__le16 reserved0;
	__le32 reserved1;
	__le64 buf_addr; /* DMA address of the buffer */
	__le64 header_buf_addr;
	__le64 reserved2;
} __packed;
_Static_assert(sizeof(struct gve_rx_desc_dqo) == 32,
    "gve: bad dqo desc struct length");

/* Descriptor for HW to notify SW of new packets received on RX queue. */
struct gve_rx_compl_desc_dqo {
	/* Must be 1 */
	uint8_t rxdid:4;
	uint8_t reserved0:4;

	/* Packet originated from this system rather than the network. */
	uint8_t loopback:1;
	/* Set when IPv6 packet contains a destination options header or routing
	 * header.
	 */
	uint8_t ipv6_ex_add:1;
	/* Invalid packet was received. */
	uint8_t rx_error:1;
	uint8_t reserved1:5;

	uint16_t packet_type:10;
	uint16_t ip_hdr_err:1;
	uint16_t udp_len_err:1;
	uint16_t raw_cs_invalid:1;
	uint16_t reserved2:3;

	uint16_t packet_len:14;
	/* Flipped by HW to notify the descriptor is populated. */
	uint16_t generation:1;
	/* Should be zero. */
	uint16_t buffer_queue_id:1;

	uint16_t header_len:10;
	uint16_t rsc:1;
	uint16_t split_header:1;
	uint16_t reserved3:4;

	uint8_t descriptor_done:1;
	uint8_t end_of_packet:1;
	uint8_t header_buffer_overflow:1;
	uint8_t l3_l4_processed:1;
	uint8_t csum_ip_err:1;
	uint8_t csum_l4_err:1;
	uint8_t csum_external_ip_err:1;
	uint8_t csum_external_udp_err:1;

	uint8_t status_error1;

	__le16 reserved5;
	__le16 buf_id; /* Buffer ID which was sent on the buffer queue. */

	union {
		/* Packet checksum. */
		__le16 raw_cs;
		/* Segment length for RSC packets. */
		__le16 rsc_seg_len;
	};
	__le32 hash;
	__le32 reserved6;
	__le64 reserved7;
} __packed;

_Static_assert(sizeof(struct gve_rx_compl_desc_dqo) == 32,
    "gve: bad dqo desc struct length");
#endif /* _GVE_DESC_DQO_H_ */
