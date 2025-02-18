/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Google LLC
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
#ifndef _GVE_DESC_H_
#define _GVE_DESC_H_

#include "gve_plat.h"

/*
 * A note on seg_addrs
 *
 * Base addresses encoded in seg_addr are not assumed to be physical
 * addresses. The ring format assumes these come from some linear address
 * space. This could be physical memory, kernel virtual memory, user virtual
 * memory.
 *
 * Each queue is assumed to be associated with a single such linear
 * address space to ensure a consistent meaning for seg_addrs posted to its
 * rings.
 */
struct gve_tx_pkt_desc {
	uint8_t	type_flags;  /* desc type is lower 4 bits, flags upper */
	uint8_t	l4_csum_offset;  /* relative offset of L4 csum word */
	uint8_t	l4_hdr_offset;  /* Offset of start of L4 headers in packet */
	uint8_t	desc_cnt;  /* Total descriptors for this packet */
	__be16	len;  /* Total length of this packet (in bytes) */
	__be16	seg_len;  /* Length of this descriptor's segment */
	__be64	seg_addr;  /* Base address (see note) of this segment */
} __packed;

struct gve_tx_mtd_desc {
	uint8_t	type_flags;	/* type is lower 4 bits, subtype upper  */
	uint8_t	path_state;	/* state is lower 4 bits, hash type upper */
	__be16	reserved0;
	__be32	path_hash;
	__be64	reserved1;
} __packed;

struct gve_tx_seg_desc {
	uint8_t	type_flags;	/* type is lower 4 bits, flags upper	*/
	uint8_t	l3_offset;	/* TSO: 2 byte units to start of IPH	*/
	__be16	reserved;
	__be16	mss;		/* TSO MSS				*/
	__be16	seg_len;
	__be64	seg_addr;
} __packed;

/* GVE Transmit Descriptor Types */
#define	GVE_TXD_STD		(0x0 << 4) /* Std with Host Address	*/
#define	GVE_TXD_TSO		(0x1 << 4) /* TSO with Host Address	*/
#define	GVE_TXD_SEG		(0x2 << 4) /* Seg with Host Address	*/
#define	GVE_TXD_MTD		(0x3 << 4) /* Metadata			*/

/* GVE Transmit Descriptor Flags for Std Pkts */
#define	GVE_TXF_L4CSUM	BIT(0)	/* Need csum offload */
#define	GVE_TXF_TSTAMP	BIT(2)	/* Timestamp required */

/* GVE Transmit Descriptor Flags for TSO Segs */
#define	GVE_TXSF_IPV6	BIT(1)	/* IPv6 TSO */

/* GVE Transmit Descriptor Options for MTD Segs */
#define GVE_MTD_SUBTYPE_PATH		0

#define GVE_MTD_PATH_STATE_DEFAULT	0
#define GVE_MTD_PATH_STATE_TIMEOUT	1
#define GVE_MTD_PATH_STATE_CONGESTION	2
#define GVE_MTD_PATH_STATE_RETRANSMIT	3

#define GVE_MTD_PATH_HASH_NONE		(0x0 << 4)
#define GVE_MTD_PATH_HASH_L4		(0x1 << 4)

/*
 * GVE Receive Packet Descriptor
 *
 * The start of an ethernet packet comes 2 bytes into the rx buffer.
 * gVNIC adds this padding so that both the DMA and the L3/4 protocol header
 * access is aligned.
 */
#define GVE_RX_PAD 2

struct gve_rx_desc {
	uint8_t	padding[48];
	__be32	rss_hash;  /* Receive-side scaling hash (Toeplitz for gVNIC) */
	__be16	mss;
	__be16	reserved;  /* Reserved to zero */
	uint8_t	hdr_len;   /* Header length (L2-L4) including padding */
	uint8_t	hdr_off;   /* 64-byte-scaled offset into RX_DATA entry */
	uint16_t csum;     /* 1's-complement partial checksum of L3+ bytes */
	__be16	len;       /* Length of the received packet */
	__be16	flags_seq; /* Flags [15:3] and sequence number [2:0] (1-7) */
} __packed;
_Static_assert(sizeof(struct gve_rx_desc) == 64, "gve: bad desc struct length");

/*
 * If the device supports raw dma addressing then the addr in data slot is
 * the dma address of the buffer.
 * If the device only supports registered segments then the addr is a byte
 * offset into the registered segment (an ordered list of pages) where the
 * buffer is.
 */
union gve_rx_data_slot {
	__be64 qpl_offset;
	__be64 addr;
};

/* GVE Receive Packet Descriptor Seq No */
#define GVE_SEQNO(x) (be16toh(x) & 0x7)

/* GVE Receive Packet Descriptor Flags */
#define GVE_RXFLG(x)	htobe16(1 << (3 + (x)))
#define	GVE_RXF_FRAG		GVE_RXFLG(3)	/* IP Fragment			*/
#define	GVE_RXF_IPV4		GVE_RXFLG(4)	/* IPv4				*/
#define	GVE_RXF_IPV6		GVE_RXFLG(5)	/* IPv6				*/
#define	GVE_RXF_TCP		GVE_RXFLG(6)	/* TCP Packet			*/
#define	GVE_RXF_UDP		GVE_RXFLG(7)	/* UDP Packet			*/
#define	GVE_RXF_ERR		GVE_RXFLG(8)	/* Packet Error Detected	*/
#define	GVE_RXF_PKT_CONT	GVE_RXFLG(10)	/* Multi Fragment RX packet	*/

/* GVE IRQ */
#define GVE_IRQ_ACK	BIT(31)
#define GVE_IRQ_MASK	BIT(30)
#define GVE_IRQ_EVENT	BIT(29)

#endif /* _GVE_DESC_H_ */
